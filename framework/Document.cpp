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

#include "Document.h"

#include "data/model/WaveFileModel.h"
#include "data/model/WritableWaveFileModel.h"
#include "data/model/DenseThreeDimensionalModel.h"
#include "data/model/DenseTimeValueModel.h"
#include "layer/Layer.h"
#include "widgets/CommandHistory.h"
#include "base/Command.h"
#include "view/View.h"
#include "base/PlayParameterRepository.h"
#include "base/PlayParameters.h"
#include "transform/TransformFactory.h"
#include "transform/ModelTransformerFactory.h"
#include <QApplication>
#include <QTextStream>
#include <QSettings>
#include <iostream>

// For alignment:
#include "data/model/AggregateWaveModel.h"
#include "data/model/SparseTimeValueModel.h"
#include "data/model/AlignmentModel.h"

//#define DEBUG_DOCUMENT 1

//!!! still need to handle command history, documentRestored/documentModified

Document::Document() :
    m_mainModel(0),
    m_autoAlignment(false)
{
    connect(this, SIGNAL(modelAboutToBeDeleted(Model *)),
            ModelTransformerFactory::getInstance(),
            SLOT(modelAboutToBeDeleted(Model *)));
}

Document::~Document()
{
    //!!! Document should really own the command history.  atm we
    //still refer to it in various places that don't have access to
    //the document, be nice to fix that

#ifdef DEBUG_DOCUMENT
    std::cerr << "\n\nDocument::~Document: about to clear command history" << std::endl;
#endif
    CommandHistory::getInstance()->clear();
    
#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::~Document: about to delete layers" << std::endl;
#endif
    while (!m_layers.empty()) {
	deleteLayer(*m_layers.begin(), true);
    }

    if (!m_models.empty()) {
	std::cerr << "Document::~Document: WARNING: " 
		  << m_models.size() << " model(s) still remain -- "
		  << "should have been garbage collected when deleting layers"
		  << std::endl;
	while (!m_models.empty()) {
            Model *model = m_models.begin()->first;
	    if (model == m_mainModel) {
		// just in case!
		std::cerr << "Document::~Document: WARNING: Main model is also"
			  << " in models list!" << std::endl;
	    } else if (model) {
                model->aboutToDelete();
		emit modelAboutToBeDeleted(model);
		delete model;
	    }
	    m_models.erase(m_models.begin());
	}
    }

#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::~Document: About to get rid of main model"
	      << std::endl;
#endif
    if (m_mainModel) {
        m_mainModel->aboutToDelete();
        emit modelAboutToBeDeleted(m_mainModel);
    }

    emit mainModelChanged(0);
    delete m_mainModel;

}

Layer *
Document::createLayer(LayerFactory::LayerType type)
{
    Layer *newLayer = LayerFactory::getInstance()->createLayer(type);
    if (!newLayer) return 0;

    newLayer->setObjectName(getUniqueLayerName(newLayer->objectName()));

    m_layers.insert(newLayer);

#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::createLayer: Added layer of type " << type
              << ", now have " << m_layers.size() << " layers" << std::endl;
#endif

    emit layerAdded(newLayer);

    return newLayer;
}

Layer *
Document::createMainModelLayer(LayerFactory::LayerType type)
{
    Layer *newLayer = createLayer(type);
    if (!newLayer) return 0;
    setModel(newLayer, m_mainModel);
    return newLayer;
}

Layer *
Document::createImportedLayer(Model *model)
{
    LayerFactory::LayerTypeSet types =
	LayerFactory::getInstance()->getValidLayerTypes(model);

    if (types.empty()) {
	std::cerr << "WARNING: Document::importLayer: no valid display layer for model" << std::endl;
	return 0;
    }

    //!!! for now, just use the first suitable layer type
    LayerFactory::LayerType type = *types.begin();

    Layer *newLayer = LayerFactory::getInstance()->createLayer(type);
    if (!newLayer) return 0;

    newLayer->setObjectName(getUniqueLayerName(newLayer->objectName()));

    addImportedModel(model);
    setModel(newLayer, model);

    //!!! and all channels
    setChannel(newLayer, -1);

    m_layers.insert(newLayer);

#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::createImportedLayer: Added layer of type " << type
              << ", now have " << m_layers.size() << " layers" << std::endl;
#endif

    emit layerAdded(newLayer);
    return newLayer;
}

Layer *
Document::createEmptyLayer(LayerFactory::LayerType type)
{
    if (!m_mainModel) return 0;

    Model *newModel =
	LayerFactory::getInstance()->createEmptyModel(type, m_mainModel);
    if (!newModel) return 0;

    Layer *newLayer = createLayer(type);
    if (!newLayer) {
	delete newModel;
	return 0;
    }

    addImportedModel(newModel);
    setModel(newLayer, newModel);

    return newLayer;
}

