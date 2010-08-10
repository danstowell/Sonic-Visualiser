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

#ifndef _DENSE_THREE_DIMENSIONAL_MODEL_H_
#define _DENSE_THREE_DIMENSIONAL_MODEL_H_

#include "Model.h"
#include "TabularModel.h"
#include "base/ZoomConstraint.h"
#include "base/RealTime.h"

#include <QMutex>
#include <QVector>

class DenseThreeDimensionalModel : public Model,
                                   public TabularModel
{
    Q_OBJECT

public:
    /**
     * Return the number of sample frames covered by each column of bins.
     */
    virtual size_t getResolution() const = 0;

    /**
     * Return the number of columns of bins in the model.
     */
    virtual size_t getWidth() const = 0;

    /**
     * Return the number of bins in each column.
     */
    virtual size_t getHeight() const = 0; 

    /**
     * Return the minimum permissible value in each bin.
     */
    virtual float getMinimumLevel() const = 0;

    /**
     * Return the maximum permissible value in each bin.
     */
    virtual float getMaximumLevel() const = 0;

    /**
     * Return true if there are data available for the given column.
     * This should return true only if getColumn(column) would not
     * have to do any substantial work to calculate its return values.
     * If this function returns false, it may still be possible to
     * retrieve the column, but its values may have to be calculated.
     */
    virtual bool isColumnAvailable(size_t column) const = 0;

    typedef QVector<float> Column;

    /**
     * Get data from the given column of bin values.
     */
    virtual Column getColumn(size_t column) const = 0;

    /**
     * Get the single data point from the n'th bin of the given column.
     */
    virtual float getValueAt(size_t column, size_t n) const = 0;

    /**
     * Get the name of a given bin (i.e. a label to associate with
     * that bin across all columns).
     */
    virtual QString getBinName(size_t n) const = 0;

    /**
     * Estimate whether a logarithmic scale might be appropriate for
     * the value scale.
     */
    virtual bool shouldUseLogValueScale() const = 0;

    /**
     * Utility function to query whether a given bin is greater than
     * its (vertical) neighbours.
     */
    bool isLocalPeak(size_t x, size_t y) {
        float value = getValueAt(x, y);
        if (y > 0 && value < getValueAt(x, y - 1)) return false;
        if (y < getHeight() - 1 && value < getValueAt(x, y + 1)) return false;
        return true;
    }

    /**
     * Utility function to query whether a given bin is greater than a
     * certain threshold.
     */
    bool isOverThreshold(size_t x, size_t y, float threshold) {
        return getValueAt(x, y) > threshold;
    }

    QString getTypeName() const { return tr("Dense 3-D"); }

    virtual int getCompletion() const = 0;

    /*
       TabularModel methods.
       This class is non-editable -- subclasses may be editable.
       Row and column are transposed for the tabular view (which is
       "on its side").
     */
    
    virtual int getRowCount() const { return getWidth(); }
    virtual int getColumnCount() const { return getHeight() + 2; }

    virtual QString getHeading(int column) const
    {
        switch (column) {
        case 0: return tr("Time");
        case 1: return tr("Frame");
        default: return getBinName(column - 2);
        }
    }

    virtual QVariant getData(int row, int column, int) const 
    {
        switch (column) {
        case 0: {
            RealTime rt = RealTime::frame2RealTime(row * getResolution(),
                                                   getSampleRate());
            return rt.toText().c_str();
        }
        case 1:
            return int(row * getResolution());
        default:
            return getValueAt(row, column - 2);
        }
    }

    virtual bool isColumnTimeValue(int col) const {
        return col < 2;
    }
    virtual SortType getSortType(int) const {
        return SortNumeric;
    }

    virtual long getFrameForRow(int row) const {
        return row * getSampleRate();
    }
    virtual int getRowForFrame(long frame) const {
        return frame / getSampleRate();
    }

protected:
    DenseThreeDimensionalModel() { }
};

#endif
