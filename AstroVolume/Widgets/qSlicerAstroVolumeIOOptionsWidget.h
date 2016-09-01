#ifndef __qSlicerAstroVolumeIOOptionsWidget_h
#define __qSlicerAstroVolumeIOOptionsWidget_h

// CTK includes
#include <ctkPimpl.h>

// SlicerQt includes
#include "qSlicerIOOptionsWidget.h"

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class qSlicerAstroVolumeIOOptionsWidgetPrivate;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroVolumeIOOptionsWidget :
  public qSlicerIOOptionsWidget
{
  Q_OBJECT
public:
  qSlicerAstroVolumeIOOptionsWidget(QWidget *parent=0);
  virtual ~qSlicerAstroVolumeIOOptionsWidget();

public slots:
  virtual void setFileName(const QString& fileName);
  virtual void setFileNames(const QStringList& fileNames);

protected slots:
  /// Update the name, labelmap, center, singleFile, discardOrientation,
  /// colorNodeID properties
  void updateProperties();
  /// Update the color node selection to the default label map
  /// or volume color node depending on the label map checkbox state.
  void updateColorSelector();

private:
  Q_DECLARE_PRIVATE_D(qGetPtrHelper(qSlicerIOOptions::d_ptr), qSlicerAstroVolumeIOOptionsWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeIOOptionsWidget);
};

#endif
