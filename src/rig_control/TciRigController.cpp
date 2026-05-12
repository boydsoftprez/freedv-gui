//=========================================================================
// Name:            TciRigController.cpp
// Purpose:         Controls radios using TCI protocol implementation
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

#include "TciRigController.h"
#include <sstream>
#include <iostream>

TciRigController::TciRigController(std::string hostname, int port, int trx, int channel)
    : ThreadedObject("TciRigController")
    , hostname_(std::move(hostname))
    , port_(port)
    , trx_(trx)
    , channel_(channel)
    , connected_(false)
    , pttState_(false)
    , currentFrequency_(0)
    , currentModulation_(tci::DIGU)
    , trxCount_(1)
    , channelCount_(1)
    , minFrequency_(0)
    , maxFrequency_(60000000)
{
#ifdef TCI_DEBUG_LOGGING
    fprintf(stderr, "TciRigController constructor: hostname='%s' port=%d\n", hostname_.c_str(), port_);
#endif
    
    wsClient_ = std::make_shared<tci::TciWebSocketClient>();

    // Set up callbacks
    wsClient_->setOnConnected([this]() { onWsConnected_(); });
    wsClient_->setOnDisconnected([this]() { onWsDisconnected_(); });
    wsClient_->setOnError([this](const std::string& error) { onError_(error); });
    
    wsClient_->setCommandCallback([this](const std::string& cmdName, const std::vector<std::string>& args) {
        enqueue_([this, cmdName, args]() {
            handleCommand_(cmdName, args);
        });
    });
}

TciRigController::~TciRigController()
{
    disconnect();
}

void TciRigController::connect()
{
    if (connected_)
    {
        return;
    }

    setStatus_(ConnectionStatus::Connecting);

    enqueue_([this]() {
        if (wsClient_->connect(hostname_, port_))
        {
            // TCP layer kicked off; WS handshake completes asynchronously.
            // onWsConnected_() fires once the open_handler runs.
        }
        else
        {
            setStatus_(ConnectionStatus::Disconnected);
            onRigError(this, "Failed to connect to TCI server at " + hostname_ + ":" + std::to_string(port_));
        }
    });
}

void TciRigController::disconnect()
{
    if (!connected_)
    {
        return;
    }

    enqueue_([this]() {
        // Turn off PTT if it's on
        if (pttState_)
        {
            ptt(false);
        }

        wsClient_->disconnect();
        connected_ = false;
        setStatus_(ConnectionStatus::Disconnected);
    });

    waitForAllTasksComplete_();
}

bool TciRigController::isConnected()
{
    return connected_;
}

void TciRigController::ptt(bool state)
{
#ifdef TCI_DEBUG_LOGGING
    fprintf(stderr, "\n=== TCI PTT REQUEST ===\n");
    fprintf(stderr, "TCI PTT: Requested state = %s\n", state ? "ON (TRANSMIT)" : "OFF (RECEIVE)");
    fprintf(stderr, "TCI PTT: Connected = %s\n", connected_ ? "YES" : "NO");
    fprintf(stderr, "TCI PTT: TRX number = %d\n", trx_);
#endif
    
    if (!connected_)
    {
#ifdef TCI_DEBUG_LOGGING
        fprintf(stderr, "TCI PTT: ERROR - Not connected, cannot set PTT\n");
        fprintf(stderr, "======================\n\n");
#endif
        return;
    }
    
#ifdef TCI_DEBUG_LOGGING
    fprintf(stderr, "TCI PTT: Enqueueing PTT command...\n");
#endif
    
    enqueue_([this, state]() {
#ifdef TCI_DEBUG_LOGGING
        fprintf(stderr, "TCI PTT: [QUEUE] Processing PTT command\n");
        fprintf(stderr, "TCI PTT: [QUEUE] Previous PTT state = %s\n", pttState_ ? "ON" : "OFF");
#endif

        if (state) {
            // Arm the pending flag BEFORE sending so the echo handler
            // can attribute the upcoming TRX:0,true; echo to us.
            pending_ptt_request_.store(true);
        } else {
            // Immediate local clear: stop sending TX audio right away, even
            // before the server echo arrives.
            our_ptt_active_.store(false);
            pending_ptt_request_.store(false);
            other_client_mox_.store(false);
        }

        pttState_ = state;

#ifdef TCI_DEBUG_LOGGING
        fprintf(stderr, "TCI PTT: [QUEUE] New PTT state = %s\n", pttState_ ? "ON" : "OFF");
#endif

        // TRX:trx_num,state[,signal_source];
        // CRITICAL: When PTT ON, must specify signal source "tci" as third parameter
        // Otherwise eesdr3 defaults to microphone and never sends TX_CHRONO!
        std::vector<std::string> args;
        args.push_back(std::to_string(trx_));
        args.push_back(state ? "true" : "false");

        if (state) {
            // PTT ON: Specify "tci" as signal source so eesdr3 expects audio via TCI
            args.push_back("tci");
#ifdef TCI_DEBUG_LOGGING
            fprintf(stderr, "TCI PTT: [QUEUE] Calling sendCommand_(\"trx\", [%d, %s, tci])\n",
                    trx_, state ? "true" : "false");
#endif
        } else {
            // PTT OFF: No signal source needed
#ifdef TCI_DEBUG_LOGGING
            fprintf(stderr, "TCI PTT: [QUEUE] Calling sendCommand_(\"trx\", [%d, %s])\n",
                    trx_, state ? "true" : "false");
#endif
        }

        sendCommand_("trx", args);

#ifdef TCI_DEBUG_LOGGING
        fprintf(stderr, "TCI PTT: [QUEUE] sendCommand_() returned\n");
        fprintf(stderr, "TCI PTT: [QUEUE] Notifying listeners...\n");
#endif

        // Notify listeners immediately (preserves existing semantics; the echo
        // from handleTrxEcho_ will fire a second notification when confirmed).
        onPttChange(this, state);

#ifdef TCI_DEBUG_LOGGING
        fprintf(stderr, "TCI PTT: [QUEUE] PTT command complete\n");
        fprintf(stderr, "======================\n\n");
#endif
    });
}

