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
   This file copyright 2000-2006 Chris Cannam and Richard Bown.
*/

#include "LADSPAPluginFactory.h"
#include <iostream>

#include <QDir>
#include <QFile>
#include <QTextStream>

#include <cmath>

#include "LADSPAPluginInstance.h"
#include "PluginIdentifier.h"

#include "system/System.h"
#include "base/Preferences.h"
#include "base/Profiler.h"

//#define DEBUG_LADSPA_PLUGIN_FACTORY 1

#ifdef HAVE_LRDF
#include "lrdf.h"
#endif // HAVE_LRDF


LADSPAPluginFactory::LADSPAPluginFactory()
{
#ifdef HAVE_LRDF
    lrdf_init();
#endif
}
 
LADSPAPluginFactory::~LADSPAPluginFactory()
{
    for (std::set<RealTimePluginInstance *>::iterator i = m_instances.begin();
	 i != m_instances.end(); ++i) {
	(*i)->setFactory(0);
	delete *i;
    }
    m_instances.clear();
    unloadUnusedLibraries();

#ifdef HAVE_LRDF
    lrdf_cleanup();
#endif // HAVE_LRDF
}

const std::vector<QString> &
LADSPAPluginFactory::getPluginIdentifiers() const
{
    return m_identifiers;
}

void
LADSPAPluginFactory::enumeratePlugins(std::vector<QString> &list)
{
    Profiler profiler("LADSPAPluginFactory::enumeratePlugins");

    for (std::vector<QString>::iterator i = m_identifiers.begin();
	 i != m_identifiers.end(); ++i) {

	const LADSPA_Descriptor *descriptor = getLADSPADescriptor(*i);

	if (!descriptor) {
	    std::cerr << "WARNING: LADSPAPluginFactory::enumeratePlugins: couldn't get descriptor for identifier " << i->toStdString() << std::endl;
	    continue;
	}
	
	list.push_back(*i);
	list.push_back(descriptor->Name);
	list.push_back(QString("%1").arg(descriptor->UniqueID));
	list.push_back(descriptor->Label);
	list.push_back(descriptor->Maker);
	list.push_back(descriptor->Copyright);
	list.push_back("false"); // is synth
	list.push_back("false"); // is grouped

	if (m_taxonomy.find(*i) != m_taxonomy.end() && m_taxonomy[*i] != "") {
//		std::cerr << "LADSPAPluginFactory: cat for " << i->toStdString()<< " found in taxonomy as " << m_taxonomy[descriptor->UniqueID] << std::endl;
	    list.push_back(m_taxonomy[*i]);
	} else {
	    list.push_back("");
//		std::cerr << "LADSPAPluginFactory: cat for " << i->toStdString() << " not found (despite having " << m_fallbackCategories.size() << " fallbacks)" << std::endl;
	    
	}

	list.push_back(QString("%1").arg(descriptor->PortCount));

	for (unsigned long p = 0; p < descriptor->PortCount; ++p) {

	    int type = 0;
	    if (LADSPA_IS_PORT_CONTROL(descriptor->PortDescriptors[p])) {
		type |= PortType::Control;
	    } else {
		type |= PortType::Audio;
	    }
	    if (LADSPA_IS_PORT_INPUT(descriptor->PortDescriptors[p])) {
		type |= PortType::Input;
	    } else {
		type |= PortType::Output;
	    }

	    list.push_back(QString("%1").arg(p));
	    list.push_back(descriptor->PortNames[p]);
	    list.push_back(QString("%1").arg(type));
	    list.push_back(QString("%1").arg(getPortDisplayHint(descriptor, p)));
	    list.push_back(QString("%1").arg(getPortMinimum(descriptor, p)));
	    list.push_back(QString("%1").arg(getPortMaximum(descriptor, p)));
	    list.push_back(QString("%1").arg(getPortDefault(descriptor, p)));
	}
    }

    unloadUnusedLibraries();
}
	
const RealTimePluginDescriptor *
LADSPAPluginFactory::getPluginDescriptor(QString identifier) const
{
    std::map<QString, RealTimePluginDescriptor *>::const_iterator i =
        m_rtDescriptors.find(identifier);

    if (i != m_rtDescriptors.end()) {
        return i->second;
    } 

    return 0;
}

