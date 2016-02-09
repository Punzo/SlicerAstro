#ifndef __qSlicerSmoothingModuleWidget_h
#define __qSlicerSmoothingModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerSmoothingModuleExport.h"

class qSlicerSmoothingModuleWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLSmoothingParametersNode;

/// \ingroup Slicer_QtModules_Smoothing
class Q_SLICER_QTMODULES_SMOOTHING_EXPORT qSlicerSmoothingModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerSmoothingModuleWidget(QWidget *parent=0);
  virtual ~qSlicerSmoothingModuleWidget();

  /// Get vtkMRMLSmoothingParametersNode
  Q_INVOKABLE vtkMRMLSmoothingParametersNode* mrmlSmoothingParametersNode()const;

public slots:
  void onApply();

protected:
  QScopedPointer<qSlicerSmoothingModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);
  void initializeParameterNode(vtkMRMLScene*);

protected slots:
  void onInputVolumeChanged(vtkMRMLNode*);
  void onOutputVolumeChanged(vtkMRMLNode*);
  void setMRMLSmoothingParametersNode(vtkMRMLNode*);
  void onMRMLSmoothingParametersNodeModified();
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

private:
  Q_DECLARE_PRIVATE(qSlicerSmoothingModuleWidget);
  Q_DISABLE_COPY(qSlicerSmoothingModuleWidget);
};

#endif
