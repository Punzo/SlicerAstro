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

#ifndef __qSlicerAstroReprojectModuleWidget_h
#define __qSlicerAstroReprojectModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroReprojectModuleExport.h"

class qSlicerAstroReprojectModuleWidgetPrivate;
class vtkMRMLAstroReprojectParametersNode;
class vtkMRMLNode;

/// \ingroup Slicer_QtModules_AstroReproject
class Q_SLICER_QTMODULES_ASTROREPROJECT_EXPORT qSlicerAstroReprojectModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroReprojectModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroReprojectModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroReprojectParametersNode
  Q_INVOKABLE vtkMRMLAstroReprojectParametersNode* mrmlAstroReprojectParametersNode()const;

public slots:
  void onApply();

protected:
  QScopedPointer<qSlicerAstroReprojectModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene* scene);
  void initializeNodes(bool forceNew = false);
  void initializeParameterNode(bool forceNew = false);

protected slots:
  void onComputationCancelled();
  void onComputationFinished();
  void onComputationStarted();
  void onEndCloseEvent();
  void onEndImportEvent();
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onInterpolationNearestNeighbour(bool toggled);
  void onInterpolationBilinear(bool toggled);
  void onInterpolationBicubic(bool toggled);
  void onMRMLAstroReprojectParametersNodeModified();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onReferenceVolumeChanged(vtkMRMLNode* mrmlNode);
  void onReprojectDataChanged(bool toggled);
  void onReprojectRotationChanged(bool toggled);
  void onReprojectTimeChanged(bool toggled);
  void onStartImportEvent();
  void updateProgress(int value);
  void setMRMLAstroReprojectParametersNode(vtkMRMLNode*);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroReprojectModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroReprojectModuleWidget);
};

#endif
