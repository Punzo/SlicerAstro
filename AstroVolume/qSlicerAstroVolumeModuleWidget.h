#ifndef __qSlicerAstroVolumeModuleWidget_h
#define __qSlicerAstroVolumeModuleWidget_h

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroVolumeModuleExport.h"

class qSlicerAstroVolumeModuleWidgetPrivate;
class vtkMRMLNode;

/// \ingroup Slicer_QtModules_AstroVolume
class Q_SLICER_QTMODULES_ASTROVOLUME_EXPORT qSlicerAstroVolumeModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT
public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroVolumeModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroVolumeModuleWidget();

protected slots:
  void resetOffset(vtkMRMLNode* node);
  void SetPresets(vtkMRMLNode* node);
  void onROICropDisplayCheckBoxToggled(bool toggle);
  void onVisibilityChanged(bool visibility);
  void onMRMLVolumeRenderingDisplayNodeModified(vtkObject*);
  void onCurrentQualityControlChanged(int);
  void setDisplayROIEnabled(bool);
  void onCropToggled(bool);
  void setDisplayConnection(vtkMRMLNode* node);
  void setNodeConnection(vtkMRMLNode* node);

protected:
  virtual void setup();
  QScopedPointer<qSlicerAstroVolumeModuleWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeModuleWidget);
};

#endif
