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

// CTK includes
#include <ctkVTKObject.h>

class qSlicerAstroReprojectModuleWidgetPrivate;
class vtkMRMLAstroReprojectParametersNode;
class vtkMRMLNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroReproject
/// Reprojection, regridding, resampling and mosaicking panel: the inputs
/// (one, or several for a mosaic), the output grid (celestial and spectral:
/// of a reference, of the inputs or manual), the resampling methods, the
/// resolution (common or target beam) and the mosaic options (primary beam
/// and noise weights, weight, sensitivity and Sault outputs). Each choice
/// is explained in the panel. The reprojection runs in the background
/// (progress and Cancel in the run button); the behaviour is in
/// vtkSlicerAstroReprojectLogic.
class Q_SLICERASTRO_QTMODULES_ASTROREPROJECT_EXPORT qSlicerAstroReprojectModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroReprojectModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroReprojectModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroReprojectParametersNode
  Q_INVOKABLE vtkMRMLAstroReprojectParametersNode* mrmlAstroReprojectParametersNode()const;

public slots:
  /// Reproject the inputs with the parameters (in the background; creates
  /// the output volumes)
  void onApply();
  /// End of a reprojection started by onApply (GUI thread)
  void finishReprojection(bool success);

  /// The only input (nullptr: none)
  void setInputVolume(vtkMRMLNode* node);
  /// Add an input (mosaic) or remove one
  void addInputVolume(vtkMRMLNode* node);
  void removeInputVolume(const QString& nodeID);

  /// Manual grid from the first input (north up: no rotation)
  void setManualGridFromInput();
  void setManualGridFromInputNorthUp();
  void setManualSpectralGridFromInput();

  /// Common beam of the inputs as the target beam
  void setTargetBeamFromCommonBeam();

  /// The output in place of the input in the views (again: the input back)
  void showResult();

protected:
  QScopedPointer<qSlicerAstroReprojectModuleWidgetPrivate> d_ptr;

  /// Initialization of the qvtk connections between MRML nodes
  virtual void setMRMLScene(vtkMRMLScene* scene);

  /// The parameters node of the scene (created if needed)
  void initializeParameterNode(bool forceNew = false);

protected slots:
  /// Update widget GUI from MRML parameter node
  void onMRMLAstroReprojectParametersNodeModified();

  /// Set the MRML parameter node
  void setMRMLAstroReprojectParametersNode(vtkMRMLNode*);

  void onEndCloseEvent();
  void onEndImportEvent();
  void onStartImportEvent();

  void onMRMLSelectionNodeModified(vtkObject* sender);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroReprojectModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroReprojectModuleWidget);
};

#endif
