/***************************************************************************
                              qgsformannotationitem.h
                              ------------------------
  begin                : February 26, 2010
  copyright            : (C) 2010 by Marco Hugentobler
  email                : marco dot hugentobler at hugis dot net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsformannotationitem.h"
#include "qgsattributeeditor.h"
#include "qgsfeature.h"
#include "qgslogger.h"
#include "qgsmapcanvas.h"
#include "qgsmaplayerregistry.h"
#include "qgsvectorlayer.h"
#include <QDomElement>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsProxyWidget>
#include <QPainter>
#include <QSettings>
#include <QUiLoader>
#include <QWidget>

QgsFormAnnotationItem::QgsFormAnnotationItem( QgsMapCanvas* canvas, QgsVectorLayer* vlayer, bool hasFeature, int feature ): \
    QgsAnnotationItem( canvas ), mWidgetContainer( 0 ), mDesignerWidget( 0 ), mVectorLayer( vlayer ), \
    mHasAssociatedFeature( hasFeature ), mFeature( feature )
{
  mWidgetContainer = new QGraphicsProxyWidget( this );
  if ( mVectorLayer && mMapCanvas ) //default to the layers edit form
  {
    mDesignerForm = mVectorLayer->editForm();
    QObject::connect( mVectorLayer, SIGNAL( layerModified( bool ) ), this, SLOT( setFeatureForMapPosition() ) );
    QObject::connect( mMapCanvas, SIGNAL( renderComplete( QPainter* ) ), this, SLOT( setFeatureForMapPosition() ) );
    QObject::connect( mMapCanvas, SIGNAL( layersChanged() ), this, SLOT( updateVisibility() ) );
  }

  setFeatureForMapPosition();
}

QgsFormAnnotationItem::~QgsFormAnnotationItem()
{
  delete mDesignerWidget;
}

void QgsFormAnnotationItem::setDesignerForm( const QString& uiFile )
{
  mDesignerForm = uiFile;
  mWidgetContainer->setWidget( 0 );
  delete mDesignerWidget;
  mDesignerWidget = createDesignerWidget( uiFile );
  if ( mDesignerWidget )
  {
    mFrameBackgroundColor = mDesignerWidget->palette().color( QPalette::Window );
    mWidgetContainer->setWidget( mDesignerWidget );
    setFrameSize( preferredFrameSize() );
  }
}

QWidget* QgsFormAnnotationItem::createDesignerWidget( const QString& filePath )
{
  QFile file( filePath );
  if ( !file.open( QFile::ReadOnly ) )
  {
    return 0;
  }

  QUiLoader loader;
  QFileInfo fi( file );
  loader.setWorkingDirectory( fi.dir() );
  QWidget* widget = loader.load( &file, 0 );
  file.close();

  //get feature and set attribute information
  if ( mVectorLayer && mHasAssociatedFeature )
  {
    QgsFeature f;
    if ( mVectorLayer->featureAtId( mFeature, f, false, true ) )
    {
      const QgsFieldMap& fieldMap = mVectorLayer->pendingFields();
      QgsAttributeMap attMap = f.attributeMap();
      QgsAttributeMap::const_iterator attIt = attMap.constBegin();
      for ( ; attIt != attMap.constEnd(); ++attIt )
      {
        QgsFieldMap::const_iterator fieldIt = fieldMap.find( attIt.key() );
        if ( fieldIt != fieldMap.constEnd() )
        {
          QWidget* attWidget = widget->findChild<QWidget*>( fieldIt->name() );
          if ( attWidget )
          {
            QgsAttributeEditor::createAttributeEditor( widget, attWidget, mVectorLayer, attIt.key(), attIt.value() );
          }
        }
      }
    }
  }
  return widget;
}

void QgsFormAnnotationItem::setMapPosition( const QgsPoint& pos )
{
  QgsAnnotationItem::setMapPosition( pos );
  setFeatureForMapPosition();
}

void QgsFormAnnotationItem::paint( QPainter * painter )
{

}

void QgsFormAnnotationItem::paint( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget )
{
  if ( !painter || !mWidgetContainer )
  {
    return;
  }

  drawFrame( painter );
  if ( mMapPositionFixed )
  {
    drawMarkerSymbol( painter );
  }

  mWidgetContainer->setGeometry( QRectF( mOffsetFromReferencePoint.x() + mFrameBorderWidth / 2.0, mOffsetFromReferencePoint.y() \
                                         + mFrameBorderWidth / 2.0, mFrameSize.width() - mFrameBorderWidth, mFrameSize.height() \
                                         - mFrameBorderWidth ) );

  if ( isSelected() )
  {
    drawSelectionBoxes( painter );
  }
}

QSizeF QgsFormAnnotationItem::minimumFrameSize() const
{
  if ( mDesignerWidget )
  {
    return mDesignerWidget->minimumSize();
  }
  else
  {
    return QSizeF( 0, 0 );
  }
}

QSizeF QgsFormAnnotationItem::preferredFrameSize() const
{
  if ( mDesignerWidget )
  {
    return mDesignerWidget->sizeHint();
  }
  else
  {
    return QSizeF( 0, 0 );
  }
}

void QgsFormAnnotationItem::writeXML( QDomDocument& doc ) const
{
  QDomElement documentElem = doc.documentElement();
  if ( documentElem.isNull() )
  {
    return;
  }

  QDomElement formAnnotationElem = doc.createElement( "FormAnnotationItem" );
  if ( mVectorLayer )
  {
    formAnnotationElem.setAttribute( "vectorLayer", mVectorLayer->getLayerID() );
  }
  formAnnotationElem.setAttribute( "hasFeature", mHasAssociatedFeature );
  formAnnotationElem.setAttribute( "feature", mFeature );
  formAnnotationElem.setAttribute( "designerForm", mDesignerForm );
  _writeXML( doc, formAnnotationElem );
  documentElem.appendChild( formAnnotationElem );
}

void QgsFormAnnotationItem::readXML( const QDomDocument& doc, const QDomElement& itemElem )
{
  mVectorLayer = 0;
  if ( itemElem.hasAttribute( "vectorLayer" ) )
  {
    mVectorLayer = dynamic_cast<QgsVectorLayer*>( QgsMapLayerRegistry::instance()->mapLayer( itemElem.attribute( "vectorLayer", "" ) ) );
    if ( mVectorLayer )
    {
      QObject::connect( mVectorLayer, SIGNAL( layerModified( bool ) ), this, SLOT( setFeatureForMapPosition() ) );
      QObject::connect( mMapCanvas, SIGNAL( renderComplete( QPainter* ) ), this, SLOT( setFeatureForMapPosition() ) );
      QObject::connect( mMapCanvas, SIGNAL( layersChanged() ), this, SLOT( updateVisibility() ) );
    }
  }
  mHasAssociatedFeature = itemElem.attribute( "hasFeature", "0" ).toInt();
  mFeature = itemElem.attribute( "feature", "0" ).toInt();
  mDesignerForm = itemElem.attribute( "designerForm", "" );
  QDomElement annotationElem = itemElem.firstChildElement( "AnnotationItem" );
  if ( !annotationElem.isNull() )
  {
    _readXML( doc, annotationElem );
  }

  mDesignerWidget = createDesignerWidget( mDesignerForm );
  if ( mDesignerWidget )
  {
    mFrameBackgroundColor = mDesignerWidget->palette().color( QPalette::Window );
    mWidgetContainer->setWidget( mDesignerWidget );
  }
  updateVisibility();
}

void QgsFormAnnotationItem::setFeatureForMapPosition()
{
  if ( !mVectorLayer || !mMapCanvas )
  {
    return;
  }

  QgsAttributeList noAttributes;
  QSettings settings;
  double identifyValue = settings.value( "/Map/identifyRadius", QGis::DEFAULT_IDENTIFY_RADIUS ).toDouble();
  double halfIdentifyWidth = mMapCanvas->extent().width() / 100 / 2 * identifyValue;
  QgsRectangle searchRect( mMapPosition.x() - halfIdentifyWidth, mMapPosition.y() - halfIdentifyWidth, \
                           mMapPosition.x() + halfIdentifyWidth, mMapPosition.y() + halfIdentifyWidth );
  mVectorLayer->select( noAttributes, searchRect, false, true );

  QgsFeature currentFeature;
  int currentFeatureId = 0;
  bool featureFound = false;

  while ( mVectorLayer->nextFeature( currentFeature ) )
  {
    currentFeatureId = currentFeature.id();
    featureFound = true;
    break;
  }

  mHasAssociatedFeature = featureFound;
  mFeature = currentFeatureId;

  //create new embedded widget
  mWidgetContainer->setWidget( 0 );
  delete mDesignerWidget;
  mDesignerWidget = createDesignerWidget( mDesignerForm );
  if ( mDesignerWidget )
  {
    mFrameBackgroundColor = mDesignerWidget->palette().color( QPalette::Window );
    mWidgetContainer->setWidget( mDesignerWidget );
  }
}

void QgsFormAnnotationItem::updateVisibility()
{
  bool visible = true;
  if ( mVectorLayer && mMapCanvas )
  {
    visible = mMapCanvas->layers().contains( mVectorLayer );
  }
  setVisible( visible );
}



