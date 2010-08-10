/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

/*
   This is a modified version of a source file from the 
   Rosegarden MIDI and audio sequencer and notation editor.
   This file copyright 2000-2006 Chris Cannam, Richard Bown, and QMUL.
*/

#include <iostream>
#include <cassert>

#include "LADSPAPluginInstance.h"
#include "LADSPAPluginFactory.h"

#ifdef HAVE_LRDF
#include "lrdf.h"
#endif // HAVE_LRDF

//#define DEBUG_LADSPA 1

#include <cmath>


LADSPAPluginInstance::LADSPAPluginInstance(RealTimePluginFactory *factory,
					   int clientId,
					   QString identifier,
                                           int position,
					   unsigned long sampleRate,
					   size_t blockSize,
					   int idealChannelCount,
                                           const LADSPA_Descriptor* descriptor) :
    RealTimePluginInstance(factory, identifier),
    m_client(clientId),
    m_position(position),
    m_instanceCount(0),
    m_descriptor(descriptor),
    m_blockSize(blockSize),
    m_sampleRate(sampleRate),
    m_latencyPort(0),
    m_run(false),
    m_bypassed(false)
{
    init(idealChannelCount);

    if (m_audioPortsIn.size() == 0) {
        m_inputBuffers = 0;
    } else {
        m_inputBuffers  = new sample_t*[m_instanceCount * m_audioPortsIn.size()];
    }

    if (m_audioPortsOut.size() == 0) {
        m_outputBuffers = 0;
    } else {
        m_outputBuffers = new sample_t*[m_instanceCount * m_audioPortsOut.size()];
    }

    for (size_t i = 0; i < m_instanceCount * m_audioPortsIn.size(); ++i) {
	m_inputBuffers[i] = new sample_t[blockSize];
    }
    for (size_t i = 0; i < m_instanceCount * m_audioPortsOut.size(); ++i) {
	m_outputBuffers[i] = new sample_t[blockSize];
    }

    m_ownBuffers = true;

    instantiate(sampleRate);
    if (isOK()) {
	connectPorts();
	activate();
    }
}

std::string
LADSPAPluginInstance::getIdentifier() const
{
    return m_descriptor->Label;
}

std::string
LADSPAPluginInstance::getName() const
{
    return m_descriptor->Name;
}

std::string 
LADSPAPluginInstance::getDescription() const
{
    return "";
}

std::string
LADSPAPluginInstance::getMaker() const
{
    return m_descriptor->Maker;
}

int
LADSPAPluginInstance::getPluginVersion() const
{
    return -1;
}

std::string
LADSPAPluginInstance::getCopyright() const
{
    return m_descriptor->Copyright;
}

LADSPAPluginInstance::ParameterList
LADSPAPluginInstance::getParameterDescriptors() const
{
    ParameterList list;
    LADSPAPluginFactory *f = dynamic_cast<LADSPAPluginFactory *>(m_factory);
    
    for (unsigned int i = 0; i < m_controlPortsIn.size(); ++i) {
        
        ParameterDescriptor pd;
        unsigned int pn = m_controlPortsIn[i].first;

        pd.identifier = m_descriptor->PortNames[pn];
        pd.name = pd.identifier;
        pd.description = "";
        pd.minValue = f->getPortMinimum(m_descriptor, pn);
        pd.maxValue = f->getPortMaximum(m_descriptor, pn);
        pd.defaultValue = f->getPortDefault(m_descriptor, pn);

        float q = f->getPortQuantization(m_descriptor, pn);
        if (q == 0.0) {
            pd.isQuantized = false;
        } else {
            pd.isQuantized = true;
            pd.quantizeStep = q;
        }

        bool haveLabels = false;

#ifdef HAVE_LRDF
        if (pd.isQuantized && pd.quantizeStep == 1.0) {

            lrdf_defaults *defaults =
                lrdf_get_scale_values(m_descriptor->UniqueID, pn);

            if (defaults) {
                if (defaults->count > 0) {
                    std::map<int, std::string> values;
                    size_t v = 0;
                    for (size_t i = 0; i < defaults->count; ++i) {
                        v = size_t(lrintf(fabsf(defaults->items[i].value)));
                        values[v] = defaults->items[i].label;
                    }
                    for (size_t i = 0; i <= v; ++i) {
                        pd.valueNames.push_back(values[i]);
                    }
                    haveLabels = true;
                }
                lrdf_free_setting_values(defaults);
            }
        }
#endif

        if (haveLabels) {
            pd.name = QString(pd.name.c_str())
                .replace(QRegExp("\\([^\\(\\)]+=[^\\(\\)]+\\)$"), "")
                .toStdString();
        } else {
            static QRegExp unitRE("[\\[\\(]([A-Za-z0-9/]+)[\\)\\]]$");
            if (unitRE.indexIn(pd.name.c_str()) >= 0) {
                pd.unit = unitRE.cap(1).toStdString();
                pd.name = QString(pd.name.c_str())
                    .replace(unitRE, "").toStdString();
            }
        }

        list.push_back(pd);
    }

    return list;
}

