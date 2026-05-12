//=========================================================================
// Name:            TciReconnectTest.cpp
// Purpose:         Unit tests for auto-reconnect state cache and
//                  ConnectionStatus callback in TciRigController.
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
//
// What is being tested
// --------------------
// TciRigController now caches the last-known frequency and modulation so
// they can be replayed after an auto-reconnect, and exposes a four-state
// ConnectionStatus observable.
//
// The tests drive the controller via the testing-only API documented in
// TciRigController.h.  Because the controller checks connected_ == false
// before enqueueing most work, and the test environment has no real TCI
// server, the testing-only API bypasses guards:
//
//   tst_cachedFreqHz()           -- read the cached frequency atom
//   tst_cachedModeIndex()        -- read the cached modulation (as int)
//   tst_setStatus(s)             -- inject a status transition directly
//   tst_simulateWsConnected()    -- call onWsConnected_() without a server
//   tst_simulateWsDisconnected() -- call onWsDisconnected_() without a server
//
// Test 4 (replay_on_reconnect) verifies the no-crash path only.
// Real-WS command-capture coverage requires a live server; that is covered
// by bench validation (noted in the test comment).
//=========================================================================

#include "TciRigController.h"

#include <iostream>
#include <functional>
#include <string>
#include <atomic>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Minimal test harness -- matches freedv-gui unit test conventions.
// ---------------------------------------------------------------------------

inline void executeTestCase(const std::string& testName, std::function<bool()> const& fn)
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
// Helper: construct a disconnected controller (no real TCI server).
// ---------------------------------------------------------------------------
static std::shared_ptr<TciRigController> makeController()
{
    return std::make_shared<TciRigController>("localhost", 40001, /*trx=*/0, /*channel=*/0);
}

// ---------------------------------------------------------------------------
// Test 1: Fresh controller -- status is Disconnected.
// ---------------------------------------------------------------------------
bool test_initial_status_is_disconnected()
{
    auto ctrl = makeController();

    if (ctrl->connectionStatus() != TciRigController::ConnectionStatus::Disconnected)
    {
        std::cerr << "[connectionStatus() should be Disconnected at construction]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 2: setFrequency caches the value even when disconnected.
// ---------------------------------------------------------------------------
bool test_cached_freq_after_setFrequency()
{
    auto ctrl = makeController();

    // setFrequency caches before the connected_ guard fires.
    ctrl->setFrequency(7050000);

    if (ctrl->tst_cachedFreqHz() != 7050000)
    {
        std::cerr << "[cachedFreqHz_ should be 7050000 after setFrequency(7050000)]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 3: ConnectionStatus callback fires exactly once on each transition.
// ---------------------------------------------------------------------------
bool test_status_callback_fires_on_transition()
{
    auto ctrl = makeController();

    std::atomic<int> callCount{0};
    TciRigController::ConnectionStatus lastSeen =
        TciRigController::ConnectionStatus::Disconnected;

    ctrl->setOnConnectionStatusChange([&](TciRigController::ConnectionStatus s) {
        lastSeen = s;
        callCount.fetch_add(1);
    });

    // Inject Connected transition.
    ctrl->tst_setStatus(TciRigController::ConnectionStatus::Connected);

    if (callCount.load() != 1)
    {
        std::cerr << "[callback should fire exactly once on Connected transition]...";
        return false;
    }
    if (lastSeen != TciRigController::ConnectionStatus::Connected)
    {
        std::cerr << "[callback should receive Connected]...";
        return false;
    }

    // Same status again: callback must NOT fire (no duplicate transitions).
    ctrl->tst_setStatus(TciRigController::ConnectionStatus::Connected);
    if (callCount.load() != 1)
    {
        std::cerr << "[callback should not fire when status is unchanged]...";
        return false;
    }

    // Transition to Reconnecting.
    ctrl->tst_setStatus(TciRigController::ConnectionStatus::Reconnecting);
    if (callCount.load() != 2)
    {
        std::cerr << "[callback should fire on Reconnecting transition]...";
        return false;
    }
    if (lastSeen != TciRigController::ConnectionStatus::Reconnecting)
    {
        std::cerr << "[callback should receive Reconnecting]...";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Test 4: onWsConnected_ with a null wsClient_ does not crash.
//
// Limitation: a real-WS integration test (verifying that "vfo:0,0,7050000;"
// is actually sent on reconnect) requires a live mock server.  That path is
// covered by bench validation using the mock_tci_server fixture.
// ---------------------------------------------------------------------------
bool test_replay_on_reconnect_does_not_crash_without_client()
{
    // TciRigController always constructs its own wsClient_ internally, so
    // tst_simulateWsConnected() will have a valid (but disconnected) client.
    // The sends will silently fail because the socket is not open -- that is
    // the expected v1 behaviour documented in the task spec.
    auto ctrl = makeController();

    // Should not throw or crash.
    ctrl->tst_simulateWsConnected();

    // After simulating connect, status should be Connected.
    if (ctrl->connectionStatus() != TciRigController::ConnectionStatus::Connected)
    {
        std::cerr << "[connectionStatus() should be Connected after tst_simulateWsConnected]...";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Test 5: onWsDisconnected_ sets Reconnecting but preserves the freq cache.
// ---------------------------------------------------------------------------
bool test_disconnect_clears_status_but_preserves_cache()
{
    auto ctrl = makeController();

    // Cache a frequency.
    ctrl->setFrequency(14225000);

    // Move to Connected.
    ctrl->tst_setStatus(TciRigController::ConnectionStatus::Connected);

    // Simulate drop.
    ctrl->tst_simulateWsDisconnected();

    if (ctrl->connectionStatus() != TciRigController::ConnectionStatus::Reconnecting)
    {
        std::cerr << "[connectionStatus() should be Reconnecting after simulated disconnect]...";
        return false;
    }

    if (ctrl->tst_cachedFreqHz() != 14225000)
    {
        std::cerr << "[cachedFreqHz_ should be preserved across disconnect]...";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_CASE(test_initial_status_is_disconnected);
    TEST_CASE(test_cached_freq_after_setFrequency);
    TEST_CASE(test_status_callback_fires_on_transition);
    TEST_CASE(test_replay_on_reconnect_does_not_crash_without_client);
    TEST_CASE(test_disconnect_clears_status_but_preserves_cache);

    return 0;
}
