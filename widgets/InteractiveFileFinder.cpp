/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "InteractiveFileFinder.h"
#include "data/fileio/FileSource.h"
#include "data/fileio/AudioFileReaderFactory.h"
#include "data/fileio/DataFileReaderFactory.h"
#include "rdf/RDFImporter.h"
#include "rdf/RDFExporter.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QImageReader>
#include <QSettings>

#include <iostream>

InteractiveFileFinder 
InteractiveFileFinder::m_instance;

InteractiveFileFinder::InteractiveFileFinder() :
    m_lastLocatedLocation("")
{
    std::cerr << "Registering interactive file finder" << std::endl;
    FileFinder::registerFileFinder(this);
}

InteractiveFileFinder::~InteractiveFileFinder()
{
}

QString
InteractiveFileFinder::getOpenFileName(FileType type, QString fallbackLocation)
{
    QString settingsKey;
    QString lastPath = fallbackLocation;
    
    QString title = tr("Select file");
    QString filter = tr("All files (*.*)");

    switch (type) {

    case SessionFile:
        settingsKey = "sessionpath";
        title = tr("Select a session file");
        filter = tr("Sonic Visualiser session files (*.sv)\nRDF files (%1)\nAll files (*.*)").arg(RDFImporter::getKnownExtensions());
        break;

    case AudioFile:
        settingsKey = "audiopath";
        title = "Select an audio file";
        filter = tr("Audio files (%1)\nAll files (*.*)")
            .arg(AudioFileReaderFactory::getKnownExtensions());
        break;

    case LayerFile:
        settingsKey = "layerpath";
        filter = tr("All supported files (%1 %2)\nSonic Visualiser Layer XML files (*.svl)\nComma-separated data files (*.csv)\nSpace-separated .lab files (*.lab)\nRDF files (%2)\nMIDI files (*.mid)\nText files (*.txt)\nAll files (*.*)")
            .arg(DataFileReaderFactory::getKnownExtensions())
            .arg(RDFImporter::getKnownExtensions());
        break;

    case LayerFileNoMidi:
        settingsKey = "layerpath";
        filter = tr("All supported files (%1 %2)\nSonic Visualiser Layer XML files (*.svl)\nComma-separated data files (*.csv)\nSpace-separated .lab files (*.lab)\nRDF files (%2)\nText files (*.txt)\nAll files (*.*)")
            .arg(DataFileReaderFactory::getKnownExtensions())
            .arg(RDFImporter::getKnownExtensions());
        break;

    case SessionOrAudioFile:
        settingsKey = "lastpath";
        filter = tr("All supported files (*.sv %1 %2)\nSonic Visualiser session files (*.sv)\nAudio files (%2)\nRDF files (%1)\nAll files (*.*)")
            .arg(RDFImporter::getKnownExtensions())
            .arg(AudioFileReaderFactory::getKnownExtensions());
        break;

    case ImageFile:
        settingsKey = "imagepath";
        {
            QStringList fmts;
            QList<QByteArray> formats = QImageReader::supportedImageFormats();
            for (QList<QByteArray>::iterator i = formats.begin();
                 i != formats.end(); ++i) {
                fmts.push_back(QString("*.%1")
                               .arg(QString::fromLocal8Bit(*i).toLower()));
            }
            filter = tr("Image files (%1)\nAll files (*.*)").arg(fmts.join(" "));
        }
        break;

    case AnyFile:
        settingsKey = "lastpath";
        filter = tr("All supported files (*.sv %1 %2 %3)\nSonic Visualiser session files (*.sv)\nAudio files (%1)\nLayer files (%2)\nRDF files (%3)\nAll files (*.*)")
            .arg(AudioFileReaderFactory::getKnownExtensions())
            .arg(DataFileReaderFactory::getKnownExtensions())
            .arg(RDFImporter::getKnownExtensions());
        break;
    };

    if (lastPath == "") {
        char *home = getenv("HOME");
        if (home) lastPath = home;
        else lastPath = ".";
    } else if (QFileInfo(lastPath).isDir()) {
        lastPath = QFileInfo(lastPath).canonicalPath();
    } else {
        lastPath = QFileInfo(lastPath).absoluteDir().canonicalPath();
    }

    QSettings settings;
    settings.beginGroup("FileFinder");
    lastPath = settings.value(settingsKey, lastPath).toString();

    QString path = "";

    // Use our own QFileDialog just for symmetry with getSaveFileName below

    QFileDialog dialog;
    dialog.setFilters(filter.split('\n'));
    dialog.setWindowTitle(title);
    dialog.setDirectory(lastPath);

    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    
    if (dialog.exec()) {
        QStringList files = dialog.selectedFiles();
        if (!files.empty()) path = *files.begin();
        
        QFileInfo fi(path);
        
        if (!fi.exists()) {
            
            QMessageBox::critical(0, tr("File does not exist"),
                                  tr("<b>File not found</b><p>File \"%1\" does not exist").arg(path));
            path = "";
            
        } else if (!fi.isReadable()) {
            
            QMessageBox::critical(0, tr("File is not readable"),
                                  tr("<b>File is not readable</b><p>File \"%1\" can not be read").arg(path));
            path = "";
            
        } else if (fi.isDir()) {
            
            QMessageBox::critical(0, tr("Directory selected"),
                                  tr("<b>Directory selected</b><p>File \"%1\" is a directory").arg(path));
            path = "";

        } else if (!fi.isFile()) {
            
            QMessageBox::critical(0, tr("Non-file selected"),
                                  tr("<b>Not a file</b><p>Path \"%1\" is not a file").arg(path));
            path = "";
            
        } else if (fi.size() == 0) {
            
            QMessageBox::critical(0, tr("File is empty"),
                                  tr("<b>File is empty</b><p>File \"%1\" is empty").arg(path));
            path = "";
        }                
    }

    if (path != "") {
        settings.setValue(settingsKey,
                          QFileInfo(path).absoluteDir().canonicalPath());
    }
    
    return path;
}

