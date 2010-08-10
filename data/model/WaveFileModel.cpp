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

#include "WaveFileModel.h"

#include "fileio/AudioFileReader.h"
#include "fileio/AudioFileReaderFactory.h"

#include "system/System.h"

#include <QFileInfo>
#include <QTextStream>

#include <iostream>
#include <unistd.h>
#include <cmath>
#include <sndfile.h>

#include <cassert>

//#define DEBUG_WAVE_FILE_MODEL 1

using std::cerr;
using std::endl;

PowerOfSqrtTwoZoomConstraint
WaveFileModel::m_zoomConstraint;

WaveFileModel::WaveFileModel(FileSource source, size_t targetRate) :
    m_source(source),
    m_path(source.getLocation()),
    m_myReader(true),
    m_startFrame(0),
    m_fillThread(0),
    m_updateTimer(0),
    m_lastFillExtent(0),
    m_exiting(false)
{
    m_source.waitForData();
    if (m_source.isOK()) {
        m_reader = AudioFileReaderFactory::createThreadingReader
            (m_source, targetRate);
        if (m_reader) {
            std::cerr << "WaveFileModel::WaveFileModel: reader rate: "
                      << m_reader->getSampleRate() << std::endl;
        }
    }
    if (m_reader) setObjectName(m_reader->getTitle());
    if (objectName() == "") setObjectName(QFileInfo(m_path).fileName());
    if (isOK()) fillCache();
}

WaveFileModel::WaveFileModel(FileSource source, AudioFileReader *reader) :
    m_source(source),
    m_path(source.getLocation()),
    m_myReader(false),
    m_startFrame(0),
    m_fillThread(0),
    m_updateTimer(0),
    m_lastFillExtent(0),
    m_exiting(false)
{
    m_reader = reader;
    if (m_reader) setObjectName(m_reader->getTitle());
    if (objectName() == "") setObjectName(QFileInfo(m_path).fileName());
    fillCache();
}

WaveFileModel::~WaveFileModel()
{
    m_exiting = true;
    if (m_fillThread) m_fillThread->wait();
    if (m_myReader) delete m_reader;
    m_reader = 0;
}

bool
WaveFileModel::isOK() const
{
    return m_reader && m_reader->isOK();
}

bool
WaveFileModel::isReady(int *completion) const
{
    bool ready = (isOK() && (m_fillThread == 0));
    double c = double(m_lastFillExtent) / double(getEndFrame() - getStartFrame());
    static int prevCompletion = 0;
    if (completion) {
        *completion = int(c * 100.0 + 0.01);
        if (m_reader) {
            int decodeCompletion = m_reader->getDecodeCompletion();
            if (decodeCompletion < 90) *completion = decodeCompletion;
            else *completion = std::min(*completion, decodeCompletion);
        }
        if (*completion != 0 &&
            *completion != 100 &&
            prevCompletion != 0 &&
            prevCompletion > *completion) {
            // just to avoid completion going backwards
            *completion = prevCompletion;
        }
        prevCompletion = *completion;
    }
#ifdef DEBUG_WAVE_FILE_MODEL
    std::cerr << "WaveFileModel::isReady(): ready = " << ready << ", completion = " << (completion ? *completion : -1) << std::endl;
#endif
    return ready;
}

Model *
WaveFileModel::clone() const
{
    WaveFileModel *model = new WaveFileModel(m_source);
    return model;
}

size_t
WaveFileModel::getFrameCount() const
{
    if (!m_reader) return 0;
    return m_reader->getFrameCount();
}

size_t
WaveFileModel::getChannelCount() const
{
    if (!m_reader) return 0;
    return m_reader->getChannelCount();
}

size_t
WaveFileModel::getSampleRate() const 
{
    if (!m_reader) return 0;
    return m_reader->getSampleRate();
}

