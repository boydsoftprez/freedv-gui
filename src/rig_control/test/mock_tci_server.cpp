//=========================================================================
// Name:            mock_tci_server.cpp
// Purpose:         Minimal localhost RFC 6455 WebSocket server fixture for
//                  integration tests of TciWebSocketClient.
//
// Authors:         J.J. Boyd
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

#if !defined(_WIN32)

#include "mock_tci_server.h"

// Vendored SHA-1 implementation from websocketpp (smallsha1 repackage).
// No external dependency -- this header is already part of the project's
// third-party tree.
#include "3rdparty/websocketpp/sha1/sha1.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>

// ---------------------------------------------------------------------------
// Minimal base64 encoder (RFC 4648 Table 1, standard alphabet).
// Operates on raw bytes and produces a padded base64 string.
// ---------------------------------------------------------------------------
static std::string base64Encode(const unsigned char* data, size_t len)
{
    static const char kAlpha[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3)
    {
        unsigned int b = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < len) { b |= static_cast<unsigned int>(data[i + 1]) << 8; }
        if (i + 2 < len) { b |= static_cast<unsigned int>(data[i + 2]); }

        out += kAlpha[(b >> 18) & 0x3F];
        out += kAlpha[(b >> 12) & 0x3F];
        out += (i + 1 < len) ? kAlpha[(b >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? kAlpha[b & 0x3F] : '=';
    }

    return out;
}

// ---------------------------------------------------------------------------
// MockTciServer implementation
// ---------------------------------------------------------------------------

MockTciServer::MockTciServer()
    : listenFd_(-1)
    , port_(0)
{
}

MockTciServer::~MockTciServer()
{
    stop();
}

bool MockTciServer::start()
{
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0)
    {
        std::cerr << "[MockTciServer] socket() failed: " << strerror(errno) << "\n";
        return false;
    }

    // SO_REUSEADDR so rapid test re-runs don't hit TIME_WAIT.
    int one = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // kernel assigns

    if (::bind(listenFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        std::cerr << "[MockTciServer] bind() failed: " << strerror(errno) << "\n";
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    // Discover the kernel-assigned port.
    socklen_t addrLen = sizeof(addr);
    if (::getsockname(listenFd_, reinterpret_cast<struct sockaddr*>(&addr), &addrLen) < 0)
    {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    port_ = ntohs(addr.sin_port);

    if (::listen(listenFd_, 1) < 0)
    {
        std::cerr << "[MockTciServer] listen() failed: " << strerror(errno) << "\n";
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    running_ = true;
    acceptThread_ = std::thread(&MockTciServer::acceptThreadFunc_, this);
    return true;
}

void MockTciServer::stop()
{
    running_ = false;

    if (listenFd_ >= 0)
    {
        ::close(listenFd_);
        listenFd_ = -1;
    }

    if (acceptThread_.joinable())
    {
        acceptThread_.join();
    }
    if (ioThread_.joinable())
    {
        ioThread_.join();
    }
}

int MockTciServer::getPort() const
{
    return port_;
}

void MockTciServer::queueTextFrame(const std::string& text)
{
    QueuedFrame f;
    f.opcode = 0x1;
    f.payload.assign(text.begin(), text.end());
    std::lock_guard<std::mutex> lock(queueMutex_);
    outQueue_.push(std::move(f));
}

void MockTciServer::queueBinaryFrame(const std::vector<uint8_t>& payload)
{
    QueuedFrame f;
    f.opcode = 0x2;
    f.payload = payload;
    std::lock_guard<std::mutex> lock(queueMutex_);
    outQueue_.push(std::move(f));
}

void MockTciServer::setOnTextFrame(std::function<void(const std::string&)> cb)
{
    onTextFrame_ = std::move(cb);
}

void MockTciServer::setOnBinaryFrame(std::function<void(const std::vector<uint8_t>&)> cb)
{
    onBinaryFrame_ = std::move(cb);
}

bool MockTciServer::isClientConnected() const
{
    return clientConnected_;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void MockTciServer::acceptThreadFunc_()
{
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);

    int clientFd = ::accept(listenFd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &len);
    if (clientFd < 0)
    {
        // Normal if stop() closed the listen socket.
        return;
    }

    // Hand off to the I/O thread.
    ioThread_ = std::thread(&MockTciServer::ioThreadFunc_, this, clientFd);
}

void MockTciServer::ioThreadFunc_(int clientFd)
{
    // --- WebSocket handshake ---

    std::string request = readHttpRequest_(clientFd);
    if (request.empty())
    {
        ::close(clientFd);
        return;
    }

    std::string key = extractWsKey_(request);
    if (key.empty())
    {
        std::cerr << "[MockTciServer] No Sec-WebSocket-Key in request\n";
        ::close(clientFd);
        return;
    }

    std::string acceptValue = computeAccept_(key);
    if (!sendHandshake_(clientFd, acceptValue))
    {
        std::cerr << "[MockTciServer] Failed to send handshake response\n";
        ::close(clientFd);
        return;
    }

    clientConnected_ = true;

    // Send any frames that were queued before the client connected.
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!outQueue_.empty())
        {
            QueuedFrame& f = outQueue_.front();
            sendWsFrame_(clientFd, f.opcode, f.payload.data(), f.payload.size());
            outQueue_.pop();
        }
    }

    // --- I/O loop: read inbound frames, flush outbound queue ---

    // Set a short receive timeout so the loop wakes up to drain outbound queue.
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10 * 1000; // 10 ms
    ::setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (running_)
    {
        // --- Drain outbound queue ---
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            while (!outQueue_.empty())
            {
                QueuedFrame& f = outQueue_.front();
                sendWsFrame_(clientFd, f.opcode, f.payload.data(), f.payload.size());
                outQueue_.pop();
            }
        }

        // --- Read one inbound frame ---

        // Byte 0: FIN + opcode.
        uint8_t byte0 = 0;
        ssize_t n = ::recv(clientFd, &byte0, 1, 0);
        if (n == 0)
        {
            // Client closed connection.
            break;
        }
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT)
            {
                // Timeout -- loop back to drain queue and try again.
                continue;
            }
            // Real error.
            break;
        }

        uint8_t opcode = byte0 & 0x0F;

        // Byte 1: MASK bit + 7-bit payload length.
        uint8_t byte1 = 0;
        if (!readExactly_(clientFd, &byte1, 1)) { break; }

        bool masked = (byte1 & 0x80) != 0;
        uint64_t payloadLen = byte1 & 0x7F;

        // Optional extended payload length (RFC 6455 section 5.2).
        if (payloadLen == 126)
        {
            uint8_t ext[2] = {};
            if (!readExactly_(clientFd, ext, 2)) { break; }
            payloadLen = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
        }
        else if (payloadLen == 127)
        {
            uint8_t ext[8] = {};
            if (!readExactly_(clientFd, ext, 8)) { break; }
            payloadLen = 0;
            for (int i = 0; i < 8; ++i)
            {
                payloadLen = (payloadLen << 8) | ext[i];
            }
        }

        // 4-byte masking key (clients MUST mask; RFC 6455 section 5.3).
        uint8_t mask[4] = {};
        if (masked)
        {
            if (!readExactly_(clientFd, mask, 4)) { break; }
        }

        // Read payload.
        std::vector<uint8_t> payload(static_cast<size_t>(payloadLen));
        if (payloadLen > 0)
        {
            if (!readExactly_(clientFd, payload.data(), static_cast<size_t>(payloadLen))) { break; }
        }

        // Unmask payload (XOR with cycling 4-byte key; RFC 6455 section 5.3).
        if (masked)
        {
            for (size_t i = 0; i < payload.size(); ++i)
            {
                payload[i] ^= mask[i % 4];
            }
        }

        // Dispatch by opcode.
        if (opcode == 0x1) // Text frame
        {
            if (onTextFrame_)
            {
                std::string text(payload.begin(), payload.end());
                onTextFrame_(text);
            }
        }
        else if (opcode == 0x2) // Binary frame
        {
            if (onBinaryFrame_)
            {
                onBinaryFrame_(payload);
            }
        }
        else if (opcode == 0x8) // Connection Close
        {
            break;
        }
        // Ping (0x9) / Pong (0xA) ignored for this minimal fixture.
    }

    clientConnected_ = false;
    ::close(clientFd);
}

