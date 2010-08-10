/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "AggregateWaveModel.h"

#include <iostream>

#include <QTextStream>

PowerOfSqrtTwoZoomConstraint
AggregateWaveModel::m_zoomConstraint;

AggregateWaveModel::AggregateWaveModel(ChannelSpecList channelSpecs) :
    m_components(channelSpecs)
{
    for (ChannelSpecList::const_iterator i = channelSpecs.begin();
         i != channelSpecs.end(); ++i) {
        if (i->model->getSampleRate() !=
            channelSpecs.begin()->model->getSampleRate()) {
            std::cerr << "AggregateWaveModel::AggregateWaveModel: WARNING: Component models do not all have the same sample rate" << std::endl;
            break;
        }
    }
}

AggregateWaveModel::~AggregateWaveModel()
{
}

bool
AggregateWaveModel::isOK() const
{
    for (ChannelSpecList::const_iterator i = m_components.begin();
         i != m_components.end(); ++i) {
        if (!i->model->isOK()) return false;
    }
    return true;
}

bool
AggregateWaveModel::isReady(int *completion) const
{
    if (completion) *completion = 100;
    bool ready = true;
    for (ChannelSpecList::const_iterator i = m_components.begin();
         i != m_components.end(); ++i) {
        int completionHere = 100;
        if (!i->model->isReady(&completionHere)) ready = false;
        if (completion && completionHere < *completion) {
            *completion = completionHere;
        }
    }
    return ready;
}

size_t
AggregateWaveModel::getFrameCount() const
{
    size_t count = 0;

    for (ChannelSpecList::const_iterator i = m_components.begin();
         i != m_components.end(); ++i) {
        size_t thisCount = i->model->getEndFrame() - i->model->getStartFrame();
        if (thisCount > count) count = thisCount;
    }

    return count;
}

size_t
AggregateWaveModel::getChannelCount() const
{
    return m_components.size();
}

size_t
AggregateWaveModel::getSampleRate() const
{
    if (m_components.empty()) return 0;
    return m_components.begin()->model->getSampleRate();
}

Model *
AggregateWaveModel::clone() const
{
    return new AggregateWaveModel(m_components);
}

size_t
AggregateWaveModel::getData(int channel, size_t start, size_t count,
                            float *buffer) const
{
    int ch0 = channel, ch1 = channel;
    bool mixing = false;
    if (channel == -1) {
        ch0 = 0;
        ch1 = getChannelCount()-1;
        mixing = true;
    }

    float *readbuf = buffer;
    if (mixing) {
        readbuf = new float[count];
        for (size_t i = 0; i < count; ++i) {
            buffer[i] = 0.f;
        }
    }

    size_t sz = count;

    for (int c = ch0; c <= ch1; ++c) {
        size_t szHere = 
            m_components[c].model->getData(m_components[c].channel,
                                           start, count,
                                           readbuf);
        if (szHere < sz) sz = szHere;
        if (mixing) {
            for (size_t i = 0; i < count; ++i) {
                buffer[i] += readbuf[i];
            }
        }
    }

    if (mixing) delete[] readbuf;
    return sz;
}
         
size_t
AggregateWaveModel::getData(int channel, size_t start, size_t count,
                            double *buffer) const
{
    int ch0 = channel, ch1 = channel;
    bool mixing = false;
    if (channel == -1) {
        ch0 = 0;
        ch1 = getChannelCount()-1;
        mixing = true;
    }

    double *readbuf = buffer;
    if (mixing) {
        readbuf = new double[count];
        for (size_t i = 0; i < count; ++i) {
            buffer[i] = 0.0;
        }
    }

    size_t sz = count;
    
    for (int c = ch0; c <= ch1; ++c) {
        size_t szHere = 
            m_components[c].model->getData(m_components[c].channel,
                                           start, count,
                                           readbuf);
        if (szHere < sz) sz = szHere;
        if (mixing) {
            for (size_t i = 0; i < count; ++i) {
                buffer[i] += readbuf[i];
            }
        }
    }
    
    if (mixing) delete[] readbuf;
    return sz;
}

size_t
AggregateWaveModel::getData(size_t fromchannel, size_t tochannel,
                            size_t start, size_t count,
                            float **buffer) const
{
    size_t min = count;

    for (size_t c = fromchannel; c <= tochannel; ++c) {
        size_t here = getData(c, start, count, buffer[c - fromchannel]);
        if (here < min) min = here;
    }
    
    return min;
}

size_t
AggregateWaveModel::getSummaryBlockSize(size_t desired) const
{
    //!!! complete
    return desired;
}
        
void
AggregateWaveModel::getSummaries(size_t channel, size_t start, size_t count,
                                 RangeBlock &ranges, size_t &blockSize) const
{
    //!!! complete
}

AggregateWaveModel::Range
AggregateWaveModel::getSummary(size_t channel, size_t start, size_t count) const
{
    //!!! complete
    return Range();
}
        
size_t
AggregateWaveModel::getComponentCount() const
{
    return m_components.size();
}

AggregateWaveModel::ModelChannelSpec
AggregateWaveModel::getComponent(size_t c) const
{
    return m_components[c];
}

void
AggregateWaveModel::componentModelChanged()
{
    emit modelChanged();
}

void
AggregateWaveModel::componentModelChanged(size_t start, size_t end)
{
    emit modelChanged(start, end);
}

void
AggregateWaveModel::componentModelCompletionChanged()
{
    emit completionChanged();
}

void
AggregateWaveModel::toXml(QTextStream &out,
                          QString indent,
                          QString extraAttributes) const
{
    //!!! complete
}