QString
InteractiveFileFinder::getSaveFileName(FileType type, QString fallbackLocation)
{
    QString settingsKey;
    QString lastPath = fallbackLocation;
    
    QString title = tr("Select file");
    QString filter = tr("All files (*.*)");

    switch (type) {

    case SessionFile:
        settingsKey = "savesessionpath";
        title = tr("Select a session file");
        filter = tr("Sonic Visualiser session files (*.sv)\nAll files (*.*)");
        break;

    case AudioFile:
        settingsKey = "saveaudiopath";
        title = "Select an audio file";
        title = tr("Select a file to export to");
        filter = tr("WAV audio files (*.wav)\nAll files (*.*)");
        break;

    case LayerFile:
        settingsKey = "savelayerpath";
        title = tr("Select a file to export to");
        filter = tr("Sonic Visualiser Layer XML files (*.svl)\nComma-separated data files (*.csv)\nRDF/Turtle files (%1)\nMIDI files (*.mid)\nText files (*.txt)\nAll files (*.*)").arg(RDFExporter::getSupportedExtensions());
        break;

    case LayerFileNoMidi:
        settingsKey = "savelayerpath";
        title = tr("Select a file to export to");
        filter = tr("Sonic Visualiser Layer XML files (*.svl)\nComma-separated data files (*.csv)\nRDF/Turtle files (%1)\nText files (*.txt)\nAll files (*.*)").arg(RDFExporter::getSupportedExtensions());
        break;

    case SessionOrAudioFile:
        std::cerr << "ERROR: Internal error: InteractiveFileFinder::getSaveFileName: SessionOrAudioFile cannot be used here" << std::endl;
        abort();

    case ImageFile:
        settingsKey = "saveimagepath";
        title = tr("Select a file to export to");
        filter = tr("Portable Network Graphics files (*.png)\nAll files (*.*)");
        break;

    case AnyFile:
        std::cerr << "ERROR: Internal error: InteractiveFileFinder::getSaveFileName: AnyFile cannot be used here" << std::endl;
        abort();
    };

    if (lastPath == "") {
        char *home = getenv("HOME");
        if (home) lastPath = home;
        else lastPath = ".";
    } else if (QFileInfo(lastPath).isDir()) {
        lastPath = QFileInfo(lastPath).canonicalPath();
    } else {
        lastPath = QFileInfo(lastPath).absoluteDir().canonicalPath();
    }

    QSettings settings;
    settings.beginGroup("FileFinder");
    lastPath = settings.value(settingsKey, lastPath).toString();

    QString path = "";

    // Use our own QFileDialog instead of static functions, as we may
    // need to adjust the file extension based on the selected filter

    QFileDialog dialog;
    dialog.setFilters(filter.split('\n'));
    dialog.setWindowTitle(title);
    dialog.setDirectory(lastPath);

    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setConfirmOverwrite(false); // we'll do that
        
    if (type == SessionFile) {
        dialog.setDefaultSuffix("sv");
    } else if (type == AudioFile) {
        dialog.setDefaultSuffix("wav");
    } else if (type == ImageFile) {
        dialog.setDefaultSuffix("png");
    }

    bool good = false;

    while (!good) {

        path = "";
        
        if (!dialog.exec()) break;
        
        QStringList files = dialog.selectedFiles();
        if (files.empty()) break;
        path = *files.begin();
        
        QFileInfo fi(path);

        std::cerr << "type = " << type << ", suffix = " << fi.suffix().toStdString() << std::endl;
        
        if ((type == LayerFile || type == LayerFileNoMidi)
            && fi.suffix() == "") {
            QString expectedExtension;
            QString selectedFilter = dialog.selectedFilter();
            if (selectedFilter.contains(".svl")) {
                expectedExtension = "svl";
            } else if (selectedFilter.contains(".txt")) {
                expectedExtension = "txt";
            } else if (selectedFilter.contains(".csv")) {
                expectedExtension = "csv";
            } else if (selectedFilter.contains(".mid")) {
                expectedExtension = "mid";
            } else if (selectedFilter.contains(".ttl")) {
                expectedExtension = "ttl";
            }
            std::cerr << "expected extension = " << expectedExtension.toStdString() << std::endl;
            if (expectedExtension != "") {
                path = QString("%1.%2").arg(path).arg(expectedExtension);
                fi = QFileInfo(path);
            }
        }
        
        if (fi.isDir()) {
            QMessageBox::critical(0, tr("Directory selected"),
                                  tr("<b>Directory selected</b><p>File \"%1\" is a directory").arg(path));
            continue;
        }
        
        if (fi.exists()) {
            if (QMessageBox::question(0, tr("File exists"),
                                      tr("<b>File exists</b><p>The file \"%1\" already exists.\nDo you want to overwrite it?").arg(path),
                                      QMessageBox::Ok,
                                      QMessageBox::Cancel) != QMessageBox::Ok) {
                continue;
            }
        }
        
        good = true;
    }
        
    if (path != "") {
        settings.setValue(settingsKey,
                          QFileInfo(path).absoluteDir().canonicalPath());
    }
    
    return path;
}

