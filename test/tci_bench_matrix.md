# TCI Bench Validation Matrix

Tracks manual validation of TCI integration against real radios + servers.
Update rows as bench access permits. Each row records the TCI server tested,
client platform, and pass/fail/notes for each test scenario.

## Test scenarios

1. **Connect**: open WS to TCI server, complete handshake, see `ready;` echo.
   Status indicator transitions Disconnected → Connecting → Connected within
   5 seconds.

2. **RX audio**: tune to a known FreeDV 700D transmission, verify decode +
   audible intelligible voice through freedv-gui's audio output. Pass requires
   sustained decode (no resync loops) over at least 60 seconds.

3. **TX audio**: press freedv-gui's TX button, speak into microphone, verify
   off-air receiver (separate radio, independent path) hears a recognizable
   FreeDV-modulated signal and decodes the speech.

4. **Reconnect**: while connected and idle (not in TX), physically disconnect
   the network (pull cable or disable Wi-Fi), wait 10 seconds, restore network.
   Verify status indicator transitions Connected → Reconnecting → Connected,
   and that the radio's cached frequency + mode are restored.

5. **Multi-client safety**: with a second TCI client (e.g., WSJT-X or another
   instance of freedv-gui) also connected to the same TCI server, trigger
   MOX from the other client. Verify freedv-gui does NOT emit any TX audio
   during the other client's MOX window. Confirmed via WS packet capture
   showing zero `tx_audio_stream` binary frames from freedv-gui in that window.

## Matrix

| Server | Server version | Client OS | Client build (commit) | Connect | RX | TX | Reconnect | Multi-client safe | Tester | Date | Notes |
|--------|---------------|-----------|----------------------|---------|----|----|-----------|-------------------|--------|------|-------|
| ExpertSDR3 | TBD | macOS 15.x | feat/tci-port-2.3.1 | TBD | TBD | TBD | TBD | TBD | KG4VCF | TBD | |
| ExpertSDR3 | TBD | Windows 11 | feat/tci-port-2.3.1 | TBD | TBD | TBD | TBD | TBD | KG4VCF | TBD | |
| ThetisTCI on ANAN-G2 | TBD | Linux | feat/tci-port-2.3.1 | TBD | TBD | TBD | TBD | TBD | KG4VCF | TBD | |

Add rows for additional servers/platforms as access permits.

## Pass/fail criteria

- **Connect**: status reaches "Connected" within 5 seconds of pressing Apply
  in the Tools-then-PTT dialog. No error dialogs. Initial state burst from
  server is received (visible in debug log: `protocol`, `device`, `trx_count`,
  `vfo_limits`, `if_limits`, `modulations_list`, `ready` commands logged).

- **RX audio**: SINAD greater than 6 dB on a 0 dB SNR FreeDV transmission,
  OR qualitative "intelligible voice" judgment after at least 60 seconds of
  decode. The "Frm Mic" meter should remain idle (no input) and the FreeDV
  decoder must show steady sync, not constant resync events.

- **TX audio**: off-air monitor receiver (separate hardware) successfully
  decodes the FreeDV signal that freedv-gui transmits. Confirmed by an
  independent freedv-gui or compatible decoder.

- **Reconnect**: connection status returns to "Connected" within 10 seconds
  of network restore. Frequency + mode on the radio are unchanged from
  pre-disconnect values. RX audio resumes within 2 seconds of the
  re-Connected state.

- **Multi-client safe**: with two TCI clients on the same server and only
  the OTHER one triggering MOX, packet capture of freedv-gui's WS connection
  shows zero binary `tx_audio_stream` frames during the entire other-client
  MOX window. freedv-gui's UI may show "MOX" indicator (acknowledging server
  state) but the gate prevents TX audio emission.

## Recording results

When updating a row:
- Use "Pass", "Fail", "Skip" (e.g., no access to this server type), or
  "Partial" (with note explaining what passed and what didn't).
- Date format: YYYY-MM-DD.
- Notes: keep terse; one line per anomaly. Link to issue if filed.

## Out of scope for v1

- IQ stream support (`iq_start` / `iq_stop`) — no panadapter in freedv-gui.
- Multi-TRX / multi-channel — single TRX 0 only.
- TCI auto-discovery — no spec; manual URL entry.
- Clock-drift handling — accepted v1 limitation (~1 sample / 24 sec at
  typical 10 ppm crystal mismatch; manifests as decoder resync after
  ~40 minutes of QSO).
- Sub-100 ms multi-client MOX false-positive window — accepted v1 limitation
  (closes after we know whether the TCI server echoes a per-request token
  in `trx` responses).
