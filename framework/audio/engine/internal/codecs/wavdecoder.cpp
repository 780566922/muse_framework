/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "wavdecoder.h"

#include <cstring>

#include "log.h"

using namespace muse;
using namespace muse::audio::codec;

namespace {
//! Wave formats we know how to read
enum WaveFormat : uint16_t {
    WAVE_FORMAT_PCM = 0x0001,
    WAVE_FORMAT_IEEE_FLOAT = 0x0003,
    WAVE_FORMAT_EXTENSIBLE = 0xFFFE
};

struct RiffChunkHeader {
    char id[4] = { 0, 0, 0, 0 };
    uint32_t size = 0;
};

bool chunkIdEquals(const char* id, const char* expected)
{
    return std::memcmp(id, expected, 4) == 0;
}

uint16_t readU16LE(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t readU32LE(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
           | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

int32_t readS24LE(const uint8_t* p)
{
    uint32_t v = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16);
    // sign-extend a 24-bit value
    if (v & 0x800000) {
        v |= 0xFF000000;
    }
    return static_cast<int32_t>(v);
}

int32_t readS32LE(const uint8_t* p)
{
    return static_cast<int32_t>(readU32LE(p));
}

float readF32LE(const uint8_t* p)
{
    uint32_t bits = readU32LE(p);
    float value = 0.f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double readF64LE(const uint8_t* p)
{
    uint64_t bits = 0;
    for (size_t i = 0; i < 8; ++i) {
        bits |= static_cast<uint64_t>(p[i]) << (8 * i);
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

//! Reads a sample of `bitsPerSample` bits and normalises it to [-1, 1]
float readSample(const uint8_t* p, uint16_t bitsPerSample, bool isFloat)
{
    if (isFloat) {
        if (bitsPerSample == 64) {
            return static_cast<float>(readF64LE(p));
        }
        return readF32LE(p);
    }

    switch (bitsPerSample) {
    case 8:
        // 8-bit WAVE samples are unsigned, centred on 128
        return (static_cast<float>(p[0]) - 128.f) / 128.f;
    case 16:
        return static_cast<float>(static_cast<int16_t>(readU16LE(p))) / 32768.f;
    case 24:
        return static_cast<float>(readS24LE(p)) / 8388608.f;
    case 32:
        return static_cast<float>(readS32LE(p)) / 2147483648.f;
    default:
        return 0.f;
    }
}
}

bool WavDecoder::decodeMemory(const ByteArray& data, WavData& out)
{
    const uint8_t* bytes = data.constData();
    const size_t size = data.size();

    // "RIFF" + size + "WAVE"
    if (size < 12 || std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0) {
        LOGE() << "not a RIFF/WAVE stream";
        return false;
    }

    uint16_t formatTag = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;

    const uint8_t* dataChunk = nullptr;
    size_t dataChunkSize = 0;

    size_t pos = 12;
    while (pos + 8 <= size) {
        RiffChunkHeader header;
        std::memcpy(header.id, bytes + pos, 4);
        header.size = readU32LE(bytes + pos + 4);

        const uint8_t* body = bytes + pos + 8;
        size_t bodySize = std::min<size_t>(header.size, size - (pos + 8));

        if (chunkIdEquals(header.id, "fmt ")) {
            // audioFormat(2) + channels(2) + sampleRate(4) + byteRate(4) + blockAlign(2) + bits(2)
            if (bodySize < 16) {
                LOGE() << "malformed fmt chunk";
                return false;
            }

            formatTag = readU16LE(body);
            channels = readU16LE(body + 2);
            sampleRate = readU32LE(body + 4);
            bitsPerSample = readU16LE(body + 14);

            if (formatTag == WAVE_FORMAT_EXTENSIBLE) {
                // cbSize(2) + validBits(2) + channelMask(4) + SubFormat GUID(16)
                if (bodySize < 40) {
                    LOGE() << "malformed WAVE_FORMAT_EXTENSIBLE fmt chunk";
                    return false;
                }
                formatTag = readU16LE(body + 24);
            }
        } else if (chunkIdEquals(header.id, "data")) {
            dataChunk = body;
            dataChunkSize = bodySize;
        }

        // chunks are padded to an even length
        pos += 8 + header.size + (header.size % 2);
    }

    if (formatTag != WAVE_FORMAT_PCM && formatTag != WAVE_FORMAT_IEEE_FLOAT) {
        LOGE() << "unsupported wave format: " << formatTag;
        return false;
    }

    if (channels == 0 || sampleRate == 0 || bitsPerSample == 0 || bitsPerSample % 8 != 0) {
        LOGE() << "unsupported wave properties, channels: " << channels
               << ", sampleRate: " << sampleRate
               << ", bitsPerSample: " << bitsPerSample;
        return false;
    }

    if (!dataChunk || dataChunkSize == 0) {
        LOGE() << "no data chunk found";
        return false;
    }

    const size_t bytesPerSample = bitsPerSample / 8;
    const size_t framesCount = dataChunkSize / (bytesPerSample * channels);

    if (framesCount == 0) {
        LOGE() << "empty data chunk";
        return false;
    }

    const bool isFloat = (formatTag == WAVE_FORMAT_IEEE_FLOAT);

    out.sampleRate = sampleRate;
    out.channels = channels;
    out.samples.clear();
    out.samples.resize(framesCount * channels);

    for (size_t frame = 0; frame < framesCount; ++frame) {
        const uint8_t* frameStart = dataChunk + frame * bytesPerSample * channels;
        for (size_t channel = 0; channel < channels; ++channel) {
            out.samples[frame * channels + channel] = readSample(frameStart + channel * bytesPerSample, bitsPerSample, isFloat);
        }
    }

    return out.isValid();
}
