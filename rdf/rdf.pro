TEMPLATE = lib

SV_UNIT_PACKAGES = redland rasqal raptor vamp-hostsdk
load(../prf/sv.prf)

CONFIG += sv staticlib qt thread warn_on stl rtti exceptions

TARGET = svrdf

DEPENDPATH += . .. 
INCLUDEPATH += . ..
OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc

# Input
HEADERS += PluginRDFDescription.h \
           PluginRDFIndexer.h \
           RDFExporter.h \
           RDFFeatureWriter.h \
           RDFImporter.h \
	   RDFTransformFactory.h \
           SimpleSPARQLQuery.h
SOURCES += PluginRDFDescription.cpp \
           PluginRDFIndexer.cpp \
           RDFExporter.cpp \
           RDFFeatureWriter.cpp \
           RDFImporter.cpp \
           RDFTransformFactory.cpp \
           SimpleSPARQLQuery.cpp

