#ifndef __qSlicerAstroWelcomeModule_h
#define __qSlicerAstroWelcomeModule_h

// CTK includes
#include <ctkPimpl.h>

// SlicerQt includes
#include "qSlicerLoadableModule.h"
#include "qSlicerAstroWelcomeModuleExport.h"

class qSlicerAbstractModuleWidget;
class qSlicerAstroWelcomeModulePrivate;

/// \ingroup Slicer_QtModules_SlicerAstroWelcome
class Q_SLICER_QTMODULES_ASTROWELCOME_EXPORT qSlicerAstroWelcomeModule :
  public qSlicerLoadableModule
{
  Q_OBJECT
  Q_INTERFACES(qSlicerLoadableModule);

public:

  typedef qSlicerLoadableModule Superclass;
  qSlicerAstroWelcomeModule(QObject *parent=0);
  virtual ~qSlicerAstroWelcomeModule();

  qSlicerGetTitleMacro(QTMODULE_TITLE);

  virtual QStringList categories()const;
  virtual QIcon icon()const;

  /// Help to use the module
  virtual QString helpText()const;
  virtual QString acknowledgementText()const;
  virtual QStringList contributors()const;

protected:

  /// Create and return the widget representation associated to this module
  virtual qSlicerAbstractModuleRepresentation * createWidgetRepresentation();

  /// Create and return the logic associated to this module
  virtual vtkMRMLAbstractLogic* createLogic();

protected:
  QScopedPointer<qSlicerAstroWelcomeModulePrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroWelcomeModule);
  Q_DISABLE_COPY(qSlicerAstroWelcomeModule);
};

#endif
