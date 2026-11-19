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
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroStatistics
/// Panel of the AstroStatistics module: the data, the region (whole data, a
/// box or a segment of a mask), the quantities, the results of the last
/// calculation and the table of the earlier ones. The calculation is in
/// vtkSlicerAstroStatisticsLogic.
class Q_SLICERASTRO_QTMODULES_ASTROSTATISTICS_EXPORT qSlicerAstroStatisticsModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroStatisticsModuleWidget(QWidget *parent=nullptr);
  ~qSlicerAstroStatisticsModuleWidget() override;

  enum Region { WholeData = 0, Box, Mask };

  void enter() override;
  void exit() override;

  Q_INVOKABLE vtkMRMLAstroStatisticsParametersNode* mrmlAstroStatisticsParametersNode() const;
  /// Text of the results of the last calculation, one "name = value unit"
  /// per line
  Q_INVOKABLE QString resultsText() const;

public slots:
  /// Calculate the statistics of the region (a row of the table)
  void onCalculate();
  /// Region (Region)
  void setRegion(int region);

protected:
  QScopedPointer<qSlicerAstroStatisticsModuleWidgetPrivate> d_ptr;

  void setMRMLScene(vtkMRMLScene*) override;
  void setup() override;
  void initializeParameterNode(bool forceNew = false);
  void initializeSegmentations();

protected slots:
  void setMRMLAstroStatisticsParametersNode(vtkMRMLNode*);
  void onMRMLAstroStatisticsParametersNodeModified();
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onInputROIChanged(vtkMRMLNode* node);
  void onTableNodeChanged(vtkMRMLNode* mrmlNode);
  void onRegionChanged(int index);
  void onQuantityToggled(bool on);
  void onROIFit();
  void onROIVisibilityChanged(bool visible);
  void onMaskSegmentationChanged(vtkMRMLNode* node);
  void onSegmentEditorNodeModified(vtkObject* sender);
  void updateMaskSelector();
  void updateResults();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onStartImportEvent();
  void onEndImportEvent();
  void onEndCloseEvent();
  void onComputationCancelled();

private:
  Q_DECLARE_PRIVATE(qSlicerAstroStatisticsModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroStatisticsModuleWidget);
};

#endif
