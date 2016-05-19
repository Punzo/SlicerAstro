#ifndef __qMRMLSliceAstroControllerWidget_h
#define __qMRMLSliceAstroControllerWidget_h

// qMRMLWidget includes
#include "qMRMLSliceControllerWidget.h"
#include <vtkVersion.h>

class qMRMLSliceAstroControllerWidgetPrivate;
//class vtkMRMLSliceNode;

// Volumes includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

/// \ingroup Slicer_QtModules_AstroVolume
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qMRMLSliceAstroControllerWidget
  : public qMRMLSliceControllerWidget
{
  Q_OBJECT
public:
  /// Superclass typedef
  typedef qMRMLSliceControllerWidget Superclass;

  /// Constructors
  explicit qMRMLSliceAstroControllerWidget(QWidget* parent = 0);
  virtual ~qMRMLSliceAstroControllerWidget();

public slots:

  /// Set slice orientation.
  /// \note Orientation could be either "XY", "XZ", "ZY" or "Reformat".
  //void setSliceOrientation(const QString& orientation);

  /// Set the display of the WCS coordinate on the slice.
  void setWCSDisplay();

protected:
  qMRMLSliceAstroControllerWidget(qMRMLSliceAstroControllerWidgetPrivate* pimpl, QWidget* parent = 0);

private:
  Q_DECLARE_PRIVATE(qMRMLSliceAstroControllerWidget);
  Q_DISABLE_COPY(qMRMLSliceAstroControllerWidget);
};

#endif
