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

#ifndef _CSV_FILE_READER_H_
#define _CSV_FILE_READER_H_

#include "DataFileReader.h"

#include "CSVFormat.h"

#include <QList>
#include <QStringList>

class QFile;

class CSVFileReader : public DataFileReader
{
public:
    CSVFileReader(QString path, CSVFormat format, size_t mainModelSampleRate);
    virtual ~CSVFileReader();

    virtual bool isOK() const;
    virtual QString getError() const;
    virtual Model *load() const;

protected:
    CSVFormat m_format;
    QFile *m_file;
    QString m_error;
    mutable int m_warnings;
    size_t m_mainModelSampleRate;

    size_t convertTimeValue(QString, int lineno, size_t sampleRate,
                            size_t windowSize) const;
};


#endif

