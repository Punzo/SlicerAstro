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

// CTK includes
#include <ctkRangeWidget.h>

// VTK includes
#include <vtkActor.h>
#include <vtkActor2D.h>
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
  , ActiveViewWidget(NULL)
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
  this->ClosedSurfacePoints = vtkSmartPointer<vtkPoints>::New();
  this->ClosedSurfaceStrips = vtkSmartPointer<vtkCellArray>::New();
  this->ClosedSurfacePolys = vtkSmartPointer<vtkCellArray>::New();
  this->CloudLasso3DSelectionPolyData = vtkSmartPointer<vtkPolyData>::New();
  this->CloudLasso3DSelectionPolyData->SetPoints(this->ClosedSurfacePoints);
  this->CloudLasso3DSelectionPolyData->SetStrips(this->ClosedSurfaceStrips);
  this->CloudLasso3DSelectionPolyData->SetPolys(this->ClosedSurfacePolys);

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

  this->FeedbackTubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
  this->FeedbackTubeFilter->SetInputConnection(this->StripperFilter->GetOutputPort());
  this->FeedbackTubeFilter->SetRadius(0.5);
  this->FeedbackTubeFilter->SetNumberOfSides(16);
  this->FeedbackTubeFilter->CappingOn();

  this->ActiveViewLastInteractionPosition[0] = 0;
  this->ActiveViewLastInteractionPosition[1] = 0;
}

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAstroCloudLassoEffectPrivate::~qSlicerSegmentEditorAstroCloudLassoEffectPrivate()
{
  Q_Q(qSlicerSegmentEditorAstroCloudLassoEffect);
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
  this->ThresholdRangeWidget->setToolTip("list of shortcut keys: \n"
                                         "'n' decrease the minimum threshold value; \n"
                                         "'m' increase the minimum threshold value; \n"
                                         "'k' decrease the maximum threshold value; \n"
                                         "'l' increase the maximum threshold value. \n");
  q->addOptionsWidget(this->ThresholdRangeWidget);

  this->AutomaticThresholdCheckbox = new QCheckBox("Automatic Threshold Updating Mode");
  this->AutomaticThresholdCheckbox->setToolTip("Activate or deactivate automatic updating "
                                               "of the selection when chaning the threshold value. "
                                               "If active: draw a 2D or 3D cloudlasso, then, change "
                                               "the threshold value. It is not possible to activate it "
                                               "in Erase Mode. The shortcut key is 'c'.");
  q->addOptionsWidget(this->AutomaticThresholdCheckbox);

  this->EraseModeCheckbox = new QCheckBox("Erase Mode");
  this->EraseModeCheckbox->setToolTip("Activate or deactivate Erase Mode. The shortcut key is 'x'.");
  q->addOptionsWidget(this->EraseModeCheckbox);

  QObject::connect(this->ThresholdRangeWidget, SIGNAL(valuesChanged(double,double)), q, SLOT(onThresholdValueChanged(double, double)));
  QObject::connect(this->EraseModeCheckbox, SIGNAL(clicked(bool)), q, SLOT(onEraseModeChanged(bool)));
  QObject::connect(this->AutomaticThresholdCheckbox, SIGNAL(clicked(bool)), q, SLOT(onAutomaticThresholdModeChanged(bool)));

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
vtkIdType qSlicerSegmentEditorAstroCloudLassoEffectPrivate::paintAddTwoPoints(double brushPosition_World[3])
{
  vtkIdType idPointFirst = this->PaintCoordinates_World->InsertNextPoint(brushPosition_World[0] + (brushPosition_World[0]/10000000.),
                                                                         brushPosition_World[1] + (brushPosition_World[1]/10000000.),
                                                                         brushPosition_World[2] + (brushPosition_World[2]/10000000.));
  vtkIdType idPointSecond = this->PaintCoordinates_World->InsertNextPoint(brushPosition_World);

  this->CloudLasso3DSelectionPoints->InsertNextPoint(brushPosition_World);

  vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
  line->GetPointIds()->SetId(0, idPointFirst);
  line->GetPointIds()->SetId(1, idPointSecond);
  this->PaintLines_World->InsertNextCell(line);
  return idPointSecond;
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

  vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
  line->GetPointIds()->SetId(0, idPoint - 1);
  line->GetPointIds()->SetId(1, idPoint);
  this->PaintLines_World->InsertNextCell(line);

  if (idPoint == 1)
    {
     vtkSmartPointer<vtkLine> closingLine = vtkSmartPointer<vtkLine>::New();
     closingLine->GetPointIds()->SetId(0, idPoint);
     closingLine->GetPointIds()->SetId(1, 0);
     this->PaintLines_World->InsertNextCell(closingLine);
    }
  else
    {
    this->PaintLines_World->ReplaceCell(1, 1, &idPoint);
    }
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
  QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));

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

    // Create a 2D polyMaskImage from the FeedbackPolyData
    q->appendPolyMask(modifierLabelmap, this->WorldToXYTransformer->GetOutput(), sliceWidget);
    }
  if (threeDWidget)
    {
    // Create a 3D closed surface from the FeedbackPolyData (tube poly data) and store it in CloudLasso3DSelectionPolyData
    this->createClosedSurfacePolyMask(viewWidget);

    // Apply RAS -> IJK transform to CloudLasso3DSelectionPolyData and updating BrushPolyDataToStencil
    this->updateBrushStencil(viewWidget);

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

  q->modifySelectedSegmentByLabelmap(modifierLabelmap, modificationMode, modifierLabelmap->GetExtent());

  this->PaintCoordinates_World->Reset();
  this->PaintLines_World->Reset();
  this->CloudLasso3DSelectionPoints->Reset();
  this->ClosedSurfacePoints->Reset();
  this->ClosedSurfaceStrips->Reset();
  this->ClosedSurfacePolys->Reset();

  q->CreateSurface(true);

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

  int Extent[6];
  modifierLabelmap->GetExtent(Extent);

  // X dimension
  if (floor(boundsIjk[0])-1 > Extent[0] &&
      floor(boundsIjk[0])-1 < Extent[1])
    {
    Extent[0] = floor(boundsIjk[0])-1;
    }
  if (ceil(boundsIjk[1])+1 > Extent[0] &&
      ceil(boundsIjk[1])+1 < Extent[1])
    {
    Extent[1] = ceil(boundsIjk[1])+1;
    }

  // Y dimension
  if (floor(boundsIjk[2])-1 > Extent[2] &&
      floor(boundsIjk[2])-1 < Extent[3])
    {
    Extent[2] = floor(boundsIjk[2])-1;
    }
  if (ceil(boundsIjk[3])+1 > Extent[2] &&
      ceil(boundsIjk[3])+1 < Extent[3])
    {
    Extent[3] = ceil(boundsIjk[3])+1;
    }

  // Z dimension
  if (floor(boundsIjk[4])-1 > Extent[4] &&
      floor(boundsIjk[4])-1 < Extent[5])
    {
    Extent[4] = floor(boundsIjk[4])-1;
    }
  if (ceil(boundsIjk[5])+1 > Extent[4] &&
      ceil(boundsIjk[5])+1 < Extent[5])
    {
    Extent[5] = ceil(boundsIjk[5])+1;
    }

  this->BrushPolyDataToStencil->SetOutputWholeExtent(Extent);
  this->BrushPolyDataToStencil->Update();
}

