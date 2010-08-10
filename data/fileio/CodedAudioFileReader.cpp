/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "CodedAudioFileReader.h"

#include "WavFileReader.h"
#include "base/TempDirectory.h"
#include "base/Exceptions.h"
#include "base/Profiler.h"
#include "base/Serialiser.h"
#include "base/Resampler.h"

#include <iostream>
#include <QDir>
#include <QMutexLocker>

CodedAudioFileReader::CodedAudioFileReader(CacheMode cacheMode,
                                           size_t targetRate) :
    m_cacheMode(cacheMode),
    m_initialised(false),
    m_serialiser(0),
    m_fileRate(0),
    m_cacheFileWritePtr(0),
    m_cacheFileReader(0),
    m_cacheWriteBuffer(0),
    m_cacheWriteBufferIndex(0),
    m_cacheWriteBufferSize(16384),
    m_resampler(0),
    m_resampleBuffer(0)
{
//    std::cerr << "CodedAudioFileReader::CodedAudioFileReader: rate " << targetRate << std::endl;

    m_frameCount = 0;
    m_sampleRate = targetRate;
}

CodedAudioFileReader::~CodedAudioFileReader()
{
    QMutexLocker locker(&m_cacheMutex);

    endSerialised();

    if (m_cacheFileWritePtr) sf_close(m_cacheFileWritePtr);

//    std::cerr << "CodedAudioFileReader::~CodedAudioFileReader: deleting cache file reader" << std::endl;

    delete m_cacheFileReader;
    delete[] m_cacheWriteBuffer;

    if (m_cacheFileName != "") {
        if (!QFile(m_cacheFileName).remove()) {
            std::cerr << "WARNING: CodedAudioFileReader::~CodedAudioFileReader: Failed to delete cache file \"" << m_cacheFileName.toStdString() << "\"" << std::endl;
        }
    }

    delete m_resampler;
    delete[] m_resampleBuffer;
}

void
CodedAudioFileReader::startSerialised(QString id)
{
//    std::cerr << "CodedAudioFileReader::startSerialised(" << id.toStdString() << ")" << std::endl;

    delete m_serialiser;
    m_serialiser = new Serialiser(id);
}

void
CodedAudioFileReader::endSerialised()
{
//    std::cerr << "CodedAudioFileReader(" << this << ")::endSerialised: id = " << (m_serialiser ? m_serialiser->getId().toStdString() : "(none)") << std::endl;

    delete m_serialiser;
    m_serialiser = 0;
}

