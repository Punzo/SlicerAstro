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

#ifndef __qSlicerAstroPVSliceModuleWidget_h
#define __qSlicerAstroPVSliceModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroPVSliceModuleExport.h"

// CTK includes
#include <ctkVTKObject.h>

class qMRMLWidget;
class qSlicerAstroPVSliceModuleWidgetPrivate;
class vtkMRMLAstroPVSliceParametersNode;
class vtkMRMLNode;
class vtkRenderWindowInteractor;
class vtkRenderWindow;
class vtkRenderer;

/// \ingroup SlicerAstro_QtModules_AstroPVSlice
class Q_SLICERASTRO_QTMODULES_ASTROPVSLICE_EXPORT qSlicerAstroPVSliceModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroPVSliceModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroPVSliceModuleWidget();

  virtual void enter();
  virtual void exit();

  /// Get vtkMRMLAstroPVSliceParametersNode
  Q_INVOKABLE vtkMRMLAstroPVSliceParametersNode* mrmlAstroPVSliceParametersNode()const;

  /// Create observations between slice view interactor and the widget.
  /// The captured events are propagated to the active effect if any.
  /// NOTE: This method should be called from the enter function of the
  ///   embedding module widget so that the events are correctly processed.
  Q_INVOKABLE void setupViewObservations();

  /// Remove observations
  /// NOTE: This method should be called from the exit function of the
  ///   embedding module widget so that events are not processed unnecessarily.
  Q_INVOKABLE void removeViewObservations();

  /// Callback function invoked when interaction happens
  /// \param callerInteractor Interactor object that was observed to catch the event
  /// \param eid Event identifier
  /// \param viewWidget Widget of the Slicer layout view. Can be \sa qMRMLSliceWidget or \sa qMRMLThreeDWidget
  virtual bool processInteractionEvents(vtkRenderWindowInteractor* callerInteractor,
                                        unsigned long eid, qMRMLWidget* viewWidget);

  /// Get render window for view widget
  Q_INVOKABLE static vtkRenderWindow* renderWindow(qMRMLWidget* viewWidget);
  /// Get renderer for view widget
  Q_INVOKABLE static vtkRenderer* renderer(qMRMLWidget* viewWidget);

protected:
  QScopedPointer<qSlicerAstroPVSliceModuleWidgetPrivate> d_ptr;

  /// Callback function invoked when interaction happens
  static void processEvents(vtkObject* caller, unsigned long eid,
                            void* clientData, void* callData);

  /// Initialization of the qvtk connections between MRML nodes
  virtual void setMRMLScene(vtkMRMLScene*);

  /// Initialization of MRML nodes
  void initializeNodes(bool forceNew = false);

  /// Initialization of MRML parameter node
  void initializeParameterNode(bool forceNew = false);

  /// Initialization of MRML moment map node
  void initializeMomentMapNode(bool forceNew = false);

  /// Initialization of MRML Line node
  void initializeLineNode(bool forceNew = false,
                           bool InitLinePositions = true);

protected slots:

  /// Set the MRML input node
  void onInputVolumeChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML moment map input node
  void onMomentMapChanged(vtkMRMLNode* mrmlNode);

  /// Set the MRML Line node
  void onLineChanged(vtkMRMLNode* mrmlNode);

  /// Update widget GUI from MRML Line node
  void onMRMLPVSliceLineNodeModified();

  /// Update widget GUI from MRML Line node
  void onMRMLAstroPVSliceCenterModified();

  /// Update widget GUI from MRML parameter node
  void onMRMLAstroPVSliceParametersNodeModified();

  /// Set the MRML parameter node
  void setMRMLAstroPVSliceParametersNode(vtkMRMLNode*);

  void on3DViewParallel();
  void on3DViewPerpendicular();

  void onEndCloseEvent();
  void onEndImportEvent();
  void onStartImportEvent();

  void onLayoutChanged(int layoutIndex); 
  void onRotateLineChanged(double theta);
  void onLineCenterRightAscensionWCSChanged(double value);
  void onLineCenterDeclinationWCSChanged(double value);
  void onLineCenterRightAscensionIJKChanged(double value);
  void onLineCenterDeclinationIJKChanged(double value);
  void onLineColorChanged(QColor color);
  void onSetLineCenterClicked();

  void onMRMLSelectionNodeModified(vtkObject* sender);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroPVSliceModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroPVSliceModuleWidget);
};

#endif
