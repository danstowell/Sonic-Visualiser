
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

#ifndef _SPECTRUM_LAYER_H_
#define _SPECTRUM_LAYER_H_

#include "SliceLayer.h"

#include "base/Window.h"

#include "data/model/DenseTimeValueModel.h"

#include <QColor>
#include <QMutex>

class FFTModel;

class SpectrumLayer : public SliceLayer
{
    Q_OBJECT

public:
    SpectrumLayer();
    ~SpectrumLayer();
    
    void setModel(DenseTimeValueModel *model);
    virtual const Model *getModel() const { return m_originModel; }

    virtual bool getCrosshairExtents(View *, QPainter &, QPoint cursorPos,
                                     std::vector<QRect> &extents) const;
    virtual void paintCrosshairs(View *, QPainter &, QPoint) const;

    virtual QString getFeatureDescription(View *v, QPoint &) const;

    virtual void paint(View *v, QPainter &paint, QRect rect) const;

    virtual VerticalPosition getPreferredFrameCountPosition() const {
	return PositionTop;
    }

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual QString getPropertyIconName(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const;
    virtual void setProperty(const PropertyName &, int value);
    virtual void setProperties(const QXmlAttributes &);

    virtual bool getValueExtents(float &min, float &max,
                                 bool &logarithmic, QString &unit) const;

    virtual bool getXScaleValue(const View *v, int x,
                                float &value, QString &unit) const;

    virtual bool getYScaleValue(const View *, int y,
                                float &value, QString &unit) const;

    virtual bool getYScaleDifference(const View *, int y0, int y1,
                                     float &diff, QString &unit) const;

    virtual bool isLayerScrollable(const View *) const { return false; }

    void setChannel(int);
    int getChannel() const { return m_channel; }

    void setWindowSize(size_t);
    size_t getWindowSize() const { return m_windowSize; }
    
    void setWindowHopLevel(size_t level);
    size_t getWindowHopLevel() const { return m_windowHopLevel; }

    void setWindowType(WindowType type);
    WindowType getWindowType() const { return m_windowType; }

    void setShowPeaks(bool);
    bool getShowPeaks() const { return m_showPeaks; }

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

protected slots:
    void preferenceChanged(PropertyContainer::PropertyName name);

protected:
    // make this SliceLayer method unavailable to the general public
//    virtual void setModel(DenseThreeDimensionalModel *model) {
//        SliceLayer::setModel(model);
//    }

    DenseTimeValueModel    *m_originModel;
    int                     m_channel;
    bool                    m_channelSet;
    size_t                  m_windowSize;
    WindowType              m_windowType;
    size_t                  m_windowHopLevel;
    bool                    m_showPeaks;
    mutable bool            m_newFFTNeeded;

    mutable QMutex m_fftMutex;

    void setupFFT();

    virtual void getBiasCurve(BiasCurve &) const;
    BiasCurve m_biasCurve;

    virtual float getXForBin(int bin, int totalBins, float w) const;
    virtual int getBinForX(float x, int totalBins, float w) const;

    float getFrequencyForX(float x, float w) const;
    float getXForFrequency(float freq, float w) const;

    size_t getWindowIncrement() const {
        if (m_windowHopLevel == 0) return m_windowSize;
        else if (m_windowHopLevel == 1) return (m_windowSize * 3) / 4;
        else return m_windowSize / (1 << (m_windowHopLevel - 1));
    }
};

#endif
