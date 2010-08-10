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

#include "RDFImporter.h"

#include <map>
#include <vector>

#include <iostream>
#include <cmath>

#include "SimpleSPARQLQuery.h"

#include "base/ProgressReporter.h"
#include "base/RealTime.h"

#include "data/model/SparseOneDimensionalModel.h"
#include "data/model/SparseTimeValueModel.h"
#include "data/model/EditableDenseThreeDimensionalModel.h"
#include "data/model/NoteModel.h"
#include "data/model/TextModel.h"
#include "data/model/RegionModel.h"
#include "data/model/WaveFileModel.h"

#include "data/fileio/FileSource.h"
#include "data/fileio/CachedFile.h"
#include "data/fileio/FileFinder.h"

using std::cerr;
using std::endl;

class RDFImporterImpl
{
public:
    RDFImporterImpl(QString url, int sampleRate);
    virtual ~RDFImporterImpl();

    void setSampleRate(int sampleRate) { m_sampleRate = sampleRate; }
    
    bool isOK();
    QString getErrorString() const;

    std::vector<Model *> getDataModels(ProgressReporter *);

protected:
    QString m_uristring;
    QString m_errorString;
    std::map<QString, Model *> m_audioModelMap;
    int m_sampleRate;

    std::map<Model *, std::map<QString, float> > m_labelValueMap;

    static bool m_prefixesLoaded;
    static void loadPrefixes(ProgressReporter *reporter);

    void getDataModelsAudio(std::vector<Model *> &, ProgressReporter *);
    void getDataModelsSparse(std::vector<Model *> &, ProgressReporter *);
    void getDataModelsDense(std::vector<Model *> &, ProgressReporter *);

    void getDenseModelTitle(Model *, QString, QString);

    void getDenseFeatureProperties(QString featureUri,
                                   int &sampleRate, int &windowLength,
                                   int &hopSize, int &width, int &height);

    void fillModel(Model *, long, long, bool, std::vector<float> &, QString);
};

bool RDFImporterImpl::m_prefixesLoaded = false;

QString
RDFImporter::getKnownExtensions()
{
    return "*.rdf *.n3 *.ttl";
}

RDFImporter::RDFImporter(QString url, int sampleRate) :
    m_d(new RDFImporterImpl(url, sampleRate)) 
{
}

RDFImporter::~RDFImporter()
{
    delete m_d;
}

void
RDFImporter::setSampleRate(int sampleRate)
{
    m_d->setSampleRate(sampleRate);
}

bool
RDFImporter::isOK()
{
    return m_d->isOK();
}

QString
RDFImporter::getErrorString() const
{
    return m_d->getErrorString();
}

std::vector<Model *>
RDFImporter::getDataModels(ProgressReporter *r)
{
    return m_d->getDataModels(r);
}

RDFImporterImpl::RDFImporterImpl(QString uri, int sampleRate) :
    m_uristring(uri),
    m_sampleRate(sampleRate)
{
}

RDFImporterImpl::~RDFImporterImpl()
{
    SimpleSPARQLQuery::closeSingleSource(m_uristring);
}

bool
RDFImporterImpl::isOK()
{
    return (m_errorString == "");
}

QString
RDFImporterImpl::getErrorString() const
{
    return m_errorString;
}

std::vector<Model *>
RDFImporterImpl::getDataModels(ProgressReporter *reporter)
{
    loadPrefixes(reporter);

    std::vector<Model *> models;

    getDataModelsAudio(models, reporter);

    if (m_sampleRate == 0) {
        m_errorString = QString("Invalid audio data model (is audio file format supported?)");
        std::cerr << m_errorString.toStdString() << std::endl;
        return models;
    }

    QString error;

    if (m_errorString != "") {
        error = m_errorString;
    }
    m_errorString = "";

    getDataModelsDense(models, reporter);

    if (m_errorString != "") {
        error = m_errorString;
    }
    m_errorString = "";

    getDataModelsSparse(models, reporter);

    if (m_errorString == "" && error != "") {
        m_errorString = error;
    }

    return models;
}

