#ifndef __qSlicerAstroWelcomeModuleWidget_h
#define __qSlicerAstroWelcomeModuleWidget_h

// CTK includes
#include <ctkPimpl.h>

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"
#include "qSlicerAstroWelcomeModuleExport.h"

class qSlicerAstroWelcomeModuleWidgetPrivate;

/// \ingroup Slicer_QtModules_SlicerAstroWelcome
class Q_SLICER_QTMODULES_ASTROWELCOME_EXPORT qSlicerAstroWelcomeModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroWelcomeModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroWelcomeModuleWidget();


public slots:

  void editApplicationSettings();
  bool loadNonDicomData();
  bool loadRemoteSampleData();
  bool navigateToSlicerWelcom();

protected:
  virtual void setup();

protected slots:
  void loadSource(QWidget*);

protected:
  QScopedPointer<qSlicerAstroWelcomeModuleWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroWelcomeModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroWelcomeModuleWidget);
};

#endif
