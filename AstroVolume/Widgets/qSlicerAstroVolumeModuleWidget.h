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

// STD includes
#include <functional>
#include <string>

// Qt includes
#include <QMessageBox>

// CTK includes
#include <ctkVTKObject.h>

// Slicer includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

#include <vtkWeakPointer.h>

class qSlicerAstroContoursSection;
class qSlicerAstroDisplaySection;
class qSlicerAstroInfoSection;
class qSlicerAstroMasksSection;
class qSlicerAstroRenderingSection;
class qSlicerAstroSourcesSection;
class qSlicerAstroVolumeModuleWidgetPrivate;
class vtkMRMLAstroVolumeNode;
class vtkMRMLNode;
class vtkMRMLSegmentationNode;
class vtkMRMLVolumeRenderingDisplayNode;
class vtkObject;

/// \ingroup SlicerAstro_QtModules_AstroVolume
/// Panel of the AstroVolume module: the active data and the reusable
/// sections of AstroVolume in tabs (Display: Display and Contours; 3D;
/// Masks: Masks and Sources; Info). The Workbench embeds the same sections.
/// The panel also arranges the comparison views of the other modules
/// (qSlicerAstroVolumeModuleWidgetViews.cxx).
class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroVolumeModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroVolumeModuleWidget(QWidget *parent=nullptr);
  ~qSlicerAstroVolumeModuleWidget() override;

  enum Tab { DisplayTab = 0, RenderingTab, MasksTab, InfoTab };

  /// The sections of the panel
  Q_INVOKABLE qSlicerAstroDisplaySection* displaySection() const;
  Q_INVOKABLE qSlicerAstroContoursSection* contoursSection() const;
  Q_INVOKABLE qSlicerAstroRenderingSection* renderingSection() const;
  Q_INVOKABLE qSlicerAstroMasksSection* masksSection() const;
  Q_INVOKABLE qSlicerAstroSourcesSection* sourcesSection() const;
  Q_INVOKABLE qSlicerAstroInfoSection* infoSection() const;
  /// Tab shown (Tab)
  Q_INVOKABLE int currentTab() const;

  /// Volume rendering of the active volume (nullptr if none)
  Q_INVOKABLE vtkMRMLVolumeRenderingDisplayNode* volumeRenderingDisplay() const;
  /// The active volume is shown automatically in the views
  /// (vtkSlicerAstroVolumeLogic::AutoShowActiveVolume)
  Q_INVOKABLE bool autoShowActiveVolume() const;

  void enter() override;
  void exit() override;

public slots:
  void setCurrentTab(int tab);
  void setAutoShowActiveVolume(bool show);

  /// Set comparative layout 1 (used by smoothing module)
  void setComparative3DViews(const char* volumeNodeOneID,
                             const char* volumeNodeTwoID,
                             bool generateMasks = false,
                             bool overlay2D = true);

  /// Set comparative layout 2 (used by reprojection module)
  void setThreeComparativeView(const char* volumeNodeOneID,
                               const char* volumeNodeTwoID,
                               const char* volumeNodeThreeID);

  /// Set quantitative layout (used by modeling module)
  void setQuantitative3DView(const char* volumeNodeOneID,
                             const char* volumeNodeTwoID,
                             const char* volumeNodeThreeID,
                             double ContourLevel,
                             double PVPhiMajor,
                             double PVPhiMinor,
                             double RAS[3]);

  /// Update quantitative layout (used by modeling module)
  void updateQuantitative3DView(const char* volumeNodeOneID,
                                const char* volumeNodeTwoID,
                                double ContourLevel,
                                double PVPhiMajor,
                                double PVPhiMinor,
                                double yellowRAS[3],
                                double greenRAS[3],
                                bool overrideSegments = false);

  /// Questions and messages to the user (e.g. reprojection issues and their
  /// fixes): also in the application log. In testing mode no dialog is
  /// shown and the answer to a question is Yes.
  static bool askUser(QWidget* parent, const QString& title, const QString& question);
  static void informUser(QWidget* parent, QMessageBox::Icon icon, const QString& title,
                         const QString& message);

  /// Run work in a worker thread and return when it is done; the GUI stays
  /// responsive, with a progress dialog (progress: 0-100) and a Cancel
  /// button (cancel: request to stop the work). The work must not modify
  /// the scene (no MRML events from the worker thread).
  static void runInBackground(QWidget* parent, const QString& label,
                              const std::function<void()>& work,
                              const std::function<int()>& progress,
                              const std::function<void()>& cancel);

  /// IDs of the segments of the contours of the Modeling module (segmentation
  /// "AstroModelingContoursSeg"): data and model at the contour level, and
  /// region of the model
  static const char* modelingDataContoursSegmentID();
  static const char* modelingModelContoursSegmentID();
  static const char* modelingModelMaskSegmentID();

  /// The active volume of the panel (and of the views)
  void setMRMLVolumeNode(vtkMRMLNode* node);

  /// Rock the 3D views
  void startRockView();
  void stopRockView();

protected slots:
  void onDatasetSelected(vtkMRMLNode* node);
  void onSelectionNodeModified();
  void onMRMLSceneEndImportEvent();
  void onMRMLSceneEndCloseEvent();

protected:
  QScopedPointer<qSlicerAstroVolumeModuleWidgetPrivate> d_ptr;

  void setup() override;
  void setMRMLScene(vtkMRMLScene*) override;

  /// The volume rendering of a cube ready to be shown (logic
  /// PrepareVolumeRendering): used by the comparison views
  void prepareVolumeRendering(vtkMRMLAstroVolumeNode* volume);

  /// Contours of the Modeling module: create or recompute in place the
  /// segment segmentID (voxels of volume in [lower, upper]), named name
  bool updateModelingContourSegment(const char* segmentID, const std::string& name,
                                    const double color[3], vtkMRMLAstroVolumeNode* volume,
                                    double lower, double upper);
  /// Remove the segments of the contours of the Modeling module that are
  /// not the data contours, the model contours or the region of the model
  void removeObsoleteModelingContourSegments();

  /// Contours of the Modeling module (owned by the scene)
  vtkWeakPointer<vtkMRMLSegmentationNode> contoursSegNodeModeling;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroVolumeModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroVolumeModuleWidget);
};

#endif