void TciRigController::setFrequency(uint64_t frequency)
{
    // Cache even when disconnected so replay on reconnect has the latest value.
    cachedFreqHz_.store(frequency);

    if (!connected_)
    {
        return;
    }

    enqueue_([this, frequency]() {
        std::lock_guard<std::mutex> lock(stateMutex_);

        // VFO:trx,vfo,frequency;
        std::vector<std::string> args;
        args.push_back(std::to_string(trx_));
        args.push_back(std::to_string(channel_));
        args.push_back(std::to_string(frequency));

        sendCommand_("vfo", args);

        currentFrequency_ = frequency;
    });
}

void TciRigController::setMode(Mode mode)
{
    // Cache even when disconnected so replay on reconnect has the latest value.
    cachedModeIndex_.store(static_cast<int>(freeDvModeToTci_(mode)));

    if (!connected_)
    {
        return;
    }

    enqueue_([this, mode]() {
        std::lock_guard<std::mutex> lock(stateMutex_);

        tci::Modulation tciMod = freeDvModeToTci_(mode);

        // MODULATION:trx,modulation;
        std::vector<std::string> args;
        args.push_back(std::to_string(trx_));
        args.push_back(tci::CommandParser::modulationToString(tciMod));

        sendCommand_("modulation", args);

        currentModulation_ = tciMod;
    });
}

void TciRigController::requestCurrentFrequencyMode()
{
    // TCI protocol sends frequency and mode updates automatically when they change.
    // No need to poll - the server pushes updates via VFO: and MODULATION: commands
    // which are handled by handleVfo_() and handleModulation_() callbacks.
    // Polling would just create unnecessary command spam on the network.
}

int TciRigController::getRigResponseTimeMicroseconds()
{
    return rigResponseTime_;
}

void TciRigController::setTrxChannel(int trx, int channel)
{
    trx_ = trx;
    channel_ = channel;
}

std::shared_ptr<tci::TciWebSocketClient> TciRigController::getWebSocketClient()
{
    return wsClient_;
}

void TciRigController::handleCommand_(const std::string& cmdName, const std::vector<std::string>& args)
{
    if (cmdName == "PROTOCOL")
    {
        handleProtocol_(args);
    }
    else if (cmdName == "DEVICE")
    {
        handleDevice_(args);
    }
    else if (cmdName == "TRX_COUNT")
    {
        handleTrxCount_(args);
    }
    else if (cmdName == "CHANNEL_COUNT")
    {
        handleChannelCount_(args);
    }
    else if (cmdName == "MODULATIONS_LIST")
    {
        handleModulationsList_(args);
    }
    else if (cmdName == "VFO_LIMITS")
    {
        handleVfoLimits_(args);
    }
    else if (cmdName == "VFO")
    {
        handleVfo_(args);
    }
    else if (cmdName == "MODULATION")
    {
        handleModulation_(args);
    }
    else if (cmdName == "TRX")
    {
        handleTrx_(args);
    }
}

