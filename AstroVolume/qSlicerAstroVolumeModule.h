#ifndef __qSlicerAstroVolumeModule_h
#define __qSlicerAstroVolumeModule_h

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include "qSlicerLoadableModule.h"

#include "qSlicerAstroVolumeModuleExport.h"

class qSlicerAbstractModuleWidget;
class qSlicerAstroVolumeModulePrivate;
class vtkMRMLSliceLogic;
class vtkObject;

/// \ingroup Slicer_QtModules_AstroVolume
class Q_SLICER_QTMODULES_ASTROVOLUME_EXPORT qSlicerAstroVolumeModule :
  public qSlicerLoadableModule
{
  Q_OBJECT
  QVTK_OBJECT
  Q_INTERFACES(qSlicerLoadableModule);

public:

  typedef qSlicerLoadableModule Superclass;
  qSlicerAstroVolumeModule(QObject *parent=0);
  virtual ~qSlicerAstroVolumeModule();

  virtual QString helpText()const;
  virtual QString acknowledgementText()const;
  virtual QStringList contributors()const;
  virtual QIcon icon()const;
  virtual QStringList categories()const;
  virtual QStringList dependencies()const;
  qSlicerGetTitleMacro(QTMODULE_TITLE);

protected:
  /// Initialize the module. Register the AstroVolume reader/writer
  virtual void setup();

  /// Create and return the widget representation associated to this module
  virtual qSlicerAbstractModuleRepresentation* createWidgetRepresentation();

  /// Create and return the logic associated to this module
  virtual vtkMRMLAbstractLogic* createLogic();

  /// Specify editable node types
  virtual QStringList associatedNodeTypes()const;

  QScopedPointer<qSlicerAstroVolumeModulePrivate> d_ptr;

 protected slots:

  void onMRMLUnitNodeIntensityModified(vtkObject* sender);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeModule);
  Q_DISABLE_COPY(qSlicerAstroVolumeModule);
};

#endif
