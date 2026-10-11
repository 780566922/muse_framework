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
//! AAC reader, built on the libfdk-aac that the engine already ships for
//! writing AAC files, so that reading one costs nothing extra to carry around.
//!
//! AAC audio normally travels inside an `.m4a`, which is an MPEG-4 file, so the
//! audio is first picked out of that container - see Mp4Reader - and only then
//! handed to the decoder.
class AacDecoder
{
public:
    //! Decodes `data`; returns false if it is not an `.m4a` holding AAC.
    static bool decodeMemory(const ByteArray& data, AudioData& out);
};
}
