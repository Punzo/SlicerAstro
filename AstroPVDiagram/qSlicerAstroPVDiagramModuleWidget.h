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

#ifndef __qSlicerAstroPVDiagramModuleWidget_h
#define __qSlicerAstroPVDiagramModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroPVDiagramModuleExport.h"

// CTK includes
#include <ctkVTKObject.h>

class qSlicerAstroPVDiagramModuleWidgetPrivate;
class vtkMRMLMarkupsCurveNode;
class vtkMRMLAstroPVDiagramParametersNode;
class vtkMRMLNode;

/// \ingroup SlicerAstro_QtModules_AstroPVDiagram
class Q_SLICERASTRO_QTMODULES_ASTROPVDIAGRAM_EXPORT qSlicerAstroPVDiagramModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroPVDiagramModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroPVDiagramModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroPVDiagramParametersNode
  Q_INVOKABLE vtkMRMLAstroPVDiagramParametersNode* mrmlAstroPVDiagramParametersNode()const;

public slots:
  /// Slots to generate the PV diagram.
  /// It creates the output volume and calls the logic
  void generatePVDiagram();

protected:
  QScopedPointer<qSlicerAstroPVDiagramModuleWidgetPrivate> d_ptr;

  /// Initialization of the qvtk connections between MRML nodes
  virtual void setMRMLScene(vtkMRMLScene*);

  /// Initialization of MRML nodes
  void initializeNodes(bool forceNew = false);

  /// Initialization of MRML parameter node
  void initializeParameterNode(bool forceNew = false);

  /// Initialization of MRML moment map node
  void initializeMomentMapNode(bool forceNew = false);

  /// Initialization of MRML fiducial markups node
  /// Line of the PV diagram: Markups open curve (the source points and the
  /// line model of the scenes of the previous versions are converted)
  void initializeLineNode(bool forceNew = false);
  /// Curve type of the line (linear or spline) from the parameter node
  void updateLineCurveType(vtkMRMLMarkupsCurveNode* lineNode);

  /// Initialization of MRML model (line) node

protected slots:

  /// Set the MRML input node
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML fiducial markups node
  void onFiducialsMarkupsChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML model (line) node

  /// Set the MRML moment map input node
  void onMomentMapChanged(vtkMRMLNode* mrmlNode);
  void onSecondMomentMapChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML output node
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Update widget GUI from MRML parameter node
  void onMRMLAstroPVDiagramInterpolationModeModified();

  /// Update widget GUI from MRML parameter node
  void onMRMLAstroPVDiagramParametersNodeModified();

  /// Update widget GUI from MRML fiducial markups node
  void onMRMLSourcePointsNodeMarkupAdded();

  /// Update widget GUI from MRML fiducial markups node
  void onMRMLSourcePointsNodeModified();

  /// Set the MRML parameter node
  void setMRMLAstroPVDiagramParametersNode(vtkMRMLNode*);

  void onEndCloseEvent();
  void onEndImportEvent();
  void onStartImportEvent();

  void onAutoUpdateToggled(bool toggled);
  void onInterpolationNone(bool toggled);
  void onInterpolationSpline(bool toggled);
  void onMRMLSelectionNodeModified(vtkObject* sender);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroPVDiagramModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroPVDiagramModuleWidget);
};

#endif