size_t
WaveFileModel::getNativeRate() const 
{
    if (!m_reader) return 0;
    size_t rate = m_reader->getNativeRate();
    if (rate == 0) rate = getSampleRate();
    return rate;
}

QString
WaveFileModel::getTitle() const
{
    QString title;
    if (m_reader) title = m_reader->getTitle();
    if (title == "") title = objectName();
    return title;
}

QString
WaveFileModel::getMaker() const
{
    if (m_reader) return m_reader->getMaker();
    return "";
}

QString
WaveFileModel::getLocation() const
{
    if (m_reader) return m_reader->getLocation();
    return "";
}
    
size_t
WaveFileModel::getData(int channel, size_t start, size_t count,
                       float *buffer) const
{
    // Always read these directly from the file. 
    // This is used for e.g. audio playback.
    // Could be much more efficient (although compiler optimisation will help)

#ifdef DEBUG_WAVE_FILE_MODEL
    std::cout << "WaveFileModel::getData[" << this << "]: " << channel << ", " << start << ", " << count << ", " << buffer << std::endl;
#endif

    if (start >= m_startFrame) {
        start -= m_startFrame;
    } else {
        for (size_t i = 0; i < count; ++i) buffer[i] = 0.f;
        if (count <= m_startFrame - start) {
            return 0;
        } else {
            count -= (m_startFrame - start);
            start = 0;
        }
    }

    if (!m_reader || !m_reader->isOK() || count == 0) {
        for (size_t i = 0; i < count; ++i) buffer[i] = 0.f;
        return 0;
    }

#ifdef DEBUG_WAVE_FILE_MODEL
//    std::cerr << "WaveFileModel::getValues(" << channel << ", "
//              << start << ", " << end << "): calling reader" << std::endl;
#endif

    int channels = getChannelCount();

    SampleBlock frames(count * channels);
    m_reader->getInterleavedFrames(start, count, frames);

    size_t i = 0;

    int ch0 = channel, ch1 = channel;
    if (channel == -1) {
	ch0 = 0;
	ch1 = channels - 1;
    }
    
    while (i < count) {

	buffer[i] = 0.0;

	for (int ch = ch0; ch <= ch1; ++ch) {

	    size_t index = i * channels + ch;
	    if (index >= frames.size()) break;
            
	    float sample = frames[index];
	    buffer[i] += sample;
	}

	++i;
    }

    return i;
}

size_t
WaveFileModel::getData(int channel, size_t start, size_t count,
                       double *buffer) const
{
#ifdef DEBUG_WAVE_FILE_MODEL
    std::cout << "WaveFileModel::getData(double)[" << this << "]: " << channel << ", " << start << ", " << count << ", " << buffer << std::endl;
#endif

    if (start > m_startFrame) {
        start -= m_startFrame;
    } else {
        for (size_t i = 0; i < count; ++i) buffer[i] = 0.0;
        if (count <= m_startFrame - start) {
            return 0;
        } else {
            count -= (m_startFrame - start);
            start = 0;
        }
    }

    if (!m_reader || !m_reader->isOK() || count == 0) {
        for (size_t i = 0; i < count; ++i) buffer[i] = 0.0;
        return 0;
    }

    int channels = getChannelCount();

    SampleBlock frames(count * channels);
    m_reader->getInterleavedFrames(start, count, frames);

    size_t i = 0;

    int ch0 = channel, ch1 = channel;
    if (channel == -1) {
	ch0 = 0;
	ch1 = channels - 1;
    }

    while (i < count) {

	buffer[i] = 0.0;

	for (int ch = ch0; ch <= ch1; ++ch) {

	    size_t index = i * channels + ch;
	    if (index >= frames.size()) break;
            
	    float sample = frames[index];
	    buffer[i] += sample;
	}

	++i;
    }

    return i;
}

