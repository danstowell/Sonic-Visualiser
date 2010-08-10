/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "Colour3DPlotLayer.h"

#include "view/View.h"
#include "base/Profiler.h"
#include "base/LogRange.h"
#include "base/RangeMapper.h"
#include "ColourMapper.h"

#include <QPainter>
#include <QImage>
#include <QRect>
#include <QTextStream>

#include <iostream>

#include <cassert>

#ifndef __GNUC__
#include <alloca.h>
#endif

//#define DEBUG_COLOUR_3D_PLOT_LAYER_PAINT 1


Colour3DPlotLayer::Colour3DPlotLayer() :
    m_model(0),
    m_cache(0),
    m_peaksCache(0),
    m_peakResolution(256),
    m_cacheValidStart(0),
    m_cacheValidEnd(0),
    m_colourScale(LinearScale),
    m_colourScaleSet(false),
    m_colourMap(0),
    m_gain(1.0),
    m_binScale(LinearBinScale),
    m_normalizeColumns(false),
    m_normalizeVisibleArea(false),
    m_invertVertical(false),
    m_opaque(false),
    m_smooth(false),
    m_miny(0),
    m_maxy(0)
{
    
}

Colour3DPlotLayer::~Colour3DPlotLayer()
{
    delete m_cache;
    delete m_peaksCache;
}

void
Colour3DPlotLayer::setModel(const DenseThreeDimensionalModel *model)
{
    if (m_model == model) return;
    const DenseThreeDimensionalModel *oldModel = m_model;
    m_model = model;
    if (!m_model || !m_model->isOK()) return;

    connectSignals(m_model);

    connect(m_model, SIGNAL(modelChanged()), this, SLOT(modelChanged()));
    connect(m_model, SIGNAL(modelChanged(size_t, size_t)),
	    this, SLOT(modelChanged(size_t, size_t)));

    m_peakResolution = 256;
    if (model->getResolution() > 512) {
        m_peakResolution = 16;
    } else if (model->getResolution() > 128) {
        m_peakResolution = 64;
    } else if (model->getResolution() > 2) {
        m_peakResolution = 128;
    }
    cacheInvalid();

    emit modelReplaced();
    emit sliceableModelReplaced(oldModel, model);
}

void
Colour3DPlotLayer::cacheInvalid()
{
    delete m_cache;
    delete m_peaksCache;
    m_cache = 0;
    m_peaksCache = 0;
    m_cacheValidStart = 0;
    m_cacheValidEnd = 0;
}

void
Colour3DPlotLayer::cacheInvalid(size_t startFrame, size_t endFrame)
{
    if (!m_cache) return;

    size_t modelResolution = m_model->getResolution();
    size_t start = startFrame / modelResolution;
    size_t end = endFrame / modelResolution + 1;
    if (m_cacheValidStart < end) m_cacheValidStart = end;
    if (m_cacheValidEnd > start) m_cacheValidEnd = start;
    if (m_cacheValidStart > m_cacheValidEnd) m_cacheValidEnd = m_cacheValidStart;
}

void
Colour3DPlotLayer::modelChanged()
{
    if (!m_colourScaleSet && m_colourScale == LinearScale) {
        if (m_model) {
            if (m_model->shouldUseLogValueScale()) {
                setColourScale(LogScale);
            } else {
                m_colourScaleSet = true;
            }
        }
    }
    cacheInvalid();
}

void
Colour3DPlotLayer::modelChanged(size_t startFrame, size_t endFrame)
{
    if (!m_colourScaleSet && m_colourScale == LinearScale) {
        if (m_model && m_model->getWidth() > 50) {
            if (m_model->shouldUseLogValueScale()) {
                setColourScale(LogScale);
            } else {
                m_colourScaleSet = true;
            }
        }
    }
    cacheInvalid(startFrame, endFrame);
}

Layer::PropertyList
Colour3DPlotLayer::getProperties() const
{
    PropertyList list;
    list.push_back("Colour");
    list.push_back("Colour Scale");
    list.push_back("Normalize Columns");
    list.push_back("Normalize Visible Area");
    list.push_back("Gain");
    list.push_back("Bin Scale");
    list.push_back("Invert Vertical Scale");
    list.push_back("Opaque");
    list.push_back("Smooth");
    return list;
}

QString
Colour3DPlotLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Colour") return tr("Colour");
    if (name == "Colour Scale") return tr("Scale");
    if (name == "Normalize Columns") return tr("Normalize Columns");
    if (name == "Normalize Visible Area") return tr("Normalize Visible Area");
    if (name == "Invert Vertical Scale") return tr("Invert Vertical Scale");
    if (name == "Gain") return tr("Gain");
    if (name == "Opaque") return tr("Always Opaque");
    if (name == "Smooth") return tr("Smooth");
    if (name == "Bin Scale") return tr("Bin Scale");
    return "";
}

QString
Colour3DPlotLayer::getPropertyIconName(const PropertyName &name) const
{
    if (name == "Normalize Columns") return "normalise-columns";
    if (name == "Normalize Visible Area") return "normalise";
    if (name == "Invert Vertical Scale") return "invert-vertical";
    if (name == "Opaque") return "opaque";
    if (name == "Smooth") return "smooth";
    return "";
}

Layer::PropertyType
Colour3DPlotLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Normalize Columns") return ToggleProperty;
    if (name == "Normalize Visible Area") return ToggleProperty;
    if (name == "Invert Vertical Scale") return ToggleProperty;
    if (name == "Opaque") return ToggleProperty;
    if (name == "Smooth") return ToggleProperty;
    return ValueProperty;
}

QString
Colour3DPlotLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Normalize Columns" ||
        name == "Normalize Visible Area" ||
	name == "Colour Scale" ||
        name == "Gain") return tr("Scale");
    if (name == "Bin Scale" ||
        name == "Invert Vertical Scale") return tr("Bins");
    if (name == "Opaque" ||
        name == "Smooth" ||
        name == "Colour") return tr("Colour");
    return QString();
}