void
RDFImporterImpl::getDataModelsAudio(std::vector<Model *> &models,
                                    ProgressReporter *reporter)
{
    SimpleSPARQLQuery query
        (SimpleSPARQLQuery::QueryFromSingleSource,
         QString
         (
             " PREFIX mo: <http://purl.org/ontology/mo/> "
             " SELECT ?signal ?source FROM <%1> "
             " WHERE { ?source a mo:AudioFile . "
             "         ?signal a mo:Signal . "
             "         ?source mo:encodes ?signal } "
             )
         .arg(m_uristring));

    SimpleSPARQLQuery::ResultList results = query.execute();

    if (results.empty()) {

        SimpleSPARQLQuery query2
            (SimpleSPARQLQuery::QueryFromSingleSource,
             QString
             (
                 " PREFIX mo: <http://purl.org/ontology/mo/> "
                 " SELECT ?signal ?source FROM <%1> "
                 " WHERE { ?signal a mo:Signal ; mo:available_as ?source } "
                 )
             .arg(m_uristring));
        
        results = query.execute();
    }

    for (int i = 0; i < results.size(); ++i) {

        QString signal = results[i]["signal"].value;
        QString source = results[i]["source"].value;

        std::cerr << "NOTE: Seeking signal source \"" << source.toStdString()
                  << "\"..." << std::endl;

        FileSource *fs = new FileSource(source, reporter);
        if (fs->isAvailable()) {
            std::cerr << "NOTE: Source is available: Local filename is \""
                      << fs->getLocalFilename().toStdString()
                      << "\"..." << std::endl;
        }
            
#ifdef NO_SV_GUI
        if (!fs->isAvailable()) {
            m_errorString = QString("Signal source \"%1\" is not available").arg(source);
            delete fs;
            continue;
        }
#else
        if (!fs->isAvailable()) {
            std::cerr << "NOTE: Signal source \"" << source.toStdString()
                      << "\" is not available, using file finder..." << std::endl;
            FileFinder *ff = FileFinder::getInstance();
            if (ff) {
                QString path = ff->find(FileFinder::AudioFile,
                                        fs->getLocation(),
                                        m_uristring);
                if (path != "") {
                    std::cerr << "File finder returns: \"" << path.toStdString()
                              << "\"" << std::endl;
                    delete fs;
                    fs = new FileSource(path, reporter);
                    if (!fs->isAvailable()) {
                        delete fs;
                        m_errorString = QString("Signal source \"%1\" is not available").arg(source);
                        continue;
                    }
                }
            }
        }
#endif

        if (reporter) {
            reporter->setMessage(RDFImporter::tr("Importing audio referenced in RDF..."));
        }
        fs->waitForData();
        WaveFileModel *newModel = new WaveFileModel(*fs, m_sampleRate);
        if (newModel->isOK()) {
            std::cerr << "Successfully created wave file model from source at \"" << source.toStdString() << "\"" << std::endl;
            models.push_back(newModel);
            m_audioModelMap[signal] = newModel;
            if (m_sampleRate == 0) {
                m_sampleRate = newModel->getSampleRate();
            }
        } else {
            m_errorString = QString("Failed to create wave file model from source at \"%1\"").arg(source);
            delete newModel;
        }
        delete fs;
    }
}

