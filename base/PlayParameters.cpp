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

#include "PlayParameters.h"

#include <iostream>

#include <QTextStream>

void
PlayParameters::copyFrom(const PlayParameters *pp)
{
    bool changed = false;

    if (m_playMuted != pp->isPlayMuted()) {
        m_playMuted = pp->isPlayMuted();
        emit playMutedChanged(m_playMuted);
        emit playAudibleChanged(!m_playMuted);
        changed = true;
    }

    if (m_playPan != pp->getPlayPan()) {
        m_playPan = pp->getPlayPan();
        emit playPanChanged(m_playPan);
        changed = true;
    }

    if (m_playGain != pp->getPlayGain()) {
        m_playGain = pp->getPlayGain();
        emit playGainChanged(m_playGain);
        changed = true;
    }

    if (m_playPluginId != pp->getPlayPluginId()) {
        m_playPluginId = pp->getPlayPluginId();
        emit playPluginIdChanged(m_playPluginId);
        changed = true;
    }
    
    if (m_playPluginConfiguration != pp->getPlayPluginConfiguration()) {
        m_playPluginConfiguration = pp->getPlayPluginConfiguration();
        emit playPluginConfigurationChanged(m_playPluginConfiguration);
        changed = true;
    }

    if (changed) emit playParametersChanged();
}

void
PlayParameters::toXml(QTextStream &stream,
                      QString indent,
                      QString extraAttributes) const
{
    stream << indent;
    stream << QString("<playparameters mute=\"%1\" pan=\"%2\" gain=\"%3\" pluginId=\"%4\" %6")
        .arg(m_playMuted ? "true" : "false")
        .arg(m_playPan)
        .arg(m_playGain)
        .arg(m_playPluginId)
        .arg(extraAttributes);
    if (m_playPluginConfiguration != "") {
        stream << ">\n  " << indent << m_playPluginConfiguration
               << "\n" << indent << "</playparameters>\n";
    } else {
        stream << "/>\n";
    }
}

void
PlayParameters::setPlayMuted(bool muted)
{
//    std::cerr << "PlayParameters: setPlayMuted(" << muted << ")" << std::endl;
    if (m_playMuted != muted) {
        m_playMuted = muted;
        emit playMutedChanged(muted);
        emit playAudibleChanged(!muted);
        emit playParametersChanged();
    }
}

void
PlayParameters::setPlayAudible(bool audible)
{
//    std::cerr << "PlayParameters(" << this << "): setPlayAudible(" << audible << ")" << std::endl;
    setPlayMuted(!audible);
}

void
PlayParameters::setPlayPan(float pan)
{
    if (m_playPan != pan) {
        m_playPan = pan;
        emit playPanChanged(pan);
        emit playParametersChanged();
    }
}

void
PlayParameters::setPlayGain(float gain)
{
    if (m_playGain != gain) {
        m_playGain = gain;
        emit playGainChanged(gain);
        emit playParametersChanged();
    }
}

void
PlayParameters::setPlayPluginId(QString id)
{
    if (m_playPluginId != id) {
        m_playPluginId = id;
        emit playPluginIdChanged(id);
        emit playParametersChanged();
    }
}

void
PlayParameters::setPlayPluginConfiguration(QString configuration)
{
    if (m_playPluginConfiguration != configuration) {
        m_playPluginConfiguration = configuration;
//        std::cerr << "PlayParameters(" << this << "): setPlayPluginConfiguration to \"" << configuration.toStdString() << "\"" << std::endl;
        emit playPluginConfigurationChanged(configuration);
        emit playParametersChanged();
    }
}


