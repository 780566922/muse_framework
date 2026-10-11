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

#include "flacdecoder.h"

#include <algorithm>
#include <cstring>

#include "FLAC++/decoder.h"

using namespace muse::audio::codec;

static const char FLAC_MAGIC[] = "fLaC";
static constexpr size_t FLAC_MAGIC_SIZE = sizeof(FLAC_MAGIC) - 1;

namespace {
//! NOTE The stream is read straight from memory, because the audio comes from
//! the score package rather than from a file on disk.
class MemoryFlacStream : public FLAC::Decoder::Stream
{
public:
    MemoryFlacStream(const uint8_t* data, size_t size)
        : m_data(data), m_size(size) {}

    unsigned int sampleRate() const { return m_sampleRate; }
    unsigned int channels() const { return m_channels; }
    bool failed() const { return m_failed; }

    std::vector<float> takeSamples() { return std::move(m_samples); }

protected:
    ::FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], size_t* bytes) override
    {
        const size_t left = m_size - m_read;
        if (left == 0) {
            *bytes = 0;
            return ::FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        }

        const size_t wanted = std::min(*bytes, left);
        std::memcpy(buffer, m_data + m_read, wanted);
        m_read += wanted;
        *bytes = wanted;

        return ::FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }

    void metadata_callback(const ::FLAC__StreamMetadata* metadata) override
    {
        if (metadata->type != ::FLAC__METADATA_TYPE_STREAMINFO) {
            return;
        }

        m_sampleRate = metadata->data.stream_info.sample_rate;
        m_channels = metadata->data.stream_info.channels;
    }

    ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame* frame,
                                                    const FLAC__int32* const buffer[]) override
    {
        const unsigned int channels = frame->header.channels;
        const unsigned int bits = frame->header.bits_per_sample;

        if (channels == 0 || bits == 0 || bits > 32) {
            return ::FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }

        //! NOTE FLAC stores whole samples of whatever width the stream
        //! declares - 16 and 24 bits being the usual ones - so each is scaled
        //! down to the [-1, 1] range everything else is decoded into.
        const double scale = 1.0 / double(uint64_t(1) << (bits - 1));
        const unsigned int blockSize = frame->header.blocksize;

        m_samples.reserve(m_samples.size() + size_t(blockSize) * channels);

        for (unsigned int i = 0; i < blockSize; ++i) {
            for (unsigned int channel = 0; channel < channels; ++channel) {
                m_samples.push_back(static_cast<float>(double(buffer[channel][i]) * scale));
            }
        }

        return ::FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

    void error_callback(::FLAC__StreamDecoderErrorStatus) override
    {
        m_failed = true;
    }

private:
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_read = 0;

    unsigned int m_sampleRate = 0;
    unsigned int m_channels = 0;
    std::vector<float> m_samples;

    //! NOTE FLAC is allowed to report a frame it could not make sense of and
    //! carry on. The material either comes out whole or it is refused.
    bool m_failed = false;
};
}

bool FlacDecoder::decodeMemory(const ByteArray& data, AudioData& out)
{
    if (data.size() < FLAC_MAGIC_SIZE || std::memcmp(data.constData(), FLAC_MAGIC, FLAC_MAGIC_SIZE) != 0) {
        return false;
    }

    MemoryFlacStream stream(data.constData(), static_cast<size_t>(data.size()));

    if (stream.init() != ::FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        return false;
    }

    if (!stream.process_until_end_of_stream()) {
        return false;
    }

    if (stream.failed()) {
        return false;
    }

    out.sampleRate = stream.sampleRate();
    out.channels = stream.channels();
    out.samples = stream.takeSamples();

    return out.isValid();
}