void
RDFImporterImpl::getDataModelsDense(std::vector<Model *> &models,
                                    ProgressReporter *reporter)
{
    if (reporter) {
        reporter->setMessage(RDFImporter::tr("Importing dense signal data from RDF..."));
    }

    SimpleSPARQLQuery query
        (SimpleSPARQLQuery::QueryFromSingleSource,
         QString
         (
             " PREFIX mo: <http://purl.org/ontology/mo/>"
             " PREFIX af: <http://purl.org/ontology/af/>"
             
             " SELECT ?feature ?feature_signal_type ?value "
             " FROM <%1> "
             
             " WHERE { "
             
             "   ?signal af:signal_feature ?feature . "
             
             "   ?feature a ?feature_signal_type ; "
             "            af:value ?value . "
    
             " } "
             )
         .arg(m_uristring));

    SimpleSPARQLQuery::ResultList results = query.execute();

    if (!query.isOK()) {
        m_errorString = query.getErrorString();
        return;
    }

    if (query.wasCancelled()) {
        m_errorString = "Query cancelled";
        return;
    }        

    for (int i = 0; i < results.size(); ++i) {

        QString feature = results[i]["feature"].value;
        QString type = results[i]["feature_signal_type"].value;
        QString value = results[i]["value"].value;

        int sampleRate = 0;
        int windowLength = 0;
        int hopSize = 0;
        int width = 0;
        int height = 0;
        getDenseFeatureProperties
            (feature, sampleRate, windowLength, hopSize, width, height);

        if (sampleRate != 0 && sampleRate != m_sampleRate) {
            cerr << "WARNING: Sample rate in dense feature description does not match our underlying rate -- using rate from feature description" << endl;
        }
        if (sampleRate == 0) sampleRate = m_sampleRate;

        if (hopSize == 0) {
            cerr << "WARNING: Dense feature description does not specify a hop size -- assuming 1" << endl;
            hopSize = 1;
        }

        if (height == 0) {
            cerr << "WARNING: Dense feature description does not specify feature signal dimensions -- assuming one-dimensional (height = 1)" << endl;
            height = 1;
        }

        QStringList values = value.split(' ', QString::SkipEmptyParts);

        if (values.empty()) {
            cerr << "WARNING: Dense feature description does not specify any values!" << endl;
            continue;
        }

        if (height == 1) {

            SparseTimeValueModel *m = new SparseTimeValueModel
                (sampleRate, hopSize, false);

            for (int j = 0; j < values.size(); ++j) {
                float f = values[j].toFloat();
                SparseTimeValueModel::Point point(j * hopSize, f, "");
                m->addPoint(point);
            }

            getDenseModelTitle(m, feature, type);
        
            m->setRDFTypeURI(type);

            models.push_back(m);

        } else {

            EditableDenseThreeDimensionalModel *m =
                new EditableDenseThreeDimensionalModel
                (sampleRate, hopSize, height, 
                 EditableDenseThreeDimensionalModel::NoCompression, false);
            
            EditableDenseThreeDimensionalModel::Column column;

            int x = 0;

            for (int j = 0; j < values.size(); ++j) {
                if (j % height == 0 && !column.empty()) {
                    m->setColumn(x++, column);
                    column.clear();
                }
                column.push_back(values[j].toFloat());
            }

            if (!column.empty()) {
                m->setColumn(x++, column);
            }

            getDenseModelTitle(m, feature, type);
        
            m->setRDFTypeURI(type);

            models.push_back(m);
        }
    }
}

void
RDFImporterImpl::getDenseModelTitle(Model *m,
                                    QString featureUri,
                                    QString featureTypeUri)
{
    QString titleQuery = QString
        (
            " PREFIX dc: <http://purl.org/dc/elements/1.1/> "
            " SELECT ?title "
            " FROM <%1> " 
            " WHERE { "
            "   <%2> dc:title ?title . "
            " } "
            ).arg(m_uristring);
    
    SimpleSPARQLQuery::Value v;

    v = SimpleSPARQLQuery::singleResultQuery
        (SimpleSPARQLQuery::QueryFromSingleSource,
         titleQuery.arg(featureUri),
         "title");

    if (v.value != "") {
        std::cerr << "RDFImporterImpl::getDenseModelTitle: Title (from signal) \"" << v.value.toStdString() << "\"" << std::endl;
        m->setObjectName(v.value);
        return;
    }

    v = SimpleSPARQLQuery::singleResultQuery
        (SimpleSPARQLQuery::QueryFromSingleSource,
         titleQuery.arg(featureTypeUri),
         "title");
    
    if (v.value != "") {
        std::cerr << "RDFImporterImpl::getDenseModelTitle: Title (from signal type) \"" << v.value.toStdString() << "\"" << std::endl;
        m->setObjectName(v.value);
        return;
    }

    std::cerr << "RDFImporterImpl::getDenseModelTitle: No title available for feature <" << featureUri.toStdString() << ">" << std::endl;
}

