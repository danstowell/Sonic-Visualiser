/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _SINGLE_COLOUR_LAYER_H_
#define _SINGLE_COLOUR_LAYER_H_

#include "Layer.h"
#include <QColor>
#include <vector>
#include <map>

class SingleColourLayer : public Layer
{
    Q_OBJECT
    
public:
    virtual void setBaseColour(int);
    virtual int getBaseColour() const;

    virtual bool hasLightBackground() const;

    virtual ColourSignificance getLayerColourSignificance() const {
        return ColourDistinguishes;
    }

    virtual QPixmap getLayerPresentationPixmap(QSize size) const;

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const;
    virtual void setProperty(const PropertyName &, int value);

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    virtual void setProperties(const QXmlAttributes &attributes);

    virtual void setDefaultColourFor(View *v);

protected:
    SingleColourLayer();

    virtual QColor getBaseQColor() const;
    virtual QColor getBackgroundQColor(View *v) const;
    virtual QColor getForegroundQColor(View *v) const;
    std::vector<QColor> getPartialShades(View *v) const;

    virtual void flagBaseColourChanged() { }
    virtual int getDefaultColourHint(bool /* darkBackground */,
                                     bool & /* impose */) { return -1; }

    typedef std::map<int, int> ColourRefCount;
    static ColourRefCount m_colourRefCount;

    int m_colour;
    bool m_colourExplicitlySet;
    bool m_defaultColourSet;
};

#endif
