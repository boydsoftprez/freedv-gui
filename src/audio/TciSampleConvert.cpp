//=========================================================================
// Name:            TciSampleConvert.cpp
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

#include "TciSampleConvert.h"

#include <algorithm>
#include <cstdint>

namespace tci {

void convertInt16ToShort(const uint8_t* data, std::size_t numSamples, std::vector<short>& output)
{
    const short* samples = reinterpret_cast<const short*>(data);

    // Note: numSamples is total samples including all channels
    // For stereo (2 channels), we convert LRLRLR... pairs to mono by averaging
    // Assuming stereo for TCI audio (channels = 2)
    const int channels = 2;
    std::size_t monoSamples = numSamples / channels;

    output.resize(monoSamples);

    for (std::size_t i = 0; i < monoSamples; ++i)
    {
        // Average left and right channels
        int32_t left = samples[i * 2];
        int32_t right = samples[i * 2 + 1];
        output[i] = static_cast<short>((left + right) / 2);
    }
}

void convertInt24ToShort(const uint8_t* data, std::size_t numSamples, std::vector<short>& output)
{
    // Note: numSamples is total samples including all channels
    // For stereo (2 channels), we convert LRLRLR... pairs to mono by averaging
    const int channels = 2;
    std::size_t monoSamples = numSamples / channels;

    output.resize(monoSamples);

    for (std::size_t i = 0; i < monoSamples; ++i)
    {
        // Convert left channel 24-bit to 16-bit
        int32_t left24 = (static_cast<int32_t>(data[i * 6 + 2]) << 16) |
                         (static_cast<int32_t>(data[i * 6 + 1]) << 8) |
                         (static_cast<int32_t>(data[i * 6 + 0]));
        if (left24 & 0x00800000) { left24 |= 0xFF000000; }

        // Convert right channel 24-bit to 16-bit
        int32_t right24 = (static_cast<int32_t>(data[i * 6 + 5]) << 16) |
                          (static_cast<int32_t>(data[i * 6 + 4]) << 8) |
                          (static_cast<int32_t>(data[i * 6 + 3]));
        if (right24 & 0x00800000) { right24 |= 0xFF000000; }

        // Average and shift down to 16-bit range
        int32_t avg = (left24 + right24) / 2;
        output[i] = static_cast<short>(avg >> 8);
    }
}

void convertInt32ToShort(const uint8_t* data, std::size_t numSamples, std::vector<short>& output)
{
    const int32_t* samples32 = reinterpret_cast<const int32_t*>(data);

    // Note: numSamples is total samples including all channels
    // For stereo (2 channels), we convert LRLRLR... pairs to mono by averaging
    const int channels = 2;
    std::size_t monoSamples = numSamples / channels;

    output.resize(monoSamples);

    for (std::size_t i = 0; i < monoSamples; ++i)
    {
        // Average left and right channels, then shift down from 32-bit to 16-bit range
        int64_t left = samples32[i * 2];
        int64_t right = samples32[i * 2 + 1];
        output[i] = static_cast<short>((left + right) / 2 >> 16);
    }
}

void convertFloat32ToShort(const uint8_t* data, std::size_t numSamples, std::vector<short>& output)
{
    const float* samplesFloat = reinterpret_cast<const float*>(data);

    // Note: numSamples is total samples including all channels
    // For stereo (2 channels), we convert LRLRLR... pairs to mono by averaging
    const int channels = 2;
    std::size_t monoSamples = numSamples / channels;

    output.resize(monoSamples);

    for (std::size_t i = 0; i < monoSamples; ++i)
    {
        // Average left and right channels
        float left = samplesFloat[i * 2];
        float right = samplesFloat[i * 2 + 1];
        float avg = (left + right) / 2.0f;

        // Convert float [-1.0, 1.0] to short [-32768, 32767]
        avg = std::max(-1.0f, std::min(1.0f, avg));
        output[i] = static_cast<short>(avg * 32767.0f);
    }
}

} // namespace tci