void
RDFImporterImpl::getDenseFeatureProperties(QString featureUri,
                                           int &sampleRate, int &windowLength,
                                           int &hopSize, int &width, int &height)
{
    SimpleSPARQLQuery::QueryType s = SimpleSPARQLQuery::QueryFromSingleSource;

    QString dimensionsQuery 
        (
            " PREFIX mo: <http://purl.org/ontology/mo/>"
            " PREFIX af: <http://purl.org/ontology/af/>"
            
            " SELECT ?dimensions "
            " FROM <%1> "

            " WHERE { "

            "   <%2> af:dimensions ?dimensions . "
            
            " } "
            );

    SimpleSPARQLQuery::Value dimensionsValue =
        SimpleSPARQLQuery::singleResultQuery
        (s, dimensionsQuery.arg(m_uristring).arg(featureUri), "dimensions");

    cerr << "Dimensions = \"" << dimensionsValue.value.toStdString() << "\""
         << endl;

    if (dimensionsValue.value != "") {
        QStringList dl = dimensionsValue.value.split(" ");
        if (dl.empty()) dl.push_back(dimensionsValue.value);
        if (dl.size() > 0) height = dl[0].toInt();
        if (dl.size() > 1) width = dl[1].toInt();
    }

    QString queryTemplate
        (
            " PREFIX mo: <http://purl.org/ontology/mo/>"
            " PREFIX af: <http://purl.org/ontology/af/>"
            " PREFIX tl: <http://purl.org/NET/c4dm/timeline.owl#>"

            " SELECT ?%3 "
            " FROM <%1> "
            
            " WHERE { "
            
            "   <%2> mo:time ?time . "
            
            "   ?time a tl:Interval ; "
            "         tl:onTimeLine ?timeline . "

            "   ?map tl:rangeTimeLine ?timeline . "

            "   ?map tl:%3 ?%3 . "
            
            " } "
            );

    // Another laborious workaround for rasqal's failure to handle
    // multiple optionals properly

    SimpleSPARQLQuery::Value srValue = 
        SimpleSPARQLQuery::singleResultQuery(s,
                                             queryTemplate
                                             .arg(m_uristring).arg(featureUri)
                                             .arg("sampleRate"),
                                             "sampleRate");
    if (srValue.value != "") {
        sampleRate = srValue.value.toInt();
    }

    SimpleSPARQLQuery::Value hopValue = 
        SimpleSPARQLQuery::singleResultQuery(s,
                                             queryTemplate
                                             .arg(m_uristring).arg(featureUri)
                                             .arg("hopSize"),
                                             "hopSize");
    if (srValue.value != "") {
        hopSize = hopValue.value.toInt();
    }

    SimpleSPARQLQuery::Value winValue = 
        SimpleSPARQLQuery::singleResultQuery(s,
                                             queryTemplate
                                             .arg(m_uristring).arg(featureUri)
                                             .arg("windowLength"),
                                             "windowLength");
    if (winValue.value != "") {
        windowLength = winValue.value.toInt();
    }

    cerr << "sr = " << sampleRate << ", hop = " << hopSize << ", win = " << windowLength << endl;
}