void
CodedAudioFileReader::initialiseDecodeCache()
{
    QMutexLocker locker(&m_cacheMutex);

//    std::cerr << "CodedAudioFileReader::initialiseDecodeCache: file rate = " << m_fileRate << std::endl;

    if (m_fileRate == 0) {
        std::cerr << "CodedAudioFileReader::initialiseDecodeCache: ERROR: File sample rate unknown (bug in subclass implementation?)" << std::endl;
        m_fileRate = 48000; // got to have something
    }
    if (m_sampleRate == 0) {
        m_sampleRate = m_fileRate;
        std::cerr << "CodedAudioFileReader::initialiseDecodeCache: rate (from file) = " << m_fileRate << std::endl;
    }
    if (m_fileRate != m_sampleRate) {
        std::cerr << "CodedAudioFileReader: resampling " << m_fileRate << " -> " <<  m_sampleRate << std::endl;
        m_resampler = new Resampler(Resampler::FastestTolerable,
                                    m_channelCount,
                                    m_cacheWriteBufferSize);
        float ratio = float(m_sampleRate) / float(m_fileRate);
        m_resampleBuffer = new float
            [lrintf(ceilf(m_cacheWriteBufferSize * m_channelCount * ratio + 1))];
    }

    m_cacheWriteBuffer = new float[m_cacheWriteBufferSize * m_channelCount];
    m_cacheWriteBufferIndex = 0;

    if (m_cacheMode == CacheInTemporaryFile) {

        try {
            QDir dir(TempDirectory::getInstance()->getPath());
            m_cacheFileName = dir.filePath(QString("decoded_%1.wav")
                                           .arg((intptr_t)this));

            SF_INFO fileInfo;
            fileInfo.samplerate = m_sampleRate;
            fileInfo.channels = m_channelCount;

            // No point in writing 24-bit or float; generally this
            // class is used for decoding files that have come from a
            // 16 bit source or that decode to only 16 bits anyway.
            fileInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    
            m_cacheFileWritePtr = sf_open(m_cacheFileName.toLocal8Bit(),
                                          SFM_WRITE, &fileInfo);

            if (m_cacheFileWritePtr) {

                // Ideally we would do this now only if we were in a
                // threaded mode -- creating the reader later if we're
                // not threaded -- but we don't have access to that
                // information here

                m_cacheFileReader = new WavFileReader(m_cacheFileName);

                if (!m_cacheFileReader->isOK()) {
                    std::cerr << "ERROR: CodedAudioFileReader::initialiseDecodeCache: Failed to construct WAV file reader for temporary file: " << m_cacheFileReader->getError().toStdString() << std::endl;
                    delete m_cacheFileReader;
                    m_cacheFileReader = 0;
                    m_cacheMode = CacheInMemory;
                    sf_close(m_cacheFileWritePtr);
                }

            } else {
                std::cerr << "CodedAudioFileReader::initialiseDecodeCache: failed to open cache file \"" << m_cacheFileName.toStdString() << "\" (" << m_channelCount << " channels, sample rate " << m_sampleRate << " for writing, falling back to in-memory cache" << std::endl;
                m_cacheMode = CacheInMemory;
            }

        } catch (DirectoryCreationFailed f) {
            std::cerr << "CodedAudioFileReader::initialiseDecodeCache: failed to create temporary directory! Falling back to in-memory cache" << std::endl;
            m_cacheMode = CacheInMemory;
        }
    }

    if (m_cacheMode == CacheInMemory) {
        m_data.clear();
    }

    m_initialised = true;
}

void
CodedAudioFileReader::addSamplesToDecodeCache(float **samples, size_t nframes)
{
    QMutexLocker locker(&m_cacheMutex);

    if (!m_initialised) return;

    for (size_t i = 0; i < nframes; ++i) {
        
        for (size_t c = 0; c < m_channelCount; ++c) {

            float sample = samples[c][i];
        
            m_cacheWriteBuffer[m_cacheWriteBufferIndex++] = sample;

            if (m_cacheWriteBufferIndex ==
                m_cacheWriteBufferSize * m_channelCount) {

                pushBuffer(m_cacheWriteBuffer, m_cacheWriteBufferSize, false);
                m_cacheWriteBufferIndex = 0;
            }

            if (m_cacheWriteBufferIndex % 10240 == 0 &&
                m_cacheFileReader) {
                m_cacheFileReader->updateFrameCount();
            }
        }
    }
}

void
CodedAudioFileReader::addSamplesToDecodeCache(float *samples, size_t nframes)
{
    QMutexLocker locker(&m_cacheMutex);

    if (!m_initialised) return;

    for (size_t i = 0; i < nframes; ++i) {
        
        for (size_t c = 0; c < m_channelCount; ++c) {

            float sample = samples[i * m_channelCount + c];
        
            m_cacheWriteBuffer[m_cacheWriteBufferIndex++] = sample;

            if (m_cacheWriteBufferIndex ==
                m_cacheWriteBufferSize * m_channelCount) {

                pushBuffer(m_cacheWriteBuffer, m_cacheWriteBufferSize, false);
                m_cacheWriteBufferIndex = 0;
            }

            if (m_cacheWriteBufferIndex % 10240 == 0 &&
                m_cacheFileReader) {
                m_cacheFileReader->updateFrameCount();
            }
        }
    }
}

