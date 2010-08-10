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

#ifndef _TIME_VALUE_LAYER_H_
#define _TIME_VALUE_LAYER_H_

#include "SingleColourLayer.h"
#include "data/model/SparseTimeValueModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;

class TimeValueLayer : public SingleColourLayer
{
    Q_OBJECT

public:
    TimeValueLayer();

    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual int getVerticalScaleWidth(View *v, QPainter &) const;
    virtual void paintVerticalScale(View *v, QPainter &paint, QRect rect) const;

    virtual QString getFeatureDescription(View *v, QPoint &) const;
    virtual QString getLabelPreceding(size_t) const;

    virtual bool snapToFeatureFrame(View *v, int &frame,
				    size_t &resolution,
				    SnapType snap) const;
    virtual bool snapToSimilarFeature(View *v, int &frame,
                                      size_t &resolution,
                                      SnapType snap) const;

    virtual void drawStart(View *v, QMouseEvent *);
    virtual void drawDrag(View *v, QMouseEvent *);
    virtual void drawEnd(View *v, QMouseEvent *);

    virtual void eraseStart(View *v, QMouseEvent *);
    virtual void eraseDrag(View *v, QMouseEvent *);
    virtual void eraseEnd(View *v, QMouseEvent *);

    virtual void editStart(View *v, QMouseEvent *);
    virtual void editDrag(View *v, QMouseEvent *);
    virtual void editEnd(View *v, QMouseEvent *);

    virtual bool editOpen(View *v, QMouseEvent *);

    virtual void moveSelection(Selection s, size_t newStartFrame);
    virtual void resizeSelection(Selection s, Selection newSize);
    virtual void deleteSelection(Selection s);

    virtual void copy(View *v, Selection s, Clipboard &to);
    virtual bool paste(View *v, const Clipboard &from, int frameOffset,
                       bool interactive);

    virtual const Model *getModel() const { return m_model; }
    void setModel(SparseTimeValueModel *model);

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual QString getPropertyIconName(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);

    void setFillColourMap(int);
    int getFillColourMap() const { return m_colourMap; }

    enum PlotStyle {
	PlotPoints,
	PlotStems,
	PlotConnectedPoints,
	PlotLines,
	PlotCurve,
	PlotSegmentation
    };

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    enum VerticalScale {
        AutoAlignScale,
        LinearScale,
        LogScale,
        PlusMinusOneScale
    };
    
    void setVerticalScale(VerticalScale scale);
    VerticalScale getVerticalScale() const { return m_verticalScale; }

    void setDrawSegmentDivisions(bool);
    bool getDrawSegmentDivisions() const { return m_drawSegmentDivisions; }

    void setShowDerivative(bool);
    bool getShowDerivative() const { return m_derivative; }

    virtual bool isLayerScrollable(const View *v) const;

    virtual bool isLayerEditable() const { return true; }

    virtual int getCompletion(View *) const { return m_model->getCompletion(); }

    virtual bool needsTextLabelHeight() const {
        return m_plotStyle == PlotSegmentation && m_model->hasTextLabels();
    }

    virtual bool getValueExtents(float &min, float &max,
                                 bool &logarithmic, QString &unit) const;

    virtual bool getDisplayExtents(float &min, float &max) const;
    virtual bool setDisplayExtents(float min, float max);

    virtual int getVerticalZoomSteps(int &defaultStep) const;
    virtual int getCurrentVerticalZoomStep() const;
    virtual void setVerticalZoomStep(int);
    virtual RangeMapper *getNewVerticalZoomRangeMapper() const;

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

    virtual ColourSignificance getLayerColourSignificance() const {
        if (m_plotStyle == PlotSegmentation) {
            return ColourHasMeaningfulValue;
        } else {
            return ColourDistinguishes;
        }
    }

protected:
    void getScaleExtents(View *, float &min, float &max, bool &log) const;
    int getYForValue(View *, float value) const;
    float getValueForY(View *, int y) const;
    QColor getColourForValue(View *v, float value) const;
    bool shouldAutoAlign() const;

    SparseTimeValueModel::PointList getLocalPoints(View *v, int) const;

    virtual int getDefaultColourHint(bool dark, bool &impose);

    SparseTimeValueModel *m_model;
    bool m_editing;
    SparseTimeValueModel::Point m_originalPoint;
    SparseTimeValueModel::Point m_editingPoint;
    SparseTimeValueModel::EditCommand *m_editingCommand;
    int m_colourMap;
    PlotStyle m_plotStyle;
    VerticalScale m_verticalScale;
    bool m_drawSegmentDivisions;
    bool m_derivative;

    mutable float m_scaleMinimum;
    mutable float m_scaleMaximum;

    void finish(SparseTimeValueModel::EditCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

