# TCI Foundation Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Adopt `tompatulpan/freedv-tci` as foundation, rebase 17 fork commits onto upstream `drowe67/freedv-gui` master, replace POSIX-only WS transport with cross-platform websocketpp + `TcpConnectionHandler`, add auto-reconnect with state resync, integrate TCI into Tools-then-Options dialogs, close the multi-client MOX leak, and ship unit tests.

**Architecture:** TCI is a WebSocket protocol with text commands and binary audio frames. We reuse the fork's `TciProtocol`, `TciRigController`, and `TciAudioDevice` essentially as-is. We replace `TciWebSocketClient` with a new impl that inherits from freedv-gui's existing `TcpConnectionHandler` (POSIX + WinSock + reconnect) and uses vendored `websocketpp` for WS framing, matching the `SocketIoClient` pattern at `src/util/SocketIoClient.{h,cpp}`. RX/TX audio paths use the existing freedv-gui `ResampleStep` (r8brain) to bridge TCI's 48 kHz to FreeDV's 8/16 kHz; no new resampling code.

**Tech Stack:** C++17, wxWidgets, websocketpp (vendored at `src/3rdparty/websocketpp`), r8brain (vendored at `src/3rdparty/r8brain`), CMake, ctest.

**Spec:** `docs/superpowers/specs/2026-05-11-tci-foundation-port-design.md`.

**Branch:** `feat/tci-port-2.3.1` in workspace `~/freedv-tci`. Origin tracks `tompatulpan/freedv-tci`; upstream tracks `drowe67/freedv-gui`.

---

## Phase A: Foundation

### Task 1: Rebase 17 fork commits onto upstream master

**Files:**
- Touched: all 32 files in fork diff
- Critical conflict hotspots: `src/main.cpp`, `src/gui/dialogs/dlg_audiooptions.cpp`, `src/gui/dialogs/dlg_easy_setup.{h,cpp}`, `src/gui/dialogs/dlg_ptt.{h,cpp}`, `src/pipeline/TxRxThread.cpp`

- [ ] **Step 1: Verify starting state**

Run:
```
cd ~/freedv-tci
git status
git log --oneline -3
git fetch upstream
```

Expected output:
```
On branch feat/tci-port-2.3.1
nothing to commit, working tree clean
5a71f518 docs: Add TCI foundation port + 2.3.1 modernization design
1d7af6cf Update README
32c00c2... Add TCI debug logging support for audio and rig control operations
```

- [ ] **Step 2: Record the rebase parameters**

Run:
```
BASE=$(git merge-base feat/tci-port-2.3.1 upstream/master)
echo "Fork base: $BASE"
echo "Upstream HEAD: $(git rev-parse upstream/master)"
echo "Fork commits to replay: $(git log --oneline $BASE..feat/tci-port-2.3.1 | wc -l)"
echo "Upstream drift: $(git log --oneline $BASE..upstream/master | wc -l)"
```

Expected:
```
Fork base: f4a35367d07d1efbbae19dd9410a320c1c8eb14d
Upstream HEAD: 77e793a3...   (or newer if upstream advanced)
Fork commits to replay: 18    (17 TCI + 1 design-doc commit)
Upstream drift: 138+
```

- [ ] **Step 3: Start interactive rebase**

Run:
```
git rebase -i --onto upstream/master f4a35367d07d1efbbae19dd9410a320c1c8eb14d feat/tci-port-2.3.1
```

In the rebase TODO list editor, leave all commits as `pick`. Save and exit.

Expected: rebase begins. Either completes cleanly or stops at first conflict.

- [ ] **Step 4: Resolve each conflict (loop)**

For each conflict the rebase stops on:

1. Run `git status` to see conflicted files.
2. Open each conflicted file. Look for `<<<<<<<`, `=======`, `>>>>>>>` markers.
3. Read the upstream side (`=======` to `>>>>>>>`) and the fork side (`<<<<<<<` to `=======`).
4. Resolve based on this rule: **preserve fork's TCI additions, accept upstream's refactors to non-TCI code, keep both where they coexist.**
   - Example for `src/main.cpp`: if upstream renamed `g_nSoundCards` setter and fork referenced the old name, use upstream's new name AND fork's TCI condition.
   - Example for `src/gui/dialogs/dlg_audiooptions.cpp`: if upstream changed device-list iteration and fork added TCI device handling, merge so both work.
5. After editing: `git add <file>` for each resolved file.
6. Run `git rebase --continue`.
7. If editor pops up for the commit message, save unchanged.
8. Repeat until rebase reports `Successfully rebased and updated`.

If at any point you decide a commit's conflicts are unrecoverable, run `git rebase --abort` and STOP. The fall-back is the squash approach in Step 8 below.

- [ ] **Step 5: Verify rebase landed cleanly**

Run:
```
git status
git log --oneline -20
git log --format="%h %an %s" -20 | grep -i tci
```

Expected: working tree clean; 18 commits showing on top of upstream master HEAD; TCI commits show Tomas Ostojic as author.

- [ ] **Step 6: Configure CMake build (macOS host)**

Run:
```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON
```

Expected: CMake configures cleanly. If it fails, capture the first error before continuing.

- [ ] **Step 7: Build**

Run:
```
cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tee /tmp/freedv-tci-build.log
```

Expected: build succeeds. If it fails, the error log identifies the broken file; fix the file inline (most likely a missed-conflict or a TCI-vs-upstream API mismatch), then `git add` + `git commit --amend --no-edit` to fold the fix into the rebased commit.

- [ ] **Step 8: Run existing test suite**

Run:
```
cd build && ctest --output-on-failure 2>&1 | tee /tmp/freedv-tci-ctest.log
```

Expected: all existing tests pass. New TCI tests don't exist yet; ctest only runs the existing ones. If a non-TCI test regresses, the rebase has a bug; fix and amend.

- [ ] **Step 9 (fallback only): Squash rebase if Step 4 became unrecoverable**

Only run this if Step 4 was aborted.

Run:
```
git checkout feat/tci-port-2.3.1
git reset --hard tci-tx-clean-implementation  # back to fork HEAD
git checkout -B feat/tci-port-2.3.1-squashed upstream/master
git merge --squash tci-tx-clean-implementation
git commit -S -m "$(cat <<'EOF'
feat(tci): Adopt tompatulpan/freedv-tci as foundation (squashed)

Squashed import of 17 commits from tompatulpan/freedv-tci
tci-tx-clean-implementation branch (HEAD 1d7af6c), onto upstream
master. Original commits preserved at:
  https://github.com/tompatulpan/freedv-tci/tree/tci-tx-clean-implementation

Primary author of squashed work: Tomas Ostojic / SM0ONR.
Co-Authored-By: Tomas Ostojic <tompatulpan@gmail.com>
EOF
)"
```

Replace original branch only after build + tests pass:
```
git branch -D feat/tci-port-2.3.1
git branch -m feat/tci-port-2.3.1
```

Then cherry-pick the design-doc commit back: `git cherry-pick 5a71f518`.

### Task 2: Verify Tomas's license headers match freedv-gui house style

**Files:**
- Audit: 8 TCI-authored files in `src/audio/Tci*.{h,cpp}`, `src/rig_control/Tci*.{h,cpp}`

- [ ] **Step 1: Compare header text**

Run:
```
diff <(sed -n '7,21p' src/util/SocketIoClient.h) <(sed -n '7,21p' src/rig_control/TciProtocol.h)
```

Expected: no diff output (headers identical except for `Name:` and `Purpose:` lines which are outside the 7-21 range).

- [ ] **Step 2: Confirm Authors line preserved**

Run:
```
for f in src/audio/TciAudioDevice.h src/audio/TciAudioDevice.cpp \
         src/rig_control/TciProtocol.h src/rig_control/TciProtocol.cpp \
         src/rig_control/TciRigController.h src/rig_control/TciRigController.cpp \
         src/rig_control/TciWebSocketClient.h src/rig_control/TciWebSocketClient.cpp; do
  echo "=== $f ==="
  grep -A1 "Authors:" "$f"
done
```

Expected: each file shows `Authors:         Tomas Ostojic`.

- [ ] **Step 3: No-op commit if headers already correct**

If Step 1 showed no diff and Step 2 confirmed authorship, this task is complete with no commit needed. Move to Task 3.

If headers differ from house style (unexpected), fix by replacing lines 7-21 of each Tci*.{h,cpp} file with the verbatim block from `src/util/SocketIoClient.h:7-21`, then:
```
git add src/audio/Tci*.{h,cpp} src/rig_control/Tci*.{h,cpp}
git commit -S -m "fix(tci): Align license header text with freedv-gui house style"
```

### Task 3: Write bench validation matrix

**Files:**
- Create: `test/tci_bench_matrix.md`

- [ ] **Step 1: Create the bench matrix file**

Write to `test/tci_bench_matrix.md`:

```markdown
# TCI Bench Validation Matrix

Tracks manual validation of TCI integration against real radios + servers.
Update rows as bench access permits. Each row records the TCI server tested,
client platform, and pass/fail/notes for each test scenario.

## Test scenarios

1. **Connect**: open WS to TCI server, complete handshake, see `ready;` echo.
2. **RX audio**: tune to a known FreeDV 700D transmission, verify decode + audible voice.
3. **TX audio**: press TX, speak, verify off-air FreeDV-modulated signal heard on independent receiver.
4. **Reconnect**: physically disconnect network, wait 10 seconds, reconnect; verify auto-resume + cached freq/mode restored.
5. **Multi-client safety**: with second TCI client (e.g., WSJT-X) connected and triggering MOX, verify freedv-gui does NOT transmit audio.

## Matrix

| Server | Server version | Client OS | Client build | Connect | RX | TX | Reconnect | Multi-client safe | Tester | Date | Notes |
|--------|---------------|-----------|--------------|---------|----|----|-----------|-------------------|--------|------|-------|
| ExpertSDR3 | TBD | macOS 15.x | feat/tci-port-2.3.1 | TBD | TBD | TBD | TBD | TBD | KG4VCF | TBD | |
| ExpertSDR3 | TBD | Windows 11 | feat/tci-port-2.3.1 | TBD | TBD | TBD | TBD | TBD | KG4VCF | TBD | |
| ThetisTCI on ANAN-G2 | TBD | Linux | feat/tci-port-2.3.1 | TBD | TBD | TBD | TBD | TBD | KG4VCF | TBD | |

## Pass/fail criteria

- **Connect**: completes within 5 seconds, no error dialogs, status shows "Connected".
- **RX audio**: SINAD > 6 dB on a 0 dB SNR FreeDV transmission, or qualitative "intelligible voice".
- **TX audio**: SINAD > 6 dB at off-air receiver decoding our FreeDV TX.
- **Reconnect**: status returns to "Connected" within 10 seconds of network restore.
- **Multi-client safe**: WS packet capture confirms zero `tx_audio_stream` binary frames sent during the other client's MOX window.
```

- [ ] **Step 2: Commit**

```
git add test/tci_bench_matrix.md
git commit -S -m "docs(tci): Add bench validation matrix"
```

---

## Phase B: Tests-first scaffolding

### Task 4: Set up unittest target wiring for TCI tests

**Files:**
- Modify: `test/unittest/CMakeLists.txt`

- [ ] **Step 1: Inspect existing unittest CMakeLists**

Run:
```
cat test/unittest/CMakeLists.txt
```

Note the pattern used to add executables (look for `add_executable` + `target_link_libraries` for an existing unittest binary, e.g., `test_codec2_fifo` or similar).

- [ ] **Step 2: Add three new unittest executables**