size_t
WaveFileModel::getData(size_t fromchannel, size_t tochannel,
                       size_t start, size_t count,
                       float **buffer) const
{
#ifdef DEBUG_WAVE_FILE_MODEL
    std::cout << "WaveFileModel::getData[" << this << "]: " << fromchannel << "," << tochannel << ", " << start << ", " << count << ", " << buffer << std::endl;
#endif

    size_t channels = getChannelCount();

    if (fromchannel > tochannel) {
        std::cerr << "ERROR: WaveFileModel::getData: fromchannel ("
                  << fromchannel << ") > tochannel (" << tochannel << ")"
                  << std::endl;
        return 0;
    }

    if (tochannel >= channels) {
        std::cerr << "ERROR: WaveFileModel::getData: tochannel ("
                  << tochannel << ") >= channel count (" << channels << ")"
                  << std::endl;
        return 0;
    }

    if (fromchannel == tochannel) {
        return getData(fromchannel, start, count, buffer[0]);
    }

    size_t reqchannels = (tochannel - fromchannel) + 1;

    // Always read these directly from the file. 
    // This is used for e.g. audio playback.
    // Could be much more efficient (although compiler optimisation will help)

    if (start >= m_startFrame) {
        start -= m_startFrame;
    } else {
        for (size_t c = 0; c < reqchannels; ++c) {
            for (size_t i = 0; i < count; ++i) buffer[c][i] = 0.f;
        }
        if (count <= m_startFrame - start) {
            return 0;
        } else {
            count -= (m_startFrame - start);
            start = 0;
        }
    }

    if (!m_reader || !m_reader->isOK() || count == 0) {
        for (size_t c = 0; c < reqchannels; ++c) {
            for (size_t i = 0; i < count; ++i) buffer[c][i] = 0.f;
        }
        return 0;
    }

    SampleBlock frames(count * channels);
    m_reader->getInterleavedFrames(start, count, frames);

    size_t i = 0;

    int ch0 = fromchannel, ch1 = tochannel;
    
    size_t index = 0, available = frames.size();

    while (i < count) {

        if (index >= available) break;

        size_t destc = 0;

        for (size_t c = 0; c < channels; ++c) {
            
            if (c >= fromchannel && c <= tochannel) {
                buffer[destc][i] = frames[index];
                ++destc;
            }

            ++index;
        }

        ++i;
    }

    return i;
}

size_t
WaveFileModel::getSummaryBlockSize(size_t desired) const
{
    int cacheType = 0;
    int power = m_zoomConstraint.getMinCachePower();
    size_t roundedBlockSize = m_zoomConstraint.getNearestBlockSize
        (desired, cacheType, power, ZoomConstraint::RoundDown);
    if (cacheType != 0 && cacheType != 1) {
        // We will be reading directly from file, so can satisfy any
        // blocksize requirement
        return desired;
    } else {
        return roundedBlockSize;
    }
}    

