//=========================================================================
// Name:            TciWebSocketClient.cpp
// Purpose:         WebSocket client for TCI protocol implementation
//
// Authors:         Tomas Ostojic (original POSIX impl)
//                  J.J. Boyd     (websocketpp + TcpConnectionHandler port)
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#include "TciWebSocketClient.h"

#include <cstring>
#include <sstream>

#include "../util/logging/ulog.h"

namespace tci
{
    TciWebSocketClient::TciWebSocketClient()
        : wsConnected_(false)
    {
        // Mirror SocketIoClient logging defaults: silence the verbose
        // websocketpp "all" channel and reopen only the high-signal ones.
        client_.clear_access_channels(websocketpp::log::alevel::all);
        client_.set_access_channels(websocketpp::log::alevel::connect);
        client_.set_access_channels(websocketpp::log::alevel::disconnect);
        client_.set_access_channels(websocketpp::log::alevel::app);
    }

    TciWebSocketClient::~TciWebSocketClient()
    {
        // Match SocketIoClient's safe-destruction sequence: clear handler
        // closures on the worker thread, then disable reconnect and wait
        // for an in-flight disconnect to land before tearing down.
        enqueue_([this]() {
            commandCallback_         = nullptr;
            streamCallback_          = nullptr;
            onConnectedCallback_     = nullptr;
            onDisconnectedCallback_  = nullptr;
            onErrorCallback_         = nullptr;
        });

        enableReconnect_.store(false, std::memory_order_release);
        auto fut = TcpConnectionHandler::disconnect();
        fut.wait();

        waitForAllTasksComplete_();
    }

    bool TciWebSocketClient::connect(const std::string& hostname, int port)
    {
        if (wsConnected_.load(std::memory_order_acquire))
        {
            return true;
        }

        // TcpConnectionHandler::connect kicks the worker thread to start
        // DNS + the TCP three-way handshake. On success it eventually
        // enqueues onConnect_(), where we run the WebSocket upgrade.
        //
        // Return true as long as we successfully kicked off the TCP
        // attempt; the WS handshake then completes asynchronously and
        // setOnConnected() fires on the worker thread.
        // enableReconnect = true so TcpConnectionHandler's RECONNECT_INTERVAL_MS
        // (5 sec) retry loop fires on drop. TciRigController::onWsConnected_
        // replays the cached state once each reconnect handshake completes.
        TcpConnectionHandler::connect(hostname.c_str(), port,
                                      /* enableReconnect = */ true);
        return true;
    }

    void TciWebSocketClient::disconnect()
    {
        // Best-effort WS close so the peer sees a proper close frame
        // before we drop the socket. If we don't have an active WS
        // connection (e.g. handshake never completed), skip straight to
        // the TCP teardown.
        if (wsConnected_.load(std::memory_order_acquire) && connection_)
        {
            websocketpp::lib::error_code ec;
            connection_->close(websocketpp::close::status::normal,
                               "client disconnect", ec);
            if (ec)
            {
                log_info("websocketpp close returned ec=%d (%s)",
                         ec.value(), ec.message().c_str());
            }
        }

        wsConnected_.store(false, std::memory_order_release);

        auto fut = TcpConnectionHandler::disconnect();
        fut.wait();
    }

    bool TciWebSocketClient::isConnected() const
    {
        return wsConnected_.load(std::memory_order_acquire);
    }

    bool TciWebSocketClient::sendCommand(const std::string& command)
    {
        if (!wsConnected_.load(std::memory_order_acquire) || !connection_)
        {
            return false;
        }

        websocketpp::lib::error_code ec;
        client_.send(connection_->get_handle(), command,
                     websocketpp::frame::opcode::text, ec);
        if (ec)
        {
            log_warn("sendCommand failed: ec=%d (%s)",
                     ec.value(), ec.message().c_str());
            return false;
        }
        return true;
    }

    bool TciWebSocketClient::sendBinaryData(const uint8_t* data, size_t size)
    {
        if (!wsConnected_.load(std::memory_order_acquire) || !connection_)
        {
            return false;
        }

        websocketpp::lib::error_code ec;
        client_.send(connection_->get_handle(),
                     reinterpret_cast<void const*>(data), size,
                     websocketpp::frame::opcode::binary, ec);
        if (ec)
        {
            log_warn("sendBinaryData failed: ec=%d (%s)",
                     ec.value(), ec.message().c_str());
            return false;
        }
        return true;
    }

    void TciWebSocketClient::setCommandCallback(CommandCallback callback)
    {
        enqueue_([this, callback = std::move(callback)]() {
            commandCallback_ = callback;
        });
    }

    void TciWebSocketClient::setStreamCallback(StreamCallback callback)
    {
        enqueue_([this, callback = std::move(callback)]() {
            streamCallback_ = callback;
        });
    }

    void TciWebSocketClient::setOnConnected(std::function<void()> callback)
    {
        enqueue_([this, callback = std::move(callback)]() {
            onConnectedCallback_ = callback;
        });
    }

    void TciWebSocketClient::setOnDisconnected(std::function<void()> callback)
    {
        enqueue_([this, callback = std::move(callback)]() {
            onDisconnectedCallback_ = callback;
        });
    }