void
InteractiveFileFinder::registerLastOpenedFilePath(FileType type, QString path)
{
    QString settingsKey;

    switch (type) {
    case SessionFile:
        settingsKey = "sessionpath";
        break;

    case AudioFile:
        settingsKey = "audiopath";
        break;

    case LayerFile:
        settingsKey = "layerpath";
        break;

    case LayerFileNoMidi:
        settingsKey = "layerpath";
        break;

    case SessionOrAudioFile:
        settingsKey = "lastpath";
        break;

    case ImageFile:
        settingsKey = "imagepath";
        break;

    case AnyFile:
        settingsKey = "lastpath";
        break;
    }

    if (path != "") {
        QSettings settings;
        settings.beginGroup("FileFinder");
        path = QFileInfo(path).absoluteDir().canonicalPath();
        settings.setValue(settingsKey, path);
        settings.setValue("lastpath", path);
    }
}
    
QString
InteractiveFileFinder::find(FileType type, QString location, QString lastKnownLocation)
{
    if (FileSource::canHandleScheme(location)) {
        if (FileSource(location).isAvailable()) {
            std::cerr << "InteractiveFileFinder::find: ok, it's available... returning" << std::endl;
            return location;
        }
    }

    if (QFileInfo(location).exists()) return location;

    QString foundAt = "";

    if ((foundAt = findRelative(location, lastKnownLocation)) != "") {
        return foundAt;
    }

    if ((foundAt = findRelative(location, m_lastLocatedLocation)) != "") {
        return foundAt;
    }

    return locateInteractive(type, location);
}

