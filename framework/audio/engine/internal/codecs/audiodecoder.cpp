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

#include "audiodecoder.h"

#include "wavdecoder.h"
#include "oggdecoder.h"
#include "flacdecoder.h"
#include "mp3decoder.h"

using namespace muse::audio::codec;

bool AudioDecoder::decodeMemory(const ByteArray& data, AudioData& out)
{
    if (data.empty()) {
        return false;
    }

    out = AudioData();

    //! NOTE The formats that announce themselves with a marker are tried first,
    //! so that MP3 - which is found by looking for something that reads like a
    //! frame anywhere in the stream - is only reached when nothing else fits.
    if (WavDecoder::decodeMemory(data, out)) {
        return true;
    }

    if (OggDecoder::decodeMemory(data, out)) {
        return true;
    }

    if (FlacDecoder::decodeMemory(data, out)) {
        return true;
    }

    if (Mp3Decoder::decodeMemory(data, out)) {
        return true;
    }

    return false;
}
