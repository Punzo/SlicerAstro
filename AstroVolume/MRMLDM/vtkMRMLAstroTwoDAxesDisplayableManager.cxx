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
#include "vtkMRMLAstroTwoDAxesDisplayableManager.h"

// Qt includes
#include <QColor>

// MRML includes
#include <vtkMRMLAbstractViewNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLViewNode.h>

// VTK includes
#include <vtkRendererCollection.h>
#include <vtkPropCollection.h>
#include <vtkActor2D.h>
#include <vtkActor2DCollection.h>
#include <vtkAxisActor2D.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkGeneralTransform.h>
#include <vtkAbstractTransform.h>
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

// STD includes
#include <sstream>
#include <cstring>
#include <algorithm>

// SlicerQt includes
#include <qSlicerApplication.h>

// vtkSlicer includes
#include <vtkSlicerApplicationLogic.h>

// Constants
static const int RENDERER_LAYER = 1; // layer ID where the orientation marker will be displayed
const double PI_2  = PI * 0.5;

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
class vtkAstroTwoDAxesRendererUpdateObserver : public vtkCommand
{
public:
  static vtkAstroTwoDAxesRendererUpdateObserver *New()
    {
    return new vtkAstroTwoDAxesRendererUpdateObserver;
    }
  vtkAstroTwoDAxesRendererUpdateObserver()
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
  vtkWeakPointer<vtkMRMLAstroTwoDAxesDisplayableManager> DisplayableManager;
};

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLAstroTwoDAxesDisplayableManager);

//---------------------------------------------------------------------------
class vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal
{
public:
  vtkInternal(vtkMRMLAstroTwoDAxesDisplayableManager * external);
  ~vtkInternal();

  void SetupMarkerRenderer();
  void AddRendererUpdateObserver(vtkRenderer* renderer);
  void RemoveRendererUpdateObserver();

  void SetupAxes();
  void UpdateAxes();
  /// Axes of a PV diagram (DATAMODEL PVDIAGRAM): offset along the line
  /// (horizontal) and velocity (vertical). Returns false if not drawn.
  bool UpdatePVDiagramAxes(vtkMRMLSliceLayerLogic* sliceLayerLogic,
                           vtkMRMLAstroVolumeDisplayNode* displayNode,
                           vtkMRMLAstroVolumeNode* pvDiagramVolume,
                           int viewWidthPixel, int viewHeightPixel,
                           int majorTickMin, int majorTickMax,
                           int minorTickMin, int minorTickMax,
                           int rulerType, int fontSize);
  void AddAxisLabel(const std::string& text, int x, int y, int fontSize);
  void HideSlicerRuler();
  void ShowActors(bool show);

  vtkSmartPointer<vtkRenderer> MarkerRenderer;
  vtkSmartPointer<vtkAstroTwoDAxesRendererUpdateObserver> RendererUpdateObserver;
  vtkSmartPointer<vtkPoints> twoDAxesPoints;
  vtkSmartPointer<vtkCellArray> twoDAxesCellArray;
  vtkSmartPointer<vtkPolyData> twoDAxesPolyData;
  vtkSmartPointer<vtkActor2D> twoDAxesActor;
  vtkSmartPointer<vtkPolyDataMapper2D> twoDAxesMapper;
  vtkSmartPointer<vtkCollection> col;

  int RendererUpdateObservationId;
  vtkWeakPointer<vtkRenderer> ObservedRenderer;

  bool ActorsAddedToRenderer;

  vtkSmartPointer<vtkDoubleArray> Color;
  int RulerColor{-1};     // ruler colour of the view node used for Color
  bool CustomColor{false}; // Color set with SetAnnotationsColor
  static const double COLOR_INVALID[3];

  int fontSize;
  std::string fontStyle;

  vtkMRMLAstroTwoDAxesDisplayableManager* External;

  qSlicerApplication* app;
};

//----------------------------------------------------------------------------
const double vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::COLOR_INVALID[3] = {1., 0.731, 0.078};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::vtkInternal(vtkMRMLAstroTwoDAxesDisplayableManager * external)
{
  this->External = external;
  this->RendererUpdateObserver = vtkSmartPointer<vtkAstroTwoDAxesRendererUpdateObserver>::New();
  this->RendererUpdateObserver->DisplayableManager = this->External;
  this->RendererUpdateObservationId = 0;
  this->ActorsAddedToRenderer = false;
  this->MarkerRenderer = vtkSmartPointer<vtkRenderer>::New();
  this->twoDAxesPoints = vtkSmartPointer<vtkPoints>::New();
  this->twoDAxesCellArray = vtkSmartPointer<vtkCellArray>::New();
  this->twoDAxesPolyData = vtkSmartPointer<vtkPolyData>::New();
  this->twoDAxesActor = vtkSmartPointer<vtkActor2D>::New();
  this->twoDAxesMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  this->col = vtkSmartPointer<vtkCollection>::New();
  this->app = nullptr;
  this->Color = vtkSmartPointer<vtkDoubleArray>::New();
  this->Color->SetNumberOfValues(3);
  this->Color->SetValue(0, COLOR_INVALID[0]);
  this->Color->SetValue(1, COLOR_INVALID[1]);
  this->Color->SetValue(2, COLOR_INVALID[2]);
  this->fontSize = 12;
  this->fontStyle = "Arial";
}

