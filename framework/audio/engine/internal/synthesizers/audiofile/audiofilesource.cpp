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

#include "audiofilesource.h"

#include <algorithm>
#include <cmath>

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::synth;

AudioFileSource::AudioFileSource(const audio::AudioInputParams& params)
    : AbstractSynthesizer(params)
{
}

void AudioFileSource::setAudioData(const std::shared_ptr<codec::WavData>& data)
{
    m_data = data;
}

void AudioFileSource::setTimelineStart(secs_t startSec)
{
    m_timelineStart = startSec;
}

void AudioFileSource::setTrim(secs_t inSec, secs_t outSec)
{
    m_inSec = inSec;
    m_outSec = outSec;
}

std::string AudioFileSource::name() const
{
    return "Audio File";
}

AudioSourceType AudioFileSource::type() const
{
    return AudioSourceType::AudioFile;
}

bool AudioFileSource::isValid() const
{
    return m_data != nullptr && m_data->isValid();
}

void AudioFileSource::setOutputSpec(const OutputSpec& spec)
{
    m_outputSpec = spec;
}

const mpe::PlaybackData& AudioFileSource::playbackData() const
{
    return m_playbackData;
}

TimePosition AudioFileSource::playbackPosition() const
{
    if (!m_outputSpec.isValid()) {
        return TimePosition();
    }

    return TimePosition::fromTime(m_positionSec, m_outputSpec.sampleRate);
}

void AudioFileSource::setPlaybackPosition(const TimePosition& position)
{
    IF_ASSERT_FAILED(position.isValid()) {
        return;
    }

    m_positionSec = position.time();
}

void AudioFileSource::flushSound()
{
    // nothing to flush: the source has no internal state beyond its position
}

void AudioFileSource::setupSound(const mpe::PlaybackSetupData&)
{
}

void AudioFileSource::setupEvents(const mpe::PlaybackData&)
{
}

samples_t AudioFileSource::process(float* buffer, samples_t samplesPerChannel)
{
    IF_ASSERT_FAILED(samplesPerChannel > 0) {
        return 0;
    }

    if (!isValid() || !m_outputSpec.isValid()) {
        return 0;
    }

    const double rate = static_cast<double>(m_data->sampleRate);
    const double frames = static_cast<double>(m_data->framesCount());
    const audioch_t outChannels = m_outputSpec.audioChannelCount;

    const double startFrame = m_inSec * rate;
    double endFrame = frames;
    if (m_outSec >= 0.0) {
        endFrame = std::min(frames, static_cast<double>(m_outSec) * rate);
    }

    const secs_t clipTime = m_positionSec - m_timelineStart + m_inSec;

    for (samples_t i = 0; i < samplesPerChannel; ++i) {
        const double frame = (clipTime + static_cast<double>(i) / static_cast<double>(m_outputSpec.sampleRate)) * rate;

        float* out = buffer + i * outChannels;

        if (frame < startFrame || frame >= endFrame || frame < 0.0) {
            std::fill(out, out + outChannels, 0.f);
            continue;
        }

        readFrame(frame, out, outChannels);
    }

    m_positionSec += static_cast<double>(samplesPerChannel) / static_cast<double>(m_outputSpec.sampleRate);

    return samplesPerChannel;
}

void AudioFileSource::readFrame(double frame, float* out, audioch_t outChannels) const
{
    const size_t frames = m_data->framesCount();
    const audioch_t srcChannels = static_cast<audioch_t>(m_data->channels);

    const double floorFrame = std::floor(frame);
    const size_t f0 = static_cast<size_t>(floorFrame);
    const float frac = static_cast<float>(frame - floorFrame);

    const size_t f1 = std::min(f0 + 1, frames - 1);

    for (audioch_t ch = 0; ch < outChannels; ++ch) {
        const audioch_t srcChannel = std::min(ch, static_cast<audioch_t>(srcChannels - 1));

        const float s0 = m_data->samples[f0 * srcChannels + srcChannel];
        const float s1 = m_data->samples[f1 * srcChannels + srcChannel];

        out[ch] = s0 + (s1 - s0) * frac;
    }
}
