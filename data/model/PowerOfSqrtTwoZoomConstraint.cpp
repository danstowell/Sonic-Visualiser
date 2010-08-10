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

#include "PowerOfSqrtTwoZoomConstraint.h"

#include <iostream>
#include <cmath>


size_t
PowerOfSqrtTwoZoomConstraint::getNearestBlockSize(size_t blockSize,
						  RoundingDirection dir) const
{
    int type, power;
    size_t rv = getNearestBlockSize(blockSize, type, power, dir);
    return rv;
}

size_t
PowerOfSqrtTwoZoomConstraint::getNearestBlockSize(size_t blockSize,
						  int &type, 
						  int &power,
						  RoundingDirection dir) const
{
//    std::cerr << "given " << blockSize << std::endl;

    size_t minCachePower = getMinCachePower();

    if (blockSize < (1U << minCachePower)) {
	type = -1;
	power = 0;
	float val = 1.0, prevVal = 1.0;
	while (val + 0.01 < blockSize) {
	    prevVal = val;
	    val *= sqrt(2.f);
	}
	size_t rval;
	if (dir == RoundUp) rval = size_t(val + 0.01);
	else if (dir == RoundDown) rval = size_t(prevVal + 0.01);
	else if (val - blockSize < blockSize - prevVal) rval = size_t(val + 0.01);
	else rval = size_t(prevVal + 0.01);
//	std::cerr << "returning " << rval << std::endl;
	return rval;
    }

    unsigned int prevBase = (1 << minCachePower);
    unsigned int prevPower = minCachePower;
    unsigned int prevType = 0;

    size_t result = 0;

    for (unsigned int i = 0; ; ++i) {

	power = minCachePower + i/2;
	type = i % 2;

	unsigned int base;
	if (type == 0) {
	    base = (1 << power);
	} else {
	    base = (((unsigned int)((1 << minCachePower) * sqrt(2.) + 0.01))
		    << (power - minCachePower));
	}

//	std::cerr << "Testing base " << base << std::endl;

        if (base == blockSize) {
            result = base;
            break;
        }

	if (base > blockSize) {
	    if (dir == RoundNearest) {
		if (base - blockSize < blockSize - prevBase) {
		    dir = RoundUp;
		} else {
		    dir = RoundDown;
		}
	    }
	    if (dir == RoundUp) {
		result = base;
		break;
	    } else {
		type = prevType;
		power = prevPower;
		result = prevBase;
		break;
	    }
	}

	prevType = type;
	prevPower = power;
	prevBase = base;
    }

    if (result > getMaxZoomLevel()) result = getMaxZoomLevel();
    return result;
}   
