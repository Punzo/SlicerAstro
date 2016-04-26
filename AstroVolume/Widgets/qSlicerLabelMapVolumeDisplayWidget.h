#ifndef __qSlicerLabelMapVolumeDisplayWidget_h
#define __qSlicerLabelMapVolumeDisplayWidget_h

// Qt includes
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include <qSlicerWidget.h>

#include "qSlicerVolumesModuleWidgetsExport.h"

class vtkMRMLNode;
class vtkMRMLAstroLabelMapVolumeNode;
class vtkMRMLAstroLabelMapVolumeDisplayNode;
class qSlicerLabelMapVolumeDisplayWidgetPrivate;

/// \ingroup Slicer_QtModules_Volumes
class Q_SLICER_QTMODULES_VOLUMES_WIDGETS_EXPORT qSlicerLabelMapVolumeDisplayWidget : public qSlicerWidget
{
  Q_OBJECT
  QVTK_OBJECT
public:
  /// Constructors
  typedef qSlicerWidget Superclass;
  explicit qSlicerLabelMapVolumeDisplayWidget(QWidget* parent);
  virtual ~qSlicerLabelMapVolumeDisplayWidget();

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
  QScopedPointer<qSlicerLabelMapVolumeDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerLabelMapVolumeDisplayWidget);
  Q_DISABLE_COPY(qSlicerLabelMapVolumeDisplayWidget);
};

#endif
