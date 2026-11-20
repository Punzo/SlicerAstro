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

#ifndef __qSlicerAstroSmoothingModuleWidget_h
#define __qSlicerAstroSmoothingModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroSmoothingModuleExport.h"

class qSlicerAstroSmoothingModuleWidgetPrivate;
class vtkMRMLAstroVolumeNode;
class vtkMRMLNode;
class vtkMRMLAstroSmoothingParametersNode;

/// \ingroup SlicerAstro_QtModules_AstroSmoothing
class Q_SLICERASTRO_QTMODULES_ASTROSMOOTHING_EXPORT qSlicerAstroSmoothingModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroSmoothingModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroSmoothingModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroSmoothingParametersNode
  Q_INVOKABLE vtkMRMLAstroSmoothingParametersNode* mrmlAstroSmoothingParametersNode()const;

public slots:
  /// Smooth the input data (new output volume)
  void onApply();
  /// End of a smoothing started by onApply (GUI thread)
  void finishSmoothing(bool success, vtkMRMLAstroVolumeNode* inputVolume, vtkMRMLAstroVolumeNode* outputVolume);

protected:
  QScopedPointer<qSlicerAstroSmoothingModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);

  void initializeNodes(bool forceNew = false);
  void initializeParameterNode(bool forceNew = false);
  void initializeSegmentations(bool forceNew = false);

protected slots:
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onOutputVolumeChanged(vtkMRMLNode* mrmlNode);
  void onInputVolumeModified();
  void onMRMLAstroSmoothingParametersNodeModified();
  void setMRMLAstroSmoothingParametersNode(vtkMRMLNode*);
  void onEndCloseEvent();
  void onEndImportEvent();
  void onStartImportEvent();

  /// The user chose a filter: its default parameters
  void onFilterSelected(int index);
  void onHardwareChanged(int index);
  void onGenerateMaskToggled(bool generate);
  void onLinkToggled(bool link);
  void onAutoRunToggled(bool autoRun);
  /// The parameters of the fields (kernel, rotation, accuracy, gradient)
  void onParametersEdited();
  /// The result in the views (active volume)
  void onShowResult();

  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);

  void onComputationCancelled();
  void onComputationFinished();
  void onComputationStarted();
  void updateProgress(int value);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroSmoothingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroSmoothingModuleWidget);
};

#endif
