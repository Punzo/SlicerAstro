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

#ifndef __qSlicerAstroStatisticsModuleWidget_h
#define __qSlicerAstroStatisticsModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroStatisticsModuleExport.h"

// CTK includes
#include <ctkVTKObject.h>

class qSlicerAstroStatisticsModuleWidgetPrivate;
class vtkMRMLAstroStatisticsParametersNode;
class vtkMRMLNode;

/// \ingroup SlicerAstro_QtModules_AstroStatistics
class Q_SLICERASTRO_QTMODULES_ASTROSTATISTICS_EXPORT qSlicerAstroStatisticsModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroStatisticsModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroStatisticsModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroStatisticsParametersNode
  Q_INVOKABLE vtkMRMLAstroStatisticsParametersNode* mrmlAstroStatisticsParametersNode()const;

public slots:
  /// Slots to calculate the statistics.
  /// It creates the output volumes and calls the logic
  void onCalculate();

protected:
  QScopedPointer<qSlicerAstroStatisticsModuleWidgetPrivate> d_ptr;

  /// Initialization of the qvtk connections between MRML nodes
  virtual void setMRMLScene(vtkMRMLScene*);

  /// Initialization of MRML nodes
  void initializeNodes(bool forceNew = false);

  /// Initialization of MRML parameter node
  void initializeParameterNode(bool forceNew = false);

  /// Initialization of MRML segmentation nodes
  void initializeSegmentations(bool forceNew = false);

  /// Initialization of MRML ROI node
  void initializeROINode(bool forceNew = false);

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

  /// Set the MRML ROI node
  void onInputROIChanged(vtkMRMLNode* node);

  /// Set the MRML segmentation editor node
  void onSegmentEditorNodeModified(vtkObject* sender);

  /// Set the MRML table node
  void onTableNodeChanged(vtkMRMLNode* mrmlNode);

  /// Update widget GUI from MRML parameter node
  void onMRMLAstroStatisticsParametersNodeModified();

  /// Set the MRML parameter node
  void setMRMLAstroStatisticsParametersNode(vtkMRMLNode*);

  void onEndCloseEvent();
  void onEndImportEvent();
  void onStartImportEvent();

  void onMaxToggled(bool toggled);
  void onMeanToggled(bool toggled);
  void onMedianToggled(bool toggled);
  void onMinToggled(bool toggled);
  void onModeChanged();
  void onNpixelsToggled(bool toggled);
  void onROIFit();
  void onROIVisibilityChanged(bool visible);
  void onSumToggled(bool toggled);
  void onStdToggled(bool toggled);
  void onTotalFluxToggled(bool toggled);

  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);

  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroStatisticsModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroStatisticsModuleWidget);
};

#endif