float
LADSPAPluginFactory::getPortMinimum(const LADSPA_Descriptor *descriptor, int port)
{
    LADSPA_PortRangeHintDescriptor d =
	descriptor->PortRangeHints[port].HintDescriptor;

    float minimum = 0.0;
		
    if (LADSPA_IS_HINT_BOUNDED_BELOW(d)) {
	float lb = descriptor->PortRangeHints[port].LowerBound;
	minimum = lb;
    } else if (LADSPA_IS_HINT_BOUNDED_ABOVE(d)) {
	float ub = descriptor->PortRangeHints[port].UpperBound;
	minimum = std::min(0.0, ub - 1.0);
    }
    
    if (LADSPA_IS_HINT_SAMPLE_RATE(d)) {
	minimum *= m_sampleRate;
    }

    if (LADSPA_IS_HINT_LOGARITHMIC(d)) {
        if (minimum == 0.f) minimum = 1.f;
    }

    return minimum;
}

float
LADSPAPluginFactory::getPortMaximum(const LADSPA_Descriptor *descriptor, int port)
{
    LADSPA_PortRangeHintDescriptor d =
	descriptor->PortRangeHints[port].HintDescriptor;

    float maximum = 1.0;
    
    if (LADSPA_IS_HINT_BOUNDED_ABOVE(d)) {
	float ub = descriptor->PortRangeHints[port].UpperBound;
	maximum = ub;
    } else {
	float lb = descriptor->PortRangeHints[port].LowerBound;
	maximum = lb + 1.0;
    }
    
    if (LADSPA_IS_HINT_SAMPLE_RATE(d)) {
	maximum *= m_sampleRate;
    }

    return maximum;
}

float
LADSPAPluginFactory::getPortDefault(const LADSPA_Descriptor *descriptor, int port)
{
    float minimum = getPortMinimum(descriptor, port);
    float maximum = getPortMaximum(descriptor, port);
    float deft;

    if (m_portDefaults.find(descriptor->UniqueID) != 
	m_portDefaults.end()) {
	if (m_portDefaults[descriptor->UniqueID].find(port) !=
	    m_portDefaults[descriptor->UniqueID].end()) {

	    deft = m_portDefaults[descriptor->UniqueID][port];
	    if (deft < minimum) deft = minimum;
	    if (deft > maximum) deft = maximum;
	    return deft;
	}
    }

    LADSPA_PortRangeHintDescriptor d =
	descriptor->PortRangeHints[port].HintDescriptor;

    bool logarithmic = LADSPA_IS_HINT_LOGARITHMIC(d);
    
    float logmin = 0, logmax = 0;
    if (logarithmic) {
        float thresh = powf(10, -10);
        if (minimum < thresh) logmin = -10;
        else logmin = log10f(minimum);
        if (maximum < thresh) logmax = -10;
        else logmax = log10f(maximum);
    }

//    std::cerr << "LADSPAPluginFactory::getPortDefault: hint = " << d << std::endl;

    if (!LADSPA_IS_HINT_HAS_DEFAULT(d)) {
	
	deft = minimum;
	
    } else if (LADSPA_IS_HINT_DEFAULT_MINIMUM(d)) {
	
	deft = minimum;
	
    } else if (LADSPA_IS_HINT_DEFAULT_LOW(d)) {
	
	if (logarithmic) {
	    deft = powf(10, logmin * 0.75 + logmax * 0.25);
	} else {
	    deft = minimum * 0.75 + maximum * 0.25;
	}
	
    } else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(d)) {
	
	if (logarithmic) {
	    deft = powf(10, logmin * 0.5 + logmax * 0.5);
	} else {
	    deft = minimum * 0.5 + maximum * 0.5;
	}
	
    } else if (LADSPA_IS_HINT_DEFAULT_HIGH(d)) {
	
	if (logarithmic) {
	    deft = powf(10, logmin * 0.25 + logmax * 0.75);
	} else {
	    deft = minimum * 0.25 + maximum * 0.75;
	}
	
    } else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(d)) {
	
	deft = maximum;
	
    } else if (LADSPA_IS_HINT_DEFAULT_0(d)) {
	
	deft = 0.0;
	
    } else if (LADSPA_IS_HINT_DEFAULT_1(d)) {
	
	deft = 1.0;
	
    } else if (LADSPA_IS_HINT_DEFAULT_100(d)) {
	
	deft = 100.0;
	
    } else if (LADSPA_IS_HINT_DEFAULT_440(d)) {
	
//	deft = 440.0;
        deft = Preferences::getInstance()->getTuningFrequency();
	
    } else {
	
	deft = minimum;
    }

