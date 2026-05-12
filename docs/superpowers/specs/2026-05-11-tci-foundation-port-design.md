# TCI support for FreeDV: foundation port + 2.3.1 modernization

Status: Design draft, brainstorm complete, pending user review before plan-writing.
Date: 2026-05-11.
Author: J.J. Boyd ~ KG4VCF.
Foundation: tompatulpan/freedv-tci `tci-tx-clean-implementation` by Tomas Ostojic / SM0ONR.

## 1. Overview

freedv-gui currently supports rig control via Hamlib, OmniRig, DXLabSuiteCommander, HRD, and direct serial PTT/CAT; audio comes from local sound-card devices via PortAudio, MacAudio, Pulse, or WASAPI. It does not speak TCI (the WebSocket-based Transceiver Control Interface used by ExpertSDR3, SunSDR, ThetisTCI, and ANAN running TCI plugins).

This design ports `tompatulpan/freedv-tci` (a working freedv-gui+TCI fork) onto current upstream master and modernizes it for production. The end state is a single-PR-sized branch in our solo fork that:

1. Builds and runs on macOS, Linux, AND Windows. The fork is POSIX-only today.
2. Auto-reconnects on WS drop with full state resync.
3. Integrates TCI configuration into Tools-then-Options Audio + Rig Control dialogs. The fork wires Easy Setup only.
4. Closes the multi-client MOX-leak gap surfaced during adversarial review.
5. Ships unit tests for the TCI protocol parser and audio frame codec. The fork ships none.

freedv-gui upstream issue #1219 (TCI feature request) was closed 2026-04-01 by @tmiw pending Hamlib's ARDC network-radio grant work. We do not target an upstream PR in v1; we maintain in our solo fork and revisit upstream when @tmiw reopens the conversation.

## 2. Provenance and license

