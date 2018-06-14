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

#ifndef __qSlicerAstroMaskingModuleWidget_h
#define __qSlicerAstroMaskingModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroMaskingModuleExport.h"

// CTK includes
#include <ctkVTKObject.h>

class qSlicerAstroMaskingModuleWidgetPrivate;
class vtkMRMLAstroMaskingParametersNode;
class vtkMRMLNode;
class vtkSegment;

/// \ingroup SlicerAstro_QtModules_AstroMasking
class Q_SLICERASTRO_QTMODULES_ASTROMASKING_EXPORT qSlicerAstroMaskingModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroMaskingModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroMaskingModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroMaskingParametersNode
  Q_INVOKABLE vtkMRMLAstroMaskingParametersNode* mrmlAstroMaskingParametersNode()const;

public slots:
  /// Slots to apply the masking operation.
  /// It creates an output volume and calls the logic
  void onApply();

protected:
  QScopedPointer<qSlicerAstroMaskingModuleWidgetPrivate> d_ptr;

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

  /// Convert a segmentation to LabelMap volume (a mask).
  /// The LabelMap ID is stored in the MRML parameter node of the module
  /// \return the selected vtkSegment (only one segment can be selected for masking)
  vtkSegment* convertSelectedSegmentToLabelMap();

protected slots:

  /// Set the MRML input node
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML ROI node
  void onInputROIChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML output node
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML segmentation editor node
  void onSegmentEditorNodeModified(vtkObject* sender);

  /// Update widget GUI from MRML parameter node
  void onMRMLAstroMaskingParametersNodeModified();

  /// Set the MRML parameter node
  void setMRMLAstroMaskingParametersNode(vtkMRMLNode* mrmlNode);

  void onEndCloseEvent();
  void onEndImportEvent();
  void onStartImportEvent();

  void onBlankValueChanged();
  void onInsideBlankRegionChanged();
  void onModeChanged();
  void onOperationChanged(QString Operation);
  void onOutsideBlankRegionChanged();
  void onROIFit();
  void onROIVisibilityChanged(bool visible);

  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);

  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroMaskingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroMaskingModuleWidget);
};

#endif
