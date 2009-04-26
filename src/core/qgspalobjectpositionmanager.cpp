/***************************************************************************
                        qgspalobjectpositionmanager.cpp  -  description
                        ---------------------------------
   begin                : October 2008
   copyright            : (C) 2008 by Marco Hugentobler
   email                : marco dot hugentobler at karto dot baug dot ethz dot ch
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgspalobjectpositionmanager.h"
#include "qgsoverlayobject.h"
#include "qgsrendercontext.h"
#include "qgsvectorlayer.h"
#include "qgsvectoroverlay.h"
#include "pal.h"
#include "label.h"
#include "layer.h"

QgsPALObjectPositionManager::QgsPALObjectPositionManager(): mNumberOfLayers( 0 )
{

}

QgsPALObjectPositionManager::~QgsPALObjectPositionManager()
{

}

void QgsPALObjectPositionManager::addLayer( QgsVectorLayer* vl, QList<QgsVectorOverlay*>& overlays )
{
  if ( overlays.size() < 1 )
  {
    return;
  }

  //set arrangement based on vector type
  pal::Arrangement labelArrangement;
  switch ( vl->geometryType() )
  {
    case QGis::Point:
      labelArrangement = pal::P_POINT;
      break;
    case QGis::Line:
      labelArrangement = pal::P_LINE;
      break;
    case QGis::Polygon:
      labelArrangement = pal::P_HORIZ;
      break;
    default:
      return; //error
  }

  pal::Layer* positionLayer = mPositionEngine.addLayer( QString::number( mNumberOfLayers ).toLocal8Bit().data(), 0, 1000000, labelArrangement, pal::PIXEL, 0.5, true, true, true );
  ++mNumberOfLayers;

  if ( !positionLayer )
  {
    return;
  }

  //register the labeling objects in the layer
  int objectNr = 0;
  QList<QgsVectorOverlay*>::const_iterator overlayIt = overlays.begin();
  for ( ; overlayIt != overlays.end(); ++overlayIt )
  {
    if ( !( *overlayIt ) )
    {
      continue;
    }

    QMap<int, QgsOverlayObject*>* positionObjects = ( *overlayIt )->overlayObjects();
    if ( !positionObjects )
    {
      continue;
    }

    QMap<int, QgsOverlayObject*>::const_iterator objectIt = positionObjects->begin();
    for ( ; objectIt != positionObjects->end(); ++objectIt )
    {
      positionLayer->registerFeature( strdup( QString::number( objectNr ).toAscii().data() ), objectIt.value(), objectIt.value()->width(), objectIt.value()->height() );
      ++objectNr;
    }
  }
}

void QgsPALObjectPositionManager::findObjectPositions( const QgsRenderContext& renderContext, QGis::UnitType unitType )
{
  //trigger label placement
  QgsRectangle viewExtent = renderContext.extent();
  double bbox[4]; bbox[0] = viewExtent.xMinimum(); bbox[1] = viewExtent.yMinimum(); bbox[2] = viewExtent.xMaximum(); bbox[3] = viewExtent.yMaximum();
  pal::PalStat* stat = 0;

  //set map units
  pal::Units mapUnits;
  switch ( unitType )
  {
    case QGis::Meters:
      mapUnits = pal::METER;
      break;

    case QGis::Feet:
      mapUnits = pal::FOOT;
      break;

    case QGis::Degrees:
      mapUnits = pal::DEGREE;
      break;
    default:
      return;
  }
  mPositionEngine.setMapUnit( mapUnits );
  std::list<pal::Label*>* resultLabelList = mPositionEngine.labeller( renderContext.rendererScale(), bbox, &stat, true );
  delete stat;

  //and read the positions back to the overlay objects
  if ( !resultLabelList )
  {
    return;
  }

  QgsOverlayObject* currentOverlayObject = 0;

  std::list<pal::Label*>::iterator labelIt = resultLabelList->begin();
  for ( ; labelIt != resultLabelList->end(); ++labelIt )
  {
    currentOverlayObject = dynamic_cast<QgsOverlayObject*>(( *labelIt )->getGeometry() );
    if ( !currentOverlayObject )
    {
      continue;
    }

    //QGIS takes the coordinates of the middle points
    double x = (( *labelIt )->getX( 0 ) + ( *labelIt )->getX( 1 ) + ( *labelIt )->getX( 2 ) + ( *labelIt )->getX( 3 ) ) / 4;
    double y = (( *labelIt )->getY( 0 ) + ( *labelIt )->getY( 1 ) + ( *labelIt )->getY( 2 ) + ( *labelIt )->getY( 3 ) ) / 4;
    currentOverlayObject->addPosition( QgsPoint( x, y ) );
  }
}

void QgsPALObjectPositionManager::removeLayers()
{
  std::list<pal::Layer*>* layerList = mPositionEngine.getLayers();
  if ( !layerList )
  {
    return;
  }

  //Iterators get invalid if elements are deleted in a std::list
  //Therefore we have to get the layer pointers in a first step and remove them in a second
  QList<pal::Layer*> layersToRemove;
  std::list<pal::Layer*>::iterator layerIt = layerList->begin();
  for ( ; layerIt != layerList->end(); ++layerIt )
  {
    layersToRemove.push_back( *layerIt );
  }

  QList<pal::Layer*>::iterator removeIt = layersToRemove.begin();
  for ( ; removeIt != layersToRemove.end(); ++removeIt )
  {
    mPositionEngine.removeLayer( *removeIt );
  }
}
//Chain, Popmusic tabu chain, Popmusic tabu, Popmusic chain
void QgsPALObjectPositionManager::setPlacementAlgorithm( const QString& algorithmName )
{
  if ( algorithmName == "Popmusic tabu chain" )
  {
    mPositionEngine.setSearch( pal::POPMUSIC_TABU_CHAIN );
  }
  else if ( algorithmName == "Popmusic tabu" )
  {
    mPositionEngine.setSearch( pal::POPMUSIC_TABU );
  }
  else if ( algorithmName == "Popmusic chain" )
  {
    mPositionEngine.setSearch( pal::POPMUSIC_CHAIN );
  }
  else //default is "Chain"
  {
    mPositionEngine.setSearch( pal::CHAIN );
  }
}