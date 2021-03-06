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

#ifndef _SPARSE_MODEL_H_
#define _SPARSE_MODEL_H_

#include "Model.h"
#include "TabularModel.h"
#include "base/Command.h"
#include "base/RealTime.h"

#include <iostream>

#include <set>
#include <vector>
#include <algorithm>
#include <iterator>

#include <cmath>

#include <QMutex>
#include <QTextStream>

/**
 * Model containing sparse data (points with some properties).  The
 * properties depend on the point type.
 */

template <typename PointType>
class SparseModel : public Model,
                    public TabularModel
{
public:
    SparseModel(size_t sampleRate, size_t resolution,
		bool notifyOnAdd = true);
    virtual ~SparseModel() { }
    
    virtual bool isOK() const { return true; }
    virtual size_t getStartFrame() const;
    virtual size_t getEndFrame() const;
    virtual size_t getSampleRate() const { return m_sampleRate; }

    virtual Model *clone() const;

    // Number of frames of the underlying sample rate that this model
    // is capable of resolving to.  For example, if m_resolution == 10
    // then every point in this model will be at a multiple of 10
    // sample frames and should be considered to cover a window ending
    // 10 sample frames later.
    virtual size_t getResolution() const {
        return m_resolution ? m_resolution : 1;
    }
    virtual void setResolution(size_t resolution);

    typedef PointType Point;
    typedef std::multiset<PointType,
			  typename PointType::OrderComparator> PointList;
    typedef typename PointList::iterator PointListIterator;
    typedef typename PointList::const_iterator PointListConstIterator;

    /**
     * Return whether the model is empty or not.
     */
    virtual bool isEmpty() const;

    /**
     * Get the total number of points in the model.
     */
    virtual size_t getPointCount() const;

    /**
     * Get all points.
     */
    virtual const PointList &getPoints() const;

    /**
     * Get all of the points in this model between the given
     * boundaries (in frames), as well as up to two points before and
     * after the boundaries.  If you need exact boundaries, check the
     * point coordinates in the returned list.
     */
    virtual PointList getPoints(long start, long end) const;

    /**
     * Get all points that cover the given frame number, taking the
     * resolution of the model into account.
     */
    virtual PointList getPoints(long frame) const;

    /**
     * Return all points that share the nearest frame number prior to
     * the given one at which there are any points.
     */
    virtual PointList getPreviousPoints(long frame) const;

    /**
     * Return all points that share the nearest frame number
     * subsequent to the given one at which there are any points.
     */
    virtual PointList getNextPoints(long frame) const;

    /**
     * Remove all points.
     */
    virtual void clear();

    /**
     * Add a point.
     */
    virtual void addPoint(const PointType &point);

    /** 
     * Remove a point.  Points are not necessarily unique, so this
     * function will remove the first point that compares equal to the
     * supplied one using Point::Comparator.  Other identical points
     * may remain in the model.
     */
    virtual void deletePoint(const PointType &point);

    virtual bool isReady(int *completion = 0) const {
        bool ready = isOK() && (m_completion == 100);
        if (completion) *completion = m_completion;
        return ready;
    }

    virtual void setCompletion(int completion, bool update = true);
    virtual int getCompletion() const { return m_completion; }

    virtual bool hasTextLabels() const { return m_hasTextLabels; }

    QString getTypeName() const { return tr("Sparse"); }

    virtual QString getXmlOutputType() const { return "sparse"; }

    virtual void toXml(QTextStream &out,
                       QString indent = "",
                       QString extraAttributes = "") const;

    virtual QString toDelimitedDataString(QString delimiter) const
    { 
        QString s;
        for (PointListConstIterator i = m_points.begin(); i != m_points.end(); ++i) {
            s += i->toDelimitedDataString(delimiter, m_sampleRate) + "\n";
        }
        return s;
    }

    /**
     * Command to add a point, with undo.
     */
    class AddPointCommand : public Command
    {
    public:
	AddPointCommand(SparseModel<PointType> *model,
			const PointType &point,
                        QString name = "") :
	    m_model(model), m_point(point), m_name(name) { }

	virtual QString getName() const {
            return (m_name == "" ? tr("Add Point") : m_name);
        }

	virtual void execute() { m_model->addPoint(m_point); }
	virtual void unexecute() { m_model->deletePoint(m_point); }

	const PointType &getPoint() const { return m_point; }

    private:
	SparseModel<PointType> *m_model;
	PointType m_point;
        QString m_name;
    };


    /**
     * Command to remove a point, with undo.
     */
    class DeletePointCommand : public Command
    {
    public:
	DeletePointCommand(SparseModel<PointType> *model,
			   const PointType &point) :
	    m_model(model), m_point(point) { }

	virtual QString getName() const { return tr("Delete Point"); }

	virtual void execute() { m_model->deletePoint(m_point); }
	virtual void unexecute() { m_model->addPoint(m_point); }

	const PointType &getPoint() const { return m_point; }

    private:
	SparseModel<PointType> *m_model;
	PointType m_point;
    };

    
    /**
     * Command to add or remove a series of points, with undo.
     * Consecutive add/remove pairs for the same point are collapsed.
     */
    class EditCommand : public MacroCommand
    {
    public:
	EditCommand(SparseModel<PointType> *model, QString commandName);

	virtual void addPoint(const PointType &point);
	virtual void deletePoint(const PointType &point);

	/**
	 * Stack an arbitrary other command in the same sequence.
	 */
	virtual void addCommand(Command *command) { addCommand(command, true); }

	/**
	 * If any points have been added or deleted, return this
	 * command (so the caller can add it to the command history).
	 * Otherwise delete the command and return NULL.
	 */
	virtual EditCommand *finish();

    protected:
	virtual void addCommand(Command *command, bool executeFirst);

	SparseModel<PointType> *m_model;
    };


    /**
     * Command to relabel a point.
     */
    class RelabelCommand : public Command
    {
    public:
	RelabelCommand(SparseModel<PointType> *model,
		       const PointType &point,
		       QString newLabel) :
	    m_model(model), m_oldPoint(point), m_newPoint(point) {
	    m_newPoint.label = newLabel;
	}

	virtual QString getName() const { return tr("Re-Label Point"); }

	virtual void execute() { 
	    m_model->deletePoint(m_oldPoint);
	    m_model->addPoint(m_newPoint);
	    std::swap(m_oldPoint, m_newPoint);
	}

	virtual void unexecute() { execute(); }

    private:
	SparseModel<PointType> *m_model;
	PointType m_oldPoint;
	PointType m_newPoint;
    };

    /**
     * TabularModel methods.  
     */

    virtual int getRowCount() const
    {
        return m_points.size();
    }

    virtual long getFrameForRow(int row) const
    {
        PointListConstIterator i = getPointListIteratorForRow(row);
        if (i == m_points.end()) return 0;
        return i->frame;
    }

    virtual int getRowForFrame(long frame) const
    {
        if (m_rows.empty()) rebuildRowVector();
        std::vector<long>::iterator i =
            std::lower_bound(m_rows.begin(), m_rows.end(), frame);
#if defined(__SUNPRO_CC) && defined(__STD_RW_ITERATOR__)
        int row = 0;
        std::distance(m_rows.begin(), i, row);
#else
        int row = std::distance(m_rows.begin(), i);
#endif
        if (i != m_rows.begin() && (i == m_rows.end() || *i != frame)) {
            --row;
        }
        return row;
    }

    virtual int getColumnCount() const { return 1; }
    virtual QVariant getData(int row, int column, int role) const
    {
        PointListConstIterator i = getPointListIteratorForRow(row);
        if (i == m_points.end()) return QVariant();

        switch (column) {
        case 0: {
            if (role == SortRole) return int(i->frame);
            RealTime rt = RealTime::frame2RealTime(i->frame, getSampleRate());
            if (role == Qt::EditRole) return rt.toString().c_str();
            else return rt.toText().c_str();
        }
        case 1: return int(i->frame);
        }

        return QVariant();
    }

    virtual Command *getSetDataCommand(int row, int column,
                                       const QVariant &value, int role)
    {
        if (role != Qt::EditRole) return false;
        PointListIterator i = getPointListIteratorForRow(row);
        if (i == m_points.end()) return false;
        EditCommand *command = new EditCommand(this, tr("Edit Data"));

        Point point(*i);
        command->deletePoint(point);

        switch (column) {
        case 0: point.frame = lrint(value.toDouble() * getSampleRate()); break;
        case 1: point.frame = value.toInt(); break; 
        }

        command->addPoint(point);
        return command->finish();
    }

    virtual Command *getInsertRowCommand(int row)
    {
        EditCommand *command = new EditCommand(this, tr("Insert Data Point"));
        Point point(0);
        PointListIterator i = getPointListIteratorForRow(row);
        if (i == m_points.end() && i != m_points.begin()) --i;
        if (i != m_points.end()) point = *i;
        command->addPoint(point);
        return command->finish();
    }
            
    virtual Command *getRemoveRowCommand(int row)
    {
        EditCommand *command = new EditCommand(this, tr("Delete Data Point"));
        PointListIterator i = getPointListIteratorForRow(row);
        if (i == m_points.end()) return 0;
        command->deletePoint(*i);
        return command->finish();
    }
            
protected:
    size_t m_sampleRate;
    size_t m_resolution;
    bool m_notifyOnAdd;
    long m_sinceLastNotifyMin;
    long m_sinceLastNotifyMax;
    bool m_hasTextLabels;

    PointList m_points;
    size_t m_pointCount;
    mutable QMutex m_mutex;
    int m_completion;

    void getPointIterators(long frame,
                           PointListIterator &startItr,
                           PointListIterator &endItr);
    void getPointIterators(long frame,
                           PointListConstIterator &startItr,
                           PointListConstIterator &endItr) const;

    // This is only used if the model is called on to act in
    // TabularModel mode
    mutable std::vector<long> m_rows; // map from row number to frame
    void rebuildRowVector() const
    {
        m_rows.clear();
        for (PointListConstIterator i = m_points.begin(); i != m_points.end(); ++i) {
            m_rows.push_back(i->frame);
        }
    }

    PointListIterator getPointListIteratorForRow(int row)
    {
        if (m_rows.empty()) rebuildRowVector();
        if (row < 0 || row + 1 > int(m_rows.size())) return m_points.end();

        size_t frame = m_rows[row];
        int indexAtFrame = 0;
        int ri = row;
        while (ri > 0 && m_rows[ri-1] == m_rows[row]) { --ri; ++indexAtFrame; }
        int initialIndexAtFrame = indexAtFrame;

        PointListIterator i0, i1;
        getPointIterators(frame, i0, i1);
        PointListIterator i = i0;

        for (i = i0; i != i1; ++i) {
            if (indexAtFrame > 0) { --indexAtFrame; continue; }
            return i;
        }

        if (indexAtFrame > 0) {
            std::cerr << "WARNING: SparseModel::getPointListIteratorForRow: No iterator available for row " << row << " (frame = " << frame << ", index at frame = " << initialIndexAtFrame << ", leftover index " << indexAtFrame << ")" << std::endl;
        }
        return i;
    }

    PointListConstIterator getPointListIteratorForRow(int row) const
    {
        if (m_rows.empty()) rebuildRowVector();
        if (row < 0 || row + 1 > int(m_rows.size())) return m_points.end();

        size_t frame = m_rows[row];
        int indexAtFrame = 0;
        int ri = row;
        while (ri > 0 && m_rows[ri-1] == m_rows[row]) { --ri; ++indexAtFrame; }
        int initialIndexAtFrame = indexAtFrame;

        PointListConstIterator i0, i1;
        getPointIterators(frame, i0, i1);
        PointListConstIterator i = i0;

        for (i = i0; i != i1; ++i) {
            if (indexAtFrame > 0) { --indexAtFrame; continue; }
            return i;
        }

        if (indexAtFrame > 0) {
            std::cerr << "WARNING: SparseModel::getPointListIteratorForRow: No iterator available for row " << row << " (frame = " << frame << ", index at frame = " << initialIndexAtFrame << ", leftover index " << indexAtFrame << ")" << std::endl;
        }
        return i;
    }
};


