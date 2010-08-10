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

#ifndef _TIME_INSTANT_LAYER_H_
#define _TIME_INSTANT_LAYER_H_

#include "SingleColourLayer.h"
#include "data/model/SparseOneDimensionalModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;

class TimeInstantLayer : public SingleColourLayer
{
    Q_OBJECT

public:
    TimeInstantLayer();
    virtual ~TimeInstantLayer();

    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual QString getLabelPreceding(size_t) const;
    virtual QString getFeatureDescription(View *v, QPoint &) const;

    virtual bool snapToFeatureFrame(View *v, int &frame,
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

    virtual bool editOpen(View *, QMouseEvent *);

    virtual void moveSelection(Selection s, size_t newStartFrame);
    virtual void resizeSelection(Selection s, Selection newSize);
    virtual void deleteSelection(Selection s);

    virtual void copy(View *v, Selection s, Clipboard &to);
    virtual bool paste(View *v, const Clipboard &from, int frameOffset,
                       bool interactive);

    virtual const Model *getModel() const { return m_model; }
    void setModel(SparseOneDimensionalModel *model);

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);

    enum PlotStyle {
	PlotInstants,
	PlotSegmentation
    };

    void setPlotStyle(PlotStyle style);
    PlotStyle getPlotStyle() const { return m_plotStyle; }

    virtual bool isLayerScrollable(const View *v) const;

    virtual bool isLayerEditable() const { return true; }

    virtual int getCompletion(View *) const { return m_model->getCompletion(); }

    virtual bool needsTextLabelHeight() const { return m_model->hasTextLabels(); }

    virtual bool getValueExtents(float &, float &, bool &, QString &) const {
        return false;
    }

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
    SparseOneDimensionalModel::PointList getLocalPoints(View *v, int) const;

    virtual int getDefaultColourHint(bool dark, bool &impose);

    bool clipboardAlignmentDiffers(View *v, const Clipboard &) const;

    SparseOneDimensionalModel *m_model;
    bool m_editing;
    SparseOneDimensionalModel::Point m_editingPoint;
    SparseOneDimensionalModel::EditCommand *m_editingCommand;
    PlotStyle m_plotStyle;

    void finish(SparseOneDimensionalModel::EditCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