//!!! No -- the min and max have already been multiplied by the rate,
//so it would happen twice if we did it here -- and e.g. DEFAULT_440
//doesn't want to be multiplied by the rate either
    
//    if (LADSPA_IS_HINT_SAMPLE_RATE(d)) {
//	deft *= m_sampleRate;
//    }

    return deft;
}

float
LADSPAPluginFactory::getPortQuantization(const LADSPA_Descriptor *descriptor, int port)
{
    int displayHint = getPortDisplayHint(descriptor, port);
    if (displayHint & PortHint::Toggled) {
        return lrintf(getPortMaximum(descriptor, port)) - 
            lrintf(getPortMinimum(descriptor, port));
    }
    if (displayHint & PortHint::Integer) {
        return 1.0;
    }
    return 0.0;
}

int
LADSPAPluginFactory::getPortDisplayHint(const LADSPA_Descriptor *descriptor, int port)
{
    LADSPA_PortRangeHintDescriptor d =
	descriptor->PortRangeHints[port].HintDescriptor;
    int hint = PortHint::NoHint;

    if (LADSPA_IS_HINT_TOGGLED(d)) hint |= PortHint::Toggled;
    if (LADSPA_IS_HINT_INTEGER(d)) hint |= PortHint::Integer;
    if (LADSPA_IS_HINT_LOGARITHMIC(d)) hint |= PortHint::Logarithmic;

    return hint;
}


RealTimePluginInstance *
LADSPAPluginFactory::instantiatePlugin(QString identifier,
				       int instrument,
				       int position,
				       unsigned int sampleRate,
				       unsigned int blockSize,
				       unsigned int channels)
{
    Profiler profiler("LADSPAPluginFactory::instantiatePlugin");

    const LADSPA_Descriptor *descriptor = getLADSPADescriptor(identifier);

    if (descriptor) {

	LADSPAPluginInstance *instance =
	    new LADSPAPluginInstance
	    (this, instrument, identifier, position, sampleRate, blockSize, channels,
	     descriptor);

	m_instances.insert(instance);

#ifdef DEBUG_LADSPA_PLUGIN_FACTORY
        std::cerr << "LADSPAPluginFactory::instantiatePlugin("
                  << identifier.toStdString() << ": now have " << m_instances.size() << " instances" << std::endl;
#endif

	return instance;
    }

    return 0;
}

void
LADSPAPluginFactory::releasePlugin(RealTimePluginInstance *instance,
				   QString identifier)
{
    Profiler profiler("LADSPAPluginFactory::releasePlugin");

    if (m_instances.find(instance) == m_instances.end()) {
	std::cerr << "WARNING: LADSPAPluginFactory::releasePlugin: Not one of mine!"
		  << std::endl;
	return;
    }

    QString type, soname, label;
    PluginIdentifier::parseIdentifier(identifier, type, soname, label);

    m_instances.erase(instance);

    bool stillInUse = false;

    for (std::set<RealTimePluginInstance *>::iterator ii = m_instances.begin();
	 ii != m_instances.end(); ++ii) {
	QString itype, isoname, ilabel;
	PluginIdentifier::parseIdentifier((*ii)->getPluginIdentifier(), itype, isoname, ilabel);
	if (isoname == soname) {
#ifdef DEBUG_LADSPA_PLUGIN_FACTORY
	    std::cerr << "LADSPAPluginFactory::releasePlugin: dll " << soname.toStdString() << " is still in use for plugin " << ilabel.toStdString() << std::endl;
#endif
	    stillInUse = true;
	    break;
	}
    }
    
    if (!stillInUse) {
        if (soname != PluginIdentifier::BUILTIN_PLUGIN_SONAME) {
#ifdef DEBUG_LADSPA_PLUGIN_FACTORY
            std::cerr << "LADSPAPluginFactory::releasePlugin: dll " << soname.toStdString() << " no longer in use, unloading" << std::endl;
#endif
            unloadLibrary(soname);
        }
    }

#ifdef DEBUG_LADSPA_PLUGIN_FACTORY
    std::cerr << "LADSPAPluginFactory::releasePlugin("
                  << identifier.toStdString() << ": now have " << m_instances.size() << " instances" << std::endl;
#endif
}