    void TciWebSocketClient::setOnError(std::function<void(const std::string&)> callback)
    {
        enqueue_([this, callback = std::move(callback)]() {
            onErrorCallback_ = callback;
        });
    }

    void TciWebSocketClient::onConnect_()
    {
        // The TCP layer is live. Install handlers on the websocketpp
        // client and kick off the WS upgrade.

        // write_handler bridges websocketpp's framer to our socket. The
        // framer hands us a buffer of frame bytes; we push it down the
        // TcpConnectionHandler send path and discard the resulting
        // future (fire-and-forget, matching SocketIoClient).
        client_.set_write_handler([this](connection_hdl const&,
                                         char const* buf, size_t len) {
            this->send(buf, static_cast<int>(len));
            return websocketpp::lib::error_code();
        });

        // message_handler fires when a complete frame is decoded.
        client_.set_message_handler(
            [this](connection_hdl const& hdl, message_ptr const& msg) {
                handleMessage_(hdl, msg);
            });

        // open_handler fires when the WebSocket upgrade completes.
        client_.set_open_handler([this](connection_hdl const&) {
            wsConnected_.store(true, std::memory_order_release);
            if (onConnectedCallback_)
            {
                onConnectedCallback_();
            }
        });

        // close_handler fires when the peer sends a close frame (or we
        // do). Reflect that into wsConnected_ + the user callback. The
        // socket itself is torn down later by TcpConnectionHandler.
        client_.set_close_handler([this](connection_hdl const&) {
            wsConnected_.store(false, std::memory_order_release);
            if (onDisconnectedCallback_)
            {
                onDisconnectedCallback_();
            }
        });

        // fail_handler fires on handshake failure (bad HTTP response,
        // protocol violation, etc).
        client_.set_fail_handler([this](connection_hdl const&) {
            wsConnected_.store(false, std::memory_order_release);
            if (onErrorCallback_)
            {
                onErrorCallback_("WebSocket handshake failed");
            }
        });

        // Build the ws:// URI and start the upgrade. The iostream
        // transport will push the HTTP handshake bytes through
        // write_handler synchronously.
        std::stringstream ss;
        ss << "ws://" << host_ << ":" << port_ << "/";

        websocketpp::lib::error_code ec;
        connection_ = client_.get_connection(ss.str(), ec);
        if (ec)
        {
            log_warn("get_connection failed: ec=%d (%s)",
                     ec.value(), ec.message().c_str());
            if (onErrorCallback_)
            {
                onErrorCallback_(std::string("WebSocket setup failed: ") + ec.message());
            }
            return;
        }

        client_.connect(connection_);
    }

    void TciWebSocketClient::onDisconnect_()
    {
        // Called by TcpConnectionHandler after the socket has been
        // closed and the recv thread joined. close_handler may have
        // already fired (clean WS close) or may not (abrupt drop);
        // fire onDisconnectedCallback_ only once either way.
        bool wasConnected = wsConnected_.exchange(false, std::memory_order_acq_rel);
        if (wasConnected && onDisconnectedCallback_)
        {
            onDisconnectedCallback_();
        }

        // Signal EOF to the framer so any partial state is reset.
        if (connection_)
        {
            connection_->eof();
        }
    }

    void TciWebSocketClient::onReceive_(char* buf, int length)
    {
        // Bytes from the TCP recv thread are dispatched on the
        // TcpConnectionHandler worker thread (see receiveImpl_ in
        // TcpConnectionHandler.cpp). Push them into the iostream
        // transport so the framer can decode frames and fire
        // message_handler / open_handler / close_handler.
        if (connection_)
        {
            connection_->read_some(buf, static_cast<size_t>(length));
        }
    }

    void TciWebSocketClient::handleMessage_(connection_hdl const&,
                                            message_ptr const& msg)
    {
        if (msg->get_opcode() == websocketpp::frame::opcode::text)
        {
            // TCI text frames arrive as one-or-more semicolon-delimited
            // commands. Split on ';' and dispatch each through the
            // command parser, matching the original raw-POSIX
            // implementation's behavior.
            if (!commandCallback_)
            {
                return;
            }

            const std::string& payload = msg->get_payload();
            std::string accumulator;
            CommandParser parser;
            for (char ch : payload)
            {
                accumulator += ch;
                if (ch == ';')
                {
                    std::string cmdName;
                    std::vector<std::string> args;
                    if (parser.parseCommand(accumulator, cmdName, args))
                    {
                        commandCallback_(cmdName, args);
                    }
                    accumulator.clear();
                }
            }
            // Trailing fragment without a terminating ';' is intentionally
            // dropped to match the original behavior; well-formed TCI
            // text frames always terminate every command with ';'.
        }
        else if (msg->get_opcode() == websocketpp::frame::opcode::binary)
        {
            if (!streamCallback_)
            {
                return;
            }

            const std::string& payload = msg->get_payload();
            if (payload.size() < sizeof(StreamHeader))
            {
                return;
            }

            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(payload.data());
            StreamHeader header;
            std::memcpy(&header, bytes, sizeof(StreamHeader));

            const uint8_t* audioData = bytes + sizeof(StreamHeader);
            size_t audioSize         = payload.size() - sizeof(StreamHeader);

            streamCallback_(header, audioData, audioSize);
        }
        // All other opcodes (continuation, ping, pong, close) are handled
        // by websocketpp internally.
    }
}
