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

#ifndef _FILE_SOURCE_H_
#define _FILE_SOURCE_H_

#include <QUrl>
#include <QMutex>
#include <QString>
#include <QTimer>

#include <map>

class QFtp;
class QHttp;
class QFile;
class QHttpResponseHeader;
class ProgressReporter;

/**
 * FileSource is a class used to refer to the contents of a file that
 * may be either local or at a remote location such as a HTTP URL.
 *
 * When a FileSource object is constructed, the file or URL passed to
 * its constructor is tested for validity, and if it refers to a
 * remote HTTP or FTP location it is retrieved asynchronously (the Qt
 * event loop must be running) and cached locally.  You can then query
 * a local path for the file and refer to that as a normal filename.
 *
 * Use isAvailable() to test whether the file or remote URL exists,
 * and isOK() to test for internal validity or transmission errors.
 * Call waitForStatus() to block until the availability and validity
 * of the URL have been established so that isAvailable may be called,
 * and waitForData() to block until the entire file has been cached.
 *
 * FileSource handles reference counting for cache files.  You can
 * construct many FileSource objects with the same URL and you will
 * receive the same cached file for each; it is also reasonable to
 * pass FileSource objects by value.  FileSource only makes sense for
 * stateless URLs that result in the same data on each request.
 *
 * Cached files share a lifetime with their "owning" FileSource
 * objects; when the last FileSource referring to a given URL is
 * deleted or goes out of scope, its cached file (if any) is also
 * removed.
 */
class FileSource : public QObject
{
    Q_OBJECT

public:
    /**
     * Construct a FileSource using the given local file path or URL.
     * The URL may be raw or encoded.
     *
     * If a ProgressReporter is provided, it will be updated with
     * progress status.  Note that the progress() signal will also be
     * emitted regularly during retrieval, even if no reporter is
     * supplied here.  Caller retains ownership of the reporter object.
     */
    FileSource(QString fileOrUrl,
               ProgressReporter *reporter = 0,
               QString preferredContentType = "");

    /**
     * Construct a FileSource using the given remote URL.
     *
     * If a ProgressReporter is provided, it will be updated with
     * progress status.  Note that the progress() signal will also be
     * emitted regularly during retrieval, even if no reporter is
     * supplied here.  Caller retains ownership of the reporter object.
     */
    FileSource(QUrl url, ProgressReporter *reporter = 0);

    FileSource(const FileSource &);

    virtual ~FileSource();

    /**
     * Block on a sub-event-loop until the availability of the file or
     * remote URL is known.
     */
    void waitForStatus();

    /**
     * Block on a sub-event-loop until the whole of the data has been
     * retrieved (if it is remote).
     */
    void waitForData();

    /**
     * Return true if the FileSource object is valid and no error
     * occurred in looking up the file or remote URL.  Non-existence
     * of the file or URL is not an error -- call isAvailable() to
     * test for that.
     */
    bool isOK() const;

    /**
     * Return true if the file or remote URL exists.  This may block
     * on a sub-event-loop (calling waitForStatus) if the status is
     * not yet available.
     */
    bool isAvailable();

    /**
     * Return true if the entire file has been retrieved and is
     * available.
     */
    bool isDone() const;

    /**
     * Return true if this FileSource is referring to a remote URL.
     */
    bool isRemote() const;

    /**
     * Return the location filename or URL as passed to the
     * constructor.
     */
    QString getLocation() const;

    /**
     * Return the name of the local file this FileSource refers to.
     * This is the filename that should be used when reading normally
     * from the FileSource.  If the filename passed to the constructor
     * was a local file, this will return the same filename; otherwise
     * this will be the name of the temporary cache file owned by the
     * FileSource.
     */
    QString getLocalFilename() const;

    /**
     * Return the MIME content type of this file, if known.
     */
    QString getContentType() const;

    /**
     * Return the file extension for this file, if any.
     */
    QString getExtension() const;

    /**
     * Return an error message, if isOK() is false.
     */
    QString getErrorString() const;

    /**
     * Specify whether any local, cached file should remain on disc
     * after this FileSource has been destroyed.  The default is false
     * (cached files share their FileSource owners' lifespans).
     */
    void setLeaveLocalFile(bool leave);

    /**
     * Return true if the given filename or URL refers to a remote
     * URL.
     */
    static bool isRemote(QString fileOrUrl);

    /**
     * Return true if FileSource can handle the retrieval scheme for
     * the given URL (or if the URL is for a local file).
     */
    static bool canHandleScheme(QUrl url);

signals:
    /**
     * Emitted during URL retrieval, when the retrieval progress
     * notches up to a new percentage.
     */
    void progress(int percent);

    /**
     * Emitted when the file's existence has been tested and/or
     * response header received.  Calls to isAvailable() after this
     * has been emitted will not block.
     */
    void statusAvailable();

    /**
     * Emitted when the entire file data has been retrieved and the
     * local file is complete (if no error has occurred).
     */
    void ready();

protected slots:
    void dataReadProgress(int done, int total);
    void httpResponseHeaderReceived(const QHttpResponseHeader &resp);
    void ftpCommandFinished(int, bool);
    void dataTransferProgress(qint64 done, qint64 total);
    void done(bool error);
    void cancelled();

protected:
    FileSource &operator=(const FileSource &); // not provided

    QUrl m_url;
    QFtp *m_ftp;
    QHttp *m_http;
    QFile *m_localFile;
    QString m_localFilename;
    QString m_errorString;
    QString m_contentType;
    QString m_preferredContentType;
    bool m_ok;
    int m_lastStatus;
    bool m_remote;
    bool m_done;
    bool m_leaveLocalFile;
    ProgressReporter *m_reporter;

    typedef std::map<QUrl, int> RemoteRefCountMap;
    typedef std::map<QUrl, QString> RemoteLocalMap;
    static RemoteRefCountMap m_refCountMap;
    static RemoteLocalMap m_remoteLocalMap;
    static QMutex m_mapMutex;
    bool m_refCounted;

    void init();
    void initHttp();
    void initFtp();

    void cleanup();

    // Create a local file for m_url.  If it already existed, return true.
    // The local filename is stored in m_localFilename.
    bool createCacheFile();
    void deleteCacheFile();

    static QMutex m_fileCreationMutex;
    static int m_count;
};

#endif
