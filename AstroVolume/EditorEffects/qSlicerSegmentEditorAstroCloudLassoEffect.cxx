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
  and was supported through the European Research Consil grant nr. 291531.

  This file was originally developed by Csaba Pinter, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

// Segmentations includes
#include "qSlicerSegmentEditorAstroCloudLassoEffect.h"
#include "qSlicerSegmentEditorAstroCloudLassoEffect_p.h"
#include "vtkMRMLSegmentationDisplayNode.h"
#include "vtkMRMLSegmentationNode.h"
#include "vtkMRMLSegmentEditorNode.h"
#include "vtkOrientedImageData.h"

// Qt includes
#include <QDebug>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

// CTK includes
#include <ctkRangeWidget.h>

// VTK includes
#include <vtkActor.h>
#include <vtkActor2D.h>
#include <vtkBoundingBox.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCellPicker.h>
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkGlyph2D.h>
#include <vtkGlyph3D.h>
#include <vtkIdList.h>
#include <vtkImageChangeInformation.h>
#include <vtkImageFillROI.h>
#include <vtkImageMathematics.h>
#include <vtkImageStencil.h>
#include <vtkImageStencilData.h>
#include <vtkImageStencilToImage.h>
#include <vtkLine.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyDataToImageStencil.h>
#include <vtkProperty2D.h>
#include <vtkProperty.h>
#include <vtkPropPicker.h>
#include <vtkRegularPolygonSource.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkSplineFilter.h>
#include <vtkStripper.h>
#include <vtkStringArray.h>
#include <vtkTriangleFilter.h>
#include <vtkTubeFilter.h>
#include <vtkImageThreshold.h>
#include <vtkWorldPointPicker.h>

// MRML includes
#include <vtkEventBroker.h>
#include <vtkMRMLScalarVolumeNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLUnitNode.h>

// AstroMRML includes
#include <vtkMRMLAstroVolumeNode.h>

// Slicer includes
#include "qMRMLSliceView.h"
#include "qMRMLSliceWidget.h"
#include "qMRMLThreeDView.h"
#include "qMRMLThreeDWidget.h"
#include "qSlicerLayoutManager.h"
#include "qSlicerApplication.h"
#include "vtkMRMLSliceLogic.h"
#include "vtkMRMLSliceLayerLogic.h"
#include "vtkOrientedImageDataResample.h"
#include "vtkSlicerApplicationLogic.h"

//-----------------------------------------------------------------------------
/// Visualization objects and pipeline for each slice view for the paint brush
class BrushPipeline
{
public:
  BrushPipeline()
    {
    this->WorldToSliceTransform = vtkSmartPointer<vtkTransform>::New();
    this->SlicePlane = vtkSmartPointer<vtkPlane>::New();
    };
  virtual ~BrushPipeline()
    {
    };
  virtual void SetBrushVisibility(bool visibility) = 0;
  virtual void SetFeedbackVisibility(bool visibility) = 0;

  vtkSmartPointer<vtkTransform> WorldToSliceTransform;
  vtkSmartPointer<vtkPlane> SlicePlane;
};

class BrushPipeline2D : public BrushPipeline
{
public:
  BrushPipeline2D()
    {
    this->BrushCutter = vtkSmartPointer<vtkCutter>::New();
    this->BrushCutter->SetCutFunction(this->SlicePlane);
    this->BrushCutter->SetGenerateCutScalars(0);

    this->BrushWorldToSliceTransformer = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
    this->BrushWorldToSliceTransformer->SetTransform(this->WorldToSliceTransform);
    this->BrushWorldToSliceTransformer->SetInputConnection(this->BrushCutter->GetOutputPort());

    this->BrushMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
    this->BrushMapper->SetInputConnection(this->BrushWorldToSliceTransformer->GetOutputPort());

    this->BrushActor = vtkSmartPointer<vtkActor2D>::New();
    this->BrushActor->SetMapper(this->BrushMapper);
    this->BrushActor->VisibilityOff();

    this->FeedbackCutter = vtkSmartPointer<vtkCutter>::New();
    this->FeedbackCutter->SetCutFunction(this->SlicePlane);
    this->FeedbackCutter->SetGenerateCutScalars(0);

    this->FeedbackWorldToSliceTransformer = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
    this->FeedbackWorldToSliceTransformer->SetTransform(this->WorldToSliceTransform);
    this->FeedbackWorldToSliceTransformer->SetInputConnection(this->FeedbackCutter->GetOutputPort());

    this->FeedbackMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
    this->FeedbackMapper->SetInputConnection(this->FeedbackWorldToSliceTransformer->GetOutputPort());
    this->FeedbackActor = vtkSmartPointer<vtkActor2D>::New();
    vtkProperty2D* feedbackActorProperty = this->FeedbackActor->GetProperty();
    feedbackActorProperty->SetColor(0.7, 0.7, 0.0);
    feedbackActorProperty->SetOpacity(0.5);
    this->FeedbackActor->SetMapper(this->FeedbackMapper);
    this->FeedbackActor->VisibilityOff();
    };
  ~BrushPipeline2D()
    {
    };

  void SetBrushVisibility(bool visibility)
    {
    this->BrushActor->SetVisibility(visibility);
    };
  void SetFeedbackVisibility(bool visibility)
    {
    this->FeedbackActor->SetVisibility(visibility);
    };

  vtkSmartPointer<vtkActor2D> BrushActor;
  vtkSmartPointer<vtkPolyDataMapper2D> BrushMapper;
  vtkSmartPointer<vtkActor2D> FeedbackActor;
  vtkSmartPointer<vtkPolyDataMapper2D> FeedbackMapper;
  vtkSmartPointer<vtkCutter> BrushCutter;
  vtkSmartPointer<vtkTransformPolyDataFilter> BrushWorldToSliceTransformer;
  vtkSmartPointer<vtkCutter> FeedbackCutter;
  vtkSmartPointer<vtkTransformPolyDataFilter> FeedbackWorldToSliceTransformer;
};

class BrushPipeline3D : public BrushPipeline
{
public:
  BrushPipeline3D()
    {
    this->BrushMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    this->BrushActor = vtkSmartPointer<vtkActor>::New();
    this->BrushActor->SetMapper(this->BrushMapper);
    this->BrushActor->VisibilityOff();
    this->BrushActor->PickableOff(); // otherwise picking in 3D view would not work

    this->FeedbackMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    this->FeedbackActor = vtkSmartPointer<vtkActor>::New();
    this->FeedbackActor->VisibilityOff();
    this->FeedbackActor->PickableOff(); // otherwise picking in 3D view would not work
    vtkProperty* feedbackActorProperty = this->FeedbackActor->GetProperty();
    feedbackActorProperty->SetColor(0.7, 0.7, 0.0);
    this->FeedbackActor->SetMapper(this->FeedbackMapper);
    this->FeedbackActor->VisibilityOff();
    };
  ~BrushPipeline3D()
    {
    };
  void SetBrushVisibility(bool visibility)
    {
    this->BrushActor->SetVisibility(visibility);
    };
  void SetFeedbackVisibility(bool visibility)
    {
    this->FeedbackActor->SetVisibility(visibility);
    };

