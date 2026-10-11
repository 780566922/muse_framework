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

#include "mp4reader.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

using namespace muse::audio::codec;

namespace {
//! A four-byte box type, spelled the way it appears in a file.
constexpr uint32_t boxType(const char* name)
{
    return (uint32_t(uint8_t(name[0])) << 24) | (uint32_t(uint8_t(name[1])) << 16)
           | (uint32_t(uint8_t(name[2])) << 8) | uint32_t(uint8_t(name[3]));
}

uint16_t readU16(const uint8_t* data)
{
    return uint16_t((uint16_t(data[0]) << 8) | uint16_t(data[1]));
}

uint32_t readU32(const uint8_t* data)
{
    return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

uint64_t readU64(const uint8_t* data)
{
    return (uint64_t(readU32(data)) << 32) | uint64_t(readU32(data + 4));
}

//! A box, which is what ISO/IEC 14496-12 calls the pieces a file is made of.
struct Box
{
    uint32_t type = 0;
    //! The contents; the size and the type that introduce them are not part of it.
    const uint8_t* data = nullptr;
    size_t size = 0;

    const uint8_t* end() const { return data + size; }
};

//! Visits the boxes laid out in [begin, end), stopping early if `visit` says so.
template<typename Fn>
void forEachBox(const uint8_t* begin, const uint8_t* end, Fn visit)
{
    const uint8_t* pos = begin;
    while (pos + 8 <= end) {
        uint64_t size = readU32(pos);
        const uint32_t type = readU32(pos + 4);
        size_t headerSize = 8;

        if (size == 1) {
            //! NOTE A size of one means the real, 64-bit size follows the type.
            if (pos + 16 > end) {
                return;
            }
            size = readU64(pos + 8);
            headerSize = 16;
        } else if (size == 0) {
            //! NOTE A size of zero means the box runs to the end of its parent.
            size = uint64_t(end - pos);
        }

        if (size < headerSize || size > uint64_t(end - pos)) {
            return;
        }

        Box box;
        box.type = type;
        box.data = pos + headerSize;
        box.size = size_t(size) - headerSize;

        if (!visit(box)) {
            return;
        }

        pos += size;
    }
}

bool findBox(const uint8_t* begin, const uint8_t* end, uint32_t type, Box& found)
{
    bool result = false;
    forEachBox(begin, end, [&](const Box& box) {
        if (box.type != type) {
            return true;
        }
        found = box;
        result = true;
        return false;
    });

    return result;
}

//! The version of a full box, which decides how its fields are laid out.
uint32_t boxVersion(const Box& box)
{
    return box.size < 4 ? 0 : readU32(box.data) >> 24;
}

//! ES descriptors carry their length over seven bits per byte, the high bit
//! set for as long as more bytes follow.
bool readDescriptorLength(const uint8_t*& pos, const uint8_t* end, uint32_t& length)
{
    length = 0;
    for (int i = 0; i < 4; ++i) {
        if (pos >= end) {
            return false;
        }

        const uint8_t byte = *pos++;
        length = (length << 7) | uint32_t(byte & 0x7F);

        if ((byte & 0x80) == 0) {
            return true;
        }
    }

    return false;
}

//! Digs the AudioSpecificConfig out of an `esds` box, which keeps it nested
//! three descriptors deep.
bool readAudioSpecificConfig(const uint8_t* data, size_t size, std::vector<uint8_t>& config)
{
    if (size < 4) {
        return false;
    }

    const uint8_t* pos = data + 4;   //!< past version and flags
    const uint8_t* end = data + size;

    if (pos >= end || *pos++ != 0x03) {   //!< ES_Descriptor
        return false;
    }

    uint32_t length = 0;
    if (!readDescriptorLength(pos, end, length)) {
        return false;
    }

    const uint8_t* esEnd = std::min(end, pos + length);
    if (pos + 3 > esEnd) {
        return false;
    }

    pos += 3;   //!< ES_ID and flags

    if (pos >= esEnd || *pos++ != 0x04) {   //!< DecoderConfigDescriptor
        return false;
    }

    if (!readDescriptorLength(pos, esEnd, length)) {
        return false;
    }

    const uint8_t* configEnd = std::min(esEnd, pos + length);
    if (pos + 13 > configEnd) {
        return false;
    }

    const uint8_t objectType = pos[0];
    pos += 13;   //!< object type, stream type, buffer size and bit rates

    //! NOTE 0x40 is MPEG-4 audio; the other three are the MPEG-2 flavours of AAC.
    if (objectType != 0x40 && objectType != 0x66 && objectType != 0x67 && objectType != 0x68) {
        return false;
    }

    if (pos >= configEnd || *pos++ != 0x05) {   //!< DecoderSpecificInfo
        return false;
    }

    if (!readDescriptorLength(pos, configEnd, length)) {
        return false;
    }

    if (length == 0 || pos + length > configEnd) {
        return false;
    }

    config.assign(pos, pos + length);

    return true;
}

//! Reads the `mp4a` sample entry, which holds what a decoder needs to start up.
bool readSampleEntry(const Box& entry, Mp4AudioTrack& track)
{
    constexpr size_t fixedPartSize = 28;
    if (entry.size < fixedPartSize) {
        return false;
    }

    //! NOTE QuickTime grew the entry twice over the years; the extra fields sit
    //! between the fixed part and the child boxes.
    const uint16_t version = readU16(entry.data + 8);
    size_t headerSize = fixedPartSize;
    if (version == 1) {
        headerSize += 16;
    } else if (version == 2) {
        headerSize += 36;
    }

    if (entry.size < headerSize) {
        return false;
    }

    track.sampleRate = readU32(entry.data + 24) >> 16;

    const uint8_t* children = entry.data + headerSize;

    Box esds;
    if (findBox(children, entry.end(), boxType("esds"), esds)) {
        return readAudioSpecificConfig(esds.data, esds.size, track.audioSpecificConfig);
    }

    //! NOTE QuickTime tucks the descriptor away one level deeper, in a `wave` box.
    Box wave;
    if (findBox(children, entry.end(), boxType("wave"), wave)
        && findBox(wave.data, wave.end(), boxType("esds"), esds)) {
        return readAudioSpecificConfig(esds.data, esds.size, track.audioSpecificConfig);
    }

    return false;
}

//! A non-zero size says every sample is that long, and that no table follows.
bool readSampleSizes(const Box& box, std::vector<uint32_t>& sizes)
{
    if (box.size < 12) {
        return false;
    }

    const uint32_t uniformSize = readU32(box.data + 4);
    const uint32_t count = readU32(box.data + 8);

    if (uniformSize != 0) {
        sizes.assign(count, uniformSize);
        return true;
    }

    if (box.size < 12 + size_t(count) * 4) {
        return false;
    }

    sizes.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        sizes.push_back(readU32(box.data + 12 + size_t(i) * 4));
    }

    return true;
}

bool readChunkOffsets(const uint8_t* begin, const uint8_t* end, std::vector<uint64_t>& offsets)
{
    Box box;
    if (findBox(begin, end, boxType("stco"), box)) {
        if (box.size < 8) {
            return false;
        }

        const uint32_t count = readU32(box.data + 4);
        if (box.size < 8 + size_t(count) * 4) {
            return false;
        }

        offsets.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            offsets.push_back(readU32(box.data + 8 + size_t(i) * 4));
        }

        return true;
    }