template <typename PointType>
SparseModel<PointType>::SparseModel(size_t sampleRate,
                                    size_t resolution,
                                    bool notifyOnAdd) :
    m_sampleRate(sampleRate),
    m_resolution(resolution),
    m_notifyOnAdd(notifyOnAdd),
    m_sinceLastNotifyMin(-1),
    m_sinceLastNotifyMax(-1),
    m_hasTextLabels(false),
    m_pointCount(0),
    m_completion(100)
{
}

template <typename PointType>
size_t
SparseModel<PointType>::getStartFrame() const
{
    QMutexLocker locker(&m_mutex);
    size_t f = 0;
    if (!m_points.empty()) {
	f = m_points.begin()->frame;
    }
    return f;
}

template <typename PointType>
size_t
SparseModel<PointType>::getEndFrame() const
{
    QMutexLocker locker(&m_mutex);
    size_t f = 0;
    if (!m_points.empty()) {
	PointListConstIterator i(m_points.end());
	f = (--i)->frame;
    }
    return f;
}

template <typename PointType>
Model *
SparseModel<PointType>::clone() const
{
    return 0; //!!! is this ever used?
/*
    SparseModel<PointType> *model =
	new SparseModel<PointType>(m_sampleRate, m_resolution, m_notifyOnAdd);
    model->m_points = m_points;
    model->m_pointCount = m_pointCount;
    return model;
*/
}

