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

#include "FFTFileCacheReader.h"
#include "FFTFileCacheWriter.h"

#include "fileio/MatrixFile.h"

#include "base/Profiler.h"
#include "base/Thread.h"
#include "base/Exceptions.h"

#include <iostream>


// The underlying matrix has height (m_height * 2 + 1).  In each
// column we store magnitude at [0], [2] etc and phase at [1], [3]
// etc, and then store the normalization factor (maximum magnitude) at
// [m_height * 2].  In compact mode, the factor takes two cells.

FFTFileCacheReader::FFTFileCacheReader(FFTFileCacheWriter *writer) :
    m_readbuf(0),
    m_readbufCol(0),
    m_readbufWidth(0),
    m_readbufGood(false),
    m_storageType(writer->getStorageType()),
    m_factorSize(m_storageType == FFTCache::Compact ? 2 : 1),
    m_mfc(new MatrixFile
          (writer->getFileBase(),
           MatrixFile::ReadOnly,
           m_storageType == FFTCache::Compact ? sizeof(uint16_t) : sizeof(float),
           writer->getWidth(),
           writer->getHeight() * 2 + m_factorSize))
{
//    std::cerr << "FFTFileCacheReader: storage type is " << (storageType == FFTCache::Compact ? "Compact" : storageType == Polar ? "Polar" : "Rectangular") << std::endl;
}

FFTFileCacheReader::~FFTFileCacheReader()
{
    if (m_readbuf) delete[] m_readbuf;
    delete m_mfc;
}

size_t
FFTFileCacheReader::getWidth() const
{
    return m_mfc->getWidth();
}

size_t
FFTFileCacheReader::getHeight() const
{
    size_t mh = m_mfc->getHeight();
    if (mh > m_factorSize) return (mh - m_factorSize) / 2;
    else return 0;
}

float
FFTFileCacheReader::getMagnitudeAt(size_t x, size_t y) const
{
    Profiler profiler("FFTFileCacheReader::getMagnitudeAt", false);

    float value = 0.f;

    switch (m_storageType) {

    case FFTCache::Compact:
        value = (getFromReadBufCompactUnsigned(x, y * 2) / 65535.0)
            * getNormalizationFactor(x);
        break;

    case FFTCache::Rectangular:
    {
        float real, imag;
        getValuesAt(x, y, real, imag);
        value = sqrtf(real * real + imag * imag);
        break;
    }

    case FFTCache::Polar:
        value = getFromReadBufStandard(x, y * 2);
        break;
    }

    return value;
}

float
FFTFileCacheReader::getNormalizedMagnitudeAt(size_t x, size_t y) const
{
    float value = 0.f;

    switch (m_storageType) {

    case FFTCache::Compact:
        value = getFromReadBufCompactUnsigned(x, y * 2) / 65535.0;
        break;

    default:
    {
        float mag = getMagnitudeAt(x, y);
        float factor = getNormalizationFactor(x);
        if (factor != 0) value = mag / factor;
        else value = 0.f;
        break;
    }
    }

    return value;
}

float
FFTFileCacheReader::getMaximumMagnitudeAt(size_t x) const
{
    return getNormalizationFactor(x);
}

float
FFTFileCacheReader::getPhaseAt(size_t x, size_t y) const
{
    float value = 0.f;
    
    switch (m_storageType) {

    case FFTCache::Compact:
        value = (getFromReadBufCompactSigned(x, y * 2 + 1) / 32767.0) * M_PI;
        break;

    case FFTCache::Rectangular:
    {
        float real, imag;
        getValuesAt(x, y, real, imag);
        value = atan2f(imag, real);
        break;
    }

    case FFTCache::Polar:
        value = getFromReadBufStandard(x, y * 2 + 1);
        break;
    }

    return value;
}

