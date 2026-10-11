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

#include "aacdecoder.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "aacdecoder_lib.h"
#include "FDK_audio.h"

#include "mp4reader.h"

using namespace muse::audio::codec;

namespace {
//! The widest a single frame can get: HE-AAC makes two samples for every one
//! stored, and eight channels is as many as an AAC stream carries.
constexpr size_t MAX_FRAME_SAMPLES = 2048 * 8;

//! One decoder, kept alive for as long as it takes to read a file.
class AacStream
{
public:
    ~AacStream()
    {
        if (m_handle) {
            aacDecoder_Close(m_handle);
        }
    }

    bool open(const std::vector<uint8_t>& config)
    {
        if (config.empty()) {
            return false;
        }

        //! NOTE The audio is handed over as bare access units, so the decoder
        //! has to be told what it is looking at before it sees any of it. The
        //! AudioSpecificConfig taken from the file says exactly that.
        m_handle = aacDecoder_Open(TT_MP4_RAW, 1);
        if (!m_handle) {
            return false;
        }

        UCHAR* configurations[] = { const_cast<UCHAR*>(config.data()) };
        const UINT lengths[] = { UINT(config.size()) };

        return aacDecoder_ConfigRaw(m_handle, configurations, lengths) == AAC_DEC_OK;
    }

    //! The run-up this decoder puts in front of the audio, in samples. The file
    //! says nothing about it, because it belongs to the decoder rather than to
    //! what was encoded.
    size_t decoderDelay() const { return m_decoderDelay; }

    //! Offers one access unit and appends whatever audio comes back out of it.
    bool decode(const uint8_t* data, size_t size, AudioData& out)
    {
        size_t left = size;

        while (left > 0) {
            UCHAR* buffers[] = { const_cast<UCHAR*>(data) };
            UINT sizes[] = { UINT(left) };
            UINT valid = UINT(left);

            if (aacDecoder_Fill(m_handle, buffers, sizes, &valid) != AAC_DEC_OK) {
                return false;
            }

            //! NOTE The decoder takes only as much as fits its own buffer. What
            //! is left over is offered again once the frames already in there
            //! have been decoded.
            const size_t copied = left - valid;
            if (copied == 0) {
                return false;
            }

            data += copied;
            left = valid;

            if (!drain(out)) {
                return false;
            }
        }

        return true;
    }

private:
    //! Pulls out every frame the decoder is already able to make up, and stops
    //! asking once it wants more input than it has.
    bool drain(AudioData& out)
    {
        while (true) {
            const AAC_DECODER_ERROR error
                = aacDecoder_DecodeFrame(m_handle, m_pcm.data(), INT(m_pcm.size()), 0);

            if (error == AAC_DEC_NOT_ENOUGH_BITS) {
                return true;
            }

            if (error != AAC_DEC_OK) {
                return false;
            }

            const CStreamInfo* info = aacDecoder_GetStreamInfo(m_handle);
            if (!info || info->frameSize <= 0 || info->numChannels <= 0) {
                return false;
            }

            if (out.channels == 0) {
                out.sampleRate = unsigned(info->sampleRate);
                out.channels = unsigned(info->numChannels);
                m_decoderDelay = size_t(info->outputDelay);
            } else if (unsigned(info->numChannels) != out.channels) {
                //! NOTE The layout has no business changing part way through.
                return false;
            }

            const size_t count = size_t(info->frameSize) * size_t(info->numChannels);
            if (count > m_pcm.size()) {
                return false;
            }

            //! NOTE The decoder hands back whole 16-bit samples, while
            //! everything else here works in the [-1, 1] range.
            for (size_t i = 0; i < count; ++i) {
                out.samples.push_back(static_cast<float>(double(m_pcm[i]) / 32768.0));
            }
        }
    }

    HANDLE_AACDECODER m_handle = nullptr;
    std::vector<INT_PCM> m_pcm = std::vector<INT_PCM>(MAX_FRAME_SAMPLES);
    size_t m_decoderDelay = 0;
};

//! Drops the run-up in front of the audio, and stops where the file says the
//! audio ends. There are two parts to that run-up: the one the encoder had to
//! produce before it could make any sound, which the file names, and the one
//! the decoder adds on top of it, which the file knows nothing about. Both have
//! to go, or the audio would start late by as much as a tenth of a second.
void trimToRecordedLength(const Mp4AudioTrack& track, size_t decoderDelay, AudioData& out)
{
    if (out.sampleRate == 0 || out.channels == 0 || out.samples.empty()) {
        return;
    }

    const size_t frames = out.framesCount();
    const size_t start
        = size_t(std::llround(track.startOffsetSec * double(out.sampleRate))) + decoderDelay;
    if (start >= frames) {
        return;
    }

    //! NOTE What the file says is taken at its word, but never past what the
    //! decoder actually produced.
    const size_t length = track.durationSec > 0.0
                          ? size_t(std::llround(track.durationSec * double(out.sampleRate)))
                          : frames - start;
    const size_t end = std::min(frames, start + length);
    if (end <= start) {
        return;
    }

    std::vector<float> trimmed(out.samples.begin() + start * out.channels,
                               out.samples.begin() + end * out.channels);
    out.samples = std::move(trimmed);
}
}

bool AacDecoder::decodeMemory(const ByteArray& data, AudioData& out)
{
    Mp4AudioTrack track;
    if (!Mp4Reader::read(data, track)) {
        return false;
    }

    AacStream stream;
    if (!stream.open(track.audioSpecificConfig)) {
        return false;
    }

    const uint8_t* file = data.constData();
    const size_t fileSize = data.size();

    for (const Mp4Sample& sample : track.samples) {
        if (sample.offset + sample.size > fileSize) {
            return false;
        }

        if (!stream.decode(file + sample.offset, sample.size, out)) {
            return false;
        }
    }

    trimToRecordedLength(track, stream.decoderDelay(), out);

    return out.isValid();
}
