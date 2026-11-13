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
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroReproject
/// Reprojection, regridding, resampling and mosaicking: inputs, output grid
/// (celestial and spectral, from a reference, the inputs or manual),
/// resampling methods, resolution (common or target beam) and mosaic
/// options (primary beam and noise weights, weight, sensitivity and Sault
/// outputs). The widgets are built in code (object names for scripts and
/// tests).
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
  /// Reproject the inputs with the parameters (creates the output volumes)
  void onApply();

  /// Manual grid from the first input (north up: no rotation)
  void setManualGridFromInput();
  void setManualGridFromInputNorthUp();
  void setManualSpectralGridFromInput();

  /// Common beam of the inputs as the target beam
  void setTargetBeamFromCommonBeam();

protected:
  QScopedPointer<qSlicerAstroReprojectModuleWidgetPrivate> d_ptr;

  /// Initialization of the qvtk connections between MRML nodes
  virtual void setMRMLScene(vtkMRMLScene* scene);

  /// Initialization of MRML nodes
  void initializeNodes(bool forceNew = false);

  /// Initialization of MRML parameter node
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

  /// Inputs (checked volumes) to the parameters node
  void onInputVolumesChanged();
  /// Noise and primary beam volumes of the inputs table to the parameters node
  void onInputsTableChanged();


private:
  Q_DECLARE_PRIVATE(qSlicerAstroReprojectModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroReprojectModuleWidget);
};

#endif