int
Colour3DPlotLayer::getPropertyRangeAndValue(const PropertyName &name,
                                            int *min, int *max, int *deflt) const
{
    int val = 0;

    int garbage0, garbage1, garbage2;
    if (!min) min = &garbage0;
    if (!max) max = &garbage1;
    if (!deflt) deflt = &garbage2;

    if (name == "Gain") {

	*min = -50;
	*max = 50;

        *deflt = lrintf(log10(1.f) * 20.0);;
	if (*deflt < *min) *deflt = *min;
	if (*deflt > *max) *deflt = *max;

	val = lrintf(log10(m_gain) * 20.0);
	if (val < *min) val = *min;
	if (val > *max) val = *max;

    } else if (name == "Colour Scale") {

	*min = 0;
	*max = 3;
        *deflt = (int)LinearScale;

	val = (int)m_colourScale;

    } else if (name == "Colour") {

	*min = 0;
	*max = ColourMapper::getColourMapCount() - 1;
        *deflt = 0;

	val = m_colourMap;

    } else if (name == "Normalize Columns") {
	
        *deflt = 0;
	val = (m_normalizeColumns ? 1 : 0);

    } else if (name == "Normalize Visible Area") {
	
        *deflt = 0;
	val = (m_normalizeVisibleArea ? 1 : 0);

    } else if (name == "Invert Vertical Scale") {
	
        *deflt = 0;
	val = (m_invertVertical ? 1 : 0);

    } else if (name == "Bin Scale") {

	*min = 0;
	*max = 1;
        *deflt = int(LinearBinScale);
	val = (int)m_binScale;

    } else if (name == "Opaque") {
	
        *deflt = 0;
	val = (m_opaque ? 1 : 0);
        
    } else if (name == "Smooth") {
	
        *deflt = 0;
	val = (m_smooth ? 1 : 0);
        
    } else {
	val = Layer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
Colour3DPlotLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == "Colour") {
        return ColourMapper::getColourMapName(value);
    }
    if (name == "Colour Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Log");
	case 2: return tr("+/-1");
	case 3: return tr("Absolute");
	}
    }
    if (name == "Bin Scale") {
	switch (value) {
	default:
	case 0: return tr("Linear");
	case 1: return tr("Log");
	}
    }
    return tr("<unknown>");
}

RangeMapper *
Colour3DPlotLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    if (name == "Gain") {
        return new LinearRangeMapper(-50, 50, -25, 25, tr("dB"));
    }
    return 0;
}

void
Colour3DPlotLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Gain") {
	setGain(pow(10, float(value)/20.0));
    } else if (name == "Colour Scale") {
	switch (value) {
	default:
	case 0: setColourScale(LinearScale); break;
	case 1: setColourScale(LogScale); break;
	case 2: setColourScale(PlusMinusOneScale); break;
	case 3: setColourScale(AbsoluteScale); break;
	}
    } else if (name == "Colour") {
        setColourMap(value);
    } else if (name == "Normalize Columns") {
	setNormalizeColumns(value ? true : false);
    } else if (name == "Normalize Visible Area") {
	setNormalizeVisibleArea(value ? true : false);
    } else if (name == "Invert Vertical Scale") {
	setInvertVertical(value ? true : false);
    } else if (name == "Opaque") {
	setOpaque(value ? true : false);
    } else if (name == "Smooth") {
	setSmooth(value ? true : false);
    } else if (name == "Bin Scale") {
	switch (value) {
	default:
	case 0: setBinScale(LinearBinScale); break;
	case 1: setBinScale(LogBinScale); break;
	}
    }
}

