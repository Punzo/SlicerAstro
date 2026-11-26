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

// MRMLDisplayableManager includes
#include "vtkMRMLAstroBeamDisplayableManager.h"

// MRML includes
#include <vtkMRMLAbstractViewNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLViewNode.h>

// VTK includes
#include <vtkActor2D.h>
#include <vtkActor2DCollection.h>
#include <vtkAxisActor2D.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkLine.h>
#include <vtksys/SystemTools.hxx>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkTransform.h>
#include <vtkMath.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <vector>

// SlicerQt includes
#include <qSlicerApplication.h>

// vtkSlicer includes
#include <vtkSlicerApplicationLogic.h>

// Constants
static const int RENDERER_LAYER = 1; // layer ID where the orientation marker will be displayed
// view pixels: margins of the labels of the 2D axes (left and bottom sides,
// vtkMRMLAstroTwoDAxesDisplayableManager: labels at 20 px from the left side
// and 15 px above the bottom side, 12 pt font), padding of the beam
static const double AXES_LEFT_MARGIN = 80.;
static const double AXES_BOTTOM_MARGIN = 34.;
static const double CORNER_PADDING = 10.;
const double PI_2  = PI * 0.5;

//---------------------------------------------------------------------------
class vtkAstroBeamRendererUpdateObserver : public vtkCommand
{
public:
  static vtkAstroBeamRendererUpdateObserver *New()
    {
    return new vtkAstroBeamRendererUpdateObserver;
    }
  vtkAstroBeamRendererUpdateObserver()
    {
    this->DisplayableManager = nullptr;
    }
  virtual void Execute(vtkObject* vtkNotUsed(wdg), unsigned long vtkNotUsed(event), void* vtkNotUsed(calldata))
    {
    if (this->DisplayableManager)
      {
      this->DisplayableManager->UpdateFromRenderer();
      }
  }
  vtkWeakPointer<vtkMRMLAstroBeamDisplayableManager> DisplayableManager;
};


//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLAstroBeamDisplayableManager);

//---------------------------------------------------------------------------
class vtkMRMLAstroBeamDisplayableManager::vtkInternal
{
public:
  vtkInternal(vtkMRMLAstroBeamDisplayableManager * external);
  ~vtkInternal();

  void SetupMarkerRenderer();
  void AddRendererUpdateObserver(vtkRenderer* renderer);
  void RemoveRendererUpdateObserver();

  void SetupBeam();
  void UpdateBeam();
  void ShowActors(bool show);
  /// Points of the beam ellipse in view coordinates, in the bottom-left
  /// corner of the visible data; false if it cannot be drawn there
  bool ComputeBeamPoints(vtkMRMLSliceNode* sliceNode, vtkMRMLAstroVolumeNode* volumeNode,
                         vtkMRMLSliceLayerLogic* layerLogic);

  vtkSmartPointer<vtkRenderer> MarkerRenderer;
  vtkSmartPointer<vtkAstroBeamRendererUpdateObserver> RendererUpdateObserver;
  vtkSmartPointer<vtkPoints> beamPoints;
  vtkSmartPointer<vtkCellArray> beamCellArray;
  vtkSmartPointer<vtkPolyData> beamPolyData;
  vtkSmartPointer<vtkActor2D> beamActor;
  vtkSmartPointer<vtkPolyDataMapper2D> beamMapper;
  vtkSmartPointer<vtkCellArray> beamFillCellArray;
  vtkSmartPointer<vtkPolyData> beamFillPolyData;
  vtkSmartPointer<vtkActor2D> beamFillActor;
  vtkSmartPointer<vtkPolyDataMapper2D> beamFillMapper;
  /// "beam", right of the ellipse
  vtkSmartPointer<vtkTextActor> beamLabel;

  int RendererUpdateObservationId;
  vtkWeakPointer<vtkRenderer> ObservedRenderer;

  bool ActorsAddedToRenderer;

  vtkSmartPointer<vtkDoubleArray> Color;
  int RulerColor{-1};     // ruler colour of the view node used for Color
  bool CustomColor{false}; // Color set with SetAnnotationsColor
  static const double COLOR_INVALID[3];

  vtkMRMLAstroBeamDisplayableManager* External;

  qSlicerApplication* app;
};

