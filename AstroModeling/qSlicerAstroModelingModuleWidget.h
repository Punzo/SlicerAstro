/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

#ifndef __qSlicerAstroModelingModuleWidget_h
#define __qSlicerAstroModelingModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroModelingModuleExport.h"

class qSlicerAstroModelingModuleWidgetPrivate;
class vtkCollection;
class vtkMRMLAstroModelingParametersNode;
class vtkMRMLNode;
class vtkStringArray;

/// \ingroup Slicer_QtModules_AstroModeling
class Q_SLICERASTRO_QTMODULES_ASTROMODELING_EXPORT qSlicerAstroModelingModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroModelingModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroModelingModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroModelingParametersNode
  Q_INVOKABLE vtkMRMLAstroModelingParametersNode* mrmlAstroModelingParametersNode()const;

public slots:
  void onCleanInitialParameters();
  void onEstimateInitialParameters();
  void onCreate();
  void onFit();
  void onApply();
  void onCalculateAndVisualize();
  void onVisualize();

protected:
  QScopedPointer<qSlicerAstroModelingModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene* scene);
  void initializeNodes(bool forceNew = false);
  void initializeFiducialNodes(bool forceNew = false);
  void initializeParameterNode(bool forceNew = false);
  void initializePlotNodes(bool forceNew = false);
  void initializeSegmentations(bool forceNew = false);
  void initializeTableNode(bool forceNew = false);
  bool convertSelectedSegmentToLabelMap();
  virtual void setup();

protected slots:
  void centerPVOffset();
  void onCloudsColumnDensityChanged(double value);
  void onColumnDensityChanged(double value);
  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void onContourLevelChanged(double value);
  void onDistanceChanged(double value);
  void onFittingFunctionChanged(int value);
  void onGreenSliceRotated(double value);
  void onInclinationChanged(double value);
  void onInclinationErrorChanged(double value);
  void onInclinationFitChanged(bool flag);
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onLayerTypeChanged(int value);
  void onMaskActiveToggled(bool active);
  void onModeChanged();
  void onMRMLAstroModelingParametersNodeModified();
  void onMRMLGreenSliceRotated();
  void onMRMLSceneEndImportEvent();
  void onMRMLSceneEndRestoreEvent();
  void onMRMLSceneEndBatchProcessEvent();
  void onMRMLSceneEndCloseEvent();
  void onMRMLSceneStartImportEvent();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);
  void onMRMLSliceNodeModified(vtkObject* sender);
  void onMRMLTableNodeModified();
  void onMRMLYellowSliceRotated();
  void onNormalizeNoneChanged(bool toggled);
  void onNormalizeLocalChanged(bool toggled);
  void onNormalizeAzimChanged(bool toggled);
  void onNumberOfCloundsChanged(double value);
  void onNumberOfRingsChanged(double value);
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onPlotSelectionChanged(vtkStringArray* mrmlPlotDataIDs, vtkCollection* selectionCol);
  void onPositionAngleChanged(double value);
  void onPositionAngleErrorChanged(double value);
  void onPositionAngleFitChanged(bool flag);
  void onRadSepChanged(double value);
  void onResidualVolumeChanged(vtkMRMLNode* mrmlNode);
  void onRadialVelocityChanged(double value);
  void onRadialVelocityFitChanged(bool flag);
  void onRotationVelocityChanged(double value);
  void onRotationVelocityFitChanged(bool flag);
  void onScaleHeightChanged(double value);
  void onScaleHeightFitChanged(bool flag);
  void onSegmentEditorNodeModified(vtkObject* sender);
  void onSystemicVelocityChanged(double value);
  void onSystemicVelocityFitChanged(bool flag);
  void onTableNodeChanged(vtkMRMLNode* mrmlNode);
  void onVelocityDispersionChanged(double value);
  void onVelocityDispersionFitChanged(bool flag);
  void onWeightingFunctionChanged(int flag);
  void onWorkFinished();
  void onXCenterChanged(double value);
  void onXCenterFitChanged(bool flag);
  void onYCenterChanged(double value);
  void onYCenterFitChanged(bool flag);
  void onYellowSliceRotated(double value);
  void setMRMLAstroModelingParametersNode(vtkMRMLNode*);
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroModelingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroModelingModuleWidget);
};

#endif