  vtkSmartPointer<vtkPolyDataMapper> BrushMapper;
  vtkSmartPointer<vtkActor> BrushActor;
  vtkSmartPointer<vtkActor> FeedbackActor;
  vtkSmartPointer<vtkPolyDataMapper> FeedbackMapper;
};


namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

}// end namespace

//-----------------------------------------------------------------------------
// qSlicerSegmentEditorAstroCloudLassoEffectPrivate methods

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAstroCloudLassoEffectPrivate::qSlicerSegmentEditorAstroCloudLassoEffectPrivate(qSlicerSegmentEditorAstroCloudLassoEffect& object)
  : q_ptr(&object)
  , UndoLastMask(true)
  , DelayedPaint(true)
  , IsPainting(false)
  , CloudLassoFrame(NULL)
  , EraseModeCheckbox(NULL)
  , AutomaticThresholdCheckbox(NULL)
  , ThresholdRangeLabel(NULL)
  , ThresholdRangeWidget(NULL)
  , distance(0.)
{
  this->Normals[0] = 0.;
  this->Normals[1] = 0.;
  this->Normals[2] = 0.;

  this->PaintCoordinates_World = vtkSmartPointer<vtkPoints>::New();
  this->PaintLines_World = vtkSmartPointer<vtkCellArray>::New();
  this->FeedbackPolyData = vtkSmartPointer<vtkPolyData>::New();
  this->FeedbackPolyData->SetPoints(this->PaintCoordinates_World);
  this->FeedbackPolyData->SetLines(this->PaintLines_World);

  this->CloudLasso3DSelectionPoints = vtkSmartPointer<vtkPoints>::New();
  this->CloudLasso3DSelectionStrips = vtkSmartPointer<vtkCellArray>::New();
  this->CloudLasso3DSelectionPolys = vtkSmartPointer<vtkCellArray>::New();
  this->CloudLasso3DSelectionPolyData = vtkSmartPointer<vtkPolyData>::New();
  this->CloudLasso3DSelectionPolyData->SetPoints(this->CloudLasso3DSelectionPoints);
  this->CloudLasso3DSelectionPolyData->SetStrips(this->CloudLasso3DSelectionStrips);
  this->CloudLasso3DSelectionPolyData->SetPolys(this->CloudLasso3DSelectionPolys);

  this->LastMask = vtkSmartPointer<vtkOrientedImageData>::New();
  this->LastSelectedSegmentLabelmap = vtkSmartPointer<vtkOrientedImageData>::New();

  this->CloudLassoIcon = QIcon(":Icons/AstroCloudLasso.png");

  this->BrushCylinderSource = vtkSmartPointer<vtkCylinderSource>::New();
  this->BrushSphereSource = vtkSmartPointer<vtkSphereSource>::New();
  this->BrushToWorldOriginTransformer = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->BrushToWorldOriginTransform = vtkSmartPointer<vtkTransform>::New();
  this->BrushToWorldOriginTransformer->SetTransform(this->BrushToWorldOriginTransform);

  this->BrushPolyDataNormals = vtkSmartPointer<vtkPolyDataNormals>::New();
  this->BrushPolyDataNormals->SetInputConnection(this->BrushToWorldOriginTransformer->GetOutputPort());
  this->BrushPolyDataNormals->AutoOrientNormalsOn();

  this->WorldOriginToWorldTransformer = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->WorldOriginToWorldTransform = vtkSmartPointer<vtkTransform>::New();
  this->WorldOriginToWorldTransformer->SetTransform(this->WorldOriginToWorldTransform);
  this->WorldOriginToWorldTransformer->SetInputConnection(this->BrushPolyDataNormals->GetOutputPort());

  this->WorldToXYTransformer = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->WorldToXYTransform = vtkSmartPointer<vtkTransform>::New();
  this->WorldToXYTransformer->SetTransform(this->WorldToXYTransform);
  this->WorldToXYTransformer->SetInputData(this->FeedbackPolyData);

  this->TriangulatorFilter = vtkSmartPointer<vtkTriangleFilter>::New();
  this->TriangulatorFilter->SetInputData(this->CloudLasso3DSelectionPolyData);
  this->WorldOriginToModifierLabelmapIjkTransformer = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->WorldOriginToModifierLabelmapIjkTransform = vtkSmartPointer<vtkTransform>::New();
  this->WorldOriginToModifierLabelmapIjkTransformer->SetTransform(this->WorldOriginToModifierLabelmapIjkTransform);
  this->WorldOriginToModifierLabelmapIjkTransformer->SetInputConnection(this->TriangulatorFilter->GetOutputPort());
  this->BrushPolyDataToStencil = vtkSmartPointer<vtkPolyDataToImageStencil>::New();
  this->BrushPolyDataToStencil->SetOutputSpacing(1.0,1.0,1.0);
  this->BrushPolyDataToStencil->SetInputConnection(this->WorldOriginToModifierLabelmapIjkTransformer->GetOutputPort());

  this->StripperFilter = vtkSmartPointer<vtkStripper>::New();
  this->StripperFilter->SetInputData(this->FeedbackPolyData);

  this->SmoothPolyFilter = vtkSmartPointer<vtkSplineFilter>::New();
  this->SmoothPolyFilter->SetInputConnection(this->StripperFilter->GetOutputPort());
  this->SmoothPolyFilter->SetNumberOfSubdivisions(1);

  this->FeedbackTubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
  this->FeedbackTubeFilter->SetInputConnection(this->SmoothPolyFilter->GetOutputPort());
  this->FeedbackTubeFilter->SetRadius(1);
  this->FeedbackTubeFilter->SetNumberOfSides(16);
  this->FeedbackTubeFilter->CappingOn();

}

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAstroCloudLassoEffectPrivate::~qSlicerSegmentEditorAstroCloudLassoEffectPrivate()
{
  this->clearBrushPipelines();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::init()
{
  Q_Q(qSlicerSegmentEditorAstroCloudLassoEffect);
  // Create options frame for this effect
  this->CloudLassoFrame = new QFrame();
  this->CloudLassoFrame->setLayout(new QHBoxLayout());
  q->addOptionsWidget(this->CloudLassoFrame);

  this->ThresholdRangeLabel = new QLabel("Threshold range values: ");
  q->addOptionsWidget(this->ThresholdRangeLabel);

  this->ThresholdRangeWidget = new ctkRangeWidget();
  this->ThresholdRangeWidget->setToolTip("list of shortcut keys: 'n' decrease the minimum threshold value; "
                                         "'m' increase the minimum threshold value; "
                                         "'k' decrease the maximum threshold value; "
                                         "'l' increase the maximum threshold value. ");
  q->addOptionsWidget(this->ThresholdRangeWidget);

  this->AutomaticThresholdCheckbox = new QCheckBox("Automatic Threshold Updating Mode");
  this->AutomaticThresholdCheckbox->setToolTip("Activate or deactivate automatic updating "
                                               "of the selection when chaning the threshold value. "
                                               "If active: draw a 2-D or 3-D cloudlasso, then, change "
                                               "the threshold value. It is not possible to activate it "
                                               "in Erase Mode. The shortcut key is 'c'.");
  q->addOptionsWidget(this->AutomaticThresholdCheckbox);

  this->EraseModeCheckbox = new QCheckBox("Erase Mode");
  this->EraseModeCheckbox->setToolTip("Activate or deactivate Erase Mode. The shortcut key is 'x'.");
  q->addOptionsWidget(this->EraseModeCheckbox);

  QObject::connect(this->ThresholdRangeWidget, SIGNAL(valuesChanged(double,double)), q, SLOT(onThresholdValueChanged(double, double)));
  QObject::connect(this->EraseModeCheckbox, SIGNAL(toggled(bool)), q, SLOT(onEraseModeChanged(bool)));
  QObject::connect(this->AutomaticThresholdCheckbox, SIGNAL(toggled(bool)), q, SLOT(onAutomaticThresholdModeChanged(bool)));

  vtkMRMLUnitNode* unitNodeIntensity = qSlicerCoreApplication::application()->applicationLogic()->GetSelectionNode()->GetUnitNode("intensity");
  this->qvtkConnect( unitNodeIntensity, vtkCommand::ModifiedEvent, this, SLOT(onUnitNodeIntensityChanged(vtkObject*)));
}

//-----------------------------------------------------------------------------
BrushPipeline* qSlicerSegmentEditorAstroCloudLassoEffectPrivate::brushForWidget(qMRMLWidget* viewWidget)
{
  Q_Q(qSlicerSegmentEditorAstroCloudLassoEffect);

  if (this->BrushPipelines.contains(viewWidget))
    {
    return this->BrushPipelines[viewWidget];
    }

  // Create brushPipeline if does not yet exist
  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);
  if (sliceWidget)
    {
    BrushPipeline2D* pipeline = new BrushPipeline2D();
    pipeline->BrushCutter->SetInputConnection(this->WorldOriginToWorldTransformer->GetOutputPort());
    pipeline->FeedbackCutter->SetInputConnection(this->FeedbackTubeFilter->GetOutputPort());
    this->updateBrush(sliceWidget, pipeline);
    q->addActor2D(viewWidget, pipeline->BrushActor);
    q->addActor2D(viewWidget, pipeline->FeedbackActor);
    this->BrushPipelines[viewWidget] = pipeline;
    return pipeline;
    }
  else if (threeDWidget)
    {
    BrushPipeline3D* pipeline = new BrushPipeline3D();
    pipeline->BrushMapper->SetInputConnection(this->WorldOriginToWorldTransformer->GetOutputPort());
    pipeline->FeedbackMapper->SetInputConnection(this->FeedbackTubeFilter->GetOutputPort());
    this->updateBrush(threeDWidget, pipeline);
    q->addActor3D(viewWidget, pipeline->BrushActor);
    q->addActor3D(viewWidget, pipeline->FeedbackActor);
    this->BrushPipelines[viewWidget] = pipeline;
    return pipeline;
    }

  return NULL;
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::forceRender(qMRMLWidget* viewWidget)
{
  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);
  if (sliceWidget)
    {
    sliceWidget->sliceView()->forceRender();
    }
  if (threeDWidget)
    {
    threeDWidget->threeDView()->forceRender();
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::scheduleRender(qMRMLWidget* viewWidget)
{
  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);
  if (sliceWidget)
    {
    sliceWidget->sliceView()->scheduleRender();
    }
  if (threeDWidget)
    {
    threeDWidget->threeDView()->scheduleRender();
    }
}

//-----------------------------------------------------------------------------
vtkIdType qSlicerSegmentEditorAstroCloudLassoEffectPrivate::paintAddPoint(double brushPosition_World[3])
{
  vtkIdType idPoint = this->PaintCoordinates_World->InsertNextPoint(brushPosition_World);

  this->CloudLasso3DSelectionPoints->InsertNextPoint(brushPosition_World);

  if(idPoint <= 0)
    {
    return idPoint;
    }

  vtkNew<vtkLine> line;
  line->GetPointIds()->SetId(0, idPoint - 1);
  line->GetPointIds()->SetId(1, idPoint);
  this->PaintLines_World->InsertNextCell(line.GetPointer());
  this->SmoothPolyFilter->SetNumberOfSubdivisions(idPoint * 10);
  return idPoint;
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::paintApply(qMRMLWidget* viewWidget)
{
  Q_Q(qSlicerSegmentEditorAstroCloudLassoEffect);

  vtkOrientedImageData* selectedSegmentLabelmap = q->selectedSegmentLabelmap();
  if (!selectedSegmentLabelmap)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid selectedSegmentLabelmap";
    return;
    }
  vtkOrientedImageData* modifierLabelmap = q->defaultModifierLabelmap();
  if (!modifierLabelmap)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid modifierLabelmap";
    return;
    }
  if (!q->parameterSetNode())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment editor parameter set node!";
    return;
    }
  vtkMRMLSegmentationNode* segmentationNode = q->parameterSetNode()->GetSegmentationNode();
  if (!segmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationNode";
    return;
    }

  q->saveStateForUndo();

  qSlicerSegmentEditorAbstractEffect::ModificationMode modificationMode =
    (q->m_Erase ? qSlicerSegmentEditorAbstractEffect::ModificationModeRemove
      : qSlicerSegmentEditorAbstractEffect::ModificationModeAdd);

  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);
  if (sliceWidget)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(
      qSlicerSegmentEditorAbstractEffect::viewNode(sliceWidget) );
    if (!sliceNode)
      {
      qCritical() << Q_FUNC_INFO << ": Failed to get slice node!";
      }

    // AppendPolyMask works on XY (screen) coordinates.
    // Therefore, we trasform FeedbackPolyData from RAS to XY (screen) coordinates.
    this->WorldToXYTransform->Identity();

    vtkNew<vtkMatrix4x4> segmentationToSegmentationXYTransformMatrix;
    segmentationToSegmentationXYTransformMatrix->DeepCopy(sliceNode->GetXYToRAS());
    segmentationToSegmentationXYTransformMatrix->Invert();
    this->WorldToXYTransform->Concatenate(segmentationToSegmentationXYTransformMatrix.GetPointer());

    this->WorldToXYTransformer->Update();

    // Create a 2-D polyMaskImage from the FeedbackPolyData
    q->appendPolyMask(modifierLabelmap, this->WorldToXYTransformer->GetOutput(), sliceWidget);
    }
  if (threeDWidget)
    {
    // Create a 3-D closed surface from the FeedbackPolyData (tube poly data) and store it in CloudLasso3DSelectionPolyData
    this->createClosedSurfacePolyMask(viewWidget);

    // Apply RAS -> IJK transform to CloudLasso3DSelectionPolyData and updating BrushPolyDataToStencil
    this->updateBrushStencil(viewWidget);
    this->BrushPolyDataToStencil->Update();

    vtkNew<vtkImageStencilToImage> stencilToImage;
    stencilToImage->SetInputConnection(this->BrushPolyDataToStencil->GetOutputPort());
    stencilToImage->SetInsideValue(q->m_FillValue);
    stencilToImage->SetOutsideValue(q->m_EraseValue);
    stencilToImage->SetOutputScalarType(modifierLabelmap->GetScalarType());

    vtkNew<vtkImageChangeInformation> brushPositioner;
    brushPositioner->SetInputConnection(stencilToImage->GetOutputPort());
    brushPositioner->SetOutputSpacing(modifierLabelmap->GetSpacing());
    brushPositioner->SetOutputOrigin(modifierLabelmap->GetOrigin());
    brushPositioner->Update();

    vtkNew<vtkOrientedImageData> orientedBrushPositionerOutput;
    orientedBrushPositionerOutput->ShallowCopy(brushPositioner->GetOutput());
    orientedBrushPositionerOutput->CopyDirections(modifierLabelmap);

    vtkOrientedImageDataResample::ModifyImage(modifierLabelmap,
      orientedBrushPositionerOutput.GetPointer(), vtkOrientedImageDataResample::OPERATION_MAXIMUM);
    }

  // Store the modifierLabelmap for subsequent automatic thresholding on the same selection.
  this->LastMask->DeepCopy(modifierLabelmap);

  // Store the selectedSegmentLabelmap for subsequent automatic thresholding on the same selection.
  this->LastSelectedSegmentLabelmap->DeepCopy(selectedSegmentLabelmap);

  // Notify editor about changes
  modifierLabelmap->Modified();

  q->modifySelectedSegmentByLabelmap(modifierLabelmap, modificationMode);

  this->PaintCoordinates_World->Reset();
  this->PaintLines_World->Reset();
  this->CloudLasso3DSelectionPoints->Reset();
  this->CloudLasso3DSelectionStrips->Reset();
  this->CloudLasso3DSelectionPolys->Reset();

  q->CreateSurface();

  QApplication::restoreOverrideCursor();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::updateBrushStencil(qMRMLWidget* viewWidget)
{
  Q_Q(qSlicerSegmentEditorAstroCloudLassoEffect);
  Q_UNUSED(viewWidget);

  if (!q->parameterSetNode())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment editor parameter set node!";
    return;
    }
  vtkMRMLSegmentationNode* segmentationNode = q->parameterSetNode()->GetSegmentationNode();
  if (!segmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationNode";
    return;
    }
  vtkOrientedImageData* modifierLabelmap = q->modifierLabelmap();
  if (!modifierLabelmap)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid modifierLabelmap";
    return;
    }

  // Brush stencil transform, FeedbackPolyData are in RAS coordinate
  this->WorldOriginToModifierLabelmapIjkTransform->Identity();

  vtkNew<vtkMatrix4x4> segmentationToSegmentationIjkTransformMatrix;
  modifierLabelmap->GetImageToWorldMatrix(segmentationToSegmentationIjkTransformMatrix.GetPointer());
  segmentationToSegmentationIjkTransformMatrix->Invert();
  this->WorldOriginToModifierLabelmapIjkTransform->Concatenate(segmentationToSegmentationIjkTransformMatrix.GetPointer());

  vtkNew<vtkMatrix4x4> worldToSegmentationTransformMatrix;
  // We don't support painting in non-linearly transformed node (it could be implemented, but would probably slow down things too much)
  // TODO: show a meaningful error message to the user if attempted
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(NULL, segmentationNode->GetParentTransformNode(), worldToSegmentationTransformMatrix.GetPointer());
  this->WorldOriginToModifierLabelmapIjkTransform->Concatenate(worldToSegmentationTransformMatrix.GetPointer());

  this->WorldOriginToModifierLabelmapIjkTransformer->Update();
  vtkPolyData* brushModel_ModifierLabelmapIjk = this->WorldOriginToModifierLabelmapIjkTransformer->GetOutput();
  double* boundsIjk = brushModel_ModifierLabelmapIjk->GetBounds();
  this->BrushPolyDataToStencil->SetOutputWholeExtent(floor(boundsIjk[0])-1, ceil(boundsIjk[1])+1,
          floor(boundsIjk[2])-1, ceil(boundsIjk[3])+1, floor(boundsIjk[4])-1, ceil(boundsIjk[5])+1);
}