//----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffectPrivate::createClosedSurfacePolyMask(qMRMLWidget* viewWidget)
{
  Q_Q(qSlicerSegmentEditorAstroCloudLassoEffect);

  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);
  if (!threeDWidget)
    {
    return;
    }

  // Get bounds of modifier labelmap
  if (!q->parameterSetNode())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment editor parameter set node";
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
  vtkNew<vtkMatrix4x4> segmentationToWorldMatrix;
  // We don't support painting in non-linearly transformed node (it could be implemented, but would probably slow down things too much)
  // TODO: show a meaningful error message to the user if attempted
  vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(segmentationNode->GetParentTransformNode(), NULL, segmentationToWorldMatrix.GetPointer());

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

  // Camera parameters
  // Camera position
  double cameraPos[4] = { 0 };
  camera->GetPosition(cameraPos);
  cameraPos[3] = 1.0;
  // Focal point position
  double cameraFP[4] = { 0 };
  camera->GetFocalPoint(cameraFP);
  cameraFP[3] = 1.0;
  // Direction of projection
  double cameraDOP[3] = { 0 };
  for (int i = 0; i < 3; i++)
    {
    cameraDOP[i] = cameraFP[i] - cameraPos[i];
    }
  vtkMath::Normalize(cameraDOP);
  // Camera view up
  double cameraViewUp[3] = { 0 };
  camera->GetViewUp(cameraViewUp);
  vtkMath::Normalize(cameraViewUp);

  renderer->SetWorldPoint(cameraFP[0], cameraFP[1], cameraFP[2], cameraFP[3]);
  renderer->WorldToDisplay();

  // Get modifier labelmap extent in camera coordinate system to know how much we
  // have to cut through
  vtkNew<vtkMatrix4x4> cameraToWorldMatrix;
  double cameraViewRight[3] = { 1, 0, 0 };
  vtkMath::Cross(cameraDOP, cameraViewUp, cameraViewRight);
  for (int i = 0; i < 3; i++)
    {
    cameraToWorldMatrix->SetElement(i, 3, cameraPos[i]);
    cameraToWorldMatrix->SetElement(i, 0, cameraViewUp[i]);
    cameraToWorldMatrix->SetElement(i, 1, cameraViewRight[i]);
    cameraToWorldMatrix->SetElement(i, 2, cameraDOP[i]);
    }
  vtkNew<vtkMatrix4x4> worldToCameraMatrix;
  vtkMatrix4x4::Invert(cameraToWorldMatrix.GetPointer(), worldToCameraMatrix.GetPointer());
  vtkNew<vtkTransform> segmentationToCameraTransform;
  segmentationToCameraTransform->Concatenate(worldToCameraMatrix.GetPointer());
  segmentationToCameraTransform->Concatenate(segmentationToWorldMatrix.GetPointer());
  double segmentationBounds_Camera[6] = { 0, -1, 0, -1, 0, -1 };
  vtkOrientedImageDataResample::TransformOrientedImageDataBounds(modifierLabelmap, segmentationToCameraTransform.GetPointer(), segmentationBounds_Camera);
  double clipRangeFromModifierLabelmap[2] =
    {
    std::min(segmentationBounds_Camera[4], segmentationBounds_Camera[5]),
    std::max(segmentationBounds_Camera[4], segmentationBounds_Camera[5])
    };
  // Extend bounds by half slice to make sure the boundaries are included
  clipRangeFromModifierLabelmap[0] -= 0.5;
  clipRangeFromModifierLabelmap[1] += 0.5;

  // Clip what we see on the camera but reduce it to the modifier labelmap's range
  // to keep the stencil as small as possible
  double* clipRangeFromCamera = camera->GetClippingRange();
  double clipRange[2] =
    {
    std::max(clipRangeFromModifierLabelmap[0], clipRangeFromCamera[0]),
    std::min(clipRangeFromModifierLabelmap[1], clipRangeFromCamera[1]),
    };

  for (int pointIndex = 0; pointIndex < numberOfPoints; pointIndex++)
    {
    // Convert the selection point into world coordinates.
    //
    double pickPosition[3] = { 0 };
    this->CloudLasso3DSelectionPoints->GetPoint(pointIndex, pickPosition);

    //  Compute the ray endpoints.  The ray is along the line running from
    //  the camera position to the selection point, starting where this line
    //  intersects the front clipping plane, and terminating where this
    //  line intersects the back clipping plane.
    double ray[3] = { 0 };
    for (int ii = 0; ii < 3; ii++)
      {
      ray[ii] = pickPosition[ii] - cameraPos[ii];
      }

    double rayLength = vtkMath::Dot(cameraDOP, ray);
    if (rayLength == 0.0)
      {
      qWarning() << Q_FUNC_INFO << ": Cannot process points";
      return;
      }

    double p1World[4] = { 0 };
    double p2World[4] = { 0 };
    double tF = 0;
    double tB = 0;
    if (camera->GetParallelProjection())
      {
      tF = clipRange[0] - rayLength;
      tB = clipRange[1] - rayLength;
      for (int ii = 0; ii < 3; ii++)
        {
        p1World[ii] = pickPosition[ii] + tF*cameraDOP[ii];
        p2World[ii] = pickPosition[ii] + tB*cameraDOP[ii];
        }
      }
    else
      {
      tF = clipRange[0] / rayLength;
      tB = clipRange[1] / rayLength;
      for (int i = 0; i < 3; i++)
        {
        p1World[i] = cameraPos[i] + tF*ray[i];
        p2World[i] = cameraPos[i] + tB*ray[i];
        }
      }
    p1World[3] = p2World[3] = 1.0;

    this->ClosedSurfacePoints->InsertNextPoint(p1World);
    this->ClosedSurfacePoints->InsertNextPoint(p2World);
    }

  // Construct polydata
  // Skirt
  this->ClosedSurfaceStrips->InsertNextCell(numberOfPoints * 2 + 2);
  for (int ii = 0; ii < numberOfPoints * 2; ii++)
    {
    this->ClosedSurfaceStrips->InsertCellPoint(ii);
    }
  this->ClosedSurfaceStrips->InsertCellPoint(0);
  this->ClosedSurfaceStrips->InsertCellPoint(1);
  // Front cap
  this->ClosedSurfacePolys->InsertNextCell(numberOfPoints);
  for (int ii = 0; ii < numberOfPoints; ii++)
    {
    this->ClosedSurfacePolys->InsertCellPoint(ii * 2);
    }
  // Back cap
  this->ClosedSurfacePolys->InsertNextCell(numberOfPoints);
  for (int ii = 0; ii < numberOfPoints; ii++)
    {
    this->ClosedSurfacePolys->InsertCellPoint(ii * 2 + 1);
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

  if (!vtkOrientedImageDataResample::CalculateEffectiveExtent(ThresholdLastMask.GetPointer(), Extent))
    {
    this->UndoLastMask = false;
    return;
    }
  else
    {
    this->UndoLastMask = true;
    }

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

  q->modifySelectedSegmentByLabelmap(this->LastMask,
    qSlicerSegmentEditorAbstractEffect::ModificationModeAdd);

  q->CreateSurface(true);
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
  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  if (!sliceWidget)
    {
    this->BrushSphereSource->SetRadius(0.4);
    this->BrushSphereSource->SetPhiResolution(16);
    this->BrushSphereSource->SetThetaResolution(16);
    this->BrushToWorldOriginTransformer->SetInputConnection(this->BrushSphereSource->GetOutputPort());
    }
  else
    {
    this->BrushCylinderSource->SetRadius(1.0);
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
  if (this->BrushToWorldOriginTransformer->GetNumberOfInputConnections(0) == 0)
    {
    pipeline->SetBrushVisibility(false);
    return;
    }
  pipeline->SetBrushVisibility(this->ActiveViewWidget != NULL);

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
    qSlicerSegmentEditorAbstractEffect::scheduleRender(sliceWidget);
    }
  for (int threeDViewId = 0; threeDViewId < layoutManager->threeDViewCount(); ++threeDViewId)
    {
    qMRMLThreeDWidget* threeDWidget = layoutManager->threeDWidget(threeDViewId);
    unusedWidgetPipelines.removeOne(threeDWidget); // not an orphan

    BrushPipeline* brushPipeline = this->brushForWidget(threeDWidget);
    this->updateBrush(threeDWidget, brushPipeline);
    qSlicerSegmentEditorAbstractEffect::scheduleRender(threeDWidget);
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

//---------------------------------------------------------------------------
bool qSlicerSegmentEditorAstroCloudLassoEffectPrivate::brushPositionInWorld(qMRMLWidget* viewWidget, int brushPositionInView[2], double brushPosition_World[3])
{
  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);

  if (sliceWidget)
    {
    double eventPositionXY[4] = {
      static_cast<double>(brushPositionInView[0]),
      static_cast<double>(brushPositionInView[1]),
      0.0,
      1.0};
    sliceWidget->sliceLogic()->GetSliceNode()->GetXYToRAS()->MultiplyPoint(eventPositionXY, brushPosition_World);
    }
  else if (threeDWidget)
    {
    vtkRenderer* renderer = qSlicerSegmentEditorAbstractEffect::renderer(viewWidget);
    if (!renderer)
      {
      return false;
      }
    static bool useCellPicker = true;
    if (useCellPicker)
      {
      vtkNew<vtkCellPicker> picker;
      picker->SetTolerance( .005 );
      if (!picker->Pick(brushPositionInView[0], brushPositionInView[1], 0, renderer))
        {
        return false;
        }

      vtkPoints* pickPositions = picker->GetPickedPositions();
      int numberOfPickedPositions = pickPositions->GetNumberOfPoints();
      if (numberOfPickedPositions<1)
        {
        return false;
        }
      double cameraPosition[3]={0,0,0};
      renderer->GetActiveCamera()->GetPosition(cameraPosition);
      pickPositions->GetPoint(0, brushPosition_World);
      double minDist2 = vtkMath::Distance2BetweenPoints(brushPosition_World, cameraPosition);
      for (int i=1; i<numberOfPickedPositions; i++)
        {
        double currentMinDist2 = vtkMath::Distance2BetweenPoints(pickPositions->GetPoint(i), cameraPosition);
        if (currentMinDist2<minDist2)
          {
          pickPositions->GetPoint(i, brushPosition_World);
          minDist2 = currentMinDist2;
          }
        }
      }
    else
      {
      vtkNew<vtkPropPicker> picker;
      if (!picker->Pick(brushPositionInView[0], brushPositionInView[1], 0, renderer))
        {
        return false;
        }
      picker->GetPickPosition(brushPosition_World);
      }
    }

  return true;
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
  return QString("Left-click and drag in a slice or 3D view to use respectively a 2D or 3D cloud lasso selection tool. "
                 "The initial lower threshold value is 3 times the DisplayThreshold value set in the AstroVolume Module.");
}

//-----------------------------------------------------------------------------
qSlicerSegmentEditorAbstractEffect* qSlicerSegmentEditorAstroCloudLassoEffect::clone()
{
  return new qSlicerSegmentEditorAstroCloudLassoEffect();
}

//---------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::activate()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  Superclass::activate();
  this->masterVolumeNodeChanged();
  d->PaintCoordinates_World->Initialize();
  d->PaintLines_World->Initialize();
  d->CloudLasso3DSelectionPoints->Initialize();
  d->ClosedSurfacePoints->Initialize();
  d->ClosedSurfaceStrips->Initialize();
  d->ClosedSurfacePolys->Initialize();
}