const LADSPA_Descriptor *
LADSPAPluginFactory::getLADSPADescriptor(QString identifier)
{
    QString type, soname, label;
    PluginIdentifier::parseIdentifier(identifier, type, soname, label);

    if (m_libraryHandles.find(soname) == m_libraryHandles.end()) {
	loadLibrary(soname);
	if (m_libraryHandles.find(soname) == m_libraryHandles.end()) {
	    std::cerr << "WARNING: LADSPAPluginFactory::getLADSPADescriptor: loadLibrary failed for " << soname.toStdString() << std::endl;
	    return 0;
	}
    }

    void *libraryHandle = m_libraryHandles[soname];

    LADSPA_Descriptor_Function fn = (LADSPA_Descriptor_Function)
	DLSYM(libraryHandle, "ladspa_descriptor");

    if (!fn) {
	std::cerr << "WARNING: LADSPAPluginFactory::getLADSPADescriptor: No descriptor function in library " << soname.toStdString() << std::endl;
	return 0;
    }

    const LADSPA_Descriptor *descriptor = 0;
    
    int index = 0;
    while ((descriptor = fn(index))) {
	if (descriptor->Label == label) return descriptor;
	++index;
    }

    std::cerr << "WARNING: LADSPAPluginFactory::getLADSPADescriptor: No such plugin as " << label.toStdString() << " in library " << soname.toStdString() << std::endl;

    return 0;
}

void
LADSPAPluginFactory::loadLibrary(QString soName)
{
    void *libraryHandle = DLOPEN(soName, RTLD_NOW);
    if (libraryHandle) {
        m_libraryHandles[soName] = libraryHandle;
        std::cerr << "LADSPAPluginFactory::loadLibrary: Loaded library \"" << soName.toStdString() << "\"" << std::endl;
        return;
    }

    if (QFileInfo(soName).exists()) {
        DLERROR();
        std::cerr << "LADSPAPluginFactory::loadLibrary: Library \"" << soName.toStdString() << "\" exists, but failed to load it" << std::endl;
        return;
    }

    std::vector<QString> pathList = getPluginPath();

    QString fileName = QFile(soName).fileName();
    QString base = QFileInfo(soName).baseName();

    for (std::vector<QString>::iterator i = pathList.begin();
	 i != pathList.end(); ++i) {
        
#ifdef DEBUG_LADSPA_PLUGIN_FACTORY
        std::cerr << "Looking at: " << (*i).toStdString() << std::endl;
#endif

        QDir dir(*i, PLUGIN_GLOB,
                 QDir::Name | QDir::IgnoreCase,
                 QDir::Files | QDir::Readable);

        if (QFileInfo(dir.filePath(fileName)).exists()) {
#ifdef DEBUG_LADSPA_PLUGIN_FACTORY
            std::cerr << "Loading: " << fileName.toStdString() << std::endl;
#endif
            libraryHandle = DLOPEN(dir.filePath(fileName), RTLD_NOW);
            if (libraryHandle) {
                m_libraryHandles[soName] = libraryHandle;
                return;
            }
        }

	for (unsigned int j = 0; j < dir.count(); ++j) {
            QString file = dir.filePath(dir[j]);
            if (QFileInfo(file).baseName() == base) {
#ifdef DEBUG_LADSPA_PLUGIN_FACTORY
                std::cerr << "Loading: " << file.toStdString() << std::endl;
#endif
                libraryHandle = DLOPEN(file, RTLD_NOW);
                if (libraryHandle) {
                    m_libraryHandles[soName] = libraryHandle;
                    return;
                }
            }
        }
    }

    std::cerr << "LADSPAPluginFactory::loadLibrary: Failed to locate plugin library \"" << soName.toStdString() << "\"" << std::endl;
}

void
LADSPAPluginFactory::unloadLibrary(QString soName)
{
    LibraryHandleMap::iterator li = m_libraryHandles.find(soName);
    if (li != m_libraryHandles.end()) {
//	std::cerr << "unloading " << soname.toStdString() << std::endl;
	DLCLOSE(m_libraryHandles[soName]);
	m_libraryHandles.erase(li);
    }
}

