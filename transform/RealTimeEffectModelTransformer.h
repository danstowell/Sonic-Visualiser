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

#ifndef _REAL_TIME_PLUGIN_TRANSFORMER_H_
#define _REAL_TIME_PLUGIN_TRANSFORMER_H_

#include "ModelTransformer.h"
#include "plugin/RealTimePluginInstance.h"

class DenseTimeValueModel;

class RealTimeEffectModelTransformer : public ModelTransformer
{
public:
    RealTimeEffectModelTransformer(Input input,
                                   const Transform &transform);
    virtual ~RealTimeEffectModelTransformer();

protected:
    virtual void run();

    QString m_units;
    RealTimePluginInstance *m_plugin;
    int m_outputNo;

    // just casts
    DenseTimeValueModel *getConformingInput();
};

#endif

