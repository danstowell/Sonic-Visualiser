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

#include "TimeValueLayer.h"

#include "data/model/Model.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/LogRange.h"
#include "base/RangeMapper.h"
#include "ColourDatabase.h"
#include "view/View.h"

#include "data/model/SparseTimeValueModel.h"
#include "data/model/Labeller.h"

#include "widgets/ItemEditDialog.h"
#include "widgets/ListInputDialog.h"

#include "ColourMapper.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QRegExp>
#include <QTextStream>
#include <QMessageBox>
#include <QInputDialog>

#include <iostream>
#include <cmath>

//#define DEBUG_TIME_VALUE_LAYER 1

TimeValueLayer::TimeValueLayer() :
    SingleColourLayer(),
    m_model(0),
    m_editing(false),
    m_originalPoint(0, 0.0, tr("New Point")),
    m_editingPoint(0, 0.0, tr("New Point")),
    m_editingCommand(0),
    m_colourMap(0),
    m_plotStyle(PlotConnectedPoints),
    m_verticalScale(AutoAlignScale),
    m_drawSegmentDivisions(true),
    m_derivative(false),
    m_scaleMinimum(0),
    m_scaleMaximum(0)
{
    
}

void
TimeValueLayer::setModel(SparseTimeValueModel *model)
{
    if (m_model == model) return;
    m_model = model;

    connectSignals(m_model);

    m_scaleMinimum = 0;
    m_scaleMaximum = 0;

    if (m_model && m_model->getRDFTypeURI().endsWith("Segment")) {
        setPlotStyle(PlotSegmentation);
    }
    if (m_model && m_model->getRDFTypeURI().endsWith("Change")) {
        setPlotStyle(PlotSegmentation);
    }

#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::setModel(" << model << ")" << std::endl;
#endif

    emit modelReplaced();
}

Layer::PropertyList
TimeValueLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Plot Type");
    list.push_back("Vertical Scale");
    list.push_back("Scale Units");
    list.push_back("Draw Segment Division Lines");
    list.push_back("Show Derivative");
    return list;
}

QString
TimeValueLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Plot Type") return tr("Plot Type");
    if (name == "Vertical Scale") return tr("Vertical Scale");
    if (name == "Scale Units") return tr("Scale Units");
    if (name == "Draw Segment Division Lines") return tr("Draw Segment Division Lines");
    if (name == "Show Derivative") return tr("Show Derivative");
    return SingleColourLayer::getPropertyLabel(name);
}

QString
TimeValueLayer::getPropertyIconName(const PropertyName &name) const
{
    if (name == "Draw Segment Division Lines") return "lines";
    if (name == "Show Derivative") return "derivative";
    return "";
}

Layer::PropertyType
TimeValueLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Plot Type") return ValueProperty;
    if (name == "Vertical Scale") return ValueProperty;
    if (name == "Scale Units") return UnitsProperty;
    if (name == "Colour" && m_plotStyle == PlotSegmentation) return ValueProperty;
    if (name == "Draw Segment Division Lines") return ToggleProperty;
    if (name == "Show Derivative") return ToggleProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
TimeValueLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Vertical Scale" || name == "Scale Units") {
        return tr("Scale");
    }
    if (name == "Plot Type" || name == "Draw Segment Division Lines" ||
        name == "Show Derivative") {
        return tr("Plot Type");
    }
    return SingleColourLayer::getPropertyGroupName(name);
}

int
TimeValueLayer::getPropertyRangeAndValue(const PropertyName &name,
					 int *min, int *max, int *deflt) const
{
    int val = 0;

    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
            
        if (min) *min = 0;
        if (max) *max = ColourMapper::getColourMapCount() - 1;
        if (deflt) *deflt = 0;
        
        val = m_colourMap;

    } else if (name == "Plot Type") {
	
	if (min) *min = 0;
	if (max) *max = 5;
        if (deflt) *deflt = int(PlotConnectedPoints);
	
	val = int(m_plotStyle);

    } else if (name == "Vertical Scale") {
	
	if (min) *min = 0;
	if (max) *max = 3;
        if (deflt) *deflt = int(AutoAlignScale);
	
	val = int(m_verticalScale);

    } else if (name == "Scale Units") {

        if (deflt) *deflt = 0;
        if (m_model) {
            val = UnitDatabase::getInstance()->getUnitId
                (m_model->getScaleUnits());
        }

    } else if (name == "Draw Segment Division Lines") {

        if (min) *min = 0;
        if (max) *max = 0;
        if (deflt) *deflt = 1;
        val = (m_drawSegmentDivisions ? 1.0 : 0.0);

    } else if (name == "Show Derivative") {

        if (min) *min = 0;
        if (max) *max = 0;
        if (deflt) *deflt = 0;
        val = (m_derivative ? 1.0 : 0.0);

    } else {
	
	val = SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
TimeValueLayer::getPropertyValueLabel(const PropertyName &name,
				    int value) const
{
    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
        return ColourMapper::getColourMapName(value);
    } else if (name == "Plot Type") {
	switch (value) {
	default:
	case 0: return tr("Points");
	case 1: return tr("Stems");
	case 2: return tr("Connected Points");
	case 3: return tr("Lines");
	case 4: return tr("Curve");
	case 5: return tr("Segmentation");
	}
    } else if (name == "Vertical Scale") {
	switch (value) {
	default:
	case 0: return tr("Auto-Align");
	case 1: return tr("Linear");
	case 2: return tr("Log");
	case 3: return tr("+/-1");
	}
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

void
TimeValueLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Colour" && m_plotStyle == PlotSegmentation) {
        setFillColourMap(value);
    } else if (name == "Plot Type") {
	setPlotStyle(PlotStyle(value));
    } else if (name == "Vertical Scale") {
	setVerticalScale(VerticalScale(value));
    } else if (name == "Scale Units") {
        if (m_model) {
            m_model->setScaleUnits
                (UnitDatabase::getInstance()->getUnitById(value));
            emit modelChanged();
        }
    } else if (name == "Draw Segment Division Lines") {
        setDrawSegmentDivisions(value > 0.5);
    } else if (name == "Show Derivative") {
        setShowDerivative(value > 0.5);
    } else {
        SingleColourLayer::setProperty(name, value);
    }
}

