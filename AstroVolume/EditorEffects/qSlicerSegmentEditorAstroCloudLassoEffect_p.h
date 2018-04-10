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

  This file was developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

#ifndef __qSlicerSegmentEditorAstroCloudLassoEffect_p_h
#define __qSlicerSegmentEditorAstroCloudLassoEffect_p_h

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Slicer API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

// Segmentations Editor Effects includes
#include "qSlicerAstroVolumeEditorEffectsExport.h"

#include "qSlicerSegmentEditorAstroCloudLassoEffect.h"

// CTK includes
#include <ctkPimpl.h>
#include <ctkVTKObject.h>

// VTK includes
#include <vtkCutter.h>
#include <vtkCylinderSource.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

// Qt includes
#include <QObject>
#include <QList>
#include <QMap>

class BrushPipeline;
class ctkRangeWidget;
class QCheckBox;
class QFrame;
class QIcon;
class QLabel;
class QPoint;
class QPushButton;
class qMRMLSliceWidget;
class qMRMLSpinBox;
class qMRMLThreeDWidget;
class vtkActor2D;
class vtkCellArray;
class vtkPoints;
class vtkPolyDataNormals;
class vtkPolyDataToImageStencil;
class vtkStripper;
class vtkTriangleFilter;
class vtkTubeFilter;

/// \ingroup SlicerRt_QtModules_Segmentations
/// \brief Private implementation of the segment editor paint effect
class qSlicerSegmentEditorAstroCloudLassoEffectPrivate: public QObject
{
  Q_OBJECT
  QVTK_OBJECT
  Q_DECLARE_PUBLIC(qSlicerSegmentEditorAstroCloudLassoEffect);
protected:
  qSlicerSegmentEditorAstroCloudLassoEffect* const q_ptr;
public:
  typedef QObject Superclass;
  qSlicerSegmentEditorAstroCloudLassoEffectPrivate(qSlicerSegmentEditorAstroCloudLassoEffect& object);
  ~qSlicerSegmentEditorAstroCloudLassoEffectPrivate();
  void init();

  /// Depending on the \sa DelayedPaint mode, either paint the given point or queue
  /// it up with a marker for later painting
  vtkIdType paintAddPoint(double pixelPositionWorld[3]);
  vtkIdType paintAddTwoPoints(double pixelPositionWorld[3]);

  /// Update paint circle glyph
  void updateBrush(qMRMLWidget* viewWidget, BrushPipeline* brush);

  /// Update brushes
  void updateBrushes();

  /// Update brush model (shape and position)
  void updateBrushModel(qMRMLWidget* viewWidget, double brushPosition_World[3]);

  /// Updates the brush stencil that can be used to quickly paint the brush shape into
  /// modifierLabelmap at many different positions.
  void updateBrushStencil(qMRMLWidget* viewWidget);

  /// create a 3D closed surface poly mask from the 2D selection on the 3D View
  void createClosedSurfacePolyMask(qMRMLWidget* viewWidget);

protected slots:
  /// reapply the paint if the threshold settings are changed
  void reApplyPaint();

  /// Handle changes for the Intensity Unit Node
  void onUnitNodeIntensityChanged(vtkObject* sender);

protected:
  /// Get brush object for widget. Create if does not exist
  BrushPipeline* brushForWidget(qMRMLWidget* viewWidget);

  /// Delete all brush pipelines
  void clearBrushPipelines();

  /// Paint labelmap
  void paintApply(qMRMLWidget* viewWidget);

  double GetSliceSpacing(qMRMLSliceWidget* sliceWidget);

  bool brushPositionInWorld(qMRMLWidget* viewWidget, int brushPositionInView[2], double brushPosition_World[3]);

public:
  QIcon CloudLassoIcon;

  vtkSmartPointer<vtkCylinderSource> BrushCylinderSource;
  vtkSmartPointer<vtkSphereSource> BrushSphereSource;
  vtkSmartPointer<vtkTransformPolyDataFilter> BrushToWorldOriginTransformer;
  vtkSmartPointer<vtkTransform> BrushToWorldOriginTransform;

  vtkSmartPointer<vtkTransformPolyDataFilter> WorldOriginToWorldTransformer;
  vtkSmartPointer<vtkTransform> WorldOriginToWorldTransform;
  vtkSmartPointer<vtkPolyDataNormals> BrushPolyDataNormals;
  vtkSmartPointer<vtkTransformPolyDataFilter> WorldToXYTransformer;
  vtkSmartPointer<vtkTransform> WorldToXYTransform;
  vtkSmartPointer<vtkTransformPolyDataFilter> WorldOriginToModifierLabelmapIjkTransformer;
  vtkSmartPointer<vtkTransform> WorldOriginToModifierLabelmapIjkTransform;
  vtkSmartPointer<vtkPolyDataToImageStencil> BrushPolyDataToStencil;

  vtkSmartPointer<vtkStripper> StripperFilter;
  vtkSmartPointer<vtkTubeFilter> FeedbackTubeFilter;

  vtkSmartPointer<vtkTriangleFilter> TriangulatorFilter;

  vtkSmartPointer<vtkPolyData> FeedbackPolyData;
  vtkSmartPointer<vtkPoints> PaintCoordinates_World;
  vtkSmartPointer<vtkCellArray> PaintLines_World;

  vtkSmartPointer<vtkPolyData> CloudLasso3DSelectionPolyData;
  vtkSmartPointer<vtkPoints> CloudLasso3DSelectionPoints;
  vtkSmartPointer<vtkPoints> ClosedSurfacePoints;
  vtkSmartPointer<vtkCellArray> ClosedSurfaceStrips;
  vtkSmartPointer<vtkCellArray> ClosedSurfacePolys;

  vtkSmartPointer<vtkOrientedImageData> LastMask;
  vtkSmartPointer<vtkOrientedImageData> LastSelectedSegmentLabelmap;
  bool UndoLastMask;

  QList<vtkActor2D*> FeedbackActors;
  QMap<qMRMLWidget*, BrushPipeline*> BrushPipelines;
  bool DelayedPaint;
  bool IsPainting;

  // Observed view node
  qMRMLWidget* ActiveViewWidget;
  int ActiveViewLastInteractionPosition[2];

  QFrame* CloudLassoFrame;
  QCheckBox* EraseModeCheckbox;
  QCheckBox* AutomaticThresholdCheckbox;
  QLabel* ThresholdRangeLabel;
  ctkRangeWidget *ThresholdRangeWidget;

  double distance;
  double Normals[3];
};

#endif
