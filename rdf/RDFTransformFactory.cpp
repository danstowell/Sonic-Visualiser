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

#include "RDFTransformFactory.h"

#include <map>
#include <vector>

#include <QTextStream>
#include <QUrl>

#include <iostream>
#include <cmath>

#include "SimpleSPARQLQuery.h"
#include "PluginRDFIndexer.h"
#include "PluginRDFDescription.h"
#include "base/ProgressReporter.h"
#include "plugin/PluginIdentifier.h"

#include "transform/TransformFactory.h"

using std::cerr;
using std::endl;

typedef const unsigned char *STR; // redland's expected string type


class RDFTransformFactoryImpl
{
public:
    RDFTransformFactoryImpl(QString url);
    virtual ~RDFTransformFactoryImpl();
    
    bool isRDF();
    bool isOK();
    QString getErrorString() const;

    std::vector<Transform> getTransforms(ProgressReporter *);

    static QString writeTransformToRDF(const Transform &, QString);

protected:
    QString m_urlString;
    QString m_errorString;
    bool m_isRDF;
    bool setOutput(Transform &, QString);
    bool setParameters(Transform &, QString);
};


QString
RDFTransformFactory::getKnownExtensions()
{
    return "*.rdf *.n3 *.ttl";
}

RDFTransformFactory::RDFTransformFactory(QString url) :
    m_d(new RDFTransformFactoryImpl(url)) 
{
}

RDFTransformFactory::~RDFTransformFactory()
{
    delete m_d;
}

bool
RDFTransformFactory::isRDF()
{
    return m_d->isRDF();
}

bool
RDFTransformFactory::isOK()
{
    return m_d->isOK();
}

QString
RDFTransformFactory::getErrorString() const
{
    return m_d->getErrorString();
}

std::vector<Transform>
RDFTransformFactory::getTransforms(ProgressReporter *r)
{
    return m_d->getTransforms(r);
}

QString
RDFTransformFactory::writeTransformToRDF(const Transform &t, QString f)
{
    return RDFTransformFactoryImpl::writeTransformToRDF(t, f);
}

RDFTransformFactoryImpl::RDFTransformFactoryImpl(QString url) :
    m_urlString(url),
    m_isRDF(false)
{
}

RDFTransformFactoryImpl::~RDFTransformFactoryImpl()
{
    SimpleSPARQLQuery::closeSingleSource(m_urlString);
}

bool
RDFTransformFactoryImpl::isRDF()
{
    return m_isRDF;
}

bool
RDFTransformFactoryImpl::isOK()
{
    return (m_errorString == "");
}

QString
RDFTransformFactoryImpl::getErrorString() const
{
    return m_errorString;
}