    //! NOTE Files past a certain size use the 64-bit flavour instead.
    if (findBox(begin, end, boxType("co64"), box)) {
        if (box.size < 8) {
            return false;
        }

        const uint32_t count = readU32(box.data + 4);
        if (box.size < 8 + size_t(count) * 8) {
            return false;
        }

        offsets.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            offsets.push_back(readU64(box.data + 8 + size_t(i) * 8));
        }

        return true;
    }

    return false;
}

//! A run of chunks that all hold the same number of samples.
struct ChunkRun
{
    uint32_t firstChunk = 0;   //!< one-based, the way the file counts them
    uint32_t samplesPerChunk = 0;
};

bool readChunkRuns(const Box& box, std::vector<ChunkRun>& runs)
{
    if (box.size < 8) {
        return false;
    }

    const uint32_t count = readU32(box.data + 4);
    if (box.size < 8 + size_t(count) * 12) {
        return false;
    }

    runs.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* entry = box.data + 8 + size_t(i) * 12;
        ChunkRun run;
        run.firstChunk = readU32(entry);
        run.samplesPerChunk = readU32(entry + 4);
        runs.push_back(run);
    }

    return true;
}

//! Turns the three tables - where the chunks are, how many samples each holds
//! and how long every sample is - into the list of access units a decoder wants.
bool buildSamples(const std::vector<ChunkRun>& runs, const std::vector<uint64_t>& chunkOffsets,
                  const std::vector<uint32_t>& sizes, std::vector<Mp4Sample>& samples)
{
    size_t index = 0;

    for (size_t chunk = 0; chunk < chunkOffsets.size(); ++chunk) {
        uint32_t samplesPerChunk = 0;
        for (const ChunkRun& run : runs) {
            //! NOTE A run covers the chunks from the one it names up to the one
            //! the next run names, which is why the last match wins.
            if (run.firstChunk > chunk + 1) {
                break;
            }
            samplesPerChunk = run.samplesPerChunk;
        }

        if (samplesPerChunk == 0) {
            return false;
        }

        uint64_t offset = chunkOffsets[chunk];
        for (uint32_t i = 0; i < samplesPerChunk && index < sizes.size(); ++i, ++index) {
            if (sizes[index] == 0) {
                continue;
            }

            Mp4Sample sample;
            sample.offset = offset;
            sample.size = sizes[index];
            samples.push_back(sample);
            offset += sizes[index];
        }
    }

    return !samples.empty();
}

