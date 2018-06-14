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

#ifndef __qSlicerAstroSmoothingModuleWidget_h
#define __qSlicerAstroSmoothingModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroSmoothingModuleExport.h"

class qSlicerAstroSmoothingModuleWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLAstroSmoothingParametersNode;

/// \ingroup SlicerAstro_QtModules_AstroSmoothing
class Q_SLICERASTRO_QTMODULES_ASTROSMOOTHING_EXPORT qSlicerAstroSmoothingModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroSmoothingModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroSmoothingModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroSmoothingParametersNode
  Q_INVOKABLE vtkMRMLAstroSmoothingParametersNode* mrmlAstroSmoothingParametersNode()const;

public slots:
  /// Slots to run smoothing algorithms.
  /// It creates the output volume and calls the logic
  void onApply();

protected:
  QScopedPointer<qSlicerAstroSmoothingModuleWidgetPrivate> d_ptr;

  /// Initialization of the qvtk connections between MRML nodes
  virtual void setMRMLScene(vtkMRMLScene*);

  /// Initialization of MRML nodes
  void initializeNodes(bool forceNew = false);

  /// Initialization of MRML parameter node
  void initializeParameterNode(bool forceNew = false);

  /// Initialization of MRML segmentation nodes
  void initializeSegmentations(bool forceNew = false);

  /// Initialization of MRML camera nodes
  void initializeCameras();

protected slots:

  /// Set the MRML input node
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML output node
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML segmentation editor node
  void onSegmentEditorNodeModified(vtkObject* sender);

  /// Update widget GUI from MRML input node
  void onInputVolumeModified();

  /// Update widget GUI from MRML parameter node
  void onMRMLAstroSmoothingParametersNodeModified();

  /// Set the MRML parameter node
  void setMRMLAstroSmoothingParametersNode(vtkMRMLNode*);

  void onEndCloseEvent();
  void onEndImportEvent();
  void onStartImportEvent();

  void onAccuracyChanged(double value);
  void onAutoRunChanged(bool value);
  void onCurrentFilterChanged(int index);
  void onHardwareChanged(int index);
  void onKChanged(double value);
  void onLinkChanged(bool value);
  void onMasksCommandChanged();
  void onModeChanged();
  void onMRMLCameraNodeModified();
  void onParameterXChanged(double value);
  void onParameterYChanged(double value);
  void onParameterZChanged(double value);
  void onRxChanged(double value);
  void onRyChanged(double value);
  void onRzChanged(double value);
  void onTimeStepChanged(double value);

  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);

  void onComputationCancelled();
  void onComputationFinished();
  void onComputationStarted();
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroSmoothingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroSmoothingModuleWidget);
};

#endif
