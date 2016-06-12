#ifndef __qSlicerAstroSmoothingModuleWidget_h
#define __qSlicerAstroSmoothingModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroSmoothingModuleExport.h"

class qSlicerAstroSmoothingModuleWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLAstroSmoothingParametersNode;

/// \ingroup Slicer_QtModules_AstroSmoothing
class Q_SLICER_QTMODULES_ASTROSMOOTHING_EXPORT qSlicerAstroSmoothingModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroSmoothingModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroSmoothingModuleWidget();

  /// Get vtkMRMLAstroSmoothingParametersNode
  Q_INVOKABLE vtkMRMLAstroSmoothingParametersNode* mrmlAstroSmoothingParametersNode()const;

public slots:
  void onApply();

protected:
  QScopedPointer<qSlicerAstroSmoothingModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);
  void initializeParameterNode(vtkMRMLScene*);

protected slots:
  void onInputVolumeChanged(vtkMRMLNode*);
  void onOutputVolumeChanged(vtkMRMLNode*);
  void setMRMLAstroSmoothingParametersNode(vtkMRMLNode*);
  void onMRMLAstroSmoothingParametersNodeModified();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onModeChanged();
  void onEndCloseEvent();
  void onCurrentFilterChanged(int index);
  void onKChanged(double value);
  void onTimeStepChanged(double value);
  void onParameterXChanged(double value);
  void onParameterYChanged(double value);
  void onParameterZChanged(double value);
  void onAccuracyChanged(double value);
  void onRxChanged(double value);
  void onRyChanged(double value);
  void onRzChanged(double value);
  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void updateProgress(int value);
  void onHardwareChanged(int index);
  void onLinkChanged(bool value);
  void onAutoRunChanged(bool value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroSmoothingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroSmoothingModuleWidget);
};

#endif
