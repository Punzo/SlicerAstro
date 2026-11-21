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

#ifndef __qSlicerAstroModelingModuleWidget_h
#define __qSlicerAstroModelingModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroModelingModuleExport.h"

// CTK includes
#include <ctkVTKObject.h>

class qSlicerAstroModelingModuleWidgetPrivate;
class qSlicerAstroRingPlot;
class vtkMRMLAstroModelingParametersNode;
class vtkMRMLNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroModeling
/// Modeling panel (3DBarolo): setup of the tilted-ring model of the data
/// cube (mask, mode, beam, rings, geometry and kinematics with the free
/// parameters of the fit, advanced options), operations (estimate, create,
/// fit, model of the edited rings) running in the background with progress,
/// cancel and log, and the results (plot and table of the rings, contours,
/// PV slices along the axes with the rotation curve, model and residual
/// cubes). The behaviour is in vtkSlicerAstroModelingLogic.
class Q_SLICERASTRO_QTMODULES_ASTROMODELING_EXPORT qSlicerAstroModelingModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroModelingModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroModelingModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroModelingParametersNode
  Q_INVOKABLE vtkMRMLAstroModelingParametersNode* mrmlAstroModelingParametersNode()const;
  /// Output of the operations of 3DBarolo shown in the log
  Q_INVOKABLE QString outputLog() const;
  Q_INVOKABLE qSlicerAstroRingPlot* ringPlot() const;
  /// The column of the rings table shown in the plot (e.g. "VRot")
  Q_INVOKABLE QString ringParameter() const;

public slots:
  /// The data cube of the model
  void setInputVolume(vtkMRMLNode* node);
  /// Operations of 3DBarolo (in the background; one at a time)
  void estimate();
  void createModel();
  void fit();
  /// The model of the rings of the table (e.g. edited rings)
  void updateModelFromTable();
  void cancelOperation();
  /// The ring parameter shown in the plot (a column of the rings table)
  void setRingParameter(const QString& column);
  /// The plot of the rings in a plot view of the layout
  void openPlotView();
  /// The results in the views (3D, PV slices, contours)
  void showResults();
  /// Save the rings table as CSV; returns true on success (fileName empty:
  /// asks for it)
  bool exportTable(const QString& fileName = QString());

protected:
  QScopedPointer<qSlicerAstroModelingModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);

protected slots:
  void setMRMLAstroModelingParametersNode(vtkMRMLNode*);
  void onParametersNodeModified();
  void onOperationFinished();
  void onProgressTimeout();
  void onTableModified();
  void onSliceNodeModified();
  void onMaskModified();
  void onSelectionNodeModified();
  void onEndCloseEvent();
  void onEndImportEvent();
  void onModeChanged(int mode);
  void onRegionChanged(int region);
  void onMaskSegmentSelectionChanged();
  void onBeamEdited();

private:
  Q_DECLARE_PRIVATE(qSlicerAstroModelingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroModelingModuleWidget);
};

#endif