void
LADSPAPluginFactory::unloadUnusedLibraries()
{
    std::vector<QString> toUnload;

    for (LibraryHandleMap::iterator i = m_libraryHandles.begin();
	 i != m_libraryHandles.end(); ++i) {

	bool stillInUse = false;

	for (std::set<RealTimePluginInstance *>::iterator ii = m_instances.begin();
	     ii != m_instances.end(); ++ii) {

	    QString itype, isoname, ilabel;
	    PluginIdentifier::parseIdentifier((*ii)->getPluginIdentifier(), itype, isoname, ilabel);
	    if (isoname == i->first) {
		stillInUse = true;
		break;
	    }
	}

	if (!stillInUse) toUnload.push_back(i->first);
    }

    for (std::vector<QString>::iterator i = toUnload.begin();
	 i != toUnload.end(); ++i) {
        if (*i != PluginIdentifier::BUILTIN_PLUGIN_SONAME) {
            unloadLibrary(*i);
        }
    }
}


// It is only later, after they've gone,
// I realize they have delivered a letter.
// It's a letter from my wife.  "What are you doing
// there?" my wife asks.  "Are you drinking?"
// I study the postmark for hours.  Then it, too, begins to fade.
// I hope someday to forget all this.


std::vector<QString>
LADSPAPluginFactory::getPluginPath()
{
    std::vector<QString> pathList;
    std::string path;

    char *cpath = getenv("LADSPA_PATH");
    if (cpath) path = cpath;

    if (path == "") {

        path = DEFAULT_LADSPA_PATH;

	char *home = getenv("HOME");
	if (home) {
            std::string::size_type f;
            while ((f = path.find("$HOME")) != std::string::npos &&
                   f < path.length()) {
                path.replace(f, 5, home);
            }
        }

#ifdef _WIN32
        char *pfiles = getenv("ProgramFiles");
        if (!pfiles) pfiles = "C:\\Program Files";
        {
        std::string::size_type f;
        while ((f = path.find("%ProgramFiles%")) != std::string::npos &&
               f < path.length()) {
            path.replace(f, 14, pfiles);
        }
        }
#endif
    }

    std::string::size_type index = 0, newindex = 0;

    while ((newindex = path.find(PATH_SEPARATOR, index)) < path.size()) {
	pathList.push_back(path.substr(index, newindex - index).c_str());
	index = newindex + 1;
    }
    
    pathList.push_back(path.substr(index).c_str());

    return pathList;
}


std::vector<QString>
LADSPAPluginFactory::getLRDFPath(QString &baseUri)
{
    std::vector<QString> lrdfPaths;

#ifdef HAVE_LRDF
    std::vector<QString> pathList = getPluginPath();

    lrdfPaths.push_back("/usr/local/share/ladspa/rdf");
    lrdfPaths.push_back("/usr/share/ladspa/rdf");

    for (std::vector<QString>::iterator i = pathList.begin();
	 i != pathList.end(); ++i) {
	lrdfPaths.push_back(*i + "/rdf");
    }

    baseUri = LADSPA_BASE;
#endif

    return lrdfPaths;
}    

void
LADSPAPluginFactory::discoverPlugins()
{
    Profiler profiler("LADSPAPluginFactory::discoverPlugins");

    std::vector<QString> pathList = getPluginPath();

//    std::cerr << "LADSPAPluginFactory::discoverPlugins - "
//	      << "discovering plugins; path is ";
//    for (std::vector<QString>::iterator i = pathList.begin();
//	 i != pathList.end(); ++i) {
//	std::cerr << "[" << i->toStdString() << "] ";
//    }
//    std::cerr << std::endl;

#ifdef HAVE_LRDF
    // read the description files 
    //
    QString baseUri;
    std::vector<QString> lrdfPaths = getLRDFPath(baseUri);

    bool haveSomething = false;

    for (size_t i = 0; i < lrdfPaths.size(); ++i) {
	QDir dir(lrdfPaths[i], "*.rdf;*.rdfs");
	for (unsigned int j = 0; j < dir.count(); ++j) {
	    if (!lrdf_read_file(QString("file:" + lrdfPaths[i] + "/" + dir[j]).toStdString().c_str())) {
//		std::cerr << "LADSPAPluginFactory: read RDF file " << (lrdfPaths[i] + "/" + dir[j]) << std::endl;
		haveSomething = true;
	    }
	}
    }

    if (haveSomething) {
	generateTaxonomy(baseUri + "Plugin", "");
    }
#endif // HAVE_LRDF

    generateFallbackCategories();

    for (std::vector<QString>::iterator i = pathList.begin();
	 i != pathList.end(); ++i) {

	QDir pluginDir(*i, PLUGIN_GLOB);

	for (unsigned int j = 0; j < pluginDir.count(); ++j) {
	    discoverPlugins(QString("%1/%2").arg(*i).arg(pluginDir[j]));
	}
    }
}

