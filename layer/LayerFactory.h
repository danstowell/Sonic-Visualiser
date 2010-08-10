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

#ifndef _LAYER_FACTORY_H_
#define _LAYER_FACTORY_H_

#include <QString>
#include <set>

class Layer;
class Model;
class Clipboard;

class LayerFactory
{
public:
    enum LayerType {

	// Standard layers
	Waveform,
	Spectrogram,
	TimeRuler,
	TimeInstants,
	TimeValues,
	Notes,
	Regions,
	Text,
        Image,
	Colour3DPlot,
        Spectrum,
        Slice,

	// Layers with different initial parameters
	MelodicRangeSpectrogram,
	PeakFrequencySpectrogram,

	// Not-a-layer-type
	UnknownLayer = 255
    };

    static LayerFactory *getInstance();
    
    virtual ~LayerFactory();

    typedef std::set<LayerType> LayerTypeSet;
    LayerTypeSet getValidLayerTypes(Model *model);
    LayerTypeSet getValidEmptyLayerTypes();

    LayerType getLayerType(const Layer *);

    Layer *createLayer(LayerType type);

    void setLayerDefaultProperties(LayerType type, Layer *layer);

    QString getLayerPresentationName(LayerType type);

    bool isLayerSliceable(const Layer *);

    void setModel(Layer *layer, Model *model);
    Model *createEmptyModel(LayerType type, Model *baseModel);

    int getChannel(Layer *layer);
    void setChannel(Layer *layer, int channel);

    QString getLayerIconName(LayerType);
    QString getLayerTypeName(LayerType);
    LayerType getLayerTypeForName(QString);

    LayerType getLayerTypeForClipboardContents(const Clipboard &);

protected:
    template <typename LayerClass, typename ModelClass>
    bool trySetModel(Layer *layerBase, Model *modelBase) {
	LayerClass *layer = dynamic_cast<LayerClass *>(layerBase);
	if (!layer) return false;
	ModelClass *model = dynamic_cast<ModelClass *>(modelBase);
	if (!model) return false;
	layer->setModel(model);
	return true;
    }

    static LayerFactory *m_instance;
};

#endif

