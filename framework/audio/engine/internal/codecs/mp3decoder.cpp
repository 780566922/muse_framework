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

#include "mp3decoder.h"

#include <cstring>
#include <vector>

#if (defined (_MSCVER) || defined (_MSC_VER))
#pragma warning(push)
#pragma warning(disable: 4244) // conversion from 'int' to 'short', possible loss of data
#pragma warning(disable: 4267) // conversion from 'size_t' to 'int', possible loss of data
#endif
#define DR_MP3_IMPLEMENTATION
#include "thirdparty/dr_libs/dr_mp3.h"
#if (defined (_MSCVER) || defined (_MSC_VER))
#pragma warning(pop)
#endif

using namespace muse::audio::codec;

bool Mp3Decoder::decodeMemory(const ByteArray& data, AudioData& out)
{
    drmp3 mp3;
    std::memset(&mp3, 0, sizeof(mp3));

    if (!drmp3_init_memory(&mp3, data.constData(), static_cast<size_t>(data.size()), nullptr)) {
        return false;
    }

    const size_t channels = static_cast<size_t>(mp3.channels);
    out.sampleRate = static_cast<unsigned int>(mp3.sampleRate);
    out.channels = static_cast<unsigned int>(mp3.channels);

    if (channels == 0 || out.sampleRate == 0) {
        drmp3_uninit(&mp3);
        return false;
    }

    constexpr drmp3_uint64 blockFrames = 4096;
    std::vector<float> block(static_cast<size_t>(blockFrames) * channels);

    for (;;) {
        const drmp3_uint64 readFrames = drmp3_read_pcm_frames_f32(&mp3, blockFrames, block.data());
        if (readFrames == 0) {
            break;
        }

        const size_t readSamples = static_cast<size_t>(readFrames) * channels;
        out.samples.insert(out.samples.end(), block.data(), block.data() + readSamples);
    }

    drmp3_uninit(&mp3);

    return out.isValid();
}
