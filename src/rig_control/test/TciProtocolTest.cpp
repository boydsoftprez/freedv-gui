//=========================================================================
// Name:            TciProtocolTest.cpp
// Purpose:         Unit tests for tci::CommandParser (TciProtocol)
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

#include "TciProtocol.h"

#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Minimal test harness (matches freedv-gui pipeline/test style)
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
// Helper: equality check with diagnostic output
// ---------------------------------------------------------------------------

static bool checkEqual(const std::string& label,
                       const std::string& got,
                       const std::string& expected)
{
    if (got != expected)
    {
        std::cerr << "[" << label << ": expected \"" << expected
                  << "\", got \"" << got << "\"]...";
        return false;
    }
    return true;
}

static bool checkEqualVec(const std::string& label,
                           const std::vector<std::string>& got,
                           const std::vector<std::string>& expected)
{
    if (got.size() != expected.size())
    {
        std::cerr << "[" << label << ": size mismatch expected="
                  << expected.size() << " got=" << got.size() << "]...";
        return false;
    }
    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (got[i] != expected[i])
        {
            std::cerr << "[" << label << "[" << i << "]: expected \""
                      << expected[i] << "\" got \"" << got[i] << "\"]...";
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 1: "ready;" -- no-args command
// ---------------------------------------------------------------------------
bool parseReady()
{
    tci::CommandParser parser;
    std::string name;
    std::vector<std::string> args;

    bool ok = parser.parseCommand("ready;", name, args);
    if (!ok)
    {
        std::cerr << "[parseCommand returned false]...";
        return false;
    }
    // Command name is uppercased by the parser
    if (!checkEqual("cmdName", name, "READY")) { return false; }
    if (!args.empty())
    {
        std::cerr << "[args not empty, size=" << args.size() << "]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 2: "dds:0,7050000;" -- two-arg command
// ---------------------------------------------------------------------------
bool parseDds()
{
    tci::CommandParser parser;
    std::string name;
    std::vector<std::string> args;

    bool ok = parser.parseCommand("dds:0,7050000;", name, args);
    if (!ok)
    {
        std::cerr << "[parseCommand returned false]...";
        return false;
    }
    if (!checkEqual("cmdName", name, "DDS")) { return false; }
    if (!checkEqualVec("args", args, {"0", "7050000"})) { return false; }
    return true;
}

// ---------------------------------------------------------------------------
// Test 3a: "trx:0,true;" -- boolean true arg
// ---------------------------------------------------------------------------
bool parseTrxTrue()
{
    tci::CommandParser parser;
    std::string name;
    std::vector<std::string> args;

    bool ok = parser.parseCommand("trx:0,true;", name, args);
    if (!ok)
    {
        std::cerr << "[parseCommand returned false]...";
        return false;
    }
    if (!checkEqual("cmdName", name, "TRX")) { return false; }
    if (!checkEqualVec("args", args, {"0", "true"})) { return false; }
    return true;
}

// ---------------------------------------------------------------------------
// Test 3b: "trx:0,false;" -- boolean false arg
// ---------------------------------------------------------------------------
bool parseTrxFalse()
{
    tci::CommandParser parser;
    std::string name;
    std::vector<std::string> args;

    bool ok = parser.parseCommand("trx:0,false;", name, args);
    if (!ok)
    {
        std::cerr << "[parseCommand returned false]...";
        return false;
    }
    if (!checkEqual("cmdName", name, "TRX")) { return false; }
    if (!checkEqualVec("args", args, {"0", "false"})) { return false; }
    return true;
}

// ---------------------------------------------------------------------------
// Test 4: "modulation:0,digu;" -- modulation command
// ---------------------------------------------------------------------------
bool parseModulation()
{
    tci::CommandParser parser;
    std::string name;
    std::vector<std::string> args;

    bool ok = parser.parseCommand("modulation:0,digu;", name, args);
    if (!ok)
    {
        std::cerr << "[parseCommand returned false]...";
        return false;
    }
    if (!checkEqual("cmdName", name, "MODULATION")) { return false; }
    if (!checkEqualVec("args", args, {"0", "digu"})) { return false; }
    return true;
}

// ---------------------------------------------------------------------------
// Test 5: "protocol;" -- no-colon no-args command
// ---------------------------------------------------------------------------
bool parseProtocol()
{
    tci::CommandParser parser;
    std::string name;
    std::vector<std::string> args;

    bool ok = parser.parseCommand("protocol;", name, args);
    if (!ok)
    {
        std::cerr << "[parseCommand returned false]...";
        return false;
    }
    if (!checkEqual("cmdName", name, "PROTOCOL")) { return false; }
    if (!args.empty())
    {
        std::cerr << "[args not empty, size=" << args.size() << "]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 6: quoted device arg -- quotes are NOT stripped by the parser
// The parser passes argument tokens verbatim; quote stripping is NOT done.
// Observed behavior: args[0] == "\"ExpertSDR3\""
// ---------------------------------------------------------------------------
bool parseDeviceQuoted()
{
    tci::CommandParser parser;
    std::string name;
    std::vector<std::string> args;

    bool ok = parser.parseCommand("device:\"ExpertSDR3\";", name, args);
    if (!ok)
    {
        std::cerr << "[parseCommand returned false]...";
        return false;
    }
    if (!checkEqual("cmdName", name, "DEVICE")) { return false; }
    // Quotes are preserved verbatim -- no stripping in splitArgs
    if (!checkEqualVec("args", args, {"\"ExpertSDR3\""})) { return false; }
    return true;
}

// ---------------------------------------------------------------------------
// Test 7: empty string -- must return false
// ---------------------------------------------------------------------------
bool parseEmpty()
{
    tci::CommandParser parser;
    std::string name;
    std::vector<std::string> args;

    bool ok = parser.parseCommand("", name, args);
    if (ok)
    {
        std::cerr << "[parseCommand returned true on empty string]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 8: missing semicolon -- must return false (no crash)
// ---------------------------------------------------------------------------
bool parseMissingSemicolon()
{
    tci::CommandParser parser;
    std::string name;
    std::vector<std::string> args;

    // The parser checks command.back() != ';' and returns false immediately
    bool ok = parser.parseCommand("dds:0,7050000", name, args);
    if (ok)
    {
        std::cerr << "[parseCommand returned true with no semicolon]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 9: buildCommand round-trip
// ---------------------------------------------------------------------------
bool buildCommandBasic()
{
    std::string result = tci::CommandParser::buildCommand("dds", {"0", "14070000"});
    return checkEqual("buildCommand", result, "dds:0,14070000;");
}

// ---------------------------------------------------------------------------
// Test 9b: buildCommand with no args
// ---------------------------------------------------------------------------
bool buildCommandNoArgs()
{
    std::string result = tci::CommandParser::buildCommand("ready", {});
    return checkEqual("buildCommand_noargs", result, "ready;");
}

// ---------------------------------------------------------------------------
// Test 10: splitArgs
// ---------------------------------------------------------------------------
bool splitArgsBasic()
{
    std::vector<std::string> result = tci::CommandParser::splitArgs("0,7050000");
    return checkEqualVec("splitArgs", result, {"0", "7050000"});
}

// ---------------------------------------------------------------------------
// Test 10b: splitArgs single token (no comma)
// ---------------------------------------------------------------------------
bool splitArgsSingle()
{
    std::vector<std::string> result = tci::CommandParser::splitArgs("digu");
    return checkEqualVec("splitArgs_single", result, {"digu"});
}

// ---------------------------------------------------------------------------
// Test 11: stringToModulation("DIGU")
// ---------------------------------------------------------------------------
bool stringToModDigu()
{
    tci::Modulation mod = tci::CommandParser::stringToModulation("DIGU");
    if (mod != tci::DIGU)
    {
        std::cerr << "[DIGU: expected " << tci::DIGU << " got " << mod << "]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 12: stringToModulation("USB")
// ---------------------------------------------------------------------------
bool stringToModUsb()
{
    tci::Modulation mod = tci::CommandParser::stringToModulation("USB");
    if (mod != tci::USB)
    {
        std::cerr << "[USB: expected " << tci::USB << " got " << mod << "]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 12b: stringToModulation is case-insensitive
// ---------------------------------------------------------------------------
bool stringToModLowercase()
{
    tci::Modulation mod = tci::CommandParser::stringToModulation("usb");
    if (mod != tci::USB)
    {
        std::cerr << "[usb lowercase: expected " << tci::USB << " got " << mod << "]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test 13: modulationToString(USB) returns lowercase "usb"
// Observed from implementation: all return values are lowercase strings.
// ---------------------------------------------------------------------------
bool modToStringUsb()
{
    std::string s = tci::CommandParser::modulationToString(tci::USB);
    return checkEqual("modulationToString(USB)", s, "usb");
}

// ---------------------------------------------------------------------------
// Test 14: round-trip DIGU -> enum -> string
// "DIGU" -> DIGU enum -> "digu" (lowercase, as returned by modulationToString)
// ---------------------------------------------------------------------------
bool roundTripDigu()
{
    tci::Modulation mod = tci::CommandParser::stringToModulation("DIGU");
    std::string s = tci::CommandParser::modulationToString(mod);
    // modulationToString returns lowercase; round-trip is DIGU -> "digu"
    return checkEqual("roundTrip_DIGU", s, "digu");
}

// ---------------------------------------------------------------------------
// Test: stringToModulation handles "CW-L" and "CW_L" aliases
// ---------------------------------------------------------------------------
bool stringToModCwlAliases()
{
    tci::Modulation m1 = tci::CommandParser::stringToModulation("CW-L");
    tci::Modulation m2 = tci::CommandParser::stringToModulation("CW_L");
    if (m1 != tci::CW_L)
    {
        std::cerr << "[CW-L alias failed: got " << m1 << "]...";
        return false;
    }
    if (m2 != tci::CW_L)
    {
        std::cerr << "[CW_L alias failed: got " << m2 << "]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Test: stringToModulation unknown -> defaults to AM
// ---------------------------------------------------------------------------
bool stringToModUnknownDefaultsToAm()
{
    tci::Modulation mod = tci::CommandParser::stringToModulation("NOTAMOD");
    if (mod != tci::AM)
    {
        std::cerr << "[unknown mod: expected AM(" << tci::AM << ") got " << mod << "]...";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    TEST_CASE(parseReady);
    TEST_CASE(parseDds);
    TEST_CASE(parseTrxTrue);
    TEST_CASE(parseTrxFalse);
    TEST_CASE(parseModulation);
    TEST_CASE(parseProtocol);
    TEST_CASE(parseDeviceQuoted);
    TEST_CASE(parseEmpty);
    TEST_CASE(parseMissingSemicolon);
    TEST_CASE(buildCommandBasic);
    TEST_CASE(buildCommandNoArgs);
    TEST_CASE(splitArgsBasic);
    TEST_CASE(splitArgsSingle);
    TEST_CASE(stringToModDigu);
    TEST_CASE(stringToModUsb);
    TEST_CASE(stringToModLowercase);
    TEST_CASE(modToStringUsb);
    TEST_CASE(roundTripDigu);
    TEST_CASE(stringToModCwlAliases);
    TEST_CASE(stringToModUnknownDefaultsToAm);

    return 0;
}
