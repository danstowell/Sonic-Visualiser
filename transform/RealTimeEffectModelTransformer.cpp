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

#include "RealTimeEffectModelTransformer.h"

#include "plugin/RealTimePluginFactory.h"
#include "plugin/RealTimePluginInstance.h"
#include "plugin/PluginXml.h"

#include "data/model/Model.h"
#include "data/model/SparseTimeValueModel.h"
#include "data/model/DenseTimeValueModel.h"
#include "data/model/WritableWaveFileModel.h"
#include "data/model/WaveFileModel.h"

#include "TransformFactory.h"

#include <iostream>

RealTimeEffectModelTransformer::RealTimeEffectModelTransformer(Input in,
                                                               const Transform &transform) :
    ModelTransformer(in, transform),
    m_plugin(0)
{
    m_units = TransformFactory::getInstance()->getTransformUnits
        (transform.getIdentifier());
    m_outputNo =
        (transform.getOutput() == "A") ? -1 : transform.getOutput().toInt();

    QString pluginId = transform.getPluginIdentifier();

    if (!m_transform.getBlockSize()) m_transform.setBlockSize(1024);

//    std::cerr << "RealTimeEffectModelTransformer::RealTimeEffectModelTransformer: plugin " << pluginId.toStdString() << ", output " << output << std::endl;

    RealTimePluginFactory *factory =
	RealTimePluginFactory::instanceFor(pluginId);

    if (!factory) {
	std::cerr << "RealTimeEffectModelTransformer: No factory available for plugin id \""
		  << pluginId.toStdString() << "\"" << std::endl;
	return;
    }

    DenseTimeValueModel *input = getConformingInput();
    if (!input) return;

    m_plugin = factory->instantiatePlugin(pluginId, 0, 0,
                                          input->getSampleRate(),
                                          m_transform.getBlockSize(),
                                          input->getChannelCount());

    if (!m_plugin) {
	std::cerr << "RealTimeEffectModelTransformer: Failed to instantiate plugin \""
		  << pluginId.toStdString() << "\"" << std::endl;
	return;
    }

    TransformFactory::getInstance()->setPluginParameters(m_transform, m_plugin);

    if (m_outputNo >= 0 &&
        m_outputNo >= int(m_plugin->getControlOutputCount())) {
        std::cerr << "RealTimeEffectModelTransformer: Plugin has fewer than desired " << m_outputNo << " control outputs" << std::endl;
        return;
    }

    if (m_outputNo == -1) {

        size_t outputChannels = m_plugin->getAudioOutputCount();
        if (outputChannels > input->getChannelCount()) {
            outputChannels = input->getChannelCount();
        }

        WritableWaveFileModel *model = new WritableWaveFileModel
            (input->getSampleRate(), outputChannels);

        m_output = model;

    } else {
	
        SparseTimeValueModel *model = new SparseTimeValueModel
            (input->getSampleRate(), m_transform.getBlockSize(), 0.0, 0.0, false);

        if (m_units != "") model->setScaleUnits(m_units);

        m_output = model;
    }
}

RealTimeEffectModelTransformer::~RealTimeEffectModelTransformer()
{
    delete m_plugin;
}

DenseTimeValueModel *
RealTimeEffectModelTransformer::getConformingInput()
{
    DenseTimeValueModel *dtvm =
	dynamic_cast<DenseTimeValueModel *>(getInputModel());
    if (!dtvm) {
	std::cerr << "RealTimeEffectModelTransformer::getConformingInput: WARNING: Input model is not conformable to DenseTimeValueModel" << std::endl;
    }
    return dtvm;
}