Layer *
Document::createDerivedLayer(LayerFactory::LayerType type,
			     TransformId transform)
{
    Layer *newLayer = createLayer(type);
    if (!newLayer) return 0;

    newLayer->setObjectName(getUniqueLayerName
                            (TransformFactory::getInstance()->
                             getTransformFriendlyName(transform)));

    return newLayer;
}

Layer *
Document::createDerivedLayer(const Transform &transform,
                             const ModelTransformer::Input &input)
{
    QString message;
    Model *newModel = addDerivedModel(transform, input, message);
    if (!newModel) {
        emit modelGenerationFailed(transform.getIdentifier(), message);
        return 0;
    } else if (message != "") {
        emit modelGenerationWarning(transform.getIdentifier(), message);
    }

    LayerFactory::LayerTypeSet types =
	LayerFactory::getInstance()->getValidLayerTypes(newModel);

    if (types.empty()) {
	std::cerr << "WARNING: Document::createLayerForTransformer: no valid display layer for output of transform " << transform.getIdentifier().toStdString() << std::endl;
        newModel->aboutToDelete();
        emit modelAboutToBeDeleted(newModel);
        m_models.erase(newModel);
	delete newModel;
	return 0;
    }

    //!!! for now, just use the first suitable layer type

    Layer *newLayer = createLayer(*types.begin());
    setModel(newLayer, newModel);

    //!!! We need to clone the model when adding the layer, so that it
    //can be edited without affecting other layers that are based on
    //the same model.  Unfortunately we can't just clone it now,
    //because it probably hasn't been completed yet -- the transform
    //runs in the background.  Maybe the transform has to handle
    //cloning and cacheing models itself.
    //
    // Once we do clone models here, of course, we'll have to avoid
    // leaking them too.
    //
    // We want the user to be able to add a model to a second layer
    // _while it's still being calculated in the first_ and have it
    // work quickly.  That means we need to put the same physical
    // model pointer in both layers, so they can't actually be cloned.
    
    if (newLayer) {
	newLayer->setObjectName(getUniqueLayerName
                                (TransformFactory::getInstance()->
                                 getTransformFriendlyName
                                 (transform.getIdentifier())));
    }

    emit layerAdded(newLayer);
    return newLayer;
}