void
RDFImporterImpl::getDataModelsSparse(std::vector<Model *> &models,
                                     ProgressReporter *reporter)
{
    if (reporter) {
        reporter->setMessage(RDFImporter::tr("Importing event data from RDF..."));
    }

    SimpleSPARQLQuery::QueryType s = SimpleSPARQLQuery::QueryFromSingleSource;

    // Our query is intended to retrieve every thing that has a time,
    // and every feature type and value associated with a thing that
    // has a time.

    // We will then need to refine this big bag of results into a set
    // of data models.

    // Results that have different source signals should go into
    // different models.

    // Results that have different feature types should go into
    // different models.

    // Results that are sparse should go into different models from
    // those that are dense (we need to examine the timestamps to
    // establish this -- if the timestamps are regular, the results
    // are dense -- so we can't do it as we go along, only after
    // collecting all results).

    // Timed things that have features associated with them should not
    // appear directly in any model -- their features should appear
    // instead -- and these should be different models from those used
    // for timed things that do not have features.

    // As we load the results, we'll push them into a partially
    // structured container that maps from source signal (URI as
    // string) -> feature type (likewise) -> time -> list of values.
    // If the source signal or feature type is unavailable, the empty
    // string will do.

    QString prefixes = QString(
        " PREFIX event: <http://purl.org/NET/c4dm/event.owl#>"
        " PREFIX tl: <http://purl.org/NET/c4dm/timeline.owl#>"
        " PREFIX mo: <http://purl.org/ontology/mo/>"
        " PREFIX af: <http://purl.org/ontology/af/>"
        " PREFIX rdfs: <http://www.w3.org/2000/01/rdf-schema#>"
        );

    QString queryString = prefixes + QString(

        " SELECT ?signal ?timed_thing ?timeline ?event_type ?value"
        " FROM <%1>"

        " WHERE {"

        "   ?signal a mo:Signal ."

        "   ?signal mo:time ?interval ."
        "   ?interval tl:onTimeLine ?timeline ."
        "   ?time tl:onTimeLine ?timeline ."
        "   ?timed_thing event:time ?time ."
        "   ?timed_thing a ?event_type ."

        "   OPTIONAL {"
        "     ?timed_thing af:feature ?value"
        "   }"
        " }"

        ).arg(m_uristring);

    //!!! NB we're using rather old terminology for these things, apparently:
    // beginsAt -> start
    // onTimeLine -> timeline

    QString timeQueryString = prefixes + QString(
        
        " SELECT ?time FROM <%1> "
        " WHERE { "        
        "   <%2> event:time ?t . "
        "   ?t tl:at ?time . "
        " } "

        ).arg(m_uristring);

    QString rangeQueryString = prefixes + QString(
        
        " SELECT ?time ?duration FROM <%1> "
        " WHERE { "
        "   <%2> event:time ?t . "
        "   ?t tl:beginsAt ?time . "
        "   ?t tl:duration ?duration . "
        " } "

        ).arg(m_uristring);

    QString labelQueryString = prefixes + QString(
        
        " SELECT ?label FROM <%1> "
        " WHERE { "
        "   <%2> rdfs:label ?label . "
        " } "

        ).arg(m_uristring);

    QString textQueryString = prefixes + QString(
        
        " SELECT ?label FROM <%1> "
        " WHERE { "
        "   <%2> af:text ?label . "
        " } "

        ).arg(m_uristring);

    SimpleSPARQLQuery query(s, queryString);
    query.setProgressReporter(reporter);

//    cerr << "Query will be: " << queryString.toStdString() << endl;

    SimpleSPARQLQuery::ResultList results = query.execute();

    if (!query.isOK()) {
        m_errorString = query.getErrorString();
        return;
    }

    if (query.wasCancelled()) {
        m_errorString = "Query cancelled";
        return;
    }        

    /*
      This function is now only used for sparse data (for dense data
      we would be in getDataModelsDense instead).

      For sparse data, the determining factors in deciding what model
      to use are: Do the features have values? and Do the features
      have duration?

      We can run through the results and check off whether we find
      values and duration for each of the source+type keys, and then
      run through the source+type keys pushing each of the results
      into a suitable model.

      Unfortunately, at this point we do not yet have any actual
      timing data (time/duration) -- just the time URI.

      What we _could_ do is to create one of each type of model at the
      start, for each of the source+type keys, and then push each
      feature into the relevant model depending on what we find out
      about it.  Then return only non-empty models.
    */

    // Map from timeline uri to event type to dimensionality to
    // presence of duration to model ptr.  Whee!
    std::map<QString, std::map<QString, std::map<int, std::map<bool, Model *> > > >
        modelMap;

    for (int i = 0; i < results.size(); ++i) {

        if (i % 4 == 0) {
            if (reporter) reporter->setProgress(i/4);
        }

        QString source = results[i]["signal"].value;
        QString timeline = results[i]["timeline"].value;
        QString type = results[i]["event_type"].value;
        QString thinguri = results[i]["timed_thing"].value;
        
        RealTime time;
        RealTime duration;

        bool haveTime = false;
        bool haveDuration = false;

        QString label = "";
        bool text = (type.contains("Text") || type.contains("text")); // Ha, ha

        if (text) {
            label = SimpleSPARQLQuery::singleResultQuery
                (s, textQueryString.arg(thinguri), "label").value;
        }

        if (label == "") {
            label = SimpleSPARQLQuery::singleResultQuery
                (s, labelQueryString.arg(thinguri), "label").value;
        }

        SimpleSPARQLQuery rangeQuery(s, rangeQueryString.arg(thinguri));
        SimpleSPARQLQuery::ResultList rangeResults = rangeQuery.execute();
        if (!rangeResults.empty()) {
//                std::cerr << rangeResults.size() << " range results" << std::endl;
            time = RealTime::fromXsdDuration
                (rangeResults[0]["time"].value.toStdString());
            duration = RealTime::fromXsdDuration
                (rangeResults[0]["duration"].value.toStdString());
//                std::cerr << "duration string " << rangeResults[0]["duration"].value.toStdString() << std::endl;
            haveTime = true;
            haveDuration = true;
        } else {
            QString timestring = SimpleSPARQLQuery::singleResultQuery
                (s, timeQueryString.arg(thinguri), "time").value;
//            std::cerr << "timestring = " << timestring.toStdString() << std::endl;
            if (timestring != "") {
                time = RealTime::fromXsdDuration(timestring.toStdString());
                haveTime = true;
            }
        }

        QString valuestring = results[i]["value"].value;
        std::vector<float> values;

        if (valuestring != "") {
            QStringList vsl = valuestring.split(" ", QString::SkipEmptyParts);
            for (int j = 0; j < vsl.size(); ++j) {
                bool success = false;
                float v = vsl[j].toFloat(&success);
                if (success) values.push_back(v);
            }
        }

        int dimensions = 1;
        if (values.size() == 1) dimensions = 2;
        else if (values.size() > 1) dimensions = 3;

        Model *model = 0;

        if (modelMap[timeline][type][dimensions].find(haveDuration) ==
            modelMap[timeline][type][dimensions].end()) {

/*
            std::cerr << "Creating new model: source = " << source.toStdString()
                      << ", type = " << type.toStdString() << ", dimensions = "
                      << dimensions << ", haveDuration = " << haveDuration
                      << ", time = " << time << ", duration = " << duration
                      << std::endl;
*/
            
            if (!haveDuration) {

                if (dimensions == 1) {

                    if (text) {
                        
                        model = new TextModel(m_sampleRate, 1, false);

                    } else {

                        model = new SparseOneDimensionalModel(m_sampleRate, 1, false);
                    }

                } else if (dimensions == 2) {

                    if (text) {

                        model = new TextModel(m_sampleRate, 1, false);

                    } else {

                        model = new SparseTimeValueModel(m_sampleRate, 1, false);
                    }

                } else {

                    // We don't have a three-dimensional sparse model,
                    // so use a note model.  We do have some logic (in
                    // extractStructure below) for guessing whether
                    // this should after all have been a dense model,
                    // but it's hard to apply it because we don't have
                    // all the necessary timing data yet... hmm

                    model = new NoteModel(m_sampleRate, 1, false);
                }

            } else { // haveDuration

                if (dimensions == 1 || dimensions == 2) {

                    // If our units are frequency or midi pitch, we
                    // should be using a note model... hm
                    
                    model = new RegionModel(m_sampleRate, 1, false);

                } else {

                    // We don't have a three-dimensional sparse model,
                    // so use a note model.  We do have some logic (in
                    // extractStructure below) for guessing whether
                    // this should after all have been a dense model,
                    // but it's hard to apply it because we don't have
                    // all the necessary timing data yet... hmm

                    model = new NoteModel(m_sampleRate, 1, false);
                }
            }

            model->setRDFTypeURI(type);

            if (m_audioModelMap.find(source) != m_audioModelMap.end()) {
                std::cerr << "source model for " << model << " is " << m_audioModelMap[source] << std::endl;
                model->setSourceModel(m_audioModelMap[source]);
            }

            QString titleQuery = QString
                (
                    " PREFIX dc: <http://purl.org/dc/elements/1.1/> "
                    " SELECT ?title "
                    " FROM <%1> " 
                    " WHERE { "
                    "   <%2> dc:title ?title . "
                    " } "
                    ).arg(m_uristring).arg(type);
            QString title = SimpleSPARQLQuery::singleResultQuery
                (s, titleQuery, "title").value;
            if (title == "") {
                // take it from the end of the event type
                title = type;
                title.replace(QRegExp("^.*[/#]"), "");
            }
            model->setObjectName(title);

            modelMap[timeline][type][dimensions][haveDuration] = model;
            models.push_back(model);
        }

        model = modelMap[timeline][type][dimensions][haveDuration];

        if (model) {
            long ftime = RealTime::realTime2Frame(time, m_sampleRate);
            long fduration = RealTime::realTime2Frame(duration, m_sampleRate);
            fillModel(model, ftime, fduration, haveDuration, values, label);
        }
    }
}

