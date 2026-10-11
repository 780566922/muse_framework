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

#include "oggdecoder.h"

#include <climits>
#include <vector>

// The implementation of stb_vorbis is compiled into vorbisdecoder.cpp, so only
// the declarations are needed here.
#define STB_VORBIS_HEADER_ONLY
#if (defined (_MSCVER) || defined (_MSC_VER))
#pragma warning(push)
#pragma warning(disable: 4456) // declaration hides previous local declaration
#endif
#include "thirdparty/stb/stb_vorbis.c"
#if (defined (_MSCVER) || defined (_MSC_VER))
#pragma warning(pop)
#endif

using namespace muse::audio::codec;

bool OggDecoder::decodeMemory(const ByteArray& data, AudioData& out)
{
    if (data.size() > static_cast<size_t>(INT_MAX)) {
        return false;
    }

    int error = 0;
    stb_vorbis* decoder = stb_vorbis_open_memory(data.constData(), static_cast<int>(data.size()), &error, nullptr);

    if (!decoder) {
        return false;
    }

    // `stb_vorbis` is an opaque struct, so the stream properties have to be
    // queried rather than read from the members directly.
    const stb_vorbis_info info = stb_vorbis_get_info(decoder);

    const int channels = info.channels;
    out.sampleRate = static_cast<unsigned int>(info.sample_rate);
    out.channels = static_cast<unsigned int>(channels);

    if (channels <= 0 || out.sampleRate == 0) {
        stb_vorbis_close(decoder);
        return false;
    }

    const int blockFloats = 4096 * channels;
    std::vector<float> block(static_cast<size_t>(blockFloats));

    for (;;) {
        //! NOTE Despite the `num_floats` argument, this returns the number of
        //! frames that were read, not the number of floats written.
        const int readFrames = stb_vorbis_get_samples_float_interleaved(decoder, channels, block.data(), blockFloats);
        if (readFrames <= 0) {
            break;
        }

        const size_t readSamples = static_cast<size_t>(readFrames) * static_cast<size_t>(channels);
        out.samples.insert(out.samples.end(), block.data(), block.data() + readSamples);
    }

    stb_vorbis_close(decoder);

    return out.isValid();
}
