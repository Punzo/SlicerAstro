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
#include <vtkMath.h>

// STD includes
#include <sstream>
#include <vector>

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
  this->col = vtkSmartPointer<vtkCollection>::New();
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

  // the collection may still hold the layers of a previous update that
  // returned early
  this->col->RemoveAllItems();
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

    // e.g. BMAJ = -1: the beam is not defined
    if (StringToDouble(volumeNode->GetAttribute("SlicerAstro.BMAJ")) <= 0. ||
        StringToDouble(volumeNode->GetAttribute("SlicerAstro.BMIN")) <= 0.)
      {
      continue;
      }

    hasVolume = true;

    // The beam ellipse, drawn on the sky: full axes BMAJ and BMIN (FWHM),
    // major axis at the position angle BPA (from north through east), each
    // point projected with the WCS of the volume (rotated axes, either
    // direction of the right ascension and non-square pixels included).
    // (the transforms of the slice layer are up to date: updating them here
    // modified the slice logic, which rendered the view again)
    vtkMRMLAstroVolumeDisplayNode* displayNode = volumeNode->GetAstroVolumeDisplayNode();
    struct wcsprm* wcs = displayNode ? displayNode->GetWCSStruct() : nullptr;
    vtkGeneralTransform* xyToIJK = sliceLayerLogic->GetXYToIJKTransform();
    if (!wcs || wcs->lng != 0 || wcs->lat != 1 || wcs->naxis > 4 || !xyToIJK)
      {
      continue;
      }

    const double BMAJ = StringToDouble(volumeNode->GetAttribute("SlicerAstro.BMAJ"));
    const double BMIN = StringToDouble(volumeNode->GetAttribute("SlicerAstro.BMIN"));
    const double BPA = vtkMath::RadiansFromDegrees(StringToDouble(volumeNode->GetAttribute("SlicerAstro.BPA")));
    const double CDELT2 = fabs(StringToDouble(volumeNode->GetAttribute("SlicerAstro.CDELT2")));
    const double NAXIS1 = StringToDouble(volumeNode->GetAttribute("SlicerAstro.NAXIS1"));
    if (!(CDELT2 > 0.))
      {
      continue;
      }

    // center: in the bottom right corner of the data (two major axes from
    // the sides), in the channel shown
    const double majorPixels = 2. * BMAJ / CDELT2;
    double viewCenter[3] = {sliceNode->GetDimensions()[0] / 2., sliceNode->GetDimensions()[1] / 2., 0.};
    double channel[3] = {0., 0., 0.};
    xyToIJK->TransformPoint(viewCenter, channel);
    // FITS pixel coordinates (from 1)
    double centerPixel[4] = {NAXIS1 - majorPixels, majorPixels + 1., channel[2] + 1., 1.};
    double centerWorld[4] = {0., 0., 0., 0.}, imgcrd[4] = {0., 0., 0., 0.}, phi = 0., theta = 0.;
    int centerStatus = 0;
    if (wcsp2s(wcs, 1, 4, centerPixel, imgcrd, &phi, &theta, centerWorld, &centerStatus))
      {
      continue;
      }
    const double cosDeclination = cos(vtkMath::RadiansFromDegrees(centerWorld[1]));
    if (!(cosDeclination > 1.E-6))
      {
      continue;
      }

    const int numberOfPoints = 61;
    std::vector<double> world(numberOfPoints * 4), pixel(numberOfPoints * 4, 0.), imgcrds(numberOfPoints * 4, 0.);
    std::vector<double> phis(numberOfPoints, 0.), thetas(numberOfPoints, 0.);
    std::vector<int> stat(numberOfPoints, 0);
    for (int point = 0; point < numberOfPoints; point++)
      {
      const double t = point * (2. * vtkMath::Pi() / (numberOfPoints - 1));
      const double alongMajor = 0.5 * BMAJ * cos(t);
      const double alongMinor = 0.5 * BMIN * sin(t);
      // offsets (degrees) towards the east and the north
      const double east = alongMajor * sin(BPA) + alongMinor * cos(BPA);
      const double north = alongMajor * cos(BPA) - alongMinor * sin(BPA);
      std::copy(centerWorld, centerWorld + 4, world.begin() + point * 4);
      world[point * 4] = centerWorld[0] + east / cosDeclination;
      world[point * 4 + 1] = centerWorld[1] + north;
      }
    const int status = wcss2p(wcs, numberOfPoints, 4, world.data(), phis.data(), thetas.data(),
                              imgcrds.data(), pixel.data(), stat.data());
    if (status && status != WCSERR_BAD_WORLD)
      {
      continue;
      }

    vtkAbstractTransform* ijkToXY = xyToIJK->GetInverse();
    vtkIdType previousId = -1;
    for (int point = 0; point < numberOfPoints; point++)
      {
      if (stat[point])
        {
        previousId = -1;
        continue;
        }
      // FITS pixel coordinates (from 1) to IJK (voxel centers from 0)
      double ijk[3] = {pixel[point * 4] - 1., pixel[point * 4 + 1] - 1., channel[2]}, xy[3] = {0., 0., 0.};
      ijkToXY->TransformPoint(ijk, xy);
      const vtkIdType id = this->beamPoints->InsertNextPoint(xy[0], xy[1], 0.);
      if (previousId >= 0)
        {
        vtkIdType ids[2] = {previousId, id};
        this->beamCellArray->InsertNextCell(2, ids);
        }
      previousId = id;
      }

    // setup the mapper and actor
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
