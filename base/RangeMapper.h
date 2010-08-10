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

#ifndef _RANGE_MAPPER_H_
#define _RANGE_MAPPER_H_

#include <QString>


class RangeMapper 
{
public:
    virtual ~RangeMapper() { }
    virtual int getPositionForValue(float value) const = 0;
    virtual float getValueForPosition(int position) const = 0;
    virtual QString getUnit() const { return ""; }
};


class LinearRangeMapper : public RangeMapper
{
public:
    LinearRangeMapper(int minpos, int maxpos,
                      float minval, float maxval,
                      QString unit = "", bool inverted = false);
    
    virtual int getPositionForValue(float value) const;
    virtual float getValueForPosition(int position) const;

    virtual QString getUnit() const { return m_unit; }

protected:
    int m_minpos;
    int m_maxpos;
    float m_minval;
    float m_maxval;
    QString m_unit;
    bool m_inverted;
};


class LogRangeMapper : public RangeMapper
{
public:
    LogRangeMapper(int minpos, int maxpos,
                   float minval, float maxval,
                   QString m_unit = "", bool inverted = false);

    static void convertRatioMinLog(float ratio, float minlog,
                                   int minpos, int maxpos,
                                   float &minval, float &maxval);

    static void convertMinMax(int minpos, int maxpos,
                              float minval, float maxval,
                              float &ratio, float &minlog);

    virtual int getPositionForValue(float value) const;
    virtual float getValueForPosition(int position) const;

    virtual QString getUnit() const { return m_unit; }

protected:
    int m_minpos;
    int m_maxpos;
    float m_ratio;
    float m_minlog;
    float m_maxlog;
    QString m_unit;
    bool m_inverted;
};


#endif