//---------------------------------------------------------------------------
vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::~vtkInternal()
{
  RemoveRendererUpdateObserver();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::ShowActors(bool show)
{
  if (this->ActorsAddedToRenderer == show)
    {
    // no change
    return;
    }
  if (show)
    {
    this->MarkerRenderer->AddViewProp(this->twoDAxesActor);
    }
  else
    {
    this->MarkerRenderer->RemoveViewProp(this->twoDAxesActor);
    }
  this->ActorsAddedToRenderer = show;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::AddRendererUpdateObserver(vtkRenderer* renderer)
{
  RemoveRendererUpdateObserver();
  if (renderer)
    {
    this->ObservedRenderer = renderer;
    this->RendererUpdateObservationId = renderer->AddObserver(vtkCommand::StartEvent, this->RendererUpdateObserver);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::RemoveRendererUpdateObserver()
{
  if (this->ObservedRenderer)
    {
    this->ObservedRenderer->RemoveObserver(this->RendererUpdateObservationId);
    this->RendererUpdateObservationId = 0;
    this->ObservedRenderer = nullptr;
    }
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::SetupMarkerRenderer()
{
  vtkRenderer* renderer = this->External->GetRenderer();
  if (renderer==nullptr)
    {
    vtkErrorWithObjectMacro(this->External, "vtkMRMLAstroTwoDAxesDisplayableManager"
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
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::SetupAxes()
{
  this->twoDAxesActor->PickableOff();
  this->twoDAxesActor->DragableOff();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::HideSlicerRuler()
{
  // The WCS axes replace the ruler of Slicer in the slice views (they are
  // controlled by the same ruler settings of the view node). SlicerAstro
  // unregisters vtkMRMLRulerDisplayableManager, but every new slice view
  // registers it again and its instances leave their marker renderer (with
  // the ruler actors) in the render window: do not draw these renderers.
  // vtkMRMLRulerDisplayableManager is the only one using a vtkAxisActor2D.
  vtkRenderer* renderer = this->External->GetRenderer();
  vtkRenderWindow* renderWindow = renderer ? renderer->GetRenderWindow() : nullptr;
  if (!renderWindow)
    {
    return;
    }
  vtkRendererCollection* renderers = renderWindow->GetRenderers();
  vtkCollectionSimpleIterator rendererIt;
  renderers->InitTraversal(rendererIt);
  while (vtkRenderer* otherRenderer = renderers->GetNextRenderer(rendererIt))
    {
    if (otherRenderer == this->MarkerRenderer || !otherRenderer->GetDraw())
      {
      continue;
      }
    vtkPropCollection* props = otherRenderer->GetViewProps();
    vtkCollectionSimpleIterator propIt;
    props->InitTraversal(propIt);
    while (vtkProp* prop = props->GetNextProp(propIt))
      {
      if (prop->IsA("vtkAxisActor2D"))
        {
        otherRenderer->DrawOff();
        break;
        }
      }
    }
}

//---------------------------------------------------------------------------
namespace
{
/// Spectral axis of a PV diagram (axis 2 of its header: the channels of the
/// cube): rows (IJK, from 0) <-> velocity (WCS units, e.g. m/s)
class PVDiagramSpectralAxis
{
public:
  explicit PVDiagramSpectralAxis(vtkMRMLAstroVolumeNode* volume)
    {
    this->WCS.flag = -1;
    if (!volume || wcsini(1, 1, &this->WCS))
      {
      return;
      }
    this->Initialized = true;
    const char* ctype = volume->GetAttribute("SlicerAstro.CTYPE2");
    const char* cunit = volume->GetAttribute("SlicerAstro.CUNIT2");
    const char* crval = volume->GetAttribute("SlicerAstro.CRVAL2");
    const char* cdelt = volume->GetAttribute("SlicerAstro.CDELT2");
    const char* crpix = volume->GetAttribute("SlicerAstro.CRPIX2");
    const char* restfreq = volume->GetAttribute("SlicerAstro.RESTFREQ");
    if (!ctype || !crval || !cdelt || !crpix)
      {
      return;
      }
    strncpy(this->WCS.ctype[0], ctype, 71);
    if (cunit)
      {
      strncpy(this->WCS.cunit[0], cunit, 71);
      }
    this->WCS.crval[0] = StringToDouble(crval);
    this->WCS.cdelt[0] = StringToDouble(cdelt);
    this->WCS.crpix[0] = StringToDouble(crpix);
    this->WCS.restfrq = restfreq ? StringToDouble(restfreq) : 0.;
    // non-standard units and AIPS spectral types (e.g. FELO-HEL)
    int fixStatus[NWCSFIX];
    wcsfix(7, nullptr, &this->WCS, fixStatus);
    this->Valid = (wcsset(&this->WCS) == 0) && this->WCS.spec == 0;
    }
  ~PVDiagramSpectralAxis()
    {
    if (this->Initialized)
      {
      wcsfree(&this->WCS);
      }
    }
  bool IsValid() const
    {
    return this->Valid;
    }
  bool RowToVelocity(double row, double& velocity)
    {
    double pixel[1] = {row + 1.}, imgcrd[1] = {0.}, phi[1] = {0.}, theta[1] = {0.}, world[1] = {0.};
    int stat[1] = {0};
    if (!this->Valid || wcsp2s(&this->WCS, 1, 1, pixel, imgcrd, phi, theta, world, stat))
      {
      return false;
      }
    velocity = world[0];
    return true;
    }
  bool VelocityToRow(double velocity, double& row)
    {
    double world[1] = {velocity}, imgcrd[1] = {0.}, phi[1] = {0.}, theta[1] = {0.}, pixel[1] = {0.};
    int stat[1] = {0};
    if (!this->Valid || wcss2p(&this->WCS, 1, 1, world, phi, theta, imgcrd, pixel, stat))
      {
      return false;
      }
    row = pixel[0] - 1.;
    return true;
    }
private:
  struct wcsprm WCS;
  bool Initialized{false};
  bool Valid{false};
};
} // end of anonymous namespace

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::AddAxisLabel(const std::string& text,
                                                                       int x, int y, int fontSize)
{
  vtkSmartPointer<vtkTextActor> textActor = vtkSmartPointer<vtkTextActor>::New();
  vtkTextProperty* textProperty = textActor->GetTextProperty();
  textProperty->SetFontSize(fontSize);
  if (!this->fontStyle.compare("Courier"))
    {
    textProperty->SetFontFamilyToCourier();
    }
  else if (!this->fontStyle.compare("Times"))
    {
    textProperty->SetFontFamilyToTimes();
    }
  else
    {
    textProperty->SetFontFamilyToArial();
    }
  textActor->GetProperty()->SetColor(this->Color->GetValue(0),
                                     this->Color->GetValue(1),
                                     this->Color->GetValue(2));
  textActor->SetInput(text.c_str());
  textActor->SetDisplayPosition(x, y);
  this->MarkerRenderer->AddViewProp(textActor);
}

//---------------------------------------------------------------------------
bool vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::UpdatePVDiagramAxes(
  vtkMRMLSliceLayerLogic* sliceLayerLogic, vtkMRMLAstroVolumeDisplayNode* displayNode,
  vtkMRMLAstroVolumeNode* pvDiagramVolume, int viewWidthPixel, int viewHeightPixel,
  int majorTickMin, int majorTickMax, int minorTickMin, int minorTickMax,
  int rulerType, int fontSize)
{
  if (!sliceLayerLogic || !displayNode || !pvDiagramVolume)
    {
    return false;
    }

  // Horizontal: offset along the line (degrees), CDELT1 per pixel
  const char* cdelt1Attribute = pvDiagramVolume->GetAttribute("SlicerAstro.CDELT1");
  const char* crpix1Attribute = pvDiagramVolume->GetAttribute("SlicerAstro.CRPIX1");
  const double cdelt1 = cdelt1Attribute ? StringToDouble(cdelt1Attribute) : 0.;
  const double crpix1 = crpix1Attribute ? StringToDouble(crpix1Attribute) : 1.;
  if (cdelt1 <= 0.)
    {
    return false;
    }

  // Vertical: velocity of the rows (the channels of the cube), from the
  // spectral axis of the PV diagram header (axis 2), with WCSLIB (exact also
  // for non-linear axes, e.g. VOPT-F2W)
  PVDiagramSpectralAxis spectralAxis(pvDiagramVolume);
  if (!spectralAxis.IsValid())
    {
    return false;
    }

  vtkGeneralTransform* xyToIJK = sliceLayerLogic->GetXYToIJKTransform();
  vtkAbstractTransform* ijkToXY = xyToIJK->GetInverse();

  double xyz[3] = {0., 0., 0.}, ijkA[3] = {0., 0., 0.}, ijkB[3] = {0., 0., 0.}, ijkC[3] = {0., 0., 0.};
  xyToIJK->TransformPoint(xyz, ijkA);
  xyz[0] = viewWidthPixel;
  xyToIJK->TransformPoint(xyz, ijkB);
  xyz[0] = 0.;
  xyz[1] = viewHeightPixel;
  xyToIJK->TransformPoint(xyz, ijkC);

  // offsets at the left and right sides of the view
  const double offsetA = (ijkA[0] + 1. - crpix1) * cdelt1;
  const double offsetB = (ijkB[0] + 1. - crpix1) * cdelt1;
  // velocities at the bottom and top sides of the view
  double velocityA = 0., velocityC = 0.;
  if (!spectralAxis.RowToVelocity(ijkA[1], velocityA) || !spectralAxis.RowToVelocity(ijkC[1], velocityC))
    {
    return false;
    }

  int numberOfPointsHorizontal = std::max(5, static_cast<int>(viewWidthPixel / 150.) + 1);
  int numberOfPointsVertical = std::max(5, static_cast<int>(viewHeightPixel / 150.) + 1);
  const double offsetStep = displayNode->GetWcsTickStepAxisY(fabs(offsetB - offsetA), &numberOfPointsHorizontal);
  const double firstOffset = displayNode->GetFirstWcsTickAxisY(offsetA, offsetB, offsetStep);
  const double velocityStep = displayNode->GetWcsTickStepAxisZ(fabs(velocityC - velocityA), &numberOfPointsVertical);
  const double firstVelocity = displayNode->GetFirstWcsTickAxisZ(velocityA, velocityC, velocityStep);
  if (!(offsetStep > 0.) || !(velocityStep > 0.))
    {
    return false;
    }

  const double rowA = ijkA[1];
  const double columnA = ijkA[0];

  // major tick and three minor ticks per step
  double oldOutputValues[3] = {0., 0., 0.}, outputValues[3] = {0., 0., 0.};
  for (int i = 0; i < numberOfPointsHorizontal; i++)
    {
    const double offset = firstOffset + offsetStep * i;
    double labelX = 0.;
    for (int quarter = 0; quarter < 4; quarter++)
      {
      double ijk[3] = {(offset + offsetStep * quarter / 4.) / cdelt1 + crpix1 - 1., rowA, 0.};
      double xy[3] = {0., 0., 0.};
      ijkToXY->TransformPoint(ijk, xy);
      if (quarter == 0)
        {
        labelX = xy[0];
        }
      vtkIdType ids[2];
      ids[0] = this->twoDAxesPoints->InsertNextPoint(xy[0], quarter == 0 ? majorTickMin : minorTickMin, 0.);
      ids[1] = this->twoDAxesPoints->InsertNextPoint(xy[0], quarter == 0 ? majorTickMax : minorTickMax, 0.);
      this->twoDAxesCellArray->InsertNextCell(2, ids);
      }
    if (labelX < 30 || labelX > viewWidthPixel - 30)
      {
      continue;
      }
    std::string label = displayNode->GetDisplayStringFromValueY(offset, oldOutputValues, outputValues, 0, true);
    std::copy(outputValues, outputValues + 3, oldOutputValues);
    this->AddAxisLabel(label, static_cast<int>(labelX - fontSize * 2), 15, fontSize);
    }

  std::fill(oldOutputValues, oldOutputValues + 3, 0.);
  std::fill(outputValues, outputValues + 3, 0.);
  for (int i = 0; i < numberOfPointsVertical; i++)
    {
    const double velocity = firstVelocity + velocityStep * i;
    double labelY = -1.;
    for (int quarter = 0; quarter < 4; quarter++)
      {
      double row = 0.;
      if (!spectralAxis.VelocityToRow(velocity + velocityStep * quarter / 4., row))
        {
        continue;
        }
      double ijk[3] = {columnA, row, 0.}, xy[3] = {0., 0., 0.};
      ijkToXY->TransformPoint(ijk, xy);
      if (quarter == 0)
        {
        labelY = xy[1];
        }
      vtkIdType ids[2];
      ids[0] = this->twoDAxesPoints->InsertNextPoint(quarter == 0 ? majorTickMin : minorTickMin, xy[1], 0.);
      ids[1] = this->twoDAxesPoints->InsertNextPoint(quarter == 0 ? majorTickMax : minorTickMax, xy[1], 0.);
      this->twoDAxesCellArray->InsertNextCell(2, ids);
      }
    if (labelY < 50 || labelY > viewHeightPixel)
      {
      continue;
      }
    std::string label = displayNode->GetDisplayStringFromValueZ(velocity, oldOutputValues, outputValues, 0);
    std::copy(outputValues, outputValues + 3, oldOutputValues);
    this->AddAxisLabel(label, 20, static_cast<int>(labelY - fontSize * 0.5), fontSize);
    }

  this->twoDAxesPolyData->SetPoints(this->twoDAxesPoints);
  this->twoDAxesPolyData->SetLines(this->twoDAxesCellArray);
  this->twoDAxesMapper->SetInputData(this->twoDAxesPolyData);
  this->twoDAxesActor->SetMapper(this->twoDAxesMapper);
  this->twoDAxesActor->GetProperty()->SetLineWidth(rulerType == vtkMRMLAbstractViewNode::RulerTypeThick ? 3 : 1);
  this->twoDAxesActor->GetProperty()->SetColor(this->Color->GetValue(0),
                                               this->Color->GetValue(1),
                                               this->Color->GetValue(2));
  this->MarkerRenderer->AddViewProp(this->twoDAxesActor);
  return true;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::UpdateAxes()
{
  this->HideSlicerRuler();

  // clean the attributes before updating
  this->twoDAxesPoints->Initialize();
  this->twoDAxesPoints->Squeeze();
  this->twoDAxesCellArray->Initialize();
  this->twoDAxesCellArray->Squeeze();
  vtkActor2DCollection* actors = this->MarkerRenderer->GetActors2D();
  actors->InitTraversal();
  for (int actorIndex = 0; actorIndex < actors->GetNumberOfItems(); actorIndex++)
    {
    vtkActor2D* actor2D = actors->GetNextActor2D();
    if (!actor2D)
      {
      continue;
      }
    if (actor2D->IsA("vtkTextActor"))
      {
      this->MarkerRenderer->RemoveViewProp(actor2D);
      }
    }

  vtkMRMLAbstractViewNode* viewNode =
      vtkMRMLAbstractViewNode::SafeDownCast
      (this->External->GetMRMLDisplayableNode());
  if (!viewNode || !viewNode->GetRulerEnabled())
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
                            " failed: view node is invalid.");
    this->ShowActors(false);
    return;
    }

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(viewNode);
  if (!sliceNode)
    {
    vtkErrorWithObjectMacro(this->External,
                            "vtkMRMLAstroTwoDAxesDisplayableManager::UpdateAxes()"
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

  if (sliceNode->GetOrientation().compare("XY") &&
      sliceNode->GetOrientation().compare("ZY") &&
      sliceNode->GetOrientation().compare("XZ") &&
      sliceNode->GetOrientation().compare("PVMajor") &&
      sliceNode->GetOrientation().compare("PVMinor") &&
      sliceNode->GetOrientation().compare("PVDiagram") &&
      sliceNode->GetOrientation().compare("Reformat"))
    {
    this->ShowActors(false);
    return;
    }

  int type = viewNode->GetRulerType();
  if (type == vtkMRMLAbstractViewNode::RulerTypeNone)
    {
    // ruler not visible, no updates are needed
    this->ShowActors(false);
    return;
    }

  int viewWidthPixel = sliceNode->GetDimensions()[0];
  int viewHeightPixel = sliceNode->GetDimensions()[1];
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

  bool hasDisplay = false;

  for (int layer = 0; layer < this->col->GetNumberOfItems(); layer++)
    {
    vtkMRMLSliceLayerLogic* sliceLayerLogic =
      vtkMRMLSliceLayerLogic::SafeDownCast
        (this->col->GetItemAsObject(layer));

    if(!sliceLayerLogic)
      {
      return;
      }

    int MinorTickMin = 0, MinorTickMax = 0, MajorTickMin = 0, MajorTickMax = 0;
    int internalFontSize = this->fontSize;
    switch (type)
      {
      case vtkMRMLAbstractViewNode::RulerTypeThin:
        MinorTickMin = 2;
        MinorTickMax = 7;
        MajorTickMin = 2;
        MajorTickMax = 12;
        break;
      case vtkMRMLAbstractViewNode::RulerTypeThick:
        MinorTickMin = 3;
        MinorTickMax = 9;
        MajorTickMin = 3;
        MajorTickMax = 14;
        internalFontSize *= 1.5;
        break;
      }

    vtkMRMLAstroVolumeDisplayNode* displayNode =
      vtkMRMLAstroVolumeDisplayNode::SafeDownCast
        (sliceLayerLogic->GetVolumeDisplayNode());

    vtkMRMLAstroLabelMapVolumeDisplayNode* displayLabelNode =
      vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast
        (sliceLayerLogic->GetVolumeDisplayNode());

    if (displayNode)
      {
      vtkMRMLAstroVolumeNode* astroVolume =
        vtkMRMLAstroVolumeNode::SafeDownCast
          (sliceLayerLogic->GetVolumeNode());

      // rotated data (CROTAi != 0) are fine: the ticks are positioned with
      // the full WCS (pixel <-> world) of the volume

      // PV diagram: offset along the line and velocity (no celestial WCS)
      const char* dataModel = astroVolume ? astroVolume->GetAttribute("SlicerAstro.DATAMODEL") : nullptr;
      const bool pvDiagram = dataModel && !strcmp(dataModel, "PVDIAGRAM");

      if (!pvDiagram && strcmp(displayNode->GetSpace(), "WCS") != 0)
        {
        continue;
        }

      hasDisplay = true;

      sliceNode->UpdateMatrices();
      sliceLayerLogic->UpdateTransforms();

      if (pvDiagram || !sliceNode->GetOrientation().compare("PVDiagram"))
        {
        this->ShowActors(pvDiagram &&
          this->UpdatePVDiagramAxes(sliceLayerLogic, displayNode, astroVolume,
                                    viewWidthPixel, viewHeightPixel,
                                    MajorTickMin, MajorTickMax, MinorTickMin, MinorTickMax,
                                    type, internalFontSize));
        return;
        }

      vtkGeneralTransform* xyToIJK =
          sliceLayerLogic->GetXYToIJKTransform();

      int numberOfPointsHorizontal = (int) (viewWidthPixel / 150.) + 1;
      if (numberOfPointsHorizontal < 5)
        {
        numberOfPointsHorizontal = 5;
        }
      int numberOfPointsVertical = (int) (viewHeightPixel / 150.) + 1;
      if (numberOfPointsVertical < 5)
        {
        numberOfPointsVertical = 5;
        }
      double worldA[] = {0.,0.,0.}, worldB[] = {0.,0.,0.}, worldC[] = {0.,0.,0.}, worldD[] = {0.,0.,0.};
      double xyz[] = {0.,0.,0.}, ijk[] = {0.,0.,0.};
      double axisCoord[] = {0.,0.}, wcsStep[] = {0.,0.};
      double PVwcsStepCos = 0., PVwcsStepSin = 0., PVAngle = 0.;
      std::vector<std::vector<double> > world;
      std::vector<std::vector<double> > xyzDisplay;
      std::vector<double> temp;

      // calculate WCS coordinates of the view's corners
      xyToIJK->TransformPoint(xyz, ijk);
      int controlY1 = ijk[1];
      displayNode->GetReferenceSpace(ijk, worldA);

      xyz[0] = viewWidthPixel;
      xyToIJK->TransformPoint(xyz, ijk);
      displayNode->GetReferenceSpace(ijk, worldB);

      xyz[0] = 0.;
      xyz[1] = viewHeightPixel;
      xyToIJK->TransformPoint(xyz, ijk);
      int controlY2 = ijk[1];
      // check if the reformat plane is parallel to the velocity axes
      bool showReformat = false;
      if (controlY1 == controlY2 && !sliceNode->GetOrientation().compare("Reformat"))
        {
        showReformat = true;
        }
      else if(!sliceNode->GetOrientation().compare("Reformat")    )
        {
        this->ShowActors(false);
        return;
        }

      displayNode->GetReferenceSpace(ijk, worldC);

      xyz[0] = viewWidthPixel / 2.;
      xyz[1] = 0.;
      xyToIJK->TransformPoint(xyz, ijk);
      displayNode->GetReferenceSpace(ijk, worldD);

      // calculate the wcsSteps for the two axes
      if (!sliceNode->GetOrientation().compare("ZY"))
        {
        wcsStep[0] = displayNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldB[2]), &numberOfPointsHorizontal);
        axisCoord[0] = displayNode->GetFirstWcsTickAxisZ(worldA[2], worldB[2], wcsStep[0]);
        wcsStep[1] = displayNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
        axisCoord[1] = displayNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("XY"))
        {
        wcsStep[0] = displayNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
        axisCoord[0] = displayNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
        wcsStep[1] = displayNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
        axisCoord[1] = displayNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("XZ"))
        {
        wcsStep[0] = displayNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
        axisCoord[0] = displayNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
        wcsStep[1] = displayNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), &numberOfPointsVertical);
        axisCoord[1] = displayNode->GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("PVMajor") ||
          !sliceNode->GetOrientation().compare("PVMinor") ||
          showReformat)
        {
        double distX = (worldA[0] - worldB[0]);
        double distY = (worldA[1] - worldB[1]);
        double dist = sqrt((distX * distX) + (distY * distY));
        wcsStep[0] = displayNode->GetWcsTickStepAxisY(dist, &numberOfPointsHorizontal);

        PVAngle = atan(distY / distX);
        PVwcsStepCos = wcsStep[0] * cos(PVAngle);
        PVwcsStepSin = wcsStep[0] * sin(PVAngle);

        if (numberOfPointsHorizontal % 2 != 0)
          {
          numberOfPointsHorizontal += 1;
          }
        wcsStep[1] = displayNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), &numberOfPointsVertical);
        axisCoord[1] = displayNode->GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1]);
        }

      // allocate point along the horizontal axes
      for (int i = 0; i < numberOfPointsHorizontal; i++)
        {
        int i8 = i * 8;
        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          temp.clear();
          temp.push_back(worldA[0]);
          temp.push_back(worldA[1]);
          temp.push_back(axisCoord[0] + wcsStep[0] * i);
          world.push_back(temp);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          temp.clear();
          temp.push_back(axisCoord[0] + wcsStep[0] * i);
          temp.push_back(worldA[1]);
          temp.push_back(worldA[2]);
          world.push_back(temp);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          temp.clear();
          temp.push_back(worldD[0] + PVwcsStepCos  * (i - (numberOfPointsHorizontal / 2.)));
          temp.push_back(worldD[1] + PVwcsStepSin  * (i - (numberOfPointsHorizontal / 2.)));
          temp.push_back(worldD[2]);
          world.push_back(temp);
          }

        displayNode->GetIJKSpace(world[i], ijk);
        xyToIJK->Inverse();
        xyToIJK->TransformPoint(ijk, xyz);
        temp.clear();
        temp.push_back(xyz[0]);
        temp.push_back(xyz[1]);
        temp.push_back(xyz[2]);
        xyzDisplay.push_back(temp);
        this->twoDAxesPoints->InsertPoint(i8, xyz[0], MajorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 1, xyz[0], MajorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos / 4.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin / 4.;
          xyz[2] = worldD[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 2, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 3, xyz[0], MinorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos / 2.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin / 2.;
          xyz[2] = worldD[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 4, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 5, xyz[0], MinorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] * 3. / 4.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] * 3. / 4.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos * 3. / 4.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin * 3. / 4.;
          xyz[2] = worldD[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 6, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 7, xyz[0], MinorTickMax, 0);

        xyToIJK->Inverse();
        }

      int nTot = numberOfPointsVertical + numberOfPointsHorizontal;

      // allocate point along the vertical axes
      for (int i = numberOfPointsHorizontal; i < nTot; i++)
        {
        int ii = i - numberOfPointsHorizontal;
        int i8 = i * 8;

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          temp.clear();
          temp.push_back(worldA[0]);
          temp.push_back(axisCoord[1] + wcsStep[1] * ii);
          temp.push_back(worldA[2]);
          world.push_back(temp);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          temp.clear();
          temp.push_back(worldA[0]);
          temp.push_back(worldA[1]);
          temp.push_back(axisCoord[1] + wcsStep[1] * ii);
          world.push_back(temp);
          }

        displayNode->GetIJKSpace(world[i], ijk);
        xyToIJK->Inverse();
        xyToIJK->TransformPoint(ijk, xyz);
        temp.clear();
        temp.push_back(xyz[0]);
        temp.push_back(xyz[1]);
        temp.push_back(xyz[2]);
        xyzDisplay.push_back(temp);

        this->twoDAxesPoints->InsertPoint(i8, MajorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 1, MajorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 2, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 3, MinorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 4, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 5, MinorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
          xyz[2] = worldA[2];
          displayNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
          displayNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 6, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 7, MinorTickMax, xyz[1], 0);

        xyToIJK->Inverse();
        }

      int n = this->twoDAxesPoints->GetNumberOfPoints();

      // unify the points with lines
      std::vector<vtkSmartPointer<vtkLine> > lines;
      for (int i = 0; i < n - 1; i++)
        {
        vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
        lines.push_back(line);
        }

      int nHori8 = numberOfPointsHorizontal * 8;
      for (int i = 0; i < nHori8 - 1; i++)
        {
        if (i%2 == 0)
          {
          lines[i]->GetPointIds()->SetId(0, i);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        else
          {
          lines[i]->GetPointIds()->SetId(0, i - 1);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        }

      int nVert8 = numberOfPointsVertical * 8;
      int nTot8 = nHori8 + nVert8;
      for (int i = nHori8; i < nTot8 - 1; i++)
        {
        if (i%2 == 0)
          {
          lines[i]->GetPointIds()->SetId(0, i);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        else
          {
          lines[i]->GetPointIds()->SetId(0, i - 1);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        }

      // create the cellArray
      for (int i = 0; i < n - 1; i++)
        {
        this->twoDAxesCellArray->InsertNextCell(lines[i]);
        }

      // setup the mapper and actor
      this->twoDAxesPolyData->SetPoints(this->twoDAxesPoints);
      this->twoDAxesPolyData->SetLines(this->twoDAxesCellArray);
      this->twoDAxesMapper->SetInputData(this->twoDAxesPolyData);
      this->twoDAxesActor->SetMapper(this->twoDAxesMapper);
      switch (type)
        {
        case vtkMRMLAbstractViewNode::RulerTypeThin:
          this->twoDAxesActor->GetProperty()->SetLineWidth(1);
          break;
        case vtkMRMLAbstractViewNode::RulerTypeThick:
          this->twoDAxesActor->GetProperty()->SetLineWidth(3);
          break;
        default:
          break;
        }
      this->twoDAxesActor->GetProperty()->SetColor(this->Color->GetValue(0),
                                                   this->Color->GetValue(1),
                                                   this->Color->GetValue(2));
      this->MarkerRenderer->AddViewProp(this->twoDAxesActor);


      std::string coord;
      double outputHorizontalValues[3] = {0.}, oldOutputHorizontalValues[3] = {0.};

      // allocate 2DTextActors for the horizontal axes
      for (int i = 0; i < numberOfPointsHorizontal; i++)
        {
        if(xyzDisplay[i][0] < 30 || xyzDisplay[i][0] > viewWidthPixel - 30)
          {
          continue;
          }

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          coord = displayNode->GetDisplayStringFromValueZ(world[i][2],
                                                           oldOutputHorizontalValues,
                                                           outputHorizontalValues, 0, true);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          coord = displayNode->GetDisplayStringFromValueX(world[i][0],
                                                           oldOutputHorizontalValues,
                                                           outputHorizontalValues, 0, true);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          double dist = sqrt(((world[i][0] - worldD[0]) * (world[i][0] - worldD[0])) +
                            ((world[i][1] - worldD[1]) * (world[i][1] - worldD[1])));
          coord = displayNode->GetDisplayStringFromValueY(dist,
                                                          oldOutputHorizontalValues,
                                                          outputHorizontalValues, 0, true);
          }

        oldOutputHorizontalValues[0] = outputHorizontalValues[0];
        oldOutputHorizontalValues[1] = outputHorizontalValues[1];
        oldOutputHorizontalValues[2] = outputHorizontalValues[2];

        vtkSmartPointer<vtkTextActor> textActorHorizontal = vtkSmartPointer<vtkTextActor>::New();
        vtkTextProperty* textProperty = textActorHorizontal->GetTextProperty();
        textProperty->SetFontSize(internalFontSize);

        if (!fontStyle.compare("Arial"))
          {
          textProperty->SetFontFamilyToArial();
          }
        else if (!fontStyle.compare("Courier"))
          {
          textProperty->SetFontFamilyToCourier();
          }
        else if (!fontStyle.compare("Times"))
          {
          textProperty->SetFontFamilyToTimes();
          }
        else
          {
          textProperty->SetFontFamilyToArial();
          }

        textActorHorizontal->GetProperty()->SetColor(this->Color->GetValue(0),
                                                     this->Color->GetValue(1),
                                                     this->Color->GetValue(2));
        textActorHorizontal->SetInput(coord.c_str());

        textActorHorizontal->SetDisplayPosition((int) (xyzDisplay[i][0] - (internalFontSize * 2)), 15);

        this->MarkerRenderer->AddViewProp(textActorHorizontal);
        }

      double outputVerticalValues[3] = {0.}, oldOutputVerticalValues[3] = {0.};

      // allocate 2DTextActors for the vertical axes
      for (int i = numberOfPointsHorizontal; i < nTot; i++)
        {
        if(xyzDisplay[i][1] < 50 || xyzDisplay[i][1] > viewHeightPixel)
          {
          continue;
          }

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          coord = displayNode->GetDisplayStringFromValueY(world[i][1],
                                                           oldOutputVerticalValues,
                                                           outputVerticalValues, 0);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          coord = displayNode->GetDisplayStringFromValueZ(world[i][2],
                                                           oldOutputVerticalValues,
                                                           outputVerticalValues, 0);
          }

        oldOutputVerticalValues[0] = outputVerticalValues[0];
        oldOutputVerticalValues[1] = outputVerticalValues[1];
        oldOutputVerticalValues[2] = outputVerticalValues[2];

        vtkSmartPointer<vtkTextActor> textActorVertical = vtkSmartPointer<vtkTextActor>::New();
        vtkTextProperty* textProperty = textActorVertical->GetTextProperty();
        textProperty->SetFontSize(internalFontSize);

        if (!fontStyle.compare("Arial"))
          {
          textProperty->SetFontFamilyToArial();
          }
        else if (!fontStyle.compare("Courier"))
          {
          textProperty->SetFontFamilyToCourier();
          }
        else if (!fontStyle.compare("Times"))
          {
          textProperty->SetFontFamilyToTimes();
          }
        else
          {
          textProperty->SetFontFamilyToArial();
          }

        textActorVertical->GetProperty()->SetColor(this->Color->GetValue(0),
                                                   this->Color->GetValue(1),
                                                   this->Color->GetValue(2));
        textActorVertical->SetInput(coord.c_str());
        textActorVertical->SetDisplayPosition(20, (int) (xyzDisplay[i][1]- (internalFontSize * 0.5)));
        this->MarkerRenderer->AddViewProp(textActorVertical);
        }

      world.clear();
      xyzDisplay.clear();
      temp.clear();

      this->ShowActors(true);
      break;
      }
    else if (displayLabelNode)
      {
      if (strcmp(displayLabelNode->GetSpace(), "WCS") != 0)
        {
        continue;
        }

      // rotated data (CROTAi != 0) are fine: the ticks are positioned with
      // the full WCS (pixel <-> world) of the volume

      hasDisplay = true;

      sliceNode->UpdateMatrices();
      sliceLayerLogic->UpdateTransforms();

      vtkGeneralTransform* xyToIJK =
          sliceLayerLogic->GetXYToIJKTransform();

      int numberOfPointsHorizontal = (int) (viewWidthPixel / 150.) + 1;
      if (numberOfPointsHorizontal < 5)
        {
        numberOfPointsHorizontal = 5;
        }
      int numberOfPointsVertical = (int) (viewHeightPixel / 150.) + 1;
      if (numberOfPointsVertical < 5)
        {
        numberOfPointsVertical = 5;
        }
      double worldA[] = {0.,0.,0.}, worldB[] = {0.,0.,0.}, worldC[] = {0.,0.,0.}, worldD[] = {0.,0.,0.};
      double xyz[] = {0.,0.,0.}, ijk[] = {0.,0.,0.};
      double axisCoord[] = {0.,0.}, wcsStep[] = {0.,0.};
      double PVwcsStepCos = 0., PVwcsStepSin = 0., PVAngle = 0.;
      std::vector<std::vector<double> > world;
      std::vector<std::vector<double> > xyzDisplay;
      std::vector<double> temp;

      // calculate WCS coordinates of the view's corners
      xyToIJK->TransformPoint(xyz, ijk);
      int controlY1 = ijk[1];
      displayLabelNode->GetReferenceSpace(ijk, worldA);

      xyz[0] = viewWidthPixel;
      xyToIJK->TransformPoint(xyz, ijk);
      displayLabelNode->GetReferenceSpace(ijk, worldB);

      xyz[0] = 0.;
      xyz[1] = viewHeightPixel;
      xyToIJK->TransformPoint(xyz, ijk);
      int controlY2 = ijk[1];
      // check if the reformat plane is parallel to the velocity axes
      bool showReformat = false;
      if (controlY1 == controlY2 && !sliceNode->GetOrientation().compare("Reformat"))
        {
        showReformat = true;
        }
      else if(!sliceNode->GetOrientation().compare("Reformat")    )
        {
        this->ShowActors(false);
        return;
        }

      displayLabelNode->GetReferenceSpace(ijk, worldC);

      xyz[0] = viewWidthPixel / 2.;
      xyz[1] = 0.;
      xyToIJK->TransformPoint(xyz, ijk);
      displayLabelNode->GetReferenceSpace(ijk, worldD);

      // calculate the wcsSteps for the two axes
      if (!sliceNode->GetOrientation().compare("ZY"))
        {
        wcsStep[0] = displayLabelNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldB[2]), &numberOfPointsHorizontal);
        axisCoord[0] = displayLabelNode->GetFirstWcsTickAxisZ(worldA[2], worldB[2], wcsStep[0]);
        wcsStep[1] = displayLabelNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
        axisCoord[1] = displayLabelNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("XY"))
        {
        wcsStep[0] = displayLabelNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
        axisCoord[0] = displayLabelNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
        wcsStep[1] = displayLabelNode->GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), &numberOfPointsVertical);
        axisCoord[1] = displayLabelNode->GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("XZ"))
        {
        wcsStep[0] = displayLabelNode->GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), &numberOfPointsHorizontal);
        axisCoord[0] = displayLabelNode->GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0]);
        wcsStep[1] = displayLabelNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), &numberOfPointsVertical);
        axisCoord[1] = displayLabelNode->GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1]);
        }

      if (!sliceNode->GetOrientation().compare("PVMajor") ||
          !sliceNode->GetOrientation().compare("PVMinor") ||
          showReformat)
        {
        double distX = (worldA[0] - worldB[0]);
        double distY = (worldA[1] - worldB[1]);
        double dist = sqrt((distX * distX) + (distY * distY));
        wcsStep[0] = displayLabelNode->GetWcsTickStepAxisY(dist, &numberOfPointsHorizontal);

        PVAngle = atan(distY / distX);
        PVwcsStepCos = wcsStep[0] * cos(PVAngle);
        PVwcsStepSin = wcsStep[0] * sin(PVAngle);

        if (numberOfPointsHorizontal % 2 != 0)
          {
          numberOfPointsHorizontal += 1;
          }
        wcsStep[1] = displayLabelNode->GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), &numberOfPointsVertical);
        axisCoord[1] = displayLabelNode->GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1]);
        }

      // allocate point along the horizontal axes
      for (int i = 0; i < numberOfPointsHorizontal; i++)
        {
        int i8 = i * 8;
        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          temp.clear();
          temp.push_back(worldA[0]);
          temp.push_back(worldA[1]);
          temp.push_back(axisCoord[0] + wcsStep[0] * i);
          world.push_back(temp);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          temp.clear();
          temp.push_back(axisCoord[0] + wcsStep[0] * i);
          temp.push_back(worldA[1]);
          temp.push_back(worldA[2]);
          world.push_back(temp);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          temp.clear();
          temp.push_back(worldD[0] + PVwcsStepCos  * (i - (numberOfPointsHorizontal / 2.)));
          temp.push_back(worldD[1] + PVwcsStepSin  * (i - (numberOfPointsHorizontal / 2.)));
          temp.push_back(worldD[2]);
          world.push_back(temp);
          }

        displayLabelNode->GetIJKSpace(world[i], ijk);
        xyToIJK->Inverse();
        xyToIJK->TransformPoint(ijk, xyz);
        temp.clear();
        temp.push_back(xyz[0]);
        temp.push_back(xyz[1]);
        temp.push_back(xyz[2]);
        xyzDisplay.push_back(temp);
        this->twoDAxesPoints->InsertPoint(i8, xyz[0], MajorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 1, xyz[0], MajorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos / 4.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin / 4.;
          xyz[2] = worldD[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 2, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 3, xyz[0], MinorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos / 2.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin / 2.;
          xyz[2] = worldD[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 4, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 5, xyz[0], MinorTickMax, 0);

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] * 3. / 4.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          xyz[0] = axisCoord[0] + (wcsStep[0] * i + wcsStep[0] * 3. / 4.);
          xyz[1] = worldA[1];
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldD[0] + PVwcsStepCos * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepCos * 3. / 4.;
          xyz[1] = worldD[1] + PVwcsStepSin * (i - (numberOfPointsHorizontal / 2.)) + PVwcsStepSin * 3. / 4.;
          xyz[2] = worldD[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 6, xyz[0], MinorTickMin, 0);
        this->twoDAxesPoints->InsertPoint(i8 + 7, xyz[0], MinorTickMax, 0);

        xyToIJK->Inverse();
        }

      int nTot = numberOfPointsVertical + numberOfPointsHorizontal;

      // allocate point along the vertical axes
      for (int i = numberOfPointsHorizontal; i < nTot; i++)
        {
        int ii = i - numberOfPointsHorizontal;
        int i8 = i * 8;

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          temp.clear();
          temp.push_back(worldA[0]);
          temp.push_back(axisCoord[1] + wcsStep[1] * ii);
          temp.push_back(worldA[2]);
          world.push_back(temp);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          temp.clear();
          temp.push_back(worldA[0]);
          temp.push_back(worldA[1]);
          temp.push_back(axisCoord[1] + wcsStep[1] * ii);
          world.push_back(temp);
          }

        displayLabelNode->GetIJKSpace(world[i], ijk);
        xyToIJK->Inverse();
        xyToIJK->TransformPoint(ijk, xyz);
        temp.clear();
        temp.push_back(xyz[0]);
        temp.push_back(xyz[1]);
        temp.push_back(xyz[2]);
        xyzDisplay.push_back(temp);

        this->twoDAxesPoints->InsertPoint(i8, MajorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 1, MajorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 4.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 2, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 3, MinorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] / 2.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 4, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 5, MinorTickMax, xyz[1], 0);

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          xyz[0] = worldA[0];
          xyz[1] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
          xyz[2] = worldA[2];
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          xyz[0] = worldA[0];
          xyz[1] = worldA[1];
          xyz[2] = axisCoord[1] + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.);
          displayLabelNode->GetIJKSpace(xyz, ijk);
          }

        xyToIJK->TransformPoint(ijk, xyz);
        this->twoDAxesPoints->InsertPoint(i8 + 6, MinorTickMin, xyz[1], 0);
        this->twoDAxesPoints->InsertPoint(i8 + 7, MinorTickMax, xyz[1], 0);

        xyToIJK->Inverse();
        }

      int n = this->twoDAxesPoints->GetNumberOfPoints();

      // unify the points with lines
      std::vector<vtkSmartPointer<vtkLine> > lines;
      for (int i = 0; i < n - 1; i++)
        {
        vtkSmartPointer<vtkLine> line = vtkSmartPointer<vtkLine>::New();
        lines.push_back(line);
        }

      int nHori8 = numberOfPointsHorizontal * 8;
      for (int i = 0; i < nHori8 - 1; i++)
        {
        if (i%2 == 0)
          {
          lines[i]->GetPointIds()->SetId(0, i);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        else
          {
          lines[i]->GetPointIds()->SetId(0, i - 1);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        }

      int nVert8 = numberOfPointsVertical * 8;
      int nTot8 = nHori8 + nVert8;
      for (int i = nHori8; i < nTot8 - 1; i++)
        {
        if (i%2 == 0)
          {
          lines[i]->GetPointIds()->SetId(0, i);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        else
          {
          lines[i]->GetPointIds()->SetId(0, i - 1);
          lines[i]->GetPointIds()->SetId(1, i + 1);
          }
        }

      // create the cellArray
      for (int i = 0; i < n - 1; i++)
        {
        this->twoDAxesCellArray->InsertNextCell(lines[i]);
        }

      // setup the mapper and actor
      this->twoDAxesPolyData->SetPoints(this->twoDAxesPoints);
      this->twoDAxesPolyData->SetLines(this->twoDAxesCellArray);
      this->twoDAxesMapper->SetInputData(this->twoDAxesPolyData);
      this->twoDAxesActor->SetMapper(this->twoDAxesMapper);
      switch (type)
        {
        case vtkMRMLAbstractViewNode::RulerTypeThin:
          this->twoDAxesActor->GetProperty()->SetLineWidth(1);
          break;
        case vtkMRMLAbstractViewNode::RulerTypeThick:
          this->twoDAxesActor->GetProperty()->SetLineWidth(3);
          break;
        default:
          break;
        }
      this->twoDAxesActor->GetProperty()->SetColor(this->Color->GetValue(0),
                                                   this->Color->GetValue(1),
                                                   this->Color->GetValue(2));
      this->MarkerRenderer->AddViewProp(this->twoDAxesActor);


      std::string coord;
      double outputHorizontalValues[3] = {0.}, oldOutputHorizontalValues[3] = {0.};

      // allocate 2DTextActors for the horizontal axes
      for (int i = 0; i < numberOfPointsHorizontal; i++)
        {
        if(xyzDisplay[i][0] < 30 || xyzDisplay[i][0] > viewWidthPixel - 30)
          {
          continue;
          }

        if (!sliceNode->GetOrientation().compare("ZY"))
          {
          coord = displayLabelNode->GetDisplayStringFromValueZ(world[i][2],
                                                           oldOutputHorizontalValues,
                                                           outputHorizontalValues, 0, true);
          }

        if (!sliceNode->GetOrientation().compare("XY") ||
            !sliceNode->GetOrientation().compare("XZ"))
          {
          coord = displayLabelNode->GetDisplayStringFromValueX(world[i][0],
                                                           oldOutputHorizontalValues,
                                                           outputHorizontalValues, 0, true);
          }

        if (!sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          double dist = sqrt(((world[i][0] - worldD[0]) * (world[i][0] - worldD[0])) +
                            ((world[i][1] - worldD[1]) * (world[i][1] - worldD[1])));
          coord = displayLabelNode->GetDisplayStringFromValueY(dist,
                                                          oldOutputHorizontalValues,
                                                          outputHorizontalValues, 0, true);
          }

        oldOutputHorizontalValues[0] = outputHorizontalValues[0];
        oldOutputHorizontalValues[1] = outputHorizontalValues[1];
        oldOutputHorizontalValues[2] = outputHorizontalValues[2];

        vtkSmartPointer<vtkTextActor> textActorHorizontal = vtkSmartPointer<vtkTextActor>::New();
        vtkTextProperty* textProperty = textActorHorizontal->GetTextProperty();
        textProperty->SetFontSize(internalFontSize);

        if (!fontStyle.compare("Arial"))
          {
          textProperty->SetFontFamilyToArial();
          }
        else if (!fontStyle.compare("Courier"))
          {
          textProperty->SetFontFamilyToCourier();
          }
        else if (!fontStyle.compare("Times"))
          {
          textProperty->SetFontFamilyToTimes();
          }
        else
          {
          textProperty->SetFontFamilyToArial();
          }

        textActorHorizontal->GetProperty()->SetColor(this->Color->GetValue(0),
                                                     this->Color->GetValue(1),
                                                     this->Color->GetValue(2));
        textActorHorizontal->SetInput(coord.c_str());

        textActorHorizontal->SetDisplayPosition((int) (xyzDisplay[i][0] - (internalFontSize * 2)), 15);

        this->MarkerRenderer->AddViewProp(textActorHorizontal);
        }

      double outputVerticalValues[3] = {0.}, oldOutputVerticalValues[3] = {0.};

      // allocate 2DTextActors for the vertical axes
      for (int i = numberOfPointsHorizontal; i < nTot; i++)
        {
        if(xyzDisplay[i][1] < 50 || xyzDisplay[i][1] > viewHeightPixel)
          {
          continue;
          }

        if (!sliceNode->GetOrientation().compare("ZY") ||
            !sliceNode->GetOrientation().compare("XY"))
          {
          coord = displayLabelNode->GetDisplayStringFromValueY(world[i][1],
                                                           oldOutputVerticalValues,
                                                           outputVerticalValues, 0);
          }

        if (!sliceNode->GetOrientation().compare("XZ") ||
            !sliceNode->GetOrientation().compare("PVMajor") ||
            !sliceNode->GetOrientation().compare("PVMinor") ||
            showReformat)
          {
          coord = displayLabelNode->GetDisplayStringFromValueZ(world[i][2],
                                                           oldOutputVerticalValues,
                                                           outputVerticalValues, 0);
          }

        oldOutputVerticalValues[0] = outputVerticalValues[0];
        oldOutputVerticalValues[1] = outputVerticalValues[1];
        oldOutputVerticalValues[2] = outputVerticalValues[2];

        vtkSmartPointer<vtkTextActor> textActorVertical = vtkSmartPointer<vtkTextActor>::New();
        vtkTextProperty* textProperty = textActorVertical->GetTextProperty();
        textProperty->SetFontSize(internalFontSize);

        if (!fontStyle.compare("Arial"))
          {
          textProperty->SetFontFamilyToArial();
          }
        else if (!fontStyle.compare("Courier"))
          {
          textProperty->SetFontFamilyToCourier();
          }
        else if (!fontStyle.compare("Times"))
          {
          textProperty->SetFontFamilyToTimes();
          }
        else
          {
          textProperty->SetFontFamilyToArial();
          }

        textActorVertical->GetProperty()->SetColor(this->Color->GetValue(0),
                                                   this->Color->GetValue(1),
                                                   this->Color->GetValue(2));
        textActorVertical->SetInput(coord.c_str());
        textActorVertical->SetDisplayPosition(20, (int) (xyzDisplay[i][1]- (internalFontSize * 0.5)));
        this->MarkerRenderer->AddViewProp(textActorVertical);
        }

      world.clear();
      xyzDisplay.clear();
      temp.clear();

      this->ShowActors(true);
      break;
      }
    }

  this->col->RemoveAllItems();

  if (!hasDisplay)
    {
    this->ShowActors(false);
    }
}

