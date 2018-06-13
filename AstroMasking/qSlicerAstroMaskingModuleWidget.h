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
  void onApply();

protected:
  QScopedPointer<qSlicerAstroMaskingModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);
  void initializeNodes(bool forceNew = false);
  void initializeParameterNode(bool forceNew = false);
  void initializeSegmentations(bool forceNew = false);
  void initializeROINode(bool forceNew = false);
  vtkSegment* convertSelectedSegmentToLabelMap();

protected slots:
  void onBlankValueChanged();
  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void onEndCloseEvent();
  void onEndImportEvent();
  void onInputROIChanged(vtkMRMLNode* node);
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onInsideBlankRegionChanged();
  void onModeChanged();
  void onMRMLAstroMaskingParametersNodeModified();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);
  void onOperationChanged(QString Operation);
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onOutsideBlankRegionChanged();
  void onROIFit();
  void onROIVisibilityChanged(bool visible);
  void onSegmentEditorNodeModified(vtkObject* sender);
  void onStartImportEvent();
  void setMRMLAstroMaskingParametersNode(vtkMRMLNode*);
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroMaskingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroMaskingModuleWidget);
};

#endif