void
RDFImporterImpl::fillModel(Model *model,
                           long ftime,
                           long fduration,
                           bool haveDuration,
                           std::vector<float> &values,
                           QString label)
{
//    std::cerr << "RDFImporterImpl::fillModel: adding point at frame " << ftime << std::endl;

    SparseOneDimensionalModel *sodm =
        dynamic_cast<SparseOneDimensionalModel *>(model);
    if (sodm) {
        SparseOneDimensionalModel::Point point(ftime, label);
        sodm->addPoint(point);
        return;
    }

    TextModel *tm =
        dynamic_cast<TextModel *>(model);
    if (tm) {
        TextModel::Point point
            (ftime,
             values.empty() ? 0.5f : values[0] < 0.f ? 0.f : values[0] > 1.f ? 1.f : values[0], // I was young and feckless once too
             label);
        tm->addPoint(point);
        return;
    }

    SparseTimeValueModel *stvm =
        dynamic_cast<SparseTimeValueModel *>(model);
    if (stvm) {
        SparseTimeValueModel::Point point
            (ftime, values.empty() ? 0.f : values[0], label);
        stvm->addPoint(point);
        return;
    }

    NoteModel *nm =
        dynamic_cast<NoteModel *>(model);
    if (nm) {
        if (haveDuration) {
            float value = 0.f, level = 1.f;
            if (!values.empty()) {
                value = values[0];
                if (values.size() > 1) {
                    level = values[1];
                }
            }
            NoteModel::Point point(ftime, value, fduration, level, label);
            nm->addPoint(point);
        } else {
            float value = 0.f, duration = 1.f, level = 1.f;
            if (!values.empty()) {
                value = values[0];
                if (values.size() > 1) {
                    duration = values[1];
                    if (values.size() > 2) {
                        level = values[2];
                    }
                }
            }
            NoteModel::Point point(ftime, value, duration, level, label);
            nm->addPoint(point);
        }
        return;
    }

    RegionModel *rm = 
        dynamic_cast<RegionModel *>(model);
    if (rm) {
        float value = 0.f;
        if (values.empty()) {
            // no values? map each unique label to a distinct value
            if (m_labelValueMap[model].find(label) == m_labelValueMap[model].end()) {
                m_labelValueMap[model][label] = rm->getValueMaximum() + 1.f;
            }
            value = m_labelValueMap[model][label];
        } else {
            value = values[0];
        }
        if (haveDuration) {
            RegionModel::Point point(ftime, value, fduration, label);
            rm->addPoint(point);
        } else {
            // This won't actually happen -- we only create region models
            // if we do have duration -- but just for completeness
            float duration = 1.f;
            if (!values.empty()) {
                value = values[0];
                if (values.size() > 1) {
                    duration = values[1];
                }
            }
            RegionModel::Point point(ftime, value, duration, label);
            rm->addPoint(point);
        }
        return;
    }
            
    std::cerr << "WARNING: RDFImporterImpl::fillModel: Unknown or unexpected model type" << std::endl;
    return;
}

