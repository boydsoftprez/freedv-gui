//=========================================================================
// Name:            TciWebSocketClient.h
// Purpose:         WebSocket client for TCI protocol
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

#ifndef TCI_WEBSOCKET_CLIENT_H
#define TCI_WEBSOCKET_CLIENT_H

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "../util/websocketpp_config.h"
#include <websocketpp/client.hpp>

#include "../util/TcpConnectionHandler.h"
#include "TciProtocol.h"

namespace tci
{
    // TciWebSocketClient
    //
    // RFC 6455 WebSocket client for TCI. Built on the project's standard
    // cross-platform transport stack:
    //
    //   * TcpConnectionHandler  - POSIX/WinSock sockets, Happy-Eyeballs
    //                             DNS, 5 s reconnect, send/recv split.
    //   * websocketpp           - WS framing only (iostream transport).
    //
    // This is the same pattern used by util/SocketIoClient.
    //
    // Public API is preserved verbatim against the prior raw-POSIX
    // implementation so existing consumers (TciRigController,
    // TciAudioDevice) do not need source changes.
    class TciWebSocketClient : public TcpConnectionHandler
    {
    public:
        TciWebSocketClient();
        virtual ~TciWebSocketClient();

        // Connect to TCI server. Returns true if the TCP layer kicked off;
        // the WebSocket handshake completes asynchronously, after which
        // setOnConnected() fires on the worker thread.
        bool connect(const std::string& hostname, int port);

        // Disconnect from TCI server. Blocks until the recv thread is
        // joined and the disconnect callback has fired.
        void disconnect();

        // Check if the WebSocket handshake has completed and the socket
        // is still alive.
        bool isConnected() const;

        // Send a TCI command as a single WebSocket text frame.
        bool sendCommand(const std::string& command);

        // Send binary audio stream data as a single WebSocket binary frame.
        bool sendBinaryData(const uint8_t* data, size_t size);

        // Set callback for received commands (parsed via CommandParser).
        void setCommandCallback(CommandCallback callback);

        // Set callback for received audio streams (binary frames, header
        // parsed off the front).
        void setStreamCallback(StreamCallback callback);

        // Connection lifecycle callbacks.
        void setOnConnected(std::function<void()> callback);
        void setOnDisconnected(std::function<void()> callback);
        void setOnError(std::function<void(const std::string&)> callback);

    protected:
        // TcpConnectionHandler hooks. All three fire on the worker thread
        // (TcpConnectionHandler enqueues them).
        virtual void onConnect_() override;
        virtual void onDisconnect_() override;
        virtual void onReceive_(char* buf, int length) override;

    private:
        using WebSocketClient = websocketpp::client<websocketpp::config::custom_config>;
        using message_ptr     = WebSocketClient::message_ptr;
        using connection_hdl  = websocketpp::connection_hdl;

        WebSocketClient                client_;
        WebSocketClient::connection_ptr connection_;

        // wsConnected_ goes true after the WS open_handler fires, goes
        // false on close_handler / fail_handler / onDisconnect_. Read by
        // isConnected() and the send paths.
        std::atomic<bool> wsConnected_;

        CommandCallback              commandCallback_;
        StreamCallback               streamCallback_;
        std::function<void()>        onConnectedCallback_;
        std::function<void()>        onDisconnectedCallback_;
        std::function<void(const std::string&)> onErrorCallback_;

        // Initial wsConnected_ snapshot (used by sendCommand to short-
        // circuit before the handshake completes without going through
        // the worker queue).
        void handleMessage_(connection_hdl const& hdl, message_ptr const& msg);
    };
}

#endif // TCI_WEBSOCKET_CLIENT_H
