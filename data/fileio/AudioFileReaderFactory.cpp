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

#include "AudioFileReaderFactory.h"

#include "WavFileReader.h"
#include "ResamplingWavFileReader.h"
#include "OggVorbisFileReader.h"
#include "MP3FileReader.h"
#include "QuickTimeFileReader.h"

#include <QString>
#include <QFileInfo>
#include <iostream>

QString
AudioFileReaderFactory::getKnownExtensions()
{
    std::set<QString> extensions;

    WavFileReader::getSupportedExtensions(extensions);
#ifdef HAVE_MAD
    MP3FileReader::getSupportedExtensions(extensions);
#endif
#ifdef HAVE_OGGZ
#ifdef HAVE_FISHSOUND
    OggVorbisFileReader::getSupportedExtensions(extensions);
#endif
#endif
#ifdef HAVE_QUICKTIME
    QuickTimeFileReader::getSupportedExtensions(extensions);
#endif

    QString rv;
    for (std::set<QString>::const_iterator i = extensions.begin();
         i != extensions.end(); ++i) {
        if (i != extensions.begin()) rv += " ";
        rv += "*." + *i;
    }

    return rv;
}

AudioFileReader *
AudioFileReaderFactory::createReader(FileSource source, size_t targetRate,
                                     ProgressReporter *reporter)
{
    return create(source, targetRate, false, reporter);
}

AudioFileReader *
AudioFileReaderFactory::createThreadingReader(FileSource source, size_t targetRate,
                                              ProgressReporter *reporter)
{
    return create(source, targetRate, true, reporter);
}

AudioFileReader *
AudioFileReaderFactory::create(FileSource source, size_t targetRate, bool threading,
                               ProgressReporter *reporter)
{
    QString err;

//    std::cerr << "AudioFileReaderFactory::createReader(\"" << source.getLocation().toStdString() << "\"): Requested rate: " << targetRate << std::endl;

    if (!source.isOK() || !source.isAvailable()) {
        std::cerr << "AudioFileReaderFactory::createReader(\"" << source.getLocation().toStdString() << "\": Source unavailable" << std::endl;
        return 0;
    }

    AudioFileReader *reader = 0;

    // Try to construct a preferred reader based on the extension or
    // MIME type.

    if (WavFileReader::supports(source)) {

        reader = new WavFileReader(source);

        if (targetRate != 0 &&
            reader->isOK() &&
            reader->getSampleRate() != targetRate) {

            std::cerr << "AudioFileReaderFactory::createReader: WAV file rate: " << reader->getSampleRate() << ", creating resampling reader" << std::endl;

            delete reader;
            reader = new ResamplingWavFileReader
                (source,
                 threading ?
                 ResamplingWavFileReader::ResampleThreaded :
                 ResamplingWavFileReader::ResampleAtOnce,
                 ResamplingWavFileReader::CacheInTemporaryFile,
                 targetRate,
                 reporter);
            if (!reader->isOK()) {
                delete reader;
                reader = 0;
            }
        }
    }
    
#ifdef HAVE_OGGZ
#ifdef HAVE_FISHSOUND
    if (!reader) {
        if (OggVorbisFileReader::supports(source)) {
            reader = new OggVorbisFileReader
                (source,
                 threading ?
                 OggVorbisFileReader::DecodeThreaded :
                 OggVorbisFileReader::DecodeAtOnce,
                 OggVorbisFileReader::CacheInTemporaryFile,
                 targetRate,
                 reporter);
            if (!reader->isOK()) {
                delete reader;
                reader = 0;
            }
        }
    }
#endif
#endif

#ifdef HAVE_MAD
    if (!reader) {
        if (MP3FileReader::supports(source)) {
            reader = new MP3FileReader
                (source,
                 threading ?
                 MP3FileReader::DecodeThreaded :
                 MP3FileReader::DecodeAtOnce,
                 MP3FileReader::CacheInTemporaryFile,
                 targetRate,
                 reporter);
            if (!reader->isOK()) {
                delete reader;
                reader = 0;
            }
        }
    }
#endif

#ifdef HAVE_QUICKTIME
    if (!reader) {
        if (QuickTimeFileReader::supports(source)) {
            reader = new QuickTimeFileReader
                (source,
                 threading ?
                 QuickTimeFileReader::DecodeThreaded : 
                 QuickTimeFileReader::DecodeAtOnce,
                 QuickTimeFileReader::CacheInTemporaryFile,
                 targetRate,
                 reporter);
            if (!reader->isOK()) {
                delete reader;
                reader = 0;
            }
        }
    }
#endif

    // If none of the readers claimed to support this file extension,
    // perhaps the extension is missing or misleading.  Try again,
    // ignoring it.  We have to be confident that the reader won't
    // open just any old text file or whatever and pretend it's
    // succeeded

    if (!reader) {

        reader = new WavFileReader(source);

        if (targetRate != 0 &&
            reader->isOK() &&
            reader->getSampleRate() != targetRate) {

            std::cerr << "AudioFileReaderFactory::createReader: WAV file rate: " << reader->getSampleRate() << ", creating resampling reader" << std::endl;

            delete reader;
            reader = new ResamplingWavFileReader
                (source,
                 threading ?
                 ResamplingWavFileReader::ResampleThreaded :
                 ResamplingWavFileReader::ResampleAtOnce,
                 ResamplingWavFileReader::CacheInTemporaryFile,
                 targetRate,
                 reporter);
        }

        if (!reader->isOK()) {
            delete reader;
            reader = 0;
        }
    }
    
#ifdef HAVE_OGGZ
#ifdef HAVE_FISHSOUND
    if (!reader) {
        reader = new OggVorbisFileReader
            (source,
             threading ?
             OggVorbisFileReader::DecodeThreaded :
             OggVorbisFileReader::DecodeAtOnce,
             OggVorbisFileReader::CacheInTemporaryFile,
             targetRate,
             reporter);

        if (!reader->isOK()) {
            delete reader;
            reader = 0;
        }
    }
#endif
#endif

#ifdef HAVE_MAD
    if (!reader) {
        reader = new MP3FileReader
            (source,
             threading ?
             MP3FileReader::DecodeThreaded :
             MP3FileReader::DecodeAtOnce,
             MP3FileReader::CacheInTemporaryFile,
             targetRate,
             reporter);

        if (!reader->isOK()) {
            delete reader;
            reader = 0;
        }
    }
#endif

#ifdef HAVE_QUICKTIME
    if (!reader) {
        reader = new QuickTimeFileReader
            (source,
             threading ?
             QuickTimeFileReader::DecodeThreaded : 
             QuickTimeFileReader::DecodeAtOnce,
             QuickTimeFileReader::CacheInTemporaryFile,
             targetRate,
             reporter);

        if (!reader->isOK()) {
            delete reader;
            reader = 0;
        }
    }
#endif

    if (reader) {
        if (reader->isOK()) {
//            std::cerr << "AudioFileReaderFactory: Reader is OK" << std::endl;
            return reader;
        }
        std::cerr << "AudioFileReaderFactory: Preferred reader for "
                  << "url \"" << source.getLocation().toStdString()
                  << "\" (content type \""
                  << source.getContentType().toStdString() << "\") failed";

        if (reader->getError() != "") {
            std::cerr << ": \"" << reader->getError().toStdString() << "\"";
        }
        std::cerr << std::endl;
        delete reader;
        reader = 0;
    }

    std::cerr << "AudioFileReaderFactory: No reader" << std::endl;
    return reader;
}

