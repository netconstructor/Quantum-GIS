#include <cmath>

#include <qtoolbutton.h>
#include <qcombobox.h>
#include <qfiledialog.h>
#include <qframe.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>

#include "datapointacetate.h"
#include "qgspointdialog.h"
#include "mapcoordsdialog.h"
#include "qgsleastsquares.h"

#include "zoom_in.xpm"
#include "zoom_out.xpm"
#include "pan.xpm"
#include "add_point.xpm"

QgsPointDialog::QgsPointDialog() {
  
}


QgsPointDialog::QgsPointDialog(QgsRasterLayer* layer, const QString& worldfile,
			       QWidget* parent, const char* name, 
			       bool modal, WFlags fl) 
  : QgsPointDialogBase(parent, name, modal, fl), 
    mLayer(layer), mWorldfile(worldfile), mCursor(NULL) {
  
  // set up the canvas
  QHBoxLayout* layout = new QHBoxLayout(canvasFrame);
  layout->setAutoAdd(true);
  mCanvas = new QgsMapCanvas(canvasFrame, "georefCanvas");
  mCanvas->setBackgroundColor(Qt::white);
  mCanvas->setMinimumWidth(400);
  mCanvas->freeze(true);
  mCanvas->addLayer(mLayer);
  tbnAddPoint->setOn(true);
  
  // load previously added points
  QFile pointFile(mWorldfile + ".points");
  if (pointFile.open(IO_ReadOnly)) {
    QTextStream points(&pointFile);
    QString tmp;
    points>>tmp>>tmp>>tmp>>tmp;
    while (!points.atEnd()) {
      double mapX, mapY, pixelX, pixelY;
      points>>mapX>>mapY>>pixelX>>pixelY;
      QgsPoint mapCoords(mapX, mapY);
      QgsPoint pixelCoords(pixelX, pixelY);
      addPoint(pixelCoords, mapCoords);
    }
  }
  
  mCanvas->setExtent(mLayer->extent());
  mCanvas->freeze(false);
  connect(mCanvas, SIGNAL(xyClickCoordinates(QgsPoint&)),
	  this, SLOT(handleCanvasClick(QgsPoint&)));
  leSelectWorldFile->setText(worldfile);
}


QgsPointDialog::~QgsPointDialog() {
  
}


void QgsPointDialog::handleCanvasClick(QgsPoint& pixelCoords) {
  if (tbnAddPoint->state() == QButton::On)
    showCoordDialog(pixelCoords);
  else if (tbnDeletePoint->state() == QButton::On)
    deleteDataPoint(pixelCoords);
}


void QgsPointDialog::addPoint(const QgsPoint& pixelCoords, 
			      const QgsPoint& mapCoords) {
  mPixelCoords.push_back(pixelCoords);
  mMapCoords.push_back(mapCoords);
  static int acetateCounter = 0;
  mAcetateIDs.push_back(QString("%1").arg(++acetateCounter));
  mCanvas->addAcetateObject(mAcetateIDs[mAcetateIDs.size() - 1],
			    new DataPointAcetate(pixelCoords, mapCoords));
  mCanvas->refresh();
}


void QgsPointDialog::pbnCancel_clicked() {
  delete mCanvas;
  delete mLayer;
  reject();
}


void QgsPointDialog::pbnGenerateWorldFile_clicked() {
  if (generateWorldFile()) {
    delete mCanvas;
    delete mLayer;
    accept();
  }
}


void QgsPointDialog::pbnGenerateAndLoad_clicked() {
  if (generateWorldFile()) {
    delete mCanvas;
    emit loadLayer(mLayer->source());
    delete mLayer;
    accept();
  }
}


void QgsPointDialog::pbnSelectWorldFile_clicked() {
  QString filename = 
    QFileDialog::getSaveFileName(".",
				 NULL,
				 this,
				 "Save world file"
				 "Choose a name for the world file");
  leSelectWorldFile->setText(filename);
}