void
Document::setMainModel(WaveFileModel *model)
{
    Model *oldMainModel = m_mainModel;
    m_mainModel = model;
    
    emit modelAdded(m_mainModel);
    if (model) {
        emit activity(tr("Set main model to %1").arg(model->objectName()));
    } else {
        emit activity(tr("Clear main model"));
    }

    std::vector<Layer *> obsoleteLayers;
    std::set<QString> failedTransformers;

    // We need to ensure that no layer is left using oldMainModel or
    // any of the old derived models as its model.  Either replace the
    // model, or delete the layer for each layer that is currently
    // using one of these.  Carry out this replacement before we
    // delete any of the models.

#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::setMainModel: Have "
              << m_layers.size() << " layers" << std::endl;
    std::cerr << "Models now: ";
    for (ModelMap::const_iterator i = m_models.begin(); i != m_models.end(); ++i) {
        std::cerr << i->first << " ";
    } 
    std::cerr << std::endl;
#endif

    for (LayerSet::iterator i = m_layers.begin(); i != m_layers.end(); ++i) {

	Layer *layer = *i;
	Model *model = layer->getModel();

#ifdef DEBUG_DOCUMENT
        std::cerr << "Document::setMainModel: inspecting model "
                  << (model ? model->objectName().toStdString() : "(null)") << " in layer "
                  << layer->objectName().toStdString() << std::endl;
#endif

	if (model == oldMainModel) {
#ifdef DEBUG_DOCUMENT
            std::cerr << "... it uses the old main model, replacing" << std::endl;
#endif
	    LayerFactory::getInstance()->setModel(layer, m_mainModel);
	    continue;
	}

        if (!model) {
            std::cerr << "WARNING: Document::setMainModel: Null model in layer "
                      << layer << std::endl;
	    // get rid of this hideous degenerate
	    obsoleteLayers.push_back(layer);
	    continue;
	}

	if (m_models.find(model) == m_models.end()) {
	    std::cerr << "WARNING: Document::setMainModel: Unknown model "
		      << model << " in layer " << layer << std::endl;
	    // and this one
	    obsoleteLayers.push_back(layer);
	    continue;
	}
	    
	if (m_models[model].source &&
            (m_models[model].source == oldMainModel)) {

#ifdef DEBUG_DOCUMENT
            std::cerr << "... it uses a model derived from the old main model, regenerating" << std::endl;
#endif

	    // This model was derived from the previous main
	    // model: regenerate it.
	    
	    const Transform &transform = m_models[model].transform;
            QString transformId = transform.getIdentifier();
	    
            //!!! We have a problem here if the number of channels in
            //the main model has changed.

            QString message;
	    Model *replacementModel =
                addDerivedModel(transform,
                                ModelTransformer::Input
                                (m_mainModel, m_models[model].channel),
                                message);
	    
	    if (!replacementModel) {
		std::cerr << "WARNING: Document::setMainModel: Failed to regenerate model for transform \""
			  << transformId.toStdString() << "\"" << " in layer " << layer << std::endl;
                if (failedTransformers.find(transformId)
                    == failedTransformers.end()) {
                    emit modelRegenerationFailed(layer->objectName(),
                                                 transformId,
                                                 message);
                    failedTransformers.insert(transformId);
                }
		obsoleteLayers.push_back(layer);
	    } else {
                if (message != "") {
                    emit modelRegenerationWarning(layer->objectName(),
                                                  transformId,
                                                  message);
                }
#ifdef DEBUG_DOCUMENT
                std::cerr << "Replacing model " << model << " (type "
                          << typeid(*model).name() << ") with model "
                          << replacementModel << " (type "
                          << typeid(*replacementModel).name() << ") in layer "
                          << layer << " (name " << layer->objectName().toStdString() << ")"
                          << std::endl;
#endif
                RangeSummarisableTimeValueModel *rm =
                    dynamic_cast<RangeSummarisableTimeValueModel *>(replacementModel);
#ifdef DEBUG_DOCUMENT
                if (rm) {
                    std::cerr << "new model has " << rm->getChannelCount() << " channels " << std::endl;
                } else {
                    std::cerr << "new model " << replacementModel << " is not a RangeSummarisableTimeValueModel!" << std::endl;
                }
#endif
		setModel(layer, replacementModel);
	    }
	}	    
    }

    for (size_t k = 0; k < obsoleteLayers.size(); ++k) {
	deleteLayer(obsoleteLayers[k], true);
    }

    for (ModelMap::iterator i = m_models.begin(); i != m_models.end(); ++i) {

        Model *m = i->first;

#ifdef DEBUG_DOCUMENT
        std::cerr << "considering alignment for model " << m << " (name \""
                  << m->objectName().toStdString() << "\")" << std::endl;
#endif

        if (m_autoAlignment) {

            alignModel(m);

        } else if (oldMainModel &&
                   (m->getAlignmentReference() == oldMainModel)) {

            alignModel(m);
        }
    }

    if (oldMainModel) {
        oldMainModel->aboutToDelete();
        emit modelAboutToBeDeleted(oldMainModel);
    }

    if (m_autoAlignment) {
        alignModel(m_mainModel);
    }

    emit mainModelChanged(m_mainModel);

    delete oldMainModel;
}

void
Document::addDerivedModel(const Transform &transform,
                          const ModelTransformer::Input &input,
                          Model *outputModelToAdd)
{
    if (m_models.find(outputModelToAdd) != m_models.end()) {
	std::cerr << "WARNING: Document::addDerivedModel: Model already added"
		  << std::endl;
	return;
    }

#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::addDerivedModel: source is " << input.getModel() << " \"" << input.getModel()->objectName().toStdString() << "\"" << std::endl;
#endif

    ModelRecord rec;
    rec.source = input.getModel();
    rec.channel = input.getChannel();
    rec.transform = transform;
    rec.refcount = 0;

    outputModelToAdd->setSourceModel(input.getModel());

    m_models[outputModelToAdd] = rec;

#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::addDerivedModel: Added model " << outputModelToAdd << std::endl;
    std::cerr << "Models now: ";
    for (ModelMap::const_iterator i = m_models.begin(); i != m_models.end(); ++i) {
        std::cerr << i->first << " ";
    } 
    std::cerr << std::endl;
#endif

    emit modelAdded(outputModelToAdd);
}


void
Document::addImportedModel(Model *model)
{
    if (m_models.find(model) != m_models.end()) {
	std::cerr << "WARNING: Document::addImportedModel: Model already added"
		  << std::endl;
	return;
    }

    ModelRecord rec;
    rec.source = 0;
    rec.refcount = 0;

    m_models[model] = rec;

#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::addImportedModel: Added model " << model << std::endl;
    std::cerr << "Models now: ";
    for (ModelMap::const_iterator i = m_models.begin(); i != m_models.end(); ++i) {
        std::cerr << i->first << " ";
    } 
    std::cerr << std::endl;
#endif

    if (m_autoAlignment) alignModel(model);

    emit modelAdded(model);
}

