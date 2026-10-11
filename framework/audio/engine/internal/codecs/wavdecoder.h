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
//! Minimal RIFF/WAVE reader.
//!
//! Supports PCM (8/16/24/32-bit integer) and IEEE float (32/64-bit) streams,
//! including the WAVE_FORMAT_EXTENSIBLE variant. Everything is read from
//! memory, because in practice the audio comes from the score package rather
//! than from a file on disk.
class WavDecoder
{
public:
    //! Decodes `data`; returns false if it is not a supported WAVE stream.
    static bool decodeMemory(const ByteArray& data, AudioData& out);
};
}
