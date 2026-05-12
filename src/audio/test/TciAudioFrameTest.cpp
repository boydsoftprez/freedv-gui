//=========================================================================
// Name:            TciAudioFrameTest.cpp
// Purpose:         Unit tests for TCI sample-format conversion helpers
//                  and StreamHeader struct layout.
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

#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>

#include "TciSampleConvert.h"
#include "rig_control/TciProtocol.h"

// ---- simple assertion helpers ----

static int g_failures = 0;

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (_a != _b) { \
            std::cerr << "FAIL  " << __FILE__ << ":" << __LINE__ \
                      << "  expected " << _b << "  got " << _a << "\n"; \
            ++g_failures; \
        } \
    } while (0)

#define ASSERT_NEAR(val, expected, tol) \
    do { \
        auto _v = (val); \
        auto _e = (expected); \
        auto _t = (tol); \
        if (_v < _e - _t || _v > _e + _t) { \
            std::cerr << "FAIL  " << __FILE__ << ":" << __LINE__ \
                      << "  expected " << _e << " +/- " << _t << "  got " << _v << "\n"; \
            ++g_failures; \
        } \
    } while (0)

// ---- Test 1: Int16 stereo-to-mono average ----
// Input: L=1000, R=2000, L=3000, R=4000 (two interleaved stereo pairs)
// Expected mono: out[0] = (1000+2000)/2 = 1500, out[1] = (3000+4000)/2 = 3500
static void test_int16_stereo_average()
{
    std::cout << "Test 1 (Int16 stereo-to-mono average): ";

    int16_t src[] = { 1000, 2000, 3000, 4000 };
    // numSamples = total interleaved samples = 4 (not mono count)
    std::vector<short> out;
    tci::convertInt16ToShort(reinterpret_cast<const uint8_t*>(src), 4, out);

    ASSERT_EQ(static_cast<int>(out.size()), 2);
    ASSERT_EQ(static_cast<int>(out[0]), 1500);
    ASSERT_EQ(static_cast<int>(out[1]), 3500);

    std::cout << (g_failures == 0 ? "PASS" : "FAIL") << "\n";
}

// ---- Test 2: Float32 clamping ----
// Input: L=2.0, R=2.0, L=-2.0, R=-2.0 (exceeds [-1, +1])
// After averaging and clamping: avg=2.0 -> 32767; avg=-2.0 -> -32768
static void test_float32_clamping()
{
    int before = g_failures;
    std::cout << "Test 2 (Float32 clamping): ";

    float src[] = { 2.0f, 2.0f, -2.0f, -2.0f };
    std::vector<short> out;
    tci::convertFloat32ToShort(reinterpret_cast<const uint8_t*>(src), 4, out);

    ASSERT_EQ(static_cast<int>(out.size()), 2);
    // Implementation scales by 32767.0f (not 32768.0f), so the minimum
    // clamped output is -32767, not -32768. This preserves existing behavior.
    ASSERT_EQ(static_cast<int>(out[0]), 32767);
    ASSERT_EQ(static_cast<int>(out[1]), -32767);

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

// ---- Test 3: Float32 DC signal ----
// Input: L=0.5, R=0.5, L=0.5, R=0.5
// avg = 0.5; 0.5 * 32767 = 16383.5 -> truncated to 16383
static void test_float32_dc()
{
    int before = g_failures;
    std::cout << "Test 3 (Float32 DC signal): ";

    float src[] = { 0.5f, 0.5f, 0.5f, 0.5f };
    std::vector<short> out;
    tci::convertFloat32ToShort(reinterpret_cast<const uint8_t*>(src), 4, out);

    ASSERT_EQ(static_cast<int>(out.size()), 2);
    ASSERT_NEAR(static_cast<int>(out[0]), 16383, 1);
    ASSERT_NEAR(static_cast<int>(out[1]), 16383, 1);

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

// ---- Test 4: Int32 high-bit preservation ----
// Input: L=1<<30, R=1<<30, L=-(1<<30), R=-(1<<30)
// (left+right)/2 >> 16: (1<<30 + 1<<30)/2 = 1<<30; 1<<30 >> 16 = 16384
// For negative: (-(1<<30) + -(1<<30))/2 = -(1<<30); -(1<<30) >> 16 = -16384
static void test_int32_high_bit()
{
    int before = g_failures;
    std::cout << "Test 4 (Int32 high-bit preservation): ";

    int32_t src[] = { 1 << 30, 1 << 30, -(1 << 30), -(1 << 30) };
    std::vector<short> out;
    tci::convertInt32ToShort(reinterpret_cast<const uint8_t*>(src), 4, out);

    ASSERT_EQ(static_cast<int>(out.size()), 2);
    ASSERT_EQ(static_cast<int>(out[0]), 16384);
    ASSERT_EQ(static_cast<int>(out[1]), -16384);

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

// ---- Test 5: Int24 sign extension ----
// Input: 6 bytes, all 0xFF (L = 0xFFFFFF = -1 in signed 24-bit, R = same)
// avg = (-1 + -1)/2 = -1; output = (short)(-1 >> 8) = -1
static void test_int24_sign_extension()
{
    int before = g_failures;
    std::cout << "Test 5 (Int24 sign extension): ";

    uint8_t src[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    std::vector<short> out;
    tci::convertInt24ToShort(src, 2, out);  // 2 total interleaved samples = 1 mono

    ASSERT_EQ(static_cast<int>(out.size()), 1);
    ASSERT_NEAR(static_cast<int>(out[0]), -1, 1);

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

// ---- Test 6: StreamHeader struct size ----
static void test_stream_header_size()
{
    int before = g_failures;
    std::cout << "Test 6 (StreamHeader size == 64): ";

    ASSERT_EQ(sizeof(tci::StreamHeader), 64u);

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

int main()
{
    test_int16_stereo_average();
    test_float32_clamping();
    test_float32_dc();
    test_int32_high_bit();
    test_int24_sign_extension();
    test_stream_header_size();

    if (g_failures == 0)
    {
        std::cout << "\nAll tests PASSED.\n";
        return 0;
    }
    else
    {
        std::cout << "\n" << g_failures << " test(s) FAILED.\n";
        return 1;
    }
}