float
LADSPAPluginInstance::getParameter(std::string id) const
{
    for (unsigned int i = 0; i < m_controlPortsIn.size(); ++i) {
        if (id == m_descriptor->PortNames[m_controlPortsIn[i].first]) {
            return getParameterValue(i);
        }
    }

    return 0.0;
}

void
LADSPAPluginInstance::setParameter(std::string id, float value)
{
    for (unsigned int i = 0; i < m_controlPortsIn.size(); ++i) {
        if (id == m_descriptor->PortNames[m_controlPortsIn[i].first]) {
#ifdef DEBUG_LADSPA
            std::cerr << "LADSPAPluginInstance::setParameter: Found id "
                      << id << " at control port " << i << std::endl;
#endif
            setParameterValue(i, value);
            break;
        }
    }
}    

void
LADSPAPluginInstance::init(int idealChannelCount)
{
#ifdef DEBUG_LADSPA
    std::cerr << "LADSPAPluginInstance::init(" << idealChannelCount << "): plugin has "
	      << m_descriptor->PortCount << " ports" << std::endl;
#endif

    // Discover ports numbers and identities
    //
    for (unsigned long i = 0; i < m_descriptor->PortCount; ++i) {

        if (LADSPA_IS_PORT_AUDIO(m_descriptor->PortDescriptors[i])) {

            if (LADSPA_IS_PORT_INPUT(m_descriptor->PortDescriptors[i])) {
#ifdef DEBUG_LADSPA
		std::cerr << "LADSPAPluginInstance::init: port " << i << " is audio in" << std::endl;
#endif
                m_audioPortsIn.push_back(i);
	    } else {
#ifdef DEBUG_LADSPA
		std::cerr << "LADSPAPluginInstance::init: port " << i << " is audio out" << std::endl;
#endif
                m_audioPortsOut.push_back(i);
	    }

        } else if (LADSPA_IS_PORT_CONTROL(m_descriptor->PortDescriptors[i])) {

	    if (LADSPA_IS_PORT_INPUT(m_descriptor->PortDescriptors[i])) {

#ifdef DEBUG_LADSPA
		std::cerr << "LADSPAPluginInstance::init: port " << i << " is control in" << std::endl;
#endif
		LADSPA_Data *data = new LADSPA_Data(0.0);
		m_controlPortsIn.push_back(
                    std::pair<unsigned long, LADSPA_Data*>(i, data));

	    } else {

#ifdef DEBUG_LADSPA
		std::cerr << "LADSPAPluginInstance::init: port " << i << " is control out" << std::endl;
#endif
		LADSPA_Data *data = new LADSPA_Data(0.0);
		m_controlPortsOut.push_back(
                    std::pair<unsigned long, LADSPA_Data*>(i, data));
		if (!strcmp(m_descriptor->PortNames[i], "latency") ||
		    !strcmp(m_descriptor->PortNames[i], "_latency")) {
#ifdef DEBUG_LADSPA
		    std::cerr << "Wooo! We have a latency port!" << std::endl;
#endif
		    m_latencyPort = data;
		}

	    }
        }
#ifdef DEBUG_LADSPA
        else
            std::cerr << "LADSPAPluginInstance::init - "
                      << "unrecognised port type" << std::endl;
#endif
    }

    m_instanceCount = 1;

    if (idealChannelCount > 0) {
	if (m_audioPortsIn.size() == 1) {
	    // mono plugin: duplicate it if need be
	    m_instanceCount = idealChannelCount;
	}
    }
}

