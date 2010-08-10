/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2009 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _FFT_FILE_CACHE_READER_H_
#define _FFT_FILE_CACHE_READER_H_

#include "data/fileio/MatrixFile.h"
#include "FFTCacheReader.h"
#include "FFTCacheStorageType.h"

class FFTFileCacheWriter;

class FFTFileCacheReader : public FFTCacheReader
{
public:
    FFTFileCacheReader(FFTFileCacheWriter *);
    ~FFTFileCacheReader();

    size_t getWidth() const;
    size_t getHeight() const;
	
    float getMagnitudeAt(size_t x, size_t y) const;
    float getNormalizedMagnitudeAt(size_t x, size_t y) const;
    float getMaximumMagnitudeAt(size_t x) const;
    float getPhaseAt(size_t x, size_t y) const;

    void getValuesAt(size_t x, size_t y, float &real, float &imag) const;
    void getMagnitudesAt(size_t x, float *values, size_t minbin, size_t count, size_t step) const;

    bool haveSetColumnAt(size_t x) const;

    static size_t getCacheSize(size_t width, size_t height,
                               FFTCache::StorageType type);

    FFTCache::StorageType getStorageType() const { return m_storageType; }

protected:
    mutable char *m_readbuf;
    mutable size_t m_readbufCol;
    mutable size_t m_readbufWidth;
    mutable bool m_readbufGood;

    float getFromReadBufStandard(size_t x, size_t y) const {
        float v;
        if (m_readbuf &&
            (m_readbufCol == x || (m_readbufWidth > 1 && m_readbufCol+1 == x))) {
            v = ((float *)m_readbuf)[(x - m_readbufCol) * m_mfc->getHeight() + y];
            return v;
        } else {
            populateReadBuf(x);
            v = getFromReadBufStandard(x, y);
            return v;
        }
    }

    float getFromReadBufCompactUnsigned(size_t x, size_t y) const {
        float v;
        if (m_readbuf &&
            (m_readbufCol == x || (m_readbufWidth > 1 && m_readbufCol+1 == x))) {
            v = ((uint16_t *)m_readbuf)[(x - m_readbufCol) * m_mfc->getHeight() + y];
            return v;
        } else {
            populateReadBuf(x);
            v = getFromReadBufCompactUnsigned(x, y);
            return v;
        }
    }

    float getFromReadBufCompactSigned(size_t x, size_t y) const {
        float v;
        if (m_readbuf &&
            (m_readbufCol == x || (m_readbufWidth > 1 && m_readbufCol+1 == x))) {
            v = ((int16_t *)m_readbuf)[(x - m_readbufCol) * m_mfc->getHeight() + y];
            return v;
        } else {
            populateReadBuf(x);
            v = getFromReadBufCompactSigned(x, y);
            return v;
        }
    }

    void populateReadBuf(size_t x) const;

    float getNormalizationFactor(size_t col) const {
        size_t h = m_mfc->getHeight();
        if (h < m_factorSize) return 0;
        if (m_storageType != FFTCache::Compact) {
            return getFromReadBufStandard(col, h - 1);
        } else {
            union {
                float f;
                uint16_t u[2];
            } factor;
            if (!m_readbuf ||
                !(m_readbufCol == col ||
                  (m_readbufWidth > 1 && m_readbufCol+1 == col))) {
                populateReadBuf(col);
            }
            size_t ix = (col - m_readbufCol) * m_mfc->getHeight() + h;
            factor.u[0] = ((uint16_t *)m_readbuf)[ix - 2];
            factor.u[1] = ((uint16_t *)m_readbuf)[ix - 1];
            return factor.f;
        }
    }
 
    FFTCache::StorageType m_storageType;
    size_t m_factorSize;
    MatrixFile *m_mfc;
};

#endif
