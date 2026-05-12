//=========================================================================
// Name:            TciMoxGateTest.cpp
// Purpose:         Unit tests for the multi-client MOX gate state machine
//                  in TciRigController.
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
// TciRigController holds three atomics that form a state machine used to
// distinguish "we pressed PTT and the server confirmed it" from "another
// client or foot switch keyed the radio".  maySendTxAudio() is the sole
// correct gate for outgoing TX audio.
//
// The tests drive the machine via the testing-only API documented in
// TciRigController.h.  Because the controller checks connected_ == true
// before enqueueing ptt() work, and the test environment has no real TCI
// server, we bypass the connected_ guard by using:
//
//   tst_armPendingPtt()   -- equivalent to "ptt(true) on a connected controller"
//   tst_clearAllFlags()   -- equivalent to "ptt(false) on a connected controller"
//   tst_injectTrxEcho()   -- simulate a TRX:0,true/false; server echo
//=========================================================================

#include "TciRigController.h"

#include <iostream>
#include <functional>
#include <string>
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
// Helper: construct a controller backed by an internal (disconnected) WS
// client.  TciRigController always creates its own TciWebSocketClient
// internally; it is never connected in unit tests, so no network traffic
// is generated and ptt() returns early at the connected_ guard.
// ---------------------------------------------------------------------------
static std::shared_ptr<TciRigController> makeController()
{
    return std::make_shared<TciRigController>("localhost", 40001, /*trx=*/0, /*channel=*/0);
}

