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

#include "WavFileWriter.h"

#include "model/DenseTimeValueModel.h"
#include "base/Selection.h"

#include <QFileInfo>

#include <iostream>

WavFileWriter::WavFileWriter(QString path,
			     size_t sampleRate,
                             size_t channels) :
    m_path(path),
    m_sampleRate(sampleRate),
    m_channels(channels),
    m_file(0)
{
    SF_INFO fileInfo;
    fileInfo.samplerate = m_sampleRate;
    fileInfo.channels = m_channels;
    fileInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    
    m_file = sf_open(m_path.toLocal8Bit(), SFM_WRITE, &fileInfo);
    if (!m_file) {
	std::cerr << "WavFileWriter: Failed to open file ("
		  << sf_strerror(m_file) << ")" << std::endl;
	m_error = QString("Failed to open audio file '%1' for writing")
	    .arg(m_path);
    }
}

WavFileWriter::~WavFileWriter()
{
    if (m_file) close();
}

bool
WavFileWriter::isOK() const
{
    return (m_error.isEmpty());
}

QString
WavFileWriter::getError() const
{
    return m_error;
}

bool
WavFileWriter::writeModel(DenseTimeValueModel *source,
                          MultiSelection *selection)
{
    if (source->getChannelCount() != m_channels) {
        std::cerr << "WavFileWriter::writeModel: Wrong number of channels ("
                  << source->getChannelCount()  << " != " << m_channels << ")"
                  << std::endl;
        m_error = QString("Failed to write model to audio file '%1'")
            .arg(m_path);
        return false;
    }

    if (!m_file) {
        m_error = QString("Failed to write model to audio file '%1': File not open")
            .arg(m_path);
	return false;
    }

    bool ownSelection = false;
    if (!selection) {
	selection = new MultiSelection;
	selection->setSelection(Selection(source->getStartFrame(),
					  source->getEndFrame()));
        ownSelection = true;
    }

    size_t bs = 2048;
    float *ub = new float[bs]; // uninterleaved buffer (one channel)
    float *ib = new float[bs * m_channels]; // interleaved buffer

    for (MultiSelection::SelectionList::iterator i =
	     selection->getSelections().begin();
	 i != selection->getSelections().end(); ++i) {
	
	size_t f0(i->getStartFrame()), f1(i->getEndFrame());

	for (size_t f = f0; f < f1; f += bs) {
	    
	    size_t n = std::min(bs, f1 - f);

	    for (int c = 0; c < int(m_channels); ++c) {
		source->getData(c, f, n, ub);
		for (size_t i = 0; i < n; ++i) {
		    ib[i * m_channels + c] = ub[i];
		}
	    }	    

	    sf_count_t written = sf_writef_float(m_file, ib, n);

	    if (written < n) {
		m_error = QString("Only wrote %1 of %2 frames at file frame %3")
		    .arg(written).arg(n).arg(f);
		break;
	    }
	}
    }

    delete[] ub;
    delete[] ib;
    if (ownSelection) delete selection;

    return isOK();
}
	
bool
WavFileWriter::writeSamples(float **samples, size_t count)
{
    if (!m_file) {
        m_error = QString("Failed to write model to audio file '%1': File not open")
            .arg(m_path);
	return false;
    }

    float *b = new float[count * m_channels];
    for (size_t i = 0; i < count; ++i) {
        for (size_t c = 0; c < m_channels; ++c) {
            b[i * m_channels + c] = samples[c][i];
        }
    }

    sf_count_t written = sf_writef_float(m_file, b, count);

    delete[] b;

    if (written < count) {
        m_error = QString("Only wrote %1 of %2 frames")
            .arg(written).arg(count);
    }

    return isOK();
}
    
bool
WavFileWriter::close()
{
    if (m_file) {
        sf_close(m_file);
        m_file = 0;
    }
    return true;
}