std::vector<Transform>
RDFTransformFactoryImpl::getTransforms(ProgressReporter *reporter)
{
    std::vector<Transform> transforms;

    std::map<QString, Transform> uriTransformMap;

    QString query = 
        " PREFIX vamp: <http://purl.org/ontology/vamp/> "

        " SELECT ?transform ?plugin "
        
        " FROM <%2> "

        " WHERE { "
        "   ?transform a vamp:Transform ; "
        "              vamp:plugin ?plugin . "
        " } ";

    SimpleSPARQLQuery transformsQuery
        (SimpleSPARQLQuery::QueryFromSingleSource, query.arg(m_urlString));

    SimpleSPARQLQuery::ResultList transformResults = transformsQuery.execute();

    if (!transformsQuery.isOK()) {
        m_errorString = transformsQuery.getErrorString();
        return transforms;
    }

    m_isRDF = true;

    if (transformResults.empty()) {
        cerr << "RDFTransformFactory: NOTE: No RDF/TTL transform descriptions found in document at <" << m_urlString.toStdString() << ">" << endl;
        return transforms;
    }

    // There are various queries we need to make that might include
    // data from either the transform RDF or the model accumulated
    // from plugin descriptions.  For example, the transform RDF may
    // specify the output's true URI, or it might have a blank node or
    // some other URI with the appropriate vamp:identifier included in
    // the file.  To cover both cases, we need to add the file itself
    // into the model and always query the model using the transform
    // URI rather than querying the file itself subsequently.

    SimpleSPARQLQuery::addSourceToModel(m_urlString);

    PluginRDFIndexer *indexer = PluginRDFIndexer::getInstance();

    for (int i = 0; i < transformResults.size(); ++i) {

        SimpleSPARQLQuery::KeyValueMap &result = transformResults[i];

        QString transformUri = result["transform"].value;
        QString pluginUri = result["plugin"].value;

        QString pluginId = indexer->getIdForPluginURI(pluginUri);
        if (pluginId == "") {
            cerr << "RDFTransformFactory: WARNING: Unknown plugin <"
                 << pluginUri.toStdString() << "> for transform <"
                 << transformUri.toStdString() << ">, skipping this transform"
                 << endl;
            continue;
        }

        Transform transform;
        transform.setPluginIdentifier(pluginId);

        if (!setOutput(transform, transformUri)) {
            return transforms;
        }

        if (!setParameters(transform, transformUri)) {
            return transforms;
        }

        uriTransformMap[transformUri] = transform;

        // We have to do this a very long way round, to work around
        // rasqal's current inability to handle correctly more than one
        // OPTIONAL graph in a query

        static const char *optionals[] = {
            "output",
            "program",
            "summary_type",
            "step_size",
            "block_size",
            "window_type",
            "sample_rate",
            "start", 
            "duration"
        };
        
        for (int j = 0; j < sizeof(optionals)/sizeof(optionals[0]); ++j) {

            QString optional = optionals[j];

            QString queryTemplate = 
                " PREFIX vamp: <http://purl.org/ontology/vamp/> "
                
                " SELECT ?%1 "
                
                " WHERE { "
                "   <%2> vamp:%1 ?%1 "
                " } ";
            
            SimpleSPARQLQuery query
                (SimpleSPARQLQuery::QueryFromModel,
                 queryTemplate.arg(optional).arg(transformUri));
        
            SimpleSPARQLQuery::ResultList results = query.execute();

            if (!query.isOK()) {
                m_errorString = query.getErrorString();
                return transforms;
            }

            if (results.empty()) continue;

            for (int k = 0; k < results.size(); ++k) {

                const SimpleSPARQLQuery::Value &v = results[k][optional];

                if (v.type == SimpleSPARQLQuery::LiteralValue) {
                
                    if (optional == "program") {
                        transform.setProgram(v.value);
                    } else if (optional == "summary_type") {
                        transform.setSummaryType
                            (transform.stringToSummaryType(v.value));
                    } else if (optional == "step_size") {
                        transform.setStepSize(v.value.toUInt());
                    } else if (optional == "block_size") {
                        transform.setBlockSize(v.value.toUInt());
                    } else if (optional == "window_type") {
                        cerr << "NOTE: can't handle window type yet (value is \""
                             << v.value.toStdString() << "\")" << endl;
                    } else if (optional == "sample_rate") {
                        transform.setSampleRate(v.value.toFloat());
                    } else if (optional == "start") {
                        transform.setStartTime
                            (RealTime::fromXsdDuration(v.value.toStdString()));
                    } else if (optional == "duration") {
                        transform.setDuration
                            (RealTime::fromXsdDuration(v.value.toStdString()));
                    } else {
                        cerr << "RDFTransformFactory: ERROR: Inconsistent optionals lists (unexpected optional \"" << optional.toStdString() << "\"" << endl;
                    }
                }
            }
        }

        cerr << "RDFTransformFactory: NOTE: Transform is: " << endl;
        cerr << transform.toXmlString().toStdString() << endl;

        transforms.push_back(transform);
    }
        
    return transforms;
}

bool
RDFTransformFactoryImpl::setOutput(Transform &transform,
                                   QString transformUri)
{
    SimpleSPARQLQuery::Value outputValue =
        SimpleSPARQLQuery::singleResultQuery
        (SimpleSPARQLQuery::QueryFromModel,
         QString
         (
             " PREFIX vamp: <http://purl.org/ontology/vamp/> "
             
             " SELECT ?output_id "

             " WHERE { "
             "   <%1> vamp:output ?output . "
             "   ?output vamp:identifier ?output_id "
             " } "
             )
         .arg(transformUri),
         "output_id");
    
    if (outputValue.type == SimpleSPARQLQuery::NoValue) {
        return true;
    }
    
    if (outputValue.type != SimpleSPARQLQuery::LiteralValue) {
        m_errorString = QString("No vamp:identifier found for output of transform <%1>, or vamp:identifier is not a literal").arg(transformUri);
        return false;
    }

    transform.setOutput(outputValue.value);

    return true;
}
        

bool
RDFTransformFactoryImpl::setParameters(Transform &transform,
                                       QString transformUri)
{
    SimpleSPARQLQuery paramQuery
        (SimpleSPARQLQuery::QueryFromModel,
         QString
         (
             " PREFIX vamp: <http://purl.org/ontology/vamp/> "
             
             " SELECT ?param_id ?param_value "
             
             " WHERE { "
             "   <%1> vamp:parameter_binding ?binding . "
             "   ?binding vamp:parameter ?param ; "
             "            vamp:value ?param_value . "
             "   ?param vamp:identifier ?param_id "
             " } "
             )
         .arg(transformUri));
    
    SimpleSPARQLQuery::ResultList paramResults = paramQuery.execute();
    
    if (!paramQuery.isOK()) {
        m_errorString = paramQuery.getErrorString();
        return false;
    }
    
    if (paramQuery.wasCancelled()) {
        m_errorString = "Query cancelled";
        return false;
    }
    
    for (int j = 0; j < paramResults.size(); ++j) {
        
        QString paramId = paramResults[j]["param_id"].value;
        QString paramValue = paramResults[j]["param_value"].value;
        
        if (paramId == "" || paramValue == "") continue;
        
        transform.setParameter(paramId, paramValue.toFloat());
    }

    return true;
}

