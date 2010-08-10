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

#ifndef _SELECTION_H_
#define _SELECTION_H_

#include <cstddef>
#include <set>

#include "XmlExportable.h"

class Selection
{
public:
    Selection();
    Selection(size_t startFrame, size_t endFrame);
    Selection(const Selection &);
    Selection &operator=(const Selection &);
    virtual ~Selection();

    bool isEmpty() const;
    size_t getStartFrame() const;
    size_t getEndFrame() const;
    bool contains(size_t frame) const;

    bool operator<(const Selection &) const;
    bool operator==(const Selection &) const;
    
protected:
    size_t m_startFrame;
    size_t m_endFrame;
};

class MultiSelection : public XmlExportable
{
public:
    MultiSelection();
    virtual ~MultiSelection();

    typedef std::set<Selection> SelectionList;

    const SelectionList &getSelections() const;
    void setSelection(const Selection &selection);
    void addSelection(const Selection &selection);
    void removeSelection(const Selection &selection);
    void clearSelections();

    void getExtents(size_t &startFrame, size_t &endFrame) const;

    /**
     * Return the selection that contains a given frame.
     * If defaultToFollowing is true, and if the frame is not in a
     * selected area, return the next selection after the given frame.
     * Return the empty selection if no appropriate selection is found.
     */
    Selection getContainingSelection(size_t frame, bool defaultToFollowing) const;

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

protected:
    SelectionList m_selections;
};
    

#endif
