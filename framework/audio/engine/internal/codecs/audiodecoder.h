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

#pragma once

#include <vector>

#include "global/types/bytearray.h"

namespace muse::audio::codec {
//! Decoded contents of an audio file, in a layout the engine can play back.
//!
//! Regardless of the format the audio was stored in, everything ends up here
//! as interleaved samples normalised to the [-1, 1] range.
struct AudioData
{
    unsigned int sampleRate = 0;
    unsigned int channels = 0;

    //! Interleaved samples, normalised to the [-1, 1] range
    std::vector<float> samples;

    bool isValid() const
    {
        return sampleRate > 0 && channels > 0 && !samples.empty();
    }

    size_t framesCount() const
    {
        return channels > 0 ? samples.size() / channels : 0;
    }
};

//! Entry point for decoding an audio file attached to a score.
//!
//! The data is always read from memory, because in practice the audio comes
//! from the score package rather than from a file on disk. The format is
//! detected from the contents of the stream, so the caller does not need to
//! know anything about it.
class AudioDecoder
{
public:
    //! Decodes `data`; returns false if the format is not supported.
    static bool decodeMemory(const ByteArray& data, AudioData& out);
};
}