void
LADSPAPluginFactory::discoverPlugins(QString soname)
{
    void *libraryHandle = DLOPEN(soname, RTLD_LAZY);

    if (!libraryHandle) {
        std::cerr << "WARNING: LADSPAPluginFactory::discoverPlugins: couldn't load plugin library "
                  << soname.toStdString() << " - " << DLERROR() << std::endl;
        return;
    }

    LADSPA_Descriptor_Function fn = (LADSPA_Descriptor_Function)
	DLSYM(libraryHandle, "ladspa_descriptor");

    if (!fn) {
	std::cerr << "WARNING: LADSPAPluginFactory::discoverPlugins: No descriptor function in " << soname.toStdString() << std::endl;
	return;
    }

    const LADSPA_Descriptor *descriptor = 0;
    
    int index = 0;
    while ((descriptor = fn(index))) {

        RealTimePluginDescriptor *rtd = new RealTimePluginDescriptor;
        rtd->name = descriptor->Name;
        rtd->label = descriptor->Label;
        rtd->maker = descriptor->Maker;
        rtd->copyright = descriptor->Copyright;
        rtd->category = "";
        rtd->isSynth = false;
        rtd->parameterCount = 0;
        rtd->audioInputPortCount = 0;
        rtd->audioOutputPortCount = 0;
        rtd->controlOutputPortCount = 0;

	QString identifier = PluginIdentifier::createIdentifier
	    ("ladspa", soname, descriptor->Label);

#ifdef HAVE_LRDF
	char *def_uri = 0;
	lrdf_defaults *defs = 0;
		
        if (m_lrdfTaxonomy[descriptor->UniqueID] != "") {
            m_taxonomy[identifier] = m_lrdfTaxonomy[descriptor->UniqueID];
//            std::cerr << "set id \"" << identifier.toStdString() << "\" to cat \"" << m_taxonomy[identifier].toStdString() << "\" from LRDF" << std::endl;
//            std::cout << identifier.toStdString() << "::" << m_taxonomy[identifier].toStdString() << std::endl;
        }

	QString category = m_taxonomy[identifier];
	
	if (category == "" && descriptor->Name != 0) {
	    std::string name = descriptor->Name;
	    if (name.length() > 4 &&
		name.substr(name.length() - 4) == " VST") {
		category = "VST effects";
		m_taxonomy[identifier] = category;
	    }
	}
	
        rtd->category = category.toStdString();

//	std::cerr << "Plugin id is " << descriptor->UniqueID
//		  << ", category is \"" << (category ? category : QString("(none)"))
//		  << "\", name is " << descriptor->Name
//		  << ", label is " << descriptor->Label
//		  << std::endl;
	
	def_uri = lrdf_get_default_uri(descriptor->UniqueID);
	if (def_uri) {
	    defs = lrdf_get_setting_values(def_uri);
	}

	unsigned int controlPortNumber = 1;
	
	for (unsigned long i = 0; i < descriptor->PortCount; i++) {
	    
	    if (LADSPA_IS_PORT_CONTROL(descriptor->PortDescriptors[i])) {
		
		if (def_uri && defs) {
		    
		    for (unsigned int j = 0; j < defs->count; j++) {
			if (defs->items[j].pid == controlPortNumber) {
//			    std::cerr << "Default for this port (" << defs->items[j].pid << ", " << defs->items[j].label << ") is " << defs->items[j].value << "; applying this to port number " << i << " with name " << descriptor->PortNames[i] << std::endl;
			    m_portDefaults[descriptor->UniqueID][i] =
				defs->items[j].value;
			}
		    }
		}
		
		++controlPortNumber;
	    }
	}
#endif // HAVE_LRDF

	for (unsigned long i = 0; i < descriptor->PortCount; i++) {
	    if (LADSPA_IS_PORT_CONTROL(descriptor->PortDescriptors[i])) {
                if (LADSPA_IS_PORT_INPUT(descriptor->PortDescriptors[i])) {
                    ++rtd->parameterCount;
                } else {
                    if (strcmp(descriptor->PortNames[i], "latency") &&
                        strcmp(descriptor->PortNames[i], "_latency")) {
                        ++rtd->controlOutputPortCount;
                        rtd->controlOutputPortNames.push_back
                            (descriptor->PortNames[i]);
                    }
                }
            } else {
                if (LADSPA_IS_PORT_INPUT(descriptor->PortDescriptors[i])) {
                    ++rtd->audioInputPortCount;
                } else if (LADSPA_IS_PORT_OUTPUT(descriptor->PortDescriptors[i])) {
                    ++rtd->audioOutputPortCount;
                }
            }
        }

	m_identifiers.push_back(identifier);

        m_rtDescriptors[identifier] = rtd;

	++index;
    }

    if (DLCLOSE(libraryHandle) != 0) {
        std::cerr << "WARNING: LADSPAPluginFactory::discoverPlugins - can't unload " << libraryHandle << std::endl;
        return;
    }
}

