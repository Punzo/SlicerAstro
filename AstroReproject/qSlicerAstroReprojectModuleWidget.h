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

/// \ingroup SlicerAstro_QtModules_AstroReproject
class Q_SLICERASTRO_QTMODULES_ASTROREPROJECT_EXPORT qSlicerAstroReprojectModuleWidget :
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
  /// Slots to reproject the data.
  /// It creates the output volume and calls the logic.
  void onApply();

protected:
  QScopedPointer<qSlicerAstroReprojectModuleWidgetPrivate> d_ptr;

  /// Initialization of the qvtk connections between MRML nodes
  virtual void setMRMLScene(vtkMRMLScene* scene);

  /// Initialization of MRML nodes
  void initializeNodes(bool forceNew = false);

  /// Initialization of MRML parameter node
  void initializeParameterNode(bool forceNew = false);

protected slots:

  /// Set the MRML input node
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML reference node
  void onReferenceVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML output node
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Update widget GUI from MRML parameter node
  void onMRMLAstroReprojectParametersNodeModified();

  /// Set the MRML parameter node
  void setMRMLAstroReprojectParametersNode(vtkMRMLNode*);

  void onEndCloseEvent();
  void onEndImportEvent();
  void onStartImportEvent();

  void onInterpolationNearestNeighbour(bool toggled);
  void onInterpolationBilinear(bool toggled);
  void onInterpolationBicubic(bool toggled);
  void onReprojectDataChanged(bool toggled);
  void onReprojectRotationChanged(bool toggled);

  void onMRMLSelectionNodeModified(vtkObject* sender);

  void onComputationCancelled();
  void onComputationFinished();
  void onComputationStarted();
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroReprojectModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroReprojectModuleWidget);
};

#endif