void
TimeValueLayer::setFillColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    emit layerParametersChanged();
}

void
TimeValueLayer::setPlotStyle(PlotStyle style)
{
    if (m_plotStyle == style) return;
    bool colourTypeChanged = (style == PlotSegmentation ||
                              m_plotStyle == PlotSegmentation);
    m_plotStyle = style;
    if (colourTypeChanged) {
        emit layerParameterRangesChanged();
    }
    emit layerParametersChanged();
}

void
TimeValueLayer::setVerticalScale(VerticalScale scale)
{
    if (m_verticalScale == scale) return;
    m_verticalScale = scale;
    emit layerParametersChanged();
}

void
TimeValueLayer::setDrawSegmentDivisions(bool draw)
{
    if (m_drawSegmentDivisions == draw) return;
    m_drawSegmentDivisions = draw;
    emit layerParametersChanged();
}

void
TimeValueLayer::setShowDerivative(bool show)
{
    if (m_derivative == show) return;
    m_derivative = show;
    emit layerParametersChanged();
}

bool
TimeValueLayer::isLayerScrollable(const View *v) const
{
    // We don't illuminate sections in the line or curve modes, so
    // they're always scrollable

    if (m_plotStyle == PlotLines ||
	m_plotStyle == PlotCurve) return true;

    QPoint discard;
    return !v->shouldIlluminateLocalFeatures(this, discard);
}

bool
TimeValueLayer::getValueExtents(float &min, float &max,
                                bool &logarithmic, QString &unit) const
{
    if (!m_model) return false;
    min = m_model->getValueMinimum();
    max = m_model->getValueMaximum();
    logarithmic = (m_verticalScale == LogScale);
    unit = m_model->getScaleUnits();
    if (m_derivative) {
        max = std::max(fabsf(min), fabsf(max));
        min = -max;
    }
    return true;
}

bool
TimeValueLayer::getDisplayExtents(float &min, float &max) const
{
    if (!m_model || shouldAutoAlign()) return false;

    if (m_scaleMinimum == m_scaleMaximum) {
        min = m_model->getValueMinimum();
        max = m_model->getValueMaximum();
    } else {
        min = m_scaleMinimum;
        max = m_scaleMaximum;
    }

    if (m_derivative) {
        max = std::max(fabsf(min), fabsf(max));
        min = -max;
    }

#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::getDisplayExtents: min = " << min << ", max = " << max << std::endl;
#endif

    return true;
}

bool
TimeValueLayer::setDisplayExtents(float min, float max)
{
    if (!m_model) return false;

    if (min == max) {
        if (min == 0.f) {
            max = 1.f;
        } else {
            max = min * 1.0001;
        }
    }

    m_scaleMinimum = min;
    m_scaleMaximum = max;

#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::setDisplayExtents: min = " << min << ", max = " << max << std::endl;
#endif
    
    emit layerParametersChanged();
    return true;
}

int
TimeValueLayer::getVerticalZoomSteps(int &defaultStep) const
{
    if (shouldAutoAlign()) return 0;
    if (!m_model) return 0;

    defaultStep = 0;
    return 100;
}

int
TimeValueLayer::getCurrentVerticalZoomStep() const
{
    if (shouldAutoAlign()) return 0;
    if (!m_model) return 0;

    RangeMapper *mapper = getNewVerticalZoomRangeMapper();
    if (!mapper) return 0;

    float dmin, dmax;
    getDisplayExtents(dmin, dmax);

    int nr = mapper->getPositionForValue(dmax - dmin);

#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::getCurrentVerticalZoomStep: dmin = " << dmin << ", dmax = " << dmax << ", nr = " << nr << std::endl;
#endif

    delete mapper;

    return 100 - nr;
}

void
TimeValueLayer::setVerticalZoomStep(int step)
{
    if (shouldAutoAlign()) return;
    if (!m_model) return;

    RangeMapper *mapper = getNewVerticalZoomRangeMapper();
    if (!mapper) return;
    
    float min, max;
    bool logarithmic;
    QString unit;
    getValueExtents(min, max, logarithmic, unit);
    
    float dmin, dmax;
    getDisplayExtents(dmin, dmax);

    float newdist = mapper->getValueForPosition(100 - step);

    float newmin, newmax;

    if (logarithmic) {

        // see SpectrogramLayer::setVerticalZoomStep

        newmax = (newdist + sqrtf(newdist*newdist + 4*dmin*dmax)) / 2;
        newmin = newmax - newdist;

#ifdef DEBUG_TIME_VALUE_LAYER
        std::cerr << "newmin = " << newmin << ", newmax = " << newmax << std::endl;
#endif

    } else {
        float dmid = (dmax + dmin) / 2;
        newmin = dmid - newdist / 2;
        newmax = dmid + newdist / 2;
    }

    if (newmin < min) {
        newmax += (min - newmin);
        newmin = min;
    }
    if (newmax > max) {
        newmax = max;
    }
    
#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::setVerticalZoomStep: " << step << ": " << newmin << " -> " << newmax << " (range " << newdist << ")" << std::endl;
#endif

    setDisplayExtents(newmin, newmax);
}

RangeMapper *
TimeValueLayer::getNewVerticalZoomRangeMapper() const
{
    if (!m_model) return 0;
    
    RangeMapper *mapper;

    float min, max;
    bool logarithmic;
    QString unit;
    getValueExtents(min, max, logarithmic, unit);

    if (min == max) return 0;
    
    if (logarithmic) {
        mapper = new LogRangeMapper(0, 100, min, max, unit);
    } else {
        mapper = new LinearRangeMapper(0, 100, min, max, unit);
    }

    return mapper;
}