void
RealTimeEffectModelTransformer::run()
{
    DenseTimeValueModel *input = getConformingInput();
    if (!input) return;

    while (!input->isReady() && !m_abandoned) {
        std::cerr << "RealTimeEffectModelTransformer::run: Waiting for input model to be ready..." << std::endl;
        usleep(500000);
    }
    if (m_abandoned) return;

    SparseTimeValueModel *stvm = dynamic_cast<SparseTimeValueModel *>(m_output);
    WritableWaveFileModel *wwfm = dynamic_cast<WritableWaveFileModel *>(m_output);
    if (!stvm && !wwfm) return;

    if (stvm && (m_outputNo >= int(m_plugin->getControlOutputCount()))) return;

    size_t sampleRate = input->getSampleRate();
    size_t channelCount = input->getChannelCount();
    if (!wwfm && m_input.getChannel() != -1) channelCount = 1;

    long blockSize = m_plugin->getBufferSize();

    float **inbufs = m_plugin->getAudioInputBuffers();

    long startFrame = m_input.getModel()->getStartFrame();
    long   endFrame = m_input.getModel()->getEndFrame();
    
    RealTime contextStartRT = m_transform.getStartTime();
    RealTime contextDurationRT = m_transform.getDuration();

    long contextStart =
        RealTime::realTime2Frame(contextStartRT, sampleRate);

    long contextDuration =
        RealTime::realTime2Frame(contextDurationRT, sampleRate);

    if (contextStart == 0 || contextStart < startFrame) {
        contextStart = startFrame;
    }

    if (contextDuration == 0) {
        contextDuration = endFrame - contextStart;
    }
    if (contextStart + contextDuration > endFrame) {
        contextDuration = endFrame - contextStart;
    }

    if (wwfm) {
        wwfm->setStartFrame(contextStart);
    }

    long blockFrame = contextStart;

    long prevCompletion = 0;

    long latency = m_plugin->getLatency();

    while (blockFrame < contextStart + contextDuration + latency &&
           !m_abandoned) {

	long completion =
	    (((blockFrame - contextStart) / blockSize) * 99) /
	    ((contextDuration) / blockSize);

	long got = 0;

	if (channelCount == 1) {
            if (inbufs && inbufs[0]) {
                got = input->getData
                    (m_input.getChannel(), blockFrame, blockSize, inbufs[0]);
                while (got < blockSize) {
                    inbufs[0][got++] = 0.0;
                }          
            }
            for (size_t ch = 1; ch < m_plugin->getAudioInputCount(); ++ch) {
                for (long i = 0; i < blockSize; ++i) {
                    inbufs[ch][i] = inbufs[0][i];
                }
            }
	} else {
            if (inbufs && inbufs[0]) {
                got = input->getData(0, channelCount - 1,
                                     blockFrame, blockSize,
                                     inbufs);
                while (got < blockSize) {
                    for (size_t ch = 0; ch < channelCount; ++ch) {
                        inbufs[ch][got] = 0.0;
                    }
                    ++got;
                }
            }
            for (size_t ch = channelCount; ch < m_plugin->getAudioInputCount(); ++ch) {
                for (long i = 0; i < blockSize; ++i) {
                    inbufs[ch][i] = inbufs[ch % channelCount][i];
                }
            }
	}

/*
        std::cerr << "Input for plugin: " << m_plugin->getAudioInputCount() << " channels "<< std::endl;

        for (size_t ch = 0; ch < m_plugin->getAudioInputCount(); ++ch) {
            std::cerr << "Input channel " << ch << std::endl;
            for (size_t i = 0; i < 100; ++i) {
                std::cerr << inbufs[ch][i] << " ";
                if (isnan(inbufs[ch][i])) {
                    std::cerr << "\n\nWARNING: NaN in audio input" << std::endl;
                }
            }
        }
*/

        m_plugin->run(Vamp::RealTime::frame2RealTime(blockFrame, sampleRate));

        if (stvm) {

            float value = m_plugin->getControlOutputValue(m_outputNo);

            long pointFrame = blockFrame;
            if (pointFrame > latency) pointFrame -= latency;
            else pointFrame = 0;

            stvm->addPoint(SparseTimeValueModel::Point
                           (pointFrame, value, ""));

        } else if (wwfm) {

            float **outbufs = m_plugin->getAudioOutputBuffers();

            if (outbufs) {

                if (blockFrame >= latency) {
                    long writeSize = std::min
                        (blockSize,
                         contextStart + contextDuration + latency - blockFrame);
                    wwfm->addSamples(outbufs, writeSize);
                } else if (blockFrame + blockSize >= latency) {
                    long offset = latency - blockFrame;
                    long count = blockSize - offset;
                    float **tmp = new float *[channelCount];
                    for (size_t c = 0; c < channelCount; ++c) {
                        tmp[c] = outbufs[c] + offset;
                    }
                    wwfm->addSamples(tmp, count);
                    delete[] tmp;
                }
            }
        }

	if (blockFrame == contextStart || completion > prevCompletion) {
	    if (stvm) stvm->setCompletion(completion);
	    if (wwfm) wwfm->setCompletion(completion);
	    prevCompletion = completion;
	}
        
	blockFrame += blockSize;
    }

    if (m_abandoned) return;
    
    if (stvm) stvm->setCompletion(100);
    if (wwfm) wwfm->setCompletion(100);
}

