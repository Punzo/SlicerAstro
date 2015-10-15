#ifndef __qSlicerAstroVolumeModuleWidget_h
#define __qSlicerAstroVolumeModuleWidget_h

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

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroVolumeModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroVolumeModuleWidget();

protected slots:
  void resetOffset(vtkMRMLNode* node);
  void SetPresets(vtkMRMLNode* node);
  void onROICropDisplayCheckBoxToggled(bool toggle);

protected:
  virtual void setup();
  QScopedPointer<qSlicerAstroVolumeModuleWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeModuleWidget);
};

#endif
