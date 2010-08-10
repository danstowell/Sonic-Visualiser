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

#include "PropertyContainer.h"
#include "RangeMapper.h"
#include "UnitDatabase.h"

#include <iostream>

PropertyContainer::PropertyList
PropertyContainer::getProperties() const
{
    return PropertyList();
}

PropertyContainer::PropertyType
PropertyContainer::getPropertyType(const PropertyName &) const
{
    return InvalidProperty;
}

QString
PropertyContainer::getPropertyIconName(const PropertyName &) const
{
    return QString();
}

QString
PropertyContainer::getPropertyGroupName(const PropertyName &) const
{
    return QString();
}

int
PropertyContainer::getPropertyRangeAndValue(const PropertyName &,
                                            int *min, int *max, int *deflt) const
{
    if (min) *min = 0;
    if (max) *max = 0;
    if (deflt) *deflt = 0;
    return 0;
}

QString
PropertyContainer::getPropertyValueLabel(const PropertyName &name, int value) const
{
    return QString();
}

RangeMapper *
PropertyContainer::getNewPropertyRangeMapper(const PropertyName &) const
{
    return 0;
}

void
PropertyContainer::setProperty(const PropertyName &name, int) 
{
    std::cerr << "WARNING: PropertyContainer[" << getPropertyContainerName().toStdString() << "]::setProperty(" << name.toStdString() << "): no implementation in subclass!" << std::endl;
}

Command *
PropertyContainer::getSetPropertyCommand(const PropertyName &name, int value)
{
    int currentValue = getPropertyRangeAndValue(name, 0, 0, 0);
    if (value == currentValue) return 0;
    return new SetPropertyCommand(this, name, value);
}
 
void
PropertyContainer::setPropertyFuzzy(QString nameString, QString valueString)
{
    PropertyName name;
    int value;
    if (!convertPropertyStrings(nameString, valueString, name, value)) {
        std::cerr << "WARNING: PropertyContainer::setProperty(\""
                  << nameString.toStdString() << "\", \""
                  << valueString.toStdString()
                  << "\"): Name and value conversion failed" << std::endl;
        return;
    }
    setProperty(name, value);
}
 
Command *
PropertyContainer::getSetPropertyCommand(QString nameString, QString valueString)
{
    PropertyName name;
    int value;
    if (!convertPropertyStrings(nameString, valueString, name, value)) {
        std::cerr << "WARNING: PropertyContainer::getSetPropertyCommand(\""
                  << nameString.toStdString() << "\", \""
                  << valueString.toStdString()
                  << "\"): Name and value conversion failed" << std::endl;
        return 0;
    }
    return getSetPropertyCommand(name, value);
}

bool
PropertyContainer::convertPropertyStrings(QString nameString, QString valueString,
                                          PropertyName &name, int &value)
{
    PropertyList pl = getProperties();

    QString adjusted = nameString.trimmed();
    adjusted.replace('_', ' ');
    adjusted.replace('-', ' ');
    
    name = "";

    for (PropertyList::iterator pli = pl.begin(); pli != pl.end(); ++pli) {

        QString label = getPropertyLabel(*pli);

        if (label != "" && (nameString == label || adjusted == label)) {
            name = *pli;
            break;
        } else if (nameString == *pli) {
            name = *pli;
            break;
        }
    }

    if (name == "") {
        std::cerr << "PropertyContainer::convertPropertyStrings: Unable to match name string \"" << nameString.toStdString() << "\"" << std::endl;
        return false;
    }

    value = 0;
    bool success = false;
    
    bool isDouble = false;
    double dval = valueString.toDouble(&isDouble);

    switch (getPropertyType(name)) {

    case ToggleProperty:
        if (valueString == tr("yes") || 
            valueString == tr("on") ||
            valueString == tr("true")) {
            value = 1; success = true;
        } else if (valueString == tr("no") ||
                   valueString == tr("off") ||
                   valueString == tr("false")) {
            value = 0; success = true;
        }
        break;

    case RangeProperty:
        if (isDouble) {
            RangeMapper *mapper = getNewPropertyRangeMapper(name);
            if (mapper) {
                value = mapper->getPositionForValue(dval);
                delete mapper;
                success = true;
            }
        }
        break;

    case ValueProperty:
    case ColourProperty:
    {
        int min, max;
        getPropertyRangeAndValue(name, &min, &max, 0);
        for (int i = min; i <= max; ++i) {
            if (valueString == getPropertyValueLabel(name, i)) {
                value = i;
                success = true;
                break;
            }
        }
        break;
    }
        
    case UnitsProperty:
        value = UnitDatabase::getInstance()->getUnitId(valueString, false);
        if (value >= 0) success = true;
        else value = 0;
        break;

    case InvalidProperty:
        std::cerr << "PropertyContainer::convertPropertyStrings: Invalid property name \"" << name.toStdString() << "\"" << std::endl;
        return false;
    }

    if (success) return true;

    int min, max;
    getPropertyRangeAndValue(name, &min, &max, 0);
    
    bool ok = false;
    int i = valueString.toInt(&ok);
    if (!ok) {
        std::cerr << "PropertyContainer::convertPropertyStrings: Unable to parse value string \"" << valueString.toStdString() << "\"" << std::endl;
        return false;
    } else if (i < min || i > max) {
        std::cerr << "PropertyContainer::convertPropertyStrings: Property value \"" << i << "\" outside valid range " << min << " to " << max << std::endl;
        return false;
    }

    value = i;
    return true;
}

PropertyContainer::SetPropertyCommand::SetPropertyCommand(PropertyContainer *pc,
							  const PropertyName &pn,
							  int value) :
    m_pc(pc),
    m_pn(pn),
    m_value(value),
    m_oldValue(0)
{
}

void
PropertyContainer::SetPropertyCommand::execute()
{
    m_oldValue = m_pc->getPropertyRangeAndValue(m_pn, 0, 0, 0);
    m_pc->setProperty(m_pn, m_value);
}

void
PropertyContainer::SetPropertyCommand::unexecute() 
{
    m_pc->setProperty(m_pn, m_oldValue);
}

QString
PropertyContainer::SetPropertyCommand::getName() const
{
    return tr("Set %1 Property").arg(m_pn);
}

