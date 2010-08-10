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

#include "CSVFileReader.h"

#include "model/Model.h"
#include "base/RealTime.h"
#include "base/StringBits.h"
#include "model/SparseOneDimensionalModel.h"
#include "model/SparseTimeValueModel.h"
#include "model/EditableDenseThreeDimensionalModel.h"
#include "model/RegionModel.h"
#include "DataFileReaderFactory.h"

#include <QFile>
#include <QString>
#include <QRegExp>
#include <QStringList>
#include <QTextStream>

#include <iostream>
#include <map>

CSVFileReader::CSVFileReader(QString path, CSVFormat format,
                             size_t mainModelSampleRate) :
    m_format(format),
    m_file(0),
    m_warnings(0),
    m_mainModelSampleRate(mainModelSampleRate)
{
    m_file = new QFile(path);
    bool good = false;
    
    if (!m_file->exists()) {
	m_error = QFile::tr("File \"%1\" does not exist").arg(path);
    } else if (!m_file->open(QIODevice::ReadOnly | QIODevice::Text)) {
	m_error = QFile::tr("Failed to open file \"%1\"").arg(path);
    } else {
	good = true;
    }

    if (!good) {
	delete m_file;
	m_file = 0;
    }
}

CSVFileReader::~CSVFileReader()
{
    std::cerr << "CSVFileReader::~CSVFileReader: file is " << m_file << std::endl;

    if (m_file) {
        std::cerr << "CSVFileReader::CSVFileReader: Closing file" << std::endl;
        m_file->close();
    }
    delete m_file;
}

bool
CSVFileReader::isOK() const
{
    return (m_file != 0);
}

QString
CSVFileReader::getError() const
{
    return m_error;
}

size_t
CSVFileReader::convertTimeValue(QString s, int lineno, size_t sampleRate,
                                size_t windowSize) const
{
    QRegExp nonNumericRx("[^0-9eE.,+-]");
    unsigned int warnLimit = 10;

    CSVFormat::TimeUnits timeUnits = m_format.getTimeUnits();

    size_t calculatedFrame = 0;

    bool ok = false;
    QString numeric = s;
    numeric.remove(nonNumericRx);
    
    if (timeUnits == CSVFormat::TimeSeconds) {

        double time = numeric.toDouble(&ok);
        if (!ok) time = StringBits::stringToDoubleLocaleFree(numeric, &ok);
        calculatedFrame = int(time * sampleRate + 0.5);
        
    } else {
        
        long n = numeric.toLong(&ok);
        if (n >= 0) calculatedFrame = n;
        
        if (timeUnits == CSVFormat::TimeWindows) {
            calculatedFrame *= windowSize;
        }
    }
    
    if (!ok) {
        if (m_warnings < warnLimit) {
            std::cerr << "WARNING: CSVFileReader::load: "
                      << "Bad time format (\"" << s.toStdString()
                      << "\") in data line "
                      << lineno+1 << std::endl;
        } else if (m_warnings == warnLimit) {
            std::cerr << "WARNING: Too many warnings" << std::endl;
        }
        ++m_warnings;
    }

    return calculatedFrame;
}

