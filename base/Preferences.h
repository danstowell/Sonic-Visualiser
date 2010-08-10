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

#ifndef _PREFERENCES_H_
#define _PREFERENCES_H_

#include "PropertyContainer.h"

#include "Window.h"

class Preferences : public PropertyContainer
{
    Q_OBJECT

public:
    static Preferences *getInstance();

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &, int *, int *, int *) const;
    virtual QString getPropertyValueLabel(const PropertyName &, int value) const;
    virtual QString getPropertyContainerName() const;
    virtual QString getPropertyContainerIconName() const;

    enum SpectrogramSmoothing {
        NoSpectrogramSmoothing,
        SpectrogramInterpolated,
        SpectrogramZeroPadded,
        SpectrogramZeroPaddedAndInterpolated
    };

    enum SpectrogramXSmoothing {
        NoSpectrogramXSmoothing,
        SpectrogramXInterpolated
    };

    SpectrogramSmoothing getSpectrogramSmoothing() const { return m_spectrogramSmoothing; }
    SpectrogramXSmoothing getSpectrogramXSmoothing() const { return m_spectrogramXSmoothing; }
    float getTuningFrequency() const { return m_tuningFrequency; }
    WindowType getWindowType() const { return m_windowType; }
    int getResampleQuality() const { return m_resampleQuality; }

    //!!! harmonise with PaneStack
    enum PropertyBoxLayout {
        VerticallyStacked,
        Layered
    };
    PropertyBoxLayout getPropertyBoxLayout() const { return m_propertyBoxLayout; }

    int getViewFontSize() const { return m_viewFontSize; }

    bool getOmitTempsFromRecentFiles() const { return m_omitRecentTemps; }

    QString getTemporaryDirectoryRoot() const { return m_tempDirRoot; }

    bool getResampleOnLoad() const { return m_resampleOnLoad; }    
    
    enum BackgroundMode {
        BackgroundFromTheme,
        DarkBackground,
        LightBackground 
    };
    BackgroundMode getBackgroundMode() const { return m_backgroundMode; }

    enum TimeToTextMode {
        TimeToTextMs,
        TimeToTextUs,
        TimeToText24Frame,
        TimeToText25Frame,
        TimeToText30Frame,
        TimeToText50Frame,
        TimeToText60Frame
    };
    TimeToTextMode getTimeToTextMode() const { return m_timeToTextMode; }

    bool getShowSplash() const { return m_showSplash; }

public slots:
    virtual void setProperty(const PropertyName &, int);

    void setSpectrogramSmoothing(SpectrogramSmoothing smoothing);
    void setSpectrogramXSmoothing(SpectrogramXSmoothing smoothing);
    void setTuningFrequency(float freq);
    void setPropertyBoxLayout(PropertyBoxLayout layout);
    void setWindowType(WindowType type);
    void setResampleQuality(int quality);
    void setOmitTempsFromRecentFiles(bool omit);
    void setTemporaryDirectoryRoot(QString tempDirRoot);
    void setResampleOnLoad(bool);
    void setBackgroundMode(BackgroundMode mode);
    void setTimeToTextMode(TimeToTextMode mode);
    void setViewFontSize(int size);
    void setShowSplash(bool);

private:
    Preferences(); // may throw DirectoryCreationFailed
    virtual ~Preferences();

    static Preferences *m_instance;

    SpectrogramSmoothing m_spectrogramSmoothing;
    SpectrogramXSmoothing m_spectrogramXSmoothing;
    float m_tuningFrequency;
    PropertyBoxLayout m_propertyBoxLayout;
    WindowType m_windowType;
    int m_resampleQuality;
    bool m_omitRecentTemps;
    QString m_tempDirRoot;
    bool m_resampleOnLoad;
    int m_viewFontSize;
    BackgroundMode m_backgroundMode;
    TimeToTextMode m_timeToTextMode;
    bool m_showSplash;
};

#endif
