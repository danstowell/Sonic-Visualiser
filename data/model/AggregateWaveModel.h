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

#ifndef _AGGREGATE_WAVE_MODEL_H_
#define _AGGREGATE_WAVE_MODEL_H_

#include "RangeSummarisableTimeValueModel.h"
#include "PowerOfSqrtTwoZoomConstraint.h"

#include <vector>

class AggregateWaveModel : public RangeSummarisableTimeValueModel
{
    Q_OBJECT

public:
    struct ModelChannelSpec
    {
        ModelChannelSpec(RangeSummarisableTimeValueModel *m, int c) :
            model(m), channel(c) { }
        RangeSummarisableTimeValueModel *model;
        int channel;
    };

    typedef std::vector<ModelChannelSpec> ChannelSpecList;

    AggregateWaveModel(ChannelSpecList channelSpecs);
    ~AggregateWaveModel();

    bool isOK() const;
    bool isReady(int *) const;

    QString getTypeName() const { return tr("Aggregate Wave"); }

    size_t getComponentCount() const;
    ModelChannelSpec getComponent(size_t c) const;

    const ZoomConstraint *getZoomConstraint() const { return &m_zoomConstraint; }

    size_t getFrameCount() const;
    size_t getChannelCount() const;
    size_t getSampleRate() const;

    virtual Model *clone() const;

    float getValueMinimum() const { return -1.0f; }
    float getValueMaximum() const { return  1.0f; }

    virtual size_t getStartFrame() const { return 0; }
    virtual size_t getEndFrame() const { return getFrameCount(); }

    virtual size_t getData(int channel, size_t start, size_t count,
                           float *buffer) const;

    virtual size_t getData(int channel, size_t start, size_t count,
                           double *buffer) const;

    virtual size_t getData(size_t fromchannel, size_t tochannel,
                           size_t start, size_t count,
                           float **buffer) const;

    virtual size_t getSummaryBlockSize(size_t desired) const;

    virtual void getSummaries(size_t channel, size_t start, size_t count,
                              RangeBlock &ranges,
                              size_t &blockSize) const;

    virtual Range getSummary(size_t channel, size_t start, size_t count) const;

    virtual void toXml(QTextStream &out,
                       QString indent = "",
                       QString extraAttributes = "") const;

signals:
    void modelChanged();
    void modelChanged(size_t, size_t);
    void completionChanged();

protected slots:
    void componentModelChanged();
    void componentModelChanged(size_t, size_t);
    void componentModelCompletionChanged();

protected:
    ChannelSpecList m_components;
    static PowerOfSqrtTwoZoomConstraint m_zoomConstraint;
};

#endif