size_t
LADSPAPluginInstance::getLatency()
{
    if (m_latencyPort) {
	if (!m_run) {
            for (size_t i = 0; i < getAudioInputCount(); ++i) {
                for (size_t j = 0; j < m_blockSize; ++j) {
                    m_inputBuffers[i][j] = 0.f;
                }
            }
            run(Vamp::RealTime::zeroTime);
        }
	if (*m_latencyPort > 0) return (size_t)*m_latencyPort;
    }
    return 0;
}

void
LADSPAPluginInstance::silence()
{
    if (isOK()) {
	deactivate();
	activate();
    }
}

void
LADSPAPluginInstance::setIdealChannelCount(size_t channels)
{
    if (m_audioPortsIn.size() != 1 || channels == m_instanceCount) {
	silence();
	return;
    }

    if (isOK()) {
	deactivate();
    }

    //!!! don't we need to reallocate inputBuffers and outputBuffers?

    cleanup();
    m_instanceCount = channels;
    instantiate(m_sampleRate);
    if (isOK()) {
	connectPorts();
	activate();
    }
}


LADSPAPluginInstance::~LADSPAPluginInstance()
{
#ifdef DEBUG_LADSPA
    std::cerr << "LADSPAPluginInstance::~LADSPAPluginInstance" << std::endl;
#endif

    if (m_instanceHandles.size() != 0) { // "isOK()"
	deactivate();
    }

    cleanup();

    for (unsigned int i = 0; i < m_controlPortsIn.size(); ++i)
        delete m_controlPortsIn[i].second;

    for (unsigned int i = 0; i < m_controlPortsOut.size(); ++i)
        delete m_controlPortsOut[i].second;

    m_controlPortsIn.clear();
    m_controlPortsOut.clear();

    if (m_ownBuffers) {
	for (size_t i = 0; i < m_instanceCount * m_audioPortsIn.size(); ++i) {
	    delete[] m_inputBuffers[i];
	}
	for (size_t i = 0; i < m_instanceCount * m_audioPortsOut.size(); ++i) {
	    delete[] m_outputBuffers[i];
	}

	delete[] m_inputBuffers;
	delete[] m_outputBuffers;
    }

    m_audioPortsIn.clear();
    m_audioPortsOut.clear();
}


void
LADSPAPluginInstance::instantiate(unsigned long sampleRate)
{
#ifdef DEBUG_LADSPA
    std::cout << "LADSPAPluginInstance::instantiate - plugin unique id = "
              << m_descriptor->UniqueID << std::endl;
#endif
    if (!m_descriptor) return;

    if (!m_descriptor->instantiate) {
	std::cerr << "Bad plugin: plugin id " << m_descriptor->UniqueID
		  << ":" << m_descriptor->Label
		  << " has no instantiate method!" << std::endl;
	return;
    }

    for (size_t i = 0; i < m_instanceCount; ++i) {
	m_instanceHandles.push_back
	    (m_descriptor->instantiate(m_descriptor, sampleRate));
    }
}

void
LADSPAPluginInstance::activate()
{
    if (!m_descriptor || !m_descriptor->activate) return;

    for (std::vector<LADSPA_Handle>::iterator hi = m_instanceHandles.begin();
	 hi != m_instanceHandles.end(); ++hi) {
	m_descriptor->activate(*hi);
    }
}

void
LADSPAPluginInstance::connectPorts()
{
    if (!m_descriptor || !m_descriptor->connect_port) return;

    assert(sizeof(LADSPA_Data) == sizeof(float));
    assert(sizeof(sample_t) == sizeof(float));

    LADSPAPluginFactory *f = dynamic_cast<LADSPAPluginFactory *>(m_factory);
    int inbuf = 0, outbuf = 0;

    for (std::vector<LADSPA_Handle>::iterator hi = m_instanceHandles.begin();
	 hi != m_instanceHandles.end(); ++hi) {

	for (unsigned int i = 0; i < m_audioPortsIn.size(); ++i) {
	    m_descriptor->connect_port(*hi,
				       m_audioPortsIn[i],
				       (LADSPA_Data *)m_inputBuffers[inbuf]);
	    ++inbuf;
	}

	for (unsigned int i = 0; i < m_audioPortsOut.size(); ++i) {
	    m_descriptor->connect_port(*hi,
				       m_audioPortsOut[i],
				       (LADSPA_Data *)m_outputBuffers[outbuf]);
	    ++outbuf;
	}

	// If there is more than one instance, they all share the same
	// control port ins (and outs, for the moment, because we
	// don't actually do anything with the outs anyway -- but they
	// do have to be connected as the plugin can't know if they're
	// not and will write to them anyway).

	for (unsigned int i = 0; i < m_controlPortsIn.size(); ++i) {
	    m_descriptor->connect_port(*hi,
				       m_controlPortsIn[i].first,
				       m_controlPortsIn[i].second);
            if (f) {
                float defaultValue = f->getPortDefault
                    (m_descriptor, m_controlPortsIn[i].first);
                *m_controlPortsIn[i].second = defaultValue;
            }
	}

	for (unsigned int i = 0; i < m_controlPortsOut.size(); ++i) {
	    m_descriptor->connect_port(*hi,
				       m_controlPortsOut[i].first,
				       m_controlPortsOut[i].second);
	}
    }
}