//----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::createClosedSurfacePolyMask(qMRMLWidget* viewWidget)
{
  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);
  if (!threeDWidget)
    {
    return;
    }

  vtkRenderer* renderer = qSlicerSegmentEditorAbstractEffect::renderer(threeDWidget);
  if (!renderer)
    {
    return;
    }

  vtkCamera *camera = renderer->GetActiveCamera();
  if (!camera)
    {
    return;
    }

  int numberOfPoints = this->CloudLasso3DSelectionPoints->GetNumberOfPoints();

  if (numberOfPoints <= 0)
    {
    return;
    }

  double distCenterSelectionCamera = 0.;
  double selectionCenter[3] = { 0., 0., 0. };

  if (!camera->GetParallelProjection())
    {
    for (int ii = 0; ii < numberOfPoints; ii++)
      {
      double Point[3] = { 0., 0., 0. };
      this->CloudLasso3DSelectionPoints->GetPoint(ii, Point);
      for (int jj = 0; jj < 3; jj++)
        {
        selectionCenter[jj] += Point[jj];
        }
      }

    for (int ii = 0; ii < 3; ii++)
      {
      selectionCenter[ii] /= numberOfPoints;
      }

    double cameraPos[3] = { 0., 0., 0. };
    camera->GetPosition(cameraPos);

    for (int ii = 0; ii < 3; ii++)
      {
      this->Normals[ii] = cameraPos[ii] - selectionCenter[ii];
      }

    distCenterSelectionCamera = sqrt(this->Normals[0] * this->Normals[0] +
                                     this->Normals[1] * this->Normals[1] +
                                     this->Normals[2] * this->Normals[2]);
    vtkMath::Normalize(this->Normals);
    }

  double dist2 = this->distance * 2.;
  for (int ii = 0; ii < numberOfPoints; ii++)
    {
    double Point[3] = { 0., 0., 0. };

    this->CloudLasso3DSelectionPoints->GetPoint(ii, Point);

    double distancePointSelectionCenter = 0., tanAngle = 0.;
    double diff[3] = { 0., 0., 0. };
    if (!camera->GetParallelProjection())
      {
      for (int jj = 0; jj < 3; jj++)
        {
        diff[jj] = Point[jj] - selectionCenter[jj];
        }

      distancePointSelectionCenter = sqrt(diff[0] * diff[0] +
                                          diff[1] * diff[1] +
                                          diff[2] * diff[2]);
      tanAngle =  distancePointSelectionCenter / distCenterSelectionCamera;
      vtkMath::Normalize(diff);
      }

    double NewPoint[3] = { 0., 0., 0. };
    for (int jj = 0; jj < 3; jj++)
      {
      NewPoint[jj] = Point[jj] - (this->Normals[jj] * dist2) + (dist2 * tanAngle * diff[jj]);
      }
    this->CloudLasso3DSelectionPoints->InsertNextPoint(NewPoint);
    }

  this->CloudLasso3DSelectionStrips->InsertNextCell(numberOfPoints*2+2);
  for (int ii = 0; ii < numberOfPoints; ii++)
    {
    this->CloudLasso3DSelectionStrips->InsertCellPoint(ii);
    this->CloudLasso3DSelectionStrips->InsertCellPoint(ii+numberOfPoints);
    }

  this->CloudLasso3DSelectionStrips->InsertCellPoint(0);
  this->CloudLasso3DSelectionStrips->InsertCellPoint(numberOfPoints);

  this->CloudLasso3DSelectionPolys->InsertNextCell(numberOfPoints);
  for (int ii = 0; ii < numberOfPoints; ii++)
    {
    this->CloudLasso3DSelectionPolys->InsertCellPoint(ii);
    }

  this->CloudLasso3DSelectionPolys->InsertNextCell(numberOfPoints);
  for (int ii = numberOfPoints; ii < numberOfPoints*2; ii++)
    {
    this->CloudLasso3DSelectionPolys->InsertCellPoint(ii);
    }
}

