TEMPLATE = lib

SV_UNIT_PACKAGES =
load(../prf/sv.prf)

CONFIG += sv staticlib qt thread warn_on stl rtti exceptions

QT -= gui

TARGET = svsystem

DEPENDPATH += .
INCLUDEPATH += .

# Input
HEADERS += Init.h System.h
SOURCES += Init.cpp System.cpp