SparseTimeValueModel::PointList
TimeValueLayer::getLocalPoints(View *v, int x) const
{
    if (!m_model) return SparseTimeValueModel::PointList();

    long frame = v->getFrameForX(x);

    SparseTimeValueModel::PointList onPoints =
	m_model->getPoints(frame);

    if (!onPoints.empty()) {
	return onPoints;
    }

    SparseTimeValueModel::PointList prevPoints =
	m_model->getPreviousPoints(frame);
    SparseTimeValueModel::PointList nextPoints =
	m_model->getNextPoints(frame);

    SparseTimeValueModel::PointList usePoints = prevPoints;

    if (prevPoints.empty()) {
	usePoints = nextPoints;
    } else if (long(prevPoints.begin()->frame) < v->getStartFrame() &&
	       !(nextPoints.begin()->frame > v->getEndFrame())) {
	usePoints = nextPoints;
    } else if (nextPoints.begin()->frame - frame <
	       frame - prevPoints.begin()->frame) {
	usePoints = nextPoints;
    }

    if (!usePoints.empty()) {
	int fuzz = 2;
	int px = v->getXForFrame(usePoints.begin()->frame);
	if ((px > x && px - x > fuzz) ||
	    (px < x && x - px > fuzz + 1)) {
	    usePoints.clear();
	}
    }

    return usePoints;
}

QString
TimeValueLayer::getLabelPreceding(size_t frame) const
{
    if (!m_model) return "";
    SparseTimeValueModel::PointList points = m_model->getPreviousPoints(frame);
    for (SparseTimeValueModel::PointList::const_iterator i = points.begin();
         i != points.end(); ++i) {
        if (i->label != "") return i->label;
    }
    return "";
}

QString
TimeValueLayer::getFeatureDescription(View *v, QPoint &pos) const
{
    int x = pos.x();

    if (!m_model || !m_model->getSampleRate()) return "";

    SparseTimeValueModel::PointList points = getLocalPoints(v, x);

    if (points.empty()) {
	if (!m_model->isReady()) {
	    return tr("In progress");
	} else {
	    return tr("No local points");
	}
    }

    long useFrame = points.begin()->frame;

    RealTime rt = RealTime::frame2RealTime(useFrame, m_model->getSampleRate());
    
    QString text;
    QString unit = m_model->getScaleUnits();
    if (unit != "") unit = " " + unit;

    if (points.begin()->label == "") {
	text = QString(tr("Time:\t%1\nValue:\t%2%3\nNo label"))
	    .arg(rt.toText(true).c_str())
	    .arg(points.begin()->value)
            .arg(unit);
    } else {
	text = QString(tr("Time:\t%1\nValue:\t%2%3\nLabel:\t%4"))
	    .arg(rt.toText(true).c_str())
	    .arg(points.begin()->value)
            .arg(unit)
	    .arg(points.begin()->label);
    }

    pos = QPoint(v->getXForFrame(useFrame),
		 getYForValue(v, points.begin()->value));
    return text;
}

bool
TimeValueLayer::snapToFeatureFrame(View *v, int &frame,
				   size_t &resolution,
				   SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToFeatureFrame(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();
    SparseTimeValueModel::PointList points;

    if (snap == SnapNeighbouring) {
	
	points = getLocalPoints(v, v->getXForFrame(frame));
	if (points.empty()) return false;
	frame = points.begin()->frame;
	return true;
    }    

    points = m_model->getPoints(frame, frame);
    int snapped = frame;
    bool found = false;

    for (SparseTimeValueModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

	if (snap == SnapRight) {

	    if (i->frame > frame) {
		snapped = i->frame;
		found = true;
		break;
	    }

	} else if (snap == SnapLeft) {

	    if (i->frame <= frame) {
		snapped = i->frame;
		found = true; // don't break, as the next may be better
	    } else {
		break;
	    }

	} else { // nearest

	    SparseTimeValueModel::PointList::const_iterator j = i;
	    ++j;

	    if (j == points.end()) {

		snapped = i->frame;
		found = true;
		break;

	    } else if (j->frame >= frame) {

		if (j->frame - frame < frame - i->frame) {
		    snapped = j->frame;
		} else {
		    snapped = i->frame;
		}
		found = true;
		break;
	    }
	}
    }

    frame = snapped;
    return found;
}

bool
TimeValueLayer::snapToSimilarFeature(View *v, int &frame,
                                     size_t &resolution,
                                     SnapType snap) const
{
    if (!m_model) {
	return Layer::snapToSimilarFeature(v, frame, resolution, snap);
    }

    resolution = m_model->getResolution();

    const SparseTimeValueModel::PointList &points = m_model->getPoints();
    SparseTimeValueModel::PointList close = m_model->getPoints(frame, frame);

    SparseTimeValueModel::PointList::const_iterator i;

    int matchframe = frame;
    float matchvalue = 0.f;

    for (i = close.begin(); i != close.end(); ++i) {
        if (i->frame > frame) break;
        matchvalue = i->value;
        matchframe = i->frame;
    }

    int snapped = frame;
    bool found = false;
    bool distant = false;
    float epsilon = 0.0001;

    i = close.begin();

    // Scan through the close points first, then the more distant ones
    // if no suitable close one is found

    while (i != points.end()) {

        if (i == close.end()) {
            i = points.begin();
            distant = true;
        }

	if (snap == SnapRight) {

	    if (i->frame > matchframe &&
                fabsf(i->value - matchvalue) < epsilon) {
		snapped = i->frame;
		found = true;
		break;
	    }

	} else if (snap == SnapLeft) {

	    if (i->frame < matchframe) {
                if (fabsf(i->value - matchvalue) < epsilon) {
                    snapped = i->frame;
                    found = true; // don't break, as the next may be better
                }
	    } else if (found || distant) {
		break;
	    }

	} else { 
            // no other snap types supported
	}

        ++i;
    }

    frame = snapped;
    return found;
}

void
TimeValueLayer::getScaleExtents(View *v, float &min, float &max, bool &log) const
{
    min = 0.0;
    max = 0.0;
    log = false;

    if (shouldAutoAlign()) {

        if (!v->getValueExtents(m_model->getScaleUnits(), min, max, log)) {
            min = m_model->getValueMinimum();
            max = m_model->getValueMaximum();
        } else if (log) {
            LogRange::mapRange(min, max);
        }

    } else if (m_verticalScale == PlusMinusOneScale) {

        min = -1.0;
        max = 1.0;

    } else {

        getDisplayExtents(min, max);

        if (m_verticalScale == LogScale) {
            LogRange::mapRange(min, max);
            log = true;
        }
    }

    if (max == min) max = min + 1.0;
}

int
TimeValueLayer::getYForValue(View *v, float val) const
{
    float min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->height();

    getScaleExtents(v, min, max, logarithmic);

#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "getYForValue(" << val << "): min " << min << ", max "
              << max << ", log " << logarithmic << std::endl;
#endif

    if (logarithmic) {
        val = LogRange::map(val);
    }

    return int(h - ((val - min) * h) / (max - min));
}

float
TimeValueLayer::getValueForY(View *v, int y) const
{
    float min = 0.0, max = 0.0;
    bool logarithmic = false;
    int h = v->height();

    getScaleExtents(v, min, max, logarithmic);

    float val = min + (float(h - y) * float(max - min)) / h;

    if (logarithmic) {
        val = LogRange::map(val);
    }

    return val;
}

bool
TimeValueLayer::shouldAutoAlign() const
{
    if (!m_model) return false;
    QString unit = m_model->getScaleUnits();
    return (m_verticalScale == AutoAlignScale && unit != "");
}

QColor
TimeValueLayer::getColourForValue(View *v, float val) const
{
    float min, max;
    bool log;
    getScaleExtents(v, min, max, log);

    if (min > max) std::swap(min, max);
    if (max == min) max = min + 1;

    if (log) {
        val = LogRange::map(val);
    }

#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::getColourForValue: min " << min << ", max "
              << max << ", log " << log << ", value " << val << std::endl;
#endif

    QColor solid = ColourMapper(m_colourMap, min, max).map(val);
    return QColor(solid.red(), solid.green(), solid.blue(), 120);
}

int
TimeValueLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "Bright Green" : "Green"));
}

