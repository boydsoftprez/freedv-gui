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

// All conversions expect interleaved stereo input and produce mono short output.
// numSamples = total mono samples to produce (the function reads numSamples*2 source values).
void convertInt16ToShort(const uint8_t* data, std::size_t numSamples, std::vector<short>& output);
void convertInt24ToShort(const uint8_t* data, std::size_t numSamples, std::vector<short>& output);
void convertInt32ToShort(const uint8_t* data, std::size_t numSamples, std::vector<short>& output);
void convertFloat32ToShort(const uint8_t* data, std::size_t numSamples, std::vector<short>& output);

} // namespace tci

#endif // TCI_SAMPLE_CONVERT_H
