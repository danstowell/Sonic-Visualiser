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

#ifndef _FFT_FUZZY_ADAPTER_H_
#define _FFT_FUZZY_ADAPTER_H_

#include "FFTDataServer.h"

class FFTFuzzyAdapter
{
public:
    FFTFuzzyAdapter(const DenseTimeValueModel *model,
                    int channel,
                    WindowType windowType,
                    size_t windowSize,
                    size_t windowIncrement,
                    size_t fftSize,
                    bool polar,
                    size_t fillFromColumn = 0);
    ~FFTFuzzyAdapter();

    size_t getWidth() const {
        return m_server->getWidth() >> m_xshift;
    }
    size_t getHeight() const {
        return m_server->getHeight() >> m_yshift;
    }
    float getMagnitudeAt(size_t x, size_t y) {
        return m_server->getMagnitudeAt(x << m_xshift, y << m_yshift);
    }
    float getNormalizedMagnitudeAt(size_t x, size_t y) {
        return m_server->getNormalizedMagnitudeAt(x << m_xshift, y << m_yshift);
    }
    float getMaximumMagnitudeAt(size_t x) {
        return m_server->getMaximumMagnitudeAt(x << m_xshift);
    }
    float getPhaseAt(size_t x, size_t y) {
        return m_server->getPhaseAt(x << m_xshift, y << m_yshift);
    }
    void getValuesAt(size_t x, size_t y, float &real, float &imaginary) {
        m_server->getValuesAt(x << m_xshift, y << m_yshift, real, imaginary);
    }
    bool isColumnReady(size_t x) {
        return m_server->isColumnReady(x << m_xshift);
    }
    bool isLocalPeak(size_t x, size_t y) {
        float mag = getMagnitudeAt(x, y);
        if (y > 0 && mag < getMagnitudeAt(x, y - 1)) return false;
        if (y < getHeight() - 1 && mag < getMagnitudeAt(x, y + 1)) return false;
        return true;
    }
    bool isOverThreshold(size_t x, size_t y, float threshold) {
        return getMagnitudeAt(x, y) > threshold;
    }

    size_t getFillCompletion() const { return m_server->getFillCompletion(); }
    size_t getFillExtent() const { return m_server->getFillExtent(); }

private:
    FFTFuzzyAdapter(const FFTFuzzyAdapter &); // not implemented
    FFTFuzzyAdapter &operator=(const FFTFuzzyAdapter &); // not implemented

    FFTDataServer *m_server;
    int m_xshift;
    int m_yshift;
};

#endif