void
TimeValueLayer::paint(View *v, QPainter &paint, QRect rect) const
{
    if (!m_model || !m_model->isOK()) return;

    int sampleRate = m_model->getSampleRate();
    if (!sampleRate) return;

    paint.setRenderHint(QPainter::Antialiasing, false);

//    Profiler profiler("TimeValueLayer::paint", true);

    int x0 = rect.left(), x1 = rect.right();
    long frame0 = v->getFrameForX(x0);
    long frame1 = v->getFrameForX(x1);
    if (m_derivative) --frame0;

    SparseTimeValueModel::PointList points(m_model->getPoints
					   (frame0, frame1));
    if (points.empty()) return;

    paint.setPen(getBaseQColor());

    QColor brushColour(getBaseQColor());
    brushColour.setAlpha(80);
    paint.setBrush(brushColour);

#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::paint: resolution is "
	      << m_model->getResolution() << " frames" << std::endl;
#endif

    float min = m_model->getValueMinimum();
    float max = m_model->getValueMaximum();
    if (max == min) max = min + 1.0;

    int origin = int(nearbyint(v->height() -
			       (-min * v->height()) / (max - min)));

    QPoint localPos;
    long illuminateFrame = -1;

    if (v->shouldIlluminateLocalFeatures(this, localPos)) {
	SparseTimeValueModel::PointList localPoints =
	    getLocalPoints(v, localPos.x());
#ifdef DEBUG_TIME_VALUE_LAYER
        std::cerr << "TimeValueLayer: " << localPoints.size() << " local points" << std::endl;
#endif
	if (!localPoints.empty()) illuminateFrame = localPoints.begin()->frame;
    }

    int w =
	v->getXForFrame(frame0 + m_model->getResolution()) -
	v->getXForFrame(frame0);

    paint.save();

    QPainterPath path;
    int pointCount = 0;

    int textY = 0;
    if (m_plotStyle == PlotSegmentation) {
        textY = v->getTextLabelHeight(this, paint);
    } else {
        int originY = getYForValue(v, 0.f);
        if (originY > 0 && originY < v->height()) {
            paint.save();
            paint.setPen(getPartialShades(v)[1]);
            paint.drawLine(x0, originY, x1, originY);
            paint.restore();
        }
    }
    
    for (SparseTimeValueModel::PointList::const_iterator i = points.begin();
	 i != points.end(); ++i) {

        if (m_derivative && i == points.begin()) continue;
        
	const SparseTimeValueModel::Point &p(*i);

        float value = p.value;
        if (m_derivative) {
            SparseTimeValueModel::PointList::const_iterator j = i;
            --j;
            value -= j->value;
        }

	int x = v->getXForFrame(p.frame);
	int y = getYForValue(v, value);

        if (m_plotStyle != PlotSegmentation) {
            textY = y - paint.fontMetrics().height()
                      + paint.fontMetrics().ascent();
            if (textY < paint.fontMetrics().ascent() + 1) {
                textY = paint.fontMetrics().ascent() + 1;
            }
        }

	bool haveNext = false;
	int nx = v->getXForFrame(v->getModelsEndFrame());
	int ny = y;

	SparseTimeValueModel::PointList::const_iterator j = i;
	++j;

	if (j != points.end()) {
	    const SparseTimeValueModel::Point &q(*j);
            float nvalue = q.value;
            if (m_derivative) nvalue -= p.value;
	    nx = v->getXForFrame(q.frame);
	    ny = getYForValue(v, nvalue);
	    haveNext = true;
        }

//        std::cout << "frame = " << p.frame << ", x = " << x << ", haveNext = " << haveNext 
//                  << ", nx = " << nx << std::endl;

	if (w < 1) w = 1;
	paint.setPen(getBaseQColor());

	if (m_plotStyle == PlotSegmentation) {
            paint.setPen(getForegroundQColor(v));
            paint.setBrush(getColourForValue(v, value));
	} else if (m_plotStyle == PlotLines ||
		   m_plotStyle == PlotCurve) {
	    paint.setBrush(Qt::NoBrush);
	} else {
	    paint.setBrush(brushColour);
	}	    

	if (m_plotStyle == PlotStems) {
/*
	    paint.setPen(brushColour);
	    if (y < origin - 1) {
		paint.drawRect(x + w/2, y + 1, 1, origin - y);
	    } else if (y > origin + 1) {
		paint.drawRect(x + w/2, origin, 1, y - origin - 1);
	    }
*/
	    paint.setPen(getBaseQColor());
	    if (y < origin - 1) {
		paint.drawLine(x + w/2, y + 1, x + w/2, origin);
	    } else if (y > origin + 1) {
		paint.drawLine(x + w/2, origin, x + w/2, y - 1);
	    }
	}

	if (illuminateFrame == p.frame) {

	    //!!! aside from the problem of choosing a colour, it'd be
	    //better to save the highlighted rects and draw them at
	    //the end perhaps

	    //!!! not equipped to illuminate the right section in line
	    //or curve mode

	    if (m_plotStyle != PlotCurve &&
		m_plotStyle != PlotLines) {
		paint.setPen(getForegroundQColor(v));
	    }	    
	}

	if (m_plotStyle != PlotLines &&
	    m_plotStyle != PlotCurve &&
	    m_plotStyle != PlotSegmentation) {
            if (m_plotStyle != PlotStems ||
                w > 1) {
                paint.drawRect(x, y - 1, w, 2);
            }
	}

	if (m_plotStyle == PlotConnectedPoints ||
	    m_plotStyle == PlotLines ||
	    m_plotStyle == PlotCurve) {

	    if (haveNext) {

		if (m_plotStyle == PlotConnectedPoints) {
		    
                    paint.save();
		    paint.setPen(brushColour);
		    paint.drawLine(x + w, y, nx, ny);
                    paint.restore();

		} else if (m_plotStyle == PlotLines) {
                    
                    if (pointCount == 0) {
                        path.moveTo(x + w/2, y);
                    }
                    ++pointCount;

//		    paint.drawLine(x + w/2, y, nx + w/2, ny);
                    path.lineTo(nx + w/2, ny);

		} else {

		    float x0 = x + float(w)/2;
		    float x1 = nx + float(w)/2;
		    
		    float y0 = y;
		    float y1 = ny;

		    if (pointCount == 0) {
			path.moveTo((x0 + x1) / 2, (y0 + y1) / 2);
		    }
		    ++pointCount;

		    if (nx - x > 5) {
			path.cubicTo(x0, y0,
				     x0, y0,
				     (x0 + x1) / 2, (y0 + y1) / 2);

			// // or
			// path.quadTo(x0, y0, (x0 + x1) / 2, (y0 + y1) / 2);

		    } else {
			path.lineTo((x0 + x1) / 2, (y0 + y1) / 2);
		    }
		}
	    }
	}

	if (m_plotStyle == PlotSegmentation) {

#ifdef DEBUG_TIME_VALUE_LAYER
            std::cerr << "drawing rect" << std::endl;
#endif
	    
	    if (nx <= x) continue;

            paint.setPen(QPen(getForegroundQColor(v), 2));

            if (illuminateFrame != p.frame) {
                if (!m_drawSegmentDivisions ||
                    nx < x + 5 ||
                    x >= v->width() - 1) {
                    paint.setPen(Qt::NoPen);
                }
	    }

	    paint.drawRect(x, -1, nx - x, v->height() + 1);
	}

	if (p.label != "") {
            if (!haveNext || nx > x + 6 + paint.fontMetrics().width(p.label)) {
                paint.drawText(x + 5, textY, p.label);
            }
	}
    }

    if ((m_plotStyle == PlotCurve || m_plotStyle == PlotLines)
        && !path.isEmpty()) {
	paint.setRenderHint(QPainter::Antialiasing, pointCount <= v->width());
	paint.drawPath(path);
    }

    paint.restore();

    // looks like save/restore doesn't deal with this:
    paint.setRenderHint(QPainter::Antialiasing, false);
}