// static
bool MockTciServer::readExactly_(int fd, uint8_t* buf, size_t n)
{
    size_t received = 0;
    while (received < n)
    {
        ssize_t r = ::recv(fd, buf + received, n - received, 0);
        if (r <= 0)
        {
            if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT))
            {
                // Keep waiting for the rest of the current frame bytes.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            return false;
        }
        received += static_cast<size_t>(r);
    }
    return true;
}

// static
std::string MockTciServer::readHttpRequest_(int fd)
{
    std::string request;
    char buf[1];

    while (true)
    {
        ssize_t n = ::recv(fd, buf, 1, 0);
        if (n <= 0)
        {
            break;
        }
        request += buf[0];

        // HTTP headers end with a blank line: \r\n\r\n
        if (request.size() >= 4 &&
            request.compare(request.size() - 4, 4, "\r\n\r\n") == 0)
        {
            break;
        }
    }

    return request;
}

// static
std::string MockTciServer::extractWsKey_(const std::string& request)
{
    // Find the header line (case-insensitive prefix match on the field name).
    const std::string headerName = "Sec-WebSocket-Key:";

    // Search line by line so we don't need a full HTTP parser.
    std::istringstream ss(request);
    std::string line;
    while (std::getline(ss, line))
    {
        // Strip trailing \r if present.
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        // Case-insensitive prefix match.
        if (line.size() >= headerName.size())
        {
            bool match = true;
            for (size_t i = 0; i < headerName.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(line[i])) !=
                    std::tolower(static_cast<unsigned char>(headerName[i])))
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                // Value follows the colon, optionally preceded by whitespace.
                std::string value = line.substr(headerName.size());
                // Trim leading whitespace.
                size_t start = value.find_first_not_of(" \t");
                if (start != std::string::npos)
                {
                    value = value.substr(start);
                }
                return value;
            }
        }
    }

    return {};
}

