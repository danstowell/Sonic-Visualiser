/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "WavFileReader.h"

#include <iostream>

#include <QMutexLocker>
#include <QFileInfo>

WavFileReader::WavFileReader(FileSource source, bool fileUpdating) :
    m_file(0),
    m_source(source),
    m_path(source.getLocalFilename()),
    m_buffer(0),
    m_bufsiz(0),
    m_lastStart(0),
    m_lastCount(0),
    m_updating(fileUpdating)
{
    m_frameCount = 0;
    m_channelCount = 0;
    m_sampleRate = 0;

    m_fileInfo.format = 0;
    m_fileInfo.frames = 0;
    m_file = sf_open(m_path.toLocal8Bit(), SFM_READ, &m_fileInfo);

    if (!m_file || (!fileUpdating && m_fileInfo.channels <= 0)) {
	std::cerr << "WavFileReader::initialize: Failed to open file ("
		  << sf_strerror(m_file) << ")" << std::endl;

	if (m_file) {
	    m_error = QString("Couldn't load audio file '%1':\n%2")
		.arg(m_path).arg(sf_strerror(m_file));
	} else {
	    m_error = QString("Failed to open audio file '%1'")
		.arg(m_path);
	}
	return;
    }

    if (m_fileInfo.channels > 0) {
        m_frameCount = m_fileInfo.frames;
        m_channelCount = m_fileInfo.channels;
        m_sampleRate = m_fileInfo.samplerate;
    }

//    std::cerr << "WavFileReader: Frame count " << m_frameCount << ", channel count " << m_channelCount << ", sample rate " << m_sampleRate << std::endl;

}

WavFileReader::~WavFileReader()
{
    if (m_file) sf_close(m_file);
    delete[] m_buffer;
}

void
WavFileReader::updateFrameCount()
{
    QMutexLocker locker(&m_mutex);

    size_t prevCount = m_fileInfo.frames;

    if (m_file) {
        sf_close(m_file);
        m_file = sf_open(m_path.toLocal8Bit(), SFM_READ, &m_fileInfo);
        if (!m_file || m_fileInfo.channels <= 0) {
            std::cerr << "WavFileReader::updateFrameCount: Failed to open file ("
                      << sf_strerror(m_file) << ")" << std::endl;
        }
    }

//    std::cerr << "WavFileReader::updateFrameCount: now " << m_fileInfo.frames << std::endl;

    m_frameCount = m_fileInfo.frames;

    if (m_channelCount == 0) {
        m_channelCount = m_fileInfo.channels;
        m_sampleRate = m_fileInfo.samplerate;
    }

    if (m_frameCount != prevCount) {
//        std::cerr << "frameCountChanged" << std::endl;
        emit frameCountChanged();
    }
}

void
WavFileReader::updateDone()
{
    updateFrameCount();
    m_updating = false;
}

void
WavFileReader::getInterleavedFrames(size_t start, size_t count,
				    SampleBlock &results) const
{
    if (count == 0) return;
    results.clear();
    results.reserve(count * m_fileInfo.channels);

    QMutexLocker locker(&m_mutex);

    if (!m_file || !m_channelCount) {
        return;
    }

    if ((long)start >= m_fileInfo.frames) {
//        std::cerr << "WavFileReader::getInterleavedFrames: " << start
//                  << " > " << m_fileInfo.frames << std::endl;
	return;
    }

    if (long(start + count) > m_fileInfo.frames) {
	count = m_fileInfo.frames - start;
    }

    sf_count_t readCount = 0;

    if (start != m_lastStart || count != m_lastCount) {

	if (sf_seek(m_file, start, SEEK_SET) < 0) {
//            std::cerr << "sf_seek failed" << std::endl;
	    return;
	}
	
	if (count * m_fileInfo.channels > m_bufsiz) {
//	    std::cerr << "WavFileReader: Reallocating buffer for " << count
//		      << " frames, " << m_fileInfo.channels << " channels: "
//		      << m_bufsiz << " floats" << std::endl;
	    m_bufsiz = count * m_fileInfo.channels;
	    delete[] m_buffer;
	    m_buffer = new float[m_bufsiz];
	}
	
	if ((readCount = sf_readf_float(m_file, m_buffer, count)) < 0) {
//            std::cerr << "sf_readf_float failed" << std::endl;
	    return;
	}

	m_lastStart = start;
	m_lastCount = readCount;
    }

    for (size_t i = 0; i < count * m_fileInfo.channels; ++i) {
        if (i >= m_bufsiz) {
            std::cerr << "INTERNAL ERROR: WavFileReader::getInterleavedFrames: " << i << " >= " << m_bufsiz << std::endl;
        }
	results.push_back(m_buffer[i]);
    }

    return;
}

void
WavFileReader::getSupportedExtensions(std::set<QString> &extensions)
{
    int count;

    if (sf_command(0, SFC_GET_FORMAT_MAJOR_COUNT, &count, sizeof(count))) {
        extensions.insert("wav");
        extensions.insert("aiff");
        extensions.insert("aifc");
        extensions.insert("aif");
        return;
    }

    SF_FORMAT_INFO info;
    for (int i = 0; i < count; ++i) {
        info.format = i;
        if (!sf_command(0, SFC_GET_FORMAT_MAJOR, &info, sizeof(info))) {
            extensions.insert(QString(info.extension).toLower());
        }
    }
}

bool
WavFileReader::supportsExtension(QString extension)
{
    std::set<QString> extensions;
    getSupportedExtensions(extensions);
    return (extensions.find(extension.toLower()) != extensions.end());
}

bool
WavFileReader::supportsContentType(QString type)
{
    return (type == "audio/x-wav" ||
            type == "audio/x-aiff" ||
            type == "audio/basic");
}

bool
WavFileReader::supports(FileSource &source)
{
    return (supportsExtension(source.getExtension()) ||
            supportsContentType(source.getContentType()));
}


