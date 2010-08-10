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

#include "LogRange.h"
#include "system/System.h"

#include <algorithm>
#include <iostream>
#include <cmath>

void
LogRange::mapRange(float &min, float &max, float logthresh)
{
    if (min > max) std::swap(min, max);
    if (max == min) max = min + 1;

//    std::cerr << "LogRange::mapRange: min = " << min << ", max = " << max << std::endl;

    if (min >= 0.f) {

        max = log10f(max); // we know max != 0

        if (min == 0.f) min = std::min(logthresh, max);
        else min = log10f(min);

//        std::cerr << "LogRange::mapRange: positive: min = " << min << ", max = " << max << std::endl;

    } else if (max <= 0.f) {
        
        min = log10f(-min); // we know min != 0
        
        if (max == 0.f) max = std::min(logthresh, min);
        else max = log10f(-max);
        
        std::swap(min, max);
        
//        std::cerr << "LogRange::mapRange: negative: min = " << min << ", max = " << max << std::endl;

    } else {
        
        // min < 0 and max > 0
        
        max = log10f(std::max(max, -min));
        min = std::min(logthresh, max);

//        std::cerr << "LogRange::mapRange: spanning: min = " << min << ", max = " << max << std::endl;
    }

    if (min == max) min = max - 1;
}        

float
LogRange::map(float value, float thresh)
{
    if (value == 0.f) return thresh;
    return log10f(fabsf(value));
}

float
LogRange::unmap(float value)
{
    return powf(10.0, value);
}

static float
sd(const std::vector<float> &values, size_t start, size_t n)
{
    float sum = 0.f, mean = 0.f, variance = 0.f;
    for (size_t i = 0; i < n; ++i) {
        sum += values[start + i];
    }
    mean = sum / n;
    for (size_t i = 0; i < n; ++i) {
        float diff = values[start + i] - mean;
        variance += diff * diff;
    }
    variance = variance / n;
    return sqrtf(variance);
}

bool
LogRange::useLogScale(std::vector<float> values)
{
    // Principle: Partition the data into two sets around the median;
    // calculate the standard deviation of each set; if the two SDs
    // are very different, it's likely that a log scale would be good.

    if (values.size() < 4) return false;
    std::sort(values.begin(), values.end());
    size_t mi = values.size() / 2;

    float sd0 = sd(values, 0, mi);
    float sd1 = sd(values, mi, values.size() - mi);

    std::cerr << "LogRange::useLogScale: sd0 = "
              << sd0 << ", sd1 = " << sd1 << std::endl;

    if (sd0 == 0 || sd1 == 0) return false;

    // I wonder what method of determining "one sd much bigger than
    // the other" would be appropriate here...
    if (std::max(sd0, sd1) / std::min(sd0, sd1) > 10.f) return true;
    else return false;
}
    
