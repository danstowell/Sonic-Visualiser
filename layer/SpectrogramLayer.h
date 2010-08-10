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

#ifndef _SPECTROGRAM_LAYER_H_
#define _SPECTROGRAM_LAYER_H_

#include "SliceableLayer.h"
#include "base/Window.h"
#include "base/RealTime.h"
#include "base/Thread.h"
#include "base/PropertyContainer.h"
#include "data/model/PowerOfSqrtTwoZoomConstraint.h"
#include "data/model/DenseTimeValueModel.h"
#include "data/model/FFTModel.h"

#include <QMutex>
#include <QWaitCondition>
#include <QImage>
#include <QPixmap>

class View;
class QPainter;
class QImage;
class QPixmap;
class QTimer;
class FFTModel;
class Dense3DModelPeakCache;


/**
 * SpectrogramLayer represents waveform data (obtained from a
 * DenseTimeValueModel) in spectrogram form.
 */

class SpectrogramLayer : public SliceableLayer,
			 public PowerOfSqrtTwoZoomConstraint
{
    Q_OBJECT

public:
    enum Configuration { FullRangeDb, MelodicRange, MelodicPeaks };
    
    SpectrogramLayer(Configuration = FullRangeDb);
    ~SpectrogramLayer();

    virtual const ZoomConstraint *getZoomConstraint() const { return this; }
    virtual const Model *getModel() const { return m_model; }
    virtual void paint(View *v, QPainter &paint, QRect rect) const;
    virtual void setSynchronousPainting(bool synchronous);

    virtual int getVerticalScaleWidth(View *v, QPainter &) const;
    virtual void paintVerticalScale(View *v, QPainter &paint, QRect rect) const;

    virtual bool getCrosshairExtents(View *, QPainter &, QPoint cursorPos,
                                     std::vector<QRect> &extents) const;
    virtual void paintCrosshairs(View *, QPainter &, QPoint) const;

    virtual QString getFeatureDescription(View *v, QPoint &) const;

    virtual bool snapToFeatureFrame(View *v, int &frame,
				    size_t &resolution,
				    SnapType snap) const;

    virtual void measureDoubleClick(View *, QMouseEvent *);

    virtual bool hasLightBackground() const;

    void setModel(const DenseTimeValueModel *model);

    virtual PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual QString getPropertyIconName(const PropertyName &) const;
    virtual PropertyType getPropertyType(const PropertyName &) const;
    virtual QString getPropertyGroupName(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual RangeMapper *getNewPropertyRangeMapper(const PropertyName &) const;
    virtual void setProperty(const PropertyName &, int value);

    /**
     * Specify the channel to use from the source model.
     * A value of -1 means to mix all available channels.
     * The default is channel 0.
     */
    void setChannel(int);
    int getChannel() const;

    void setWindowSize(size_t);
    size_t getWindowSize() const;
    
    void setWindowHopLevel(size_t level);
    size_t getWindowHopLevel() const;

    void setWindowType(WindowType type);
    WindowType getWindowType() const;

    void setZeroPadLevel(size_t level);
    size_t getZeroPadLevel() const;

    /**
     * Set the gain multiplier for sample values in this view.
     * The default is 1.0.
     */
    void setGain(float gain);
    float getGain() const;

    /**
     * Set the threshold for sample values to qualify for being shown
     * in the FFT, in voltage units.
     *
     * The default is 0.0.
     */
    void setThreshold(float threshold);
    float getThreshold() const;

    void setMinFrequency(size_t);
    size_t getMinFrequency() const;

    void setMaxFrequency(size_t); // 0 -> no maximum
    size_t getMaxFrequency() const;

    enum ColourScale {
	LinearColourScale,
	MeterColourScale,
        dBSquaredColourScale,
	dBColourScale,
	PhaseColourScale
    };

    /**
     * Specify the scale for sample levels.  See WaveformLayer for
     * details of meter and dB scaling.  The default is dBColourScale.
     */
    void setColourScale(ColourScale);
    ColourScale getColourScale() const;

    enum FrequencyScale {
	LinearFrequencyScale,
	LogFrequencyScale
    };
    
    /**
     * Specify the scale for the y axis.
     */
    void setFrequencyScale(FrequencyScale);
    FrequencyScale getFrequencyScale() const;

    enum BinDisplay {
	AllBins,
	PeakBins,
	PeakFrequencies
    };
    
    /**
     * Specify the processing of frequency bins for the y axis.
     */
    void setBinDisplay(BinDisplay);
    BinDisplay getBinDisplay() const;

    void setNormalizeColumns(bool n);
    bool getNormalizeColumns() const;

    void setNormalizeVisibleArea(bool n);
    bool getNormalizeVisibleArea() const;

    void setColourMap(int map);
    int getColourMap() const;

    /**
     * Specify the colourmap rotation for the colour scale.
     */
    void setColourRotation(int);
    int getColourRotation() const;

    virtual VerticalPosition getPreferredFrameCountPosition() const {
	return PositionTop;
    }

    virtual bool isLayerOpaque() const { return true; }

    virtual ColourSignificance getLayerColourSignificance() const {
        return ColourHasMeaningfulValue;
    }

    float getYForFrequency(const View *v, float frequency) const;
    float getFrequencyForY(const View *v, int y) const;

    virtual int getCompletion(View *v) const;

    virtual bool getValueExtents(float &min, float &max,
                                 bool &logarithmic, QString &unit) const;

    virtual bool getDisplayExtents(float &min, float &max) const;

    virtual bool setDisplayExtents(float min, float max);

    virtual bool getYScaleValue(const View *, int, float &, QString &) const;

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    void setProperties(const QXmlAttributes &attributes);

    virtual void setLayerDormant(const View *v, bool dormant);

    virtual bool isLayerScrollable(const View *) const { return false; }

    virtual int getVerticalZoomSteps(int &defaultStep) const;
    virtual int getCurrentVerticalZoomStep() const;
    virtual void setVerticalZoomStep(int);
    virtual RangeMapper *getNewVerticalZoomRangeMapper() const;

    virtual const Model *getSliceableModel() const;

protected slots:
    void cacheInvalid();
    void cacheInvalid(size_t startFrame, size_t endFrame);
    
    void preferenceChanged(PropertyContainer::PropertyName name);

    void fillTimerTimedOut();

protected:
    const DenseTimeValueModel *m_model; // I do not own this

    int                 m_channel;
    size_t              m_windowSize;
    WindowType          m_windowType;
    size_t              m_windowHopLevel;
    size_t              m_zeroPadLevel;
    size_t              m_fftSize;
    float               m_gain;
    float               m_initialGain;
    float               m_threshold;
    float               m_initialThreshold;
    int                 m_colourRotation;
    int                 m_initialRotation;
    size_t              m_minFrequency;
    size_t              m_maxFrequency;
    size_t              m_initialMaxFrequency;
    ColourScale         m_colourScale;
    int                 m_colourMap;
    QColor              m_crosshairColour;
    FrequencyScale      m_frequencyScale;
    BinDisplay          m_binDisplay;
    bool                m_normalizeColumns;
    bool                m_normalizeVisibleArea;
    int                 m_lastEmittedZoomStep;
    bool                m_synchronous;

    mutable int         m_lastPaintBlockWidth;
    mutable RealTime    m_lastPaintTime;

    enum { NO_VALUE = 0 }; // colour index for unused pixels

    class Palette
    {
    public:
        QColor getColour(unsigned char index) const {
            return m_colours[index];
        }
    
        void setColour(unsigned char index, QColor colour) {
            m_colours[index] = colour;
        }

    private:
        QColor m_colours[256];
    };

    Palette m_palette;

    /**
     * ImageCache covers the area of the view, at view resolution.
     * Not all of it is necessarily valid at once (it is refreshed
     * in parts when scrolling, for example).
     */
    struct ImageCache
    {
        QImage image;
        QRect validArea;
        long startFrame;
        size_t zoomLevel;
    };
    typedef std::map<const View *, ImageCache> ViewImageCache;
    void invalidateImageCaches();
    void invalidateImageCaches(size_t startFrame, size_t endFrame);
    mutable ViewImageCache m_imageCaches;

    /**
     * When painting, we draw directly onto the draw buffer and then
     * copy this to the part of the image cache that needed refreshing
     * before copying the image cache onto the window.  (Remind me why
     * we don't draw directly onto the cache?)
     */
    mutable QImage m_drawBuffer;

    mutable QTimer *m_updateTimer;

    mutable size_t m_candidateFillStartFrame;
    bool m_exiting;

    void initialisePalette();
    void rotatePalette(int distance);

    unsigned char getDisplayValue(View *v, float input) const;
    float getInputForDisplayValue(unsigned char uc) const;

    int getColourScaleWidth(QPainter &) const;

    void illuminateLocalFeatures(View *v, QPainter &painter) const;

    float getEffectiveMinFrequency() const;
    float getEffectiveMaxFrequency() const;

    struct LayerRange {
	long   startFrame;
	int    zoomLevel;
	size_t modelStart;
	size_t modelEnd;
    };

    // Note that the getYBin... methods return the nominal bin in the
    // un-smoothed spectrogram.  This is not necessarily the same bin
    // as is pulled from the spectrogram and drawn at the given
    // position, if the spectrogram has oversampling smoothing.  Use
    // getSmoothedYBinRange to obtain that.

    bool getXBinRange(View *v, int x, float &windowMin, float &windowMax) const;
    bool getYBinRange(View *v, int y, float &freqBinMin, float &freqBinMax) const;
    bool getSmoothedYBinRange(View *v, int y, float &freqBinMin, float &freqBinMax) const;

    bool getYBinSourceRange(View *v, int y, float &freqMin, float &freqMax) const;
    bool getAdjustedYBinSourceRange(View *v, int x, int y,
				    float &freqMin, float &freqMax,
				    float &adjFreqMin, float &adjFreqMax) const;
    bool getXBinSourceRange(View *v, int x, RealTime &timeMin, RealTime &timeMax) const;
    bool getXYBinSourceRange(View *v, int x, int y, float &min, float &max,
			     float &phaseMin, float &phaseMax) const;

    size_t getWindowIncrement() const {
        if (m_windowHopLevel == 0) return m_windowSize;
        else if (m_windowHopLevel == 1) return (m_windowSize * 3) / 4;
        else return m_windowSize / (1 << (m_windowHopLevel - 1));
    }

    size_t getZeroPadLevel(const View *v) const;
    size_t getFFTSize(const View *v) const;
    FFTModel *getFFTModel(const View *v) const;
    Dense3DModelPeakCache *getPeakCache(const View *v) const;
    void invalidateFFTModels();

    typedef std::pair<FFTModel *, int> FFTFillPair; // model, last fill
    typedef std::map<const View *, FFTFillPair> ViewFFTMap;
    typedef std::map<const View *, Dense3DModelPeakCache *> PeakCacheMap;
    mutable ViewFFTMap m_fftModels;
    mutable PeakCacheMap m_peakCaches;
    mutable Model *m_sliceableModel;

    class MagnitudeRange {
    public:
        MagnitudeRange() : m_min(0), m_max(0) { }
        bool operator==(const MagnitudeRange &r) {
            return r.m_min == m_min && r.m_max == m_max;
        }
        bool isSet() const { return (m_min != 0 || m_max != 0); }
        void set(float min, float max) {
            m_min = convert(min);
            m_max = convert(max);
            if (m_max < m_min) m_max = m_min;
        }
        bool sample(float f) {
            unsigned int ui = convert(f);
            bool changed = false;
            if (isSet()) {
                if (ui < m_min) { m_min = ui; changed = true; }
                if (ui > m_max) { m_max = ui; changed = true; }
            } else {
                m_max = m_min = ui;
                changed = true;
            }
            return changed;
        }            
        bool sample(const MagnitudeRange &r) {
            bool changed = false;
            if (isSet()) {
                if (r.m_min < m_min) { m_min = r.m_min; changed = true; }
                if (r.m_max > m_max) { m_max = r.m_max; changed = true; }
            } else {
                m_min = r.m_min;
                m_max = r.m_max;
                changed = true;
            }
            return changed;
        }            
        float getMin() const { return float(m_min) / UINT_MAX; }
        float getMax() const { return float(m_max) / UINT_MAX; }
    private:
        unsigned int m_min;
        unsigned int m_max;
        unsigned int convert(float f) {
            if (f < 0.f) f = 0.f;
            if (f > 1.f) f = 1.f;
            return (unsigned int)(f * UINT_MAX);
        }
    };

    typedef std::map<const View *, MagnitudeRange> ViewMagMap;
    mutable ViewMagMap m_viewMags;
    mutable std::vector<MagnitudeRange> m_columnMags;
    void invalidateMagnitudes();
    bool updateViewMagnitudes(View *v) const;
    bool paintDrawBuffer(View *v, int w, int h,
                         int *binforx, float *binfory,
                         bool usePeaksCache,
                         MagnitudeRange &overallMag,
                         bool &overallMagChanged) const;
    bool paintDrawBufferPeakFrequencies(View *v, int w, int h,
                                        int *binforx,
                                        int minbin,
                                        int maxbin,
                                        float displayMinFreq,
                                        float displayMaxFreq,
                                        bool logarithmic,
                                        MagnitudeRange &overallMag,
                                        bool &overallMagChanged) const;

    virtual void updateMeasureRectYCoords(View *v, const MeasureRect &r) const;
    virtual void setMeasureRectYCoord(View *v, MeasureRect &r, bool start, int y) const;
};

#endif