void
WaveFileModel::getSummaries(size_t channel, size_t start, size_t count,
                            RangeBlock &ranges, size_t &blockSize) const
{
    ranges.clear();
    if (!isOK()) return;
    ranges.reserve((count / blockSize) + 1);

    if (start > m_startFrame) start -= m_startFrame;
    else if (count <= m_startFrame - start) return;
    else {
        count -= (m_startFrame - start);
        start = 0;
    }

    int cacheType = 0;
    int power = m_zoomConstraint.getMinCachePower();
    size_t roundedBlockSize = m_zoomConstraint.getNearestBlockSize
        (blockSize, cacheType, power, ZoomConstraint::RoundDown);

    size_t channels = getChannelCount();

    if (cacheType != 0 && cacheType != 1) {

	// We need to read directly from the file.  We haven't got
	// this cached.  Hope the requested area is small.  This is
	// not optimal -- we'll end up reading the same frames twice
	// for stereo files, in two separate calls to this method.
	// We could fairly trivially handle this for most cases that
	// matter by putting a single cache in getInterleavedFrames
	// for short queries.

        m_directReadMutex.lock();

        if (m_lastDirectReadStart != start ||
            m_lastDirectReadCount != count ||
            m_directRead.empty()) {

            m_reader->getInterleavedFrames(start, count, m_directRead);
            m_lastDirectReadStart = start;
            m_lastDirectReadCount = count;
        }

	float max = 0.0, min = 0.0, total = 0.0;
	size_t i = 0, got = 0;

	while (i < count) {

	    size_t index = i * channels + channel;
	    if (index >= m_directRead.size()) break;
            
	    float sample = m_directRead[index];
            if (sample > max || got == 0) max = sample;
	    if (sample < min || got == 0) min = sample;
            total += fabsf(sample);
	    
	    ++i;
            ++got;
            
            if (got == blockSize) {
                ranges.push_back(Range(min, max, total / got));
                min = max = total = 0.0f;
                got = 0;
	    }
	}

        m_directReadMutex.unlock();

	if (got > 0) {
            ranges.push_back(Range(min, max, total / got));
	}

	return;

    } else {

	QMutexLocker locker(&m_mutex);
    
	const RangeBlock &cache = m_cache[cacheType];

        blockSize = roundedBlockSize;

	size_t cacheBlock, div;
        
	if (cacheType == 0) {
	    cacheBlock = (1 << m_zoomConstraint.getMinCachePower());
            div = (1 << power) / cacheBlock;
	} else {
	    cacheBlock = ((unsigned int)((1 << m_zoomConstraint.getMinCachePower()) * sqrt(2.) + 0.01));
            div = ((unsigned int)((1 << power) * sqrt(2.) + 0.01)) / cacheBlock;
	}

	size_t startIndex = start / cacheBlock;
	size_t endIndex = (start + count) / cacheBlock;

	float max = 0.0, min = 0.0, total = 0.0;
	size_t i = 0, got = 0;

#ifdef DEBUG_WAVE_FILE_MODEL
	cerr << "blockSize is " << blockSize << ", cacheBlock " << cacheBlock << ", start " << start << ", count " << count << " (frame count " << getFrameCount() << "), power is " << power << ", div is " << div << ", startIndex " << startIndex << ", endIndex " << endIndex << endl;
#endif

	for (i = 0; i <= endIndex - startIndex; ) {
        
	    size_t index = (i + startIndex) * channels + channel;
	    if (index >= cache.size()) break;
            
            const Range &range = cache[index];
            if (range.max() > max || got == 0) max = range.max();
            if (range.min() < min || got == 0) min = range.min();
            total += range.absmean();
            
	    ++i;
            ++got;
            
	    if (got == div) {
		ranges.push_back(Range(min, max, total / got));
                min = max = total = 0.0f;
                got = 0;
	    }
	}
		
	if (got > 0) {
            ranges.push_back(Range(min, max, total / got));
	}
    }

#ifdef DEBUG_WAVE_FILE_MODEL
    cerr << "returning " << ranges.size() << " ranges" << endl;
#endif
    return;
}

