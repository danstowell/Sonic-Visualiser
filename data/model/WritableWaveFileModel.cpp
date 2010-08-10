/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "WritableWaveFileModel.h"

#include "base/TempDirectory.h"
#include "base/Exceptions.h"

#include "fileio/WavFileWriter.h"
#include "fileio/WavFileReader.h"

#include <QDir>
#include <QTextStream>

#include <cassert>
#include <iostream>

//#define DEBUG_WRITABLE_WAVE_FILE_MODEL 1

WritableWaveFileModel::WritableWaveFileModel(size_t sampleRate,
					     size_t channels,
					     QString path) :
    m_model(0),
    m_writer(0),
    m_reader(0),
    m_sampleRate(sampleRate),
    m_channels(channels),
    m_frameCount(0),
    m_startFrame(0),
    m_completion(0)
{
    if (path.isEmpty()) {
        try {
            QDir dir(TempDirectory::getInstance()->getPath());
            path = dir.filePath(QString("written_%1.wav")
                                .arg((intptr_t)this));
        } catch (DirectoryCreationFailed f) {
            std::cerr << "WritableWaveFileModel: Failed to create temporary directory" << std::endl;
            return;
        }
    }

    m_writer = new WavFileWriter(path, sampleRate, channels);
    if (!m_writer->isOK()) {
        std::cerr << "WritableWaveFileModel: Error in creating WAV file writer: " << m_writer->getError().toStdString() << std::endl;
        delete m_writer; 
        m_writer = 0;
        return;
    }

    FileSource source(m_writer->getPath());

    m_reader = new WavFileReader(source, true);
    if (!m_reader->getError().isEmpty()) {
        std::cerr << "WritableWaveFileModel: Error in creating wave file reader" << std::endl;
        delete m_reader;
        m_reader = 0;
        return;
    }
    
    m_model = new WaveFileModel(source, m_reader);
    if (!m_model->isOK()) {
        std::cerr << "WritableWaveFileModel: Error in creating wave file model" << std::endl;
        delete m_model;
        m_model = 0;
        delete m_reader;
        m_reader = 0;
        return;
    }
    m_model->setStartFrame(m_startFrame);

    connect(m_model, SIGNAL(modelChanged()), this, SIGNAL(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
            this, SIGNAL(modelChanged(size_t, size_t)));
}

WritableWaveFileModel::~WritableWaveFileModel()
{
    delete m_model;
    delete m_writer;
    delete m_reader;
}

void
WritableWaveFileModel::setStartFrame(size_t startFrame)
{
    m_startFrame = startFrame;
    if (m_model) m_model->setStartFrame(startFrame);
}

bool
WritableWaveFileModel::addSamples(float **samples, size_t count)
{
    if (!m_writer) return false;

#ifdef DEBUG_WRITABLE_WAVE_FILE_MODEL
//    std::cerr << "WritableWaveFileModel::addSamples(" << count << ")" << std::endl;
#endif

    if (!m_writer->writeSamples(samples, count)) {
        std::cerr << "ERROR: WritableWaveFileModel::addSamples: writer failed: " << m_writer->getError().toStdString() << std::endl;
        return false;
    }

    m_frameCount += count;

    static int updateCounter = 0;

    if (m_reader && m_reader->getChannelCount() == 0) {
#ifdef DEBUG_WRITABLE_WAVE_FILE_MODEL
        std::cerr << "WritableWaveFileModel::addSamples(" << count << "): calling updateFrameCount (initial)" << std::endl;
#endif
        m_reader->updateFrameCount();
    } else if (++updateCounter == 100) {
#ifdef DEBUG_WRITABLE_WAVE_FILE_MODEL
        std::cerr << "WritableWaveFileModel::addSamples(" << count << "): calling updateFrameCount (periodic)" << std::endl;
#endif
        if (m_reader) m_reader->updateFrameCount();
        updateCounter = 0;
    }

    return true;
}

bool
WritableWaveFileModel::isOK() const
{
    bool ok = (m_writer && m_writer->isOK());
//    std::cerr << "WritableWaveFileModel::isOK(): ok = " << ok << std::endl;
    return ok;
}

bool
WritableWaveFileModel::isReady(int *completion) const
{
    if (completion) *completion = m_completion;
    return (m_completion == 100);
}

void
WritableWaveFileModel::setCompletion(int completion)
{
    m_completion = completion;
    if (completion == 100) {
        if (m_reader) m_reader->updateDone();
    }
}

size_t
WritableWaveFileModel::getFrameCount() const
{
//    std::cerr << "WritableWaveFileModel::getFrameCount: count = " << m_frameCount << std::endl;
    return m_frameCount;
}

Model *
WritableWaveFileModel::clone() const
{
    assert(0); //!!!
    return 0;
}

size_t
WritableWaveFileModel::getData(int channel, size_t start, size_t count,
                               float *buffer) const
{
    if (!m_model || m_model->getChannelCount() == 0) return 0;
    return m_model->getData(channel, start, count, buffer);
}

size_t
WritableWaveFileModel::getData(int channel, size_t start, size_t count,
                               double *buffer) const
{
    if (!m_model || m_model->getChannelCount() == 0) return 0;
    return m_model->getData(channel, start, count, buffer);
}

size_t
WritableWaveFileModel::getData(size_t fromchannel, size_t tochannel,
                               size_t start, size_t count,
                               float **buffers) const
{
    if (!m_model || m_model->getChannelCount() == 0) return 0;
    return m_model->getData(fromchannel, tochannel, start, count, buffers);
}    

size_t
WritableWaveFileModel::getSummaryBlockSize(size_t desired) const
{
    if (!m_model) return desired;
    return m_model->getSummaryBlockSize(desired);
}

void
WritableWaveFileModel::getSummaries(size_t channel, size_t start, size_t count,
                                    RangeBlock &ranges,
                                    size_t &blockSize) const
{
    ranges.clear();
    if (!m_model || m_model->getChannelCount() == 0) return;
    m_model->getSummaries(channel, start, count, ranges, blockSize);
}

WritableWaveFileModel::Range
WritableWaveFileModel::getSummary(size_t channel, size_t start, size_t count) const
{
    if (!m_model || m_model->getChannelCount() == 0) return Range();
    return m_model->getSummary(channel, start, count);
}

void
WritableWaveFileModel::toXml(QTextStream &out,
                             QString indent,
                             QString extraAttributes) const
{
    // We don't actually write the data to XML.  We just write a brief
    // description of the model.  Any code that uses this class is
    // going to need to be aware that it will have to make separate
    // arrangements for the audio file itself.

    Model::toXml
        (out, indent,
         QString("type=\"writablewavefile\" file=\"%1\" channels=\"%2\" %3")
         .arg(encodeEntities(m_writer->getPath()))
         .arg(m_model->getChannelCount()).arg(extraAttributes));
}

