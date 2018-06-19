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

/// \ingroup SlicerAstro_QtModules_AstroModeling
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

  /// Slots to clean the initial parameters of the model.
  void onCleanInitialParameters();

  /// Slots to estimate the initial parameters of the model.
  void onEstimateInitialParameters();

  /// Slots to create a model from initial parameters.
  void onCreate();

  /// Slots to fit the data.
  void onFit();

  /// Slots to run teh computations.
  /// It creates the output volume and calls the logic
  void onApply();

  /// Slots to recalculate the model from parameters in table.
  void onCalculateAndVisualize();

  /// Slots to visualize the model at different intensity level.
  void onVisualize();

protected:
  QScopedPointer<qSlicerAstroModelingModuleWidgetPrivate> d_ptr;

  /// Initialization of the qvtk connections between MRML nodes
  virtual void setMRMLScene(vtkMRMLScene*);

  /// Initialization of MRML nodes
  void initializeNodes(bool forceNew = false);

  /// Initialization of MRML fiducial markups nodes
  void initializeFiducialNodes(bool forceNew = false);

  /// Initialization of MRML parameter node
  void initializeParameterNode(bool forceNew = false);

  /// Initialization of MRML plot nodes
  void initializePlotNodes(bool forceNew = false);

  /// Initialization of MRML segmentation nodes
  void initializeSegmentations(bool forceNew = false);

  /// Initialization of MRML table node
  void initializeTableNode(bool forceNew = false);

  /// Convert a segmentation to LabelMap volume (a mask).
  /// The LabelMap ID is stored in the MRML parameter node of the module
  /// \return Success flag
  bool convertSelectedSegmentToLabelMap();

  /// Initialization of module widgets
  virtual void setup();

protected slots:

  /// Set the MRML input node
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML table node
  void onTableNodeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML output node
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML residual node
  void onResidualVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML segmentation editor node
  void onSegmentEditorNodeModified(vtkObject* sender);

  /// Update widget GUI from MRML parameter node
  void onMRMLAstroModelingParametersNodeModified();

  /// Update widget GUI from MRML slice node
  void onMRMLSliceNodeModified(vtkObject* sender);

  /// Update widget GUI from MRML table node
  void onMRMLTableNodeModified();

  /// Update widget GUI from fiducial markups selection
  void onPlotSelectionChanged(vtkStringArray* mrmlPlotDataIDs,
                              vtkCollection* selectionCol);

  /// Set the MRML parameter node
  void setMRMLAstroModelingParametersNode(vtkMRMLNode*);

  void onMRMLSceneEndImportEvent();
  void onMRMLSceneEndRestoreEvent();
  void onMRMLSceneEndBatchProcessEvent();
  void onMRMLSceneEndCloseEvent();
  void onMRMLSceneStartImportEvent();

  void centerPVOffset();
  void onADRIFTCorrectionChanged(bool toggled);
  void onCloudsColumnDensityChanged(double value);
  void onColumnDensityChanged(double value);
  void onContourLevelChanged(double value);
  void onDistanceChanged(double value);
  void onFittingFunctionChanged(int value);
  void onGreenSliceRotated(double value);
  void onInclinationChanged(double value);
  void onInclinationErrorChanged(double value);
  void onInclinationFitChanged(bool flag);
  void onLayerTypeChanged(int value);
  void onMaskActiveToggled(bool active);
  void onModeChanged();
  void onMRMLGreenSliceRotated();
  void onMRMLYellowSliceRotated();
  void onNormalizeNoneChanged(bool toggled);
  void onNormalizeLocalChanged(bool toggled);
  void onNormalizeAzimChanged(bool toggled);
  void onNumberOfCloundsChanged(double value);
  void onNumberOfRingsChanged(double value);
  void onPositionAngleChanged(double value);
  void onPositionAngleErrorChanged(double value);
  void onPositionAngleFitChanged(bool flag);
  void onRadSepChanged(double value);
  void onRadialVelocityChanged(double value);
  void onRadialVelocityFitChanged(bool flag);
  void onRotationVelocityChanged(double value);
  void onRotationVelocityFitChanged(bool flag);
  void onScaleHeightChanged(double value);
  void onScaleHeightFitChanged(bool flag);
  void onSystemicVelocityChanged(double value);
  void onSystemicVelocityFitChanged(bool flag);
  void onTolleranceChanged(double value);
  void onVelocityDispersionChanged(double value);
  void onVelocityDispersionFitChanged(bool flag);
  void onVerticalVelocityChanged(double value);
  void onVerticalRotationalGradientChanged(double value);
  void onVerticalRotationalGradientHeightChanged(double value);
  void onWeightingFunctionChanged(int flag);
  void onXCenterChanged(double value);
  void onXCenterFitChanged(bool flag);
  void onYCenterChanged(double value);
  void onYCenterFitChanged(bool flag);
  void onYellowSliceRotated(double value);

  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);

  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void onWorkFinished();
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroModelingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroModelingModuleWidget);
};

#endif
