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

#ifndef _PLAY_PARAMETERS_H_
#define _PLAY_PARAMETERS_H_

#include <QObject>

#include "XmlExportable.h"

class PlayParameters : public QObject, public XmlExportable
{
    Q_OBJECT

public:
    PlayParameters() : m_playMuted(false), m_playPan(0.0), m_playGain(1.0) { }

    virtual bool isPlayMuted() const { return m_playMuted; }
    virtual bool isPlayAudible() const { return !m_playMuted; }
    virtual float getPlayPan() const { return m_playPan; } // -1.0 -> 1.0
    virtual float getPlayGain() const { return m_playGain; }

    virtual QString getPlayPluginId() const { return m_playPluginId; } 
    virtual QString getPlayPluginConfiguration() const { return m_playPluginConfiguration; }

    virtual void copyFrom(const PlayParameters *);

    virtual void toXml(QTextStream &stream,
                       QString indent = "",
                       QString extraAttributes = "") const;

public slots:
    virtual void setPlayMuted(bool muted);
    virtual void setPlayAudible(bool nonMuted);
    virtual void setPlayPan(float pan);
    virtual void setPlayGain(float gain);
    virtual void setPlayPluginId(QString id);
    virtual void setPlayPluginConfiguration(QString configuration);

signals:
    void playParametersChanged();
    void playMutedChanged(bool);
    void playAudibleChanged(bool);
    void playPanChanged(float);
    void playGainChanged(float);
    void playPluginIdChanged(QString);
    void playPluginConfigurationChanged(QString);

protected:
    bool m_playMuted;
    float m_playPan;
    float m_playGain;
    QString m_playPluginId;
    QString m_playPluginConfiguration;

private:
    PlayParameters(const PlayParameters &);
    PlayParameters &operator=(const PlayParameters &);
};

#endif

    

    
