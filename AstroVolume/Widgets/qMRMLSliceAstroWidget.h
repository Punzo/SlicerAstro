#ifndef __qMRMLSliceAstroWidget_h
#define __qMRMLSliceAstroWidget_h


// qMRMLWidget includes
#include "qMRMLSliceWidget.h"

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qMRMLSliceAstroWidgetPrivate;
class qMRMLSliceAstroControllerWidget;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qMRMLSliceAstroWidget
    : public qMRMLSliceWidget
{
  Q_OBJECT

public:
  /// Superclass typedef
  typedef qMRMLSliceWidget Superclass;

  /// Constructors
  explicit qMRMLSliceAstroWidget(QWidget* parent = 0);
  virtual ~qMRMLSliceAstroWidget();

protected:
  qMRMLSliceAstroWidget(qMRMLSliceAstroWidgetPrivate* pimpl, QWidget* parent = 0);

private:
  Q_DECLARE_PRIVATE(qMRMLSliceAstroWidget);
  Q_DISABLE_COPY(qMRMLSliceAstroWidget);
};

#endif
