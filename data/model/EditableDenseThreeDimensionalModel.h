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

#ifndef _EDITABLE_DENSE_THREE_DIMENSIONAL_MODEL_H_
#define _EDITABLE_DENSE_THREE_DIMENSIONAL_MODEL_H_

#include "DenseThreeDimensionalModel.h"

#include <QReadWriteLock>

#include <vector>

class EditableDenseThreeDimensionalModel : public DenseThreeDimensionalModel
{
    Q_OBJECT

public:

    // EditableDenseThreeDimensionalModel supports a basic compression
    // method that reduces the size of multirate data (e.g. wavelet
    // transform outputs) that are stored as plain 3d grids by about
    // 60% or thereabouts.  However, it can only be used for models
    // whose columns are set in order from 0 and never subsequently
    // changed.  If the model is going to be actually edited, it must
    // have NoCompression.

    enum CompressionType
    {
        NoCompression,
        BasicMultirateCompression
    };

    EditableDenseThreeDimensionalModel(size_t sampleRate,
				       size_t resolution,
				       size_t yBinCount,
                                       CompressionType compression,
				       bool notifyOnAdd = true);

    virtual bool isOK() const;

    virtual size_t getSampleRate() const;
    virtual size_t getStartFrame() const;
    virtual size_t getEndFrame() const;

    virtual Model *clone() const;
    

    /**
     * Set the frame offset of the first column.
     */
    virtual void setStartFrame(size_t);

    /**
     * Return the number of sample frames covered by each set of bins.
     */
    virtual size_t getResolution() const;

    /**
     * Set the number of sample frames covered by each set of bins.
     */
    virtual void setResolution(size_t sz);

    /**
     * Return the number of columns.
     */
    virtual size_t getWidth() const;

    /**
     * Return the number of bins in each set of bins.
     */
    virtual size_t getHeight() const; 

    /**
     * Set the number of bins in each set of bins.
     */
    virtual void setHeight(size_t sz);

    /**
     * Return the minimum value of the value in each bin.
     */
    virtual float getMinimumLevel() const;

    /**
     * Set the minimum value of the value in a bin.
     */
    virtual void setMinimumLevel(float sz);

    /**
     * Return the maximum value of the value in each bin.
     */
    virtual float getMaximumLevel() const;

    /**
     * Set the maximum value of the value in a bin.
     */
    virtual void setMaximumLevel(float sz);

    /**
     * Return true if there are data available for the given column.
     */
    virtual bool isColumnAvailable(size_t x) const { return x < getWidth(); }

    /**
     * Get the set of bin values at the given column.
     */
    virtual Column getColumn(size_t x) const;

    /**
     * Get a single value, from the n'th bin of the given column.
     */
    virtual float getValueAt(size_t x, size_t n) const;

    /**
     * Set the entire set of bin values at the given column.
     */
    virtual void setColumn(size_t x, const Column &values);

    virtual QString getBinName(size_t n) const;
    virtual void setBinName(size_t n, QString);
    virtual void setBinNames(std::vector<QString> names);

    bool shouldUseLogValueScale() const;

    virtual void setCompletion(int completion, bool update = true);
    virtual int getCompletion() const { return m_completion; }

    QString getTypeName() const { return tr("Editable Dense 3-D"); }

    virtual QString toDelimitedDataString(QString delimiter) const;

    virtual void toXml(QTextStream &out,
                       QString indent = "",
                       QString extraAttributes = "") const;

protected:
    typedef QVector<Column> ValueMatrix;
    ValueMatrix m_data;

    // m_trunc is used for simple compression.  If at least the top N
    // elements of column x (for N = some proportion of the column
    // height) are equal to those of an earlier column x', then
    // m_trunc[x] will contain x-x' and column x will be truncated so
    // as to remove the duplicate elements.  If the equal elements are
    // at the bottom, then m_trunc[x] will contain x'-x (a negative
    // value).  If m_trunc[x] is 0 then the whole of column x is
    // stored.
    std::vector<signed char> m_trunc;
    void truncateAndStore(size_t index, const Column & values);
    Column expandAndRetrieve(size_t index) const;

    std::vector<QString> m_binNames;

    size_t m_startFrame;
    size_t m_sampleRate;
    size_t m_resolution;
    size_t m_yBinCount;
    CompressionType m_compression;
    float m_minimum;
    float m_maximum;
    bool m_haveExtents;
    bool m_notifyOnAdd;
    long m_sinceLastNotifyMin;
    long m_sinceLastNotifyMax;
    int m_completion;

    mutable QReadWriteLock m_lock;
};

#endif
