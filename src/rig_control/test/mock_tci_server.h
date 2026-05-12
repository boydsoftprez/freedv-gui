//=========================================================================
// Name:            mock_tci_server.h
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

#ifndef MOCK_TCI_SERVER_H
#define MOCK_TCI_SERVER_H

#if !defined(_WIN32)

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Minimal RFC 6455 WebSocket server for use only in unit tests.
// Accepts exactly one client connection, performs the HTTP upgrade handshake,
// then runs an I/O loop that decodes/encodes RFC 6455 frames.
//
// Not thread-safe for concurrent calls to public API members, but the
// callbacks are dispatched from the I/O thread.  Design is intentionally
// simple -- this exists only to exercise TciWebSocketClient.
class MockTciServer
{
public:
    MockTciServer();
    ~MockTciServer();

    // Bind on port 0 (kernel-assigned), start accept thread.
    // Returns true on success.  Call getPort() after a successful start().
    bool start();

    // Signal the server to stop, close sockets, join threads.
    void stop();

    // Returns the kernel-assigned port after start(), or 0 before start().
    int getPort() const;

    // Queue a text frame to be sent to the client as soon as the I/O loop
    // picks it up.  Safe to call from the test thread.
    void queueTextFrame(const std::string& text);

    // Queue a binary frame.  Safe to call from the test thread.
    void queueBinaryFrame(const std::vector<uint8_t>& payload);

    // Register a callback invoked on the I/O thread when a text frame
    // arrives from the client.
    void setOnTextFrame(std::function<void(const std::string&)> cb);

    // Register a callback invoked on the I/O thread when a binary frame
    // arrives from the client.
    void setOnBinaryFrame(std::function<void(const std::vector<uint8_t>&)> cb);

    // Returns true once the client has completed the WebSocket handshake.
    bool isClientConnected() const;

private:
    // Accept loop: blocks waiting for one client, then hands off to I/O loop.
    void acceptThreadFunc_();

    // I/O loop: decode inbound frames, send queued outbound frames.
    void ioThreadFunc_(int clientFd);

    // Read exactly n bytes from fd into buf.  Returns false on error/EOF.
    static bool readExactly_(int fd, uint8_t* buf, size_t n);

    // Read the HTTP request until \r\n\r\n.  Returns the full request string.
    static std::string readHttpRequest_(int fd);

    // Extract the Sec-WebSocket-Key header value from an HTTP request string.
    static std::string extractWsKey_(const std::string& request);

    // Compute the Sec-WebSocket-Accept value per RFC 6455.
    static std::string computeAccept_(const std::string& key);

    // Send the 101 Switching Protocols response.
    static bool sendHandshake_(int fd, const std::string& acceptValue);

    // Encode and send one RFC 6455 frame (server-to-client, unmasked).
    // opcode: 0x1 = text, 0x2 = binary.
    static bool sendWsFrame_(int fd, uint8_t opcode, const uint8_t* payload, size_t payloadLen);

    // Outbound frame queue -- mutex-protected.
    struct QueuedFrame
    {
        uint8_t opcode;
        std::vector<uint8_t> payload;
    };

    int listenFd_ = -1;
    int port_ = 0;

    std::atomic<bool> running_ { false };
    std::atomic<bool> clientConnected_ { false };

    std::thread acceptThread_;
    std::thread ioThread_;

    std::mutex queueMutex_;
    std::queue<QueuedFrame> outQueue_;

    std::function<void(const std::string&)> onTextFrame_;
    std::function<void(const std::vector<uint8_t>&)> onBinaryFrame_;
};

#endif // !defined(_WIN32)
#endif // MOCK_TCI_SERVER_H
