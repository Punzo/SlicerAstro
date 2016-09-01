#ifndef __qSlicerAstroVolumeDisplayWidget_h
#define __qSlicerAstroVolumeDisplayWidget_h

// Qt includes
#include <QStackedWidget>

// CTK includes
#include <ctkVTKObject.h>

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class vtkMRMLNode;
class qSlicerAstroVolumeDisplayWidgetPrivate;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroVolumeDisplayWidget : public QStackedWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  /// Constructors
  typedef QStackedWidget Superclass;
  explicit qSlicerAstroVolumeDisplayWidget(QWidget* parent=0);
  virtual ~qSlicerAstroVolumeDisplayWidget();

public slots:
  /// Set the MRML node of interest
  void setMRMLVolumeNode(vtkMRMLNode* node);

protected slots:
  /// Internally use in case the current display widget should change when the
  /// volume node changes (typically if the LabelMap attribute is changed)
  void updateFromMRML(vtkObject* volume);
protected:
  QScopedPointer<qSlicerAstroVolumeDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeDisplayWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeDisplayWidget);
};

#endif