// static
std::string MockTciServer::computeAccept_(const std::string& key)
{
    // RFC 6455 section 1.3: accept = base64(SHA1(key + magic_guid))
    static const char kMagicGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    std::string input = key + kMagicGuid;

    unsigned char sha1Digest[20];
    websocketpp::sha1::calc(input.data(), input.size(), sha1Digest);

    return base64Encode(sha1Digest, sizeof(sha1Digest));
}

// static
bool MockTciServer::sendHandshake_(int fd, const std::string& acceptValue)
{
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptValue + "\r\n"
        "\r\n";

    ssize_t sent = ::send(fd, response.data(), response.size(), 0);
    return sent == static_cast<ssize_t>(response.size());
}

// static
bool MockTciServer::sendWsFrame_(int fd, uint8_t opcode,
                                  const uint8_t* payload, size_t payloadLen)
{
    // Server-to-client frames are NOT masked (RFC 6455 section 5.1).
    std::vector<uint8_t> frame;
    frame.reserve(10 + payloadLen);

    // Byte 0: FIN=1 + opcode.
    frame.push_back(static_cast<uint8_t>(0x80 | (opcode & 0x0F)));

    // Byte 1 (+optional extension): MASK=0 + payload length.
    if (payloadLen < 126)
    {
        frame.push_back(static_cast<uint8_t>(payloadLen));
    }
    else if (payloadLen < 65536)
    {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payloadLen & 0xFF));
    }
    else
    {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i)
        {
            frame.push_back(static_cast<uint8_t>((payloadLen >> (i * 8)) & 0xFF));
        }
    }

    // Payload (unmasked).
    frame.insert(frame.end(), payload, payload + payloadLen);

    ssize_t sent = ::send(fd, frame.data(), frame.size(), 0);
    return sent == static_cast<ssize_t>(frame.size());
}

#endif // !defined(_WIN32)
