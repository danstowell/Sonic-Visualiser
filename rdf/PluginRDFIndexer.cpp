/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2008 QMUL.
   
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PluginRDFIndexer.h"

#include "SimpleSPARQLQuery.h"

#include "data/fileio/CachedFile.h"
#include "data/fileio/FileSource.h"
#include "data/fileio/PlaylistFileReader.h"
#include "plugin/PluginIdentifier.h"

#include "base/Profiler.h"

#include <vamp-hostsdk/PluginHostAdapter.h>

#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QDateTime>
#include <QSettings>
#include <QFile>

#include <iostream>
using std::cerr;
using std::endl;
using std::vector;
using std::string;
using Vamp::PluginHostAdapter;

PluginRDFIndexer *
PluginRDFIndexer::m_instance = 0;

bool
PluginRDFIndexer::m_prefixesLoaded = false;

PluginRDFIndexer *
PluginRDFIndexer::getInstance() 
{
    if (!m_instance) m_instance = new PluginRDFIndexer();
    return m_instance;
}

PluginRDFIndexer::PluginRDFIndexer()
{
    indexInstalledURLs();
}

PluginRDFIndexer::~PluginRDFIndexer()
{
    QMutexLocker locker(&m_mutex);
}

void
PluginRDFIndexer::indexInstalledURLs()
{
    vector<string> paths = PluginHostAdapter::getPluginPath();

    QStringList filters;
    filters << "*.n3";
    filters << "*.N3";
    filters << "*.rdf";
    filters << "*.RDF";

    // Search each Vamp plugin path for a .rdf file that either has
    // name "soname", "soname:label" or "soname/label" plus RDF
    // extension.  Use that order of preference, and prefer n3 over
    // rdf extension.

    for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
        
        QDir dir(i->c_str());
        if (!dir.exists()) continue;

        QStringList entries = dir.entryList
            (filters, QDir::Files | QDir::Readable);

        for (QStringList::const_iterator j = entries.begin();
             j != entries.end(); ++j) {
            QFileInfo fi(dir.filePath(*j));
            pullFile(fi.absoluteFilePath());
        }

        QStringList subdirs = dir.entryList
            (QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Readable);

        for (QStringList::const_iterator j = subdirs.begin();
             j != subdirs.end(); ++j) {
            QDir subdir(dir.filePath(*j));
            if (subdir.exists()) {
                entries = subdir.entryList
                    (filters, QDir::Files | QDir::Readable);
                for (QStringList::const_iterator k = entries.begin();
                     k != entries.end(); ++k) {
                    QFileInfo fi(subdir.filePath(*k));
                    pullFile(fi.absoluteFilePath());
                }
            }
        }
    }

    reindex();
}

bool
PluginRDFIndexer::indexConfiguredURLs()
{
    std::cerr << "PluginRDFIndexer::indexConfiguredURLs" << std::endl;

    QSettings settings;
    settings.beginGroup("RDF");
    
    QString indexKey("rdf-indices");
    QStringList indices = settings.value(indexKey).toStringList();
    
    for (int i = 0; i < indices.size(); ++i) {

        QString index = indices[i];

        std::cerr << "PluginRDFIndexer::indexConfiguredURLs: index url is "
                  << index.toStdString() << std::endl;

        CachedFile cf(index);
        if (!cf.isOK()) continue;

        FileSource indexSource(cf.getLocalFilename());

        PlaylistFileReader reader(indexSource);
        if (!reader.isOK()) continue;

        PlaylistFileReader::Playlist list = reader.load();
        for (PlaylistFileReader::Playlist::const_iterator j = list.begin();
             j != list.end(); ++j) {
            std::cerr << "PluginRDFIndexer::indexConfiguredURLs: url is "
                      << j->toStdString() << std::endl;
            pullURL(*j);
        }
    }

    QString urlListKey("rdf-urls");
    QStringList urls = settings.value(urlListKey).toStringList();

    for (int i = 0; i < urls.size(); ++i) {
        pullURL(urls[i]);
    }
    
    settings.endGroup();
    reindex();
    return true;
}

QString
PluginRDFIndexer::getURIForPluginId(QString pluginId)
{
    QMutexLocker locker(&m_mutex);

    if (m_idToUriMap.find(pluginId) == m_idToUriMap.end()) return "";
    return m_idToUriMap[pluginId];
}

QString
PluginRDFIndexer::getIdForPluginURI(QString uri)
{
    m_mutex.lock();

    if (m_uriToIdMap.find(uri) == m_uriToIdMap.end()) {

        m_mutex.unlock();

        // Haven't found this uri referenced in any document on the
        // local filesystem; try resolving the pre-fragment part of
        // the uri as a document URL and reading that if possible.

        // Because we may want to refer to this document again, we
        // cache it locally if it turns out to exist.

        cerr << "PluginRDFIndexer::getIdForPluginURI: NOTE: Failed to find a local RDF document describing plugin <" << uri.toStdString() << ">: attempting to retrieve one remotely by guesswork" << endl;

        QString baseUrl = QUrl(uri).toString(QUrl::RemoveFragment);

        indexURL(baseUrl);

        m_mutex.lock();

        if (m_uriToIdMap.find(uri) == m_uriToIdMap.end()) {
            m_uriToIdMap[uri] = "";
        }
    }

    QString id = m_uriToIdMap[uri];
    m_mutex.unlock();
    return id;
}