int
TimeValueLayer::getVerticalScaleWidth(View *, QPainter &paint) const
{
    int w = paint.fontMetrics().width("-000.000");
    if (m_plotStyle == PlotSegmentation) return w + 20;
    else return w + 10;
}

void
TimeValueLayer::paintVerticalScale(View *v, QPainter &paint, QRect) const
{
    if (!m_model) return;

    int h = v->height();

    int n = 10;

    float min, max;
    bool logarithmic;
    getScaleExtents(v, min, max, logarithmic);

    if (m_plotStyle == PlotSegmentation) {
        QString unit;
        getValueExtents(min, max, logarithmic, unit);
        if (logarithmic) {
            LogRange::mapRange(min, max);
        }
    }

    float val = min;
    float inc = (max - val) / n;

    char buffer[40];

    int w = getVerticalScaleWidth(v, paint);

    int tx = 5;

    int boxx = 5, boxy = 5;
    if (m_model->getScaleUnits() != "") {
        boxy += paint.fontMetrics().height();
    }
    int boxw = 10, boxh = h - boxy - 5;

    if (m_plotStyle == PlotSegmentation) {
        tx += boxx + boxw;
        paint.drawRect(boxx, boxy, boxw, boxh);
    }

    if (m_plotStyle == PlotSegmentation) {
        paint.save();
        for (int y = 0; y < boxh; ++y) {
            float val = ((boxh - y) * (max - min)) / boxh + min;
            if (logarithmic) {
                paint.setPen(getColourForValue(v, LogRange::unmap(val)));
            } else {
                paint.setPen(getColourForValue(v, val));
            }
            paint.drawLine(boxx + 1, y + boxy + 1, boxx + boxw, y + boxy + 1);
        }
        paint.restore();
    }
    
    float round = 1.f;
    int dp = 0;
    if (inc > 0) {
        int prec = trunc(log10f(inc));
        prec -= 1;
        if (prec < 0) dp = -prec;
        round = powf(10.f, prec);
#ifdef DEBUG_TIME_VALUE_LAYER
        std::cerr << "inc = " << inc << ", round = " << round << std::endl;
#endif
    }

    int prevy = -1;
                
    for (int i = 0; i < n; ++i) {

	int y, ty;
        bool drawText = true;

        float dispval = val;

        if (m_plotStyle == PlotSegmentation) {
            y = boxy + int(boxh - ((val - min) * boxh) / (max - min));
            ty = y;
        } else {
            if (i == n-1 &&
                v->height() < paint.fontMetrics().height() * (n*2)) {
                if (m_model->getScaleUnits() != "") drawText = false;
            }
            dispval = lrintf(val / round) * round;
#ifdef DEBUG_TIME_VALUE_LAYER
            std::cerr << "val = " << val << ", dispval = " << dispval << std::endl;
#endif
            if (logarithmic) {
                y = getYForValue(v, LogRange::unmap(dispval));
            } else {
                y = getYForValue(v, dispval);
            }                
            ty = y - paint.fontMetrics().height() +
                     paint.fontMetrics().ascent() + 2;

            if (prevy >= 0 && (prevy - y) < paint.fontMetrics().height()) {
                val += inc;
                continue;
            }
        }

        if (logarithmic) {
            sprintf(buffer, "%.*g", dp < 2 ? 2 : dp, LogRange::unmap(dispval));
        } else {
            sprintf(buffer, "%.*f", dp, dispval);
        }            
	QString label = QString(buffer);

        if (m_plotStyle != PlotSegmentation) {
            paint.drawLine(w - 5, y, w, y);
        } else {
            paint.drawLine(boxx + boxw - boxw/3, y, boxx + boxw, y);
        }

        if (drawText) {
            if (m_plotStyle != PlotSegmentation) {
                paint.drawText(tx + w - paint.fontMetrics().width(label) - 8,
                               ty, label);
            } else {
                paint.drawText(tx, ty, label);
            }
        }

        prevy = y;
	val += inc;
    }
    
    if (m_model->getScaleUnits() != "") {
        paint.drawText(5, 5 + paint.fontMetrics().ascent(),
                       m_model->getScaleUnits());
    }
}

