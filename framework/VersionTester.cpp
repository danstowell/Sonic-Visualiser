/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

/*
   This is a modified version of a source file from the 
   Rosegarden MIDI and audio sequencer and notation editor.
   This file copyright 2000-2009 Chris Cannam.
*/

#include "VersionTester.h"

#include <iostream>

#include <QHttp>

VersionTester::VersionTester(QString hostname, QString versionFilePath,
			     QString myVersion) :
    m_httpFailed(false),
    m_myVersion(myVersion)
{
    QHttp *http = new QHttp();
    connect(http, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)),
            this, SLOT(httpResponseHeaderReceived(const QHttpResponseHeader &)));
    connect(http, SIGNAL(done(bool)),
            this, SLOT(httpDone(bool)));
    http->setHost(hostname);
    http->get(versionFilePath);
}

VersionTester::~VersionTester()
{
}

bool
VersionTester::isVersionNewerThan(QString a, QString b)
{
    QRegExp re("[._-]");
    QStringList alist = a.split(re, QString::SkipEmptyParts);
    QStringList blist = b.split(re, QString::SkipEmptyParts);
    int ae = alist.size();
    int be = blist.size();
    int e = std::max(ae, be);
    for (int i = 0; i < e; ++i) {
    int an = 0, bn = 0;
    if (i < ae) {
        an = alist[i].toInt();
        if (an == 0) an = -1; // non-numeric field -> "-pre1" etc
    }
    if (i < be) {
        bn = blist[i].toInt();
        if (bn == 0) bn = -1;
    }
    if (an < bn) return false;
    if (an > bn) return true;
    }
    return false;
}

void
VersionTester::httpResponseHeaderReceived(const QHttpResponseHeader &h)
{
    if (h.statusCode() / 100 != 2) m_httpFailed = true;
}

void
VersionTester::httpDone(bool error)
{
    QHttp *http = const_cast<QHttp *>(dynamic_cast<const QHttp *>(sender()));
    if (!http) return;
    http->deleteLater();
    if (error || m_httpFailed) return;

    QByteArray responseData = http->readAll();
    QString str = QString::fromUtf8(responseData.data());
    QStringList lines = str.split('\n', QString::SkipEmptyParts);
    if (lines.empty()) return;

    QString latestVersion = lines[0];
    std::cerr << "Comparing current version \"" << m_myVersion.toStdString()
              << "\" with latest version \"" << latestVersion.toStdString()
	      << "\"" << std::endl;
    if (isVersionNewerThan(latestVersion, m_myVersion)) {
        emit newerVersionAvailable(latestVersion);
    }
}