Model *
Document::addDerivedModel(const Transform &transform,
                          const ModelTransformer::Input &input,
                          QString &message)
{
    Model *model = 0;

    for (ModelMap::iterator i = m_models.begin(); i != m_models.end(); ++i) {
	if (i->second.transform == transform &&
	    i->second.source == input.getModel() && 
            i->second.channel == input.getChannel()) {
	    return i->first;
	}
    }

    model = ModelTransformerFactory::getInstance()->transform
        (transform, input, message);

    // The transform we actually used was presumably identical to the
    // one asked for, except that the version of the plugin may
    // differ.  It's possible that the returned message contains a
    // warning about this; that doesn't concern us here, but we do
    // need to ensure that the transform we remember is correct for
    // what was actually applied, with the current plugin version.

    Transform applied = transform;
    applied.setPluginVersion
        (TransformFactory::getInstance()->
         getDefaultTransformFor(transform.getIdentifier(),
                                lrintf(transform.getSampleRate()))
         .getPluginVersion());

    if (!model) {
	std::cerr << "WARNING: Document::addDerivedModel: no output model for transform " << transform.getIdentifier().toStdString() << std::endl;
    } else {
	addDerivedModel(applied, input, model);
    }

    return model;
}

void
Document::releaseModel(Model *model) // Will _not_ release main model!
{
    if (model == 0) {
	return;
    }

    if (model == m_mainModel) {
	return;
    }

    bool toDelete = false;

    if (m_models.find(model) != m_models.end()) {
	
	if (m_models[model].refcount == 0) {
	    std::cerr << "WARNING: Document::releaseModel: model " << model
		      << " reference count is zero already!" << std::endl;
	} else {
	    if (--m_models[model].refcount == 0) {
		toDelete = true;
	    }
	}
    } else { 
	std::cerr << "WARNING: Document::releaseModel: Unfound model "
		  << model << std::endl;
	toDelete = true;
    }

    if (toDelete) {

	int sourceCount = 0;

	for (ModelMap::iterator i = m_models.begin(); i != m_models.end(); ++i) {
	    if (i->second.source == model) {
		++sourceCount;
		i->second.source = 0;
	    }
	}

	if (sourceCount > 0) {
	    std::cerr << "Document::releaseModel: Deleting model "
		      << model << " even though it is source for "
		      << sourceCount << " other derived model(s) -- resetting "
		      << "their source fields appropriately" << std::endl;
	}

        model->aboutToDelete();
	emit modelAboutToBeDeleted(model);
	m_models.erase(model);

#ifdef DEBUG_DOCUMENT
        std::cerr << "Document::releaseModel: Deleted model " << model << std::endl;
        std::cerr << "Models now: ";
        for (ModelMap::const_iterator i = m_models.begin(); i != m_models.end(); ++i) {
            std::cerr << i->first << " ";
        } 
        std::cerr << std::endl;
#endif

	delete model;
    }
}

void
Document::deleteLayer(Layer *layer, bool force)
{
    if (m_layerViewMap.find(layer) != m_layerViewMap.end() &&
	m_layerViewMap[layer].size() > 0) {

	std::cerr << "WARNING: Document::deleteLayer: Layer "
		  << layer << " [" << layer->objectName().toStdString() << "]"
		  << " is still used in " << m_layerViewMap[layer].size()
		  << " views!" << std::endl;

	if (force) {

#ifdef DEBUG_DOCUMENT
	    std::cerr << "(force flag set -- deleting from all views)" << std::endl;
#endif

	    for (std::set<View *>::iterator j = m_layerViewMap[layer].begin();
		 j != m_layerViewMap[layer].end(); ++j) {
		// don't use removeLayerFromView, as it issues a command
		layer->setLayerDormant(*j, true);
		(*j)->removeLayer(layer);
	    }
	    
	    m_layerViewMap.erase(layer);

	} else {
	    return;
	}
    }

    if (m_layers.find(layer) == m_layers.end()) {
	std::cerr << "Document::deleteLayer: Layer "
		  << layer << " does not exist, or has already been deleted "
		  << "(this may not be as serious as it sounds)" << std::endl;
	return;
    }

    m_layers.erase(layer);

#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::deleteLayer: Removing, now have "
              << m_layers.size() << " layers" << std::endl;
#endif

    releaseModel(layer->getModel());
    emit layerRemoved(layer);
    emit layerAboutToBeDeleted(layer);
    delete layer;
}