RDFImporter::RDFDocumentType
RDFImporter::identifyDocumentType(QString url)
{
    bool haveAudio = false;
    bool haveAnnotations = false;

    // This query is not expected to return any values, but if it
    // executes successfully (leaving no error in the error string)
    // then we know we have RDF
    SimpleSPARQLQuery q(SimpleSPARQLQuery::QueryFromSingleSource,
                        QString(" SELECT ?x FROM <%1> WHERE { ?x <y> <z> } ")
                        .arg(url));
    
    SimpleSPARQLQuery::ResultList r = q.execute();
    if (!q.isOK()) {
        SimpleSPARQLQuery::closeSingleSource(url);
        return NotRDF;
    }

    // "MO-conformant" structure for audio files

    SimpleSPARQLQuery::Value value =
        SimpleSPARQLQuery::singleResultQuery
        (SimpleSPARQLQuery::QueryFromSingleSource,
         QString
         (" PREFIX mo: <http://purl.org/ontology/mo/> "
          " SELECT ?url FROM <%1> "
          " WHERE { ?url a mo:AudioFile } "
             ).arg(url),
         "url");

    if (value.type == SimpleSPARQLQuery::URIValue) {

        haveAudio = true;

    } else {

        // Sonic Annotator v0.2 and below used to write this structure
        // (which is not properly in conformance with the Music
        // Ontology)

        value =
            SimpleSPARQLQuery::singleResultQuery
            (SimpleSPARQLQuery::QueryFromSingleSource,
             QString
             (" PREFIX mo: <http://purl.org/ontology/mo/> "
              " SELECT ?url FROM <%1> "
              " WHERE { ?signal a mo:Signal ; mo:available_as ?url } "
                 ).arg(url),
             "url");

        if (value.type == SimpleSPARQLQuery::URIValue) {
            haveAudio = true;
        }
    }

    std::cerr << "NOTE: RDFImporter::identifyDocumentType: haveAudio = "
              << haveAudio << std::endl;

    value =
        SimpleSPARQLQuery::singleResultQuery
        (SimpleSPARQLQuery::QueryFromSingleSource,
         QString
         (" PREFIX event: <http://purl.org/NET/c4dm/event.owl#> "
          " SELECT ?thing FROM <%1> "
          " WHERE { ?thing event:time ?time } "
             ).arg(url),
         "thing");

    if (value.type == SimpleSPARQLQuery::URIValue) {
        haveAnnotations = true;
    }

    if (!haveAnnotations) {
        
        value =
            SimpleSPARQLQuery::singleResultQuery
            (SimpleSPARQLQuery::QueryFromSingleSource,
             QString
             (" PREFIX af: <http://purl.org/ontology/af/> "
              " SELECT ?thing FROM <%1> "
              " WHERE { ?signal af:signal_feature ?thing } "
             ).arg(url),
             "thing");
        
        if (value.type == SimpleSPARQLQuery::URIValue) {
            haveAnnotations = true;
        }
    }

    std::cerr << "NOTE: RDFImporter::identifyDocumentType: haveAnnotations = "
              << haveAnnotations << std::endl;

    SimpleSPARQLQuery::closeSingleSource(url);

    if (haveAudio) {
        if (haveAnnotations) {
            return AudioRefAndAnnotations;
        } else {
            return AudioRef;
        }
    } else {
        if (haveAnnotations) {
            return Annotations;
        } else {
            return OtherRDFDocument;
        }
    }

    return OtherRDFDocument;
}

void
RDFImporterImpl::loadPrefixes(ProgressReporter *reporter)
{
    return;
//!!!
    if (m_prefixesLoaded) return;
    const char *prefixes[] = {
        "http://purl.org/NET/c4dm/event.owl",
        "http://purl.org/NET/c4dm/timeline.owl",
        "http://purl.org/ontology/mo/",
        "http://purl.org/ontology/af/",
        "http://www.w3.org/2000/01/rdf-schema",
        "http://purl.org/dc/elements/1.1/",
    };
    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i) {
        CachedFile cf(prefixes[i], reporter, "application/rdf+xml");
        if (!cf.isOK()) continue;
        SimpleSPARQLQuery::addSourceToModel
            (QUrl::fromLocalFile(cf.getLocalFilename()).toString());
    }
    m_prefixesLoaded = true;
}
