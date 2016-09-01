#ifndef __qSlicerAstroLabelMapVolumeDisplayWidget_h
#define __qSlicerAstroLabelMapVolumeDisplayWidget_h

// Qt includes
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include <qSlicerWidget.h>

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class vtkMRMLNode;
class vtkMRMLAstroLabelMapVolumeNode;
class vtkMRMLAstroLabelMapVolumeDisplayNode;
class qSlicerAstroLabelMapVolumeDisplayWidgetPrivate;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroLabelMapVolumeDisplayWidget : public qSlicerWidget
{
  Q_OBJECT
  QVTK_OBJECT
public:
  /// Constructors
  typedef qSlicerWidget Superclass;
  explicit qSlicerAstroLabelMapVolumeDisplayWidget(QWidget* parent);
  virtual ~qSlicerAstroLabelMapVolumeDisplayWidget();

  vtkMRMLAstroLabelMapVolumeNode* volumeNode()const;
  vtkMRMLAstroLabelMapVolumeDisplayNode* volumeDisplayNode()const;

  int sliceIntersectionThickness()const;

public slots:

  /// Set the MRML node of interest
  void setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode* volumeNode);
  void setMRMLVolumeNode(vtkMRMLNode* node);

  void setColorNode(vtkMRMLNode* colorNode);

  void setSliceIntersectionThickness(int);

protected slots:
  void updateWidgetFromMRML();

protected:
  QScopedPointer<qSlicerAstroLabelMapVolumeDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroLabelMapVolumeDisplayWidget);
  Q_DISABLE_COPY(qSlicerAstroLabelMapVolumeDisplayWidget);
};

#endif