void
CodedAudioFileReader::addSamplesToDecodeCache(const SampleBlock &samples)
{
    QMutexLocker locker(&m_cacheMutex);

    if (!m_initialised) return;

    for (size_t i = 0; i < samples.size(); ++i) {

        float sample = samples[i];
        
        m_cacheWriteBuffer[m_cacheWriteBufferIndex++] = sample;

        if (m_cacheWriteBufferIndex ==
            m_cacheWriteBufferSize * m_channelCount) {

            pushBuffer(m_cacheWriteBuffer, m_cacheWriteBufferSize, false);
            m_cacheWriteBufferIndex = 0;
        }

        if (m_cacheWriteBufferIndex % 10240 == 0 &&
            m_cacheFileReader) {
            m_cacheFileReader->updateFrameCount();
        }
    }
}

void
CodedAudioFileReader::finishDecodeCache()
{
    QMutexLocker locker(&m_cacheMutex);

    Profiler profiler("CodedAudioFileReader::finishDecodeCache", true);

    if (!m_initialised) {
        std::cerr << "WARNING: CodedAudioFileReader::finishDecodeCache: Cache was never initialised!" << std::endl;
        return;
    }

    if (m_cacheWriteBufferIndex > 0) {
        pushBuffer(m_cacheWriteBuffer,
                   m_cacheWriteBufferIndex / m_channelCount,
                   true);
    }        

    delete[] m_cacheWriteBuffer;
    m_cacheWriteBuffer = 0;

    delete[] m_resampleBuffer;
    m_resampleBuffer = 0;

    delete m_resampler;
    m_resampler = 0;

    if (m_cacheMode == CacheInTemporaryFile) {
        sf_close(m_cacheFileWritePtr);
        m_cacheFileWritePtr = 0;
        if (m_cacheFileReader) m_cacheFileReader->updateFrameCount();
    }
}

void
CodedAudioFileReader::pushBuffer(float *buffer, size_t sz, bool final)
{
    float max = 1.0;
    size_t count = sz * m_channelCount;

    if (m_resampler && m_fileRate != 0) {
        
        float ratio = float(m_sampleRate) / float(m_fileRate);

        if (ratio != 1.f) {

            size_t out = m_resampler->resampleInterleaved
                (buffer,
                 m_resampleBuffer,
                 sz,
                 ratio,
                 final);

            buffer = m_resampleBuffer;
            sz = out;
            count = sz * m_channelCount;
        }
    }

    for (size_t i = 0; i < count; ++i) {
        if (buffer[i] >  max) buffer[i] =  max;
    }
    for (size_t i = 0; i < count; ++i) {
        if (buffer[i] < -max) buffer[i] = -max;
    }

    m_frameCount += sz;

    switch (m_cacheMode) {

    case CacheInTemporaryFile:
        if (sf_writef_float(m_cacheFileWritePtr, buffer, sz) < sz) {
            sf_close(m_cacheFileWritePtr);
            m_cacheFileWritePtr = 0;
            throw InsufficientDiscSpace(TempDirectory::getInstance()->getPath());
        }
        break;

    case CacheInMemory:
        m_dataLock.lockForWrite();
        for (size_t s = 0; s < count; ++s) {
            m_data.push_back(buffer[s]);
        }
	MUNLOCK_SAMPLEBLOCK(m_data);
        m_dataLock.unlock();
        break;
    }
}

void
CodedAudioFileReader::getInterleavedFrames(size_t start, size_t count,
                                           SampleBlock &frames) const
{
    // Lock is only required in CacheInMemory mode (the cache file
    // reader is expected to be thread safe and manage its own
    // locking)

    if (!m_initialised) {
        std::cerr << "CodedAudioFileReader::getInterleavedFrames: not initialised" << std::endl;
        return;
    }

    switch (m_cacheMode) {

    case CacheInTemporaryFile:
        if (m_cacheFileReader) {
            m_cacheFileReader->getInterleavedFrames(start, count, frames);
        }
        break;

    case CacheInMemory:
    {
        frames.clear();
        if (!isOK()) return;
        if (count == 0) return;
        frames.reserve(count * m_channelCount);

        size_t idx = start * m_channelCount;
        size_t i = 0;

        m_dataLock.lockForRead();
        while (i < count * m_channelCount && idx < m_data.size()) {
            frames.push_back(m_data[idx]);
            ++idx;
        }
        m_dataLock.unlock();
    }
    }
}

