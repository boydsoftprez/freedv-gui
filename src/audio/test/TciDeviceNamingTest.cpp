//=========================================================================
// Name:            TciDeviceNamingTest.cpp
// Purpose:         Unit tests for TCI audio device name helpers.
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
#include <string>

#include "TciDeviceNaming.h"

// ---- simple assertion helpers ----

static int g_failures = 0;

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "FAIL  " << __FILE__ << ":" << __LINE__ \
                      << "  expected true: " #expr "\n"; \
            ++g_failures; \
        } \
    } while (0)

#define ASSERT_FALSE(expr) \
    do { \
        if ((expr)) { \
            std::cerr << "FAIL  " << __FILE__ << ":" << __LINE__ \
                      << "  expected false: " #expr "\n"; \
            ++g_failures; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); \
        auto _b = (b); \
        if (_a != _b) { \
            std::cerr << "FAIL  " << __FILE__ << ":" << __LINE__ \
                      << "  expected \"" << _b << "\"  got \"" << _a << "\"\n"; \
            ++g_failures; \
        } \
    } while (0)

// ---- Test 1: isTciDeviceName positive cases ----
static void test_isTciDeviceName_positive()
{
    int before = g_failures;
    std::cout << "Test 1 (isTciDeviceName positive): ";

    ASSERT_TRUE(tci::isTciDeviceName("TCI: 192.168.1.10:40001 RX"));
    ASSERT_TRUE(tci::isTciDeviceName("TCI: 127.0.0.1:40001 TX"));

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

// ---- Test 2: isTciDeviceName negative cases ----
static void test_isTciDeviceName_negative()
{
    int before = g_failures;
    std::cout << "Test 2 (isTciDeviceName negative): ";

    ASSERT_FALSE(tci::isTciDeviceName(""));
    ASSERT_FALSE(tci::isTciDeviceName("Built-in Output"));
    // lowercase prefix must not match (exact prefix "TCI: " required)
    ASSERT_FALSE(tci::isTciDeviceName("tci: lowercase"));
    // too short to contain the full prefix
    ASSERT_FALSE(tci::isTciDeviceName("TCI"));

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

// ---- Test 3: makeTciDeviceName round-trip ----
static void test_makeTciDeviceName()
{
    int before = g_failures;
    std::cout << "Test 3 (makeTciDeviceName): ";

    std::string result = tci::makeTciDeviceName("192.168.1.10", 40001, "RX");
    ASSERT_EQ(result, std::string("TCI: 192.168.1.10:40001 RX"));

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

// ---- Test 4: parseTciDeviceName round-trip ----
static void test_parse_roundtrip()
{
    int before = g_failures;
    std::cout << "Test 4 (parseTciDeviceName round-trip): ";

    std::string host;
    int port = 0;
    std::string direction;

    bool ok = tci::parseTciDeviceName("TCI: 10.0.0.5:40001 RX",
                                      host, port, direction);
    ASSERT_TRUE(ok);
    ASSERT_EQ(host, std::string("10.0.0.5"));
    ASSERT_EQ(port, 40001);
    ASSERT_EQ(direction, std::string("RX"));

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

// ---- Test 5: parseTciDeviceName rejects malformed input ----
static void test_parse_rejects_bad()
{
    int before = g_failures;
    std::cout << "Test 5 (parseTciDeviceName rejects bad input): ";

    std::string host;
    int port = 0;
    std::string direction;

    // Not a TCI name at all.
    ASSERT_FALSE(tci::parseTciDeviceName("Built-in Output", host, port, direction));

    // Missing port (no colon after host).
    ASSERT_FALSE(tci::parseTciDeviceName("TCI: 10.0.0.5 RX", host, port, direction));

    // Non-numeric port.
    ASSERT_FALSE(tci::parseTciDeviceName("TCI: 10.0.0.5:notanumber RX",
                                         host, port, direction));

    std::cout << (g_failures == before ? "PASS" : "FAIL") << "\n";
}

int main()
{
    test_isTciDeviceName_positive();
    test_isTciDeviceName_negative();
    test_makeTciDeviceName();
    test_parse_roundtrip();
    test_parse_rejects_bad();

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
