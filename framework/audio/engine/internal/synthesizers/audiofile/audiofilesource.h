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

#include <memory>

#include "../../codecs/wavdecoder.h"
#include "../abstractsynthesizer.h"

namespace muse::audio::synth {
//! Plays back a decoded audio clip in sync with the playback position of the
//! score. The clip itself is never modified: no time-stretching or pitch
//! shifting is applied, the samples are simply resampled to the output rate.
class AudioFileSource : public AbstractSynthesizer
{
public:
    AudioFileSource(const audio::AudioInputParams& params);

    void setAudioData(const std::shared_ptr<codec::WavData>& data);

    //! Position on the playback timeline (in seconds) at which the clip starts.
    //! Before it the source is silent.
    void setTimelineStart(secs_t startSec);

    //! Trims the clip: `inSec` is where playback starts inside the clip,
    //! `outSec` where it ends. A negative `outSec` means "play to the end".
    void setTrim(secs_t inSec, secs_t outSec);

    std::string name() const override;
    AudioSourceType type() const override;
    bool isValid() const override;

    void setOutputSpec(const OutputSpec& spec) override;

    const mpe::PlaybackData& playbackData() const override;

    TimePosition playbackPosition() const override;
    void setPlaybackPosition(const TimePosition& position) override;

    void flushSound() override;

    samples_t process(float* buffer, samples_t samplesPerChannel) override;

protected:
    void setupSound(const mpe::PlaybackSetupData& setupData) override;
    void setupEvents(const mpe::PlaybackData& playbackData) override;

private:
    //! Reads one frame, interpolating between neighbouring frames
    void readFrame(double frame, float* out, audioch_t outChannels) const;

    std::shared_ptr<codec::WavData> m_data;

    OutputSpec m_outputSpec;

    secs_t m_timelineStart = 0.0;
    secs_t m_inSec = 0.0;
    secs_t m_outSec = -1.0;

    //! Current position on the playback timeline, in seconds
    secs_t m_positionSec = 0.0;

    //! An audio clip is not driven by notation events
    mpe::PlaybackData m_playbackData;
};
}