bool QgsPointDialog::generateWorldFile() {
  QgsPoint origin(0, 0);
  double pixelSize = 1;
  double rotation = 0;
  
  // compute the parameters using the least squares method 
  // (might throw std::domain_error)
  try {
    if (cmbTransformType->currentItem() == 0)
      QgsLeastSquares::linear(mMapCoords, mPixelCoords, origin, pixelSize);
    else if (cmbTransformType->currentItem() == 1) {
      QMessageBox::critical(this, "Not implemented!",
			    "A Helmert transform requires a rotation of the "
			    "original raster file. This is not yet "
			    "supported.");
      return false;
      /*
	QgsLeastSquares::helmert(mMapCoords, mPixelCoords, 
	origin, pixelSize, rotation);
      */
    }
    else if (cmbTransformType->currentItem() == 2) {
      QMessageBox::critical(this, "Not implemented!",
			    "An affine transform requires changing the "
			    "original raster file. This is not yet "
			    "supported.");
      return false;
    }
  }
  catch (std::domain_error& e) {
    QMessageBox::critical(this, "Error", QString(e.what()));
    return false;
  }

  std::cerr<<"================="<<std::endl
	   <<pixelSize<<std::endl
	   <<0<<std::endl
	   <<0<<std::endl
	   <<-pixelSize<<std::endl
	   <<origin.x()<<std::endl
	   <<origin.y()<<std::endl
	   <<"================="<<std::endl
	   <<"ROTATION: "<<(rotation*180/3.14159265)<<std::endl;
  
  // write the world file
  QFile file(leSelectWorldFile->text());
  if (!file.open(IO_WriteOnly)) {
    QMessageBox::critical(this, "Error", 
			  "Could not write to " + leSelectWorldFile->text());
    return false;
  }
  QTextStream stream(&file);
  stream<<pixelSize<<endl
	<<0<<endl
	<<0<<endl
	<<-pixelSize<<endl
	<<origin.x()<<endl
	<<origin.y()<<endl;  
  
  // write the data points in case we need them later
  QFile pointFile(leSelectWorldFile->text() + ".points");
  if (pointFile.open(IO_WriteOnly)) {
    QTextStream points(&pointFile);
    points<<"mapX\tmapY\tpixelX\tpixelY"<<endl;
    for (int i = 0; i < mMapCoords.size(); ++i) {
      points<<(QString("%1\t%2\t%3\t%4").
	       arg(mMapCoords[i].x()).arg(mMapCoords[i].y()).
	       arg(mPixelCoords[i].x()).arg(mPixelCoords[i].y()))<<endl;
    }
  }
  return true;
}


void QgsPointDialog::tbnZoomIn_changed(int state) {
  if (state == QButton::On) {
    tbnZoomOut->setOn(false);
    tbnPan->setOn(false);
    tbnAddPoint->setOn(false);
    tbnDeletePoint->setOn(false);
    mCanvas->setMapTool(QGis::ZoomIn);
    delete mCursor;
    QPixmap pix((const char **)zoom_in2);
    mCursor = new QCursor(pix, 7, 7);
    mCanvas->setCursor(*mCursor);
  }
}


void QgsPointDialog::tbnZoomOut_changed(int state) {
  if (state == QButton::On) {
    tbnZoomIn->setOn(false);
    tbnPan->setOn(false);
    tbnAddPoint->setOn(false);
    tbnDeletePoint->setOn(false);
    mCanvas->setMapTool(QGis::ZoomOut);
    delete mCursor;
    QPixmap pix((const char **)zoom_out);
    mCursor = new QCursor(pix, 7, 7);
    mCanvas->setCursor(*mCursor);
  }
}


void QgsPointDialog::tbnZoomToLayer_clicked() {
  mCanvas->setExtent(mLayer->extent());
  mCanvas->refresh();
}


void QgsPointDialog::tbnPan_changed(int state) {
  if (state == QButton::On) {
    tbnZoomIn->setOn(false);
    tbnZoomOut->setOn(false);
    tbnAddPoint->setOn(false);
    tbnDeletePoint->setOn(false);
    mCanvas->setMapTool(QGis::Pan);
    delete mCursor;
    QPixmap pix((const char **)pan);
    mCursor = new QCursor(pix, 7, 7);
    mCanvas->setCursor(*mCursor);
  }
}


void QgsPointDialog::tbnAddPoint_changed(int state) {
  if (state == QButton::On) {
    tbnZoomIn->setOn(false);
    tbnZoomOut->setOn(false);
    tbnPan->setOn(false);
    tbnDeletePoint->setOn(false);
    mCanvas->setMapTool(QGis::EmitPoint);
    delete mCursor;
    QPixmap pix((const char **)add_point);
    mCursor = new QCursor(pix, 7, 7);
    mCanvas->setCursor(*mCursor);
  }
}


void QgsPointDialog::tbnDeletePoint_changed(int state) {
  if (state == QButton::On) {
    tbnZoomIn->setOn(false);
    tbnZoomOut->setOn(false);
    tbnPan->setOn(false);
    tbnAddPoint->setOn(false);
    mCanvas->setMapTool(QGis::EmitPoint);
  }
}


void QgsPointDialog::showCoordDialog(QgsPoint& pixelCoords) {
  MapCoordsDialog* mcd = new MapCoordsDialog(pixelCoords, this, NULL, true);
  connect(mcd, SIGNAL(pointAdded(const QgsPoint&, const QgsPoint&)),
	  this, SLOT(addPoint(const QgsPoint&, const QgsPoint&)));
  mcd->show();
}


void QgsPointDialog::deleteDataPoint(QgsPoint& pixelCoords) {
  std::vector<QgsPoint>::iterator pixIter = mPixelCoords.begin();
  std::vector<QgsPoint>::iterator mapIter = mMapCoords.begin();
  std::vector<QString>::iterator idIter = mAcetateIDs.begin();

  for ( ; pixIter != mPixelCoords.end(); ++pixIter, ++mapIter, ++idIter) {
    if (std::sqrt(std::pow(pixIter->x() - pixelCoords.x(), 2) +
		  std::pow(pixIter->y() - pixelCoords.y(), 2)) < 
	5 * mCanvas->mupp()) {
      mCanvas->removeAcetateObject(*idIter);
      mAcetateIDs.erase(idIter);
      mPixelCoords.erase(pixIter);
      mMapCoords.erase(mapIter);
      mCanvas->refresh();
      break;
    }
  }
}
