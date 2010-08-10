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

#ifndef _WAV_FILE_READER_H_
#define _WAV_FILE_READER_H_

#include "AudioFileReader.h"

#include <sndfile.h>
#include <QMutex>

#include <set>

class WavFileReader : public AudioFileReader
{
public:
    WavFileReader(FileSource source, bool fileUpdating = false);
    virtual ~WavFileReader();

    virtual QString getLocation() const { return m_source.getLocation(); }
    virtual QString getError() const { return m_error; }

    /** 
     * Must be safe to call from multiple threads with different
     * arguments on the same object at the same time.
     */
    virtual void getInterleavedFrames(size_t start, size_t count,
				      SampleBlock &frames) const;
    
    static void getSupportedExtensions(std::set<QString> &extensions);
    static bool supportsExtension(QString ext);
    static bool supportsContentType(QString type);
    static bool supports(FileSource &source);

    virtual int getDecodeCompletion() const { return 100; }

    bool isUpdating() const { return m_updating; }

    void updateFrameCount();
    void updateDone();

protected:
    SF_INFO m_fileInfo;
    SNDFILE *m_file;

    FileSource m_source;
    QString m_path;
    QString m_error;

    mutable QMutex m_mutex;
    mutable float *m_buffer;
    mutable size_t m_bufsiz;
    mutable size_t m_lastStart;
    mutable size_t m_lastCount;

    bool m_updating;
};

#endif