void
Colour3DPlotLayer::setColourScale(ColourScale scale)
{
    if (m_colourScale == scale) return;
    m_colourScale = scale;
    m_colourScaleSet = true;
    cacheInvalid();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    cacheInvalid();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setGain(float gain)
{
    if (m_gain == gain) return;
    m_gain = gain;
    cacheInvalid();
    emit layerParametersChanged();
}

float
Colour3DPlotLayer::getGain() const
{
    return m_gain;
}

void
Colour3DPlotLayer::setBinScale(BinScale binScale)
{
    if (m_binScale == binScale) return;
    m_binScale = binScale;
    cacheInvalid();
    emit layerParametersChanged();
}

Colour3DPlotLayer::BinScale
Colour3DPlotLayer::getBinScale() const
{
    return m_binScale;
}

void
Colour3DPlotLayer::setNormalizeColumns(bool n)
{
    if (m_normalizeColumns == n) return;
    m_normalizeColumns = n;
    cacheInvalid();
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::getNormalizeColumns() const
{
    return m_normalizeColumns;
}

void
Colour3DPlotLayer::setNormalizeVisibleArea(bool n)
{
    if (m_normalizeVisibleArea == n) return;
    m_normalizeVisibleArea = n;
    cacheInvalid();
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::getNormalizeVisibleArea() const
{
    return m_normalizeVisibleArea;
}

void
Colour3DPlotLayer::setInvertVertical(bool n)
{
    if (m_invertVertical == n) return;
    m_invertVertical = n;
    cacheInvalid();
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setOpaque(bool n)
{
    if (m_opaque == n) return;
    m_opaque = n;
    emit layerParametersChanged();
}

void
Colour3DPlotLayer::setSmooth(bool n)
{
    if (m_smooth == n) return;
    m_smooth = n;
    emit layerParametersChanged();
}

bool
Colour3DPlotLayer::getInvertVertical() const
{
    return m_invertVertical;
}

bool
Colour3DPlotLayer::getOpaque() const
{
    return m_opaque;
}

bool
Colour3DPlotLayer::getSmooth() const
{
    return m_smooth;
}

void
Colour3DPlotLayer::setLayerDormant(const View *v, bool dormant)
{
    if (dormant) {

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
        std::cerr << "Colour3DPlotLayer::setLayerDormant(" << dormant << ")"
                  << std::endl;
#endif

        if (isLayerDormant(v)) {
            return;
        }

        Layer::setLayerDormant(v, true);

        cacheInvalid();
	
    } else {

        Layer::setLayerDormant(v, false);
    }
}

bool
Colour3DPlotLayer::isLayerScrollable(const View *v) const
{
    if (m_normalizeVisibleArea) return false;
    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
Colour3DPlotLayer::getValueExtents(float &min, float &max,
                                   bool &logarithmic, QString &unit) const
{
    if (!m_model) return false;

    min = 0;
    max = m_model->getHeight();

    logarithmic = false;
    unit = "";

    return true;
}

bool
Colour3DPlotLayer::getDisplayExtents(float &min, float &max) const
{
    if (!m_model) return false;

    min = m_miny;
    max = m_maxy;
    if (max <= min) {
        min = 0;
        max = m_model->getHeight();
    }
    if (min < 0) min = 0;
    if (max > m_model->getHeight()) max = m_model->getHeight();

    return true;
}

bool
Colour3DPlotLayer::setDisplayExtents(float min, float max)
{
    if (!m_model) return false;

    m_miny = lrintf(min);
    m_maxy = lrintf(max);
    
    emit layerParametersChanged();
    return true;
}

int
Colour3DPlotLayer::getVerticalZoomSteps(int &defaultStep) const
{
    if (!m_model) return 0;

    defaultStep = 0;
    int h = m_model->getHeight();
    return h;
}

int
Colour3DPlotLayer::getCurrentVerticalZoomStep() const
{
    if (!m_model) return 0;

    float min, max;
    getDisplayExtents(min, max);
    return m_model->getHeight() - lrintf(max - min);
}

void
Colour3DPlotLayer::setVerticalZoomStep(int step)
{
    if (!m_model) return;

//    std::cerr << "Colour3DPlotLayer::setVerticalZoomStep(" <<step <<"): before: miny = " << m_miny << ", maxy = " << m_maxy << std::endl;

    int dist = m_model->getHeight() - step;
    if (dist < 1) dist = 1;
    float centre = m_miny + (float(m_maxy) - float(m_miny)) / 2.f;
    m_miny = lrintf(centre - float(dist)/2);
    if (m_miny < 0) m_miny = 0;
    m_maxy = m_miny + dist;
    if (m_maxy > m_model->getHeight()) m_maxy = m_model->getHeight();

//    std::cerr << "Colour3DPlotLayer::setVerticalZoomStep(" <<step <<"):  after: miny = " << m_miny << ", maxy = " << m_maxy << std::endl;
    
    emit layerParametersChanged();
}

RangeMapper *
Colour3DPlotLayer::getNewVerticalZoomRangeMapper() const
{
    if (!m_model) return 0;

    return new LinearRangeMapper(0, m_model->getHeight(),
                                 0, m_model->getHeight(), "");
}

float
Colour3DPlotLayer::getYForBin(View *v, float bin) const
{
    float y = bin;
    if (!m_model) return y;
    float mn = 0, mx = m_model->getHeight();
    getDisplayExtents(mn, mx);
    float h = v->height();
    if (m_binScale == LinearBinScale) {
        y = h - (((bin - mn) * h) / (mx - mn));
    } else {
        float logmin = mn + 1, logmax = mx + 1;
        LogRange::mapRange(logmin, logmax);
        y = h - (((LogRange::map(bin + 1) - logmin) * h) / (logmax - logmin));
    }
    return y;
}

float
Colour3DPlotLayer::getBinForY(View *v, float y) const
{
    float bin = y;
    if (!m_model) return bin;
    float mn = 0, mx = m_model->getHeight();
    getDisplayExtents(mn, mx);
    float h = v->height();
    if (m_binScale == LinearBinScale) {
        bin = mn + ((h - y) * (mx - mn)) / h;
    } else {
        float logmin = mn + 1, logmax = mx + 1;
        LogRange::mapRange(logmin, logmax);
        bin = LogRange::unmap(logmin + ((h - y) * (logmax - logmin)) / h) - 1;
    }
    return bin;
}

QString
Colour3DPlotLayer::getFeatureDescription(View *v, QPoint &pos) const
{
    if (!m_model) return "";

    int x = pos.x();
    int y = pos.y();

    size_t modelStart = m_model->getStartFrame();
    size_t modelResolution = m_model->getResolution();

    float srRatio =
        float(v->getViewManager()->getMainModelSampleRate()) /
        float(m_model->getSampleRate());

    int sx0 = int((v->getFrameForX(x) / srRatio - long(modelStart)) /
                  long(modelResolution));

    int f0 = sx0 * modelResolution;
    int f1 =  f0 + modelResolution;

    int sh = m_model->getHeight();

    int symin = m_miny;
    int symax = m_maxy;
    if (symax <= symin) {
        symin = 0;
        symax = sh;
    }
    if (symin < 0) symin = 0;
    if (symax > sh) symax = sh;

 //    float binHeight = float(v->height()) / (symax - symin);
//    int sy = int((v->height() - y) / binHeight) + symin;

    int sy = getBinForY(v, y);

    if (m_invertVertical) sy = m_model->getHeight() - sy - 1;

    float value = m_model->getValueAt(sx0, sy);

//    std::cerr << "bin value (" << sx0 << "," << sy << ") is " << value << std::endl;
    
    QString binName = m_model->getBinName(sy);
    if (binName == "") binName = QString("[%1]").arg(sy + 1);
    else binName = QString("%1 [%2]").arg(binName).arg(sy + 1);

    QString text = tr("Time:\t%1 - %2\nBin:\t%3\nValue:\t%4")
	.arg(RealTime::frame2RealTime(f0, m_model->getSampleRate())
	     .toText(true).c_str())
	.arg(RealTime::frame2RealTime(f1, m_model->getSampleRate())
	     .toText(true).c_str())
	.arg(binName)
	.arg(value);

    return text;
}

int
Colour3DPlotLayer::getColourScaleWidth(QPainter &) const
{
    int cw = 20;
    return cw;
}

int
Colour3DPlotLayer::getVerticalScaleWidth(View *, QPainter &paint) const
{
    if (!m_model) return 0;

    QString sampleText = QString("[%1]").arg(m_model->getHeight());
    int tw = paint.fontMetrics().width(sampleText);
    bool another = false;

    for (size_t i = 0; i < m_model->getHeight(); ++i) {
	if (m_model->getBinName(i).length() > sampleText.length()) {
	    sampleText = m_model->getBinName(i);
            another = true;
	}
    }
    if (another) {
	tw = std::max(tw, paint.fontMetrics().width(sampleText));
    }

    return tw + 13 + getColourScaleWidth(paint);
}

void
Colour3DPlotLayer::paintVerticalScale(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model) return;

    int h = rect.height(), w = rect.width();

    int cw = getColourScaleWidth(paint);
    
    int ch = h - 20;
    if (ch > 20 && m_cache) {

        float min = m_model->getMinimumLevel();
        float max = m_model->getMaximumLevel();

        float mmin = min;
        float mmax = max;

        if (m_colourScale == LogScale) {
            LogRange::mapRange(mmin, mmax);
        } else if (m_colourScale == PlusMinusOneScale) {
            mmin = -1.f;
            mmax = 1.f;
        } else if (m_colourScale == AbsoluteScale) {
            if (mmin < 0) {
                if (fabsf(mmin) > fabsf(mmax)) mmax = fabsf(mmin);
                else mmax = fabsf(mmax);
                mmin = 0;
            } else {
                mmin = fabsf(mmin);
                mmax = fabsf(mmax);
            }
        }
    
        if (max == min) max = min + 1.0;
        if (mmax == mmin) mmax = mmin + 1.0;
    
        paint.setPen(v->getForeground());
        paint.drawRect(4, 10, cw - 8, ch+1);

        for (int y = 0; y < ch; ++y) {
            float value = ((max - min) * (ch - y - 1)) / ch + min;
            if (m_colourScale == LogScale) {
                value = LogRange::map(value);
            }
            int pixel = int(((value - mmin) * 256) / (mmax - mmin));
            if (pixel >= 0 && pixel < 256) {
                QRgb c = m_cache->color(pixel);
                paint.setPen(QColor(qRed(c), qGreen(c), qBlue(c)));
                paint.drawLine(5, 11 + y, cw - 5, 11 + y);
            } else {
                std::cerr << "WARNING: Colour3DPlotLayer::paintVerticalScale: value " << value << ", mmin " << mmin << ", mmax " << mmax << " leads to invalid pixel " << pixel << std::endl;
            }
        }

        QString minstr = QString("%1").arg(min);
        QString maxstr = QString("%1").arg(max);
        
        paint.save();

        QFont font = paint.font();
        font.setPixelSize(10);
        paint.setFont(font);

        int msw = paint.fontMetrics().width(maxstr);

        QMatrix m;
        m.translate(cw - 6, ch + 10);
        m.rotate(-90);

        paint.setWorldMatrix(m);

        v->drawVisibleText(paint, 2, 0, minstr, View::OutlinedText);

        m.translate(ch - msw - 2, 0);
        paint.setWorldMatrix(m);

        v->drawVisibleText(paint, 0, 0, maxstr, View::OutlinedText);

        paint.restore();
    }

    paint.setPen(v->getForeground());

    int sh = m_model->getHeight();

    int symin = m_miny;
    int symax = m_maxy;
    if (symax <= symin) {
        symin = 0;
        symax = sh;
    }
    if (symin < 0) symin = 0;
    if (symax > sh) symax = sh;

    paint.save();

    int py = h;

    for (size_t i = symin; i <= symax; ++i) {

        int y0;

        y0 = lrintf(getYForBin(v, i));
        int h = py - y0;

        if (i > symin) {
            if (paint.fontMetrics().height() >= h) {
                if (h >= 8) {
                    QFont tf = paint.font();
                    tf.setPixelSize(h-2);
                    paint.setFont(tf);
                } else {
                    continue;
                }
            }
        }
	
        py = y0;

        if (i < symax) {
            paint.drawLine(cw, y0, w, y0);
        }

        if (i > symin) {

            size_t idx = i - 1;
            if (m_invertVertical) idx = m_model->getHeight() - idx - 1;

            QString text = m_model->getBinName(idx);
            if (text == "") text = QString("[%1]").arg(idx + 1);

            int ty = y0 + (h/2) - (paint.fontMetrics().height()/2) +
                paint.fontMetrics().ascent() + 1;

            paint.drawText(cw + 5, ty, text);
        }
    }

    paint.restore();
}

DenseThreeDimensionalModel::Column
Colour3DPlotLayer::getColumn(size_t col) const
{
    DenseThreeDimensionalModel::Column values = m_model->getColumn(col);
    while (values.size() < m_model->getHeight()) values.push_back(0.f);
    if (!m_normalizeColumns) return values;

    float colMax = 0.f, colMin = 0.f;
    float min = 0.f, max = 0.f;

    min = m_model->getMinimumLevel();
    max = m_model->getMaximumLevel();

    for (size_t y = 0; y < values.size(); ++y) {
        if (y == 0 || values.at(y) > colMax) colMax = values.at(y);
        if (y == 0 || values.at(y) < colMin) colMin = values.at(y);
    }
    if (colMin == colMax) colMax = colMin + 1;
    
    for (size_t y = 0; y < values.size(); ++y) {
    
        float value = values.at(y);
        float norm = (value - colMin) / (colMax - colMin);
        float newvalue = min + (max - min) * norm;

        if (value != newvalue) values[y] = newvalue;
    }

    return values;
}
    
void
Colour3DPlotLayer::fillCache(size_t firstBin, size_t lastBin) const
{
    Profiler profiler("Colour3DPlotLayer::fillCache");

    size_t modelStart = m_model->getStartFrame();
    size_t modelEnd = m_model->getEndFrame();
    size_t modelResolution = m_model->getResolution();

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    std::cerr << "Colour3DPlotLayer::fillCache: " << firstBin << " -> " << lastBin << std::endl;
#endif

    size_t modelStartBin = modelStart / modelResolution;
    size_t modelEndBin = modelEnd / modelResolution;

    size_t cacheWidth = modelEndBin - modelStartBin + 1;
    if (lastBin > modelEndBin) cacheWidth = lastBin - modelStartBin + 1;
    size_t cacheHeight = m_model->getHeight();

    if (m_cache && (m_cache->height() != int(cacheHeight))) {
        delete m_cache;
        delete m_peaksCache;
        m_cache = 0;
        m_peaksCache = 0;
    } 

    if (m_cache && (m_cache->width() != int(cacheWidth))) {
        QImage *newCache =
            new QImage(m_cache->copy(0, 0, cacheWidth, cacheHeight));
        delete m_cache;
        m_cache = newCache;
        if (m_peaksCache) {
            QImage *newPeaksCache =
                new QImage(m_peaksCache->copy
                           (0, 0, cacheWidth / m_peakResolution, cacheHeight));
            delete m_peaksCache;
            m_peaksCache = newPeaksCache;
        }
    }

    if (!m_cache) {
        m_cache = new QImage
            (cacheWidth, cacheHeight, QImage::Format_Indexed8);
        m_cache->setNumColors(256);
        m_cache->fill(0);
        if (!m_normalizeVisibleArea) {
            m_peaksCache = new QImage
                (cacheWidth / m_peakResolution + 1, cacheHeight,
                 QImage::Format_Indexed8);
            m_peaksCache->setNumColors(256);
            m_peaksCache->fill(0);
        } else if (m_peaksCache) {
            delete m_peaksCache;
            m_peaksCache = 0;
        }
        m_cacheValidStart = 0;
        m_cacheValidEnd = 0;
    }

    if (m_cacheValidStart <= firstBin && m_cacheValidEnd >= lastBin) {
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
        std::cerr << "Cache is valid in this region already" << std::endl;
#endif
        return;
    }
    
    size_t fillStart = firstBin;
    size_t fillEnd = lastBin;

    if (fillStart < modelStartBin) fillStart = modelStartBin;
    if (fillStart > modelEndBin) fillStart = modelEndBin;
    if (fillEnd < modelStartBin) fillEnd = modelStartBin;
    if (fillEnd > modelEndBin) fillEnd = modelEndBin;

    bool normalizeVisible = (m_normalizeVisibleArea && !m_normalizeColumns);

    if (!normalizeVisible && (m_cacheValidStart < m_cacheValidEnd)) {
        
        if (m_cacheValidEnd < fillStart) {
            fillStart = m_cacheValidEnd + 1;
        }
        if (m_cacheValidStart > fillEnd) {
            fillEnd = m_cacheValidStart - 1;
        }
        
        m_cacheValidStart = std::min(fillStart, m_cacheValidStart);
        m_cacheValidEnd = std::max(fillEnd, m_cacheValidEnd);

    } else {

        // the only valid area, ever, is the currently visible one

        m_cacheValidStart = fillStart;
        m_cacheValidEnd = fillEnd;
    }

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    std::cerr << "Cache size " << cacheWidth << "x" << cacheHeight << " will be valid from " << m_cacheValidStart << " to " << m_cacheValidEnd << std::endl;
#endif

    DenseThreeDimensionalModel::Column values;

    float min = m_model->getMinimumLevel();
    float max = m_model->getMaximumLevel();

    if (m_colourScale == LogScale) {
        LogRange::mapRange(min, max);
    } else if (m_colourScale == PlusMinusOneScale) {
        min = -1.f;
        max = 1.f;
    } else if (m_colourScale == AbsoluteScale) {
        if (min < 0) {
            if (fabsf(min) > fabsf(max)) max = fabsf(min);
            else max = fabsf(max);
            min = 0;
        } else {
            min = fabsf(min);
            max = fabsf(max);
        }
    }
    
    if (max == min) max = min + 1.0;
    
    ColourMapper mapper(m_colourMap, 0.f, 255.f);
    
    for (int index = 0; index < 256; ++index) {
        QColor colour = mapper.map(index);
        m_cache->setColor
            (index, qRgb(colour.red(), colour.green(), colour.blue()));
        if (m_peaksCache) {
            m_peaksCache->setColor
                (index, qRgb(colour.red(), colour.green(), colour.blue()));
        }
    }
    
    float visibleMax = 0.f, visibleMin = 0.f;

    if (normalizeVisible) {
        
        for (size_t c = fillStart; c <= fillEnd; ++c) {
	
            values = getColumn(c);

            float colMax = 0.f, colMin = 0.f;

            for (size_t y = 0; y < cacheHeight; ++y) {
                if (y >= values.size()) break;
                if (y == 0 || values[y] > colMax) colMax = values[y];
                if (y == 0 || values[y] < colMin) colMin = values[y];
            }

            if (c == fillStart || colMax > visibleMax) visibleMax = colMax;
            if (c == fillStart || colMin < visibleMin) visibleMin = colMin;
        }

        if (m_colourScale == LogScale) {
            visibleMin = LogRange::map(visibleMin);
            visibleMax = LogRange::map(visibleMax);
            if (visibleMin > visibleMax) std::swap(visibleMin, visibleMax);
        } else if (m_colourScale == AbsoluteScale) {
            if (visibleMin < 0) {
                if (fabsf(visibleMin) > fabsf(visibleMax)) visibleMax = fabsf(visibleMin);
                else visibleMax = fabsf(visibleMax);
                visibleMin = 0;
            } else {
                visibleMin = fabsf(visibleMin);
                visibleMax = fabsf(visibleMax);
            }
        }
    }
    
    if (visibleMin == visibleMax) visibleMax = visibleMin + 1;

    int *peaks = 0;
    if (m_peaksCache) {
        peaks = new int[cacheHeight];
        for (int y = 0; y < cacheHeight; ++y) {
            peaks[y] = 0;
        }
    }

    for (size_t c = fillStart; c <= fillEnd; ++c) {
	
        values = getColumn(c);

        for (size_t y = 0; y < cacheHeight; ++y) {

            float value = min;
            if (y < values.size()) {
                value = values.at(y);
            }

            value = value * m_gain;

            if (m_colourScale == LogScale) {
                value = LogRange::map(value);
            } else if (m_colourScale == AbsoluteScale) {
                value = fabsf(value);
            }
            
            if (normalizeVisible) {
                float norm = (value - visibleMin) / (visibleMax - visibleMin);
                value = min + (max - min) * norm;
            }

            int pixel = int(((value - min) * 256) / (max - min));
            if (pixel < 0) pixel = 0;
            if (pixel > 255) pixel = 255;
            if (peaks && (pixel > peaks[y])) peaks[y] = pixel;

            if (m_invertVertical) {
                m_cache->setPixel(c, cacheHeight - y - 1, pixel);
            } else {
                m_cache->setPixel(c, y, pixel);
            }
        }

        if (peaks) {
            size_t notch = (c % m_peakResolution);
            if (notch == m_peakResolution-1 || c == fillEnd) {
                size_t pc = c / m_peakResolution;
                for (size_t y = 0; y < cacheHeight; ++y) {
                    if (m_invertVertical) {
                        m_peaksCache->setPixel(pc, cacheHeight - y - 1, peaks[y]);
                    } else {
                        m_peaksCache->setPixel(pc, y, peaks[y]);
                    }
                }
                for (int y = 0; y < cacheHeight; ++y) {
                    peaks[y] = 0;
                }
            }
        }
    }

    delete[] peaks;
}

void
Colour3DPlotLayer::paint(View *v, QPainter &paint, QRect rect) const
{
/*
    if (m_model) {
        std::cerr << "Colour3DPlotLayer::paint: model says shouldUseLogValueScale = " << m_model->shouldUseLogValueScale() << std::endl;
    }
*/
    Profiler profiler("Colour3DPlotLayer::paint");
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    std::cerr << "Colour3DPlotLayer::paint(): m_model is " << m_model << ", zoom level is " << v->getZoomLevel() << std::endl;
#endif

    int completion = 0;
    if (!m_model || !m_model->isOK() || !m_model->isReady(&completion)) {
	if (completion > 0) {
	    paint.fillRect(0, 10, v->width() * completion / 100,
			   10, QColor(120, 120, 120));
	}
	return;
    }

    if (m_normalizeVisibleArea && !m_normalizeColumns) rect = v->rect();

    size_t modelStart = m_model->getStartFrame();
    size_t modelEnd = m_model->getEndFrame();
    size_t modelResolution = m_model->getResolution();

    // The cache is from the model's start frame to the model's end
    // frame at the model's window increment frames per pixel.  We
    // want to draw from our start frame + x0 * zoomLevel to our start
    // frame + x1 * zoomLevel at zoomLevel frames per pixel.

    //  We have quite different paint mechanisms for rendering "large"
    //  bins (more than one bin per pixel in both directions) and
    //  "small".  This is "large"; see paintDense below for "small".

    int x0 = rect.left();
    int x1 = rect.right() + 1;

    int h = v->height();

    float srRatio =
        float(v->getViewManager()->getMainModelSampleRate()) /
        float(m_model->getSampleRate());

    int sx0 = int((v->getFrameForX(x0) / srRatio - long(modelStart))
                  / long(modelResolution));
    int sx1 = int((v->getFrameForX(x1) / srRatio - long(modelStart))
                  / long(modelResolution));
    int sh = m_model->getHeight();

    int symin = m_miny;
    int symax = m_maxy;
    if (symax <= symin) {
        symin = 0;
        symax = sh;
    }
    if (symin < 0) symin = 0;
    if (symax > sh) symax = sh;

    if (sx0 > 0) --sx0;
    fillCache(sx0 < 0 ? 0 : sx0,
              sx1 < 0 ? 0 : sx1);

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    std::cerr << "Colour3DPlotLayer::paint: height = "<< m_model->getHeight() << ", modelStart = " << modelStart << ", resolution = " << modelResolution << ", model rate = " << m_model->getSampleRate() << " (zoom level = " << v->getZoomLevel() << ", srRatio = " << srRatio << ")" << std::endl;
#endif

    if (m_opaque || 
        m_smooth ||
        int(m_model->getHeight()) >= v->height() ||
        ((modelResolution * srRatio) / v->getZoomLevel()) < 2) {
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
        std::cerr << "calling paintDense" << std::endl;
#endif
        paintDense(v, paint, rect);
        return;
    }

#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
    std::cerr << "Colour3DPlotLayer::paint: w " << x1-x0 << ", h " << h << ", sx0 " << sx0 << ", sx1 " << sx1 << ", sw " << sx1-sx0 << ", sh " << sh << std::endl;
    std::cerr << "Colour3DPlotLayer: sample rate is " << m_model->getSampleRate() << ", resolution " << m_model->getResolution() << std::endl;
#endif

    QPoint illuminatePos;
    bool illuminate = v->shouldIlluminateLocalFeatures(this, illuminatePos);
    char labelbuf[10];

    for (int sx = sx0; sx <= sx1; ++sx) {

	int fx = sx * int(modelResolution);

	if (fx + int(modelResolution) <= int(modelStart) ||
	    fx > int(modelEnd)) continue;

        int rx0 = v->getXForFrame(int((fx + int(modelStart)) * srRatio));
	int rx1 = v->getXForFrame(int((fx + int(modelStart) + int(modelResolution) + 1) * srRatio));

	int rw = rx1 - rx0;
	if (rw < 1) rw = 1;

	bool showLabel = (rw > 10 &&
			  paint.fontMetrics().width("0.000000") < rw - 3 &&
			  paint.fontMetrics().height() < (h / sh));
        
	for (int sy = symin; sy < symax; ++sy) {

            int ry0 = getYForBin(v, sy);
            int ry1 = getYForBin(v, sy + 1);
            QRect r(rx0, ry1, rw, ry0 - ry1);

	    QRgb pixel = qRgb(255, 255, 255);
	    if (sx >= 0 && sx < m_cache->width() &&
		sy >= 0 && sy < m_cache->height()) {
		pixel = m_cache->pixel(sx, sy);
	    }

            if (rw == 1) {
                paint.setPen(pixel);
                paint.setBrush(Qt::NoBrush);
                paint.drawLine(r.x(), r.y(), r.x(), r.y() + r.height() - 1);
                continue;
            }

	    QColor pen(255, 255, 255, 80);
	    QColor brush(pixel);

            if (rw > 3 && r.height() > 3) {
                brush.setAlpha(160);
            }

	    paint.setPen(Qt::NoPen);
	    paint.setBrush(brush);

	    if (illuminate) {
		if (r.contains(illuminatePos)) {
		    paint.setPen(v->getForeground());
		}
	    }
            
#ifdef DEBUG_COLOUR_3D_PLOT_LAYER_PAINT
//            std::cerr << "rect " << r.x() << "," << r.y() << " "
//                      << r.width() << "x" << r.height() << std::endl;
#endif

	    paint.drawRect(r);

	    if (showLabel) {
		if (sx >= 0 && sx < m_cache->width() &&
		    sy >= 0 && sy < m_cache->height()) {
		    float value = m_model->getValueAt(sx, sy);
		    sprintf(labelbuf, "%06f", value);
		    QString text(labelbuf);
		    paint.setPen(v->getBackground());
		    paint.drawText(rx0 + 2,
				   ry0 - h / sh - 1 + 2 + paint.fontMetrics().ascent(),
				   text);
		}
	    }
	}
    }
}

void
Colour3DPlotLayer::paintDense(View *v, QPainter &paint, QRect rect) const
{
    Profiler profiler("Colour3DPlotLayer::paintDense");
    if (!m_cache) return;

    float modelStart = m_model->getStartFrame();
    float modelResolution = m_model->getResolution();

    int mmsr = v->getViewManager()->getMainModelSampleRate();
    int msr = m_model->getSampleRate();
    float srRatio = float(mmsr) / float(msr);

    int x0 = rect.left();
    int x1 = rect.right() + 1;

    const int w = x1 - x0; // const so it can be used as array size below
    int h = v->height(); // we always paint full height
    int sh = m_model->getHeight();

    int symin = m_miny;
    int symax = m_maxy;
    if (symax <= symin) {
        symin = 0;
        symax = sh;
    }
    if (symin < 0) symin = 0;
    if (symax > sh) symax = sh;

    QImage img(w, h, QImage::Format_Indexed8);
    img.setColorTable(m_cache->colorTable());

    uchar *peaks = new uchar[w];
    memset(peaks, 0, w);

    int zoomLevel = v->getZoomLevel();
    
    QImage *source = m_cache;
    
    std::cerr << "modelResolution " << modelResolution << ", srRatio "
              << srRatio << ", m_peakResolution " << m_peakResolution
              << ", zoomLevel " << zoomLevel << ", result "
              << ((modelResolution * srRatio * m_peakResolution) / zoomLevel)
              << std::endl;

    if (m_peaksCache) {
        if (((modelResolution * srRatio * m_peakResolution) / zoomLevel) < 1) {
            std::cerr << "using peaks cache" << std::endl;
            source = m_peaksCache;
            modelResolution *= m_peakResolution;
        } else {
            std::cerr << "not using peaks cache" << std::endl;
        }
    } else {
        std::cerr << "have no peaks cache" << std::endl;
    }

    int psy1i = -1;
    int sw = source->width();
    
    long xf = -1;
    long nxf = v->getFrameForX(x0);

    float epsilon = 0.000001;

#ifdef __GNUC__
    float sxa[w * 2];
#else
    float *sxa = (float *)alloca(w * 2 * sizeof(float));
#endif
    for (int x = 0; x < w; ++x) {

        xf = nxf;
        nxf = xf + zoomLevel;

        float sx0 = (float(xf) / srRatio - modelStart) / modelResolution;
        float sx1 = (float(nxf) / srRatio - modelStart) / modelResolution;

        sxa[x*2] = sx0;
        sxa[x*2 + 1] = sx1;
    }

    float logmin = symin+1, logmax = symax+1;
    LogRange::mapRange(logmin, logmax);

    if (m_smooth) {
        
        for (int y = 0; y < h; ++y) {

            float sy = getBinForY(v, y) - 0.5;
            int syi = int(sy + epsilon);
            if (syi < 0 || syi >= source->height()) continue;

            uchar *targetLine = img.scanLine(y);
            uchar *sourceLine = source->scanLine(syi);
            uchar *nextSource;
            if (syi + 1 < source->height()) {
                nextSource = source->scanLine(syi + 1);
            } else {
                nextSource = sourceLine;
            }

            for (int x = 0; x < w; ++x) {

                targetLine[x] = 0;

                float sx0 = sxa[x*2];
                if (sx0 < 0) continue;
                int sx0i = int(sx0 + epsilon);
                if (sx0i >= sw) break;

                float a, b, value;

                float sx1 = sxa[x*2+1];
                if (sx1 > sx0 + 1.f) {
                    int sx1i = int(sx1);
                    bool have = false;
                    for (int sx = sx0i; sx <= sx1i; ++sx) {
                        if (sx < 0 || sx >= sw) continue;
                        if (!have) {
                            a = float(sourceLine[sx]);
                            b = float(nextSource[sx]);
                            have = true;
                        } else {
                            a = std::max(a, float(sourceLine[sx]));
                            b = std::max(b, float(nextSource[sx]));
                        }
                    }
                    float yprop = sy - syi;
                    value = (a * (1.f - yprop) + b * yprop);
                } else {
                    a = float(sourceLine[sx0i]);
                    b = float(nextSource[sx0i]);
                    float yprop = sy - syi;
                    value = (a * (1.f - yprop) + b * yprop);
                    int oi = sx0i + 1;
                    float xprop = sx0 - sx0i;
                    xprop -= 0.5;
                    if (xprop < 0) {
                        oi = sx0i - 1;
                        xprop = -xprop;
                    }
                    if (oi < 0 || oi >= sw) oi = sx0i;
                    a = float(sourceLine[oi]);
                    b = float(nextSource[oi]);
                    value = (value * (1.f - xprop) +
                             (a * (1.f - yprop) + b * yprop) * xprop);
                }
                
                int vi = lrintf(value);
                if (vi > 255) vi = 255;
                if (vi < 0) vi = 0;
                targetLine[x] = uchar(vi);
            }
        }
    } else {

        for (int y = 0; y < h; ++y) {

            float sy0, sy1;

            sy0 = getBinForY(v, y + 1);
            sy1 = getBinForY(v, y);

            int sy0i = int(sy0 + epsilon);
            int sy1i = int(sy1);

            uchar *targetLine = img.scanLine(y);

            if (sy0i == sy1i && sy0i == psy1i) { // same source scan line as just computed
                goto copy;
            }

            for (int x = 0; x < w; ++x) {
                peaks[x] = 0;
            }
        
            for (int sy = sy0i; sy <= sy1i; ++sy) {

                if (sy < 0 || sy >= source->height()) continue;

                uchar *sourceLine = source->scanLine(sy);
            
                for (int x = 0; x < w; ++x) {

                    float sx1 = sxa[x*2 + 1];
                    if (sx1 < 0) continue;
                    int sx1i = int(sx1);

                    float sx0 = sxa[x*2];
                    if (sx0 < 0) continue;
                    int sx0i = int(sx0 + epsilon);
                    if (sx0i >= sw) break;

                    uchar peak = 0;
                    for (int sx = sx0i; sx <= sx1i; ++sx) {
                        if (sx < 0 || sx >= sw) continue;
                        if (sourceLine[sx] > peak) peak = sourceLine[sx];
                    }
                    peaks[x] = peak;
                }
            }
        
        copy:
            for (int x = 0; x < w; ++x) {
                targetLine[x] = peaks[x];
            }
        }
    }

    delete[] peaks;

    paint.drawImage(x0, 0, img);
}

bool
Colour3DPlotLayer::snapToFeatureFrame(View *v, int &frame,
				      size_t &resolution,
				      SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    int left = (frame / resolution) * resolution;
    int right = left + resolution;

    switch (snap) {
    case SnapLeft:  frame = left;  break;
    case SnapRight: frame = right; break;
    case SnapNearest:
    case SnapNeighbouring:
	if (frame - left > right - frame) frame = right;
	else frame = left;
	break;
    }
    
    return true;
}

void
Colour3DPlotLayer::toXml(QTextStream &stream,
                         QString indent, QString extraAttributes) const
{
    QString s = QString("scale=\"%1\" "
                        "colourScheme=\"%2\" "
                        "normalizeColumns=\"%3\" "
                        "normalizeVisibleArea=\"%4\" "
                        "minY=\"%5\" "
                        "maxY=\"%6\" "
                        "invertVertical=\"%7\" "
                        "opaque=\"%8\" %9")
	.arg((int)m_colourScale)
        .arg(m_colourMap)
        .arg(m_normalizeColumns ? "true" : "false")
        .arg(m_normalizeVisibleArea ? "true" : "false")
        .arg(m_miny)
        .arg(m_maxy)
        .arg(m_invertVertical ? "true" : "false")
        .arg(m_opaque ? "true" : "false")
        .arg(QString("binScale=\"%1\" smooth=\"%2\" gain=\"%3\" ")
             .arg((int)m_binScale)
             .arg(m_smooth ? "true" : "false")
             .arg(m_gain));
    
    Layer::toXml(stream, indent, extraAttributes + " " + s);
}

void
Colour3DPlotLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false, alsoOk = false;

    ColourScale scale = (ColourScale)attributes.value("scale").toInt(&ok);
    if (ok) setColourScale(scale);

    int colourMap = attributes.value("colourScheme").toInt(&ok);
    if (ok) setColourMap(colourMap);

    BinScale binscale = (BinScale)attributes.value("binScale").toInt(&ok);
    if (ok) setBinScale(binscale);

    bool normalizeColumns =
        (attributes.value("normalizeColumns").trimmed() == "true");
    setNormalizeColumns(normalizeColumns);

    bool normalizeVisibleArea =
        (attributes.value("normalizeVisibleArea").trimmed() == "true");
    setNormalizeVisibleArea(normalizeVisibleArea);

    bool invertVertical =
        (attributes.value("invertVertical").trimmed() == "true");
    setInvertVertical(invertVertical);

    bool opaque =
        (attributes.value("opaque").trimmed() == "true");
    setOpaque(opaque);

    bool smooth =
        (attributes.value("smooth").trimmed() == "true");
    setSmooth(smooth);

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    float min = attributes.value("minY").toFloat(&ok);
    float max = attributes.value("maxY").toFloat(&alsoOk);
    if (ok && alsoOk) setDisplayExtents(min, max);
}