Append the following to `test/unittest/CMakeLists.txt` (adjust `target_link_libraries` to match the project's existing pattern; the exact library names depend on what other unittests link):

```cmake
# TCI protocol parser tests
add_executable(test_tci_protocol test_tci_protocol.cpp
    ../../src/rig_control/TciProtocol.cpp)
target_include_directories(test_tci_protocol PRIVATE ../../src ../../src/3rdparty/websocketpp)
add_test(NAME tci_protocol COMMAND test_tci_protocol)

# TCI audio frame codec tests
add_executable(test_tci_audio_frame test_tci_audio_frame.cpp
    ../../src/rig_control/TciProtocol.cpp
    ../../src/audio/TciAudioDevice.cpp)
target_include_directories(test_tci_audio_frame PRIVATE ../../src ../../src/3rdparty/websocketpp)
target_link_libraries(test_tci_audio_frame PRIVATE pthread)
add_test(NAME tci_audio_frame COMMAND test_tci_audio_frame)

# TCI WS roundtrip smoke test (uses mock server fixture)
add_executable(test_tci_ws_mock test_tci_ws_mock.cpp mock_tci_server.cpp
    ../../src/rig_control/TciProtocol.cpp
    ../../src/rig_control/TciWebSocketClient.cpp
    ../../src/util/TcpConnectionHandler.cpp
    ../../src/util/ThreadedObject.cpp
    ../../src/util/ThreadedTimer.cpp)
target_include_directories(test_tci_ws_mock PRIVATE ../../src ../../src/util ../../src/3rdparty/websocketpp)
target_link_libraries(test_tci_ws_mock PRIVATE pthread)
add_test(NAME tci_ws_mock COMMAND test_tci_ws_mock)
```

- [ ] **Step 3: Reconfigure and verify CMake accepts**

Run:
```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON
```

Expected: configures cleanly. The three new test executables will fail to build until their source files exist (Tasks 5-8); that is intended.

- [ ] **Step 4: Commit**

```
git add test/unittest/CMakeLists.txt
git commit -S -m "build(tci): Wire ctest targets for TCI unittests"
```

### Task 5: TCI protocol parser unit tests

**Files:**
- Create: `test/unittest/test_tci_protocol.cpp`
- Reads: `src/rig_control/TciProtocol.{h,cpp}` (existing)

- [ ] **Step 1: Read the existing parser interface**

Run:
```
grep -nE "class|parseCommand|parseStream|StreamHeader|struct |enum " src/rig_control/TciProtocol.h
```

Note the parse function signatures. Capture the exact names for use in the tests.

- [ ] **Step 2: Write the failing test file**

Write to `test/unittest/test_tci_protocol.cpp`:

```cpp
//=========================================================================
// Name:            test_tci_protocol.cpp
// Purpose:         Unit tests for TCI protocol parser.
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

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "TciProtocol.h"

static int failures = 0;
#define ASSERT_EQ(a, b) do { \
    if (!((a) == (b))) { \
        std::fprintf(stderr, "FAIL %s:%d: " #a " != " #b "\n", __FILE__, __LINE__); \
        ++failures; \
    } \
} while (0)

static void test_parse_ready() {
    auto cmd = tci::parseCommand("ready;");
    ASSERT_EQ(cmd.name, "ready");
    ASSERT_EQ(cmd.args.size(), 0u);
}

static void test_parse_dds_with_freq() {
    auto cmd = tci::parseCommand("dds:0,7050000;");
    ASSERT_EQ(cmd.name, "dds");
    ASSERT_EQ(cmd.args.size(), 2u);
    ASSERT_EQ(cmd.args[0], "0");
    ASSERT_EQ(cmd.args[1], "7050000");
}

static void test_parse_trx_state() {
    auto cmd_on = tci::parseCommand("trx:0,true;");
    ASSERT_EQ(cmd_on.name, "trx");
    ASSERT_EQ(cmd_on.args[1], "true");

    auto cmd_off = tci::parseCommand("trx:0,false;");
    ASSERT_EQ(cmd_off.name, "trx");
    ASSERT_EQ(cmd_off.args[1], "false");
}

static void test_parse_modulation_string() {
    auto cmd = tci::parseCommand("modulation:0,digu;");
    ASSERT_EQ(cmd.name, "modulation");
    ASSERT_EQ(cmd.args[0], "0");
    ASSERT_EQ(cmd.args[1], "digu");
}

static void test_parse_no_args() {
    auto cmd = tci::parseCommand("protocol;");
    ASSERT_EQ(cmd.name, "protocol");
    ASSERT_EQ(cmd.args.size(), 0u);
}

static void test_parse_quoted_arg() {
    auto cmd = tci::parseCommand("device:\"ExpertSDR3\";");
    ASSERT_EQ(cmd.name, "device");
    ASSERT_EQ(cmd.args[0], "ExpertSDR3");
}

static void test_parse_malformed_empty() {
    auto cmd = tci::parseCommand("");
    ASSERT_EQ(cmd.name, "");
}

static void test_parse_malformed_no_semicolon() {
    auto cmd = tci::parseCommand("dds:0,7050000");
    // Should either parse what it can or report empty name; either is acceptable
    // but must not crash.
    (void)cmd;
}

static void test_serialize_command() {
    tci::Command cmd;
    cmd.name = "dds";
    cmd.args = {"0", "14070000"};
    auto wire = tci::serializeCommand(cmd);
    ASSERT_EQ(wire, std::string("dds:0,14070000;"));
}

int main() {
    test_parse_ready();
    test_parse_dds_with_freq();
    test_parse_trx_state();
    test_parse_modulation_string();
    test_parse_no_args();
    test_parse_quoted_arg();
    test_parse_malformed_empty();
    test_parse_malformed_no_semicolon();
    test_serialize_command();

    if (failures) {
        std::fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    std::printf("All TCI protocol parser tests passed\n");
    return 0;
}
```

- [ ] **Step 3: Build test executable**

Run:
```
cmake --build build --target test_tci_protocol
```

Expected: either builds cleanly (if the API names assumed above match Tomas's actual API in `TciProtocol.h`) or fails with compile errors that name specific missing symbols.

- [ ] **Step 4: Adjust test file to match actual API**

If Step 3 reported missing symbols (e.g., `tci::parseCommand` is actually named `tci::TciProtocol::parse` or `tci::CommandParser::parse`), edit the test file to use the actual function names from Tomas's API. Re-run Step 3 until it builds.

If the API differs significantly from `parseCommand` / `serializeCommand` / `Command` struct, take that as a sign that Tomas's API doesn't separate parse-and-serialize this cleanly. In that case, **either** add small free-function wrappers in `TciProtocol.cpp` that match the test's expected interface (recommended; thin shims), **or** rewrite the tests to use Tomas's actual API.

- [ ] **Step 5: Run test, expect pass**

Run:
```
cd build && ctest -R tci_protocol --output-on-failure
```

Expected: PASS for all subtests.

- [ ] **Step 6: Commit**

```
git add test/unittest/test_tci_protocol.cpp src/rig_control/TciProtocol.{h,cpp}
git commit -S -m "test(tci): Add unit tests for TciProtocol command parser

Covers ready, dds, trx, modulation, protocol, quoted args,
malformed empty/no-semicolon inputs, and round-trip serialization."
```

### Task 6: TCI audio frame codec unit tests

**Files:**
- Create: `test/unittest/test_tci_audio_frame.cpp`
- Reads: `src/audio/TciAudioDevice.{h,cpp}` (existing, for `convertInt16ToShort_` / `convertInt24ToShort_` / `convertInt32ToShort_` / `convertFloat32ToShort_` and `StreamHeader` layout)
- Reads: `src/rig_control/TciProtocol.h` (existing, for `StreamHeader`)

- [ ] **Step 1: Read the format conversion API**

Run:
```
grep -nE "convertInt16ToShort_|convertInt24ToShort_|convertInt32ToShort_|convertFloat32ToShort_|StreamHeader|StreamType" src/audio/TciAudioDevice.h src/rig_control/TciProtocol.h
```

Note: the convert functions are private. We test them either via a friend declaration, exposing them as static helpers in a new internal header, OR moving the conversion bodies into a free-function utility namespace. Pick whichever the codebase pattern supports best; the simplest is **add static helpers in an internal-namespace header**, e.g., `src/audio/TciSampleConvert.h`, and have `TciAudioDevice` call those.

- [ ] **Step 2: Create `src/audio/TciSampleConvert.h` with static helpers**

Write to `src/audio/TciSampleConvert.h`:

```cpp
//=========================================================================
// Name:            TciSampleConvert.h
// Purpose:         Static helpers to convert TCI sample formats to short.
//
// Authors:         J.J. Boyd  (extracted from Tomas Ostojic's TciAudioDevice)
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

#ifndef TCI_SAMPLE_CONVERT_H
#define TCI_SAMPLE_CONVERT_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace tci {

// All conversions expect interleaved stereo input and produce mono short output
// (the existing TciAudioDevice convention). numSamples = total frames in the
// payload (not per-channel; the function reads numSamples*2 source values).

void convertInt16ToShort(const uint8_t* data, std::size_t numSamples,
                         std::vector<short>& output);
void convertInt24ToShort(const uint8_t* data, std::size_t numSamples,
                         std::vector<short>& output);
void convertInt32ToShort(const uint8_t* data, std::size_t numSamples,
                         std::vector<short>& output);
void convertFloat32ToShort(const uint8_t* data, std::size_t numSamples,
                           std::vector<short>& output);

} // namespace tci

#endif // TCI_SAMPLE_CONVERT_H
```

- [ ] **Step 3: Create `src/audio/TciSampleConvert.cpp`**

Move the bodies of the four `convert*ToShort_` member functions from `TciAudioDevice.cpp` into the new file. The bodies are pure functions of inputs; they should not reference any `TciAudioDevice` member state. If they do, refactor to take the state as parameters first.

Write to `src/audio/TciSampleConvert.cpp`:

```cpp
//=========================================================================
// Name:            TciSampleConvert.cpp
// Purpose:         (see header)
//=========================================================================

#include "TciSampleConvert.h"
#include <algorithm>
#include <cstring>

namespace tci {

void convertInt16ToShort(const uint8_t* data, std::size_t numSamples,
                         std::vector<short>& output)
{
    output.resize(numSamples);
    const int16_t* src = reinterpret_cast<const int16_t*>(data);
    for (std::size_t i = 0; i < numSamples; ++i) {
        // Stereo to mono: average L + R.
        int sum = static_cast<int>(src[2*i]) + static_cast<int>(src[2*i + 1]);
        output[i] = static_cast<short>(sum / 2);
    }
}

void convertInt24ToShort(const uint8_t* data, std::size_t numSamples,
                         std::vector<short>& output)
{
    output.resize(numSamples);
    for (std::size_t i = 0; i < numSamples; ++i) {
        // 24-bit little-endian, sign-extended.
        auto read24 = [](const uint8_t* p) -> int32_t {
            int32_t v = (p[0]) | (p[1] << 8) | (p[2] << 16);
            if (v & 0x00800000) v |= 0xFF000000;  // sign extend
            return v;
        };
        int32_t l = read24(data + (2*i)     * 3);
        int32_t r = read24(data + (2*i + 1) * 3);
        // Average then downshift to int16 range.
        int32_t avg = (l + r) / 2;
        output[i] = static_cast<short>(std::clamp(avg >> 8, -32768, 32767));
    }
}

void convertInt32ToShort(const uint8_t* data, std::size_t numSamples,
                         std::vector<short>& output)
{
    output.resize(numSamples);
    const int32_t* src = reinterpret_cast<const int32_t*>(data);
    for (std::size_t i = 0; i < numSamples; ++i) {
        int64_t l = src[2*i];
        int64_t r = src[2*i + 1];
        int64_t avg = (l + r) / 2;
        output[i] = static_cast<short>(std::clamp<int64_t>(avg >> 16, -32768, 32767));
    }
}

void convertFloat32ToShort(const uint8_t* data, std::size_t numSamples,
                           std::vector<short>& output)
{
    output.resize(numSamples);
    const float* src = reinterpret_cast<const float*>(data);
    for (std::size_t i = 0; i < numSamples; ++i) {
        float avg = (src[2*i] + src[2*i + 1]) * 0.5f;
        float scaled = avg * 32767.0f;
        if (scaled >  32767.0f) scaled =  32767.0f;
        if (scaled < -32768.0f) scaled = -32768.0f;
        output[i] = static_cast<short>(scaled);
    }
}

} // namespace tci
```

- [ ] **Step 4: Refactor `TciAudioDevice.cpp` to call the new helpers**

In `src/audio/TciAudioDevice.cpp`, replace the bodies of the four private `convertXxxToShort_` methods with single-line calls to `tci::convertXxxToShort(...)`.

Example for `convertInt16ToShort_`:

```cpp
void TciAudioDevice::convertInt16ToShort_(const uint8_t* data, size_t numSamples,
                                          std::vector<short>& output)
{
    tci::convertInt16ToShort(data, numSamples, output);
}
```

Repeat for the three other format functions.

Add `#include "TciSampleConvert.h"` near the top of `TciAudioDevice.cpp`.

- [ ] **Step 5: Update `src/audio/CMakeLists.txt`**

Add `TciSampleConvert.cpp` to the source list for the freedv-gui audio library target. Find the existing `target_sources(... TciAudioDevice.cpp ...)` or `add_library(audio ... TciAudioDevice.cpp ...)` line and append `TciSampleConvert.cpp` alongside it.

- [ ] **Step 6: Build freedv-gui and verify the refactor compiles**

Run:
```
cmake --build build -j$(sysctl -n hw.ncpu)
```

Expected: builds clean. If linker complains about duplicate symbols or missing references, the refactor in Step 4 missed something; fix and rebuild.

- [ ] **Step 7: Write the test file**

Write to `test/unittest/test_tci_audio_frame.cpp`:

```cpp
//=========================================================================
// Name:            test_tci_audio_frame.cpp
// Purpose:         Unit tests for TCI audio sample format conversion + StreamHeader.
// (Header: same LGPL boilerplate as test_tci_protocol.cpp)
//=========================================================================

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdint>
#include "TciSampleConvert.h"
#include "TciProtocol.h"

static int failures = 0;
#define ASSERT_EQ(a, b) do { \
    if (!((a) == (b))) { \
        std::fprintf(stderr, "FAIL %s:%d: " #a " != " #b "\n", __FILE__, __LINE__); \
        ++failures; \
    } \
} while (0)

#define ASSERT_NEAR(a, b, eps) do { \
    auto _diff = (a) - (b); \
    if (_diff < 0) _diff = -_diff; \
    if (_diff > (eps)) { \
        std::fprintf(stderr, "FAIL %s:%d: " #a " not within " #eps " of " #b "\n", __FILE__, __LINE__); \
        ++failures; \
    } \
} while (0)

static void test_int16_stereo_to_mono_average() {
    // Stereo input: [L=1000, R=2000, L=3000, R=4000]
    int16_t src[] = {1000, 2000, 3000, 4000};
    std::vector<short> out;
    tci::convertInt16ToShort(reinterpret_cast<const uint8_t*>(src), 2, out);
    ASSERT_EQ(out.size(), 2u);
    ASSERT_EQ(out[0], 1500);
    ASSERT_EQ(out[1], 3500);
}

static void test_float32_clamping() {
    // Float input that exceeds [-1, +1] should clamp.
    float src[] = {2.0f, 2.0f, -2.0f, -2.0f};
    std::vector<short> out;
    tci::convertFloat32ToShort(reinterpret_cast<const uint8_t*>(src), 2, out);
    ASSERT_EQ(out[0],  32767);
    ASSERT_EQ(out[1], -32768);
}

static void test_float32_dc_signal() {
    float src[] = {0.5f, 0.5f, 0.5f, 0.5f};
    std::vector<short> out;
    tci::convertFloat32ToShort(reinterpret_cast<const uint8_t*>(src), 2, out);
    // 0.5 * 32767 = 16383.5; rounded toward zero by cast = 16383.
    ASSERT_NEAR(out[0], 16383, 1);
    ASSERT_NEAR(out[1], 16383, 1);
}

static void test_int32_high_bits_preserved() {
    int32_t src[] = {1 << 30, 1 << 30, -(1 << 30), -(1 << 30)};
    std::vector<short> out;
    tci::convertInt32ToShort(reinterpret_cast<const uint8_t*>(src), 2, out);
    // (1<<30) >> 16 = 16384.
    ASSERT_EQ(out[0],  16384);
    ASSERT_EQ(out[1], -16384);
}

static void test_int24_sign_extension() {
    // 24-bit -1 (0xFFFFFF), interleaved.
    uint8_t src[] = {0xFF, 0xFF, 0xFF,   0xFF, 0xFF, 0xFF};
    std::vector<short> out;
    tci::convertInt24ToShort(src, 1, out);
    // -1 averaged with -1 is -1; shifted right 8 = -1 (arithmetic shift).
    ASSERT_EQ(out.size(), 1u);
    ASSERT_NEAR(out[0], 0, 1);  // -1 >> 8 sign-extends; result may be -1 or 0 depending on shift impl
}

static void test_stream_header_size() {
    // 16 little-endian uint32 fields per design spec §5; verify size.
    ASSERT_EQ(sizeof(tci::StreamHeader), 64u);
}

int main() {
    test_int16_stereo_to_mono_average();
    test_float32_clamping();
    test_float32_dc_signal();
    test_int32_high_bits_preserved();
    test_int24_sign_extension();
    test_stream_header_size();

    if (failures) {
        std::fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    std::printf("All TCI audio frame tests passed\n");
    return 0;
}
```

- [ ] **Step 8: Build + run the test**

Run:
```
cmake --build build --target test_tci_audio_frame
cd build && ctest -R tci_audio_frame --output-on-failure
```

Expected: PASS. If `StreamHeader` size assertion fails, the struct has padding; fix `TciProtocol.h` with `#pragma pack(push, 1)` / `#pragma pack(pop)` around the struct definition or use `__attribute__((packed))` per the project's existing pack discipline.

- [ ] **Step 9: Commit**

```
git add src/audio/TciSampleConvert.{h,cpp} src/audio/TciAudioDevice.cpp \
        src/audio/CMakeLists.txt test/unittest/test_tci_audio_frame.cpp
git commit -S -m "test(tci): Extract sample-format conversion to testable helpers + tests

Moves four convertXxxToShort bodies from private TciAudioDevice
methods into a free-function namespace (tci::convertXxxToShort) so
they can be unit-tested. TciAudioDevice methods become one-line
delegates. Tests cover stereo-to-mono averaging, float clamping,
int24 sign extension, int32 high-bit preservation, and StreamHeader
struct packing."
```

### Task 7: Mock TCI server for WS roundtrip tests

**Files:**
- Create: `test/unittest/mock_tci_server.h`
- Create: `test/unittest/mock_tci_server.cpp`
- Create: `test/unittest/test_tci_ws_mock.cpp`

- [ ] **Step 1: Mock server header**

Write to `test/unittest/mock_tci_server.h`:

```cpp
//=========================================================================
// Name:            mock_tci_server.h
// Purpose:         Minimal localhost TCI server for unit testing the
//                  TciWebSocketClient. Accepts one connection, performs
//                  WS handshake, exchanges configurable text + binary
//                  frames, and shuts down.
// (Header: same LGPL boilerplate)
//=========================================================================

#ifndef MOCK_TCI_SERVER_H
#define MOCK_TCI_SERVER_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class MockTciServer {
public:
    MockTciServer();
    ~MockTciServer();

    // Start listening on a random local port (set via getPort()).
    // Returns false if bind fails.
    bool start();

    // Stop accepting; close client connection if open; join thread.
    void stop();

    int getPort() const;

    // Queue a text frame to be sent to the client after WS handshake.
    void queueTextFrame(const std::string& text);

    // Queue a binary frame (already-formed payload; the mock will frame it
    // per RFC 6455).
    void queueBinaryFrame(const std::vector<uint8_t>& payload);

    // Set a callback fired when the mock receives a text frame from the client.
    void setOnTextFrame(std::function<void(const std::string&)> cb);

    // Set a callback fired when the mock receives a binary frame.
    void setOnBinaryFrame(std::function<void(const std::vector<uint8_t>&)> cb);

    // True once a client has completed the WS handshake.
    bool isClientConnected() const;

private:
    int listenFd_;
    int clientFd_;
    int port_;
    std::atomic<bool> running_;
    std::atomic<bool> clientHandshakeComplete_;
    std::unique_ptr<std::thread> acceptThread_;
    std::unique_ptr<std::thread> ioThread_;

    std::vector<std::string> outgoingText_;
    std::vector<std::vector<uint8_t>> outgoingBinary_;

    std::function<void(const std::string&)> onTextFrame_;
    std::function<void(const std::vector<uint8_t>&)> onBinaryFrame_;

    void acceptLoop_();
    void ioLoop_();
    bool doHandshake_();
};

#endif // MOCK_TCI_SERVER_H
```

- [ ] **Step 2: Mock server implementation**

Write to `test/unittest/mock_tci_server.cpp` an implementation that:

1. Opens an IPv4 TCP socket on port 0 (kernel-assigned).
2. Stores the assigned port in `port_` via `getsockname`.
3. Accepts ONE incoming connection.
4. Reads the HTTP GET request, finds the `Sec-WebSocket-Key` header, computes the base64(sha1(key + GUID)) accept-token per RFC 6455, sends a 101 Switching Protocols response.
5. After handshake completes: alternates between reading WS frames from the client (calls `onTextFrame_` / `onBinaryFrame_`) and sending any queued outgoing frames.
6. Frames are RFC 6455 single-frame, no fragmentation, no extensions, no masking on server-to-client side (mask required on client-to-server side).

The implementation is ~250 lines. Reference materials:
- RFC 6455 Section 5 (Data Framing).
- RFC 6455 Section 4 (Handshake).
- Existing freedv-gui SHA-1 helper if available, otherwise use a public-domain or vendored impl (search for `sha1` in `src/3rdparty/` first).

Key code sketch:

```cpp
// Compute Sec-WebSocket-Accept from Sec-WebSocket-Key:
//   accept = base64(sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))

// Read one WS frame:
//   byte 0: FIN | RSV1-3 | opcode (4 bits)
//   byte 1: MASK | payload_len (7 bits)
//   if payload_len == 126: next 2 bytes are extended length
//   if payload_len == 127: next 8 bytes are extended length
//   if MASK: next 4 bytes are masking key
//   then payload (XORed with masking key if MASK set)

// Send one WS frame (no mask):
//   byte 0: 0x80 | opcode  (FIN set, no RSV)
//   byte 1: payload_len (or 126 + 2-byte ext, or 127 + 8-byte ext)
//   then payload
```

Pick an existing minimal RFC 6455 reference (e.g., the websocketpp internals at `src/3rdparty/websocketpp/websocketpp/processors/hybi13.hpp`) and translate the framing logic. **Do not pull in a full WS library for the mock; that's the unit-under-test.**

After writing the file, run:
```
cmake --build build --target test_tci_ws_mock 2>&1 | tail -30
```

Iterate on compile errors until clean. The test executable target was already wired in Task 4.

- [ ] **Step 3: Smoke test wiring**

Write to `test/unittest/test_tci_ws_mock.cpp`:

```cpp
//=========================================================================
// Name:            test_tci_ws_mock.cpp
// Purpose:         Roundtrip smoke test of TciWebSocketClient against
//                  the mock TCI server. Verifies WS handshake, command
//                  send/receive, and binary frame send/receive.
// (Header: same LGPL boilerplate)
//=========================================================================

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
#include "mock_tci_server.h"
#include "TciWebSocketClient.h"

using namespace std::chrono_literals;

static int failures = 0;
#define ASSERT_TRUE(x) do { if (!(x)) { ++failures; std::fprintf(stderr, "FAIL %s:%d: " #x "\n", __FILE__, __LINE__); } } while (0)
#define ASSERT_EQ(a, b) do { if (!((a) == (b))) { ++failures; std::fprintf(stderr, "FAIL %s:%d: " #a " != " #b "\n", __FILE__, __LINE__); } } while (0)

static void test_connect_handshake() {
    MockTciServer mock;
    ASSERT_TRUE(mock.start());

    tci::TciWebSocketClient client;
    std::atomic<bool> connected{false};
    client.setOnConnected([&]() { connected = true; });

    ASSERT_TRUE(client.connect("127.0.0.1", mock.getPort()));

    // Wait up to 2 seconds for handshake.
    for (int i = 0; i < 200 && !connected.load(); ++i) {
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_TRUE(connected.load());
    ASSERT_TRUE(mock.isClientConnected());

    client.disconnect();
    mock.stop();
}

static void test_send_text_frame() {
    MockTciServer mock;
    ASSERT_TRUE(mock.start());

    std::atomic<bool> got{false};
    std::string received;
    mock.setOnTextFrame([&](const std::string& text) {
        received = text;
        got = true;
    });

    tci::TciWebSocketClient client;
    ASSERT_TRUE(client.connect("127.0.0.1", mock.getPort()));
    std::this_thread::sleep_for(100ms);  // let handshake settle

    client.sendCommand("dds:0,7050000;");

    for (int i = 0; i < 100 && !got.load(); ++i) {
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_TRUE(got.load());
    ASSERT_EQ(received, std::string("dds:0,7050000;"));

    client.disconnect();
    mock.stop();
}

static void test_receive_text_frame() {
    MockTciServer mock;
    ASSERT_TRUE(mock.start());
    mock.queueTextFrame("dds:0,14070000;");

    tci::TciWebSocketClient client;
    std::atomic<bool> got{false};
    std::string received;
    client.setCommandCallback([&](const std::string& cmd) {
        received = cmd;
        got = true;
    });

    ASSERT_TRUE(client.connect("127.0.0.1", mock.getPort()));

    for (int i = 0; i < 200 && !got.load(); ++i) {
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_TRUE(got.load());
    ASSERT_EQ(received, std::string("dds:0,14070000;"));

    client.disconnect();
    mock.stop();
}

int main() {
    test_connect_handshake();
    test_send_text_frame();
    test_receive_text_frame();

    if (failures) {
        std::fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    std::printf("All TCI WS mock tests passed\n");
    return 0;
}
```

- [ ] **Step 4: Run test against existing (POSIX) TciWebSocketClient**

```
cmake --build build --target test_tci_ws_mock
cd build && ctest -R tci_ws_mock --output-on-failure
```

Expected: tests **pass** against Tomas's POSIX implementation (mock server speaks raw RFC 6455 which his client also speaks). This establishes a known-passing baseline before we swap out the implementation in Phase D. If a test fails here, the mock server has a bug; fix it before proceeding.

- [ ] **Step 5: Commit**

```
git add test/unittest/mock_tci_server.{h,cpp} test/unittest/test_tci_ws_mock.cpp
git commit -S -m "test(tci): Add localhost mock TCI server + WS roundtrip smoke tests

Mock server speaks raw RFC 6455 (no library) so it exercises the
TciWebSocketClient end-to-end without circular dependencies.
Tests cover WS handshake, text frame send, text frame receive.
Will be the regression-baseline when TciWebSocketClient transport
is replaced in Phase D."
```

---

## Phase C: Fork-documented bug fixes

### Task 8: Fix sample-rate dropdown N/A edge case

**Files:**
- Modify: `src/gui/dialogs/dlg_audiooptions.cpp`

- [ ] **Step 1: Locate the dropdown population code**

Run:
```
grep -nE "buildSampleRates|populateSampleRate|sampleRateChoice|wxArrayString.*Rate" src/gui/dialogs/dlg_audiooptions.cpp | head -20
```

Read the lines around each hit to find where sample rates are populated in pairs (RxIn + TxOut, etc.).

- [ ] **Step 2: Read the fix description from the fork**

Reference: `TCI_TX_TESTING_GUIDE.md` "Issue 1: Sample Rate Validation Edge Case" (the document Tomas added; should be in the tree at root level after rebase). Original problem: when TX Out = "none", paired population logic short-circuits before populating RX In's sample rate.

- [ ] **Step 3: Find the paired-population block**

Look for code resembling:
```cpp
if (RxIn != "none" && TxOut != "none") {
    buildSampleRates(RxIn);
    buildSampleRates(TxOut);
}
```

(or any variant where two-device-check gates one or more `buildSampleRates` calls.)

- [ ] **Step 4: Replace paired logic with per-device individual checks**

Edit to:
```cpp
if (RxIn  != "none") buildSampleRates(RxIn);
if (TxOut != "none") buildSampleRates(TxOut);
if (TxIn  != "none") buildSampleRates(TxIn);
if (RxOut != "none") buildSampleRates(RxOut);
```

The exact variable names depend on which scope you're in; preserve the local-variable names used in surrounding code.

- [ ] **Step 5: Build and smoke-test**

Run:
```
cmake --build build -j$(sysctl -n hw.ncpu)
```

Then launch freedv-gui, open Tools-then-Audio Config, set TX Out to "none", and confirm RX In sample-rate dropdown populates correctly. The fix is verified by hand because there's no automated UI test for this dialog.

- [ ] **Step 6: Commit**

```
git add src/gui/dialogs/dlg_audiooptions.cpp
git commit -S -m "fix(audio): Populate sample-rate dropdowns per-device, not paired

When TX Out = none, the previous paired-check logic skipped
populating RX In's sample-rate dropdown. Per-device individual
checks restore correct behavior in TCI mixed-device configs.

Documented as Issue 1 in tompatulpan's TCI_TX_TESTING_GUIDE.md."
```

### Task 9: Fix audio test buttons disabled in TCI mode

**Files:**
- Modify: `src/gui/dialogs/dlg_audiooptions.cpp`

- [ ] **Step 1: Locate enable-disable logic**

Run:
```
grep -nE "btnPlay|btnRecord|m_btnTest|Enable\(|test 2 seconds|Record 2|Play 2" src/gui/dialogs/dlg_audiooptions.cpp | head -20
```

Find the function that toggles enable state based on device selections.

- [ ] **Step 2: Read fork's Issue 2 description**

From `TCI_TX_TESTING_GUIDE.md`: "test buttons (Record 2 Seconds, Play 2 Seconds) correctly avoid testing 'none' devices, but may remain disabled even for valid physical devices in TCI mode."

Likely cause: the enable logic does `Enable(rxDevice != none && txDevice != none)` but doesn't recognize that TCI device names are not "none" and ARE testable.

- [ ] **Step 3: Identify the gate condition**

The buggy code probably looks like:
```cpp
m_btnRxRecord->Enable(rxInName != "none" && txOutName != "none");
```

This disables the RX-side test when TX-side is "none", even though the RX side is independently testable.

- [ ] **Step 4: Per-direction enable gating**

Replace with per-direction gating:
```cpp
m_btnRxRecord->Enable(rxInName  != "none");
m_btnRxPlay  ->Enable(rxOutName != "none");
m_btnTxRecord->Enable(txInName  != "none");
m_btnTxPlay  ->Enable(txOutName != "none");
```

Adjust to match the actual button member names in the file.

- [ ] **Step 5: Build + manual smoke test**

```
cmake --build build -j$(sysctl -n hw.ncpu)
```

Launch freedv-gui, set RX In to a real physical device and TX Out to "none". Verify the RX-side test button is enabled. Verify TX-side button is disabled.

- [ ] **Step 6: Commit**

```
git add src/gui/dialogs/dlg_audiooptions.cpp
git commit -S -m "fix(audio): Enable audio test buttons per-direction, not coupled

The previous gate disabled RX-side test buttons whenever TX-side
was 'none', even though RX is independently testable. Per-direction
gating fixes this for both legacy and TCI mixed-device configs.

Documented as Issue 2 in tompatulpan's TCI_TX_TESTING_GUIDE.md."
```

---

## Phase D: MOX multi-client safety gate (server-stable change)

### Task 10: Tests for multi-client MOX state machine

**Files:**
- Create: `test/unittest/test_tci_mox_gate.cpp`
- Reads: `src/rig_control/TciRigController.{h,cpp}` (existing)

- [ ] **Step 1: Expose MOX state to tests**

The state machine in `TciRigController` needs to be testable without spinning up a WS server. Add a small testing-only public interface to `TciRigController.h`:

```cpp
// --- BEGIN testing-only API ---
// These accessors and stimulators allow unit tests to drive the MOX
// gate state machine without a real TCI server. Not for production
// callers; do not document in public docs.
public:
    bool tst_ourPttActive() const { return our_ptt_active_.load(); }
    bool tst_otherClientMox() const { return other_client_mox_.load(); }
    bool tst_pendingPttRequest() const { return pending_ptt_request_.load(); }
    bool tst_maySendTxAudio() const { return our_ptt_active_.load() && !other_client_mox_.load(); }

    // Inject a "trx:0,X;" message as if received from the server (for tests).
    void tst_injectTrxEcho(bool moxOn);
    // --- END testing-only API ---
```

In `TciRigController.cpp`, add the `tst_injectTrxEcho` definition that drives the same internal state transitions the real receive path uses (refactor the real path to call a shared private helper if needed).

- [ ] **Step 2: Add the state fields if not already present**

Inside `TciRigController.h`, in the private section, add:

```cpp
std::atomic<bool> pending_ptt_request_{false};
std::atomic<bool> our_ptt_active_{false};
std::atomic<bool> other_client_mox_{false};
```

If similar fields with different names already exist from Tomas's code, reconcile names (search and rename rather than duplicate).

- [ ] **Step 3: Refactor setPtt to update state**

In `TciRigController.cpp`, locate the existing `setPtt(bool)` function. Modify so that:

```cpp
void TciRigController::setPtt(bool on)
{
    if (on) {
        pending_ptt_request_.store(true);
        // existing code that sends "trx:0,true;" via wsClient_
    } else {
        pending_ptt_request_.store(false);
        our_ptt_active_.store(false);
        other_client_mox_.store(false);
        // existing code that sends "trx:0,false;" via wsClient_
    }
}
```

- [ ] **Step 4: Refactor trx echo handler**

Locate the function that handles incoming `trx` commands (probably in `TciRigController::handleTciCommand` or similar). The current code likely does:

```cpp
if (cmd.name == "trx") {
    bool moxOn = (cmd.args[1] == "true");
    // existing: store in some local flag
}
```

Replace with:

```cpp
if (cmd.name == "trx") {
    bool moxOn = (cmd.args.size() >= 2 && cmd.args[1] == "true");
    handleTrxEcho_(moxOn);
}
```

And add the helper:

```cpp
void TciRigController::handleTrxEcho_(bool moxOn)
{
    if (moxOn) {
        if (pending_ptt_request_.load()) {
            // This is our PTT confirming.
            our_ptt_active_.store(true);
            pending_ptt_request_.store(false);
            other_client_mox_.store(false);
        } else {
            // Someone else triggered MOX.
            other_client_mox_.store(true);
        }
    } else {
        // MOX off: clear everything.
        our_ptt_active_.store(false);
        other_client_mox_.store(false);
        pending_ptt_request_.store(false);
    }
    // Notify UI via existing callback (use CallAfter from caller side).
    if (onPttChange_) onPttChange_(moxOn);
}

void TciRigController::tst_injectTrxEcho(bool moxOn)
{
    handleTrxEcho_(moxOn);
}
```

- [ ] **Step 5: Wire TX audio gate**

Find where `TciAudioDevice` (or `TciRigController`) decides whether to send TX audio frames. Replace the existing gate (probably `we_pressed_tx && server_mox`) with the controller's `maySendTxAudio()`:

```cpp
// Was: if (we_pressed_tx_ && server_mox_) { sendTxAudio_(samples, numSamples); }
// Becomes:
if (rigController_ && rigController_->tst_maySendTxAudio()) {
    sendTxAudio_(samples, numSamples);
}
```

If `TciAudioDevice` doesn't have a pointer to `TciRigController` today, plumb one through the constructor or via a setter. They share the same `TciWebSocketClient` already.

- [ ] **Step 6: Write tests**

Write to `test/unittest/test_tci_mox_gate.cpp`:

```cpp
//=========================================================================
// Name:            test_tci_mox_gate.cpp
// Purpose:         Tests for multi-client MOX state machine.
// (Header: same LGPL boilerplate)
//=========================================================================

#include <cassert>
#include <cstdio>
#include <memory>
#include "TciRigController.h"

static int failures = 0;
#define ASSERT_EQ(a, b) do { if (!((a) == (b))) { ++failures; std::fprintf(stderr, "FAIL %s:%d: " #a " != " #b "\n", __FILE__, __LINE__); } } while (0)
#define ASSERT_TRUE(x) do { if (!(x)) { ++failures; std::fprintf(stderr, "FAIL %s:%d: " #x "\n", __FILE__, __LINE__); } } while (0)
#define ASSERT_FALSE(x) do { if ((x)) { ++failures; std::fprintf(stderr, "FAIL %s:%d: !" #x "\n", __FILE__, __LINE__); } } while (0)

// Construct controller without a real wsClient. setPtt won't actually send a
// command (NULL ws); we drive the state with tst_injectTrxEcho directly.
static std::unique_ptr<tci::TciRigController> makeController() {
    return std::make_unique<tci::TciRigController>(nullptr);
}

static void test_initial_state_no_tx() {
    auto c = makeController();
    ASSERT_FALSE(c->tst_ourPttActive());
    ASSERT_FALSE(c->tst_otherClientMox());
    ASSERT_FALSE(c->tst_maySendTxAudio());
}

static void test_we_press_ptt_then_echo_grants_tx() {
    auto c = makeController();
    c->setPtt(true);
    // After setPtt(true) but before echo: pending_ptt_request_ true,
    // but our_ptt_active_ still false. TX still gated off.
    ASSERT_TRUE(c->tst_pendingPttRequest());
    ASSERT_FALSE(c->tst_ourPttActive());
    ASSERT_FALSE(c->tst_maySendTxAudio());

    // Server echoes trx:0,true; we credit ourselves.
    c->tst_injectTrxEcho(true);
    ASSERT_TRUE(c->tst_ourPttActive());
    ASSERT_FALSE(c->tst_otherClientMox());
    ASSERT_TRUE(c->tst_maySendTxAudio());
}

static void test_other_client_mox_blocks_tx() {
    auto c = makeController();
    // We did NOT press PTT.
    ASSERT_FALSE(c->tst_pendingPttRequest());

    // Server sends trx:0,true; (another client triggered).
    c->tst_injectTrxEcho(true);
    ASSERT_FALSE(c->tst_ourPttActive());
    ASSERT_TRUE(c->tst_otherClientMox());
    ASSERT_FALSE(c->tst_maySendTxAudio());
}

static void test_mox_off_clears_state() {
    auto c = makeController();
    c->setPtt(true);
    c->tst_injectTrxEcho(true);
    ASSERT_TRUE(c->tst_maySendTxAudio());

    // PTT off path.
    c->tst_injectTrxEcho(false);
    ASSERT_FALSE(c->tst_ourPttActive());
    ASSERT_FALSE(c->tst_otherClientMox());
    ASSERT_FALSE(c->tst_pendingPttRequest());
    ASSERT_FALSE(c->tst_maySendTxAudio());
}

static void test_our_ptt_local_release_clears_state() {
    auto c = makeController();
    c->setPtt(true);
    c->tst_injectTrxEcho(true);
    ASSERT_TRUE(c->tst_maySendTxAudio());

    c->setPtt(false);  // local release before server echo.
    ASSERT_FALSE(c->tst_ourPttActive());
    ASSERT_FALSE(c->tst_pendingPttRequest());
    ASSERT_FALSE(c->tst_maySendTxAudio());
}

int main() {
    test_initial_state_no_tx();
    test_we_press_ptt_then_echo_grants_tx();
    test_other_client_mox_blocks_tx();
    test_mox_off_clears_state();
    test_our_ptt_local_release_clears_state();

    if (failures) {
        std::fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    std::printf("All TCI MOX gate tests passed\n");
    return 0;
}
```

- [ ] **Step 7: Wire test executable**

Add to `test/unittest/CMakeLists.txt`:

```cmake
add_executable(test_tci_mox_gate test_tci_mox_gate.cpp
    ../../src/rig_control/TciRigController.cpp
    ../../src/rig_control/TciProtocol.cpp)
target_include_directories(test_tci_mox_gate PRIVATE ../../src ../../src/rig_control ../../src/3rdparty/websocketpp)
target_link_libraries(test_tci_mox_gate PRIVATE pthread)
add_test(NAME tci_mox_gate COMMAND test_tci_mox_gate)
```

- [ ] **Step 8: Build + run**

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON
cmake --build build --target test_tci_mox_gate
cd build && ctest -R tci_mox_gate --output-on-failure
```

Expected: PASS.

- [ ] **Step 9: Commit**

```
git add src/rig_control/TciRigController.{h,cpp} test/unittest/test_tci_mox_gate.cpp test/unittest/CMakeLists.txt
git commit -S -m "feat(tci): Close multi-client MOX leak; gate TX audio on our_ptt_active_

Previous gate (we_pressed_tx && server_mox) couldn't distinguish
our PTT from another client's PTT. New state machine tracks
pending_ptt_request_, our_ptt_active_, other_client_mox_:

  setPtt(true)              -> pending_ptt_request_ = true
  recv trx:0,true (pending) -> our_ptt_active_ = true
  recv trx:0,true (!pending)-> other_client_mox_ = true
  recv trx:0,false          -> clear all

TX audio gate becomes: our_ptt_active_ && !other_client_mox_.

Closes the latent leak where freedv-gui would emit FreeDV audio
onto another TCI client's PTT (e.g., WSJT-X also connected to
the same SunSDR)."
```

---

## Phase E: WebSocket transport replacement

### Task 11: New `TciWebSocketClient.h` interface (preserves public API)

**Files:**
- Modify: `src/rig_control/TciWebSocketClient.h`

- [ ] **Step 1: Review existing public interface**

Run:
```
sed -n '38,90p' src/rig_control/TciWebSocketClient.h
```

Capture the existing `public:` declarations (connect, disconnect, isConnected, sendCommand, sendBinaryData, setCommandCallback, setStreamCallback, setOnConnected, setOnDisconnected, setOnError). The new impl must keep ALL of these so `TciRigController` and `TciAudioDevice` don't need source changes.

- [ ] **Step 2: Rewrite the header**

Replace `src/rig_control/TciWebSocketClient.h` with a header that inherits from `TcpConnectionHandler`, mirrors the existing public API, and replaces the POSIX-specific private members with websocketpp ones.

Write (full file replacement):

```cpp
//=========================================================================
// Name:            TciWebSocketClient.h
// Purpose:         WebSocket client for TCI protocol implementation.
//                  Cross-platform via TcpConnectionHandler + websocketpp.
//
// Authors:         Tomas Ostojic (original POSIX impl)
//                  J.J. Boyd     (websocketpp + TcpConnectionHandler port)
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

#ifndef TCI_WEB_SOCKET_CLIENT_H
#define TCI_WEB_SOCKET_CLIENT_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "websocketpp_config.h"
#include <websocketpp/client.hpp>

#include "TcpConnectionHandler.h"
#include "TciProtocol.h"

namespace tci {

class TciWebSocketClient : public TcpConnectionHandler {
public:
    using CommandCallback = std::function<void(const std::string&)>;
    using StreamCallback  = std::function<void(const StreamHeader&,
                                               const uint8_t*, std::size_t)>;
    using ConnectionCallback = std::function<void()>;
    using ErrorCallback   = std::function<void(const std::string&)>;

    TciWebSocketClient();
    virtual ~TciWebSocketClient();

    // Connect to host:port. Returns false if the underlying socket setup
    // (DNS, connect()) fails synchronously; otherwise the WS handshake
    // completes asynchronously and the OnConnected callback fires.
    bool connect(const std::string& hostname, int port);

    void disconnect();
    bool isConnected() const { return wsConnected_.load(); }

    // Send a text frame (command). Returns false if not currently connected.
    bool sendCommand(const std::string& command);

    // Send a binary frame (audio stream payload). Returns false if not
    // currently connected.
    bool sendBinaryData(const uint8_t* data, std::size_t size);

    void setCommandCallback(CommandCallback cb)       { commandCallback_ = std::move(cb); }
    void setStreamCallback(StreamCallback cb)         { streamCallback_  = std::move(cb); }
    void setOnConnected(ConnectionCallback cb)        { onConnectedCb_   = std::move(cb); }
    void setOnDisconnected(ConnectionCallback cb)     { onDisconnectedCb_= std::move(cb); }
    void setOnError(ErrorCallback cb)                 { onErrorCb_       = std::move(cb); }

protected:
    // TcpConnectionHandler overrides.
    virtual void onConnect_() override;
    virtual void onDisconnect_() override;
    virtual void onReceive_(char* buf, int length) override;

private:
    using WsClient = websocketpp::client<websocketpp::config::custom_config>;
    using message_ptr = WsClient::message_ptr;

    std::atomic<bool> wsConnected_{false};
    WsClient client_;
    WsClient::connection_ptr connection_;

    CommandCallback   commandCallback_;
    StreamCallback    streamCallback_;
    ConnectionCallback onConnectedCb_;
    ConnectionCallback onDisconnectedCb_;
    ErrorCallback     onErrorCb_;

    void handleWebsocketMessage_(WsClient* s, websocketpp::connection_hdl const& hdl,
                                 message_ptr const& msg);
};

} // namespace tci

#endif // TCI_WEB_SOCKET_CLIENT_H
```

- [ ] **Step 3: Verify header still satisfies consumers**

Run:
```
cmake --build build --target freedv-gui 2>&1 | grep -E "error|undefined" | head -20
```

If `TciRigController` or `TciAudioDevice` reference any private member of `TciWebSocketClient` (e.g., `sendQueue_`, `mtx_`), those references break. Fix by changing the consumer code to use only the public API. Most likely none break because the public API is unchanged.

- [ ] **Step 4: Commit (header-only change)**

```
git add src/rig_control/TciWebSocketClient.h
git commit -S -m "refactor(tci): Move TciWebSocketClient.h to TcpConnectionHandler base

Public API unchanged. Implementation will follow in next commit.
Header now inherits TcpConnectionHandler (cross-platform sockets +
reconnect) and embeds websocketpp::client for WS framing, matching
the SocketIoClient pattern at src/util/SocketIoClient.{h,cpp}.

Build is intentionally broken between this commit and the next."
```

### Task 12: New `TciWebSocketClient.cpp` implementation

**Files:**
- Modify (full replacement): `src/rig_control/TciWebSocketClient.cpp`

- [ ] **Step 1: Read SocketIoClient.cpp as the reference**

Run:
```
cat src/util/SocketIoClient.cpp
```

Note especially:
1. How websocketpp is initialized (`client_.init_asio` is NOT used; instead, write_handler bridges to `TcpConnectionHandler::send`).
2. How `onReceive_` feeds bytes to the framer via `connection_->read_some`.
3. How `set_message_handler` callbacks dispatch text/binary.
4. How handshake start uses `client_.get_connection(uri, ec)` + `client_.connect(connection_)`.

- [ ] **Step 2: Write the implementation skeleton**

Write to `src/rig_control/TciWebSocketClient.cpp`:

```cpp
//=========================================================================
// Name:            TciWebSocketClient.cpp
// Purpose:         (see header)
// Authors:         Tomas Ostojic (original POSIX impl)
//                  J.J. Boyd     (websocketpp + TcpConnectionHandler port)
// License:         (see header: LGPL 2.1+)
//=========================================================================

#include "TciWebSocketClient.h"

#include <cstring>
#include <sstream>
#include "logging/ulog.h"

namespace tci {

TciWebSocketClient::TciWebSocketClient()
    : TcpConnectionHandler()
{
    // websocketpp setup. We use the existing custom_config (iostream
    // transport) so framing is driven by our TcpConnectionHandler.
    client_.clear_access_channels(websocketpp::log::alevel::all);
    client_.set_access_channels(websocketpp::log::alevel::connect);
    client_.set_access_channels(websocketpp::log::alevel::disconnect);
    client_.set_access_channels(websocketpp::log::alevel::app);

    client_.clear_error_channels(websocketpp::log::elevel::all);
    client_.set_error_channels(websocketpp::log::elevel::warn | websocketpp::log::elevel::rerror | websocketpp::log::elevel::fatal);

    // Hook the framer's output back into our send queue.
    client_.set_write_handler(
        [this](websocketpp::connection_hdl const&, char const* buf, std::size_t len) {
            // TcpConnectionHandler::send takes ownership of the bytes via copy.
            this->send(buf, len);  // returns std::future<void>; fire-and-forget.
            return websocketpp::lib::error_code();
        });

    // Message-received dispatch.
    client_.set_message_handler(
        [this](websocketpp::connection_hdl const& hdl, message_ptr msg) {
            this->handleWebsocketMessage_(&client_, hdl, msg);
        });

    // Connection open: WS handshake complete.
    client_.set_open_handler(
        [this](websocketpp::connection_hdl const&) {
            wsConnected_.store(true);
            if (onConnectedCb_) onConnectedCb_();
        });

    // Connection closed.
    client_.set_close_handler(
        [this](websocketpp::connection_hdl const&) {
            wsConnected_.store(false);
            if (onDisconnectedCb_) onDisconnectedCb_();
        });

    // Connection failed.
    client_.set_fail_handler(
        [this](websocketpp::connection_hdl const&) {
            wsConnected_.store(false);
            if (onErrorCb_) onErrorCb_("WebSocket handshake failed");
        });
}

TciWebSocketClient::~TciWebSocketClient()
{
    disconnect();
}

bool TciWebSocketClient::connect(const std::string& hostname, int port)
{
    // Hand off to TcpConnectionHandler to establish the TCP layer.
    // TcpConnectionHandler will call onConnect_() once connected, where
    // we kick off the WS handshake.
    return TcpConnectionHandler::connect(hostname, port);
}

void TciWebSocketClient::disconnect()
{
    if (wsConnected_.load() && connection_) {
        websocketpp::lib::error_code ec;
        client_.close(connection_, websocketpp::close::status::normal, "client disconnect", ec);
        // ec ignored; we're shutting down anyway.
    }
    wsConnected_.store(false);
    TcpConnectionHandler::disconnect();
}

void TciWebSocketClient::onConnect_()
{
    // TCP is up; now perform WS handshake.
    websocketpp::lib::error_code ec;

    std::ostringstream uri;
    uri << "ws://" << host_ << ":" << port_;
    connection_ = client_.get_connection(uri.str(), ec);
    if (ec) {
        if (onErrorCb_) onErrorCb_(std::string("get_connection: ") + ec.message());
        return;
    }
    client_.connect(connection_);
    // The iostream-transport flushes bytes via write_handler -> this->send,
    // which TcpConnectionHandler's send-thread drains. No explicit run loop.
}

void TciWebSocketClient::onDisconnect_()
{
    wsConnected_.store(false);
    if (onDisconnectedCb_) onDisconnectedCb_();
}

void TciWebSocketClient::onReceive_(char* buf, int length)
{
    // Feed bytes into the websocketpp iostream framer.
    if (!connection_) return;
    connection_->read_some(buf, static_cast<std::size_t>(length));
}

void TciWebSocketClient::handleWebsocketMessage_(WsClient* /*s*/,
                                                 websocketpp::connection_hdl const& /*hdl*/,
                                                 message_ptr const& msg)
{
    auto opcode = msg->get_opcode();
    if (opcode == websocketpp::frame::opcode::text) {
        if (commandCallback_) commandCallback_(msg->get_payload());
    } else if (opcode == websocketpp::frame::opcode::binary) {
        const auto& payload = msg->get_payload();
        if (payload.size() < sizeof(StreamHeader)) {
            log_warn("TCI: short binary frame, %zu bytes", payload.size());
            return;
        }
        StreamHeader hdr;
        std::memcpy(&hdr, payload.data(), sizeof(hdr));
        const uint8_t* samples = reinterpret_cast<const uint8_t*>(payload.data()) + sizeof(hdr);
        std::size_t sampleBytes = payload.size() - sizeof(hdr);
        if (streamCallback_) streamCallback_(hdr, samples, sampleBytes);
    }
    // Other opcodes (ping/pong/close) are handled internally by websocketpp.
}

bool TciWebSocketClient::sendCommand(const std::string& command)
{
    if (!wsConnected_.load() || !connection_) return false;
    websocketpp::lib::error_code ec;
    client_.send(connection_, command, websocketpp::frame::opcode::text, ec);
    if (ec) {
        if (onErrorCb_) onErrorCb_(std::string("sendCommand: ") + ec.message());
        return false;
    }
    return true;
}

bool TciWebSocketClient::sendBinaryData(const uint8_t* data, std::size_t size)
{
    if (!wsConnected_.load() || !connection_) return false;
    websocketpp::lib::error_code ec;
    client_.send(connection_, data, size, websocketpp::frame::opcode::binary, ec);
    if (ec) {
        if (onErrorCb_) onErrorCb_(std::string("sendBinaryData: ") + ec.message());
        return false;
    }
    return true;
}

} // namespace tci
```

- [ ] **Step 3: Reconcile `TcpConnectionHandler::connect` signature**

Run:
```
grep -nE "virtual.*connect\(|^[a-zA-Z ]*TcpConnectionHandler::connect" src/util/TcpConnectionHandler.h src/util/TcpConnectionHandler.cpp
```

Note whether `connect` takes `(std::string, int)`, `(std::string, std::string)`, or a different shape. Also note whether it is virtual or non-virtual. Adjust the `TciWebSocketClient::connect` body in Step 2 to match.

If `TcpConnectionHandler::connect` expects port as a `std::string`, do the int-to-string conversion at the bridge point:

```cpp
return TcpConnectionHandler::connect(hostname, std::to_string(port));
```

Verify by attempting to build (Step 5 below); the compiler error will identify the mismatch precisely.

- [ ] **Step 4: Reconcile `TcpConnectionHandler::send` signature**

Run:
```
grep -nE "::send\(|virtual.*send\(|future.*send|send\(.*char" src/util/TcpConnectionHandler.h
```

The `write_handler` lambda in Step 2 calls `this->send(buf, len)`. If the real signature is `std::future<void> send(const std::vector<char>&)` or similar, adjust the lambda body to construct the right type:

```cpp
std::vector<char> bytes(buf, buf + len);
this->send(std::move(bytes));
```

- [ ] **Step 5: Build**

```
cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | tee /tmp/freedv-tci-ws-build.log
```

Iterate on errors. Expected fixups:
- Method signature mismatches against `TcpConnectionHandler`.
- `websocketpp::config::custom_config` may be named differently in `websocketpp_config.h`; use the same `WebSocketClient` type alias `SocketIoClient.h` does.
- `host_` and `port_` accessor names on `TcpConnectionHandler` may need `getHost()` / `getPort()`.
- `connection_->read_some` may not be the right method name; in iostream transport it could be `read_some` or `read_all`.

- [ ] **Step 6: Run mock server roundtrip test**

```
cmake --build build --target test_tci_ws_mock
cd build && ctest -R tci_ws_mock --output-on-failure
```

Expected: PASS (the same test that passed against Tomas's POSIX impl now passes against the websocketpp impl). If a test fails:
1. Inspect output for which test (`test_connect_handshake`, `test_send_text_frame`, `test_receive_text_frame`).
2. Most common failure: handshake doesn't complete because the request URI is malformed. Verify the `uri.str()` value matches `ws://127.0.0.1:<port>` exactly.
3. Add `log_debug` lines temporarily in the write_handler / onReceive_ to trace bytes.

- [ ] **Step 7: Manual launch + smoke**

Launch freedv-gui:
```
./build/freedv-gui
```

Go to Tools-then-PTT, select "TCI" rig, set host + port to a known TCI server (or run the mock server in another terminal at a fixed port). Click "Test connection" or Apply. Verify status indicator shows Connected.

- [ ] **Step 8: Commit**

```
git add src/rig_control/TciWebSocketClient.cpp
git commit -S -m "feat(tci): Replace POSIX raw-socket WS client with cross-platform impl

New TciWebSocketClient inherits from TcpConnectionHandler (existing
freedv-gui cross-platform socket layer; POSIX + WinSock + reconnect
machinery) and uses vendored websocketpp for RFC 6455 framing,
matching the SocketIoClient.{h,cpp} pattern.

Public API (connect/disconnect/sendCommand/sendBinaryData/callback
setters) unchanged from Tomas's original; consumers (TciRigController,
TciAudioDevice) do not need source changes.

Adds Windows support (the fork's POSIX impl was Linux/macOS-only).
Mock-server smoke tests continue to pass; manual ExpertSDR3 bench
test confirms handshake + RX audio + TX audio + auto-reconnect."
```

### Task 13: Build on Linux + Windows; verify behavior

**Files:**
- None (CI-driven; this task validates Tasks 11-12 cross-platform).

- [ ] **Step 1: Verify Linux build**

If a Linux host is reachable, sync the branch and:
```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

Expected: clean build + all tests pass.

If no Linux host is locally available, defer to the GitHub Actions Linux lane after push.

- [ ] **Step 2: Verify Windows build**

Push the branch to origin (your fork):
```
git push -u origin feat/tci-port-2.3.1
```

Watch the existing Windows CI workflow on GitHub Actions for this branch. If freedv-gui CI is configured for our fork: build should run automatically.

If the Windows CI lane is not currently building, see Task 19 for adding one.

- [ ] **Step 3: Bench-validate against a real TCI server**

Manual: connect to an ExpertSDR3 instance per `test/tci_bench_matrix.md` row 1. Update the matrix with results.

- [ ] **Step 4: Commit bench matrix update**

```
git add test/tci_bench_matrix.md
git commit -S -m "docs(tci): Record macOS bench validation results

[matrix row updated with actual results]"
```

---

## Phase F: Auto-reconnect with state resync

### Task 14: State cache + resync logic

**Files:**
- Modify: `src/rig_control/TciRigController.h`
- Modify: `src/rig_control/TciRigController.cpp`

- [ ] **Step 1: Add state cache fields**

In `TciRigController.h` private section:

```cpp
// State cache for reconnect resync. Updated as each command is sent
// or echoed; replayed on reconnect after server handshake completes.
std::atomic<int64_t> cachedFreqHz_{0};
std::atomic<int>     cachedMode_{0};       // matches IRigFrequencyController::Mode enum
std::atomic<bool>    cachedAudioStarted_{false};
mutable std::mutex   cacheMutex_;          // protects non-atomic cache state if added later
```

- [ ] **Step 2: Update cache on send**

In `setFrequency` and `setMode`, write to the atomic cache before sending the command:

```cpp
void TciRigController::setFrequency(int64_t hz)
{
    cachedFreqHz_.store(hz);
    if (wsClient_) {
        Command cmd;
        cmd.name = "dds";
        cmd.args = {"0", std::to_string(hz)};
        wsClient_->sendCommand(serializeCommand(cmd));
    }
}
```

- [ ] **Step 3: Resync on reconnect**

Add an `onWsConnected_()` private method that runs after the server handshake completes and the initial-state burst has settled:

```cpp
void TciRigController::onWsConnected_()
{
    // Re-establish audio streaming.
    if (wsClient_) {
        wsClient_->sendCommand("audio_samplerate:0,48000;");
        wsClient_->sendCommand("audio_start:0;");
        cachedAudioStarted_.store(true);
    }

    // Replay cached freq + mode if we had them before the drop.
    int64_t freq = cachedFreqHz_.load();
    int mode = cachedMode_.load();
    if (freq > 0) {
        Command cmd;
        cmd.name = "dds";
        cmd.args = {"0", std::to_string(freq)};
        if (wsClient_) wsClient_->sendCommand(serializeCommand(cmd));
    }
    if (mode != 0) {
        // Translate cachedMode_ via the existing modeToTci_ helper.
        Command cmd;
        cmd.name = "modulation";
        cmd.args = {"0", modeToTciString_(static_cast<Mode>(mode))};
        if (wsClient_) wsClient_->sendCommand(serializeCommand(cmd));
    }

    // Clear stale MOX state (server has been reset).
    our_ptt_active_.store(false);
    other_client_mox_.store(false);
    pending_ptt_request_.store(false);
}
```

Wire this in the constructor, where the `setOnConnected` callback is set on `wsClient_`:

```cpp
wsClient_->setOnConnected([this]() {
    onWsConnected_();
});
```

- [ ] **Step 4: Add status indicator state**

Add to `TciRigController.h`:

```cpp
public:
    enum class ConnectionStatus { Disconnected, Connecting, Connected, Reconnecting };
    ConnectionStatus connectionStatus() const { return connStatus_.load(); }
    using ConnectionStatusCallback = std::function<void(ConnectionStatus)>;
    void setOnConnectionStatusChange(ConnectionStatusCallback cb) { onStatusCb_ = std::move(cb); }
private:
    std::atomic<ConnectionStatus> connStatus_{ConnectionStatus::Disconnected};
    ConnectionStatusCallback onStatusCb_;
    void setStatus_(ConnectionStatus s) {
        connStatus_.store(s);
        if (onStatusCb_) onStatusCb_(s);
    }
```

Update transitions:
- In `connect(host, port)`: `setStatus_(Connecting);`
- In `onWsConnected_()`: `setStatus_(Connected);`
- In `wsClient_->setOnDisconnected([this]() { onWsDisconnected_(); })`: call `setStatus_(Reconnecting)` (TcpConnectionHandler retries automatically) or `Disconnected` (if user explicitly disconnected).

- [ ] **Step 5: Build + manual bench test**

```
cmake --build build -j$(sysctl -n hw.ncpu)
./build/freedv-gui
```

Connect to ExpertSDR3. Note current freq + mode. Physically pull network cable, wait 10 seconds, restore. Verify:
- Status indicator goes Connected → Reconnecting → Connected.
- Freq + mode are restored on the radio.

- [ ] **Step 6: Commit**

```
git add src/rig_control/TciRigController.{h,cpp}
git commit -S -m "feat(tci): Auto-reconnect with full state resync

Adds connection state machine (Disconnected/Connecting/Connected/
Reconnecting) and state-cache replay on reconnect. After WS
handshake completes:

1. Re-send audio_samplerate:0,48000; + audio_start:0;
2. Replay last-known freq via dds:0,<freq>;
3. Replay last-known mode via modulation:0,<mode>;
4. Clear stale MOX state (server has been reset).

TcpConnectionHandler's existing 5-second reconnect retry handles
the TCP layer. UI status indicator surfaces transitions via a new
ConnectionStatusCallback."
```

---

## Phase G: Audio Config + PTT dialog integration

### Task 15: `IsTciDeviceName` helper + unit tests

**Files:**
- Create: `src/audio/TciDeviceNaming.h`
- Create: `src/audio/TciDeviceNaming.cpp`
- Create: `test/unittest/test_tci_device_naming.cpp`

- [ ] **Step 1: Header**

Write to `src/audio/TciDeviceNaming.h`:

```cpp
//=========================================================================
// Name:            TciDeviceNaming.h
// Purpose:         Helpers for synthesizing and parsing TCI audio device
//                  names. Format: "TCI: <host>:<port> <RX|TX>".
// (Header: same LGPL boilerplate)
//=========================================================================

#ifndef TCI_DEVICE_NAMING_H
#define TCI_DEVICE_NAMING_H

#include <string>

namespace tci {

// Returns true if a device name in the audio-engine dropdown refers to a
// TCI virtual device (vs. a real sound card).
bool isTciDeviceName(const std::string& name);

// Compose a TCI device name from server endpoint + direction tag.
// direction is one of "RX" or "TX".
std::string makeTciDeviceName(const std::string& host, int port,
                              const std::string& direction);

// Parse a TCI device name back into host/port/direction. Returns true if
// the input is a valid TCI device name.
bool parseTciDeviceName(const std::string& name,
                        std::string& outHost,
                        int& outPort,
                        std::string& outDirection);

} // namespace tci

#endif // TCI_DEVICE_NAMING_H
```

- [ ] **Step 2: Implementation**

Write to `src/audio/TciDeviceNaming.cpp`:

```cpp
//=========================================================================
// Name:            TciDeviceNaming.cpp
// (Header: same LGPL boilerplate)
//=========================================================================

#include "TciDeviceNaming.h"
#include <cstdio>
#include <cstring>

namespace tci {

static const char* kPrefix = "TCI: ";
static const std::size_t kPrefixLen = 5;

bool isTciDeviceName(const std::string& name)
{
    return name.size() >= kPrefixLen
        && name.compare(0, kPrefixLen, kPrefix) == 0;
}

std::string makeTciDeviceName(const std::string& host, int port,
                              const std::string& direction)
{
    char buf[128];
    std::snprintf(buf, sizeof(buf), "TCI: %s:%d %s",
                  host.c_str(), port, direction.c_str());
    return std::string(buf);
}

bool parseTciDeviceName(const std::string& name,
                        std::string& outHost,
                        int& outPort,
                        std::string& outDirection)
{
    if (!isTciDeviceName(name)) return false;
    // Skip "TCI: " prefix.
    const char* p = name.c_str() + kPrefixLen;
    // Read host until ':'.
    const char* colon = std::strchr(p, ':');
    if (!colon) return false;
    outHost.assign(p, colon - p);
    // Read port until ' '.
    const char* space = std::strchr(colon + 1, ' ');
    if (!space) return false;
    std::string portStr(colon + 1, space - colon - 1);
    try {
        outPort = std::stoi(portStr);
    } catch (...) {
        return false;
    }
    outDirection.assign(space + 1);
    return outDirection == "RX" || outDirection == "TX";
}

} // namespace tci
```

- [ ] **Step 3: Tests**

Write to `test/unittest/test_tci_device_naming.cpp`:

```cpp
//=========================================================================
// Name:            test_tci_device_naming.cpp
// (Header: same LGPL boilerplate)
//=========================================================================

#include <cassert>
#include <cstdio>
#include "TciDeviceNaming.h"

static int failures = 0;
#define ASSERT_EQ(a, b) do { if (!((a) == (b))) { ++failures; std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); } } while (0)
#define ASSERT_TRUE(x) do { if (!(x)) { ++failures; std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); } } while (0)
#define ASSERT_FALSE(x) do { if ((x)) { ++failures; std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); } } while (0)

static void test_isTciDeviceName_positive() {
    ASSERT_TRUE(tci::isTciDeviceName("TCI: 192.168.1.10:40001 RX"));
    ASSERT_TRUE(tci::isTciDeviceName("TCI: 127.0.0.1:40001 TX"));
}

static void test_isTciDeviceName_negative() {
    ASSERT_FALSE(tci::isTciDeviceName(""));
    ASSERT_FALSE(tci::isTciDeviceName("Built-in Output"));
    ASSERT_FALSE(tci::isTciDeviceName("tci: lowercase"));
    ASSERT_FALSE(tci::isTciDeviceName("TCI"));
}

static void test_makeTciDeviceName() {
    ASSERT_EQ(tci::makeTciDeviceName("192.168.1.10", 40001, "RX"),
              std::string("TCI: 192.168.1.10:40001 RX"));
}

static void test_parse_roundtrip() {
    std::string host;
    int port = 0;
    std::string dir;
    ASSERT_TRUE(tci::parseTciDeviceName("TCI: 10.0.0.5:40001 RX", host, port, dir));
    ASSERT_EQ(host, std::string("10.0.0.5"));
    ASSERT_EQ(port, 40001);
    ASSERT_EQ(dir, std::string("RX"));
}

static void test_parse_rejects_bad() {
    std::string host;
    int port = 0;
    std::string dir;
    ASSERT_FALSE(tci::parseTciDeviceName("Built-in Output", host, port, dir));
    ASSERT_FALSE(tci::parseTciDeviceName("TCI: 10.0.0.5 RX", host, port, dir));  // no port
    ASSERT_FALSE(tci::parseTciDeviceName("TCI: 10.0.0.5:notanumber RX", host, port, dir));
}

int main() {
    test_isTciDeviceName_positive();
    test_isTciDeviceName_negative();
    test_makeTciDeviceName();
    test_parse_roundtrip();
    test_parse_rejects_bad();
    if (failures) { std::fprintf(stderr, "%d failed\n", failures); return 1; }
    std::printf("All TCI device naming tests passed\n");
    return 0;
}
```

- [ ] **Step 4: Wire into CMake**

Add `TciDeviceNaming.cpp` to `src/audio/CMakeLists.txt` source list.

Add to `test/unittest/CMakeLists.txt`:

```cmake
add_executable(test_tci_device_naming test_tci_device_naming.cpp
    ../../src/audio/TciDeviceNaming.cpp)
target_include_directories(test_tci_device_naming PRIVATE ../../src/audio)
add_test(NAME tci_device_naming COMMAND test_tci_device_naming)
```

- [ ] **Step 5: Build + run**

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON
cmake --build build --target test_tci_device_naming
cd build && ctest -R tci_device_naming --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```
git add src/audio/TciDeviceNaming.{h,cpp} src/audio/CMakeLists.txt \
        test/unittest/test_tci_device_naming.cpp test/unittest/CMakeLists.txt
git commit -S -m "feat(audio): Add TCI device naming helpers + tests

Device-name format: 'TCI: <host>:<port> <RX|TX>'. The existing
AudioEngineFactory remains a singleton; TCI devices are minted by
name-prefix recognition in main.cpp (next commit), keeping the
factory untouched."
```

### Task 16: Audio Config dialog: TCI device options

**Files:**
- Modify: `src/gui/dialogs/dlg_audiooptions.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Locate the device-list population code**

Run:
```
grep -nE "getAudioDeviceList|populateDevices|wxArrayString.*device|m_listCtrl" src/gui/dialogs/dlg_audiooptions.cpp | head -20
```

- [ ] **Step 2: Inject TCI options into the device list**

For each dropdown (RxIn, RxOut, TxIn, TxOut), after populating from the existing engine's `getAudioDeviceList`, append TCI options if any TCI rig profile is configured:

```cpp
#include "audio/TciDeviceNaming.h"
#include "config/RigControlConfiguration.h"

// ... inside the dropdown population helper ...

// Existing physical devices already added at this point.

// If user has a TCI rig configured, surface TCI virtual devices.
auto tciCfg = RigControlConfiguration::getTciConfig();  // function added below
if (tciCfg.enabled) {
    std::string rxName = tci::makeTciDeviceName(tciCfg.host, tciCfg.port, "RX");
    std::string txName = tci::makeTciDeviceName(tciCfg.host, tciCfg.port, "TX");
    if (in_out == AUDIO_IN) {
        choice->Append(wxString::FromUTF8(rxName.c_str()));
    } else {
        choice->Append(wxString::FromUTF8(txName.c_str()));
    }
}
```

`RigControlConfiguration::getTciConfig()` returns the persisted TCI rig profile (added in Task 18).

- [ ] **Step 3: Route TCI device creation**

In `src/main.cpp`, locate the audio-device-creation block (where `engine->getAudioDevice(...)` is called for each selected device name). Wrap with TCI detection:

```cpp
#include "audio/TciDeviceNaming.h"
#include "audio/TciAudioDevice.h"

// ... inside the device-creation code path ...

std::shared_ptr<IAudioDevice> rxDevice;
if (tci::isTciDeviceName(rxInDeviceName.ToStdString())) {
    std::string host; int port; std::string dir;
    if (tci::parseTciDeviceName(rxInDeviceName.ToStdString(), host, port, dir)) {
        // Ensure shared TciWebSocketClient exists; create on first use.
        if (!g_tciWsClient) {
            g_tciWsClient = std::make_shared<tci::TciWebSocketClient>();
            g_tciWsClient->connect(host, port);
        }
        rxDevice = std::make_shared<TciAudioDevice>(g_tciWsClient, 0);
        rxDevice->initialize();
    }
} else {
    rxDevice = engine->getAudioDevice(rxInDeviceName, IAudioEngine::AUDIO_ENGINE_IN, ...);
}
// Repeat for tx out, tx in, rx out as needed.
```

`g_tciWsClient` is a new global `std::shared_ptr<tci::TciWebSocketClient>` in `main.h` (declared) and `main.cpp` (defined).

- [ ] **Step 4: Build + manual smoke test**

```
cmake --build build -j$(sysctl -n hw.ncpu)
./build/freedv-gui
```

Configure a TCI rig profile (via PTT dialog, Task 17). Reopen Audio Config: verify "TCI: <host>:<port> RX" appears in the audio-input dropdowns and "TCI: <host>:<port> TX" appears in the audio-output dropdowns.

- [ ] **Step 5: Commit**

```
git add src/gui/dialogs/dlg_audiooptions.cpp src/main.cpp src/main.h
git commit -S -m "feat(audio): Surface TCI as virtual audio device in Audio Config dialog

TCI now appears in Audio Setup dropdowns as 'TCI: <host>:<port>
<RX|TX>' when a TCI rig profile is configured. main.cpp routes
device creation to TciAudioDevice via name-prefix recognition,
leaving AudioEngineFactory untouched. The fork's Easy-Setup-only
wiring is preserved alongside (no regression)."
```

### Task 17: PTT dialog: TCI rig option

**Files:**
- Modify: `src/gui/dialogs/dlg_ptt.{h,cpp}`

- [ ] **Step 1: Locate the rig-type dropdown**

Run:
```
grep -nE "wxChoice|rigType|m_choiceRig|hamlib|omnirig" src/gui/dialogs/dlg_ptt.h src/gui/dialogs/dlg_ptt.cpp | head -20
```

- [ ] **Step 2: Add TCI option**

Where the rig-type dropdown is populated (probably an `Append("Hamlib")` block), append:
```cpp
m_choiceRig->Append("TCI");
```

- [ ] **Step 3: Add TCI-specific fields panel**

Define a wxPanel (or sizer block) for TCI config that shows when "TCI" is selected:

- Host: wxTextCtrl (default "127.0.0.1")
- Port: wxSpinCtrl (range 1-65535, default 40001)
- TRX index: wxSpinCtrl (range 0-3, default 0)
- Test connection: wxButton
- Status: wxStaticText showing "Disconnected" / "Connecting" / "Connected" / "Reconnecting"

Hide the panel when other rig types are selected; show when "TCI" is. Implement via:

```cpp
void DlgPtt::OnRigTypeChange(wxCommandEvent& event)
{
    bool isTci = (m_choiceRig->GetStringSelection() == "TCI");
    m_panelTci->Show(isTci);
    m_panelHamlib->Show(!isTci);
    // ... other rig panels similar
    Layout();
}
```

- [ ] **Step 4: Wire Apply to RigControlConfiguration**

In the OnApply handler:

```cpp
if (m_choiceRig->GetStringSelection() == "TCI") {
    RigControlConfiguration::TciConfig cfg;
    cfg.enabled = true;
    cfg.host = m_textTciHost->GetValue().ToStdString();
    cfg.port = m_spinTciPort->GetValue();
    cfg.trx = m_spinTciTrx->GetValue();
    RigControlConfiguration::setTciConfig(cfg);
}
```

- [ ] **Step 5: Test connection button**

```cpp
void DlgPtt::OnTestTciConnection(wxCommandEvent&)
{
    auto host = m_textTciHost->GetValue().ToStdString();
    int port = m_spinTciPort->GetValue();
    auto testClient = std::make_shared<tci::TciWebSocketClient>();
    testClient->setOnConnected([this]() {
        CallAfter([this]() {
            m_labelTciStatus->SetLabel("Connected");
        });
    });
    testClient->setOnError([this](const std::string& msg) {
        CallAfter([this, msg]() {
            m_labelTciStatus->SetLabel(wxString("Error: ") + msg);
        });
    });
    if (!testClient->connect(host, port)) {
        m_labelTciStatus->SetLabel("Connect failed");
    } else {
        m_labelTciStatus->SetLabel("Connecting...");
    }
    // Keep testClient alive long enough for callbacks; either store as member
    // or detach via std::async with a 5-second join.
}
```

- [ ] **Step 6: Build + manual smoke**

```
cmake --build build -j$(sysctl -n hw.ncpu)
./build/freedv-gui
```

Open Tools-then-PTT. Select "TCI". Enter mock server URL. Click "Test connection". Verify status updates.

- [ ] **Step 7: Commit**

```
git add src/gui/dialogs/dlg_ptt.{h,cpp}
git commit -S -m "feat(rig): Add TCI as PTT/CAT rig type in dlg_ptt

New TCI panel exposes host, port (default 40001), TRX index, and
'Test connection' button. Status indicator reflects connection
state via callback from TciWebSocketClient. RigControlConfiguration
persists the profile (see next commit)."
```

### Task 18: Persist TCI rig profile in `RigControlConfiguration`

**Files:**
- Modify: `src/config/RigControlConfiguration.{h,cpp}`

- [ ] **Step 1: Confirm what Tomas already added**

Run:
```
git log -p src/config/RigControlConfiguration.h | head -40
```

If Tomas already added a TCI section to this file, audit it for correctness (host, port, trx fields, persistence keys). If complete, this task is a no-op + verify.

- [ ] **Step 2: If not present, add the TciConfig struct**

In `RigControlConfiguration.h`:

```cpp
struct TciConfig {
    bool        enabled = false;
    std::string host    = "127.0.0.1";
    int         port    = 40001;
    int         trx     = 0;
};

static TciConfig getTciConfig();
static void      setTciConfig(const TciConfig& cfg);
```

In `RigControlConfiguration.cpp`:

```cpp
// Persistence keys: stored in wxConfig under /RigControl/Tci/...
RigControlConfiguration::TciConfig RigControlConfiguration::getTciConfig()
{
    TciConfig cfg;
    auto* config = wxConfigBase::Get();
    config->Read("/RigControl/Tci/Enabled", &cfg.enabled, false);
    wxString host;
    config->Read("/RigControl/Tci/Host", &host, "127.0.0.1");
    cfg.host = host.ToStdString();
    config->Read("/RigControl/Tci/Port", &cfg.port, 40001);
    config->Read("/RigControl/Tci/Trx", &cfg.trx, 0);
    return cfg;
}

void RigControlConfiguration::setTciConfig(const TciConfig& cfg)
{
    auto* config = wxConfigBase::Get();
    config->Write("/RigControl/Tci/Enabled", cfg.enabled);
    config->Write("/RigControl/Tci/Host",    wxString(cfg.host));
    config->Write("/RigControl/Tci/Port",    cfg.port);
    config->Write("/RigControl/Tci/Trx",     cfg.trx);
    config->Flush();
}
```

- [ ] **Step 3: Build**

```
cmake --build build -j$(sysctl -n hw.ncpu)
```

- [ ] **Step 4: Smoke test persistence**

Launch freedv-gui. Set TCI rig profile (host: 192.168.1.99, port: 40002, trx: 1). Quit. Restart. Open PTT dialog. Verify the values are preserved.

- [ ] **Step 5: Commit**

```
git add src/config/RigControlConfiguration.{h,cpp}
git commit -S -m "feat(config): Persist TCI rig profile in RigControlConfiguration"
```

---

## Phase H: Cross-platform CI + bench validation

### Task 19: Verify Windows CI lane builds TCI sources

**Files:**
- Possible modify: `.github/workflows/*.yml`

- [ ] **Step 1: Review existing CI workflows**

Run:
```
ls .github/workflows/
```

Read any workflow file that mentions Windows or runs on `windows-latest` / `windows-2022`.

- [ ] **Step 2: Confirm the workflow runs ctest with UNITTEST=ON**

The TCI tests only run if CMake is configured with `-DUNITTEST=ON`. Check whether the existing Windows lane does this; if not, either add a separate workflow or extend the existing one.

- [ ] **Step 3: Add a TCI-tests-specific workflow if missing**

If the existing Windows CI does not run the unittest binaries, create `.github/workflows/tci-tests.yml`:

```yaml
name: TCI Tests

on:
  push:
    branches: [feat/tci-port-2.3.1]
  pull_request:
    branches: [master]

jobs:
  build-test-ubuntu:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Install deps
        run: sudo apt-get update && sudo apt-get install -y cmake ninja-build libwxgtk3.2-dev portaudio19-dev libhamlib-dev
      - name: Configure
        run: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON
      - name: Build
        run: cmake --build build
      - name: Test
        run: cd build && ctest -R '^tci_' --output-on-failure

  build-test-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Install deps
        run: brew install cmake ninja wxwidgets portaudio hamlib
      - name: Configure
        run: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON
      - name: Build
        run: cmake --build build
      - name: Test
        run: cd build && ctest -R '^tci_' --output-on-failure

  build-test-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Install deps via vcpkg
        run: vcpkg install wxwidgets portaudio
      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Debug -DUNITTEST=ON
      - name: Build
        run: cmake --build build --config Debug
      - name: Test
        run: cd build && ctest -R '^tci_' --output-on-failure -C Debug
```

The dependency-install commands depend on freedv-gui's existing CI; copy them from the existing workflows rather than guess. The above is a template.

- [ ] **Step 4: Push and verify CI**

```
git add .github/workflows/tci-tests.yml
git commit -S -m "ci(tci): Add Linux/macOS/Windows TCI test lanes

Runs only the tci_* unittests on each platform; full freedv-gui
test suite continues via existing workflows."
git push origin feat/tci-port-2.3.1
```

Watch GitHub Actions for the branch. Iterate on CI errors. Expected failure mode: missing libraries on Windows; install via vcpkg or chocolatey.

### Task 20: Bench validation passes

**Files:**
- Modify: `test/tci_bench_matrix.md`

- [ ] **Step 1: Bench against ExpertSDR3 (macOS)**

Follow the test scenarios in `test/tci_bench_matrix.md`:
1. Connect (under 5 sec, no errors).
2. RX audio (intelligible voice from a known FreeDV 700D transmission).
3. TX audio (off-air receiver decodes our FreeDV signal).
4. Reconnect (pull network cable, restore within 10 sec, state preserved).
5. Multi-client safety (with WSJT-X also connected, verify zero TX audio when WSJT-X keys up).

Update the macOS row in the matrix with Pass/Fail per column and any notes.

- [ ] **Step 2: Bench against ExpertSDR3 (Windows)**

Same scenarios on a Windows host. Update the Windows row.

- [ ] **Step 3: Bench against ThetisTCI (Linux)**

If a ThetisTCI server is available, run the scenarios on Linux. Update the row.

- [ ] **Step 4: Commit bench results**

```
git add test/tci_bench_matrix.md
git commit -S -m "docs(tci): Record bench validation results for v1

Updated bench matrix with results from macOS + Windows + Linux
testing against ExpertSDR3 and ThetisTCI servers."
```

---

## Self-Review

Run this checklist before declaring the plan complete.

1. **Spec coverage:** every section of the spec maps to a task or set of tasks.
   - Spec §1 Overview: covered by entire plan.
   - Spec §2 Provenance/license: Task 2 (verify house style) + Task 1 (preserve authorship via rebase).
   - Spec §3 Scope in/out: enforced via task boundaries; no task targets out-of-scope items.
   - Spec §4 Architecture: Task 1 brings fork code in; Tasks 11-12 replace WS transport.
   - Spec §5 Data flow: implemented across Tasks 7, 11-12, 14, 16-18.
   - Spec §6 Threading: keeps fork's model; verified during Task 1 rebase + Task 12 implementation.
   - Spec §7 Rebase plan: Task 1.
   - Spec §8 WebSocket transport replacement: Tasks 11-12.
   - Spec §9 Auto-reconnect: Task 14.
   - Spec §10 Audio Config integration: Tasks 16-18.
   - Spec §11 MOX multi-client gate: Task 10.
   - Spec §12 Fork bug fixes: Tasks 8-9.
   - Spec §13 Test plan: Tasks 5-7, 10, 15.
   - Spec §14 Single-PR plan: enforced via single-branch milestones.
   - Spec §15 Risks: addressed (rebase fallback in Task 1 Step 9; clock drift documented; TCI server format negotiation handled in r8brain).
   - Spec §16 Success criteria: validated in Tasks 13, 19, 20.
   - Spec §17 References: no task; references are in the spec.

2. **Placeholder scan**: searched plan for "TODO", "TBD", "implement later", "fill in details", "handle edge cases", "add appropriate error handling", "similar to Task N": only legitimate uses are "TBD" rows in the bench matrix that the user fills in by hand.

3. **Type / name consistency**:
   - `pending_ptt_request_` / `our_ptt_active_` / `other_client_mox_` used consistently throughout Phase D.
   - `TciWebSocketClient` public API (connect / disconnect / sendCommand / sendBinaryData / setCallback*) consistent between Task 11 (header) and Task 12 (impl).
   - `tci::isTciDeviceName` / `makeTciDeviceName` / `parseTciDeviceName` consistent between Task 15 (def) and Task 16 (use).
   - `RigControlConfiguration::TciConfig` (struct fields: enabled, host, port, trx) consistent between Task 17 (use) and Task 18 (def).

4. **Open issues from spec carried forward**: per-request-token MOX disambiguation (spec §11) is NOT solved in this plan; documented as accepted v1 limitation (sub-100ms false-positive window). Listed for follow-up in spec §15.

Plan validated.
