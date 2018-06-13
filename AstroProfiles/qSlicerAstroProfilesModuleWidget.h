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

#ifndef __qSlicerAstroProfilesModuleWidget_h
#define __qSlicerAstroProfilesModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroProfilesModuleExport.h"

// CTK includes
#include <ctkVTKObject.h>

class qSlicerAstroProfilesModuleWidgetPrivate;
class vtkMRMLAstroProfilesParametersNode;
class vtkMRMLNode;

/// \ingroup SlicerAstro_QtModules_AstroProfiles
class Q_SLICERASTRO_QTMODULES_ASTROPROFILES_EXPORT qSlicerAstroProfilesModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroProfilesModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroProfilesModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroProfilesParametersNode
  Q_INVOKABLE vtkMRMLAstroProfilesParametersNode* mrmlAstroProfilesParametersNode()const;

public slots:
  void onCalculate();

protected:
  QScopedPointer<qSlicerAstroProfilesModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene* scene);
  void initializeNodes(bool forceNew = false);
  void initializeParameterNode(bool forceNew = false);
  void initializePlotNodes(bool forceNew = false);
  void initializeSegmentations(bool forceNew = false);
  bool convertSelectedSegmentToLabelMap();

protected slots:
  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void onEndCloseEvent();
  void onEndImportEvent();
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onInputVolumeModified();
  void onMaskActiveToggled(bool active);
  void onMRMLAstroProfilesParametersNodeModified();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);
  void onProfileVolumeChanged(vtkMRMLNode* mrmlNode);
  void onSegmentEditorNodeModified(vtkObject* sender);
  void onStartImportEvent();
  void onThresholdRangeChanged(double min, double max);
  void onUnitNodeIntensityChanged(vtkObject* sender);
  void onUnitNodeVelocityChanged(vtkObject* sender);
  void onVelocityRangeChanged(double min, double max);
  void setMRMLAstroProfilesParametersNode(vtkMRMLNode*);
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroProfilesModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroProfilesModuleWidget);
};

#endif
