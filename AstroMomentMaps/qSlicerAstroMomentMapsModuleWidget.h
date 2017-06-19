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
  and was supported through the European Research Consil grant nr. 291531.

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

/// \ingroup Slicer_QtModules_AstroMomentMaps
class Q_SLICER_QTMODULES_ASTROMOMENTMAPS_EXPORT qSlicerAstroMomentMapsModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroMomentMapsModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroMomentMapsModuleWidget();

  /// Get vtkMRMLAstroMomentMapsParametersNode
  Q_INVOKABLE vtkMRMLAstroMomentMapsParametersNode* mrmlAstroMomentMapsParametersNode()const;

public slots:
  void onCalculate();

protected:
  QScopedPointer<qSlicerAstroMomentMapsModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);
  void initializeParameterNode(vtkMRMLScene*);
  bool convertFirstSegmentToLabelMap();

protected slots:
  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void onEndCloseEvent();
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onFirstMomentVolumeChanged(vtkMRMLNode* mrmlNode);
  void onGenerateFirstToggled(bool generate);
  void onGenerateSecondToggled(bool generate);
  void onGenerateZeroToggled(bool generate);
  void onMaskActiveToggled(bool active);
  void onMRMLAstroMomentMapsParametersNodeModified();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);
  void onSecondMomentVolumeChanged(vtkMRMLNode* mrmlNode);
  void onSegmentEditorNodeModified(vtkObject* sender);
  void onThresholdRangeChanged(double min, double max);
  void onUnitNodeIntensityChanged(vtkObject* sender);
  void onUnitNodeVelocityChanged(vtkObject* sender);
  void onVelocityRangeChanged(double min, double max);
  void onZeroMomentVolumeChanged(vtkMRMLNode* mrmlNode);
  void setMRMLAstroMomentMapsParametersNode(vtkMRMLNode*);
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroMomentMapsModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroMomentMapsModuleWidget);
};

#endif