WaveFileModel::Range
WaveFileModel::getSummary(size_t channel, size_t start, size_t count) const
{
    Range range;
    if (!isOK()) return range;

    if (start > m_startFrame) start -= m_startFrame;
    else if (count <= m_startFrame - start) return range;
    else {
        count -= (m_startFrame - start);
        start = 0;
    }

    size_t blockSize;
    for (blockSize = 1; blockSize <= count; blockSize *= 2);
    if (blockSize > 1) blockSize /= 2;

    bool first = false;

    size_t blockStart = (start / blockSize) * blockSize;
    size_t blockEnd = ((start + count) / blockSize) * blockSize;

    if (blockStart < start) blockStart += blockSize;
        
    if (blockEnd > blockStart) {
        RangeBlock ranges;
        getSummaries(channel, blockStart, blockEnd - blockStart, ranges, blockSize);
        for (size_t i = 0; i < ranges.size(); ++i) {
            if (first || ranges[i].min() < range.min()) range.setMin(ranges[i].min());
            if (first || ranges[i].max() > range.max()) range.setMax(ranges[i].max());
            if (first || ranges[i].absmean() < range.absmean()) range.setAbsmean(ranges[i].absmean());
            first = false;
        }
    }

    if (blockStart > start) {
        Range startRange = getSummary(channel, start, blockStart - start);
        range.setMin(std::min(range.min(), startRange.min()));
        range.setMax(std::max(range.max(), startRange.max()));
        range.setAbsmean(std::min(range.absmean(), startRange.absmean()));
    }

    if (blockEnd < start + count) {
        Range endRange = getSummary(channel, blockEnd, start + count - blockEnd);
        range.setMin(std::min(range.min(), endRange.min()));
        range.setMax(std::max(range.max(), endRange.max()));
        range.setAbsmean(std::min(range.absmean(), endRange.absmean()));
    }

    return range;
}

void
WaveFileModel::fillCache()
{
    m_mutex.lock();

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(fillTimerTimedOut()));
    m_updateTimer->start(100);

    m_fillThread = new RangeCacheFillThread(*this);
    connect(m_fillThread, SIGNAL(finished()), this, SLOT(cacheFilled()));

    m_mutex.unlock();
    m_fillThread->start();

#ifdef DEBUG_WAVE_FILE_MODEL
    std::cerr << "WaveFileModel::fillCache: started fill thread" << std::endl;
#endif
}   

void
WaveFileModel::fillTimerTimedOut()
{
    if (m_fillThread) {
	size_t fillExtent = m_fillThread->getFillExtent();
#ifdef DEBUG_WAVE_FILE_MODEL
        cerr << "WaveFileModel::fillTimerTimedOut: extent = " << fillExtent << endl;
#endif
	if (fillExtent > m_lastFillExtent) {
	    emit modelChanged(m_lastFillExtent, fillExtent);
	    m_lastFillExtent = fillExtent;
	}
    } else {
#ifdef DEBUG_WAVE_FILE_MODEL
        cerr << "WaveFileModel::fillTimerTimedOut: no thread" << std::endl;
#endif
	emit modelChanged();
    }
}

void
WaveFileModel::cacheFilled()
{
    m_mutex.lock();
    delete m_fillThread;
    m_fillThread = 0;
    delete m_updateTimer;
    m_updateTimer = 0;
    m_mutex.unlock();
    if (getEndFrame() > m_lastFillExtent) {
        emit modelChanged(m_lastFillExtent, getEndFrame());
    }
    emit modelChanged();
    emit ready();
#ifdef DEBUG_WAVE_FILE_MODEL
    cerr << "WaveFileModel::cacheFilled" << endl;
#endif
}