//---------------------------------------------------------------------------
// vtkMRMLRulerDisplayableManager methods

//---------------------------------------------------------------------------
vtkMRMLAstroTwoDAxesDisplayableManager::vtkMRMLAstroTwoDAxesDisplayableManager()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkMRMLAstroTwoDAxesDisplayableManager::~vtkMRMLAstroTwoDAxesDisplayableManager()
{
  delete this->Internal;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
vtkRenderer *vtkMRMLAstroTwoDAxesDisplayableManager::vtkMarkerRenderer()
{
  return this->Internal->MarkerRenderer;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::SetAnnotationsColor(double red,
                                                                 double green,
                                                                 double blue)
{
  this->Internal->CustomColor = true;
  this->Internal->Color->SetValue(0, red);
  this->Internal->Color->SetValue(1, green);
  this->Internal->Color->SetValue(2, blue);
  this->Internal->UpdateAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::SetAnnotationsFontStyle(const char *font)
{
  if (!font)
    {
    return;
    }

  this->Internal->fontStyle = font;
  this->Internal->UpdateAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::SetAnnotationsFontSize(int size)
{
  this->Internal->fontSize = size;
  this->Internal->UpdateAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::Create()
{
  this->Internal->SetupMarkerRenderer();
  this->Internal->SetupAxes();
  this->Superclass::Create();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::UpdateFromViewNode()
{
  // View node is changed, which may mean that either the marker type (visibility), size, or orientation is changed
  this->Internal->UpdateAxes();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::OnMRMLDisplayableNodeModifiedEvent(vtkObject* vtkNotUsed(caller))
{
  // view node is changed
  this->UpdateFromViewNode();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::UpdateFromRenderer()
{
  // Rendering is performed, so let's re-render the marker with up-to-date orientation
  this->Internal->UpdateAxes();
}