QString
RDFTransformFactoryImpl::writeTransformToRDF(const Transform &transform,
                                             QString uri)
{
    QString str;
    QTextStream s(&str);

    // assumes the usual prefixes are available; requires that uri be
    // a local fragment (e.g. ":transform") rather than a uri enclosed
    // in <>, so that we can suffix it if need be

    QString pluginId = transform.getPluginIdentifier();
    QString pluginUri = PluginRDFIndexer::getInstance()->getURIForPluginId(pluginId);

    if (pluginUri != "") {
        s << uri << " a vamp:Transform ;" << endl;
        s << "    vamp:plugin <" << QUrl(pluginUri).toEncoded().data() << "> ;" << endl;
    } else {
        std::cerr << "WARNING: RDFTransformFactory::writeTransformToRDF: No plugin URI available for plugin id \"" << pluginId.toStdString() << "\", writing synthetic plugin and library resources" << std::endl;
        QString type, soname, label;
        PluginIdentifier::parseIdentifier(pluginId, type, soname, label);
        s << uri << "_plugin a vamp:Plugin ;" << endl;
        s << "    vamp:identifier \"" << label << "\" .\n" << endl;
        s << uri << "_library a vamp:PluginLibrary ;" << endl;
        s << "    vamp:identifier \"" << soname << "\" ;" << endl;
        s << "    vamp:available_plugin " << uri << "_plugin .\n" << endl;
        s << uri << " a vamp:Transform ;" << endl;
        s << "    vamp:plugin " << uri << "_plugin ;" << endl;
    }

    PluginRDFDescription description(pluginId);
    QString outputId = transform.getOutput();
    QString outputUri = description.getOutputUri(outputId);

    if (transform.getOutput() != "" && outputUri == "") {
        std::cerr << "WARNING: RDFTransformFactory::writeTransformToRDF: No output URI available for transform output id \"" << transform.getOutput().toStdString() << "\", writing a synthetic output resource" << std::endl;
    }

    if (transform.getStepSize() != 0) {
        s << "    vamp:step_size \"" << transform.getStepSize() << "\"^^xsd:int ; " << endl;
    }
    if (transform.getBlockSize() != 0) {
        s << "    vamp:block_size \"" << transform.getBlockSize() << "\"^^xsd:int ; " << endl;
    }
    if (transform.getStartTime() != RealTime::zeroTime) {
        s << "    vamp:start \"" << transform.getStartTime().toXsdDuration().c_str() << "\"^^xsd:duration ; " << endl;
    }
    if (transform.getDuration() != RealTime::zeroTime) {
        s << "    vamp:duration \"" << transform.getDuration().toXsdDuration().c_str() << "\"^^xsd:duration ; " << endl;
    }
    if (transform.getSampleRate() != 0) {
        s << "    vamp:sample_rate \"" << transform.getSampleRate() << "\"^^xsd:float ; " << endl;
    }
    
    QString program = transform.getProgram();
    if (program != "") {
        s << "    vamp:program \"\"\"" << program << "\"\"\" ;" << endl;
    }

    QString summary = transform.summaryTypeToString(transform.getSummaryType());
    if (summary != "") {
        s << "    vamp:summary_type \"" << summary << "\" ;" << endl;
    }

    Transform::ParameterMap parameters = transform.getParameters();
    for (Transform::ParameterMap::const_iterator i = parameters.begin();
         i != parameters.end(); ++i) {
        QString name = i->first;
        float value = i->second;
        s << "    vamp:parameter_binding [" << endl;
        s << "        vamp:parameter [ vamp:identifier \"" << name << "\" ] ;" << endl;
        s << "        vamp:value \"" << value << "\"^^xsd:float ;" << endl;
        s << "    ] ;" << endl;
    }

    if (outputUri != "") {
        s << "    vamp:output <" << QUrl(outputUri).toEncoded().data() << "> ." << endl;
    } else if (outputId != "") {
        s << "    vamp:output [ vamp:identifier \"" << outputId << "\" ] ." << endl;
    } else {
        s << "    ." << endl;
    }

    return str;
}