**Foundation**: `tompatulpan/freedv-tci` branch `tci-tx-clean-implementation`. 5,216 insertions / 258 deletions across 32 files. 17 TCI-specific commits between fork base (`f4a35367`, 2026-02-02, Hamlib PR #1209) and HEAD (`1d7af6c`, README update). Upstream master is 138 commits ahead of that base at `77e793a` (2026-05-10).

**Primary upstream author**: Tomas Ostojic / SM0ONR. Authorship preserved on all rebased commits via `git rebase` (not squash-merge). New work commits are authored by JJ Boyd / KG4VCF.

**License**: LGPL 2.1 or later (matches freedv-gui upstream).

The fork's eight TCI-authored files carry an incorrect "GNU General Public License version 2.1" string in the boilerplate. The only license with version 2.1 is the **Lesser** GPL, and the rest of the boilerplate matches LGPL grants. We treat this as a transcription error: during port we replace verbatim with the standard freedv-gui LGPL header text (matching `IRigController.h` lines 7-21), preserving the `Authors: Tomas Ostojic` line and adding a `Modifications:` line for our work.

**Reference implementations consulted (no code copied from non-LGPL sources)**:
- `wsjtx-3.1.0_improved_PLUS_260418` `Transceiver/TCITransceiver.{hpp,cpp}` (Qt5/QWebSocket, GPLv3). Studied for protocol surface and command set.
- Thetis `TCIServer.cs` at `Project Files/Source/Console/`. Studied for frame layout (lines 5201-5305), command echo semantics, TX backlog cap behavior (line 5689).
- ExpertSDR3 TCI specification at `https://github.com/ExpertSDR3/TCI`.

## 3. Scope

### In scope (this PR, single long-lived branch)

1. Rebase 17 fork commits onto upstream master at the point of merge (currently `77e793a`).
2. Replace `TciWebSocketClient` raw-POSIX socket layer with vendored websocketpp + `TcpConnectionHandler` pattern. Same public interface so `TciRigController` and `TciAudioDevice` do not change.
3. Auto-reconnect with state resync (on reconnect: re-send `audio_samplerate:0,48000;` then `audio_start:0;`, wait for `ready;` before declaring usable, then re-apply cached freq + mode).
4. TCI as a first-class option in `dlg_audiooptions.cpp` (audio device dropdowns) and `dlg_ptt.cpp` (rig control dropdown). Not Easy Setup-only.
5. MOX multi-client safety gate: track our own `trx:0,true` echo specifically. If `server_mox` arrives before our echo, treat as "another client" and refuse to send TX audio.
6. Bug fixes for the two issues documented in `TCI_TX_TESTING_GUIDE.md` (sample-rate dropdown N/A edge case; audio test buttons disabled in TCI mode).
7. License header correction across all 8 TCI-authored files.
8. Unit tests for `TciProtocol`, command parsing, sample-format conversion, ring-buffer behavior.
9. Localhost mock TCI server adequate for CI smoke tests.
10. Windows CI lane that exercises the TCI-enabled build end to end through the mock server.

### Out of scope (deferred)

1. Multi-TRX, multi-channel TCI. Single TRX 0 only. Fork limitation, fine for v1.
2. IQ stream support (`iq_start` / `iq_stop`). freedv-gui has no panadapter to feed.
3. TCI auto-discovery / mDNS. No spec.
4. Clock-drift PLL between TCI server and local sound card. Documented in §15.
5. Hamlib ARDC network-radio integration. Revisit when their work lands.
6. Upstream PR to `drowe67/freedv-gui`. See §1.

## 4. Architecture and adoption strategy

### Files inherited from the fork (kept; minor edits)

| File | Approx lines | Treatment |
|------|--------------|-----------|
| `src/rig_control/TciProtocol.{h,cpp}` | 300 | Adopt verbatim after license-header fix. Light edits to act on `vfo`, `if`, `tune`, `rx_mute` commands (currently parsed but no-op'd). |
| `src/rig_control/TciRigController.{h,cpp}` | 616 | Adopt with multi-client MOX gate added (§11). |
| `src/audio/TciAudioDevice.{h,cpp}` | 731 | Adopt after auditing all cross-thread calls for `wxQueueEvent` (zero hits expected, no replacement needed; if present, swap to `CallAfter`). |

### Files replaced

| File | Why |
|------|-----|
| `src/rig_control/TciWebSocketClient.{h,cpp}` (635 lines raw POSIX sockets, hand-rolled WS handshake) | Replace with websocketpp + `TcpConnectionHandler` impl (~500 lines). Same public interface so consumers do not change. Gets Windows support. Inherits `TcpConnectionHandler`'s `RECONNECT_INTERVAL_MS=5000ms` retry loop. |

### Files added

| File | Purpose |
|------|---------|
| `src/rig_control/TciWebSocketClient.cpp` (new impl) | Production WS client on top of vendored websocketpp. |
| `test/unittest/test_tci_protocol.cpp` | Unit tests for command parse and serialize. |
| `test/unittest/test_tci_audio_frame.cpp` | Unit tests for `convertInt16ToShort_` / `convertInt24ToShort_` / `convertInt32ToShort_` / `convertFloat32ToShort_`. |
| `test/unittest/test_tci_ws_mock.cpp` | Localhost mock server smoke tests. |
| `docs/superpowers/specs/2026-05-11-tci-foundation-port-design.md` | This file. |
| `docs/superpowers/plans/2026-05-11-tci-port-plan.md` (TBD) | Implementation plan, written next via the `writing-plans` skill. |

### Files modified (integration touchpoints)

| File | Modification |
|------|--------------|
| `src/gui/dialogs/dlg_audiooptions.{h,cpp}` | Add TCI device options to audio dropdowns; fix the two TCI_TX_TESTING_GUIDE.md issues. |
| `src/gui/dialogs/dlg_ptt.{h,cpp}` | Add TCI rig option. |
| `src/gui/dialogs/dlg_easy_setup.{h,cpp}` | Keep fork's TCI wiring; rebase-clean against upstream changes. |
| `src/main.cpp` | Wire TCI device creation from Audio Config path. Reconcile against fork's existing `g_nSoundCards=2` pattern detection. |
| `src/pipeline/TxRxThread.cpp` | Preserve fork's mic-buffer-trim-on-PTT-entry fix (lines 723-745 in fork; line numbers will shift on rebase). |
| `CMakeLists.txt`, `src/rig_control/CMakeLists.txt`, `src/audio/CMakeLists.txt` | Build wiring. |
| `.github/workflows/*.yml` | Confirm Windows lane builds + runs TCI tests. |

## 5. Data flow

### Initialization

```
freedv-gui opens WS to ws://<host>:40001
  -> TCI server sends initial-state burst:
     protocol;device;trx_count;channel_count;vfo_limits;if_limits;
     modulations_list;preamp;dds:0,FREQ;modulation:0,MODE;...;ready;
  <- freedv-gui sends audio_samplerate:0,48000;
  -> server echoes audio_samplerate:0,48000;
  <- freedv-gui sends audio_start:0;
  -> server echoes audio_start:0;
  -> server begins emitting RxAudioStream binary frames
  -> freedv-gui sends modulation:0,digu; (or LSB/USB per user pref)
  -> server echoes modulation:0,digu;
```

### RX (server to freedv-gui)

```
TcpConnectionHandler receive thread:
  socket recv -> websocketpp framer -> binary frame complete
  -> TciWebSocketClient streamCallback_
  -> TciAudioDevice::handleStream_
     parse 64-byte StreamHeader; if type == RxAudioStream && receiver == trx_:
       convert<Format>ToShort_ (one of 4 paths based on header.format)
       average stereo pairs (L+R)/2 -> mono short
       rxMutex_ lock; append to rxBuffer_; cv notify
  -> rxThread_ wakes; signals IAudioDevice::onAudioDataFunction with chunk
  -> freedv-gui's existing pipeline ResampleStep (r8brain) handles 48k -> 8k/16k
  -> FreeDV decoder -> speaker
```

### TX (freedv-gui to server) with TX_CHRONO pull model

```
freedv-gui mic input -> existing pipeline -> FreeDV encoder (8k/16k) -> outfifo1
  When server emits TX_CHRONO stream frame (server-paced pull request):
    TciAudioDevice handleStream_ sees type == TxChrono
    -> calls onTxAudioDataFunction (separate callback path; see TciAudioDevice.h:62-64)
    -> consumer pulls samples from outfifo1
    -> resample 8k/16k -> 48k via existing pipeline (r8brain)
    -> short -> float32 conversion
    -> mono -> stereo (duplicate L+R) via TciAudioDevice::enqueueTxAudio
    -> txThread_ wakes; packs StreamHeader + payload; sendBinaryData
  -> websocketpp framer; TcpConnectionHandler send queue -> socket send
```

TX_CHRONO is the key TCI semantic the initial brainstorm missed: the server pulls TX audio on its own schedule, not on freedv-gui's. The fork already implements this correctly.

### Control / MOX

```
User clicks freedv-gui TX button (main thread):
  TciRigController::setPtt(true)
  -> stores pending_ptt_request_ = true (atomic)
  -> sends "trx:0,true;" via TciWebSocketClient::sendCommand
TCI server processes (possibly arbitrates among multiple clients):
  -> sends "trx:0,true;" echo back
TciWebSocketClient receive thread:
  -> TciRigController::handleTciCommand sees "trx:0,true;"
  -> if pending_ptt_request_ was true: mark our_ptt_active_ = true (atomic);
     clear pending; this IS our PTT
  -> if pending_ptt_request_ was false: server_mox = true but NOT our PTT;
     mark other_client_mox = true (atomic)
  -> CallAfter(MainFrame, [...]() { update UI state })
Audio callback path (txThread_):
  may_send_tx_audio = our_ptt_active_ && !other_client_mox
  -> if false: drop FreeDV samples on the floor (do not enqueue)
```

The fork's existing `we_pressed_tx && server_mox` gate gets replaced with `our_ptt_active_ && !other_client_mox` to close the multi-client leak. See §11 for the state machine in full.

## 6. Threading model

The fork uses four threads inside TCI:

1. `TciWebSocketClient::ioThread_`: WS send drain
2. `TciWebSocketClient::receiveThread_`: WS recv pump
3. `TciAudioDevice::rxThread_`: dispatch RX samples to consumer callback
4. `TciAudioDevice::txThread_`: dispatch TX_CHRONO frames

We keep this model. Reasons:

1. The websocketpp + `TcpConnectionHandler` replacement already imposes the send/recv thread split (the existing `SocketIoClient` pattern at `src/util/SocketIoClient.cpp` follows it).
2. RX/TX audio threads match `IAudioDevice` semantics (per-direction `start`/`stop`/`getSampleRate`).
3. The fork shipped working TX audio with this model and documented its bugs (see `TCI_TX_TESTING_GUIDE.md`); consolidating without evidence of benefit risks regression.

Cross-thread communication for UI state uses `CallAfter` (the freedv-gui house pattern; 26+ hits in `main.cpp`). Zero hits of `wxQueueEvent` in the existing tree; rebase audit must verify the fork does not introduce any.

Mutual exclusion:

- `TciAudioDevice::rxMutex_` + `rxCv_`: RX sample handoff.
- `TciAudioDevice::txMutex_` + `txCv_`: TX sample queue.
- `TciWebSocketClient::sendMutex_`: send queue.
- All MOX/PTT state flags are `std::atomic<bool>` to avoid locks in the audio data path.

## 7. Rebase plan

Current fork branch state:

- Fork branch: `tci-tx-clean-implementation` at `1d7af6c`.
- Fork base in upstream history: `f4a35367` (2026-02-02, PR #1209).
- Upstream master HEAD: `77e793a` (2026-05-10).
- 17 fork commits to replay onto 138 commits of upstream drift.

Procedure:

1. Branch the work: `git checkout -b feat/tci-port-2.3.1 tci-tx-clean-implementation`.
2. Interactive rebase: `git rebase --onto upstream/master f4a35367 feat/tci-port-2.3.1`.
3. Resolve conflicts as they arise. Expected hot spots (heavily-modified upstream files):
   - `src/main.cpp` (fork adds 283 lines; upstream has been refactoring around audio engine factory).
   - `src/gui/dialogs/dlg_audiooptions.cpp` (fork adds 87 lines; upstream has been changing device-list logic for the recent Windows audio work, PRs #1317 and #1326).
   - `src/gui/dialogs/dlg_easy_setup.{h,cpp}` (fork adds ~95 lines; upstream redid Easy Setup heavily in #1303 and #1306).
   - `src/gui/dialogs/dlg_ptt.{h,cpp}` (fork adds 143 lines; rig-control PRs #1320 and #1321 changed VFO ping-pong logic).
   - `src/pipeline/TxRxThread.cpp` (fork adds 140 lines including the mic-buffer-trim fix).
4. After each conflict: build (Linux first, fastest CMake cycle), then run unit tests.
5. After all 17 commits replay: `git rebase --continue` then full ctest run.
6. Save the rebased branch as the new `feat/tci-port-2.3.1` reference; do not force-push over `tci-tx-clean-implementation` (preserves Tomas's branch history at origin).

If conflict resolution exceeds reasonable effort on any one commit, fall back to squashing the fork's 17 commits to a single "Adopt tompatulpan/freedv-tci as foundation" commit with full attribution in the message body. Loses some history granularity but is recoverable.

## 8. WebSocket transport replacement

The fork's `TciWebSocketClient` uses raw POSIX sockets (`<sys/socket.h>`, `<netinet/in.h>`, `<netdb.h>`) with a hand-coded HTTP upgrade handshake (`TciWebSocketClient.cpp` lines 268-317 in the fork). It does not link Windows because WinSock has incompatible types and headers. The fork's own README lists this as a future-work item.

Replacement strategy:

1. Subclass `TcpConnectionHandler` (`src/util/TcpConnectionHandler.{h,cpp}`) the same way `SocketIoClient` does. `TcpConnectionHandler` already handles cross-platform socket I/O (POSIX + WinSock), DNS resolution (IPv4 + IPv6), reconnect with `RECONNECT_INTERVAL_MS=5000ms`, and the send/recv thread split.
2. Use vendored websocketpp (`src/3rdparty/websocketpp/`) for WS framing on top. The existing `websocketpp_config.h` uses `transport::iostream::endpoint`, which is exactly the right shape: websocketpp does framing only and we feed it bytes via the `write_handler` callback. Same pattern as `SocketIoClient.cpp` lines 159-162.
3. Public interface preserved: `connect(host, port)`, `disconnect()`, `isConnected()`, `sendCommand(text)`, `sendBinaryData(bytes, size)`, plus the four callback setters. `TciRigController` and `TciAudioDevice` need no changes.
4. Per-message-deflate stays disabled (it was off in `websocketpp_config.h:269` already; compression on random float32 audio is negative-gain).
5. `max_message_size` stays at the existing 32 MB default; TCI frames are at most ~16 KB.

Risk: the existing `TcpConnectionHandler` recv polling interval (`RX_ATTEMPT_INTERVAL_MS=50ms`, hardcoded at `TcpConnectionHandler.cpp:54`) gates RX latency. At 50 ms wake-up cadence the actual added RX latency floor is the average half-interval (25 ms) plus any frame-size aggregation. For 320 ms RX ring buffers this is fine. If a future use case needs lower latency, refactor `TcpConnectionHandler` to expose the interval as a per-subclass setting; out of scope for v1.

## 9. Auto-reconnect with state resync

`TcpConnectionHandler` already retries connection every 5 seconds via `RECONNECT_INTERVAL_MS`. The fork does not surface this; on WS drop the fork goes inert until the user reconnects manually.

State resync sequence on reconnect:

1. WS handshake completes; `onConnected` fires.
2. Wait for server initial-state burst (terminated by `ready;`).
3. Re-send `audio_samplerate:0,48000;` and `audio_start:0;`.
4. Wait for echoes.
5. Re-apply cached freq (`dds:0,<cached_freq>;`) and mode (`modulation:0,<cached_mode>;`).
6. Mark connection healthy; UI shows "TCI connected".

Behavior during reconnect window:

- TX audio: drop silently (cannot transmit while disconnected).
- RX audio: silence to consumer (buffered consumer sees underrun, freedv-gui pipeline already handles underruns gracefully).
- PTT requests during reconnect: queue or refuse? Refuse (set fail flag; PTT button reverts to off in UI via `CallAfter`). Queueing PTT through reconnect is a footgun (user might keep talking, then PTT activates 5 seconds later mid-sentence).
- Frequency / mode requests during reconnect: queue last value; apply after resync.

UI signaling: connection status in `dlg_audiooptions` and `dlg_ptt` status indicators ("Connected", "Reconnecting in Ns...", "Disconnected"). Implemented via `CallAfter` posts from `TciWebSocketClient`'s connection callbacks.

## 10. Audio Config dialog integration

The fork wires TCI through Easy Setup only (`src/gui/dialogs/dlg_easy_setup.cpp`). v1 brings it into the regular Audio Config (`src/gui/dialogs/dlg_audiooptions.cpp`) and PTT (`src/gui/dialogs/dlg_ptt.cpp`) dialogs so power users can configure TCI directly.

Audio Config changes:

1. Add "TCI: <server>:<port> RX" and "TCI: <server>:<port> TX" entries to the audio device dropdowns.
2. Hidden when TCI is not configured; visible when at least one TCI rig profile exists.
3. Device-name format: `"TCI: <host>:<port> <RX|TX>"`. `IAudioEngine::getAudioDevice(name, ...)` matches by exact string.
4. The existing `AudioEngineFactory` singleton does NOT change. `TciAudioDevice` instances are minted by name-prefix recognition: `main.cpp`'s device-creation logic gets a small helper `IsTciDeviceName(wxString)` and routes accordingly. Matches the reviewer's recommendation for issue #4.

PTT dialog changes:

1. Add "TCI" to the rig type dropdown alongside Hamlib, OmniRig, etc.
2. When "TCI" is selected: show server URL field, port field, TRX index (default 0), "Test connection" button, status indicator.
3. Persist TCI rig profile in `RigControlConfiguration` (already extended by the fork at `src/config/RigControlConfiguration.{h,cpp}`).

Bug fixes adopted in this section:

- Sample-rate dropdown N/A edge case when TX Out is set to "none" (fork's documented Issue #1). Fix: populate sample rates per-device individually rather than in pairs.
- Audio test buttons (Record / Play 2 Seconds) sometimes disabled in TCI mode (fork's documented Issue #2). Investigation needed; suspect the enable-disable logic does not handle the TCI mixed-pattern correctly.

## 11. MOX multi-client safety gate

Problem statement: TCI servers serve multiple clients. The fork's gate is `we_pressed_tx && server_mox`. Both can be true simultaneously when a foot-switch fires AFTER we set our local flag; freedv-gui would then transmit on top of someone else's PTT.

Fix: track our own `trx:0,true` echo specifically.

State (all `std::atomic<bool>` in `TciRigController`):
- `pending_ptt_request_`: we sent `trx:0,true;` and have not seen our echo yet.
- `our_ptt_active_`: we sent it and the echo confirmed us as the originator.
- `other_client_mox_`: server signals MOX but our pending flag was false at echo time, so someone else triggered.

Transitions:
- `setPtt(true)`: set `pending_ptt_request_ = true`; send `trx:0,true;`.
- `setPtt(false)`: clear `pending_ptt_request_` and `our_ptt_active_`; send `trx:0,false;`.
- On `trx:0,true;` echo received: if `pending_ptt_request_` was true at receive time, set `our_ptt_active_ = true` and clear pending; else set `other_client_mox_ = true`.
- On `trx:0,false;` echo received: clear both `our_ptt_active_` and `other_client_mox_`.

TX audio gate: `may_send = our_ptt_active_ && !other_client_mox_`.

Edge case: hardware PTT (foot switch) fires WHILE `pending_ptt_request_` is set but before our echo arrives. The server may emit the foot-switch MOX echo before our own. The race: we receive `trx:0,true;` and credit it to ourselves. Mitigation: include a per-request token in our SET (`tci`-server-specific param mentioned in the fork: `Calling sendCommand_("trx", [0, true, tci])`: see `TCI_TX_TESTING_GUIDE.md:752`). If the server echoes the token in some commands, use it as the disambiguator. If not, accept a small false-positive window (sub-100 ms) as acceptable v1 behavior; document.

## 12. Fork-documented bugs to fix

From `TCI_TX_TESTING_GUIDE.md`:

1. **Sample-rate dropdown N/A** when TX Out = "none". Fix: populate per-device, not paired.
2. **Audio test buttons disabled** in TCI mode. Fix: investigate enable/disable logic for TCI mixed patterns.

Plus the license-header typo across all 8 TCI-authored files (one-line edit per file).

## 13. Test plan

### Unit tests (must run on macOS, Linux, Windows in CI)

1. `test_tci_protocol.cpp`: parse representative TCI text frames (single command, multi-arg, with quotes, with semicolons in payload, malformed); round-trip serialize.
2. `test_tci_audio_frame.cpp`: encode + decode `StreamHeader` for each `(format, sampleRate, length)` permutation; verify `convertInt16ToShort_` / `convertInt24ToShort_` / `convertInt32ToShort_` / `convertFloat32ToShort_` against known-answer fixtures.
3. `test_tci_ws_mock.cpp`: spin up a localhost mock TCI server (asio acceptor, minimal handshake + echo); have `TciWebSocketClient` connect, exchange handshake, send/receive a command + a binary audio frame; assert callback fires with correct payload.

### Bench validation (manual; matrix lives at `test/tci_bench_matrix.md`)

| Server | Platform | RX | TX | Reconnect | Notes |
|--------|----------|----|----|-----------|-------|
| ExpertSDR3 (SunSDR MB1 or similar) | macOS | TBD | TBD | TBD | Primary target |
| ExpertSDR3 | Windows 11 | TBD | TBD | TBD | Windows smoke |
| ThetisTCI on ANAN-G2 | Linux | TBD | TBD | TBD | If ThetisTCI is installed and stable |

User to fill in rows as bench access permits.

### Regression tests (existing freedv-gui suite)

All existing ctest targets must pass. Hamlib + serial PTT paths must be unchanged in behavior; verified by smoke-testing Hamlib against a real rig before merge.

## 14. Single-PR plan

This is a long-lived branch in our solo fork, NOT a PR to upstream drowe67. We do not send emails to Tomas in v1; if his fork remains the source-of-truth for new TCI work upstream we revisit collaboration after our v1 ships.

Branch: `feat/tci-port-2.3.1` in our fork at `https://github.com/boydsoftprez/freedv-tci` (TBD; we will create this fork on push). Origin remote becomes our fork; tompatulpan and drowe67 are tracked remotes for upstream visibility.

Milestones (commits or commit groups within the same branch, not separate PRs):

1. **M1 - Rebase**: 17 fork commits replayed onto upstream master `77e793a`. Builds clean on macOS + Linux. (No Windows yet because POSIX-only.)
2. **M2 - WebSocket transport replacement**: new `TciWebSocketClient` on websocketpp + `TcpConnectionHandler`. Builds clean on macOS + Linux + Windows. Unit tests pass.
3. **M3 - Auto-reconnect**: state resync sequence implemented, reconnect-during-active-TX handled.
4. **M4 - Audio Config + PTT dialog integration**: TCI selectable from main dialogs, not Easy Setup-only.
5. **M5 - MOX multi-client gate**: §11 state machine.
6. **M6 - Documented bug fixes + license header corrections**.
7. **M7 - Tests + mock server + Windows CI lane**.
8. **M8 - Bench validation against real hardware**.

Commits within each milestone are individually meaningful, GPG-signed, with clear messages. No squash at milestone boundaries.

## 15. Risks and open questions

**Risk: rebase complexity**. 17 commits onto 138 commits of drift, with hot-spot files (main.cpp, dlg_audiooptions.cpp, dlg_easy_setup.cpp) all heavily modified upstream. Mitigation: do the rebase first, before any other work; budget a full day for conflict resolution; if it goes south, fall back to squash-then-replay.

**Risk: Windows audio path differences**. The fork was developed on macOS/Linux; freedv-gui has separate `MacAudioEngine`, `WASAPIAudioEngine`, `PulseAudioEngine`, `PortAudioEngine` impls. `TciAudioDevice` is engine-independent (extends `IAudioDevice`) but the device-name routing in `main.cpp` may have engine-specific assumptions that break on Windows. Mitigation: run all four engines' integration paths in CI to the extent possible.

**Risk: `IRigPttController::getRigResponseTimeMicroseconds()`**. Pure-virtual in the interface (`IRigPttController.h:38`). Fork's `TciRigController` must implement. Approach: measure send-to-echo round trip for `trx` commands; cache the most recent value; surface in UI. If TCI server is unresponsive, return a pessimistic constant (50 ms) until we have a sample.

**Risk: clock drift accumulator**. After ~40 minutes of QSO the 320 ms RX ring will start dropping samples at the 10 ppm-typical crystal mismatch rate (~1 sample/24sec). For digital voice this manifests as decoder resync events. Acceptable for v1; flag in user-visible docs. Fix later via slow integral controller on the resample ratio (Mumble pattern).

**Risk: TCI server format negotiation**. We send `audio_samplerate:0,48000;` but the server can refuse and stream at its own rate. Fork handles this via `header.sampleRate` per binary frame. Need to verify the fork's r8brain (existing `ResampleStep`) handles changing rates gracefully if a server switches mid-stream (rare but possible). If not, treat sample-rate change as a "destroy and recreate" event in `TciAudioDevice`.

**Open question: per-request token disambiguation for MOX gate** (§11 edge case). Need to confirm against ExpertSDR3 spec whether `trx` echoes include the originating client's token. If not, accept the sub-100ms false-positive window.

**Open question: should we re-export Tomas's branch under our fork name** or keep `tompatulpan/freedv-tci` as the visible upstream-of-record? Defer to first push; both are reasonable.

## 16. Success criteria

1. Builds clean on macOS, Linux, AND Windows. Windows is the new requirement Tomas did not have.
2. End-to-end RX path: TCI server delivers RX audio; FreeDV decodes; speaker outputs intelligible voice from a real off-air FreeDV transmission.
3. End-to-end TX path: mic input; FreeDV encodes; TCI server transmits; off-air receiver hears recognizable FreeDV signal.
4. Auto-reconnect: physically yank ethernet cable on test bench, reconnect within 5 seconds, audio resumes, freq + mode preserved.
5. Multi-client safety: with two TCI clients connected to the same server, one client triggers PTT; freedv-gui does NOT transmit. Verified by sniffing the TCI socket for `tx_audio_stream` frames in this case.
6. No regression in non-TCI paths: existing Hamlib, OmniRig, serial PTT, sound-card audio all behave identically to v2.3.1 main.
7. Unit tests green on all three platforms in CI.

## 17. References

- Brainstorm session: this repo, `docs/superpowers/specs/2026-05-11-tci-foundation-port-design.md` (this file).
- Adversarial review findings: synthesized inline through §11 and §12 from agent review (not separately filed).
- Foundation fork: `https://github.com/tompatulpan/freedv-tci`, branch `tci-tx-clean-implementation`, HEAD `1d7af6c`.
- Upstream: `https://github.com/drowe67/freedv-gui`, master `77e793a` (2026-05-10).
- Upstream issue #1219 (closed, pending Hamlib ARDC): `https://github.com/drowe67/freedv-gui/issues/1219`.
- Hamlib ARDC grant: `https://www.ardc.net/apply/grants/2025-grants/hamlib-stability-enhancements-and-sdr-transceiver-support/`.
- TCI spec: `https://github.com/ExpertSDR3/TCI`.
- Reference: wsjtx-improved TCI implementation at `wsjtx-3.1.0_improved_PLUS_260418` (studied, no code copied; GPLv3).
- Reference: Thetis `TCIServer.cs` server-side implementation (studied for frame layout; LGPL-compatible source).
- Vendored libraries: websocketpp (`src/3rdparty/websocketpp/`), r8brain (`src/3rdparty/r8brain/`).