//----------------------------------------------------------------------------
const double vtkMRMLAstroBeamDisplayableManager::vtkInternal::COLOR_INVALID[3] = {1., 0.731, 0.078};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLAstroBeamDisplayableManager::vtkInternal::vtkInternal(vtkMRMLAstroBeamDisplayableManager * external)
{
  this->External = external;
  this->RendererUpdateObserver = vtkSmartPointer<vtkAstroBeamRendererUpdateObserver>::New();
  this->RendererUpdateObserver->DisplayableManager = this->External;
  this->RendererUpdateObservationId = 0;
  this->ActorsAddedToRenderer = false;
  this->MarkerRenderer = vtkSmartPointer<vtkRenderer>::New();
  this->beamPoints = vtkSmartPointer<vtkPoints>::New();
  this->beamCellArray = vtkSmartPointer<vtkCellArray>::New();
  this->beamPolyData = vtkSmartPointer<vtkPolyData>::New();
  this->beamActor = vtkSmartPointer<vtkActor2D>::New();
  this->beamMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  this->beamFillCellArray = vtkSmartPointer<vtkCellArray>::New();
  this->beamFillPolyData = vtkSmartPointer<vtkPolyData>::New();
  this->beamFillActor = vtkSmartPointer<vtkActor2D>::New();
  this->beamFillMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  this->beamLabel = vtkSmartPointer<vtkTextActor>::New();
  this->beamLabel->SetInput("beam");
  this->beamLabel->GetTextProperty()->SetFontSize(12);
  this->beamLabel->GetTextProperty()->SetFontFamilyToArial();
  this->beamLabel->GetTextProperty()->SetJustificationToLeft();
  this->beamLabel->GetTextProperty()->SetVerticalJustificationToCentered();
  this->app = nullptr;
  this->Color = vtkSmartPointer<vtkDoubleArray>::New();
  this->Color->SetNumberOfValues(3);
  this->Color->SetValue(0, COLOR_INVALID[0]);
  this->Color->SetValue(1, COLOR_INVALID[1]);
  this->Color->SetValue(2, COLOR_INVALID[2]);
}

