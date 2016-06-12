#ifndef __qSlicerAstroSmoothingModule_h
#define __qSlicerAstroSmoothingModule_h

// SlicerQt includes
#include "qSlicerLoadableModule.h"

#include "qSlicerAstroSmoothingModuleExport.h"

class qSlicerAstroSmoothingModulePrivate;

/// \ingroup Slicer_QtModules_AstroSmoothing
class Q_SLICER_QTMODULES_ASTROSMOOTHING_EXPORT qSlicerAstroSmoothingModule :
  public qSlicerLoadableModule
{
  Q_OBJECT
  Q_INTERFACES(qSlicerLoadableModule);

public:

  typedef qSlicerLoadableModule Superclass;
  explicit qSlicerAstroSmoothingModule(QObject *parent=0);
  virtual ~qSlicerAstroSmoothingModule();

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
  QScopedPointer<qSlicerAstroSmoothingModulePrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroSmoothingModule);
  Q_DISABLE_COPY(qSlicerAstroSmoothingModule);

};

#endif
