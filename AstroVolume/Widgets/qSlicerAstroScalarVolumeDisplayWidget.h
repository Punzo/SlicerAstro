#ifndef __qSlicerAstroScalarVolumeDisplayWidget_h
#define __qSlicerAstroScalarVolumeDisplayWidget_h

// Qt includes
#include <QWidget>

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include <qSlicerWidget.h>

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class vtkMRMLNode;
class vtkMRMLAstroVolumeDisplayNode;
class vtkMRMLAstroVolumeNode;
class qSlicerAstroScalarVolumeDisplayWidgetPrivate;
class vtkImageData;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroScalarVolumeDisplayWidget : public qSlicerWidget
{
  Q_OBJECT
  QVTK_OBJECT
  Q_PROPERTY(bool enableColorTableComboBox READ isColorTableComboBoxEnabled WRITE setColorTableComboBoxEnabled )
  Q_PROPERTY(bool enableMRMLWindowLevelWidget READ isMRMLWindowLevelWidgetEnabled WRITE setMRMLWindowLevelWidgetEnabled )
public:
  /// Constructors
  typedef qSlicerWidget Superclass;
  explicit qSlicerAstroScalarVolumeDisplayWidget(QWidget* parent);
  virtual ~qSlicerAstroScalarVolumeDisplayWidget();

  vtkMRMLAstroVolumeNode* volumeNode()const;
  vtkMRMLAstroVolumeDisplayNode* volumeDisplayNode()const;
  vtkImageData* volumeImageData()const;

  bool isColorTableComboBoxEnabled()const;
  void setColorTableComboBoxEnabled(bool);

  bool isMRMLWindowLevelWidgetEnabled()const;
  void setMRMLWindowLevelWidgetEnabled(bool);

public slots:

  ///
  /// Set the MRML node of interest
  void setMRMLVolumeNode(vtkMRMLAstroVolumeNode* volumeNode);
  void setMRMLVolumeNode(vtkMRMLNode* node);

  void setInterpolate(bool interpolate);
  void setColorNode(vtkMRMLNode* colorNode);

protected slots:
  void updateWidgetFromMRML();
  void updateTransferFunction();

protected:
  void showEvent(QShowEvent * event);
protected:
  QScopedPointer<qSlicerAstroScalarVolumeDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroScalarVolumeDisplayWidget);
  Q_DISABLE_COPY(qSlicerAstroScalarVolumeDisplayWidget);
};

#endif