void TciRigController::handleProtocol_(const std::vector<std::string>& args)
{
    if (args.size() >= 2)
    {
        std::string protocol = args[0];
        std::string version = args[1];
        std::cout << "TCI Protocol: " << protocol << " v" << version << std::endl;
    }
}

void TciRigController::handleDevice_(const std::vector<std::string>& args)
{
    if (args.size() >= 1)
    {
        deviceName_ = args[0];
        std::cout << "TCI Device: " << deviceName_ << std::endl;
    }
}

void TciRigController::handleTrxCount_(const std::vector<std::string>& args)
{
    if (args.size() >= 1)
    {
        trxCount_ = std::stoi(args[0]);
    }
}

void TciRigController::handleChannelCount_(const std::vector<std::string>& args)
{
    if (args.size() >= 1)
    {
        channelCount_ = std::stoi(args[0]);
    }
}

void TciRigController::handleModulationsList_(const std::vector<std::string>& args)
{
    supportedModulations_ = args;
}

void TciRigController::handleVfoLimits_(const std::vector<std::string>& args)
{
    if (args.size() >= 2)
    {
        minFrequency_ = std::stoull(args[0]);
        maxFrequency_ = std::stoull(args[1]);
    }
}

void TciRigController::handleVfo_(const std::vector<std::string>& args)
{
    if (args.size() >= 3)
    {
        int trx = std::stoi(args[0]);
        int vfo = std::stoi(args[1]);
        uint64_t frequency = std::stoull(args[2]);
        
        if (trx == trx_ && vfo == channel_)
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            currentFrequency_ = frequency;
            
            // Notify listeners
            onFreqModeChange(this, currentFrequency_, tciModeToFreeDv_(currentModulation_));
        }
    }
}

void TciRigController::handleModulation_(const std::vector<std::string>& args)
{
    if (args.size() >= 2)
    {
        int trx = std::stoi(args[0]);
        std::string modStr = args[1];
        
        if (trx == trx_)
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            currentModulation_ = tci::CommandParser::stringToModulation(modStr);
            
            // Notify listeners
            onFreqModeChange(this, currentFrequency_, tciModeToFreeDv_(currentModulation_));
        }
    }
}

void TciRigController::handleTrx_(const std::vector<std::string>& args)
{
    if (args.size() >= 2)
    {
        int trx = std::stoi(args[0]);
        // args[1] is the raw token from the server -- the parser uppercases
        // the command NAME but leaves argument values verbatim, so the server
        // sends lowercase "true"/"false" (confirmed by parseTrxTrue/False tests).
        bool state = (args[1] == "true");

        if (trx == trx_)
        {
            handleTrxEcho_(state);
        }
    }
}

void TciRigController::handleTrxEcho_(bool moxOn)
{
    if (moxOn)
    {
        // Idempotency: a `trx:0,true` echo that arrives while we are already
        // in a known MOX state is a server state-sync push (TCI servers
        // commonly re-broadcast state after audio_start, mode change, other
        // client connect, etc.). It is NOT a new ownership signal. Without
        // this guard, the second echo lands in the else-branch below and
        // mis-credits the PTT to "another client", which closes our TX gate
        // and the radio drops MOX after a brief moment.
        if (our_ptt_active_.load() || other_client_mox_.load())
        {
            pttState_.store(true);
            return;
        }

        if (pending_ptt_request_.load())
        {
            // The server echoed MOX-on while we had a pending request:
            // this echo is the confirmation that OUR PTT is active.
            our_ptt_active_.store(true);
            pending_ptt_request_.store(false);
            other_client_mox_.store(false);
        }
        else
        {
            // MOX-on arrived without a pending request from us:
            // another source (foot switch, second client) keyed the radio.
            other_client_mox_.store(true);
        }
    }
    else
    {
        // MOX off: server is back to RX regardless of who initiated TX.
        our_ptt_active_.store(false);
        other_client_mox_.store(false);
        pending_ptt_request_.store(false);
    }

    // Preserve existing pttState_ semantics and fire the callback.
    pttState_.store(moxOn);
    onPttChange(this, moxOn);
}

void TciRigController::sendCommand_(const std::string& cmdName, const std::vector<std::string>& args)
{
    std::string command = tci::CommandParser::buildCommand(cmdName, args);
    wsClient_->sendCommand(command);
}