QStringList
PluginRDFIndexer::getIndexedPluginIds() 
{
    QMutexLocker locker(&m_mutex);

    QStringList ids;
    for (StringMap::const_iterator i = m_idToUriMap.begin();
         i != m_idToUriMap.end(); ++i) {
        ids.push_back(i->first);
    }
    return ids;
}

bool
PluginRDFIndexer::pullFile(QString filepath)
{
    QUrl url = QUrl::fromLocalFile(filepath);
    QString urlString = url.toString();
    return pullURL(urlString);
}

bool
PluginRDFIndexer::indexURL(QString urlString)
{
    bool pulled = pullURL(urlString);
    if (!pulled) return false;
    reindex();
    return true;
}

bool
PluginRDFIndexer::pullURL(QString urlString)
{
    Profiler profiler("PluginRDFIndexer::indexURL");

    loadPrefixes();

//    std::cerr << "PluginRDFIndexer::indexURL(" << urlString.toStdString() << ")" << std::endl;

    QMutexLocker locker(&m_mutex);

    QString localString = urlString;

    if (FileSource::isRemote(urlString) &&
        FileSource::canHandleScheme(urlString)) {

        CachedFile cf(urlString, 0, "application/rdf+xml");
        if (!cf.isOK()) {
            return false;
        }

        localString = QUrl::fromLocalFile(cf.getLocalFilename()).toString();
    }

    return SimpleSPARQLQuery::addSourceToModel(localString);
}

bool
PluginRDFIndexer::reindex()
{
    SimpleSPARQLQuery::QueryType m = SimpleSPARQLQuery::QueryFromModel;

    SimpleSPARQLQuery query
        (m,
         QString
         (
             " PREFIX vamp: <http://purl.org/ontology/vamp/> "

             " SELECT ?plugin ?library ?plugin_id "

             " WHERE { "
             "   ?plugin a vamp:Plugin . "
             "   ?plugin vamp:identifier ?plugin_id . "

             "   OPTIONAL { "
             "     ?library vamp:available_plugin ?plugin "
             "   } "
             " } "
             ));

    SimpleSPARQLQuery::ResultList results = query.execute();

    if (!query.isOK()) {
        cerr << "ERROR: PluginRDFIndexer::reindex: ERROR: Failed to query plugins from model: "
             << query.getErrorString().toStdString() << endl;
        return false;
    }

    if (results.empty()) {
        cerr << "PluginRDFIndexer::reindex: NOTE: no vamp:Plugin resources found in indexed documents" << endl;
        return false;
    }

    bool foundSomething = false;
    bool addedSomething = false;

    for (SimpleSPARQLQuery::ResultList::iterator i = results.begin();
         i != results.end(); ++i) {

        QString pluginUri = (*i)["plugin"].value;
        QString soUri = (*i)["library"].value;
        QString identifier = (*i)["plugin_id"].value;

        if (identifier == "") {
            cerr << "PluginRDFIndexer::reindex: NOTE: No vamp:identifier for plugin <"
                 << pluginUri.toStdString() << ">"
                 << endl;
            continue;
        }
        if (soUri == "") {
            cerr << "PluginRDFIndexer::reindex: NOTE: No implementation library for plugin <"
                 << pluginUri.toStdString() << ">"
                 << endl;
            continue;
        }

        QString sonameQuery =
            QString(
                " PREFIX vamp: <http://purl.org/ontology/vamp/> "
                " SELECT ?library_id "
                " WHERE { "
                "   <%1> vamp:identifier ?library_id "
                " } "
                )
            .arg(soUri);

        SimpleSPARQLQuery::Value sonameValue = 
            SimpleSPARQLQuery::singleResultQuery(m, sonameQuery, "library_id");
        QString soname = sonameValue.value;
        if (soname == "") {
            cerr << "PluginRDFIndexer::reindex: NOTE: No identifier for library <"
                 << soUri.toStdString() << ">"
                 << endl;
            continue;
        }

        QString pluginId = PluginIdentifier::createIdentifier
            ("vamp", soname, identifier);

        foundSomething = true;

        if (m_idToUriMap.find(pluginId) != m_idToUriMap.end()) {
            continue;
        }

        m_idToUriMap[pluginId] = pluginUri;

        addedSomething = true;

        if (pluginUri != "") {
            if (m_uriToIdMap.find(pluginUri) != m_uriToIdMap.end()) {
                cerr << "PluginRDFIndexer::reindex: WARNING: Found multiple plugins with the same URI:" << endl;
                cerr << "  1. Plugin id \"" << m_uriToIdMap[pluginUri].toStdString() << "\"" << endl;
                cerr << "  2. Plugin id \"" << pluginId.toStdString() << "\"" << endl;
                cerr << "both claim URI <" << pluginUri.toStdString() << ">" << endl;
            } else {
                m_uriToIdMap[pluginUri] = pluginId;
            }
        }
    }

    if (!foundSomething) {
        cerr << "PluginRDFIndexer::reindex: NOTE: Plugins found, but none sufficiently described" << endl;
    }
    
    return addedSomething;
}

void
PluginRDFIndexer::loadPrefixes()
{
    return;
//!!!
    if (m_prefixesLoaded) return;
    const char *prefixes[] = {
        "http://purl.org/ontology/vamp/"
    };
    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i) {
        CachedFile cf(prefixes[i], 0, "application/rdf+xml");
        if (!cf.isOK()) continue;
        SimpleSPARQLQuery::addSourceToModel
            (QUrl::fromLocalFile(cf.getLocalFilename()).toString());
    }
    m_prefixesLoaded = true;
}


