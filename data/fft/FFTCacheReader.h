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

#ifndef _FFT_CACHE_READER_H_
#define _FFT_CACHE_READER_H_

#include "FFTCacheStorageType.h"
#include <stddef.h>

class FFTCacheReader
{
public:
    virtual ~FFTCacheReader() { }

    virtual size_t getWidth() const = 0;
    virtual size_t getHeight() const = 0;
	
    virtual float getMagnitudeAt(size_t x, size_t y) const = 0;
    virtual float getNormalizedMagnitudeAt(size_t x, size_t y) const = 0;
    virtual float getMaximumMagnitudeAt(size_t x) const = 0;
    virtual float getPhaseAt(size_t x, size_t y) const = 0;

    virtual void getValuesAt(size_t x, size_t y, float &real, float &imag) const = 0;
    virtual void getMagnitudesAt(size_t x, float *values, size_t minbin, size_t count, size_t step) const = 0;

    virtual bool haveSetColumnAt(size_t x) const = 0;

    virtual FFTCache::StorageType getStorageType() const = 0;
};

#endif