QString
InteractiveFileFinder::findRelative(QString location, QString relativeTo)
{
    if (relativeTo == "") return "";

    std::cerr << "Looking for \"" << location.toStdString() << "\" next to \""
              << relativeTo.toStdString() << "\"..." << std::endl;

    QString fileName;
    QString resolved;

    if (FileSource::isRemote(location)) {
        fileName = QUrl(location).path().section('/', -1, -1,
                                                 QString::SectionSkipEmpty);
    } else {
        if (QUrl(location).scheme() == "file") {
            location = QUrl(location).toLocalFile();
        }
        fileName = QFileInfo(location).fileName();
    }

    if (FileSource::isRemote(relativeTo)) {
        resolved = QUrl(relativeTo).resolved(fileName).toString();
        if (!FileSource(resolved).isAvailable()) resolved = "";
        std::cerr << "resolved: " << resolved.toStdString() << std::endl;
    } else {
        if (QUrl(relativeTo).scheme() == "file") {
            relativeTo = QUrl(relativeTo).toLocalFile();
        }
        resolved = QFileInfo(relativeTo).dir().filePath(fileName);
        if (!QFileInfo(resolved).exists() ||
            !QFileInfo(resolved).isFile() ||
            !QFileInfo(resolved).isReadable()) {
            resolved = "";
        }
    }
            
    return resolved;
}

QString
InteractiveFileFinder::locateInteractive(FileType type, QString thing)
{
    QString question;
    if (type == AudioFile) {
        question = tr("<b>File not found</b><p>Audio file \"%1\" could not be opened.\nDo you want to locate it?");
    } else {
        question = tr("<b>File not found</b><p>File \"%1\" could not be opened.\nDo you want to locate it?");
    }

    QString path = "";
    bool done = false;

    while (!done) {

        int rv = QMessageBox::question
            (0, 
             tr("Failed to open file"),
             question.arg(thing),
             tr("Locate file..."),
             tr("Use URL..."),
             tr("Cancel"),
             0, 2);
        
        switch (rv) {

        case 0: // Locate file

            if (QFileInfo(thing).dir().exists()) {
                path = QFileInfo(thing).dir().canonicalPath();
            }
            
            path = getOpenFileName(type, path);
            done = (path != "");
            break;

        case 1: // Use URL
        {
            bool ok = false;
            path = QInputDialog::getText
                (0, tr("Use URL"),
                 tr("Please enter the URL to use for this file:"),
                 QLineEdit::Normal, "", &ok);

            if (ok && path != "") {
                if (FileSource(path).isAvailable()) {
                    done = true;
                } else {
                    QMessageBox::critical
                        (0, tr("Failed to open location"),
                         tr("<b>Failed to open location</b><p>URL \"%1\" could not be opened").arg(path));
                    path = "";
                }
            }
            break;
        }

        case 2: // Cancel
            path = "";
            done = true;
            break;
        }
    }

    if (path != "") m_lastLocatedLocation = path;
    return path;
}