//----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::reApplyPaint()
{
  Q_Q(qSlicerSegmentEditorAstroCloudLassoEffect);

  vtkMRMLSegmentEditorNode *parameterNode = q->parameterSetNode();
  if (!parameterNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid parameterNode";
    return;
    }

  if (q->m_Erase)
    {
    return;
    }

  if (!this->LastMask)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid Image Mask";
    return;
    }

  int Extent[6] = { 0, 0, 0, 0, 0, 0 };
  this->LastMask->GetExtent(Extent);
  int sum = 0.;
  for (int ii = 0; ii < 6; ii++)
    {
    sum += abs(Extent[ii]);
    }

  if (sum <= 3)
    {
    return;
    }

  vtkOrientedImageData* masterVolumeOrientedImageData = q->masterVolumeImageData();
  if (!masterVolumeOrientedImageData)
    {
    return;
    }
  // Make sure the modifier labelmap has the same geometry as the master volume
  if (!vtkOrientedImageDataResample::DoGeometriesMatch(this->LastMask, masterVolumeOrientedImageData))
    {
    return;
    }

  double ThresholdMinimumValue = q->doubleParameter("ThresholdMinimumValue");
  double ThresholdMaximumValue = q->doubleParameter("ThresholdMaximumValue");
  // Create threshold image
  vtkNew<vtkImageThreshold> threshold;
  threshold->SetInputData(masterVolumeOrientedImageData);
  threshold->ThresholdBetween(ThresholdMinimumValue, ThresholdMaximumValue);
  threshold->SetInValue(1);
  threshold->SetOutValue(0);
  threshold->SetOutputScalarType(this->LastMask->GetScalarType());
  threshold->Update();

  vtkNew<vtkOrientedImageData> thresholdMask;
  thresholdMask->DeepCopy(threshold->GetOutput());
  vtkNew<vtkMatrix4x4> modifierLabelmapToWorldMatrix;
  this->LastMask->GetImageToWorldMatrix(modifierLabelmapToWorldMatrix.GetPointer());
  thresholdMask->SetGeometryFromImageToWorldMatrix(modifierLabelmapToWorldMatrix.GetPointer());

  vtkNew<vtkOrientedImageData> ThresholdLastMask;
  ThresholdLastMask->DeepCopy(this->LastMask);
  q->applyImageMask(ThresholdLastMask.GetPointer(), thresholdMask.GetPointer(), q->m_EraseValue);

  if (this->UndoLastMask)
    {
    vtkMRMLSegmentationNode* segmentationNode = q->parameterSetNode()->GetSegmentationNode();
    const char* selectedSegmentID = q->parameterSetNode()->GetSelectedSegmentID();
    if (!segmentationNode || !selectedSegmentID)
      {
      q->defaultModifierLabelmap();
      return;
      }

    // Get binary labelmap representation of selected segment
    vtkSegment* selectedSegment = segmentationNode->GetSegmentation()->GetSegment(selectedSegmentID);
    if (!selectedSegment)
      {
      vtkGenericWarningMacro("vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment: Invalid selected segment");
      return;
      }
    vtkOrientedImageData* segmentLabelmap = vtkOrientedImageData::SafeDownCast(
      selectedSegment->GetRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName()) );
    if (!segmentLabelmap)
      {
      vtkErrorWithObjectMacro(segmentationNode, "vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment: "
                                                "Failed to get binary labelmap representation in segmentation " << segmentationNode->GetName());
      return;
      }

    segmentLabelmap->DeepCopy(this->LastSelectedSegmentLabelmap);
    }

  if (!vtkOrientedImageDataResample::CalculateEffectiveExtent(ThresholdLastMask.GetPointer(), Extent))
    {
    this->UndoLastMask = false;
    return;
    }
  else
    {
    this->UndoLastMask = true;
    }

  q->modifySelectedSegmentByLabelmap(this->LastMask,
    qSlicerSegmentEditorAbstractEffect::ModificationModeAdd);

  q->CreateSurface();
}

