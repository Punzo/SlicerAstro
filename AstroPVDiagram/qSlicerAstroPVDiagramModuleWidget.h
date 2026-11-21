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
class vtkMRMLAstroPVDiagramParametersNode;
class vtkMRMLNode;

/// \ingroup SlicerAstro_QtModules_AstroPVDiagram
/// PV diagram panel: the path drawn on the moment-0 map of the cube (in the
/// panel or in the views), its shape and width; the PV diagram follows the
/// path (vtkSlicerAstroPVDiagramLogic::UpdatePVDiagram).
class Q_SLICERASTRO_QTMODULES_ASTROPVDIAGRAM_EXPORT qSlicerAstroPVDiagramModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroPVDiagramModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroPVDiagramModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroPVDiagramParametersNode
  Q_INVOKABLE vtkMRMLAstroPVDiagramParametersNode* mrmlAstroPVDiagramParametersNode()const;

public slots:
  /// The data cube of the PV diagram
  void setInputVolume(vtkMRMLNode* node);
  /// Spline (true) or straight segments through the points of the path
  void setInterpolation(bool spline);
  /// Width of the path (pixels)
  void setWidth(int width);
  /// Points of the path placed in the views (persistent place mode) or not
  void setDrawing(bool drawing);
  /// Remove the points of the path
  void clearPath();
  /// The PV diagram along the path now (otherwise it follows the path after
  /// the pending events)
  void updatePVDiagram();

protected:
  QScopedPointer<qSlicerAstroPVDiagramModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);

protected slots:
  void setMRMLAstroPVDiagramParametersNode(vtkMRMLNode*);
  void onParametersNodeModified();
  void onInterpolationModified();
  void onPathModified();
  void onSelectionNodeModified();
  void onInteractionNodeModified();
  void onShowResult();
  void onResultDeleted();
  void onEndCloseEvent();
  void onEndImportEvent();

private:
  Q_DECLARE_PRIVATE(qSlicerAstroPVDiagramModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroPVDiagramModuleWidget);
};

#endif
