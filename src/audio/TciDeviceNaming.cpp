//=========================================================================
// Name:            TciDeviceNaming.cpp
// Purpose:         Helpers for synthesizing and parsing TCI audio device
//                  names. Format: "TCI: <host>:<port> <RX|TX>".
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