template <typename PointType>
bool
SparseModel<PointType>::isEmpty() const
{
    return m_pointCount == 0;
}

template <typename PointType>
size_t
SparseModel<PointType>::getPointCount() const
{
    return m_pointCount;
}

template <typename PointType>
const typename SparseModel<PointType>::PointList &
SparseModel<PointType>::getPoints() const
{
    return m_points;
}

template <typename PointType>
typename SparseModel<PointType>::PointList
SparseModel<PointType>::getPoints(long start, long end) const
{
    if (start > end) return PointList();
    QMutexLocker locker(&m_mutex);

    PointType startPoint(start), endPoint(end);
    
    PointListConstIterator startItr = m_points.lower_bound(startPoint);
    PointListConstIterator   endItr = m_points.upper_bound(endPoint);

    if (startItr != m_points.begin()) --startItr;
    if (startItr != m_points.begin()) --startItr;
    if (endItr != m_points.end()) ++endItr;
    if (endItr != m_points.end()) ++endItr;

    PointList rv;

    for (PointListConstIterator i = startItr; i != endItr; ++i) {
	rv.insert(*i);
    }

    return rv;
}

template <typename PointType>
typename SparseModel<PointType>::PointList
SparseModel<PointType>::getPoints(long frame) const
{
    PointListConstIterator startItr, endItr;
    getPointIterators(frame, startItr, endItr);

    PointList rv;

    for (PointListConstIterator i = startItr; i != endItr; ++i) {
	rv.insert(*i);
    }

    return rv;
}