void
TimeValueLayer::drawStart(View *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::drawStart(" << e->x() << "," << e->y() << ")" << std::endl;
#endif

    if (!m_model) return;

    long frame = v->getFrameForX(e->x());
    long resolution = m_model->getResolution();
    if (frame < 0) frame = 0;
    frame = (frame / resolution) * resolution;

    float value = getValueForY(v, e->y());

    bool havePoint = false;

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());
    if (!points.empty()) {
        for (SparseTimeValueModel::PointList::iterator i = points.begin();
             i != points.end(); ++i) {
            if (((i->frame / resolution) * resolution) != frame) {
#ifdef DEBUG_TIME_VALUE_LAYER
                std::cerr << "ignoring out-of-range frame at " << i->frame << std::endl;
#endif
                continue;
            }
            m_editingPoint = *i;
            havePoint = true;
        }
    }

    if (!havePoint) {
        m_editingPoint = SparseTimeValueModel::Point
            (frame, value, tr("New Point"));
    }

    m_originalPoint = m_editingPoint;

    if (m_editingCommand) finish(m_editingCommand);
    m_editingCommand = new SparseTimeValueModel::EditCommand(m_model,
							     tr("Draw Point"));
    if (!havePoint) {
        m_editingCommand->addPoint(m_editingPoint);
    }

    m_editing = true;
}

void
TimeValueLayer::drawDrag(View *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::drawDrag(" << e->x() << "," << e->y() << ")" << std::endl;
#endif

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    long resolution = m_model->getResolution();
    if (frame < 0) frame = 0;
    frame = (frame / resolution) * resolution;

    float value = getValueForY(v, e->y());

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());

#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << points.size() << " points" << std::endl;
#endif

    bool havePoint = false;

    if (!points.empty()) {
        for (SparseTimeValueModel::PointList::iterator i = points.begin();
             i != points.end(); ++i) {
            if (i->frame == m_editingPoint.frame &&
                i->value == m_editingPoint.value) {
#ifdef DEBUG_TIME_VALUE_LAYER
                std::cerr << "ignoring current editing point at " << i->frame << ", " << i->value << std::endl;
#endif
                continue;
            }
            if (((i->frame / resolution) * resolution) != frame) {
#ifdef DEBUG_TIME_VALUE_LAYER
                std::cerr << "ignoring out-of-range frame at " << i->frame << std::endl;
#endif
                continue;
            }
#ifdef DEBUG_TIME_VALUE_LAYER
            std::cerr << "adjusting to new point at " << i->frame << ", " << i->value << std::endl;
#endif
            m_editingPoint = *i;
            m_originalPoint = m_editingPoint;
            m_editingCommand->deletePoint(m_editingPoint);
            havePoint = true;
        }
    }

    if (!havePoint) {
        if (frame == m_editingPoint.frame) {
            m_editingCommand->deletePoint(m_editingPoint);
        }
    }

//    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingPoint.value = value;
    m_editingCommand->addPoint(m_editingPoint);
}

void
TimeValueLayer::drawEnd(View *, QMouseEvent *)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::drawEnd" << std::endl;
#endif
    if (!m_model || !m_editing) return;
    finish(m_editingCommand);
    m_editingCommand = 0;
    m_editing = false;
}

void
TimeValueLayer::eraseStart(View *v, QMouseEvent *e)
{
    if (!m_model) return;

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return;

    m_editingPoint = *points.begin();

    if (m_editingCommand) {
	finish(m_editingCommand);
	m_editingCommand = 0;
    }

    m_editing = true;
}

void
TimeValueLayer::eraseDrag(View *v, QMouseEvent *e)
{
}

void
TimeValueLayer::eraseEnd(View *v, QMouseEvent *e)
{
    if (!m_model || !m_editing) return;

    m_editing = false;

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return;
    if (points.begin()->frame != m_editingPoint.frame ||
        points.begin()->value != m_editingPoint.value) return;

    m_editingCommand = new SparseTimeValueModel::EditCommand
        (m_model, tr("Erase Point"));

    m_editingCommand->deletePoint(m_editingPoint);

    finish(m_editingCommand);
    m_editingCommand = 0;
    m_editing = false;
}

void
TimeValueLayer::editStart(View *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::editStart(" << e->x() << "," << e->y() << ")" << std::endl;
#endif

    if (!m_model) return;

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return;

    m_editingPoint = *points.begin();
    m_originalPoint = m_editingPoint;

    if (m_editingCommand) {
	finish(m_editingCommand);
	m_editingCommand = 0;
    }

    m_editing = true;
}