void
WaveFileModel::RangeCacheFillThread::run()
{
    size_t cacheBlockSize[2];
    cacheBlockSize[0] = (1 << m_model.m_zoomConstraint.getMinCachePower());
    cacheBlockSize[1] = ((unsigned int)((1 << m_model.m_zoomConstraint.getMinCachePower()) *
                                        sqrt(2.) + 0.01));
    
    size_t frame = 0;
    int readBlockSize = 16384;
    SampleBlock block;

    if (!m_model.isOK()) return;
    
    size_t channels = m_model.getChannelCount();
    bool updating = m_model.m_reader->isUpdating();

    if (updating) {
        while (channels == 0 && !m_model.m_exiting) {
//            std::cerr << "WaveFileModel::fill: Waiting for channels..." << std::endl;
            sleep(1);
            channels = m_model.getChannelCount();
        }
    }

    Range *range = new Range[2 * channels];
    float *means = new float[2 * channels];
    size_t count[2];
    count[0] = count[1] = 0;
    for (int i = 0; i < 2 * channels; ++i) {
        means[i] = 0.f;
    }

    bool first = true;

    while (first || updating) {

        updating = m_model.m_reader->isUpdating();
        m_frameCount = m_model.getFrameCount();

//        std::cerr << "WaveFileModel::fill: frame = " << frame << ", count = " << m_frameCount << std::endl;

        while (frame < m_frameCount) {

//            std::cerr << "WaveFileModel::fill inner loop: frame = " << frame << ", count = " << m_frameCount << ", blocksize " << readBlockSize << std::endl;

            if (updating && (frame + readBlockSize > m_frameCount)) break;

            m_model.m_reader->getInterleavedFrames(frame, readBlockSize, block);

//            std::cerr << "block is " << block.size() << std::endl;

            for (int i = 0; i < readBlockSize; ++i) {
		
                if (channels * i + channels > block.size()) break;

                for (int ch = 0; ch < channels; ++ch) {

                    int index = channels * i + ch;
                    float sample = block[index];
                    
                    for (int ct = 0; ct < 2; ++ct) { // cache type
                        
                        int rangeIndex = ch * 2 + ct;
                        
                        if (sample > range[rangeIndex].max() || count[ct] == 0) {
                            range[rangeIndex].setMax(sample);
                        }
                        if (sample < range[rangeIndex].min() || count[ct] == 0) {
                            range[rangeIndex].setMin(sample);
                        }

                        means[rangeIndex] += fabsf(sample);
                    }
                }
                
                QMutexLocker locker(&m_model.m_mutex);

                for (size_t ct = 0; ct < 2; ++ct) {

                    if (++count[ct] == cacheBlockSize[ct]) {
                        
                        for (size_t ch = 0; ch < size_t(channels); ++ch) {
                            size_t rangeIndex = ch * 2 + ct;
                            means[rangeIndex] /= count[ct];
                            range[rangeIndex].setAbsmean(means[rangeIndex]);
                            m_model.m_cache[ct].push_back(range[rangeIndex]);
                            range[rangeIndex] = Range();
                            means[rangeIndex] = 0.f;
                        }

                        count[ct] = 0;
                    }
                }
                
                ++frame;
            }
            
            if (m_model.m_exiting) break;
            
            m_fillExtent = frame;
        }

//        std::cerr << "WaveFileModel: inner loop ended" << std::endl;

        first = false;
        if (m_model.m_exiting) break;
        if (updating) {
//            std::cerr << "sleeping..." << std::endl;
            sleep(1);
        }
    }

    if (!m_model.m_exiting) {

        QMutexLocker locker(&m_model.m_mutex);

        for (size_t ct = 0; ct < 2; ++ct) {

            if (count[ct] > 0) {

                for (size_t ch = 0; ch < size_t(channels); ++ch) {
                    size_t rangeIndex = ch * 2 + ct;
                    means[rangeIndex] /= count[ct];
                    range[rangeIndex].setAbsmean(means[rangeIndex]);
                    m_model.m_cache[ct].push_back(range[rangeIndex]);
                    range[rangeIndex] = Range();
                    means[rangeIndex] = 0.f;
                }

                count[ct] = 0;
            }
            
            const Range &rr = *m_model.m_cache[ct].begin();
            MUNLOCK(&rr, m_model.m_cache[ct].capacity() * sizeof(Range));
        }
    }
    
    delete[] means;
    delete[] range;

    m_fillExtent = m_frameCount;

#ifdef DEBUG_WAVE_FILE_MODEL        
    for (size_t ct = 0; ct < 2; ++ct) {
        cerr << "Cache type " << ct << " now contains " << m_model.m_cache[ct].size() << " ranges" << endl;
    }
#endif
}

void
WaveFileModel::toXml(QTextStream &out,
                     QString indent,
                     QString extraAttributes) const
{
    Model::toXml(out, indent,
                 QString("type=\"wavefile\" file=\"%1\" %2")
                 .arg(encodeEntities(m_path)).arg(extraAttributes));
}

    
