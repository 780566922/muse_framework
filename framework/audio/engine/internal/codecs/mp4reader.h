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

#include <cstdint>
#include <vector>

#include "global/types/bytearray.h"

namespace muse::audio::codec {
//! One access unit of an MPEG-4 audio track: where it sits in the file and how
//! many bytes it takes up.
struct Mp4Sample
{
    uint64_t offset = 0;
    uint32_t size = 0;
};

//! What the sound track of an MPEG-4 file says about itself. An `.m4a` is an
//! MPEG-4 file, and this is the part of it worth knowing before decoding.
struct Mp4AudioTrack
{
    //! An AudioSpecificConfig, meaning the track holds AAC.
    std::vector<uint8_t> audioSpecificConfig;
    //! The access units, in presentation order.
    std::vector<Mp4Sample> samples;
    //! The rate the track was sampled at, as told by the sample entry. The
    //! decoder knows better once it has seen the data.
    uint32_t sampleRate = 0;
    //! Frames the file says are not part of the audio - the run-up an AAC
    //! encoder needs before it can produce sound, which the decoder hands back
    //! as if it were audio. Zero when the file does not say.
    double startOffsetSec = 0.0;
    //! How long the track really is, as opposed to how much was encoded.
    //! Zero when the file does not say.
    double durationSec = 0.0;
};

//! Reads the audio out of an MPEG-4 file.
class Mp4Reader
{
public:
    //! Fills `track` from `data`; returns false if this is not an MPEG-4 file
    //! with AAC audio in it.
    static bool read(const ByteArray& data, Mp4AudioTrack& track);
};
}
