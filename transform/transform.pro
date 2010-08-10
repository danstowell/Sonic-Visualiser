TEMPLATE = lib

SV_UNIT_PACKAGES = vamp vamp-hostsdk
load(../prf/sv.prf)

CONFIG += sv staticlib qt thread warn_on stl rtti exceptions
QT += xml

TARGET = svtransform

DEPENDPATH += . .. 
INCLUDEPATH += . ..
OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc

# Input
HEADERS += CSVFeatureWriter.h \
           FeatureExtractionModelTransformer.h \
           FeatureWriter.h \
           FileFeatureWriter.h \
           RealTimeEffectModelTransformer.h \
           Transform.h \
           TransformDescription.h \
           TransformFactory.h \
           ModelTransformer.h \
           ModelTransformerFactory.h
SOURCES += CSVFeatureWriter.cpp \
           FeatureExtractionModelTransformer.cpp \
           FileFeatureWriter.cpp \
           RealTimeEffectModelTransformer.cpp \
           Transform.cpp \
           TransformFactory.cpp \
           ModelTransformer.cpp \
           ModelTransformerFactory.cpp
