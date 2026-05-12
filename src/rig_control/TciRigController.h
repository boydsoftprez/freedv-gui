//=========================================================================
// Name:            TciRigController.h
// Purpose:         Controls radios using TCI protocol
//
// Authors:         Tomas Ostojic
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

#ifndef TCI_RIG_CONTROLLER_H
#define TCI_RIG_CONTROLLER_H

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <map>

#include "util/ThreadedObject.h"
#include "IRigFrequencyController.h"
#include "IRigPttController.h"
#include "TciWebSocketClient.h"
#include "TciProtocol.h"

class TciRigController : public ThreadedObject,
                         public IRigFrequencyController,
                         public IRigPttController
{
public:
    TciRigController(std::string hostname, int port = 40001, int trx = 0, int channel = 0);
    virtual ~TciRigController();

    // IRigController interface
    virtual void connect() override;
    virtual void disconnect() override;
    virtual bool isConnected() override;

    // IRigPttController interface
    virtual void ptt(bool state) override;
    virtual int getRigResponseTimeMicroseconds() override;

    // IRigFrequencyController interface
    virtual void setFrequency(uint64_t frequency) override;
    virtual void setMode(Mode mode) override;
    virtual void requestCurrentFrequencyMode() override;

    // TCI-specific methods
    void setTrxChannel(int trx, int channel);
    int getTrx() const { return trx_; }
    int getChannel() const { return channel_; }

    // Get TCI WebSocket client for audio device
    std::shared_ptr<tci::TciWebSocketClient> getWebSocketClient();

    // Connection-status observable.
    //
    // Four states track the lifecycle from first connect through auto-reconnect.
    // The callback fires on whatever thread the WS open/close handler runs on
    // (the TcpConnectionHandler worker thread). UI consumers are responsible
    // for marshalling to their own main thread (e.g. wxPostEvent / CallAfter).
    enum class ConnectionStatus { Disconnected, Connecting, Connected, Reconnecting };

    using ConnectionStatusCallback = std::function<void(ConnectionStatus)>;

    void setOnConnectionStatusChange(ConnectionStatusCallback cb);
    ConnectionStatus connectionStatus() const;
    
private:
    std::string hostname_;
    int port_;
    int trx_;           // TCI transceiver number (usually 0)
    int channel_;       // TCI channel number (usually 0)

    std::shared_ptr<tci::TciWebSocketClient> wsClient_;
    std::atomic<bool> connected_;
    std::atomic<bool> pttState_;

    // Multi-client MOX gate state machine.
    //
    // Problem: with two TCI clients on the same server (e.g. freedv-gui +
    // WSJT-X), a TRX echo from the server does not indicate WHICH client
    // pressed PTT.  The single pttState_ flag cannot distinguish "we keyed
    // up" from "another client keyed up", so TX audio would leak onto another
    // client's transmission.
    //
    // Solution: three fine-grained atomics track the request-confirm round
    // trip.  maySendTxAudio() is the only correct TX-audio gate.
    std::atomic<bool> pending_ptt_request_{false};  // set by setPtt(true), cleared on echo
    std::atomic<bool> our_ptt_active_{false};        // set only when echo confirms our request
    std::atomic<bool> other_client_mox_{false};      // set when MOX echo arrives without a pending request

    std::mutex stateMutex_;
    uint64_t currentFrequency_;
    tci::Modulation currentModulation_;

    // TCI device capabilities
    std::string deviceName_;
    int trxCount_;
    int channelCount_;
    std::vector<std::string> supportedModulations_;
    uint64_t minFrequency_;
    uint64_t maxFrequency_;

    // Response time for PTT
    const int rigResponseTime_ = 100000; // 100ms in microseconds

    // State cache for reconnect resync. Updated as commands are sent or echoed;
    // replayed on reconnect after WS handshake completes.
    std::atomic<uint64_t> cachedFreqHz_{0};
    // -1 = no cache yet; else stores tci::Modulation cast to int.
    std::atomic<int> cachedModeIndex_{-1};

    // Connection-status tracking.
    std::atomic<ConnectionStatus> connStatus_{ConnectionStatus::Disconnected};
    ConnectionStatusCallback onStatusCb_;
    void setStatus_(ConnectionStatus s);

    // Command handlers
    void handleCommand_(const std::string& cmdName, const std::vector<std::string>& args);
    void handleProtocol_(const std::vector<std::string>& args);
    void handleDevice_(const std::vector<std::string>& args);
    void handleTrxCount_(const std::vector<std::string>& args);
    void handleChannelCount_(const std::vector<std::string>& args);
    void handleModulationsList_(const std::vector<std::string>& args);
    void handleVfoLimits_(const std::vector<std::string>& args);
    void handleVfo_(const std::vector<std::string>& args);
    void handleModulation_(const std::vector<std::string>& args);
    void handleTrx_(const std::vector<std::string>& args);

    // State machine helper: drives the three MOX gate atomics on every
    // TRX echo received from the server, then fires onPttChange.
    void handleTrxEcho_(bool moxOn);

    // Helper methods
    void sendCommand_(const std::string& cmdName, const std::vector<std::string>& args);
    tci::Modulation freeDvModeToTci_(Mode mode);
    Mode tciModeToFreeDv_(tci::Modulation mod);

    // Connection event handlers (called from wsClient_ callbacks).
    void onWsConnected_();
    void onWsDisconnected_();
    void onError_(const std::string& error);

public:
    // Returns true only when we pressed PTT and the server has confirmed it.
    // Gate all outgoing TX audio on this predicate to prevent leaking audio
    // onto transmissions started by another client or a foot switch.
    bool maySendTxAudio() const
    {
        return our_ptt_active_.load() && !other_client_mox_.load();
    }

    // --- BEGIN testing-only API ---
    // These accessors let unit tests drive the MOX gate state machine and
    // reconnect cache without a real TCI server.  Not for production callers.
    //
    // Note: ptt() returns early when connected_ == false (no server).
    // Tests drive the machine by setting pending_ptt_request_ directly via
    // tst_armPendingPtt() and then injecting echoes via tst_injectTrxEcho().
    bool tst_ourPttActive()      const { return our_ptt_active_.load(); }
    bool tst_otherClientMox()    const { return other_client_mox_.load(); }
    bool tst_pendingPttRequest() const { return pending_ptt_request_.load(); }
    bool tst_maySendTxAudio()    const { return maySendTxAudio(); }
    void tst_injectTrxEcho(bool moxOn) { handleTrxEcho_(moxOn); }
    // Arm the pending flag as if setPtt(true) had been called on a connected controller.
    void tst_armPendingPtt() { pending_ptt_request_.store(true); }
    // Clear all three flags as if setPtt(false) had been called on a connected controller.
    void tst_clearAllFlags()
    {
        our_ptt_active_.store(false);
        pending_ptt_request_.store(false);
        other_client_mox_.store(false);
    }
    // Reconnect state cache accessors.
    uint64_t tst_cachedFreqHz()      const { return cachedFreqHz_.load(); }
    int      tst_cachedModeIndex()   const { return cachedModeIndex_.load(); }
    // Status injection (drives setStatus_ directly so callback fires without a real WS).
    void tst_setStatus(ConnectionStatus s) { setStatus_(s); }
    // Simulate WS open/close events without a real server.
    void tst_simulateWsConnected()    { onWsConnected_(); }
    void tst_simulateWsDisconnected() { onWsDisconnected_(); }
    // --- END testing-only API ---
};

#endif // TCI_RIG_CONTROLLER_H