template <typename PointType>
void
SparseModel<PointType>::getPointIterators(long frame,
                                          PointListIterator &startItr,
                                          PointListIterator &endItr)
{
    QMutexLocker locker(&m_mutex);

    if (m_resolution == 0) {
        startItr = m_points.end();
        endItr = m_points.end();
        return;
    }

    long start = (frame / m_resolution) * m_resolution;
    long end = start + m_resolution;

    PointType startPoint(start), endPoint(end);
    
    startItr = m_points.lower_bound(startPoint);
      endItr = m_points.upper_bound(endPoint);
}

template <typename PointType>
void
SparseModel<PointType>::getPointIterators(long frame,
                                          PointListConstIterator &startItr,
                                          PointListConstIterator &endItr) const
{
    QMutexLocker locker(&m_mutex);

    if (m_resolution == 0) {
        startItr = m_points.end();
        endItr = m_points.end();
        return;
    }

    long start = (frame / m_resolution) * m_resolution;
    long end = start + m_resolution;

    PointType startPoint(start), endPoint(end);
    
    startItr = m_points.lower_bound(startPoint);
      endItr = m_points.upper_bound(endPoint);
}

template <typename PointType>
typename SparseModel<PointType>::PointList
SparseModel<PointType>::getPreviousPoints(long originFrame) const
{
    QMutexLocker locker(&m_mutex);

    PointType lookupPoint(originFrame);
    PointList rv;

    PointListConstIterator i = m_points.lower_bound(lookupPoint);
    if (i == m_points.begin()) return rv;

    --i;
    long frame = i->frame;
    while (i->frame == frame) {
	rv.insert(*i);
	if (i == m_points.begin()) break;
	--i;
    }

    return rv;
}
 
