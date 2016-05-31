#ifndef __qSlicerSmoothingModule_h
#define __qSlicerSmoothingModule_h

// SlicerQt includes
#include "qSlicerLoadableModule.h"

#include "qSlicerSmoothingModuleExport.h"

class qSlicerSmoothingModulePrivate;

/// \ingroup Slicer_QtModules_Smoothing
class Q_SLICER_QTMODULES_SMOOTHING_EXPORT qSlicerSmoothingModule :
  public qSlicerLoadableModule
{
  Q_OBJECT
  Q_INTERFACES(qSlicerLoadableModule);

public:

  typedef qSlicerLoadableModule Superclass;
  explicit qSlicerSmoothingModule(QObject *parent=0);
  virtual ~qSlicerSmoothingModule();

  qSlicerGetTitleMacro(QTMODULE_TITLE);

  /// Return a custom icon for the module
  virtual QIcon icon()const;
  virtual QStringList categories() const;

  virtual QString helpText()const;
  virtual QString acknowledgementText()const;
  virtual QStringList contributors()const;

  virtual QStringList dependencies()const;

protected:
  /// Initialize the module. Register the volumes reader/writer
  virtual void setup();

  /// Create and return the widget representation associated to this module
  virtual qSlicerAbstractModuleRepresentation * createWidgetRepresentation();

  /// Create and return the logic associated to this module
  virtual vtkMRMLAbstractLogic* createLogic();

protected:
  QScopedPointer<qSlicerSmoothingModulePrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerSmoothingModule);
  Q_DISABLE_COPY(qSlicerSmoothingModule);

};

#endif
