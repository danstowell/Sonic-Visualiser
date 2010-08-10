/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "AudioFileReader.h"

void
AudioFileReader::getDeInterleavedFrames(size_t start, size_t count,
                                        std::vector<SampleBlock> &frames) const
{
    SampleBlock interleaved;
    getInterleavedFrames(start, count, interleaved);
    
    size_t channels = getChannelCount();
    size_t rc = interleaved.size() / channels;

    frames.clear();

    for (size_t c = 0; c < channels; ++c) {
        frames.push_back(SampleBlock());
    }

    for (size_t i = 0; i < rc; ++i) {
        for (size_t c = 0; c < channels; ++c) {
            frames[c].push_back(interleaved[i * channels + c]);
        }
    }
}