//! One entry of an edit list, in the units the file wrote it in.
struct RawEdit
{
    uint64_t segmentDuration = 0;   //!< in movie units
    int64_t mediaTime = -1;         //!< in media units, -1 for an empty edit
};

//! An edit list says that only part of what was encoded is the audio. It sits
//! inside a box of its own, which is what the search starts from.
void readEditList(const uint8_t* begin, const uint8_t* end, std::vector<RawEdit>& edits)
{
    Box edts;
    if (!findBox(begin, end, boxType("edts"), edts)) {
        return;
    }

    Box box;
    if (!findBox(edts.data, edts.end(), boxType("elst"), box) || box.size < 8) {
        return;
    }

    const uint32_t version = boxVersion(box);
    const uint32_t count = readU32(box.data + 4);
    const size_t entrySize = version == 1 ? 20 : 12;
    if (box.size < 8 + size_t(count) * entrySize) {
        return;
    }

    const uint8_t* pos = box.data + 8;
    for (uint32_t i = 0; i < count; ++i) {
        RawEdit edit;
        if (version == 1) {
            edit.segmentDuration = readU64(pos);
            edit.mediaTime = int64_t(readU64(pos + 8));
        } else {
            edit.segmentDuration = readU32(pos);
            edit.mediaTime = int32_t(readU32(pos + 4));
        }

        edits.push_back(edit);
        pos += entrySize;
    }
}

//! Apple's files leave the edit list out and name the same numbers in a tag
//! instead: the run-up to drop, the padding to drop and the length of the
//! audio, in that order, each written in hex.
void readAppleGaplessTag(const uint8_t* begin, const uint8_t* end, uint32_t mediaTimescale, Mp4AudioTrack& track)
{
    if (mediaTimescale == 0) {
        return;
    }

    Box udta;
    if (!findBox(begin, end, boxType("udta"), udta)) {
        return;
    }

    Box meta;
    if (!findBox(udta.data, udta.end(), boxType("meta"), meta)) {
        return;
    }

    //! NOTE `meta` is a full box, but the oldest QuickTime files do not say so.
    Box ilst;
    if (!findBox(meta.data + std::min<size_t>(4, meta.size), meta.end(), boxType("ilst"), ilst)
        && !findBox(meta.data, meta.end(), boxType("ilst"), ilst)) {
        return;
    }

    forEachBox(ilst.data, ilst.end(), [&](const Box& item) {
        if (item.type != boxType("----")) {
            return true;
        }

        bool isGapless = false;
        Box value;
        forEachBox(item.data, item.end(), [&](const Box& child) {
            if (child.type == boxType("name")) {
                const uint8_t* name = child.data + 4;
                const size_t nameSize = child.size > 4 ? child.size - 4 : 0;
                isGapless = nameSize >= 8 && std::memcmp(name, "iTunSMPB", 8) == 0;
            } else if (child.type == boxType("data")) {
                value = child;
            }

            return true;
        });

        if (!isGapless || value.size <= 8) {
            return true;
        }

        //! NOTE The fields are a reserved one, the run-up, the padding that
        //! trails the audio and the length of the audio itself. Only the run-up
        //! and the length are of any use here.
        const std::string text(reinterpret_cast<const char*>(value.data + 8), value.size - 8);
        unsigned int runUp = 0;
        unsigned long long duration = 0;
        if (std::sscanf(text.c_str(), " %*x %x %*x %llx", &runUp, &duration) != 2) {
            return true;
        }

        track.startOffsetSec = double(runUp) / double(mediaTimescale);
        track.durationSec = double(duration) / double(mediaTimescale);

        return false;
    });
}