unsigned int
LADSPAPluginInstance::getParameterCount() const
{
    return m_controlPortsIn.size();
}

void
LADSPAPluginInstance::setParameterValue(unsigned int parameter, float value)
{
    if (parameter >= m_controlPortsIn.size()) return;

    unsigned int portNumber = m_controlPortsIn[parameter].first;

    LADSPAPluginFactory *f = dynamic_cast<LADSPAPluginFactory *>(m_factory);
    if (f) {
	if (value < f->getPortMinimum(m_descriptor, portNumber)) {
	    value = f->getPortMinimum(m_descriptor, portNumber);
	}
	if (value > f->getPortMaximum(m_descriptor, portNumber)) {
	    value = f->getPortMaximum(m_descriptor, portNumber);
	}
    }

    (*m_controlPortsIn[parameter].second) = value;
}

float
LADSPAPluginInstance::getControlOutputValue(size_t output) const
{
    if (output > m_controlPortsOut.size()) return 0.0;
    return (*m_controlPortsOut[output].second);
}

float
LADSPAPluginInstance::getParameterValue(unsigned int parameter) const
{
    if (parameter >= m_controlPortsIn.size()) return 0.0;
    return (*m_controlPortsIn[parameter].second);
}

float
LADSPAPluginInstance::getParameterDefault(unsigned int parameter) const
{
    if (parameter >= m_controlPortsIn.size()) return 0.0;

    LADSPAPluginFactory *f = dynamic_cast<LADSPAPluginFactory *>(m_factory);
    if (f) {
	return f->getPortDefault(m_descriptor, m_controlPortsIn[parameter].first);
    } else {
	return 0.0f;
    }
}

int
LADSPAPluginInstance::getParameterDisplayHint(unsigned int parameter) const
{
    if (parameter >= m_controlPortsIn.size()) return 0.0;

    LADSPAPluginFactory *f = dynamic_cast<LADSPAPluginFactory *>(m_factory);
    if (f) {
	return f->getPortDisplayHint(m_descriptor, m_controlPortsIn[parameter].first);
    } else {
	return PortHint::NoHint;
    }
}

void
LADSPAPluginInstance::run(const Vamp::RealTime &, size_t count)
{
    if (!m_descriptor || !m_descriptor->run) return;

    if (count == 0) count = m_blockSize;

    for (std::vector<LADSPA_Handle>::iterator hi = m_instanceHandles.begin();
	 hi != m_instanceHandles.end(); ++hi) {

        m_descriptor->run(*hi, count);
    }

    m_run = true;
}

void
LADSPAPluginInstance::deactivate()
{
    if (!m_descriptor || !m_descriptor->deactivate) return;

    for (std::vector<LADSPA_Handle>::iterator hi = m_instanceHandles.begin();
	 hi != m_instanceHandles.end(); ++hi) {
        m_descriptor->deactivate(*hi);
    }
}

void
LADSPAPluginInstance::cleanup()
{
    if (!m_descriptor) return;

    if (!m_descriptor->cleanup) {
	std::cerr << "Bad plugin: plugin id " << m_descriptor->UniqueID
		  << ":" << m_descriptor->Label
		  << " has no cleanup method!" << std::endl;
	return;
    }

    for (std::vector<LADSPA_Handle>::iterator hi = m_instanceHandles.begin();
	 hi != m_instanceHandles.end(); ++hi) {
	m_descriptor->cleanup(*hi);
    }

    m_instanceHandles.clear();
}


