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
#include <vtkMRMLLogic.h>
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
#include <vtkTransform.h>

// STD includes
#include <sstream>

// SlicerQt includes
#include <qSlicerApplication.h>

// vtkSlicer includes
#include <vtkSlicerApplicationLogic.h>

// Constants
static const int RENDERER_LAYER = 1; // layer ID where the orientation marker will be displayed
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
    this->DisplayableManager = 0;
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

  vtkSmartPointer<vtkRenderer> MarkerRenderer;
  vtkSmartPointer<vtkAstroBeamRendererUpdateObserver> RendererUpdateObserver;
  vtkSmartPointer<vtkPoints> beamPoints;
  vtkSmartPointer<vtkCellArray> beamCellArray;
  vtkSmartPointer<vtkPolyData> beamPolyData;
  vtkSmartPointer<vtkActor2D> beamActor;
  vtkSmartPointer<vtkPolyDataMapper2D> beamMapper;
  vtkSmartPointer<vtkCollection> col;

  int RendererUpdateObservationId;
  vtkWeakPointer<vtkRenderer> ObservedRenderer;

  bool ActorsAddedToRenderer;
  vtkMRMLAstroBeamDisplayableManager* External;

  qSlicerApplication* app;
};

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
  this->col = vtkSmartPointer<vtkCollection>::New();
  this->app = 0;
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
    this->MarkerRenderer->AddViewProp(this->beamActor);
    }
  else
    {
    this->MarkerRenderer->RemoveViewProp(this->beamActor);
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
    this->ObservedRenderer = NULL;
    }
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::vtkInternal::SetupMarkerRenderer()
{
  vtkRenderer* renderer = this->External->GetRenderer();
  if (renderer==NULL)
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

}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::vtkInternal::SetupBeam()
{
  this->beamActor->PickableOff();
  this->beamActor->DragableOff();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::vtkInternal::UpdateBeam()
{
  // clean the attributes before updating
  this->beamPoints->Initialize();
  this->beamPoints->Squeeze();
  this->beamCellArray->Initialize();
  this->beamCellArray->Squeeze();

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

  this->col->AddItem(sliceLogic->GetBackgroundLayer());
  this->col->AddItem(sliceLogic->GetForegroundLayer());
  this->col->AddItem(sliceLogic->GetLabelLayer());

  bool hasVolume = false;

  for (int layer = 0; layer < this->col->GetNumberOfItems(); layer++)
    {
    vtkMRMLSliceLayerLogic* sliceLayerLogic =
      vtkMRMLSliceLayerLogic::SafeDownCast
        (this->col->GetItemAsObject(layer));

    if(!sliceLayerLogic)
      {
      return;
      }

    vtkMRMLAstroVolumeNode* volumeNode =
      vtkMRMLAstroVolumeNode::SafeDownCast
        (sliceLayerLogic->GetVolumeNode());

    if (!volumeNode)
      {
      continue;
      }

    if (!strcmp(volumeNode->GetAttribute("SlicerAstro.BMAJ"), "UNDEFINED") ||
        !strcmp(volumeNode->GetAttribute("SlicerAstro.BMIN"), "UNDEFINED") ||
        !strcmp(volumeNode->GetAttribute("SlicerAstro.BPA"), "UNDEFINED"))
      {
      continue;
      }

    hasVolume = true;

    sliceNode->UpdateMatrices();
    sliceLayerLogic->UpdateTransforms();

    vtkNew<vtkTransform> transform;
    double BPA = StringToDouble(volumeNode->GetAttribute("SlicerAstro.BPA"));
    transform->RotateZ(BPA);
    const double degtorad = atan(1.) / 45.;
    vtkGeneralTransform* ijkToXY =
        sliceLayerLogic->GetXYToIJKTransform();
    ijkToXY->Inverse();

    double BMAJ = StringToDouble(volumeNode->GetAttribute("SlicerAstro.BMAJ"));
    double BMIN = StringToDouble(volumeNode->GetAttribute("SlicerAstro.BMIN"));
    double CDELT1 = StringToDouble(volumeNode->GetAttribute("SlicerAstro.CDELT1"));
    double CDELT2 = StringToDouble(volumeNode->GetAttribute("SlicerAstro.CDELT2"));
    double NAXIS1 = StringToDouble(volumeNode->GetAttribute("SlicerAstro.NAXIS1"));

    double aCosPixel = BMAJ * cos(BPA * degtorad) / CDELT1;
    double aSinPixel = BMAJ * sin(BPA * degtorad) / CDELT2;
    double a = sqrt(aCosPixel * aCosPixel + aSinPixel * aSinPixel);

    double bCosPixel = BMIN * cos((BPA + 90) * degtorad) / CDELT1;
    double bSinPixel = BMIN * sin((BPA + 90) * degtorad) / CDELT2;
    double b = sqrt(bCosPixel * bCosPixel + bSinPixel * bSinPixel);

    double centerX = NAXIS1 - a * 3;
    double centerY = b * 3;

    double ijk[3], xy[3];
    // add points
    for (int ii = 0; ii <= 60; ii++)
      {
      double rot = ii * (2 * PI / 60.);
      ijk[0] = a * cos(rot);
      ijk[1] = b * sin(rot);
      ijk[2] = 0.;

      transform->TransformPoint(ijk, ijk);

      ijk[0] += centerX;
      ijk[1] += centerY;

      ijkToXY->TransformPoint(ijk, xy);

      this->beamPoints->InsertPoint(ii, xy[0], xy[1], 0);
      }

    int n = this->beamPoints->GetNumberOfPoints();
    // unify the points with lines
    std::vector<vtkSmartPointer<vtkLine> > lines;
    for (int ii = 0; ii < n - 1; ii++)
      {
      vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
      lines.push_back(line);
      }

    for (int ii = 0; ii < n - 1; ii++)
      {
      lines[ii]->GetPointIds()->SetId(0, ii);
      lines[ii]->GetPointIds()->SetId(1, ii + 1);
      }

    // create the cellArray
    for (int ii = 0; ii < n - 1; ii++)
      {
      this->beamCellArray->InsertNextCell(lines[ii]);
      }

    // setup the mapper and actor
    this->beamPolyData->SetPoints(this->beamPoints);
    this->beamPolyData->SetLines(this->beamCellArray);
    this->beamMapper->SetInputData(this->beamPolyData);
    this->beamActor->SetMapper(this->beamMapper);
    this->beamActor->GetProperty()->SetLineWidth(1.5);
    this->MarkerRenderer->AddActor2D(this->beamActor);

    this->ShowActors(true);
    break;
    }

  if (!hasVolume)
    {
    this->ShowActors(false);
    }
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
void vtkMRMLAstroBeamDisplayableManager::Create()
{
  this->Internal->SetupMarkerRenderer();
  this->Internal->SetupBeam();
  this->Superclass::Create();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeamDisplayableManager::UpdateFromViewNode()
{
  // View node is changed, which may mean that either the marker type (visibility), size, or orientation is changed
  this->Internal->UpdateBeam();
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