void
LADSPAPluginFactory::generateFallbackCategories()
{
    std::vector<QString> pluginPath = getPluginPath();
    std::vector<QString> path;

    for (size_t i = 0; i < pluginPath.size(); ++i) {
	if (pluginPath[i].contains("/lib/")) {
	    QString p(pluginPath[i]);
            path.push_back(p);
	    p.replace("/lib/", "/share/");
	    path.push_back(p);
//	    std::cerr << "LADSPAPluginFactory::generateFallbackCategories: path element " << p.toStdString() << std::endl;
	}
	path.push_back(pluginPath[i]);
//	std::cerr << "LADSPAPluginFactory::generateFallbackCategories: path element " << pluginPath[i].toStdString() << std::endl;
    }

    for (size_t i = 0; i < path.size(); ++i) {

	QDir dir(path[i], "*.cat");

//	std::cerr << "LADSPAPluginFactory::generateFallbackCategories: directory " << path[i].toStdString() << " has " << dir.count() << " .cat files" << std::endl;
	for (unsigned int j = 0; j < dir.count(); ++j) {

	    QFile file(path[i] + "/" + dir[j]);

//	    std::cerr << "LADSPAPluginFactory::generateFallbackCategories: about to open " << (path[i].toStdString() + "/" + dir[j].toStdString()) << std::endl;

	    if (file.open(QIODevice::ReadOnly)) {
//		    std::cerr << "...opened" << std::endl;
		QTextStream stream(&file);
		QString line;

		while (!stream.atEnd()) {
		    line = stream.readLine();
//		    std::cerr << "line is: \"" << line.toStdString() << "\"" << std::endl;
		    QString id = PluginIdentifier::canonicalise
                        (line.section("::", 0, 0));
		    QString cat = line.section("::", 1, 1);
		    m_taxonomy[id] = cat;
//		    std::cerr << "set id \"" << id.toStdString() << "\" to cat \"" << cat.toStdString() << "\"" << std::endl;
		}
	    }
	}
    }
}    

void
LADSPAPluginFactory::generateTaxonomy(QString uri, QString base)
{
#ifdef HAVE_LRDF
    lrdf_uris *uris = lrdf_get_instances(uri.toStdString().c_str());

    if (uris != NULL) {
	for (unsigned int i = 0; i < uris->count; ++i) {
	    m_lrdfTaxonomy[lrdf_get_uid(uris->items[i])] = base;
	}
	lrdf_free_uris(uris);
    }

    uris = lrdf_get_subclasses(uri.toStdString().c_str());

    if (uris != NULL) {
	for (unsigned int i = 0; i < uris->count; ++i) {
	    char *label = lrdf_get_label(uris->items[i]);
	    generateTaxonomy(uris->items[i],
			     base + (base.length() > 0 ? " > " : "") + label);
	}
	lrdf_free_uris(uris);
    }
#endif
}
    
QString
LADSPAPluginFactory::getPluginCategory(QString identifier)
{
    return m_taxonomy[identifier];
}