//---------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::deactivate()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  Superclass::deactivate();
  this->parameterSetNode()->SetMasterVolumeIntensityMask(false);
  d->clearBrushPipelines();
  d->PaintCoordinates_World->Initialize();
  d->PaintLines_World->Initialize();
  d->CloudLasso3DSelectionPoints->Initialize();
  d->ClosedSurfacePoints->Initialize();
  d->ClosedSurfaceStrips->Initialize();
  d->ClosedSurfacePolys->Initialize();
  d->ActiveViewWidget = NULL;
}

//---------------------------------------------------------------------------
bool qSlicerSegmentEditorAstroCloudLassoEffect::processInteractionEvents(
  vtkRenderWindowInteractor* callerInteractor,
  unsigned long eid,
  qMRMLWidget* viewWidget )
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);
  bool abortEvent = false;

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

  d->ActiveViewWidget = viewWidget;

  int eventPosition[2] = { 0, 0 };
  callerInteractor->GetEventPosition(eventPosition);
  d->ActiveViewLastInteractionPosition[0] = eventPosition[0];
  d->ActiveViewLastInteractionPosition[1] = eventPosition[1];

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
    for (int ii = 0; ii < 3; ii++)
      {
      d->distance += fabs(cameraPos[ii]);
      }

    for (int ii = 0 ; ii < 3 ; ii++)
      {
      FinalPickPosition[ii] =  cameraPos[ii] - (d->Normals[ii] * 150);
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

    // reset the 3D rendering boundaries
    renderer->ResetCameraClippingRange();
    renderer->Render();
    }

  if (eid == vtkCommand::LeftButtonPressEvent)
    {
    d->IsPainting = true;
    QList<qMRMLWidget*> viewWidgets = d->BrushPipelines.keys();
    foreach (qMRMLWidget* viewWidget, viewWidgets)
      {
      d->BrushPipelines[viewWidget]->SetFeedbackVisibility(d->DelayedPaint);
      }
    d->paintAddTwoPoints(brushPosition_World);
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
      d->paintAddPoint(brushPosition_World);
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
    d->ActiveViewWidget = NULL;
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
      bool automaticThresholdMode = this->integerParameter("AutomaticThresholdMode");
      this->onAutomaticThresholdModeChanged(!automaticThresholdMode);
      }
    if (!strcmp(key, "x"))
      {
      bool eraseMode = this->integerParameter("EraseMode");
      this->onEraseModeChanged(!eraseMode);
      }
    }

  d->updateBrushModel(viewWidget, brushPosition_World);
  d->updateBrushes();

  qSlicerSegmentEditorAbstractEffect::forceRender(viewWidget);
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

    if (viewWidget == d->ActiveViewWidget)
      {
      double brushPosition_World[4] = { 0.0, 0.0, 0.0, 1.0 };
      if (d->brushPositionInWorld(viewWidget, d->ActiveViewLastInteractionPosition, brushPosition_World))
        {
        d->updateBrushModel(viewWidget, brushPosition_World);
        }
      else
        {
        d->updateBrushModel(viewWidget, NULL);
        }
      d->updateBrushes();
      qSlicerSegmentEditorAbstractEffect::scheduleRender(d->ActiveViewWidget);
      }

    d->updateBrush(viewWidget, brushPipeline);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::setupOptionsFrame()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  // Setup widgets corresponding to the parent class of this effect
  Superclass::setupOptionsFrame();

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
  this->setCommonParameterDefault("Threshold3DisplayThresholdValue", 0.);
  this->setCommonParameterDefault("ThresholdDecimals", 0);
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::masterVolumeNodeChanged()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  if (!this->parameterSetNode())
    {
    return;
    }

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

  d->ThresholdRangeWidget->reset();
  d->ThresholdRangeWidget->setRange(min, max);

  double singleStep = (max - min) / 100.;
  this->setCommonParameter("ThresholdSingleStep", singleStep);

  vtkMRMLUnitNode* unitNodeIntensity = qSlicerCoreApplication::application()
    ->applicationLogic()->GetSelectionNode()->GetUnitNode("intensity");
  this->setCommonParameter("ThresholdDecimals", unitNodeIntensity->GetPrecision());

  double noise3 = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.DisplayThreshold")) * 3.;

  if (noise3 != 0.)
    {
    this->setCommonParameter("ThresholdMinimumValue", noise3);
    this->setCommonParameter("Threshold3DisplayThresholdValue", noise3);
    d->ThresholdRangeWidget->setMinimumValue(noise3);
    d->ThresholdRangeWidget->setMaximumValue(max);
    }
  else
    {
    this->setCommonParameter("ThresholdMinimumValue", min);
    this->setCommonParameter("Threshold3DisplayThresholdValue", min);
    d->ThresholdRangeWidget->setMinimumValue(min);
    d->ThresholdRangeWidget->setMaximumValue(max);
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::CreateSurface(bool on)
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

  // If just have been checked, then create closed surface representation and show it
  if (on)
    {
    // Make sure closed surface representation exists
    if (segmentationNode->GetSegmentation()->CreateRepresentation(
      vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName() ))
      {
      // Set closed surface as displayed poly data representation
      displayNode->SetPreferredDisplayRepresentationName3D(
        vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName() );
      // But keep binary labelmap for 2D
      bool binaryLabelmapPresent = segmentationNode->GetSegmentation()->ContainsRepresentation(
        vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName());
      if (binaryLabelmapPresent)
        {
        displayNode->SetPreferredDisplayRepresentationName2D(
          vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName() );
        }
      }
    }
  // If unchecked, then remove representation (but only if it's not the master representation)
  else if (segmentationNode->GetSegmentation()->GetMasterRepresentationName() !=
    vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName())
    {
    segmentationNode->GetSegmentation()->RemoveRepresentation(
      vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());
    }
}

