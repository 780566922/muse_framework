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

#include "audiodecoder.h"

namespace muse::audio::codec {
//! Ogg Vorbis reader, built on top of stb_vorbis, which the engine already
//! bundles for reading compressed soundfont samples.
class OggDecoder
{
public:
    //! Decodes `data`; returns false if it is not a playable Ogg Vorbis stream.
    static bool decodeMemory(const ByteArray& data, AudioData& out);
};
}
