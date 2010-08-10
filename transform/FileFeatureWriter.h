/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.

    Sonic Annotator
    A utility for batch feature extraction from audio files.

    Mark Levy, Chris Sutton and Chris Cannam, Queen Mary, University of London.
    Copyright 2007-2008 QMUL.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _FILE_FEATURE_WRITER_H_
#define _FILE_FEATURE_WRITER_H_

#include <string>
#include <map>
#include <set>

#include "FeatureWriter.h"

using std::string;
using std::map;
using std::set;
using std::pair;

class QTextStream;
class QFile;

class FileFeatureWriter : public FeatureWriter
{
public:
    virtual ~FileFeatureWriter();

    virtual ParameterList getSupportedParameters() const;
    virtual void setParameters(map<string, string> &params);

    virtual void testOutputFile(QString trackId, TransformId transformId);
    virtual void flush();
    virtual void finish();

protected:
    enum FileWriteSupport {
        SupportOneFilePerTrackTransform = 1,
        SupportOneFilePerTrack = 2,
        SupportOneFileTotal = 4
    };

    FileFeatureWriter(int support, QString extension);
    QTextStream *getOutputStream(QString, TransformId);

    typedef pair<QString, TransformId> TrackTransformPair;
    typedef map<TrackTransformPair, QFile *> FileMap;
    typedef map<QFile *, QTextStream *> FileStreamMap;
    FileMap m_files;
    FileStreamMap m_streams;
    QTextStream *m_prevstream;

    QString getOutputFilename(QString, TransformId);
    QFile *getOutputFile(QString, TransformId);
    
    // subclass can implement this to be called before file is opened for append
    virtual void reviewFileForAppending(QString filename) { }

    int m_support;
    QString m_extension;
    QString m_baseDir;
    bool m_manyFiles;
    QString m_singleFileName;
    bool m_stdout;
    bool m_append;
    bool m_force;
};

#endif
