//=========================================================================
// Name:            TciWebSocketClientTest.cpp
// Purpose:         Integration smoke tests for tci::TciWebSocketClient using
//                  the MockTciServer localhost fixture.
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
#include "rig_control/TciWebSocketClient.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal test harness (same style as TciProtocolTest.cpp).
// ---------------------------------------------------------------------------

inline void executeTestCase(const std::string& testName,
                            std::function<bool()> const& fn)
{
    std::cout << "Executing " << testName << "...";
    if (fn())
    {
        std::cout << "passed" << std::endl;
    }
    else
    {
        std::cout << "FAILED" << std::endl;
        exit(-1); // NOLINT
    }
}

#define TEST_CASE(name) executeTestCase(#name, name)

// ---------------------------------------------------------------------------
// Polling helper: busy-waits up to maxWaitMs for pred() to return true.
// ---------------------------------------------------------------------------
static bool waitFor(std::function<bool()> pred, int maxWaitMs = 2000)
{
    const int kStepMs = 10;
    int elapsed = 0;
    while (elapsed < maxWaitMs)
    {
        if (pred()) { return true; }
        std::this_thread::sleep_for(std::chrono::milliseconds(kStepMs));
        elapsed += kStepMs;
    }
    return pred(); // one final check
}

// ---------------------------------------------------------------------------
// Test 1: test_connect_handshake
// The client completes the RFC 6455 handshake and both sides report connected.
// ---------------------------------------------------------------------------
bool test_connect_handshake()
{
    MockTciServer mock;
    if (!mock.start())
    {
        std::cerr << "[mock.start() failed]...";
        return false;
    }

    std::atomic<bool> onConnectedFired { false };

    tci::TciWebSocketClient client;
    client.setOnConnected([&onConnectedFired]() {
        onConnectedFired = true;
    });

    bool connected = client.connect("127.0.0.1", mock.getPort());
    if (!connected)
    {
        std::cerr << "[client.connect() returned false]...";
        mock.stop();
        return false;
    }

    // Wait for onConnected callback and for the mock to acknowledge the client.
    bool cbFired = waitFor([&onConnectedFired]() { return onConnectedFired.load(); }, 2000);
    if (!cbFired)
    {
        std::cerr << "[onConnected callback did not fire within 2 s]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    bool mockSees = waitFor([&mock]() { return mock.isClientConnected(); }, 2000);
    if (!mockSees)
    {
        std::cerr << "[mock.isClientConnected() false after 2 s]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    if (!client.isConnected())
    {
        std::cerr << "[client.isConnected() false after handshake]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    client.disconnect();
    mock.stop();
    return true;
}

// ---------------------------------------------------------------------------
// Test 2: test_send_text_frame
// The client sends a TCI command; the mock receives it as a raw text frame.
//
// Note: TciWebSocketClient::sendCommand() sends the string verbatim as a
// WebSocket text frame (opcode 0x1).  MockTciServer::onTextFrame_ receives
// the raw payload string, so we compare against the exact string sent.
// ---------------------------------------------------------------------------
bool test_send_text_frame()
{
    MockTciServer mock;
    if (!mock.start())
    {
        std::cerr << "[mock.start() failed]...";
        return false;
    }

    std::atomic<bool> receivedFrame { false };
    std::string receivedText;

    mock.setOnTextFrame([&receivedFrame, &receivedText](const std::string& text) {
        receivedText = text;
        receivedFrame = true;
    });

    std::atomic<bool> onConnectedFired { false };

    tci::TciWebSocketClient client;
    client.setOnConnected([&onConnectedFired]() {
        onConnectedFired = true;
    });

    if (!client.connect("127.0.0.1", mock.getPort()))
    {
        std::cerr << "[client.connect() failed]...";
        mock.stop();
        return false;
    }

    // Wait for handshake to complete before sending.
    if (!waitFor([&onConnectedFired]() { return onConnectedFired.load(); }, 2000))
    {
        std::cerr << "[onConnected did not fire before send]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    const std::string kCommand = "dds:0,7050000;";
    if (!client.sendCommand(kCommand))
    {
        std::cerr << "[sendCommand() returned false]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    // Wait for the mock's I/O thread to decode and deliver the text frame.
    bool got = waitFor([&receivedFrame]() { return receivedFrame.load(); }, 1000);
    if (!got)
    {
        std::cerr << "[mock did not receive text frame within 1 s]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    if (receivedText != kCommand)
    {
        std::cerr << "[received text mismatch: expected \"" << kCommand
                  << "\" got \"" << receivedText << "\"]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    client.disconnect();
    mock.stop();
    return true;
}

// ---------------------------------------------------------------------------
// Test 3: test_receive_text_frame
// The mock queues a text frame before the client connects; after the handshake
// the frame is flushed to the client, which parses it through CommandParser
// and invokes the commandCallback_ with (cmdName, args).
//
// "dds:0,14070000;" parses to cmdName="DDS", args=["0","14070000"].
// ---------------------------------------------------------------------------
bool test_receive_text_frame()
{
    MockTciServer mock;

    // Queue the frame before start() -- the I/O loop flushes the queue after
    // the handshake completes, so this also covers the "pre-connect queue" path.
    mock.queueTextFrame("dds:0,14070000;");

    if (!mock.start())
    {
        std::cerr << "[mock.start() failed]...";
        return false;
    }

    std::atomic<bool> callbackFired { false };
    std::string gotCmdName;
    std::vector<std::string> gotArgs;

    tci::TciWebSocketClient client;
    client.setCommandCallback([&callbackFired, &gotCmdName, &gotArgs](
        const std::string& cmdName,
        const std::vector<std::string>& args)
    {
        gotCmdName = cmdName;
        gotArgs = args;
        callbackFired = true;
    });

    if (!client.connect("127.0.0.1", mock.getPort()))
    {
        std::cerr << "[client.connect() failed]...";
        mock.stop();
        return false;
    }

    // Allow time for the handshake + frame delivery + callback invocation.
    bool got = waitFor([&callbackFired]() { return callbackFired.load(); }, 2000);
    if (!got)
    {
        std::cerr << "[commandCallback did not fire within 2 s]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    // CommandParser uppercases the command name.
    if (gotCmdName != "DDS")
    {
        std::cerr << "[cmdName mismatch: expected \"DDS\" got \""
                  << gotCmdName << "\"]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    if (gotArgs.size() != 2 ||
        gotArgs[0] != "0" ||
        gotArgs[1] != "14070000")
    {
        std::cerr << "[args mismatch]...";
        client.disconnect();
        mock.stop();
        return false;
    }

    client.disconnect();
    mock.stop();
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_CASE(test_connect_handshake);
    TEST_CASE(test_send_text_frame);
    TEST_CASE(test_receive_text_frame);
    return 0;
}

#else
// Windows -- tests not yet ported; no-op main so CMake test still links.
int main() { return 0; }
#endif // !defined(_WIN32)