//! Reads one track; returns true only for a sound track holding AAC.
bool readTrack(const Box& trak, const Box& moov, uint32_t movieTimescale, Mp4AudioTrack& track)
{
    const uint8_t* begin = trak.data;
    const uint8_t* end = trak.end();

    Box mdia;
    if (!findBox(begin, end, boxType("mdia"), mdia)) {
        return false;
    }

    //! NOTE Only sound tracks are looked at. A video track is somebody else's.
    Box hdlr;
    if (!findBox(mdia.data, mdia.end(), boxType("hdlr"), hdlr) || hdlr.size < 12) {
        return false;
    }

    if (readU32(hdlr.data + 8) != boxType("soun")) {
        return false;
    }

    uint32_t mediaTimescale = 0;
    Box mdhd;
    if (findBox(mdia.data, mdia.end(), boxType("mdhd"), mdhd)) {
        const size_t offset = boxVersion(mdhd) == 1 ? 20 : 12;
        if (mdhd.size >= offset + 4) {
            mediaTimescale = readU32(mdhd.data + offset);
        }
    }

    Box minf;
    if (!findBox(mdia.data, mdia.end(), boxType("minf"), minf)) {
        return false;
    }

    Box stbl;
    if (!findBox(minf.data, minf.end(), boxType("stbl"), stbl)) {
        return false;
    }

    Box stsd;
    if (!findBox(stbl.data, stbl.end(), boxType("stsd"), stsd) || stsd.size < 8) {
        return false;
    }

    Box entry;
    bool hasEntry = false;
    forEachBox(stsd.data + 8, stsd.end(), [&](const Box& box) {
        entry = box;
        hasEntry = true;
        return false;
    });

    //! NOTE Anything but `mp4a` - ALAC, an encrypted track, a video codec - is
    //! not something this reader knows how to feed a decoder.
    if (!hasEntry || entry.type != boxType("mp4a")) {
        return false;
    }

    if (!readSampleEntry(entry, track)) {
        return false;
    }

    Box stsz;
    Box stsc;
    std::vector<uint32_t> sizes;
    std::vector<uint64_t> chunkOffsets;
    std::vector<ChunkRun> runs;
    std::vector<Mp4Sample> samples;

    if (!findBox(stbl.data, stbl.end(), boxType("stsz"), stsz) || !readSampleSizes(stsz, sizes)) {
        return false;
    }

    if (!findBox(stbl.data, stbl.end(), boxType("stsc"), stsc) || !readChunkRuns(stsc, runs)) {
        return false;
    }

    if (!readChunkOffsets(stbl.data, stbl.end(), chunkOffsets)) {
        return false;
    }

    if (!buildSamples(runs, chunkOffsets, sizes, samples)) {
        return false;
    }

    track.samples = std::move(samples);

    //! NOTE Where in the encoded signal the audio begins, and how long it runs.
    //! The edit list says it one way, Apple's tag says it another, and a file
    //! may well say it both ways.
    if (mediaTimescale > 0) {
        std::vector<RawEdit> edits;
        readEditList(begin, end, edits);

        for (const RawEdit& edit : edits) {
            if (edit.mediaTime < 0) {
                continue;
            }

            track.startOffsetSec = double(edit.mediaTime) / double(mediaTimescale);
            if (movieTimescale > 0 && edit.segmentDuration > 0) {
                track.durationSec = double(edit.segmentDuration) / double(movieTimescale);
            }
            break;
        }

        if (track.startOffsetSec == 0.0) {
            readAppleGaplessTag(begin, end, mediaTimescale, track);
            if (track.startOffsetSec == 0.0) {
                readAppleGaplessTag(moov.data, moov.end(), mediaTimescale, track);
            }
        }
    }

    return true;
}
}

bool Mp4Reader::read(const ByteArray& data, Mp4AudioTrack& track)
{
    track = Mp4AudioTrack();

    //! NOTE An MPEG-4 file introduces itself with a file type box.
    if (data.size() < 8 || readU32(data.constData() + 4) != boxType("ftyp")) {
        return false;
    }

    const uint8_t* begin = data.constData();
    const uint8_t* end = begin + data.size();

    Box moov;
    if (!findBox(begin, end, boxType("moov"), moov)) {
        return false;
    }

    uint32_t movieTimescale = 0;
    Box mvhd;
    if (findBox(moov.data, moov.end(), boxType("mvhd"), mvhd)) {
        const size_t offset = boxVersion(mvhd) == 1 ? 20 : 12;
        if (mvhd.size >= offset + 4) {
            movieTimescale = readU32(mvhd.data + offset);
        }
    }

    bool found = false;
    forEachBox(moov.data, moov.end(), [&](const Box& box) {
        if (box.type != boxType("trak")) {
            return true;
        }

        if (readTrack(box, moov, movieTimescale, track)) {
            found = true;
            return false;
        }

        return true;
    });

    return found;
}