template <typename PointType>
typename SparseModel<PointType>::PointList
SparseModel<PointType>::getNextPoints(long originFrame) const
{
    QMutexLocker locker(&m_mutex);

    PointType lookupPoint(originFrame);
    PointList rv;

    PointListConstIterator i = m_points.upper_bound(lookupPoint);
    if (i == m_points.end()) return rv;

    long frame = i->frame;
    while (i != m_points.end() && i->frame == frame) {
	rv.insert(*i);
	++i;
    }

    return rv;
}

template <typename PointType>
void
SparseModel<PointType>::setResolution(size_t resolution)
{
    {
	QMutexLocker locker(&m_mutex);
	m_resolution = resolution;
    }
    m_rows.clear();
    emit modelChanged();
}

template <typename PointType>
void
SparseModel<PointType>::clear()
{
    {
	QMutexLocker locker(&m_mutex);
	m_points.clear();
        m_pointCount = 0;
    }
    m_rows.clear();
    emit modelChanged();
}

template <typename PointType>
void
SparseModel<PointType>::addPoint(const PointType &point)
{
    {
	QMutexLocker locker(&m_mutex);
	m_points.insert(point);
        m_pointCount++;
        if (point.getLabel() != "") m_hasTextLabels = true;
    }

    // Even though this model is nominally sparse, there may still be
    // too many signals going on here (especially as they'll probably
    // be queued from one thread to another), which is why we need the
    // notifyOnAdd as an option rather than a necessity (the
    // alternative is to notify on setCompletion).

    if (m_notifyOnAdd) {
        m_rows.clear(); //!!! inefficient
	emit modelChanged(point.frame, point.frame + m_resolution);
    } else {
	if (m_sinceLastNotifyMin == -1 ||
	    point.frame < m_sinceLastNotifyMin) {
	    m_sinceLastNotifyMin = point.frame;
	}
	if (m_sinceLastNotifyMax == -1 ||
	    point.frame > m_sinceLastNotifyMax) {
	    m_sinceLastNotifyMax = point.frame;
	}
    }
}

template <typename PointType>
void
SparseModel<PointType>::deletePoint(const PointType &point)
{
    {
	QMutexLocker locker(&m_mutex);

	PointListIterator i = m_points.lower_bound(point);
	typename PointType::Comparator comparator;
	while (i != m_points.end()) {
	    if (i->frame > point.frame) break;
	    if (!comparator(*i, point) && !comparator(point, *i)) {
		m_points.erase(i);
                m_pointCount--;
		break;
	    }
	    ++i;
	}
    }
//    std::cout << "SparseOneDimensionalModel: emit modelChanged("
//	      << point.frame << ")" << std::endl;
    m_rows.clear(); //!!! inefficient
    emit modelChanged(point.frame, point.frame + m_resolution);
}

