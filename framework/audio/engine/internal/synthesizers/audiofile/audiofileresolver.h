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
#include <unordered_map>

#include "../../../isynthresolver.h"

#include "../../codecs/audiodecoder.h"

namespace muse::audio::synth {
//! Resolves audio clips attached to a score into playable sources.
//!
//! Unlike soundfonts or plugins, an audio clip is not a globally available
//! resource: it belongs to one particular score. The decoded data is therefore
//! handed over by the caller (see `setClipData`) before the track asks for a
//! source, and it is keyed by the resource id of the clip.
class AudioFileResolver : public ISynthResolver::IResolver
{
public:
    void setClipData(const audio::AudioResourceId& id, const std::shared_ptr<codec::AudioData>& data);
    void clearClipData(const audio::AudioResourceId& id);

    ISynthesizerPtr resolveSynth(const audio::TrackId trackId, const audio::AudioInputParams& params,
                                 const OutputSpec& spec) const override;
    bool hasCompatibleResources(const audio::PlaybackSetupData& setup) const override;

    audio::AudioResourceMetaList resolveResources() const override;
    audio::SoundPresetList resolveSoundPresets(const AudioResourceMeta& resourceMeta) const override;

    void refresh() override;
    void clearSources() override;

private:
    std::unordered_map<audio::AudioResourceId, std::shared_ptr<codec::AudioData> > m_clips;
};

using AudioFileResolverPtr = std::shared_ptr<AudioFileResolver>;
}