//-----------------------------------------------------------------------------
void qSlicerSegmentEditorAstroCloudLassoEffect::updateGUIFromMRML()
{
  Q_D(qSlicerSegmentEditorAstroCloudLassoEffect);

  if (!this->active() || !this->parameterSetNode() || !this->scene())
    {
    return;
    }

  Superclass::updateGUIFromMRML();

  bool eraseMode = this->integerParameter("EraseMode");
  d->EraseModeCheckbox->setChecked(eraseMode);

  bool automaticThresholdMode = this->integerParameter("AutomaticThresholdMode");
  if (eraseMode)
    {
    automaticThresholdMode = false;
    }
  d->AutomaticThresholdCheckbox->setChecked(automaticThresholdMode);
  d->AutomaticThresholdCheckbox->setEnabled(!eraseMode);

  double ThresholdSingleStep = this->doubleParameter("ThresholdSingleStep");
  double ThresholdMaximumValue = this->doubleParameter("ThresholdMaximumValue");
  double ThresholdMinimumValue = this->doubleParameter("ThresholdMinimumValue");
  int ThresholdDecimals = this->integerParameter("ThresholdDecimals");

  d->ThresholdRangeWidget->setMinimumValue(ThresholdMinimumValue);
  d->ThresholdRangeWidget->setMaximumValue(ThresholdMaximumValue);
  d->ThresholdRangeWidget->setSingleStep(ThresholdSingleStep);
  d->ThresholdRangeWidget->setDecimals(ThresholdDecimals);

  int wasModifying = this->parameterSetNode()->StartModify();
  this->parameterSetNode()->SetMasterVolumeIntensityMask(true);
  this->parameterSetNode()->SetMasterVolumeIntensityMaskRange(ThresholdMinimumValue,
                                                              ThresholdMaximumValue);
  this->parameterSetNode()->EndModify(wasModifying);
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
    ThresholdValue = this->doubleParameter("Threshold3DisplayThresholdValue");
    }
  this->setCommonParameter("ThresholdMinimumValue", ThresholdValue);

  if (!this->parameterSetNode())
    {
    return;
    }

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

  double max = StringToDouble(astroMasterVolume->GetAttribute("SlicerAstro.DATAMAX"));
  this->setCommonParameter("ThresholdMaximumValue", max);
  this->updateGUIFromMRML();
}
