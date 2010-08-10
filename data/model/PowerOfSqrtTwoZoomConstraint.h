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

#ifndef _POWER_OF_SQRT_TWO_ZOOM_CONSTRAINT_H_
#define _POWER_OF_SQRT_TWO_ZOOM_CONSTRAINT_H_

#include "base/ZoomConstraint.h"

class PowerOfSqrtTwoZoomConstraint : virtual public ZoomConstraint
{
public:
    virtual size_t getNearestBlockSize(size_t requestedBlockSize,
				       RoundingDirection dir = RoundNearest)
	const;
    
    virtual size_t getNearestBlockSize(size_t requestedBlockSize,
				       int &type,
				       int &power,
				       RoundingDirection dir = RoundNearest)
	const;
	
    virtual size_t getMinCachePower() const { return 6; }
};

#endif