//---------------------------------------------------------------------------
vtkMRMLAstroBeamDisplayableManager::vtkInternal::~vtkInternal()
{
  RemoveRendererUpdateObserver();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::vtkInternal::ShowActors(bool show)
{
  if (this->ActorsAddedToRenderer == show)
    {
    // no change
    return;
    }
  if (show)
    {
    this->MarkerRenderer->AddViewProp(this->beamFillActor);
    this->MarkerRenderer->AddViewProp(this->beamActor);
    this->MarkerRenderer->AddViewProp(this->beamLabel);
    }
  else
    {
    this->MarkerRenderer->RemoveViewProp(this->beamFillActor);
    this->MarkerRenderer->RemoveViewProp(this->beamActor);
    this->MarkerRenderer->RemoveViewProp(this->beamLabel);
    }
  this->ActorsAddedToRenderer = show;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::vtkInternal::AddRendererUpdateObserver(vtkRenderer* renderer)
{
  RemoveRendererUpdateObserver();
  if (renderer)
    {
    this->ObservedRenderer = renderer;
    this->RendererUpdateObservationId = renderer->AddObserver(vtkCommand::StartEvent, this->RendererUpdateObserver);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::vtkInternal::RemoveRendererUpdateObserver()
{
  if (this->ObservedRenderer)
    {
    this->ObservedRenderer->RemoveObserver(this->RendererUpdateObservationId);
    this->RendererUpdateObservationId = 0;
    this->ObservedRenderer = nullptr;
    }
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::vtkInternal::SetupMarkerRenderer()
{
  vtkRenderer* renderer = this->External->GetRenderer();
  if (renderer==nullptr)
    {
    vtkErrorWithObjectMacro(this->External, "vtkMRMLAstroBeamDisplayableManager"
                                            "::vtkInternal::SetupMarkerRenderer() failed: renderer is invalid");
    return;
    }

  this->MarkerRenderer->InteractiveOff();

  vtkRenderWindow* renderWindow = renderer->GetRenderWindow();
  if (renderWindow->GetNumberOfLayers() < RENDERER_LAYER+1)
    {
    renderWindow->SetNumberOfLayers( RENDERER_LAYER+1 );
    }
  this->MarkerRenderer->SetLayer(RENDERER_LAYER);
  renderWindow->AddRenderer(this->MarkerRenderer);

  // updated before each render of the view (UpdateFromRenderer)
  this->AddRendererUpdateObserver(renderer);
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::vtkInternal::SetupBeam()
{
  this->beamActor->PickableOff();
  this->beamActor->DragableOff();
  this->beamFillActor->PickableOff();
  this->beamFillActor->DragableOff();
  this->beamLabel->PickableOff();
  this->beamLabel->DragableOff();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::vtkInternal::UpdateBeam()
{
  // clean the attributes before updating
  this->beamPoints->Reset();
  this->beamCellArray->Reset();

  vtkMRMLAbstractViewNode* viewNode =
      vtkMRMLAbstractViewNode::SafeDownCast
      (this->External->GetMRMLDisplayableNode());
  if (!viewNode)
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroBeamDisplayableManager::UpdateBeam()"
                            " failed: view node is invalid.");
    this->ShowActors(false);
    return;
    }

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(viewNode);
  if (!sliceNode)
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroBeamDisplayableManager::UpdateBeam()"
                            " failed: displayable node is invalid.");
    this->ShowActors(false);
    return;
    }

  // The colour follows the ruler colour of the view (menu of the slice view
  // controller); a custom colour (SetAnnotationsColor) is used until the
  // ruler colour is changed again.
  const int rulerColor = viewNode->GetRulerColor();
  if (rulerColor != this->RulerColor)
    {
    if (this->RulerColor != -1 || !this->CustomColor)
      {
      switch (rulerColor)
        {
        case vtkMRMLAbstractViewNode::RulerColorWhite:
          this->Color->SetValue(0, 1.);
          this->Color->SetValue(1, 1.);
          this->Color->SetValue(2, 1.);
          break;
        case vtkMRMLAbstractViewNode::RulerColorBlack:
          this->Color->SetValue(0, 0.);
          this->Color->SetValue(1, 0.);
          this->Color->SetValue(2, 0.);
          break;
        default: // yellow: the SlicerAstro annotations colour
          this->Color->SetValue(0, COLOR_INVALID[0]);
          this->Color->SetValue(1, COLOR_INVALID[1]);
          this->Color->SetValue(2, COLOR_INVALID[2]);
          break;
        }
      this->CustomColor = false;
      }
    this->RulerColor = rulerColor;
    }

  if (!sliceNode->GetAttribute("SlicerAstro.Beam"))
    {
    return;
    }

  if (sliceNode->GetOrientation().compare("XY") ||
      !strcmp(sliceNode->GetAttribute("SlicerAstro.Beam"), "off"))
    {
    this->ShowActors(false);
    return;
    }

  // get the Logics
  this->app = qSlicerApplication::application();

  vtkMRMLSliceLogic* sliceLogic =
    this->app->applicationLogic()->GetSliceLogic(sliceNode);

  if (!sliceLogic)
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
                            " failed: sliceLogic node is invalid.");
    this->ShowActors(false);
    return;
    }

  // the volume whose beam is shown: the same rule as the beam button of the
  // slice controller and of the AstroWorkbench view bars
  vtkMRMLAstroVolumeNode* volumeNode =
    vtkMRMLAstroVolumeNode::GetBeamVolume(sliceLogic->GetSliceCompositeNode());
  vtkMRMLSliceLayerLogic* layerLogic = nullptr;
  vtkMRMLSliceLayerLogic* layers[2] = {sliceLogic->GetBackgroundLayer(), sliceLogic->GetForegroundLayer()};
  for (vtkMRMLSliceLayerLogic* layer : layers)
    {
    if (volumeNode && layer && layer->GetVolumeNode() == volumeNode)
      {
      layerLogic = layer;
      break;
      }
    }
  if (!layerLogic || !this->ComputeBeamPoints(sliceNode, volumeNode, layerLogic))
    {
    this->ShowActors(false);
    return;
    }

  // outline and translucent fill of the ellipse
  this->beamPoints->Modified();
  this->beamCellArray->Modified();
  this->beamPolyData->SetPoints(this->beamPoints);
  this->beamPolyData->SetLines(this->beamCellArray);
  this->beamPolyData->Modified();
  this->beamMapper->SetInputData(this->beamPolyData);
  this->beamActor->SetMapper(this->beamMapper);
  this->beamActor->GetProperty()->SetLineWidth(2);
  this->beamActor->GetProperty()->SetColor(this->Color->GetValue(0),
                                           this->Color->GetValue(1),
                                           this->Color->GetValue(2));
  this->beamFillPolyData->SetPoints(this->beamPoints);
  this->beamFillPolyData->SetPolys(this->beamFillCellArray);
  this->beamFillPolyData->Modified();
  this->beamFillMapper->SetInputData(this->beamFillPolyData);
  this->beamFillActor->SetMapper(this->beamFillMapper);
  this->beamFillActor->GetProperty()->SetColor(this->Color->GetValue(0),
                                               this->Color->GetValue(1),
                                               this->Color->GetValue(2));
  this->beamFillActor->GetProperty()->SetOpacity(0.3);
  // the label right of the ellipse, at its height
  double bounds[6] = {0., 0., 0., 0., 0., 0.};
  this->beamPoints->GetBounds(bounds);
  this->beamLabel->SetDisplayPosition(static_cast<int>(bounds[1] + 6.), static_cast<int>((bounds[2] + bounds[3]) / 2.));
  this->beamLabel->GetTextProperty()->SetColor(this->Color->GetValue(0),
                                               this->Color->GetValue(1),
                                               this->Color->GetValue(2));

  this->ShowActors(true);
}

//---------------------------------------------------------------------------
bool vtkMRMLAstroBeamDisplayableManager::vtkInternal::ComputeBeamPoints(vtkMRMLSliceNode* sliceNode,
  vtkMRMLAstroVolumeNode* volumeNode, vtkMRMLSliceLayerLogic* layerLogic)
{
  // The beam ellipse on the sky (vtkMRMLAstroVolumeNode::GetBeamEllipse),
  // placed in view coordinates in the bottom-left corner of the visible part
  // of the data (radio astronomy convention), inside the labels of the 2D
  // axes: visible whatever the zoom, the pan and the orientation of the axes
  // on the screen; its size is the size on the sky.
  // (the transforms of the slice layer are up to date: updating them here
  // modified the slice logic, which rendered the view again)
  vtkGeneralTransform* xyToIJK = layerLogic->GetXYToIJKTransform();
  vtkImageData* imageData = volumeNode->GetImageData();
  if (!xyToIJK || !imageData)
    {
    return false;
    }
  const int* viewDimensions = sliceNode->GetDimensions();
  int dimensions[3] = {0, 0, 0};
  imageData->GetDimensions(dimensions);
  if (viewDimensions[0] <= 0 || viewDimensions[1] <= 0 || dimensions[0] <= 0 || dimensions[1] <= 0)
    {
    return false;
    }
  vtkAbstractTransform* ijkToXY = xyToIJK->GetInverse();

  // the channel shown
  double viewCenter[3] = {viewDimensions[0] / 2., viewDimensions[1] / 2., 0.};
  double centerIJK[3] = {0., 0., 0.};
  xyToIJK->TransformPoint(viewCenter, centerIJK);
  const double channel = centerIJK[2];

  // visible part of the data in the view (XY, origin at the bottom left)
  double visible[4] = {VTK_DOUBLE_MAX, -VTK_DOUBLE_MAX, VTK_DOUBLE_MAX, -VTK_DOUBLE_MAX};
  for (int corner = 0; corner < 4; corner++)
    {
    double ijk[3] = {corner % 2 ? dimensions[0] - 0.5 : -0.5, corner / 2 ? dimensions[1] - 0.5 : -0.5, channel};
    double xy[3] = {0., 0., 0.};
    ijkToXY->TransformPoint(ijk, xy);
    visible[0] = std::min(visible[0], xy[0]);
    visible[1] = std::max(visible[1], xy[0]);
    visible[2] = std::min(visible[2], xy[1]);
    visible[3] = std::max(visible[3], xy[1]);
    }
  // inside the view and, with the 2D axes, inside their labels (left and
  // bottom sides, vtkMRMLAstroTwoDAxesDisplayableManager)
  const bool axes = sliceNode->GetRulerType() != vtkMRMLAbstractViewNode::RulerTypeNone;
  visible[0] = std::max(visible[0], axes ? AXES_LEFT_MARGIN : 0.);
  visible[1] = std::min(visible[1], static_cast<double>(viewDimensions[0]));
  visible[2] = std::max(visible[2], axes ? AXES_BOTTOM_MARGIN : 0.);
  visible[3] = std::min(visible[3], static_cast<double>(viewDimensions[1]));

  // the ellipse around a point of the view: its points in view coordinates
  const int numberOfPoints = 61;
  std::vector<double> points;
  vtkNew<vtkPoints> ijkPoints;
  std::function<bool(const double*)> ellipseAt = [&](const double* centerXY) -> bool
    {
    points.clear();
    double xy[3] = {centerXY[0], centerXY[1], 0.}, ijk[3] = {0., 0., 0.};
    xyToIJK->TransformPoint(xy, ijk);
    ijk[2] = channel;
    if (!volumeNode->GetBeamEllipse(ijk, numberOfPoints, ijkPoints))
      {
      return false;
      }
    for (vtkIdType point = 0; point < ijkPoints->GetNumberOfPoints(); point++)
      {
      double pointXY[3] = {0., 0., 0.};
      ijkToXY->TransformPoint(ijkPoints->GetPoint(point), pointXY);
      points.push_back(pointXY[0]);
      points.push_back(pointXY[1]);
      }
    return true;
    };

  // size of the ellipse on the screen (at the centre of the visible data)
  const double reference[2] = {(visible[0] + visible[1]) / 2., (visible[2] + visible[3]) / 2.};
  if (visible[1] <= visible[0] || visible[3] <= visible[2] || !ellipseAt(reference))
    {
    return false;
    }
  double halfWidth = 0., halfHeight = 0.;
  for (int point = 0; point < numberOfPoints; point++)
    {
    halfWidth = std::max(halfWidth, fabs(points[2 * point] - reference[0]));
    halfHeight = std::max(halfHeight, fabs(points[2 * point + 1] - reference[1]));
    }
  // bottom-left corner, hidden if it does not fit in the visible data
  const double anchor[2] = {visible[0] + CORNER_PADDING + halfWidth, visible[2] + CORNER_PADDING + halfHeight};
  if (anchor[0] + halfWidth + CORNER_PADDING > visible[1] ||
      anchor[1] + halfHeight + CORNER_PADDING > visible[3] ||
      !ellipseAt(anchor))
    {
    return false;
    }

  this->beamPoints->Reset();
  this->beamCellArray->Reset();
  this->beamFillCellArray->Reset();
  this->beamFillCellArray->InsertNextCell(numberOfPoints - 1);
  for (int point = 0; point < numberOfPoints; point++)
    {
    const vtkIdType id = this->beamPoints->InsertNextPoint(points[2 * point], points[2 * point + 1], 0.);
    if (point > 0)
      {
      vtkIdType ids[2] = {id - 1, id};
      this->beamCellArray->InsertNextCell(2, ids);
      }
    // the last point is the first one again
    if (point < numberOfPoints - 1)
      {
      this->beamFillCellArray->InsertCellPoint(id);
      }
    }
  return true;
}

//---------------------------------------------------------------------------
// vtkMRMLRulerDisplayableManager methods

//---------------------------------------------------------------------------
vtkMRMLAstroBeamDisplayableManager::vtkMRMLAstroBeamDisplayableManager()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkMRMLAstroBeamDisplayableManager::~vtkMRMLAstroBeamDisplayableManager()
{
  delete this->Internal;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
vtkRenderer *vtkMRMLAstroBeamDisplayableManager::vtkMarkerRenderer()
{
  return this->Internal->MarkerRenderer;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::SetAnnotationsColor(double red, double green, double blue)
{
  this->Internal->CustomColor = true;
  this->Internal->Color->SetValue(0, red);
  this->Internal->Color->SetValue(1, green);
  this->Internal->Color->SetValue(2, blue);
  this->Internal->UpdateBeam();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::Create()
{
  this->Internal->SetupMarkerRenderer();
  this->Internal->SetupBeam();
  this->Superclass::Create();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::UpdateFromViewNode()
{
  // The view node is changed (e.g. on every mouse move while panning): the
  // view is rendered again and the beam is updated before the render
  // (UpdateFromRenderer), once per render
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::OnMRMLDisplayableNodeModifiedEvent(vtkObject* vtkNotUsed(caller))
{
  // view node is changed
  this->UpdateFromViewNode();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::UpdateFromRenderer()
{
  // Rendering is performed, so let's re-render the marker with up-to-date orientation
  this->Internal->UpdateBeam();
}
