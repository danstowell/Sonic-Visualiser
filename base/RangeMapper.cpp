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

#include "RangeMapper.h"
#include "system/System.h"

#include <cassert>
#include <cmath>

#include <iostream>

LinearRangeMapper::LinearRangeMapper(int minpos, int maxpos,
				     float minval, float maxval,
                                     QString unit, bool inverted) :
    m_minpos(minpos),
    m_maxpos(maxpos),
    m_minval(minval),
    m_maxval(maxval),
    m_unit(unit),
    m_inverted(inverted)
{
    assert(m_maxval != m_minval);
    assert(m_maxpos != m_minpos);
}

int
LinearRangeMapper::getPositionForValue(float value) const
{
    int position = m_minpos +
        lrintf(((value - m_minval) / (m_maxval - m_minval))
               * (m_maxpos - m_minpos));
    if (position < m_minpos) position = m_minpos;
    if (position > m_maxpos) position = m_maxpos;
//    std::cerr << "LinearRangeMapper::getPositionForValue: " << value << " -> "
//              << position << " (minpos " << m_minpos << ", maxpos " << m_maxpos << ", minval " << m_minval << ", maxval " << m_maxval << ")" << std::endl;
    if (m_inverted) return m_maxpos - position;
    else return position;
}

float
LinearRangeMapper::getValueForPosition(int position) const
{
    if (m_inverted) position = m_maxpos - position;
    float value = m_minval +
        ((float(position - m_minpos) / float(m_maxpos - m_minpos))
         * (m_maxval - m_minval));
    if (value < m_minval) value = m_minval;
    if (value > m_maxval) value = m_maxval;
//    std::cerr << "LinearRangeMapper::getValueForPosition: " << position << " -> "
//              << value << " (minpos " << m_minpos << ", maxpos " << m_maxpos << ", minval " << m_minval << ", maxval " << m_maxval << ")" << std::endl;
    return value;
}

LogRangeMapper::LogRangeMapper(int minpos, int maxpos,
                               float minval, float maxval,
                               QString unit, bool inverted) :
    m_minpos(minpos),
    m_maxpos(maxpos),
    m_unit(unit),
    m_inverted(inverted)
{
    convertMinMax(minpos, maxpos, minval, maxval, m_minlog, m_ratio);

    std::cerr << "LogRangeMapper: minpos " << minpos << ", maxpos "
              << maxpos << ", minval " << minval << ", maxval "
              << maxval << ", minlog " << m_minlog << ", ratio " << m_ratio
              << ", unit " << unit.toStdString() << std::endl;

    assert(m_maxpos != m_minpos);

    m_maxlog = (m_maxpos - m_minpos) / m_ratio + m_minlog;
}

void
LogRangeMapper::convertMinMax(int minpos, int maxpos,
                              float minval, float maxval,
                              float &minlog, float &ratio)
{
    static float thresh = powf(10, -10);
    if (minval < thresh) minval = thresh;
    minlog = log10f(minval);
    ratio = (maxpos - minpos) / (log10f(maxval) - minlog);
}

void
LogRangeMapper::convertRatioMinLog(float ratio, float minlog,
                                   int minpos, int maxpos,
                                   float &minval, float &maxval)
{
    minval = powf(10, minlog);
    maxval = powf(10, (maxpos - minpos) / ratio + minlog);
}

int
LogRangeMapper::getPositionForValue(float value) const
{
    int position = (log10(value) - m_minlog) * m_ratio + m_minpos;
    if (position < m_minpos) position = m_minpos;
    if (position > m_maxpos) position = m_maxpos;
//    std::cerr << "LogRangeMapper::getPositionForValue: " << value << " -> "
//              << position << " (minpos " << m_minpos << ", maxpos " << m_maxpos << ", ratio " << m_ratio << ", minlog " << m_minlog << ")" << std::endl;
    if (m_inverted) return m_maxpos - position;
    else return position;
}

float
LogRangeMapper::getValueForPosition(int position) const
{
    if (m_inverted) position = m_maxpos - position;
    float value = powf(10, (position - m_minpos) / m_ratio + m_minlog);
//    std::cerr << "LogRangeMapper::getValueForPosition: " << position << " -> "
//              << value << " (minpos " << m_minpos << ", maxpos " << m_maxpos << ", ratio " << m_ratio << ", minlog " << m_minlog << ")" << std::endl;
    return value;
}