void
Document::setModel(Layer *layer, Model *model)
{
    if (model && 
	model != m_mainModel &&
	m_models.find(model) == m_models.end()) {
	std::cerr << "ERROR: Document::setModel: Layer " << layer
		  << " (\"" << layer->objectName().toStdString()
                  << "\") wants to use unregistered model " << model
		  << ": register the layer's model before setting it!"
		  << std::endl;
	return;
    }

    Model *previousModel = layer->getModel();

    if (previousModel == model) {
        std::cerr << "NOTE: Document::setModel: Layer " << layer << " (\""
                  << layer->objectName().toStdString()
                  << "\") is already set to model "
                  << model << " (\""
                  << (model ? model->objectName().toStdString() : "(null)")
                  << "\")" << std::endl;
        return;
    }

    if (model && model != m_mainModel) {
	m_models[model].refcount ++;
    }

    if (model && previousModel) {
        PlayParameterRepository::getInstance()->copyParameters
            (previousModel, model);
    }

    LayerFactory::getInstance()->setModel(layer, model);

    if (previousModel) {
        releaseModel(previousModel);
    }
}

void
Document::setChannel(Layer *layer, int channel)
{
    LayerFactory::getInstance()->setChannel(layer, channel);
}

void
Document::addLayerToView(View *view, Layer *layer)
{
    Model *model = layer->getModel();
    if (!model) {
#ifdef DEBUG_DOCUMENT
	std::cerr << "Document::addLayerToView: Layer (\""
                  << layer->objectName().toStdString()
                  << "\") with no model being added to view: "
                  << "normally you want to set the model first" << std::endl;
#endif
    } else {
	if (model != m_mainModel &&
	    m_models.find(model) == m_models.end()) {
	    std::cerr << "ERROR: Document::addLayerToView: Layer " << layer
		      << " has unregistered model " << model
		      << " -- register the layer's model before adding the layer!" << std::endl;
	    return;
	}
    }

    CommandHistory::getInstance()->addCommand
	(new Document::AddLayerCommand(this, view, layer));
}

void
Document::removeLayerFromView(View *view, Layer *layer)
{
    CommandHistory::getInstance()->addCommand
	(new Document::RemoveLayerCommand(this, view, layer));
}

void
Document::addToLayerViewMap(Layer *layer, View *view)
{
    bool firstView = (m_layerViewMap.find(layer) == m_layerViewMap.end() ||
                      m_layerViewMap[layer].empty());

    if (m_layerViewMap[layer].find(view) !=
	m_layerViewMap[layer].end()) {
	std::cerr << "WARNING: Document::addToLayerViewMap:"
		  << " Layer " << layer << " -> view " << view << " already in"
		  << " layer view map -- internal inconsistency" << std::endl;
    }

    m_layerViewMap[layer].insert(view);

    if (firstView) emit layerInAView(layer, true);
}
    
void
Document::removeFromLayerViewMap(Layer *layer, View *view)
{
    if (m_layerViewMap[layer].find(view) ==
	m_layerViewMap[layer].end()) {
	std::cerr << "WARNING: Document::removeFromLayerViewMap:"
		  << " Layer " << layer << " -> view " << view << " not in"
		  << " layer view map -- internal inconsistency" << std::endl;
    }

    m_layerViewMap[layer].erase(view);

    if (m_layerViewMap[layer].empty()) {
        m_layerViewMap.erase(layer);
        emit layerInAView(layer, false);
    }
}

QString
Document::getUniqueLayerName(QString candidate)
{
    for (int count = 1; ; ++count) {

        QString adjusted =
            (count > 1 ? QString("%1 <%2>").arg(candidate).arg(count) :
             candidate);
        
        bool duplicate = false;

        for (LayerSet::iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
            if ((*i)->objectName() == adjusted) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) return adjusted;
    }
}

std::vector<Model *>
Document::getTransformInputModels()
{
    std::vector<Model *> models;

    if (!m_mainModel) return models;

    models.push_back(m_mainModel);

    //!!! This will pick up all models, including those that aren't visible...

    for (ModelMap::iterator i = m_models.begin(); i != m_models.end(); ++i) {

        Model *model = i->first;
        if (!model || model == m_mainModel) continue;
        DenseTimeValueModel *dtvm = dynamic_cast<DenseTimeValueModel *>(model);
        
        if (dtvm) {
            models.push_back(dtvm);
        }
    }

    return models;
}

bool
Document::isKnownModel(const Model *model) const
{
    if (model == m_mainModel) return true;
    return (m_models.find(const_cast<Model *>(model)) != m_models.end());
}

