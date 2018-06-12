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

/// \ingroup Slicer_QtModules_AstroStatistics
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
  void onCalculate();

protected:
  QScopedPointer<qSlicerAstroStatisticsModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);
  void initializeNodes(bool forceNew = false);
  void initializeParameterNode(bool forceNew = false);
  void initializeSegmentations(bool forceNew = false);
  void initializeROINode(bool forceNew = false);
  void initializeTableNode(bool forceNew = false);
  bool convertSelectedSegmentToLabelMap();
  virtual void setup();

protected slots:
  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void onEndCloseEvent();
  void onEndImportEvent();
  void onInputROIChanged(vtkMRMLNode* node);
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onMaxToggled(bool toggled);
  void onMeanToggled(bool toggled);
  void onMedianToggled(bool toggled);
  void onMinToggled(bool toggled);
  void onModeChanged();
  void onMRMLAstroStatisticsParametersNodeModified();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);
  void onNpixelsToggled(bool toggled);
  void onROIFit();
  void onROIVisibilityChanged(bool visible);
  void onSegmentEditorNodeModified(vtkObject* sender);
  void onSumToggled(bool toggled);
  void onStartImportEvent();
  void onStdToggled(bool toggled);
  void onTableNodeChanged(vtkMRMLNode* mrmlNode);
  void onTotalFluxToggled(bool toggled);
  void setMRMLAstroStatisticsParametersNode(vtkMRMLNode*);
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroStatisticsModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroStatisticsModuleWidget);
};

#endif
