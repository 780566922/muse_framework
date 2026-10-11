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

#include "audiofileresolver.h"

#include "audiofilesource.h"

#include "log.h"

using namespace muse;
using namespace muse::audio;
using namespace muse::audio::synth;

void AudioFileResolver::setClipData(const audio::AudioResourceId& id, const std::shared_ptr<codec::AudioData>& data,
                                    const AudioClipParams& params)
{
    Clip clip;
    clip.data = data;
    clip.params = params;

    m_clips.insert_or_assign(id, clip);
}

void AudioFileResolver::clearClipData(const audio::AudioResourceId& id)
{
    m_clips.erase(id);
}

ISynthesizerPtr AudioFileResolver::resolveSynth(const audio::TrackId trackId, const audio::AudioInputParams& params,
                                                const OutputSpec& spec) const
{
    auto search = m_clips.find(params.resourceMeta.id);
    if (search == m_clips.end()) {
        LOGE() << "no audio data registered for resource: " << params.resourceMeta.id
               << ", trackId: " << trackId;
        return nullptr;
    }

    const Clip& clip = search->second;

    std::shared_ptr<AudioFileSource> source = std::make_shared<AudioFileSource>(params);
    source->setAudioData(clip.data);
    source->setTimelineStart(clip.params.startSec);
    source->setTrim(clip.params.inSec, clip.params.outSec);
    source->setOutputSpec(spec);

    return source;
}

bool AudioFileResolver::hasCompatibleResources(const audio::PlaybackSetupData&) const
{
    // an audio clip does not depend on any instrument setup data
    return true;
}

audio::AudioResourceMetaList AudioFileResolver::resolveResources() const
{
    // clips belong to a score, so they are not part of the global resource list
    return audio::AudioResourceMetaList();
}

audio::SoundPresetList AudioFileResolver::resolveSoundPresets(const AudioResourceMeta&) const
{
    return audio::SoundPresetList();
}

void AudioFileResolver::refresh()
{
}

void AudioFileResolver::clearSources()
{
    m_clips.clear();
}
