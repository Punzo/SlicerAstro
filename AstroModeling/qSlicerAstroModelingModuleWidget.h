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

#ifndef __qSlicerAstroModelingModuleWidget_h
#define __qSlicerAstroModelingModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroModelingModuleExport.h"

class qSlicerAstroModelingModuleWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLAstroModelingParametersNode;

/// \ingroup Slicer_QtModules_AstroModeling
class Q_SLICER_QTMODULES_ASTROMODELING_EXPORT qSlicerAstroModelingModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroModelingModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroModelingModuleWidget();

  /// Get vtkMRMLAstroModelingParametersNode
  Q_INVOKABLE vtkMRMLAstroModelingParametersNode* mrmlAstroModelingParametersNode()const;

public slots:
  void onApply();

protected:
  QScopedPointer<qSlicerAstroModelingModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);
  void initializeParameterNode(vtkMRMLScene*);

protected slots:
  void onInputVolumeChanged(vtkMRMLNode*);
  void onOutputVolumeChanged(vtkMRMLNode*);
  void setMRMLAstroModelingParametersNode(vtkMRMLNode*);
  void onMRMLAstroModelingParametersNodeModified();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onEndCloseEvent();
  void onComputationStarted();
  void onComputationCancelled();
  void onComputationFinished();
  void updateProgress(int value);
  void onAutoRunChanged(bool value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroModelingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroModelingModuleWidget);
};

#endif
