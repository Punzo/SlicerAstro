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
  void generatePVDiagram();

protected:
  QScopedPointer<qSlicerAstroPVDiagramModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);
  void initializeNodes(bool forceNew = false);
  void initializeParameterNode(bool forceNew = false);
  void initializeMomentMapNode(bool forceNew = false);
  void initializeFiducialsMarkupsNode(bool forceNew = false);
  void initializeLineModelNode(bool forceNew = false);

protected slots:
  void onAutoUpdateToggled(bool toggled);
  void onEndCloseEvent();
  void onEndImportEvent();
  void onFiducialsMarkupsChanged(vtkMRMLNode* mrmlNode);
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onInterpolationNone(bool toggled);
  void onInterpolationSpline(bool toggled);
  void onModelChanged(vtkMRMLNode* mrmlNode);
  void onMomentMapChanged(vtkMRMLNode* mrmlNode);
  void onMRMLAstroPVDiagramInterpolationModeModified();
  void onMRMLAstroPVDiagramParametersNodeModified();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSourcePointsNodeMarkupAdded();
  void onMRMLSourcePointsNodeModified();
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onStartImportEvent();
  void setMRMLAstroPVDiagramParametersNode(vtkMRMLNode*);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroPVDiagramModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroPVDiagramModuleWidget);
};

#endif