void
TimeValueLayer::editDrag(View *v, QMouseEvent *e)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::editDrag(" << e->x() << "," << e->y() << ")" << std::endl;
#endif

    if (!m_model || !m_editing) return;

    long frame = v->getFrameForX(e->x());
    if (frame < 0) frame = 0;
    frame = frame / m_model->getResolution() * m_model->getResolution();

    float value = getValueForY(v, e->y());

    if (!m_editingCommand) {
	m_editingCommand = new SparseTimeValueModel::EditCommand(m_model,
								 tr("Drag Point"));
    }

    m_editingCommand->deletePoint(m_editingPoint);
    m_editingPoint.frame = frame;
    m_editingPoint.value = value;
    m_editingCommand->addPoint(m_editingPoint);
}

void
TimeValueLayer::editEnd(View *, QMouseEvent *)
{
#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "TimeValueLayer::editEnd" << std::endl;
#endif
    if (!m_model || !m_editing) return;

    if (m_editingCommand) {

	QString newName = m_editingCommand->getName();

	if (m_editingPoint.frame != m_originalPoint.frame) {
	    if (m_editingPoint.value != m_originalPoint.value) {
		newName = tr("Edit Point");
	    } else {
		newName = tr("Relocate Point");
	    }
	} else {
	    newName = tr("Change Point Value");
	}

	m_editingCommand->setName(newName);
	finish(m_editingCommand);
    }

    m_editingCommand = 0;
    m_editing = false;
}

bool
TimeValueLayer::editOpen(View *v, QMouseEvent *e)
{
    if (!m_model) return false;

    SparseTimeValueModel::PointList points = getLocalPoints(v, e->x());
    if (points.empty()) return false;

    SparseTimeValueModel::Point point = *points.begin();

    ItemEditDialog *dialog = new ItemEditDialog
        (m_model->getSampleRate(),
         ItemEditDialog::ShowTime |
         ItemEditDialog::ShowValue |
         ItemEditDialog::ShowText,
         m_model->getScaleUnits());

    dialog->setFrameTime(point.frame);
    dialog->setValue(point.value);
    dialog->setText(point.label);

    if (dialog->exec() == QDialog::Accepted) {

        SparseTimeValueModel::Point newPoint = point;
        newPoint.frame = dialog->getFrameTime();
        newPoint.value = dialog->getValue();
        newPoint.label = dialog->getText();
        
        SparseTimeValueModel::EditCommand *command =
            new SparseTimeValueModel::EditCommand(m_model, tr("Edit Point"));
        command->deletePoint(point);
        command->addPoint(newPoint);
        finish(command);
    }

    delete dialog;
    return true;
}

