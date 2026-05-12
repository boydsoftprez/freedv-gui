# freedv-gui with TCI (Transceiver Control Interface)

A cross-platform fork of [drowe67/freedv-gui](https://github.com/drowe67/freedv-gui)
that adds end-to-end TCI support for rig control + RX/TX audio. TCI is the
WebSocket-based protocol used by ExpertSDR3, SunSDR, ThetisTCI, and other
SDR consoles that prefer network-attached digital-mode clients over USB
audio cables.

This fork descends from [tompatulpan/freedv-tci](https://github.com/tompatulpan/freedv-tci)
(by Tomas Ostojic / SM0ONR), rebased onto current upstream `master` and
modernized for production: cross-platform WebSocket transport, auto-reconnect,
multi-client safety, unit tests, and CI lanes on Linux + macOS + Windows.

**Status: alpha.** Functional on macOS; CI green on Linux. Windows is built but
the WS smoke test is a no-op there (POSIX-only mock server is a deferred
follow-up). Bench validation against real radios is the next step.

## Why this fork

freedv-gui upstream supports rig control via Hamlib + OmniRig + serial and
audio via PortAudio / MacAudio / Pulse / WASAPI. It does not speak TCI.
[Issue #1219](https://github.com/drowe67/freedv-gui/issues/1219) on upstream
was closed in April 2026 pending the Hamlib ARDC network-radio grant work;
we revisit upstream contribution when that lands. For now this lives as a
solo fork.

The fork that started this work (tompatulpan/freedv-tci) shipped a working
proof-of-concept on POSIX with TX audio, RX audio, and basic PTT control.
This rebase + modernization adds:

- **Cross-platform WebSocket transport.** The original POSIX raw-socket
  client is replaced with the existing freedv-gui `TcpConnectionHandler` +
  vendored `websocketpp` pattern, matching the in-tree `SocketIoClient`.
  Linux + macOS + Windows now build the same source.
- **Auto-reconnect with state resync.** WS drop -> 5-second retry loop ->
  on handshake completion, the cached frequency + mode are replayed so the
  user-visible state survives a network blip.
- **Multi-client MOX safety gate.** When multiple TCI clients share a
  server (e.g., WSJT-X + freedv-gui on the same SunSDR), freedv-gui no
  longer leaks TX audio onto another client's PTT. The gate tracks our
  own `trx` echo specifically.
- **Unit tests.** Protocol parser, audio frame codec, sample-format
  conversion, mock RFC 6455 server + WS roundtrip smoke, MOX state machine,
  auto-reconnect cache + status callback, device-name helpers. 44 test
  cases across 6 binaries.
- **CI lanes.** `.github/workflows/tci-tests.yml` runs the 6 TCI test
  binaries on ubuntu-24.04 + macos-latest + windows-latest (MSYS2/MinGW64).

## Credits

| Layer | Author |
| --- | --- |
| freedv-gui upstream | David Rowe (drowe67) + Mooneer Salem + contributors |
| TCI foundation port | Tomas Ostojic / SM0ONR ([tompatulpan/freedv-tci](https://github.com/tompatulpan/freedv-tci)) |
| Rebase + modernization | J.J. Boyd / KG4VCF |

All Tomas-authored commits are preserved verbatim through the rebase with
their original `tompatulpan@tutanota.com` authorship. New work in this fork
is GPG-signed under `kg4vcf@gmail.com`.

## Building

This is a freedv-gui build at heart; upstream's
[build instructions](https://github.com/drowe67/freedv-gui#building-freedv-gui)
apply. Cross-platform deltas for the TCI work:

### Linux (ubuntu-24.04 tested in CI)

```
sudo apt install libspeexdsp-dev sox git libwxgtk3.2-dev libhamlib-dev \
    libasound2-dev libao-dev libgsm1-dev libsndfile1-dev cmake \
    module-assistant build-essential autoconf automake libtool \
    libebur128-dev libpulse-dev libportaudio2 portaudio19-dev
git clone https://github.com/boydsoftprez/freedv-gui.git
cd freedv-gui
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUNITTEST=ON
cmake --build build -j$(nproc)
```

### macOS (tested locally)

```
brew install cmake ninja pkgconf wget portaudio hamlib
git clone https://github.com/boydsoftprez/freedv-gui.git
cd freedv-gui
git clone https://github.com/tmiw/macdylibbundler.git && cd macdylibbundler && make && cd ..
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUNITTEST=ON
cmake --build build -j$(sysctl -n hw.ncpu)
```

### Windows (MSYS2/MinGW64, tested in CI)

The TCI test lane on Windows uses MSYS2:

```
pacman -S mingw-w64-x86_64-{cmake,gcc,ninja,pkgconf,hamlib,portaudio,libsndfile,speexdsp,libebur128}
git clone https://github.com/boydsoftprez/freedv-gui.git
cd freedv-gui
cmake -B build -G Ninja -DUNITTEST=ON -DUSE_NATIVE_AUDIO=OFF
cmake --build build --target TciProtocolTest TciMoxGateTest TciReconnectTest \
    TciWebSocketClientTest TciAudioFrameTest TciDeviceNamingTest
```

Note: native Windows full-app build still requires the full freedv-gui
Windows build path (cross-compile from Linux per upstream, or use the
existing `cmake-windows.yml` MinGW-LLVM cross-build workflow).

### Running the TCI test suite

```
cd build && ctest -R '^(audio_Tci|rig_control_Tci)' --output-on-failure
```

Expected: 6/6 pass.

## TCI quick-start

freedv-gui needs three things from your radio: PTT control, RX audio, and
TX audio. The TCI integration provides all three over a single WebSocket
connection.

1. **On your radio's side**: enable TCI in your SDR console. ExpertSDR3
   ships TCI as a built-in server; ThetisTCI is a Thetis plugin; SunSDR
   speaks TCI natively. Default ports vary: ExpertSDR3 = 40001, ThetisTCI
   commonly 50001. Note your server's listening port.
2. **In freedv-gui Tools then PTT**: scroll to the "TCI Protocol Settings"
   section, check "Enable CAT control via TCI", and fill in the server
   hostname (often `localhost` or `127.0.0.1` for local SDR consoles) and
   port. Apply.
3. **In Tools then Audio Config**: set RX In to the TCI virtual input
   (or follow Tomas's original guidance using `TX Out = none` sentinel for
   TCI audio mode). The exact wiring depends on whether your SDR console
   exposes a virtual audio device or relies on the TCI audio stream
   directly. See `TCI_QUICKSTART.md` and `TCI_INTEGRATION.md` at the repo
   root for Tomas's detailed setup notes.
4. **Verify**: with FreeDV running and connected, the status indicator
   should show Connected. Tune to a known FreeDV transmission and verify
   the decoder syncs.

If the server drops or your network blinks, freedv-gui auto-reconnects
within 5 seconds and replays your last-known frequency + mode so the
radio's state is preserved.

## What works, what to watch

| Feature | State |
| --- | --- |
| WS connect + handshake | Working |
| Frequency + mode get/set over TCI | Working |
| PTT over TCI (`trx:0,true|false`) | Working |
| RX audio decode | Working (per Tomas's bench tests) |
| TX audio encode + send | Working (per Tomas's bench tests) |
| Auto-reconnect (BLOCKER fix landed in `c9f502a6`) | Working in code; bench-pending |
| Multi-client MOX safety gate | Working (5 unit tests); bench-pending |
| Mode mapping (DIGU / DIGL / USB / LSB / etc.) | Working |
| Cross-platform build | macOS Yes, Linux Yes (CI), Windows MSYS2 Yes (CI; WS test no-op) |

### Known limitations

- **Default TCI port is 50001 in the PTT dialog** (Tomas's bench choice).
  Canonical ExpertSDR3 port is 40001. Change the port in the dialog if
  your server listens elsewhere. Tracked for a follow-up patch.
- **TX gate sends silence rather than dropping frames** when `we_pressed_tx`
  is false but `server_mox` is true. Functional but a spec deviation; see
  `docs/superpowers/specs/2026-05-11-tci-foundation-port-design.md`.
- **Multi-client false-positive window**: a sub-100 ms race exists where a
  foot-switch MOX echoed before our own `trx` echo could be credited as
  ours. Acceptable for v1; documented in the spec.
- **Clock drift between TCI server and local sound card** is unhandled.
  Long QSOs (>40 min) will start dropping samples at the ring-buffer cap.
  Documented in the spec; PI-controller fix is a v2 follow-up.
- **Windows mock WS server**: the unit test mock at
  `src/rig_control/test/mock_tci_server.cpp` is POSIX-only. The Windows
  test binary builds but exits 0 immediately. Real-server testing on
  Windows still works; only the localhost mock is missing.

## Design + plan documents

The full design rationale and implementation plan land in this repo at:

- `docs/superpowers/specs/2026-05-11-tci-foundation-port-design.md`: design
  spec capturing scope, architecture, threading model, MOX state machine,
  WS transport strategy.
- `docs/superpowers/plans/2026-05-11-tci-foundation-port.md`: task-level
  implementation plan with 20 tasks, TDD where applicable.
- `test/tci_bench_matrix.md`: bench validation matrix with the 5 test
  scenarios and pass/fail criteria; rows to be filled in as we test
  against ExpertSDR3, ThetisTCI, and SunSDR.
- `TCI_INTEGRATION.md`, `TCI_QUICKSTART.md`, `TCI_TX_TESTING_GUIDE.md`:
  Tomas's original documentation from the foundation fork.

## License

LGPL 2.1 or later, matching upstream freedv-gui. See `COPYING` for the full
text.

All new files added in this fork carry the standard freedv-gui house-style
license header. Tomas-authored files retain his original Authors line.
Files we modified add a co-author Authors line crediting both Tomas (original)
and J.J. Boyd (modifications).

## Reporting issues

For TCI-specific bugs:
[boydsoftprez/freedv-gui/issues](https://github.com/boydsoftprez/freedv-gui/issues).

For underlying freedv-gui bugs unrelated to TCI:
[drowe67/freedv-gui/issues](https://github.com/drowe67/freedv-gui/issues).

For Tomas's original TCI implementation questions (which we inherit):
[tompatulpan/freedv-tci/issues](https://github.com/tompatulpan/freedv-tci/issues).

J.J. Boyd ~ KG4VCF
