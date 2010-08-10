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

#ifndef _VIEW_H_
#define _VIEW_H_

#include <QFrame>
#include <QProgressBar>

#include "base/ZoomConstraint.h"
#include "base/PropertyContainer.h"
#include "ViewManager.h"
#include "base/XmlExportable.h"

// #define DEBUG_VIEW_WIDGET_PAINT 1

class Layer;
class ViewPropertyContainer;

#include <map>
#include <set>

/**
 * View is the base class of widgets that display one or more
 * overlaid views of data against a horizontal time scale. 
 *
 * A View may have any number of attached Layers, each of which
 * is expected to have one data Model (although multiple views may
 * share the same model).
 *
 * A View may be panned in time and zoomed, although the
 * mechanisms for doing so (as well as any other operations and
 * properties available) depend on the subclass.
 */

class View : public QFrame,
	     public XmlExportable
{
    Q_OBJECT

public:
    /**
     * Deleting a View does not delete any of its layers.  They should
     * be managed elsewhere (e.g. by the Document).
     */
    virtual ~View();

    /**
     * Retrieve the first visible sample frame on the widget.
     * This is a calculated value based on the centre-frame, widget
     * width and zoom level.  The result may be negative.
     */
    long getStartFrame() const;

    /**
     * Set the widget pan based on the given first visible frame.  The
     * frame value may be negative.
     */
    void setStartFrame(long);

    /**
     * Return the centre frame of the visible widget.  This is an
     * exact value that does not depend on the zoom block size.  Other
     * frame values (start, end) are calculated from this based on the
     * zoom and other factors.
     */
    size_t getCentreFrame() const { return m_centreFrame; }

    /**
     * Set the centre frame of the visible widget.
     */
    void setCentreFrame(size_t f) { setCentreFrame(f, true); }

    /**
     * Retrieve the last visible sample frame on the widget.
     * This is a calculated value based on the centre-frame, widget
     * width and zoom level.
     */
    size_t getEndFrame() const;

    /**
     * Return the pixel x-coordinate corresponding to a given sample
     * frame (which may be negative).
     */
    int getXForFrame(long frame) const;

    /**
     * Return the closest frame to the given pixel x-coordinate.
     */
    long getFrameForX(int x) const;

    /**
     * Return the pixel y-coordinate corresponding to a given
     * frequency, if the frequency range is as specified.  This does
     * not imply any policy about layer frequency ranges, but it might
     * be useful for layers to match theirs up if desired.
     *
     * Not thread-safe in logarithmic mode.  Call only from GUI thread.
     */
    float getYForFrequency(float frequency, float minFreq, float maxFreq, 
			   bool logarithmic) const;

    /**
     * Return the closest frequency to the given pixel y-coordinate,
     * if the frequency range is as specified.
     *
     * Not thread-safe in logarithmic mode.  Call only from GUI thread.
     */
    float getFrequencyForY(int y, float minFreq, float maxFreq,
			   bool logarithmic) const;

    /**
     * Return the zoom level, i.e. the number of frames per pixel
     */
    int getZoomLevel() const;

    /**
     * Set the zoom level, i.e. the number of frames per pixel.  The
     * centre frame will be unchanged; the start and end frames will
     * change.
     */
    virtual void setZoomLevel(size_t z);

    /**
     * Zoom in or out.
     */
    virtual void zoom(bool in);

    /**
     * Scroll left or right by a smallish or largish amount.
     */
    virtual void scroll(bool right, bool lots, bool doEmit = true);

    virtual void addLayer(Layer *v);
    virtual void removeLayer(Layer *v); // does not delete the layer
    virtual int getLayerCount() const { return m_layers.size(); }

    /**
     * Return a layer, counted in stacking order.  That is, layer 0 is
     * the bottom layer and layer "getLayerCount()-1" is the top one.
     */
    virtual Layer *getLayer(int n) {
        if (n < int(m_layers.size())) return m_layers[n]; else return 0;
    }

    /**
     * Return the top layer.  This is the same as
     * getLayer(getLayerCount()-1) if there is at least one layer, and
     * 0 otherwise.
     */
    virtual Layer *getTopLayer() {
        return m_layers.empty() ? 0 : m_layers[m_layers.size()-1];
    }

    /**
     * Return the layer last selected by the user.  This is normally
     * the top layer, the same as getLayer(getLayerCount()-1).
     * However, if the user has selected the pane itself more recently
     * than any of the layers on it, this function will return 0.  It
     * will also return 0 if there are no layers.
     */
    virtual Layer *getSelectedLayer();
    virtual const Layer *getSelectedLayer() const;

    virtual void setViewManager(ViewManager *m);
    virtual void setViewManager(ViewManager *m, long initialFrame);
    virtual ViewManager *getViewManager() const { return m_manager; }

    virtual void setFollowGlobalPan(bool f);
    virtual bool getFollowGlobalPan() const { return m_followPan; }

    virtual void setFollowGlobalZoom(bool f);
    virtual bool getFollowGlobalZoom() const { return m_followZoom; }

    virtual bool hasLightBackground() const;
    virtual QColor getForeground() const;
    virtual QColor getBackground() const;

    enum TextStyle {
	BoxedText,
	OutlinedText
    };

    virtual void drawVisibleText(QPainter &p, int x, int y,
				 QString text, TextStyle style) const;

    virtual void drawMeasurementRect(QPainter &p, const Layer *,
                                     QRect rect, bool focus) const;

    virtual bool shouldIlluminateLocalFeatures(const Layer *, QPoint &) const {
	return false;
    }
    virtual bool shouldIlluminateLocalSelection(QPoint &, bool &, bool &) const {
	return false;
    }

    virtual void setPlaybackFollow(PlaybackFollowMode m);
    virtual PlaybackFollowMode getPlaybackFollow() const { return m_followPlay; }

    typedef PropertyContainer::PropertyName PropertyName;

    // We implement the PropertyContainer API, although we don't
    // actually subclass PropertyContainer.  We have our own
    // PropertyContainer that we can return on request that just
    // delegates back to us.
    virtual PropertyContainer::PropertyList getProperties() const;
    virtual QString getPropertyLabel(const PropertyName &) const;
    virtual PropertyContainer::PropertyType getPropertyType(const PropertyName &) const;
    virtual int getPropertyRangeAndValue(const PropertyName &,
					 int *min, int *max, int *deflt) const;
    virtual QString getPropertyValueLabel(const PropertyName &,
					  int value) const;
    virtual void setProperty(const PropertyName &, int value);
    virtual QString getPropertyContainerName() const {
	return objectName();
    }
    virtual QString getPropertyContainerIconName() const = 0;

    virtual size_t getPropertyContainerCount() const;

    virtual const PropertyContainer *getPropertyContainer(size_t i) const;
    virtual PropertyContainer *getPropertyContainer(size_t i);

    // Render the contents on a wide canvas
    virtual QImage *toNewImage(size_t f0, size_t f1);
    virtual QImage *toNewImage();
    virtual QSize getImageSize(size_t f0, size_t f1);
    virtual QSize getImageSize();

    virtual int getTextLabelHeight(const Layer *layer, QPainter &) const;

    virtual bool getValueExtents(QString unit, float &min, float &max,
                                 bool &log) const;

    virtual void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const;

    // First frame actually in model, to right of scale, if present
    virtual size_t getFirstVisibleFrame() const;
    virtual size_t getLastVisibleFrame() const;

    size_t getModelsStartFrame() const;
    size_t getModelsEndFrame() const;

    typedef std::set<Model *> ModelSet;
    ModelSet getModels();

    //!!!
    Model *getAligningModel() const;
    size_t alignFromReference(size_t) const;
    size_t alignToReference(size_t) const;
    int getAlignedPlaybackFrame() const;

signals:
    void propertyContainerAdded(PropertyContainer *pc);
    void propertyContainerRemoved(PropertyContainer *pc);
    void propertyContainerPropertyChanged(PropertyContainer *pc);
    void propertyContainerPropertyRangeChanged(PropertyContainer *pc);
    void propertyContainerNameChanged(PropertyContainer *pc);
    void propertyContainerSelected(PropertyContainer *pc);
    void propertyChanged(PropertyContainer::PropertyName);

    void layerModelChanged();

    void centreFrameChanged(unsigned long frame,
                            bool globalScroll,
                            PlaybackFollowMode followMode);

    void zoomLevelChanged(unsigned long, bool);

    void contextHelpChanged(const QString &);

public slots:
    virtual void modelChanged();
    virtual void modelChanged(size_t startFrame, size_t endFrame);
    virtual void modelCompletionChanged();
    virtual void modelAlignmentCompletionChanged();
    virtual void modelReplaced();
    virtual void layerParametersChanged();
    virtual void layerParameterRangesChanged();
    virtual void layerMeasurementRectsChanged();
    virtual void layerNameChanged();

    virtual void globalCentreFrameChanged(unsigned long);
    virtual void viewCentreFrameChanged(View *, unsigned long);
    virtual void viewManagerPlaybackFrameChanged(unsigned long);
    virtual void viewZoomLevelChanged(View *, unsigned long, bool);

    virtual void propertyContainerSelected(View *, PropertyContainer *pc);

    virtual void selectionChanged();
    virtual void toolModeChanged();
    virtual void overlayModeChanged();
    virtual void zoomWheelsEnabledChanged();

    virtual void progressCheckStalledTimerElapsed();

protected:
    View(QWidget *, bool showProgress);
    virtual void paintEvent(QPaintEvent *e);
    virtual void drawSelections(QPainter &);
    virtual bool shouldLabelSelections() const { return true; }
    virtual bool render(QPainter &paint, int x0, size_t f0, size_t f1);
    virtual void setPaintFont(QPainter &paint);
    
    typedef std::vector<Layer *> LayerList;

    int getModelsSampleRate() const;
    bool areLayersScrollable() const;
    LayerList getScrollableBackLayers(bool testChanged, bool &changed) const;
    LayerList getNonScrollableFrontLayers(bool testChanged, bool &changed) const;
    size_t getZoomConstraintBlockSize(size_t blockSize,
				      ZoomConstraint::RoundingDirection dir =
				      ZoomConstraint::RoundNearest) const;

    // True if the top layer(s) use colours for meaningful things.  If
    // this is the case, selections will be shown using unfilled boxes
    // rather than with a translucent fill.
    bool areLayerColoursSignificant() const;

    // True if the top layer has a time axis on the x coordinate (this
    // is generally the case except for spectrum/slice layers).  It
    // will not be possible to make or display selections if this is
    // false.
    bool hasTopLayerTimeXAxis() const;

    bool setCentreFrame(size_t f, bool doEmit);

    void movePlayPointer(unsigned long f);

    void checkProgress(void *object);
    int getProgressBarWidth() const; // if visible

    size_t              m_centreFrame;
    int                 m_zoomLevel;
    bool                m_followPan;
    bool                m_followZoom;
    PlaybackFollowMode  m_followPlay;
    size_t              m_playPointerFrame;
    bool                m_lightBackground;
    bool                m_showProgress;

    QPixmap            *m_cache;
    size_t              m_cacheCentreFrame;
    int                 m_cacheZoomLevel;
    bool                m_selectionCached;

    bool                m_deleting;

    LayerList           m_layers; // I don't own these, but see dtor note above
    bool                m_haveSelectedLayer;

    // caches for use in getScrollableBackLayers, getNonScrollableFrontLayers
    mutable LayerList m_lastScrollableBackLayers;
    mutable LayerList m_lastNonScrollableBackLayers;

    class LayerProgressBar : public QProgressBar {
    public:
	LayerProgressBar(QWidget *parent);
	virtual QString text() const { return m_text; }
	virtual void setText(QString text) { m_text = text; }
    protected:
	QString m_text;
    };

    struct ProgressBarRec {
        QProgressBar *bar;
        int lastCheck;
        QTimer *checkTimer;
    };
    typedef std::map<Layer *, ProgressBarRec> ProgressMap;
    ProgressMap m_progressBars; // I own the ProgressBars

    ViewManager *m_manager; // I don't own this
    ViewPropertyContainer *m_propertyContainer; // I own this
};


// Use this for delegation, because we can't subclass from
// PropertyContainer (which is a QObject) ourselves because of
// ambiguity with QFrame parent

class ViewPropertyContainer : public PropertyContainer
{
    Q_OBJECT

public:
    ViewPropertyContainer(View *v);
    PropertyList getProperties() const { return m_v->getProperties(); }
    QString getPropertyLabel(const PropertyName &n) const {
        return m_v->getPropertyLabel(n);
    }
    PropertyType getPropertyType(const PropertyName &n) const {
	return m_v->getPropertyType(n);
    }
    int getPropertyRangeAndValue(const PropertyName &n, int *min, int *max,
                                 int *deflt) const {
	return m_v->getPropertyRangeAndValue(n, min, max, deflt);
    }
    QString getPropertyValueLabel(const PropertyName &n, int value) const {
	return m_v->getPropertyValueLabel(n, value);
    }
    QString getPropertyContainerName() const {
	return m_v->getPropertyContainerName();
    }
    QString getPropertyContainerIconName() const {
	return m_v->getPropertyContainerIconName();
    }

public slots:
    virtual void setProperty(const PropertyName &n, int value) {
	m_v->setProperty(n, value);
    }

protected:
    View *m_v;
};

#endif

