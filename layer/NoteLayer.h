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

#ifndef _NOTE_LAYER_H_
#define _NOTE_LAYER_H_

#include "SingleColourLayer.h"
#include "data/model/NoteModel.h"

#include <QObject>
#include <QColor>

class View;
class QPainter;

class NoteLayer : public SingleColourLayer
{
    Q_OBJECT

public:
    NoteLayer();

    virtual void paint(View *v, QPainter &paint, QRect rect) const;

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

    virtual bool editOpen(View *v, QMouseEvent *);

    virtual void moveSelection(Selection s, size_t newStartFrame);
    virtual void resizeSelection(Selection s, Selection newSize);
    virtual void deleteSelection(Selection s);

    virtual void copy(View *v, Selection s, Clipboard &to);
    virtual bool paste(View *v, const Clipboard &from, int frameOffset,
                       bool interactive);

    virtual const Model *getModel() const { return m_model; }
    void setModel(NoteModel *model);

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);

    enum VerticalScale {
        AutoAlignScale,
        LinearScale,
        LogScale,
        MIDIRangeScale
    };

    void setVerticalScale(VerticalScale scale);
    VerticalScale getVerticalScale() const { return m_verticalScale; }

    virtual bool isLayerScrollable(const View *v) const;

    virtual bool isLayerEditable() const { return true; }

    virtual int getCompletion(View *) const { return m_model->getCompletion(); }

    virtual bool getValueExtents(float &min, float &max,
                                 bool &log, QString &unit) const;

    virtual bool getDisplayExtents(float &min, float &max) const;
    virtual bool setDisplayExtents(float min, float max);

    virtual int getVerticalZoomSteps(int &defaultStep) const;
    virtual int getCurrentVerticalZoomStep() const;
    virtual void setVerticalZoomStep(int);
    virtual RangeMapper *getNewVerticalZoomRangeMapper() const;

    /**
     * Add a note-on.  Used when recording MIDI "live".  The note will
     * not be finally added to the layer until the corresponding
     * note-off.
     */
    void addNoteOn(long frame, int pitch, int velocity);
    
    /**
     * Add a note-off.  This will cause a note to appear, if and only
     * if there is a matching pending note-on.
     */
    void addNoteOff(long frame, int pitch);

    /**
     * Abandon all pending note-on events.
     */
    void abandonNoteOns();

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

protected:
    void getScaleExtents(View *, float &min, float &max, bool &log) const;
    int getYForValue(View *v, float value) const;
    float getValueForY(View *v, int y) const;
    bool shouldConvertMIDIToHz() const;

    virtual int getDefaultColourHint(bool dark, bool &impose);

    NoteModel::PointList getLocalPoints(View *v, int) const;

    bool getPointToDrag(View *v, int x, int y, NoteModel::Point &) const;

    NoteModel *m_model;
    bool m_editing;
    int m_dragPointX;
    int m_dragPointY;
    int m_dragStartX;
    int m_dragStartY;
    NoteModel::Point m_originalPoint;
    NoteModel::Point m_editingPoint;
    NoteModel::EditCommand *m_editingCommand;
    VerticalScale m_verticalScale;

    typedef std::set<NoteModel::Point, NoteModel::Point::Comparator> NoteSet;
    NoteSet m_pendingNoteOns;

    mutable float m_scaleMinimum;
    mutable float m_scaleMaximum;

    bool shouldAutoAlign() const;

    void finish(NoteModel::EditCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