//----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::onUnitNodeIntensityChanged(vtkObject *sender)
{
  if (!sender)
    {
    return;
    }

  vtkMRMLUnitNode* unitNodeIntensity = vtkMRMLUnitNode::SafeDownCast(sender);

  this->ThresholdRangeWidget->setDecimals(unitNodeIntensity->GetPrecision());
}

//----------------------------------------------------------------------------
double qSlicerSegmentEditorAstroCloudLassoEffectPrivate::GetSliceSpacing(qMRMLSliceWidget* sliceWidget)
{
  // Implementation copied from vtkSliceViewInteractorStyle::GetSliceSpacing()
  vtkMRMLSliceNode *sliceNode = sliceWidget->sliceLogic()->GetSliceNode();
  double spacing = 1.0;
  if (sliceNode->GetSliceSpacingMode() == vtkMRMLSliceNode::PrescribedSliceSpacingMode)
      {
    spacing = sliceNode->GetPrescribedSliceSpacing()[2];
      }
  else
      {
    spacing = sliceWidget->sliceLogic()->GetLowestVolumeSliceSpacing()[2];
      }
  return spacing;
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::updateBrushModel(qMRMLWidget* viewWidget, double brushPosition_World[3])
{
  double radiusMm = 1.0;
  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  if (!sliceWidget)
    {
    this->BrushSphereSource->SetRadius(radiusMm);
    this->BrushSphereSource->SetPhiResolution(16);
    this->BrushSphereSource->SetThetaResolution(16);
    this->BrushToWorldOriginTransformer->SetInputConnection(this->BrushSphereSource->GetOutputPort());
    }
  else
    {
    this->BrushCylinderSource->SetRadius(radiusMm);
    this->BrushCylinderSource->SetResolution(16);
    double sliceSpacingMm = this->GetSliceSpacing(sliceWidget);
    this->BrushCylinderSource->SetHeight(sliceSpacingMm);
    this->BrushCylinderSource->SetCenter(0, 0, sliceSpacingMm/2.0);
    this->BrushToWorldOriginTransformer->SetInputConnection(this->BrushCylinderSource->GetOutputPort());
    }

  vtkNew<vtkMatrix4x4> brushToWorldOriginTransformMatrix;
  if (sliceWidget)
    {
    // brush is rotated to the slice widget plane
    brushToWorldOriginTransformMatrix->DeepCopy(sliceWidget->sliceLogic()->GetSliceNode()->GetSliceToRAS());
    brushToWorldOriginTransformMatrix->SetElement(0,3, 0);
    brushToWorldOriginTransformMatrix->SetElement(1,3, 0);
    brushToWorldOriginTransformMatrix->SetElement(2,3, 0);
    }
  this->BrushToWorldOriginTransform->Identity();
  this->BrushToWorldOriginTransform->Concatenate(brushToWorldOriginTransformMatrix.GetPointer());
  this->BrushToWorldOriginTransform->RotateX(90); // cylinder's long axis is the Y axis, we need to rotate it to Z axis

  this->WorldOriginToWorldTransform->Identity();
  this->WorldOriginToWorldTransform->Translate(brushPosition_World);

  this->StripperFilter->Modified();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::updateBrush(qMRMLWidget* viewWidget, BrushPipeline* pipeline)
{
  pipeline->SetBrushVisibility(true);

  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  if (sliceWidget)
    {
    // Update slice cutting plane position and orientation
    vtkMatrix4x4* sliceXyToRas = sliceWidget->sliceLogic()->GetSliceNode()->GetXYToRAS();
    pipeline->SlicePlane->SetNormal(sliceXyToRas->GetElement(0,2),sliceXyToRas->GetElement(1,2),sliceXyToRas->GetElement(2,2));
    pipeline->SlicePlane->SetOrigin(sliceXyToRas->GetElement(0,3),sliceXyToRas->GetElement(1,3),sliceXyToRas->GetElement(2,3));

    vtkNew<vtkMatrix4x4> rasToSliceXy;
    vtkMatrix4x4::Invert(sliceXyToRas, rasToSliceXy.GetPointer());
    pipeline->WorldToSliceTransform->SetMatrix(rasToSliceXy.GetPointer());
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::updateBrushes()
{
  Q_Q(qSlicerSegmentEditorAstroCloudLassoEffect);
  // unusedWidgetPipelines will contain those widget pointers that are not in the layout anymore
  QList<qMRMLWidget*> unusedWidgetPipelines = this->BrushPipelines.keys();

  qSlicerLayoutManager* layoutManager = qSlicerApplication::application()->layoutManager();
  foreach(QString sliceViewName, layoutManager->sliceViewNames())
    {
    qMRMLSliceWidget* sliceWidget = layoutManager->sliceWidget(sliceViewName);
    unusedWidgetPipelines.removeOne(sliceWidget); // not an orphan

    BrushPipeline* brushPipeline = this->brushForWidget(sliceWidget);
    this->updateBrush(sliceWidget, brushPipeline);
    this->scheduleRender(sliceWidget);
    }
  for (int threeDViewId = 0; threeDViewId < layoutManager->threeDViewCount(); ++threeDViewId)
    {
    qMRMLThreeDWidget* threeDWidget = layoutManager->threeDWidget(threeDViewId);
    unusedWidgetPipelines.removeOne(threeDWidget); // not an orphan

    BrushPipeline* brushPipeline = this->brushForWidget(threeDWidget);
    this->updateBrush(threeDWidget, brushPipeline);
    this->scheduleRender(threeDWidget);
    }

  foreach (qMRMLWidget* viewWidget, unusedWidgetPipelines)
    {
    BrushPipeline* pipeline = this->BrushPipelines[viewWidget];
    BrushPipeline2D* pipeline2D = dynamic_cast<BrushPipeline2D*>(pipeline);
    BrushPipeline3D* pipeline3D = dynamic_cast<BrushPipeline3D*>(pipeline);
    if (pipeline2D)
      {
      q->removeActor2D(viewWidget, pipeline2D->BrushActor);
      q->removeActor2D(viewWidget, pipeline2D->FeedbackActor);
      }
    else if (pipeline3D)
      {
      q->removeActor3D(viewWidget, pipeline3D->BrushActor);
      q->removeActor3D(viewWidget, pipeline3D->FeedbackActor);
      }
    delete pipeline;
    this->BrushPipelines.remove(viewWidget);
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::clearBrushPipelines()
{
  Q_Q(qSlicerSegmentEditorAstroCloudLassoEffect);
  QMapIterator<qMRMLWidget*, BrushPipeline*> it(this->BrushPipelines);
  while (it.hasNext())
    {
    it.next();
    qMRMLWidget* viewWidget = it.key();
    BrushPipeline* brushPipeline = it.value();
    BrushPipeline2D* pipeline2D = dynamic_cast<BrushPipeline2D*>(brushPipeline);
    BrushPipeline3D* pipeline3D = dynamic_cast<BrushPipeline3D*>(brushPipeline);
    if (pipeline2D)
      {
      q->removeActor2D(viewWidget, pipeline2D->BrushActor);
      q->removeActor2D(viewWidget, pipeline2D->FeedbackActor);
      }
    else if (pipeline3D)
      {
      q->removeActor3D(viewWidget, pipeline3D->BrushActor);
      q->removeActor3D(viewWidget, pipeline3D->FeedbackActor);
      }
    delete brushPipeline;
    }
  this->BrushPipelines.clear();
}


//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// qSlicerSegmentEditorAstroCloudLassoEffect methods

//----------------------------------------------------------------------------
qSlicerSegmentEditorAstroCloudLassoEffect::qSlicerSegmentEditorAstroCloudLassoEffect(QObject* parent)
 : Superclass(parent)
 , d_ptr( new qSlicerSegmentEditorAstroCloudLassoEffectPrivate(*this) )
{
  this->m_Name = QString("AstroCloudLasso");
  this->m_Erase = false;
  this->m_ShowEffectCursorInThreeDView = true;
}

//----------------------------------------------------------------------------
qSlicerSegmentEditorAstroCloudLassoEffect::~qSlicerSegmentEditorAstroCloudLassoEffect()
{
}

//---------------------------------------------------------------------------
QIcon qSlicerSegmentEditorAstroCloudLassoEffect::icon()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  return d->CloudLassoIcon;
}

//---------------------------------------------------------------------------
QString const qSlicerSegmentEditorAstroCloudLassoEffect::helpText()const
{
  return QString("Right-click and drag in a slice or a 3D view to use respectively a 2-D or 3-D cloud lasso selection tool. "
                 "The initial lower threshold value is 3 RMS.");
}

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAbstractEffect* qSlicerSegmentEditorAstroCloudLassoEffect::clone()
{
  return new qSlicerSegmentEditorAstroCloudLassoEffect();
}

//---------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::deactivate()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  Superclass::deactivate();
  d->clearBrushPipelines();
}

//---------------------------------------------------------------------------
bool qSlicerSegmentEditorAstroCloudLassoEffect::processInteractionEvents(
  vtkRenderWindowInteractor* callerInteractor,
  unsigned long eid,
  qMRMLWidget* viewWidget )
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  bool abortEvent = false;

  // This effect only supports interactions in the 2D slice views currently
  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);
  BrushPipeline* brushPipeline = NULL;
  if (sliceWidget)
    {
    brushPipeline = d->brushForWidget(sliceWidget);
    }
  else if (threeDWidget)
    {
    brushPipeline = d->brushForWidget(threeDWidget);
    }
  if (!brushPipeline)
    {
    qCritical() << Q_FUNC_INFO << ": Failed to create brushPipeline";
    return abortEvent;
    }

  int eventPosition[2] = { 0, 0 };
  callerInteractor->GetEventPosition(eventPosition);

  double brushPosition_World[4] = { 0.0, 0.0, 0.0, 1.0 };
  if (sliceWidget)
    {
    double eventPositionXY[4] = {
      static_cast<double>(eventPosition[0]),
      static_cast<double>(eventPosition[1]),
      0.0,
      1.0};
    sliceWidget->sliceLogic()->GetSliceNode()->GetXYToRAS()->MultiplyPoint(eventPositionXY, brushPosition_World);
    }
  else if (threeDWidget)
    {
    vtkRenderer* renderer = qSlicerSegmentEditorAbstractEffect::renderer(viewWidget);
    if (!renderer)
      {
      return abortEvent;
      }
    double selectionX = eventPosition[0];
    double selectionY = eventPosition[1];
    double selectionZ = 0.;
    double cameraPos[4] = { 0., 0., 0., 0. }, cameraFP[4] = { 0., 0., 0., 0. };
    double *displayCoords, *worldCoords;
    double FinalPickPosition[4] = { 0., 0., 0., 1. };

    // Get camera focal point and position. Convert to display (screen)
    // coordinates. We need a depth value for z-buffer.

    vtkCamera *camera = renderer->GetActiveCamera();
    if (!camera)
      {
      return abortEvent;
      }
    camera->GetPosition(cameraPos);
    camera->GetFocalPoint(cameraFP);

    d->Normals[0] = cameraPos[0] - cameraFP[0];
    d->Normals[1] = cameraPos[1] - cameraFP[1];
    d->Normals[2] = cameraPos[2] - cameraFP[2];

    vtkMath::Normalize(d->Normals);

    int Extents[6];
    if (!this->masterVolumeImageData())
      {
      return abortEvent;
      }
    this->masterVolumeImageData()->GetExtent(Extents);

    d->distance = 0.;
    for (int ii = 0; ii < 6; ii++)
      {
      d->distance += Extents[ii] * Extents[ii] / 4.;
      }
    d->distance = sqrt(d->distance);

    for (int ii = 0 ; ii < 3 ; ii++)
      {
      FinalPickPosition[ii] =  cameraFP[ii] + (d->Normals[ii] * d->distance);
      }

    renderer->SetWorldPoint(FinalPickPosition[0], FinalPickPosition[1], FinalPickPosition[2], FinalPickPosition[3]);
    renderer->WorldToDisplay();
    displayCoords = renderer->GetDisplayPoint();
    selectionZ = displayCoords[2];

    // Convert the selection point into world coordinates.
    //
    renderer->SetDisplayPoint(selectionX, selectionY, selectionZ);
    renderer->DisplayToWorld();
    worldCoords = renderer->GetWorldPoint();
    for (int ii = 0; ii < 3; ii++)
      {
      brushPosition_World[ii] = worldCoords[ii];
      }
    }

  vtkIdType idPoint;
  if (eid == vtkCommand::LeftButtonPressEvent)
    {
    d->IsPainting = true;
    QList<qMRMLWidget*> viewWidgets = d->BrushPipelines.keys();
    foreach (qMRMLWidget* viewWidget, viewWidgets)
      {
      d->BrushPipelines[viewWidget]->SetFeedbackVisibility(d->DelayedPaint);
      }
    idPoint = d->paintAddPoint(brushPosition_World);
    abortEvent = true;
    }
  else if (eid == vtkCommand::LeftButtonReleaseEvent)
    {
    d->IsPainting = false;
    d->paintApply(viewWidget);

    QList<qMRMLWidget*> viewWidgets = d->BrushPipelines.keys();
    foreach (qMRMLWidget* viewWidget, viewWidgets)
      {
      d->BrushPipelines[viewWidget]->SetFeedbackVisibility(false);
      }
    }
  else if (eid == vtkCommand::MouseMoveEvent)
    {
    if (d->IsPainting)
      {
      idPoint = d->paintAddPoint(brushPosition_World);
      abortEvent = true;
      }
    }
  else if (eid == vtkCommand::EnterEvent)
    {
    brushPipeline->SetBrushVisibility(true);
    }
  else if (eid == vtkCommand::LeaveEvent)
    {
    brushPipeline->SetBrushVisibility(false);
    }
  else if (eid == vtkCommand::KeyPressEvent)
    {
    const char* key = callerInteractor->GetKeySym();
    double StepValue = this->doubleParameter("ThresholdSingleStep");
    double ThresholdMinimumValue = this->doubleParameter("ThresholdMinimumValue");
    double ThresholdMaximumValue = this->doubleParameter("ThresholdMaximumValue");
    bool eraseMode = this->integerParameter("EraseMode");
    bool automaticThresholdMode = this->integerParameter("AutomaticThresholdMode");

    if (!strcmp(key, "n") && !eraseMode)
      {
      ThresholdMinimumValue -= StepValue;
      this->setCommonParameter("ThresholdMinimumValue", ThresholdMinimumValue);
      this->updateGUIFromMRML();
      if (automaticThresholdMode)
        {
        d->reApplyPaint();
        }
      }
    if (!strcmp(key, "m") && !eraseMode)
      {
      ThresholdMinimumValue += StepValue;
      this->setCommonParameter("ThresholdMinimumValue", ThresholdMinimumValue);
      this->updateGUIFromMRML();
      if (automaticThresholdMode)
        {
        d->reApplyPaint();
        }
      }
    if (!strcmp(key, "k") && !eraseMode)
      {
      ThresholdMaximumValue -= StepValue;
      this->setCommonParameter("ThresholdMaximumValue", ThresholdMaximumValue);
      this->updateGUIFromMRML();
      if (automaticThresholdMode)
        {
        d->reApplyPaint();
        }
      }
    if (!strcmp(key, "l") && !eraseMode)
      {
      ThresholdMaximumValue += StepValue;
      this->setCommonParameter("ThresholdMaximumValue", ThresholdMaximumValue);
      this->updateGUIFromMRML();
      if (automaticThresholdMode)
        {
        d->reApplyPaint();
        }
      }
    if (!strcmp(key, "c"))
      {
      bool eraseMode = this->integerParameter("EraseMode");
      if (!eraseMode)
        {
        bool automaticThresholdMode = this->integerParameter("AutomaticThresholdMode");
        d->AutomaticThresholdCheckbox->setChecked(!automaticThresholdMode);
        }
      }
    if (!strcmp(key, "x"))
      {
      bool eraseMode = this->integerParameter("EraseMode");
      d->EraseModeCheckbox->setChecked(!eraseMode);
      }
    }

  // Update paint feedback glyph to follow mouse if there are at least two points
  if(idPoint <= 0)
    {
    return abortEvent;
    }

  d->updateBrushModel(viewWidget, brushPosition_World);
  d->updateBrushes();
  d->forceRender(viewWidget);
  return abortEvent;
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::processViewNodeEvents(
    vtkMRMLAbstractViewNode* callerViewNode,
    unsigned long eid,
    qMRMLWidget* viewWidget)
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  Q_UNUSED(callerViewNode);
  Q_UNUSED(eid);

  // This effect only supports interactions in the 2D slice views currently
  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);
  if (!sliceWidget && !threeDWidget)
    {
    return;
    }
  BrushPipeline* brushPipeline = d->brushForWidget(viewWidget);
  if (!brushPipeline)
    {
    qCritical() << Q_FUNC_INFO << ": Failed to create brushPipeline!";
    return;
    }

  d->updateBrush(viewWidget, brushPipeline);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::setupOptionsFrame()
{
  // Setup widgets corresponding to the parent class of this effect
  Superclass::setupOptionsFrame();

  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  d->init();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::setMRMLDefaults()
{
  Superclass::setMRMLDefaults();

  this->setCommonParameterDefault("EraseMode", 0);
  this->setCommonParameterDefault("AutomaticThresholdMode", 0);
  this->setCommonParameterDefault("ThresholdValue", 0.);
  this->setCommonParameterDefault("ThresholdSingleStep", 0.);
  this->setCommonParameterDefault("ThresholdMaximumValue", 0.);
  this->setCommonParameterDefault("ThresholdMinimumValue", 0.);
  this->setCommonParameterDefault("ThresholdMaximumValueLimit", 0.);
  this->setCommonParameterDefault("ThresholdMinimumValueLimit", 0.);
  this->setCommonParameterDefault("Threshold3RMSValue", 0.);
  this->setCommonParameterDefault("ThresholdDecimals", 0);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::masterVolumeNodeChanged()
{
  vtkMRMLScalarVolumeNode *masterVolume = this->parameterSetNode()->GetMasterVolumeNode();

  if (!masterVolume)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *astroMasterVolume = vtkMRMLAstroVolumeNode::SafeDownCast(masterVolume);

  if (!astroMasterVolume)
    {
    return;
    }

  double min = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.DATAMIN"));
  this->setCommonParameter("ThresholdMinimumValueLimit", min);
  double max = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.DATAMAX"));
  this->setCommonParameter("ThresholdMaximumValue", max);
  this->setCommonParameter("ThresholdMaximumValueLimit", max);

  double singleStep = (max - min) / 100.;
  this->setCommonParameter("ThresholdSingleStep", singleStep);

  vtkMRMLUnitNode* unitNodeIntensity = qSlicerCoreApplication::application()
    ->applicationLogic()->GetSelectionNode()->GetUnitNode("intensity");
  this->setCommonParameter("ThresholdDecimals", unitNodeIntensity->GetPrecision());

  double noise3 = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.RMS")) * 3.;
  this->setCommonParameter("ThresholdMinimumValue", noise3);
  this->setCommonParameter("Threshold3RMSValue", noise3);

  this->updateGUIFromMRML();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::CreateSurface()
{
  if (!this->parameterSetNode())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment editor parameter set node";
    return;
    }

  vtkMRMLSegmentationNode* segmentationNode = this->parameterSetNode()->GetSegmentationNode();
  if (!segmentationNode)
    {
    return;
    }
  vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(
    segmentationNode->GetDisplayNode());
  if (!displayNode)
    {
    return;
    }

  // Make sure closed surface representation exists
  if (segmentationNode->GetSegmentation()->CreateRepresentation(
    vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName() ))
    {
    // Set closed surface as displayed poly data representation
    displayNode->SetPreferredDisplayRepresentationName3D(
       vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName() );
     // But keep binary labelmap for 2D
    displayNode->SetPreferredDisplayRepresentationName2D(
      vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName() );
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::updateGUIFromMRML()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  if (!this->active())
    {
    // updateGUIFromMRML is called when the effect is activated
    return;
    }

  if (!this->scene())
    {
    return;
    }

  Superclass::updateGUIFromMRML();

  bool eraseMode = this->integerParameter("EraseMode");
  d->EraseModeCheckbox->blockSignals(true);
  d->EraseModeCheckbox->setChecked(eraseMode);
  d->EraseModeCheckbox->blockSignals(false);

  bool automaticThresholdMode = this->integerParameter("AutomaticThresholdMode");
  if (eraseMode)
    {
    automaticThresholdMode = false;
    }

  d->ThresholdRangeLabel->setEnabled(!eraseMode);
  d->AutomaticThresholdCheckbox->blockSignals(true);
  d->AutomaticThresholdCheckbox->setChecked(automaticThresholdMode);
  d->AutomaticThresholdCheckbox->setEnabled(!eraseMode);
  d->AutomaticThresholdCheckbox->blockSignals(false);

  double ThresholdSingleStep = this->doubleParameter("ThresholdSingleStep");
  double ThresholdMaximumValue = this->doubleParameter("ThresholdMaximumValue");
  double ThresholdMinimumValue = this->doubleParameter("ThresholdMinimumValue");
  double ThresholdMaximumValueLimit = this->doubleParameter("ThresholdMaximumValueLimit");
  double ThresholdMinimumValueLimit = this->doubleParameter("ThresholdMinimumValueLimit");
  int ThresholdDecimals = this->integerParameter("ThresholdDecimals");
  d->ThresholdRangeWidget->blockSignals(true);
  d->ThresholdRangeWidget->setMinimum(ThresholdMinimumValueLimit);
  d->ThresholdRangeWidget->setMaximum(ThresholdMaximumValueLimit);
  d->ThresholdRangeWidget->setSingleStep(ThresholdSingleStep);
  d->ThresholdRangeWidget->setDecimals(ThresholdDecimals);
  d->ThresholdRangeWidget->setValues(ThresholdMinimumValue, ThresholdMaximumValue);
  d->ThresholdRangeWidget->setEnabled(!eraseMode);
  d->ThresholdRangeWidget->blockSignals(false);

  int wasModifying = this->parameterSetNode()->StartModify();
  this->parameterSetNode()->SetMasterVolumeIntensityMask(true);
  this->parameterSetNode()->SetMasterVolumeIntensityMaskRange(ThresholdMinimumValue,
                                                              ThresholdMaximumValue);
  this->parameterSetNode()->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::updateMRMLFromGUI()
{
  Superclass::updateMRMLFromGUI();
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::onThresholdValueChanged(double min, double max)
{
  this->setCommonParameter("ThresholdMinimumValue", min);
  this->setCommonParameter("ThresholdMaximumValue", max);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::onAutomaticThresholdModeChanged(bool mode)
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  bool eraseMode = this->integerParameter("EraseMode");
  if (eraseMode)
    {
    mode = false;
    }
  this->setCommonParameter("AutomaticThresholdMode", mode);

  if (!d->ThresholdRangeWidget)
    {
    return;
    }

  if (mode)
    {
    QObject::connect(d->ThresholdRangeWidget, SIGNAL(valuesChanged(double, double)), d, SLOT(reApplyPaint()));
    }
  else
    {
    QObject::disconnect(d->ThresholdRangeWidget, SIGNAL(valuesChanged(double, double)), d, SLOT(reApplyPaint()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::onEraseModeChanged(bool mode)
{
  this->m_Erase = mode;
  this->setCommonParameter("EraseMode", (int)mode);
  if (mode)
    {
    this->onAutomaticThresholdModeChanged(false);
    }

  double ThresholdValue = 0.;
  if (mode)
    {
    ThresholdValue = this->doubleParameter("ThresholdMinimumValueLimit");
    }
  else
    {
    ThresholdValue = this->doubleParameter("Threshold3RMSValue");
    }
  this->onThresholdValueChanged(ThresholdValue, this->doubleParameter("ThresholdMaximumValueLimit"));
}
