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

#ifndef _NOTE_MODEL_H_
#define _NOTE_MODEL_H_

#include "IntervalModel.h"
#include "base/RealTime.h"
#include "base/PlayParameterRepository.h"

/**
 * NoteModel -- a concrete IntervalModel for notes.
 */

/**
 * Note type for use in a sparse model.  All we mean by a "note" is
 * something that has an onset time, a single value, a duration, and a
 * level.  Like other points, it can also have a label.  With this
 * point type, the model can be thought of as representing a simple
 * MIDI-type piano roll, except that the y coordinates (values) do not
 * have to be discrete integers.
 */

struct Note
{
public:
    Note(long _frame) : frame(_frame), value(0.0f), duration(0), level(1.f) { }
    Note(long _frame, float _value, size_t _duration, float _level, QString _label) :
	frame(_frame), value(_value), duration(_duration), level(_level), label(_label) { }

    int getDimensions() const { return 3; }

    long frame;
    float value;
    size_t duration;
    float level;
    QString label;

    QString getLabel() const { return label; }
    
    void toXml(QTextStream &stream,
               QString indent = "",
               QString extraAttributes = "") const
    {
	stream <<
            QString("%1<point frame=\"%2\" value=\"%3\" duration=\"%4\" level=\"%5\" label=\"%6\" %7/>\n")
	    .arg(indent).arg(frame).arg(value).arg(duration).arg(level)
            .arg(XmlExportable::encodeEntities(label)).arg(extraAttributes);
    }

    QString toDelimitedDataString(QString delimiter, size_t sampleRate) const
    {
        QStringList list;
        list << RealTime::frame2RealTime(frame, sampleRate).toString().c_str();
        list << QString("%1").arg(value);
        list << RealTime::frame2RealTime(duration, sampleRate).toString().c_str();
        list << QString("%1").arg(level);
        if (label != "") list << label;
        return list.join(delimiter);
    }

    struct Comparator {
	bool operator()(const Note &p1,
			const Note &p2) const {
	    if (p1.frame != p2.frame) return p1.frame < p2.frame;
	    if (p1.value != p2.value) return p1.value < p2.value;
	    if (p1.duration != p2.duration) return p1.duration < p2.duration;
            if (p1.level != p2.level) return p1.level < p2.level;
	    return p1.label < p2.label;
	}
    };
    
    struct OrderComparator {
	bool operator()(const Note &p1,
			const Note &p2) const {
	    return p1.frame < p2.frame;
	}
    };
};


class NoteModel : public IntervalModel<Note>
{
    Q_OBJECT
    
public:
    NoteModel(size_t sampleRate, size_t resolution,
	      bool notifyOnAdd = true) :
	IntervalModel<Note>(sampleRate, resolution, notifyOnAdd),
	m_valueQuantization(0)
    {
	PlayParameterRepository::getInstance()->addPlayable(this);
    }

    NoteModel(size_t sampleRate, size_t resolution,
	      float valueMinimum, float valueMaximum,
	      bool notifyOnAdd = true) :
	IntervalModel<Note>(sampleRate, resolution,
                            valueMinimum, valueMaximum,
                            notifyOnAdd),
	m_valueQuantization(0)
    {
	PlayParameterRepository::getInstance()->addPlayable(this);
    }

    virtual ~NoteModel()
    {
        PlayParameterRepository::getInstance()->removePlayable(this);
    }

    float getValueQuantization() const { return m_valueQuantization; }
    void setValueQuantization(float q) { m_valueQuantization = q; }

    QString getTypeName() const { return tr("Note"); }

    virtual bool canPlay() const { return true; }

    virtual QString getDefaultPlayPluginId() const
    {
        return "dssi:_builtin:sample_player";
    }

    virtual QString getDefaultPlayPluginConfiguration() const
    {
        return "<plugin program=\"piano\"/>";
    }

    virtual void toXml(QTextStream &out,
                       QString indent = "",
                       QString extraAttributes = "") const
    {
        std::cerr << "NoteModel::toXml: extraAttributes = \"" 
                  << extraAttributes.toStdString() << std::endl;

        IntervalModel<Note>::toXml
	    (out,
             indent,
	     QString("%1 subtype=\"note\" valueQuantization=\"%2\"")
	     .arg(extraAttributes).arg(m_valueQuantization));
    }

    /**
     * TabularModel methods.  
     */
    
    virtual int getColumnCount() const
    {
        return 6;
    }

    virtual QString getHeading(int column) const
    {
        switch (column) {
        case 0: return tr("Time");
        case 1: return tr("Frame");
        case 2: return tr("Pitch");
        case 3: return tr("Duration");
        case 4: return tr("Level");
        case 5: return tr("Label");
        default: return tr("Unknown");
        }
    }

    virtual QVariant getData(int row, int column, int role) const
    {
        if (column < 4) {
            return IntervalModel<Note>::getData(row, column, role);
        }

        PointListConstIterator i = getPointListIteratorForRow(row);
        if (i == m_points.end()) return QVariant();

        switch (column) {
        case 4: return i->level;
        case 5: return i->label;
        default: return QVariant();
        }
    }

    virtual Command *getSetDataCommand(int row, int column, const QVariant &value, int role)
    {
        if (column < 4) {
            return IntervalModel<Note>::getSetDataCommand
                (row, column, value, role);
        }

        if (role != Qt::EditRole) return false;
        PointListConstIterator i = getPointListIteratorForRow(row);
        if (i == m_points.end()) return false;
        EditCommand *command = new EditCommand(this, tr("Edit Data"));

        Point point(*i);
        command->deletePoint(point);

        switch (column) {
        case 4: point.level = value.toDouble(); break;
        case 5: point.label = value.toString(); break;
        }

        command->addPoint(point);
        return command->finish();
    }

    virtual SortType getSortType(int column) const
    {
        if (column == 5) return SortAlphabetical;
        return SortNumeric;
    }

protected:
    float m_valueQuantization;
};

#endif