tci::Modulation TciRigController::freeDvModeToTci_(Mode mode)
{
    // Map FreeDV modes to TCI modes
    switch (mode)
    {
        case DIGU:
            return tci::DIGU;
            
        case USB:
            return tci::USB;
            
        case DIGL:
            return tci::DIGL;
            
        case LSB:
            return tci::LSB;
            
        case FM:
            return tci::NFM;
            
        case AM:
            return tci::AM;
            
        default:
            // Default to DIGU for FreeDV digital modes
            return tci::DIGU;
    }
}

IRigFrequencyController::Mode TciRigController::tciModeToFreeDv_(tci::Modulation mod)
{
    // Map TCI modes back to FreeDV modes
    switch (mod)
    {
        case tci::DIGU:
            return DIGU;
            
        case tci::USB:
            return USB;
            
        case tci::DIGL:
            return DIGL;
            
        case tci::LSB:
            return LSB;
            
        case tci::NFM:
        case tci::WFM:
            return FM;
            
        case tci::AM:
        case tci::SAM:
        case tci::DSB:
        case tci::AM_SYNC:
            return AM;
            
        default:
            return UNKNOWN;
    }
}

void TciRigController::setOnConnectionStatusChange(ConnectionStatusCallback cb)
{
    onStatusCb_ = std::move(cb);
}

TciRigController::ConnectionStatus TciRigController::connectionStatus() const
{
    return connStatus_.load();
}

void TciRigController::setStatus_(ConnectionStatus s)
{
    auto prev = connStatus_.exchange(s);
    if (prev != s && onStatusCb_)
    {
        // Fires on whichever thread called setStatus_ (usually the WS worker
        // thread). UI consumers must CallAfter / wxPostEvent themselves.
        onStatusCb_(s);
    }
}

void TciRigController::onWsConnected_()
{
    connected_ = true;
    setStatus_(ConnectionStatus::Connected);

    // Canonical TCI command per ExpertSDR3 spec is "audio_samplerate" (one
    // word) with args {trx, rate}. The fork shipped "audio_sample_rate" + a
    // single-arg payload, which ExpertSDR3 tolerates but ThetisTCI rejects.
    sendCommand_("audio_samplerate", {std::to_string(trx_), std::to_string(48000)});

    // Set RX filter bandwidth wide enough for FreeDV/RADE
    // RADE uses ~600-2350 Hz, so 100-2700 Hz gives good margin
    sendCommand_("rx_filter_band", {std::to_string(trx_), "100", "2700"});
#ifdef TCI_DEBUG_LOGGING
    fprintf(stderr, "TCI: Set RX filter bandwidth to 100-2700 Hz for TRX %d\n", trx_);
    fflush(stderr);
#endif

    // Note: audio_start command is sent by TciAudioDevice::start() when the audio
    // device is ready to receive data. Sending it here would cause audio packets
    // to arrive before the audio device is initialized, causing crashes.

    // Replay cached frequency if we have one from before a disconnect.
    uint64_t freq = cachedFreqHz_.load();
    if (freq > 0)
    {
        sendCommand_("vfo", {std::to_string(trx_), std::to_string(channel_), std::to_string(freq)});
    }

    // Replay cached modulation if we have one.
    int modeIdx = cachedModeIndex_.load();
    if (modeIdx >= 0)
    {
        std::string modStr = tci::CommandParser::modulationToString(
            static_cast<tci::Modulation>(modeIdx));
        sendCommand_("modulation", {std::to_string(trx_), modStr});
    }
    else
    {
        // No cached mode yet: set initial mode to DIGU for FreeDV and cache it.
        cachedModeIndex_.store(static_cast<int>(tci::DIGU));
        sendCommand_("modulation", {std::to_string(trx_),
            tci::CommandParser::modulationToString(tci::DIGU)});
    }

    // Clear stale MOX state: server has been reset from our viewpoint.
    our_ptt_active_.store(false);
    other_client_mox_.store(false);
    pending_ptt_request_.store(false);
    pttState_.store(false);

    // Notify listeners.
    onRigConnected(this);
}

void TciRigController::onWsDisconnected_()
{
    bool wasConnected = connected_.exchange(false);

    // TcpConnectionHandler will auto-retry every 5 s via its reconnect timer.
    // Show Reconnecting until either a fresh onWsConnected_ fires or the user
    // explicitly calls disconnect() (which sets Disconnected).
    setStatus_(ConnectionStatus::Reconnecting);

    // Do NOT clear cached freq/mode here: the cache exists precisely so we can
    // replay it once the reconnect succeeds.

    if (wasConnected)
    {
        onRigDisconnected(this);
    }
}

void TciRigController::onError_(const std::string& error)
{
    onRigError(this, error);
}
