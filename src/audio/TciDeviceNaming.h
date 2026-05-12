//=========================================================================
// Name:            TciDeviceNaming.h
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
// the input is a valid TCI device name (and the output parameters are filled).
bool parseTciDeviceName(const std::string& name,
                        std::string& outHost,
                        int& outPort,
                        std::string& outDirection);

} // namespace tci

#endif // TCI_DEVICE_NAMING_H