void
TimeValueLayer::moveSelection(Selection s, size_t newStartFrame)
{
    if (!m_model) return;

    SparseTimeValueModel::EditCommand *command =
	new SparseTimeValueModel::EditCommand(m_model,
					      tr("Drag Selection"));

    SparseTimeValueModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (SparseTimeValueModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {
	    SparseTimeValueModel::Point newPoint(*i);
	    newPoint.frame = i->frame + newStartFrame - s.getStartFrame();
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    finish(command);
}

void
TimeValueLayer::resizeSelection(Selection s, Selection newSize)
{
    if (!m_model) return;

    SparseTimeValueModel::EditCommand *command =
	new SparseTimeValueModel::EditCommand(m_model,
					      tr("Resize Selection"));

    SparseTimeValueModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    double ratio =
	double(newSize.getEndFrame() - newSize.getStartFrame()) /
	double(s.getEndFrame() - s.getStartFrame());

    for (SparseTimeValueModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

	if (s.contains(i->frame)) {

	    double target = i->frame;
	    target = newSize.getStartFrame() + 
		double(target - s.getStartFrame()) * ratio;

	    SparseTimeValueModel::Point newPoint(*i);
	    newPoint.frame = lrint(target);
	    command->deletePoint(*i);
	    command->addPoint(newPoint);
	}
    }

    finish(command);
}

void
TimeValueLayer::deleteSelection(Selection s)
{
    if (!m_model) return;

    SparseTimeValueModel::EditCommand *command =
	new SparseTimeValueModel::EditCommand(m_model,
					      tr("Delete Selected Points"));

    SparseTimeValueModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (SparseTimeValueModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {

        if (s.contains(i->frame)) {
            command->deletePoint(*i);
        }
    }

    finish(command);
}    

void
TimeValueLayer::copy(View *v, Selection s, Clipboard &to)
{
    if (!m_model) return;

    SparseTimeValueModel::PointList points =
	m_model->getPoints(s.getStartFrame(), s.getEndFrame());

    for (SparseTimeValueModel::PointList::iterator i = points.begin();
	 i != points.end(); ++i) {
	if (s.contains(i->frame)) {
            Clipboard::Point point(i->frame, i->value, i->label);
            point.setReferenceFrame(alignToReference(v, i->frame));
            to.addPoint(point);
        }
    }
}

bool
TimeValueLayer::paste(View *v, const Clipboard &from, int frameOffset,
                      bool interactive)
{
    if (!m_model) return false;

    const Clipboard::PointList &points = from.getPoints();

    bool realign = false;

    if (clipboardHasDifferentAlignment(v, from)) {

        QMessageBox::StandardButton button =
            QMessageBox::question(v, tr("Re-align pasted items?"),
                                  tr("The items you are pasting came from a layer with different source material from this one.  Do you want to re-align them in time, to match the source material for this layer?"),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                  QMessageBox::Yes);

        if (button == QMessageBox::Cancel) {
            return false;
        }

        if (button == QMessageBox::Yes) {
            realign = true;
        }
    }

    SparseTimeValueModel::EditCommand *command =
	new SparseTimeValueModel::EditCommand(m_model, tr("Paste"));

    enum ValueAvailability {
        UnknownAvailability,
        NoValues,
        SomeValues,
        AllValues
    };

    Labeller::ValueType generation = Labeller::ValueNone;

    bool haveUsableLabels = false;
    bool haveExistingItems = !(m_model->isEmpty());
    Labeller labeller;
    labeller.setSampleRate(m_model->getSampleRate());

    if (interactive) {

        ValueAvailability availability = UnknownAvailability;

        for (Clipboard::PointList::const_iterator i = points.begin();
             i != points.end(); ++i) {
        
            if (!i->haveFrame()) continue;

            if (availability == UnknownAvailability) {
                if (i->haveValue()) availability = AllValues;
                else availability = NoValues;
                continue;
            }

            if (i->haveValue()) {
                if (availability == NoValues) {
                    availability = SomeValues;
                }
            } else {
                if (availability == AllValues) {
                    availability = SomeValues;
                }
            }

            if (!haveUsableLabels) {
                if (i->haveLabel()) {
                    if (i->getLabel().contains(QRegExp("[0-9]"))) {
                        haveUsableLabels = true;
                    }
                }
            }

            if (availability == SomeValues && haveUsableLabels) break;
        }

        if (availability == NoValues || availability == SomeValues) {
            
            QString text;
            if (availability == NoValues) {
                text = tr("The items you are pasting do not have values.\nWhat values do you want to use for these items?");
            } else {
                text = tr("Some of the items you are pasting do not have values.\nWhat values do you want to use for these items?");
            }

            Labeller::TypeNameMap names = labeller.getTypeNames();

            QStringList options;
            std::vector<Labeller::ValueType> genopts;

            for (Labeller::TypeNameMap::const_iterator i = names.begin();
                 i != names.end(); ++i) {
                if (i->first == Labeller::ValueNone) options << tr("Zero for all items");
                else options << i->second;
                genopts.push_back(i->first);
            }

            static int prevSelection = 0;

            bool ok = false;
            QString selected = ListInputDialog::getItem
                (0, tr("Choose value calculation"),
                 text, options, prevSelection, &ok);

            if (!ok) return false;
            int selection = 0;
            generation = Labeller::ValueNone;

            for (QStringList::const_iterator i = options.begin();
                 i != options.end(); ++i) {
                if (selected == *i) {
                    generation = genopts[selection];
                    break;
                }
                ++selection;
            }
            
            labeller.setType(generation);

            if (generation == Labeller::ValueFromCyclicalCounter ||
                generation == Labeller::ValueFromTwoLevelCounter) {
                int cycleSize = QInputDialog::getInteger
                    (0, tr("Select cycle size"),
                     tr("Cycle size:"), 4, 2, 16, 1);
                labeller.setCounterCycleSize(cycleSize);
            }

            prevSelection = selection;
        }
    }

    SparseTimeValueModel::Point prevPoint(0);

    for (Clipboard::PointList::const_iterator i = points.begin();
         i != points.end(); ++i) {
        
        if (!i->haveFrame()) continue;

        size_t frame = 0;

        if (!realign) {
            
            frame = i->getFrame();

        } else {

            if (i->haveReferenceFrame()) {
                frame = i->getReferenceFrame();
                frame = alignFromReference(v, frame);
            } else {
                frame = i->getFrame();
            }
        }

        SparseTimeValueModel::Point newPoint(frame);
  
        if (i->haveLabel()) {
            newPoint.label = i->getLabel();
        } else if (i->haveValue()) {
            newPoint.label = QString("%1").arg(i->getValue());
        }

        bool usePrev = false;
        SparseTimeValueModel::Point formerPrevPoint = prevPoint;

        if (i->haveValue()) {
            newPoint.value = i->getValue();
        } else {
#ifdef DEBUG_TIME_VALUE_LAYER
            std::cerr << "Setting value on point at " << newPoint.frame << " from labeller";
            if (i == points.begin()) {
                std::cerr << ", no prev point" << std::endl;
            } else {
                std::cerr << ", prev point is at " << prevPoint.frame << std::endl;
            }
#endif
            labeller.setValue<SparseTimeValueModel::Point>
                (newPoint, (i == points.begin()) ? 0 : &prevPoint);
#ifdef DEBUG_TIME_VALUE_LAYER
            std::cerr << "New point value = " << newPoint.value << std::endl;
#endif
            if (labeller.actingOnPrevPoint() && i != points.begin()) {
                usePrev = true;
            }
        }

        if (usePrev) {
            command->deletePoint(formerPrevPoint);
            command->addPoint(prevPoint);
        }

        prevPoint = newPoint;
        command->addPoint(newPoint);
    }

    finish(command);
    return true;
}

void
TimeValueLayer::toXml(QTextStream &stream,
                      QString indent, QString extraAttributes) const
{
    SingleColourLayer::toXml(stream, indent,
                             extraAttributes +
                             QString(" colourMap=\"%1\" plotStyle=\"%2\" verticalScale=\"%3\" scaleMinimum=\"%4\" scaleMaximum=\"%5\" drawDivisions=\"%6\" derivative=\"%7\" ")
                             .arg(m_colourMap)
                             .arg(m_plotStyle)
                             .arg(m_verticalScale)
                             .arg(m_scaleMinimum)
                             .arg(m_scaleMaximum)
                             .arg(m_drawSegmentDivisions ? "true" : "false")
                             .arg(m_derivative ? "true" : "false"));
}

void
TimeValueLayer::setProperties(const QXmlAttributes &attributes)
{
    SingleColourLayer::setProperties(attributes);

    bool ok, alsoOk;

    int cmap = attributes.value("colourMap").toInt(&ok);
    if (ok) setFillColourMap(cmap);

    PlotStyle style = (PlotStyle)
	attributes.value("plotStyle").toInt(&ok);
    if (ok) setPlotStyle(style);

    VerticalScale scale = (VerticalScale)
	attributes.value("verticalScale").toInt(&ok);
    if (ok) setVerticalScale(scale);

    bool draw = (attributes.value("drawDivisions").trimmed() == "true");
    setDrawSegmentDivisions(draw);

    bool derivative = (attributes.value("derivative").trimmed() == "true");
    setShowDerivative(derivative);

    float min = attributes.value("scaleMinimum").toFloat(&ok);
    float max = attributes.value("scaleMaximum").toFloat(&alsoOk);
#ifdef DEBUG_TIME_VALUE_LAYER
    std::cerr << "from properties: min = " << min << ", max = " << max << std::endl;
#endif
    if (ok && alsoOk && min != max) setDisplayExtents(min, max);
}