// ---------------------------------------------------------------------------
// Test 1: Fresh controller -- all gate flags are false.
// ---------------------------------------------------------------------------
bool test_initial_state_no_tx()
{
    auto ctrl = makeController();

    if (ctrl->tst_ourPttActive())
    {
        std::cerr << "[our_ptt_active_ should be false at construction]...";
        return false;
    }
    if (ctrl->tst_otherClientMox())
    {
        std::cerr << "[other_client_mox_ should be false at construction]...";
        return false;
    }
    if (ctrl->tst_pendingPttRequest())
    {
        std::cerr << "[pending_ptt_request_ should be false at construction]...";
        return false;
    }
    if (ctrl->tst_maySendTxAudio())
    {
        std::cerr << "[maySendTxAudio() should be false at construction]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 2: We press PTT -> pending set; then server confirms -> TX allowed.
//
// Flow:
//   tst_armPendingPtt()        => pending_ptt_request_ = true  (simulates ptt(true))
//                                 our_ptt_active_ = false (not yet confirmed)
//                                 maySendTxAudio() = false
//   tst_injectTrxEcho(true)    => our_ptt_active_ = true
//                                 pending_ptt_request_ = false
//                                 maySendTxAudio() = true
// ---------------------------------------------------------------------------
bool test_we_press_ptt_then_echo_grants_tx()
{
    auto ctrl = makeController();

    // Arm the pending flag (as ptt(true) would do on a connected controller).
    ctrl->tst_armPendingPtt();

    if (!ctrl->tst_pendingPttRequest())
    {
        std::cerr << "[pending_ptt_request_ should be true after arming]...";
        return false;
    }
    if (ctrl->tst_ourPttActive())
    {
        std::cerr << "[our_ptt_active_ should be false before echo]...";
        return false;
    }
    if (ctrl->tst_maySendTxAudio())
    {
        std::cerr << "[maySendTxAudio() should be false before echo]...";
        return false;
    }

    // Server echoes MOX-on back.
    ctrl->tst_injectTrxEcho(true);

    if (!ctrl->tst_ourPttActive())
    {
        std::cerr << "[our_ptt_active_ should be true after echo confirms our request]...";
        return false;
    }
    if (ctrl->tst_otherClientMox())
    {
        std::cerr << "[other_client_mox_ should be false when we are the TX source]...";
        return false;
    }
    if (ctrl->tst_pendingPttRequest())
    {
        std::cerr << "[pending_ptt_request_ should be cleared after echo confirms]...";
        return false;
    }
    if (!ctrl->tst_maySendTxAudio())
    {
        std::cerr << "[maySendTxAudio() should be true after echo confirms our PTT]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 3: Another client keys the radio (no pending request from us).
//
// Flow:
//   tst_injectTrxEcho(true)    => other_client_mox_ = true
//                                 our_ptt_active_ = false
//                                 maySendTxAudio() = false
// ---------------------------------------------------------------------------
bool test_other_client_mox_blocks_tx()
{
    auto ctrl = makeController();

    // MOX-on arrives without us having armed a pending request.
    ctrl->tst_injectTrxEcho(true);

    if (ctrl->tst_ourPttActive())
    {
        std::cerr << "[our_ptt_active_ should be false when another client keyed]...";
        return false;
    }
    if (!ctrl->tst_otherClientMox())
    {
        std::cerr << "[other_client_mox_ should be true when another client keyed]...";
        return false;
    }
    if (ctrl->tst_maySendTxAudio())
    {
        std::cerr << "[maySendTxAudio() should be false when another client holds MOX]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 4: MOX-off clears all state.
//
// Flow:
//   tst_armPendingPtt()
//   tst_injectTrxEcho(true)    -- we are in TX
//   tst_injectTrxEcho(false)   => all three flags cleared
// ---------------------------------------------------------------------------
bool test_mox_off_clears_state()
{
    auto ctrl = makeController();

    ctrl->tst_armPendingPtt();
    ctrl->tst_injectTrxEcho(true);

    // Sanity: we should be in TX now.
    if (!ctrl->tst_ourPttActive())
    {
        std::cerr << "[sanity: our_ptt_active_ should be true mid-TX]...";
        return false;
    }

    // Server signals MOX-off.
    ctrl->tst_injectTrxEcho(false);

    if (ctrl->tst_ourPttActive())
    {
        std::cerr << "[our_ptt_active_ should be false after MOX-off]...";
        return false;
    }
    if (ctrl->tst_otherClientMox())
    {
        std::cerr << "[other_client_mox_ should be false after MOX-off]...";
        return false;
    }
    if (ctrl->tst_pendingPttRequest())
    {
        std::cerr << "[pending_ptt_request_ should be false after MOX-off]...";
        return false;
    }
    if (ctrl->tst_maySendTxAudio())
    {
        std::cerr << "[maySendTxAudio() should be false after MOX-off]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 5: Local PTT release (tst_clearAllFlags) clears state immediately,
//         even before any server echo arrives.
//
// Flow:
//   tst_armPendingPtt()
//   tst_injectTrxEcho(true)    -- we are in TX
//   tst_clearAllFlags()        => all three flags cleared (local immediate clear,
//                                 analogous to ptt(false) on a connected controller)
// ---------------------------------------------------------------------------
bool test_our_ptt_local_release_clears_state()
{
    auto ctrl = makeController();

    ctrl->tst_armPendingPtt();
    ctrl->tst_injectTrxEcho(true);

    if (!ctrl->tst_ourPttActive())
    {
        std::cerr << "[sanity: our_ptt_active_ should be true mid-TX]...";
        return false;
    }

    // Local release clears all flags eagerly, as ptt(false) does on a connected controller.
    ctrl->tst_clearAllFlags();

    if (ctrl->tst_ourPttActive())
    {
        std::cerr << "[our_ptt_active_ should be false after local PTT release]...";
        return false;
    }
    if (ctrl->tst_pendingPttRequest())
    {
        std::cerr << "[pending_ptt_request_ should be false after local PTT release]...";
        return false;
    }
    if (ctrl->tst_otherClientMox())
    {
        std::cerr << "[other_client_mox_ should be false after local PTT release]...";
        return false;
    }
    if (ctrl->tst_maySendTxAudio())
    {
        std::cerr << "[maySendTxAudio() should be false after local PTT release]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 6: Redundant trx:0,true echo from server while we are already
//         transmitting must NOT be mis-credited to "another client".
//
// Bug history: an early version of handleTrxEcho_ consumed
// pending_ptt_request_ on the first MOX-on echo, so the next echo (a
// server state-sync push, which TCI servers commonly emit after
// audio_start / mode change / other-client connect) fell through to
// the "another source keyed the radio" branch.  That set
// other_client_mox_=true and closed maySendTxAudio(), causing the radio
// to drop MOX after a brief moment because the audio stream stopped.
//
// Flow:
//   tst_armPendingPtt() + tst_injectTrxEcho(true)
//                                          => our_ptt_active_=true, maySendTxAudio=true
//   tst_injectTrxEcho(true)   (repeat)     => MUST remain our_ptt_active_=true,
//                                             other_client_mox_=false, maySendTxAudio=true
//   tst_injectTrxEcho(true)   (third)      => same; idempotent
// ---------------------------------------------------------------------------
bool test_redundant_mox_echo_stays_ours()
{
    auto ctrl = makeController();

    ctrl->tst_armPendingPtt();
    ctrl->tst_injectTrxEcho(true);

    if (!ctrl->tst_ourPttActive() || !ctrl->tst_maySendTxAudio() ||
        ctrl->tst_otherClientMox())
    {
        std::cerr << "[first echo should claim our PTT]...";
        return false;
    }

    // Server pushes the SAME state again (idempotent broadcast).
    ctrl->tst_injectTrxEcho(true);

    if (!ctrl->tst_ourPttActive())
    {
        std::cerr << "[our_ptt_active_ should remain true after redundant echo]...";
        return false;
    }
    if (ctrl->tst_otherClientMox())
    {
        std::cerr << "[other_client_mox_ should NOT be set by redundant echo]...";
        return false;
    }
    if (!ctrl->tst_maySendTxAudio())
    {
        std::cerr << "[maySendTxAudio() should remain true on redundant echo]...";
        return false;
    }

    // And a third time.
    ctrl->tst_injectTrxEcho(true);

    if (!ctrl->tst_maySendTxAudio() || ctrl->tst_otherClientMox())
    {
        std::cerr << "[state should still be ours after third echo]...";
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_CASE(test_initial_state_no_tx);
    TEST_CASE(test_we_press_ptt_then_echo_grants_tx);
    TEST_CASE(test_other_client_mox_blocks_tx);
    TEST_CASE(test_mox_off_clears_state);
    TEST_CASE(test_our_ptt_local_release_clears_state);
    TEST_CASE(test_redundant_mox_echo_stays_ours);

    return 0;
}