TransformId
Document::getAlignmentTransformName()
{
    QSettings settings;
    settings.beginGroup("Alignment");
    TransformId id =
        settings.value("transform-id",
                       "vamp:match-vamp-plugin:match:path").toString();
    settings.endGroup();
    return id;
}

bool
Document::canAlign() 
{
    TransformId id = getAlignmentTransformName();
    TransformFactory *factory = TransformFactory::getInstance();
    return factory->haveTransform(id);
}

void
Document::alignModel(Model *model)
{
    if (!m_mainModel) return;

    RangeSummarisableTimeValueModel *rm = 
        dynamic_cast<RangeSummarisableTimeValueModel *>(model);
    if (!rm) return;

    if (rm->getAlignmentReference() == m_mainModel) {
        std::cerr << "Document::alignModel: model " << rm << " is already aligned to main model " << m_mainModel << std::endl;
        return;
    }
    
    if (model == m_mainModel) {
        // The reference has an empty alignment to itself.  This makes
        // it possible to distinguish between the reference and any
        // unaligned model just by looking at the model itself,
        // without also knowing what the main model is
        std::cerr << "Document::alignModel(" << model << "): is main model, setting appropriately" << std::endl;
        rm->setAlignment(new AlignmentModel(model, model, 0, 0));
        return;
    }

    // This involves creating three new models:

    // 1. an AggregateWaveModel to provide the mixdowns of the main
    // model and the new model in its two channels, as input to the
    // MATCH plugin

    // 2. a SparseTimeValueModel, which is the model automatically
    // created by FeatureExtractionPluginTransformer when running the
    // MATCH plugin (thus containing the alignment path)

    // 3. an AlignmentModel, which stores the path model and carries
    // out alignment lookups on it.

    // The first two of these are provided as arguments to the
    // constructor for the third, which takes responsibility for
    // deleting them.  The AlignmentModel, meanwhile, is passed to the
    // new model we are aligning, which also takes responsibility for
    // it.  We should not have to delete any of these new models here.

    AggregateWaveModel::ChannelSpecList components;

    components.push_back(AggregateWaveModel::ModelChannelSpec
                         (m_mainModel, -1));

    components.push_back(AggregateWaveModel::ModelChannelSpec
                         (rm, -1));

    Model *aggregateModel = new AggregateWaveModel(components);
    ModelTransformer::Input aggregate(aggregateModel);

    TransformId id = "vamp:match-vamp-plugin:match:path"; //!!! configure
    
    TransformFactory *tf = TransformFactory::getInstance();

    Transform transform = tf->getDefaultTransformFor
        (id, aggregateModel->getSampleRate());

    transform.setStepSize(transform.getBlockSize()/2);
    transform.setParameter("serialise", 1);

    std::cerr << "Document::alignModel: Alignment transform step size " << transform.getStepSize() << ", block size " << transform.getBlockSize() << std::endl;

    ModelTransformerFactory *mtf = ModelTransformerFactory::getInstance();

    QString message;
    Model *transformOutput = mtf->transform(transform, aggregate, message);

    if (!transformOutput) {
        transform.setStepSize(0);
        transformOutput = mtf->transform(transform, aggregate, message);
    }

    SparseTimeValueModel *path = dynamic_cast<SparseTimeValueModel *>
        (transformOutput);

    if (!path) {
        std::cerr << "Document::alignModel: ERROR: Failed to create alignment path (no MATCH plugin?)" << std::endl;
        emit alignmentFailed(id, message);
        delete transformOutput;
        delete aggregateModel;
        return;
    }

    path->setCompletion(0);

    AlignmentModel *alignmentModel = new AlignmentModel
        (m_mainModel, model, aggregateModel, path);

    rm->setAlignment(alignmentModel);
}

void
Document::alignModels()
{
    for (ModelMap::iterator i = m_models.begin(); i != m_models.end(); ++i) {
        alignModel(i->first);
    }
    alignModel(m_mainModel);
}

Document::AddLayerCommand::AddLayerCommand(Document *d,
					   View *view,
					   Layer *layer) :
    m_d(d),
    m_view(view),
    m_layer(layer),
    m_name(qApp->translate("AddLayerCommand", "Add %1 Layer").arg(layer->objectName())),
    m_added(false)
{
}

Document::AddLayerCommand::~AddLayerCommand()
{
#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::AddLayerCommand::~AddLayerCommand" << std::endl;
#endif
    if (!m_added) {
	m_d->deleteLayer(m_layer);
    }
}

