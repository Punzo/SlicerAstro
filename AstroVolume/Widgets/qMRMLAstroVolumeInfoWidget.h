#ifndef __qMRMLAstroVolumeInfoWidget_h
#define __qMRMLAstroVolumeInfoWidget_h


// CTK includes
#include <ctkVTKObject.h>

// qMRML includes
#include "qMRMLWidget.h"

// Volumes includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qMRMLAstroVolumeInfoWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLVolumeNode;

/// \ingroup Slicer_QtModules_AstroVolume
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qMRMLAstroVolumeInfoWidget : public qMRMLWidget
{
  Q_OBJECT
  QVTK_OBJECT
public:
  typedef qMRMLWidget Superclass;

  qMRMLAstroVolumeInfoWidget(QWidget *parent=0);
  virtual ~qMRMLAstroVolumeInfoWidget();

  vtkMRMLVolumeNode* volumeNode()const;
  // Depends on the dimension, spacing and origin of the volume
  bool isCentered()const;

  // Disabled by default
  bool isDataTypeEditable()const;

public slots:
  /// Utility function to be connected with generic signals
  void setVolumeNode(vtkMRMLNode *node);
  /// Set the volume node to display
  void setVolumeNode(vtkMRMLVolumeNode *node);

  void setImageSpacing(double*);
  void setImageOrigin(double*);
  void center();
  void setNumberOfScalars(int);

protected slots:
  void updateWidgetFromMRML();

protected:
  QScopedPointer<qMRMLAstroVolumeInfoWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLAstroVolumeInfoWidget);
  Q_DISABLE_COPY(qMRMLAstroVolumeInfoWidget);
};

#endif