Model *
CSVFileReader::load() const
{
    if (!m_file) return 0;

    CSVFormat::ModelType modelType = m_format.getModelType();
    CSVFormat::TimingType timingType = m_format.getTimingType();
    CSVFormat::TimeUnits timeUnits = m_format.getTimeUnits();
    size_t sampleRate = m_format.getSampleRate();
    size_t windowSize = m_format.getWindowSize();
    QChar separator = m_format.getSeparator();
    bool allowQuoting = m_format.getAllowQuoting();

    if (timingType == CSVFormat::ExplicitTiming) {
        if (modelType == CSVFormat::ThreeDimensionalModel) {
            // This will be overridden later if more than one line
            // appears in our file, but we want to choose a default
            // that's likely to be visible
            windowSize = 1024;
        } else {
            windowSize = 1;
        }
	if (timeUnits == CSVFormat::TimeSeconds) {
	    sampleRate = m_mainModelSampleRate;
	}
    }

    SparseOneDimensionalModel *model1 = 0;
    SparseTimeValueModel *model2 = 0;
    RegionModel *model2a = 0;
    EditableDenseThreeDimensionalModel *model3 = 0;
    Model *model = 0;

    QTextStream in(m_file);
    in.seek(0);

    unsigned int warnings = 0, warnLimit = 10;
    unsigned int lineno = 0;

    float min = 0.0, max = 0.0;

    size_t frameNo = 0;
    size_t duration = 0;
    size_t endFrame = 0;

    bool haveAnyValue = false;
    bool haveEndTime = false;

    size_t startFrame = 0; // for calculation of dense model resolution
    bool firstEverValue = true;

    std::map<QString, int> labelCountMap;
    
    while (!in.atEnd()) {

        // QTextStream's readLine doesn't cope with old-style Mac
        // CR-only line endings.  Why did they bother making the class
        // cope with more than one sort of line ending, if it still
        // can't be configured to cope with all the common sorts?

        // For the time being we'll deal with this case (which is
        // relatively uncommon for us, but still necessary to handle)
        // by reading the entire file using a single readLine, and
        // splitting it.  For CR and CR/LF line endings this will just
        // read a line at a time, and that's obviously OK.

        QString chunk = in.readLine();
        QStringList lines = chunk.split('\r', QString::SkipEmptyParts);
        
        for (size_t li = 0; li < lines.size(); ++li) {

            QString line = lines[li];

            if (line.startsWith("#")) continue;

            QStringList list = StringBits::split(line, separator, allowQuoting);
            if (!model) {

                switch (modelType) {

                case CSVFormat::OneDimensionalModel:
                    model1 = new SparseOneDimensionalModel(sampleRate, windowSize);
                    model = model1;
                    break;
		
                case CSVFormat::TwoDimensionalModel:
                    model2 = new SparseTimeValueModel(sampleRate, windowSize, false);
                    model = model2;
                    break;
		
                case CSVFormat::TwoDimensionalModelWithDuration:
                    model2a = new RegionModel(sampleRate, windowSize, false);
                    model = model2a;
                    break;
		
                case CSVFormat::ThreeDimensionalModel:
                    model3 = new EditableDenseThreeDimensionalModel
                        (sampleRate,
                         windowSize,
                         list.size(),
                         EditableDenseThreeDimensionalModel::NoCompression);
                    model = model3;
                    break;
                }
            }

            float value = 0.f;
            QString label = "";

            duration = 0.f;
            haveEndTime = false;

            for (int i = 0; i < list.size(); ++i) {

                QString s = list[i];

                CSVFormat::ColumnPurpose purpose = m_format.getColumnPurpose(i);

                switch (purpose) {

                case CSVFormat::ColumnUnknown:
                    break;

                case CSVFormat::ColumnStartTime:
                    frameNo = convertTimeValue(s, lineno, sampleRate, windowSize);
                    break;
                
                case CSVFormat::ColumnEndTime:
                    endFrame = convertTimeValue(s, lineno, sampleRate, windowSize);
                    haveEndTime = true;
                    break;

                case CSVFormat::ColumnDuration:
                    duration = convertTimeValue(s, lineno, sampleRate, windowSize);
                    break;

                case CSVFormat::ColumnValue:
                    value = s.toFloat();
                    haveAnyValue = true;
                    break;

                case CSVFormat::ColumnLabel:
                    label = s;
                    ++labelCountMap[label];
                    break;
                }
            }

            if (haveEndTime) { // ... calculate duration now all cols read
                if (endFrame > frameNo) {
                    duration = endFrame - frameNo;
                }
            }

            if (modelType == CSVFormat::OneDimensionalModel) {
	    
                SparseOneDimensionalModel::Point point(frameNo, label);
                model1->addPoint(point);

            } else if (modelType == CSVFormat::TwoDimensionalModel) {

                SparseTimeValueModel::Point point(frameNo, value, label);
                model2->addPoint(point);

            } else if (modelType == CSVFormat::TwoDimensionalModelWithDuration) {

                RegionModel::Point point(frameNo, value, duration, label);
                model2a->addPoint(point);

            } else if (modelType == CSVFormat::ThreeDimensionalModel) {

                DenseThreeDimensionalModel::Column values;

                for (int i = 0; i < list.size(); ++i) {

                    bool ok = false;
                    float value = list[i].toFloat(&ok);

                    if (m_format.getColumnPurpose(i) == CSVFormat::ColumnValue) {
                        values.push_back(value);
                    }
	    
                    if (firstEverValue || value < min) min = value;
                    if (firstEverValue || value > max) max = value;

                    if (firstEverValue) {
                        startFrame = frameNo;
                        model3->setStartFrame(startFrame);
                    } else if (lineno == 1 &&
                               timingType == CSVFormat::ExplicitTiming) {
                        model3->setResolution(frameNo - startFrame);
                    }
                    
                    firstEverValue = false;

                    if (!ok) {
                        if (warnings < warnLimit) {
                            std::cerr << "WARNING: CSVFileReader::load: "
                                      << "Non-numeric value \""
                                      << list[i].toStdString()
                                      << "\" in data line " << lineno+1
                                      << ":" << std::endl;
                            std::cerr << line.toStdString() << std::endl;
                            ++warnings;
                        } else if (warnings == warnLimit) {
//                            std::cerr << "WARNING: Too many warnings" << std::endl;
                        }
                    }
                }
	
//                std::cerr << "Setting bin values for count " << lineno << ", frame "
//                          << frameNo << ", time " << RealTime::frame2RealTime(frameNo, sampleRate) << std::endl;

                model3->setColumn(lineno, values);
            }

            ++lineno;
            if (timingType == CSVFormat::ImplicitTiming ||
                list.size() == 0) {
                frameNo += windowSize;
            }
        }
    }

    if (!haveAnyValue) {
        if (model2a) {
            // assign values for regions based on label frequency; we
            // have this in our labelCountMap, sort of

            std::map<int, std::map<QString, float> > countLabelValueMap;
            for (std::map<QString, int>::iterator i = labelCountMap.begin();
                 i != labelCountMap.end(); ++i) {
                countLabelValueMap[i->second][i->first] = 0.f;
            }

            float v = 0.f;
            for (std::map<int, std::map<QString, float> >::iterator i =
                     countLabelValueMap.end(); i != countLabelValueMap.begin(); ) {
                --i;
                for (std::map<QString, float>::iterator j = i->second.begin();
                     j != i->second.end(); ++j) {
                    j->second = v;
                    v = v + 1.f;
                }
            }

            std::map<RegionModel::Point, RegionModel::Point,
                RegionModel::Point::Comparator> pointMap;
            for (RegionModel::PointList::const_iterator i =
                     model2a->getPoints().begin();
                 i != model2a->getPoints().end(); ++i) {
                RegionModel::Point p(*i);
                v = countLabelValueMap[labelCountMap[p.label]][p.label];
                RegionModel::Point pp(p.frame, v, p.duration, p.label);
                pointMap[p] = pp;
            }

            for (std::map<RegionModel::Point, RegionModel::Point>::iterator i = 
                     pointMap.begin(); i != pointMap.end(); ++i) {
                model2a->deletePoint(i->first);
                model2a->addPoint(i->second);
            }
        }
    }
                
    if (modelType == CSVFormat::ThreeDimensionalModel) {
	model3->setMinimumLevel(min);
	model3->setMaximumLevel(max);
    }

    return model;
}

