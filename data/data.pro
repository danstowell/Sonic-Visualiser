TEMPLATE = lib

SV_UNIT_PACKAGES = fftw3f sndfile mad quicktime id3tag oggz fishsound liblo
load(../prf/sv.prf)

CONFIG += sv staticlib qt thread warn_on stl rtti exceptions
QT += network
QT -= gui

TARGET = svdata

DEPENDPATH += fft fileio model osc ..
INCLUDEPATH += . fft fileio model osc ..
OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc

# Set up suitable platform defines for RtMidi
linux*:   DEFINES += __LINUX_ALSASEQ__
macx*:    DEFINES += __MACOSX_CORE__
win*:     DEFINES += __WINDOWS_MM__
solaris*: DEFINES += __RTMIDI_DUMMY_ONLY__

# Input
HEADERS += fft/FFTapi.h \
           fft/FFTCacheReader.h \
           fft/FFTCacheStorageType.h \
           fft/FFTCacheWriter.h \
           fft/FFTDataServer.h \
           fft/FFTFileCacheReader.h \
           fft/FFTFileCacheWriter.h \
           fft/FFTMemoryCache.h \
           fileio/AudioFileReader.h \
           fileio/AudioFileReaderFactory.h \
           fileio/BZipFileDevice.h \
           fileio/CachedFile.h \
           fileio/CodedAudioFileReader.h \
           fileio/CSVFileReader.h \
           fileio/CSVFileWriter.h \
           fileio/CSVFormat.h \
           fileio/DataFileReader.h \
           fileio/DataFileReaderFactory.h \
           fileio/FileFinder.h \
           fileio/FileReadThread.h \
           fileio/FileSource.h \
           fileio/MatchFileReader.h \
           fileio/MatrixFile.h \
           fileio/MIDIFileReader.h \
           fileio/MIDIFileWriter.h \
           fileio/MP3FileReader.h \
           fileio/OggVorbisFileReader.h \
           fileio/PlaylistFileReader.h \
           fileio/QuickTimeFileReader.h \
           fileio/ResamplingWavFileReader.h \
           fileio/WavFileReader.h \
           fileio/WavFileWriter.h \
           midi/MIDIEvent.h \
           midi/MIDIInput.h \
           midi/rtmidi/RtError.h \
           midi/rtmidi/RtMidi.h \
           model/AggregateWaveModel.h \
           model/AlignmentModel.h \
           model/Dense3DModelPeakCache.h \
           model/DenseThreeDimensionalModel.h \
           model/DenseTimeValueModel.h \
           model/EditableDenseThreeDimensionalModel.h \
           model/FFTModel.h \
           model/ImageModel.h \
           model/IntervalModel.h \
           model/Labeller.h \
           model/Model.h \
           model/ModelDataTableModel.h \
           model/NoteModel.h \
           model/PathModel.h \
           model/PowerOfSqrtTwoZoomConstraint.h \
           model/PowerOfTwoZoomConstraint.h \
           model/RangeSummarisableTimeValueModel.h \
           model/RegionModel.h \
           model/SparseModel.h \
           model/SparseOneDimensionalModel.h \
           model/SparseTimeValueModel.h \
           model/SparseValueModel.h \
           model/TabularModel.h \
           model/TextModel.h \
           model/WaveFileModel.h \
           model/WritableWaveFileModel.h \
           osc/OSCMessage.h \
           osc/OSCQueue.h 
SOURCES += fft/FFTapi.cpp \
           fft/FFTDataServer.cpp \
           fft/FFTFileCacheReader.cpp \
           fft/FFTFileCacheWriter.cpp \
           fft/FFTMemoryCache.cpp \
           fileio/AudioFileReader.cpp \
           fileio/AudioFileReaderFactory.cpp \
           fileio/BZipFileDevice.cpp \
           fileio/CachedFile.cpp \
           fileio/CodedAudioFileReader.cpp \
           fileio/CSVFileReader.cpp \
           fileio/CSVFileWriter.cpp \
           fileio/CSVFormat.cpp \
           fileio/DataFileReaderFactory.cpp \
           fileio/FileReadThread.cpp \
           fileio/FileSource.cpp \
           fileio/MatchFileReader.cpp \
           fileio/MatrixFile.cpp \
           fileio/MIDIFileReader.cpp \
           fileio/MIDIFileWriter.cpp \
           fileio/MP3FileReader.cpp \
           fileio/OggVorbisFileReader.cpp \
           fileio/PlaylistFileReader.cpp \
           fileio/QuickTimeFileReader.cpp \
           fileio/ResamplingWavFileReader.cpp \
           fileio/WavFileReader.cpp \
           fileio/WavFileWriter.cpp \
           midi/MIDIInput.cpp \
           midi/rtmidi/RtMidi.cpp \
           model/AggregateWaveModel.cpp \
           model/AlignmentModel.cpp \
           model/Dense3DModelPeakCache.cpp \
           model/DenseTimeValueModel.cpp \
           model/EditableDenseThreeDimensionalModel.cpp \
           model/FFTModel.cpp \
           model/Model.cpp \
           model/ModelDataTableModel.cpp \
           model/PowerOfSqrtTwoZoomConstraint.cpp \
           model/PowerOfTwoZoomConstraint.cpp \
           model/RangeSummarisableTimeValueModel.cpp \
           model/WaveFileModel.cpp \
           model/WritableWaveFileModel.cpp \
           osc/OSCMessage.cpp \
           osc/OSCQueue.cpp 
