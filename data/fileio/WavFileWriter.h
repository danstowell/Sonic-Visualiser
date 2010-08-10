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

#ifndef _WAV_FILE_WRITER_H_
#define _WAV_FILE_WRITER_H_

#include <QString>

#include <sndfile.h>

class DenseTimeValueModel;
class MultiSelection;

class WavFileWriter
{
public:
    WavFileWriter(QString path, size_t sampleRate, size_t channels);
    virtual ~WavFileWriter();

    bool isOK() const;

    virtual QString getError() const;

    QString getPath() const { return m_path; }

    bool writeModel(DenseTimeValueModel *source,
                    MultiSelection *selection = 0);

    bool writeSamples(float **samples, size_t count); // count per channel

    bool close();

protected:
    QString m_path;
    size_t m_sampleRate;
    size_t m_channels;
    SNDFILE *m_file;
    QString m_error;
};


#endif