QString
Document::AddLayerCommand::getName() const
{
#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::AddLayerCommand::getName(): Name is "
              << m_name.toStdString() << std::endl;
#endif
    return m_name;
}

void
Document::AddLayerCommand::execute()
{
    for (int i = 0; i < m_view->getLayerCount(); ++i) {
	if (m_view->getLayer(i) == m_layer) {
	    // already there
	    m_layer->setLayerDormant(m_view, false);
	    m_added = true;
	    return;
	}
    }

    m_view->addLayer(m_layer);
    m_layer->setLayerDormant(m_view, false);

    m_d->addToLayerViewMap(m_layer, m_view);
    m_added = true;
}

void
Document::AddLayerCommand::unexecute()
{
    m_view->removeLayer(m_layer);
    m_layer->setLayerDormant(m_view, true);

    m_d->removeFromLayerViewMap(m_layer, m_view);
    m_added = false;
}

Document::RemoveLayerCommand::RemoveLayerCommand(Document *d,
						 View *view,
						 Layer *layer) :
    m_d(d),
    m_view(view),
    m_layer(layer),
    m_name(qApp->translate("RemoveLayerCommand", "Delete %1 Layer").arg(layer->objectName())),
    m_added(true)
{
}

Document::RemoveLayerCommand::~RemoveLayerCommand()
{
#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::RemoveLayerCommand::~RemoveLayerCommand" << std::endl;
#endif
    if (!m_added) {
	m_d->deleteLayer(m_layer);
    }
}

QString
Document::RemoveLayerCommand::getName() const
{
#ifdef DEBUG_DOCUMENT
    std::cerr << "Document::RemoveLayerCommand::getName(): Name is "
              << m_name.toStdString() << std::endl;
#endif
    return m_name;
}

void
Document::RemoveLayerCommand::execute()
{
    bool have = false;
    for (int i = 0; i < m_view->getLayerCount(); ++i) {
	if (m_view->getLayer(i) == m_layer) {
	    have = true;
	    break;
	}
    }

    if (!have) { // not there!
	m_layer->setLayerDormant(m_view, true);
	m_added = false;
	return;
    }

    m_view->removeLayer(m_layer);
    m_layer->setLayerDormant(m_view, true);

    m_d->removeFromLayerViewMap(m_layer, m_view);
    m_added = false;
}

void
Document::RemoveLayerCommand::unexecute()
{
    m_view->addLayer(m_layer);
    m_layer->setLayerDormant(m_view, false);

    m_d->addToLayerViewMap(m_layer, m_view);
    m_added = true;
}

void
Document::toXml(QTextStream &out, QString indent, QString extraAttributes) const
{
    out << indent + QString("<data%1%2>\n")
        .arg(extraAttributes == "" ? "" : " ").arg(extraAttributes);

    if (m_mainModel) {

	m_mainModel->toXml(out, indent + "  ", "mainModel=\"true\"");

        PlayParameters *playParameters =
            PlayParameterRepository::getInstance()->getPlayParameters(m_mainModel);
        if (playParameters) {
            playParameters->toXml
                (out, indent + "  ",
                 QString("model=\"%1\"")
                 .arg(XmlExportable::getObjectExportId(m_mainModel)));
        }
    }

    // Models that are not used in a layer that is in a view should
    // not be written.  Get our list of required models first.

    std::set<const Model *> used;

    for (LayerViewMap::const_iterator i = m_layerViewMap.begin();
         i != m_layerViewMap.end(); ++i) {

        if (i->first && !i->second.empty() && i->first->getModel()) {
            used.insert(i->first->getModel());
        }
    }

    std::set<Model *> written;

    for (ModelMap::const_iterator i = m_models.begin();
	 i != m_models.end(); ++i) {

        Model *model = i->first;
	const ModelRecord &rec = i->second;

        if (used.find(model) == used.end()) continue;
        
        // We need an intelligent way to determine which models need
        // to be streamed (i.e. have been edited, or are small) and
        // which should not be (i.e. remain as generated by a
        // transform, and are large).
        //
        // At the moment we can get away with deciding not to stream
        // dense 3d models or writable wave file models, provided they
        // were generated from a transform, because at the moment there
        // is no way to edit those model types so it should be safe to
        // regenerate them.  That won't always work in future though.
        // It would be particularly nice to be able to ask the user,
        // as well as making an intelligent guess.

        bool writeModel = true;
        bool haveDerivation = false;

        if (rec.source && rec.transform.getIdentifier() != "") {
            haveDerivation = true;
        } 

        if (haveDerivation) {
            if (dynamic_cast<const WritableWaveFileModel *>(model)) {
                writeModel = false;
            } else if (dynamic_cast<const DenseThreeDimensionalModel *>(model)) {
                writeModel = false;
            }
        }

        if (writeModel) {
            model->toXml(out, indent + "  ");
            written.insert(model);
        }

	if (haveDerivation) {
            writeBackwardCompatibleDerivation(out, indent + "  ",
                                              model, rec);
	}

        //!!! We should probably own the PlayParameterRepository
        PlayParameters *playParameters =
            PlayParameterRepository::getInstance()->getPlayParameters(model);
        if (playParameters) {
            playParameters->toXml
                (out, indent + "  ",
                 QString("model=\"%1\"")
                 .arg(XmlExportable::getObjectExportId(model)));
        }
    }
	    
    //!!!

    // We should write out the alignment models here.  AlignmentModel
    // needs a toXml that writes out the export IDs of its reference
    // and aligned models, and then streams its path model.  Note that
    // this will only work when the alignment is complete, so we
    // should probably wait for it if it isn't already by this point.

    for (std::set<Model *>::const_iterator i = written.begin();
	 i != written.end(); ++i) {

        const Model *model = *i;
        const AlignmentModel *alignment = model->getAlignment();
        if (!alignment) continue;

        alignment->toXml(out, indent + "  ");
    }

    for (LayerSet::const_iterator i = m_layers.begin();
	 i != m_layers.end(); ++i) {

	(*i)->toXml(out, indent + "  ");
    }

    out << indent + "</data>\n";
}