void
FFTFileCacheReader::getValuesAt(size_t x, size_t y, float &real, float &imag) const
{
//    std::cerr << "FFTFileCacheReader::getValuesAt(" << x << "," << y << ")" << std::endl;

    switch (m_storageType) {

    case FFTCache::Rectangular:
        real = getFromReadBufStandard(x, y * 2);
        imag = getFromReadBufStandard(x, y * 2 + 1);
        return;

    default:
        float mag = getMagnitudeAt(x, y);
        float phase = getPhaseAt(x, y);
        real = mag * cosf(phase);
        imag = mag * sinf(phase);
        return;
    }
}

void
FFTFileCacheReader::getMagnitudesAt(size_t x, float *values, size_t minbin, size_t count, size_t step) const
{
    Profiler profiler("FFTFileCacheReader::getMagnitudesAt");

    switch (m_storageType) {

    case FFTCache::Compact:
        for (size_t i = 0; i < count; ++i) {
            size_t y = minbin + i * step;
            values[i] = (getFromReadBufCompactUnsigned(x, y * 2) / 65535.0)
                * getNormalizationFactor(x);
        }
        break;

    case FFTCache::Rectangular:
    {
        float real, imag;
        for (size_t i = 0; i < count; ++i) {
            size_t y = minbin + i * step;
            real = getFromReadBufStandard(x, y * 2);
            imag = getFromReadBufStandard(x, y * 2 + 1);
            values[i] = sqrtf(real * real + imag * imag);
        }
        break;
    }

    case FFTCache::Polar:
        for (size_t i = 0; i < count; ++i) {
            size_t y = minbin + i * step;
            values[i] = getFromReadBufStandard(x, y * 2);
        }
        break;
    }
}

bool
FFTFileCacheReader::haveSetColumnAt(size_t x) const
{
    if (m_readbuf && m_readbufGood &&
        (m_readbufCol == x || (m_readbufWidth > 1 && m_readbufCol+1 == x))) {
//        std::cerr << "FFTFileCacheReader::haveSetColumnAt: short-circuiting; we know about this one" << std::endl;
        return true;
    }
    return m_mfc->haveSetColumnAt(x);
}

size_t
FFTFileCacheReader::getCacheSize(size_t width, size_t height,
                                 FFTCache::StorageType type)
{
    return (height * 2 + (type == FFTCache::Compact ? 2 : 1)) * width *
        (type == FFTCache::Compact ? sizeof(uint16_t) : sizeof(float)) +
        2 * sizeof(size_t); // matrix file header size
}

void
FFTFileCacheReader::populateReadBuf(size_t x) const
{
    Profiler profiler("FFTFileCacheReader::populateReadBuf", false);

//    std::cerr << "FFTFileCacheReader::populateReadBuf(" << x << ")" << std::endl;

    if (!m_readbuf) {
        m_readbuf = new char[m_mfc->getHeight() * 2 * m_mfc->getCellSize()];
    }

    m_readbufGood = false;

    try {
        bool good = false;
        if (m_mfc->haveSetColumnAt(x)) {
            // If the column is not available, we have no obligation
            // to do anything with the readbuf -- we can cheerfully
            // return garbage.  It's the responsibility of the caller
            // to check haveSetColumnAt before trusting any retrieved
            // data.  However, we do record whether the data in the
            // readbuf is good or not, because we can use that to
            // return an immediate result for haveSetColumnAt if the
            // column is right.
            good = true;
            m_mfc->getColumnAt(x, m_readbuf);
        }
        if (m_mfc->haveSetColumnAt(x + 1)) {
            m_mfc->getColumnAt
                (x + 1, m_readbuf + m_mfc->getCellSize() * m_mfc->getHeight());
            m_readbufWidth = 2;
        } else {
            m_readbufWidth = 1;
        }
        m_readbufGood = good;
    } catch (FileReadFailed f) {
        std::cerr << "ERROR: FFTFileCacheReader::populateReadBuf: File read failed: "
                  << f.what() << std::endl;
        memset(m_readbuf, 0, m_mfc->getHeight() * 2 * m_mfc->getCellSize());
    }
    m_readbufCol = x;
}

