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

#ifndef __qSlicerAstroVolumeModuleWidget_h
#define __qSlicerAstroVolumeModuleWidget_h

// CTK includes
#include <ctkVTKObject.h>

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

// AstroVolume includes
#include "qSlicerAstroVolumeModuleWidgetsExport.h"

// Segmentation includes
#include "vtkMRMLSegmentationNode.h"

class qMRMLAstroVolumeInfoWidget;
class qSlicerAstroVolumeDisplayWidget;
class qSlicerAstroVolumeModuleWidgetPrivate;
class qSlicerVolumeRenderingModuleWidget;
class vtkMRMLAstroLabelMapVolumeNode;
class vtkMRMLAstroVolumeNode;
class vtkMRMLNode;
class vtkMRMLVolumeRenderingDisplayNode;

/// \ingroup Slicer_QtModules_AstroVolume_Widgets
class Q_SLICER_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroVolumeModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT
public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroVolumeModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroVolumeModuleWidget();

  /// Get AstroVolumeDispalyWidget
  Q_INVOKABLE qSlicerAstroVolumeDisplayWidget* astroVolumeDisplayWidget()const;

  /// Get AstroVolumeInfoWidget
  Q_INVOKABLE qMRMLAstroVolumeInfoWidget* astroVolumeInfoWidget()const;

  /// Get volumeRenderingDisplay
  Q_INVOKABLE vtkMRMLVolumeRenderingDisplayNode* volumeRenderingDisplay()const;

  /// Get volumeRenderingWidget
  Q_INVOKABLE qSlicerVolumeRenderingModuleWidget* volumeRenderingWidget()const;

  virtual void enter();
  virtual void exit();

public slots:
  void clearPresets();
  void onCurrentQualityControlChanged(int);
  void onVisibilityChanged(bool visibility);
  void setComparative3DViews(const char* volumeNodeOneID,
                             const char* volumeNodeTwoID,
                             bool generateMasks);
  void setQuantitative3DView(const char* volumeNodeOneID,
                             const char* volumeNodeTwoID,
                             const char* volumeNodeThreeID,
                             double ContourLevel,
                             double PVPhiMajor,
                             double PVPhiMinor,
                             double RAS[3]);
  void updateQuantitative3DView(const char* volumeNodeOneID,
                                const char* volumeNodeTwoID,
                                double ContourLevel,
                                double PVPhiMajor,
                                double PVPhiMinor,
                                double yellowRAS[3],
                                double greenRAS[3],
                                bool overrideSegments = false);
  void setMRMLVolumeNode(vtkMRMLNode* node);
  void setMRMLVolumeNode(vtkMRMLAstroVolumeNode* volumeNode);
  void setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode* volumeNode);
  void startRockView();
  void stopRockView();

protected slots:
  void offsetPreset(double offsetValue);
  void onCalculateRMS();
  void onCreateSurfaceButtonToggled(bool toggle);
  void onCropToggled(bool toggle);
  void onEditSelectedSegment();
  void onInputVolumeChanged(vtkMRMLNode *node);
  void onLockToggled(bool toggled);
  void onMRMLDisplayROINodeModified(vtkObject*);
  void onMRMLLabelVolumeNodeModified();
  void onMRMLSceneEndCloseEvent();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onMRMLSelectionNodeReferenceAdded(vtkObject* sender);
  void onMRMLSelectionNodeReferenceRemoved(vtkObject* sender);
  void onMRMLVolumeNodeModified();
  void onMRMLVolumeRenderingDisplayNodeModified(vtkObject* sender);
  void onPresetsNodeChanged(vtkMRMLNode*);
  void onPushButtonCovertLabelMapToSegmentationClicked();
  void onPushButtonConvertSegmentationToLabelMapClicked();
  void onRMSValueChanged(double RMS);
  void onROICropDisplayCheckBoxToggled(bool toggle);
  void onSegmentEditorNodeModified(vtkObject* sender);
  void resetStretch(vtkMRMLNode* node);
  void resetOffset(vtkMRMLNode* node);
  void setDisplayConnection(vtkMRMLNode* node);
  void setDisplayROIEnabled(bool visibility);
  void setOpticalVelocity();
  void setRadioVelocity();
  void spreadPreset(double stretchValue);
  void updatePresets(vtkMRMLNode* node);

signals:
  void astroLabelMapVolumeNodeChanged(bool enabled);
  void astroVolumeNodeChanged(bool enabled);
  void segmentEditorNodeChanged(bool enabled);

protected:
  virtual void setup();
  virtual void onEnter();
  virtual void setMRMLScene(vtkMRMLScene*);
  QScopedPointer<qSlicerAstroVolumeModuleWidgetPrivate> d_ptr;

  /// Update master representation in segmentation to a given representation.
  /// Used before adding a certain segment to a segmentation, making sure the user knows if data loss is possible.
  /// 1. Segmentation is empty or master is unspecified -> Master is changed to the segment's representation type
  /// 2. Segmentation is non-empty and master matches the representation -> No action
  /// 3. Segmentation is non-empty and master differs -> Choice presented to user
  /// \return False only if user chose not to change master representation on option 3, or if error occurred, otherwise true
  bool updateMasterRepresentationInSegmentation(vtkSegmentation* segmentation, QString representation);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeModuleWidget);
};

#endif