void
Document::writeBackwardCompatibleDerivation(QTextStream &out, QString indent,
                                            Model *targetModel,
                                            const ModelRecord &rec) const
{
    // There is a lot of redundancy in the XML we output here, because
    // we want it to work with older SV session file reading code as
    // well.
    //
    // Formerly, a transform was described using a derivation element
    // which set out the source and target models, execution context
    // (step size, input channel etc) and transform id, containing a
    // plugin element which set out the transform parameters and so
    // on.  (The plugin element came from a "configurationXml" string
    // obtained from PluginXml.)
    // 
    // This has been replaced by a derivation element setting out the
    // source and target models and input channel, containing a
    // transform element which sets out everything in the Transform.
    //
    // In order to retain compatibility with older SV code, however,
    // we have to write out the same stuff into the derivation as
    // before, and manufacture an appropriate plugin element as well
    // as the transform element.  In order that newer code knows it's
    // dealing with a newer format, we will also write an attribute
    // 'type="transform"' in the derivation element.

    const Transform &transform = rec.transform;

    // Just for reference, this is what we would write if we didn't
    // have to be backward compatible:
    //
    //    out << indent
    //        << QString("<derivation type=\"transform\" source=\"%1\" "
    //                   "model=\"%2\" channel=\"%3\">\n")
    //        .arg(XmlExportable::getObjectExportId(rec.source))
    //        .arg(XmlExportable::getObjectExportId(targetModel))
    //        .arg(rec.channel);
    //
    //    transform.toXml(out, indent + "  ");
    //
    //    out << indent << "</derivation>\n";
    // 
    // Unfortunately, we can't just do that.  So we do this...

    QString extentsAttributes;
    if (transform.getStartTime() != RealTime::zeroTime ||
        transform.getDuration() != RealTime::zeroTime) {
        extentsAttributes = QString("startFrame=\"%1\" duration=\"%2\" ")
            .arg(RealTime::realTime2Frame(transform.getStartTime(),
                                          targetModel->getSampleRate()))
            .arg(RealTime::realTime2Frame(transform.getDuration(),
                                          targetModel->getSampleRate()));
    }
	    
    out << indent;
    out << QString("<derivation type=\"transform\" source=\"%1\" "
                   "model=\"%2\" channel=\"%3\" domain=\"%4\" "
                   "stepSize=\"%5\" blockSize=\"%6\" %7windowType=\"%8\" "
                   "transform=\"%9\">\n")
        .arg(XmlExportable::getObjectExportId(rec.source))
        .arg(XmlExportable::getObjectExportId(targetModel))
        .arg(rec.channel)
        .arg(TransformFactory::getInstance()->getTransformInputDomain
             (transform.getIdentifier()))
        .arg(transform.getStepSize())
        .arg(transform.getBlockSize())
        .arg(extentsAttributes)
        .arg(int(transform.getWindowType()))
        .arg(XmlExportable::encodeEntities(transform.getIdentifier()));

    transform.toXml(out, indent + "  ");
    
    out << indent << "  "
        << TransformFactory::getInstance()->getPluginConfigurationXml(transform);

    out << indent << "</derivation>\n";
}