template <typename PointType>
void
SparseModel<PointType>::setCompletion(int completion, bool update)
{
//    std::cerr << "SparseModel::setCompletion(" << completion << ")" << std::endl;

    if (m_completion != completion) {
	m_completion = completion;

	if (completion == 100) {

            if (!m_notifyOnAdd) {
                emit completionChanged();
            }

	    m_notifyOnAdd = true; // henceforth
            m_rows.clear(); //!!! inefficient
	    emit modelChanged();

	} else if (!m_notifyOnAdd) {

	    if (update &&
                m_sinceLastNotifyMin >= 0 &&
		m_sinceLastNotifyMax >= 0) {
                m_rows.clear(); //!!! inefficient
		emit modelChanged(m_sinceLastNotifyMin, m_sinceLastNotifyMax);
		m_sinceLastNotifyMin = m_sinceLastNotifyMax = -1;
	    } else {
		emit completionChanged();
	    }
	} else {
	    emit completionChanged();
	}	    
    }
}

template <typename PointType>
void
SparseModel<PointType>::toXml(QTextStream &out,
                              QString indent,
                              QString extraAttributes) const
{
    std::cerr << "SparseModel::toXml: extraAttributes = \"" 
              << extraAttributes.toStdString() << std::endl;

    QString type = getXmlOutputType();

    Model::toXml
	(out,
         indent,
	 QString("type=\"%1\" dimensions=\"%2\" resolution=\"%3\" notifyOnAdd=\"%4\" dataset=\"%5\" %6")
         .arg(type)
	 .arg(PointType(0).getDimensions())
	 .arg(m_resolution)
	 .arg(m_notifyOnAdd ? "true" : "false")
	 .arg(getObjectExportId(&m_points))
	 .arg(extraAttributes));

    out << indent;
    out << QString("<dataset id=\"%1\" dimensions=\"%2\">\n")
	.arg(getObjectExportId(&m_points))
	.arg(PointType(0).getDimensions());

    for (PointListConstIterator i = m_points.begin(); i != m_points.end(); ++i) {
        i->toXml(out, indent + "  ");
    }

    out << indent;
    out << "</dataset>\n";
}

template <typename PointType>
SparseModel<PointType>::EditCommand::EditCommand(SparseModel *model,
                                                 QString commandName) :
    MacroCommand(commandName),
    m_model(model)
{
}

template <typename PointType>
void
SparseModel<PointType>::EditCommand::addPoint(const PointType &point)
{
    addCommand(new AddPointCommand(m_model, point), true);
}

template <typename PointType>
void
SparseModel<PointType>::EditCommand::deletePoint(const PointType &point)
{
    addCommand(new DeletePointCommand(m_model, point), true);
}

template <typename PointType>
typename SparseModel<PointType>::EditCommand *
SparseModel<PointType>::EditCommand::finish()
{
    if (!m_commands.empty()) {
        return this;
    } else {
        delete this;
        return 0;
    }
}

template <typename PointType>
void
SparseModel<PointType>::EditCommand::addCommand(Command *command,
						bool executeFirst)
{
    if (executeFirst) command->execute();

    if (!m_commands.empty()) {
	DeletePointCommand *dpc = dynamic_cast<DeletePointCommand *>(command);
	if (dpc) {
	    AddPointCommand *apc = dynamic_cast<AddPointCommand *>
		(m_commands[m_commands.size() - 1]);
	    typename PointType::Comparator comparator;
	    if (apc) {
		if (!comparator(apc->getPoint(), dpc->getPoint()) &&
		    !comparator(dpc->getPoint(), apc->getPoint())) {
		    deleteCommand(apc);
		    return;
		}
	    }
	}
    }

    MacroCommand::addCommand(command);
}


#endif


    
