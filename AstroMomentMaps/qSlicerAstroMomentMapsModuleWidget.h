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

#ifndef __qSlicerAstroMomentMapsModuleWidget_h
#define __qSlicerAstroMomentMapsModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroMomentMapsModuleExport.h"

// CTK includes
#include <ctkVTKObject.h>

class qSlicerAstroMomentMapsModuleWidgetPrivate;
class vtkMRMLAstroMomentMapsParametersNode;
class vtkMRMLNode;

/// \ingroup SlicerAstro_QtModules_AstroMomentMaps
class Q_SLICERASTRO_QTMODULES_ASTROMOMENTMAPS_EXPORT qSlicerAstroMomentMapsModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroMomentMapsModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroMomentMapsModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroMomentMapsParametersNode
  Q_INVOKABLE vtkMRMLAstroMomentMapsParametersNode* mrmlAstroMomentMapsParametersNode()const;

public slots:
  /// Slots to generate the moment maps.
  /// It creates the output volumes and calls the logic
  void onCalculate();

protected:
  QScopedPointer<qSlicerAstroMomentMapsModuleWidgetPrivate> d_ptr;

  /// Initialization of the qvtk connections between MRML nodes
  virtual void setMRMLScene(vtkMRMLScene*);

  /// Initialization of MRML nodes
  void initializeNodes(bool forceNew = false);

  /// Initialization of MRML parameter node
  void initializeParameterNode(bool forceNew = false);

  /// Initialization of MRML segmentation nodes
  void initializeSegmentations(bool forceNew = false);

  /// Convert a segmentation to LabelMap volume (a mask).
  /// The LabelMap ID is stored in the MRML parameter node of the module
  /// \return Success flag
  bool convertSelectedSegmentToLabelMap();

protected slots:

  /// Set the MRML input node
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML zero moment output node
  void onZeroMomentVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML first moment output node
  void onFirstMomentVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML second moment output node
  void onSecondMomentVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML segmentation editor node
  void onSegmentEditorNodeModified(vtkObject* sender);

  /// Update widget GUI from MRML input node
  void onInputVolumeModified();

  /// Update widget GUI from MRML parameter node
  void onMRMLAstroMomentMapsParametersNodeModified();

  /// Set the MRML parameter node
  void setMRMLAstroMomentMapsParametersNode(vtkMRMLNode*);

  void onEndCloseEvent();
  void onEndImportEvent();
  void onStartImportEvent();

  void onGenerateFirstToggled(bool generate);
  void onGenerateSecondToggled(bool generate);
  void onGenerateZeroToggled(bool generate);
  void onMaskActiveToggled(bool active);
  void onThresholdRangeChanged(double min, double max);
  void onUnitNodeIntensityChanged(vtkObject* sender);
  void onUnitNodeVelocityChanged(vtkObject* sender);
  void onVelocityRangeChanged(double min, double max);

  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);

  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroMomentMapsModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroMomentMapsModuleWidget);
};

#endif
