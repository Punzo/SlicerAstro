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
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLVolumeDisplayNode.h>
#include <vtkMRMLVolumeNode.h>
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
#include <vtkMath.h>
#include <cstring>
#include <algorithm>
#include <limits>
#include <cmath>
#include <vector>

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
  /// True if an input of the axes changed since the last call (slice node,
  /// slice composite node, volumes and display nodes of the slice layers)
  bool InputsChanged();
  /// Axes of a PV diagram (DATAMODEL PVDIAGRAM): offset along the line
  /// (horizontal) and velocity (vertical). Returns false if not drawn.
  bool UpdatePVDiagramAxes(vtkMRMLSliceLayerLogic* sliceLayerLogic,
                           vtkMRMLAstroVolumeDisplayNode* displayNode,
                           vtkMRMLAstroVolumeNode* pvDiagramVolume,
                           int viewWidthPixel, int viewHeightPixel,
                           int majorTickMin, int majorTickMax,
                           int minorTickMin, int minorTickMax,
                           int rulerType, int fontSize);
  /// Axes of the celestial and spectral orientations (XY, XZ, ZY) and of the
  /// PV slices (PVMajor, PVMinor, Reformat containing the spectral axis):
  /// ticks where the bottom and left sides of the view cross the
  /// coordinate values. Returns false if not drawn.
  template <class DisplayNodeType>
  bool UpdateWCSAxes(vtkMRMLSliceNode* sliceNode, vtkMRMLSliceLayerLogic* sliceLayerLogic,
                     DisplayNodeType* displayNode, int viewWidthPixel, int viewHeightPixel,
                     int majorTickMin, int majorTickMax, int minorTickMin, int minorTickMax,
                     int rulerType, int fontSize);
  void AddTick(double x0, double y0, double x1, double y1);
  void FinishTicks(int rulerType);
  /// Label of a tick: the text actors are reused between updates (creating
  /// them, with their textures, for every render was slow)
  void AddAxisLabel(const std::string& text, int x, int y, int fontSize);
  void HideUnusedLabels();
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

  /// text actors of the labels (in MarkerRenderer); the first
  /// NumberOfUsedLabels are shown
  std::vector<vtkSmartPointer<vtkTextActor> > Labels;
  std::size_t NumberOfUsedLabels{0};

  int RendererUpdateObservationId;
  vtkWeakPointer<vtkRenderer> ObservedRenderer;

  /// modification times of the inputs of the last update (InputsChanged);
  /// cleared to force the next update
  std::vector<vtkMTimeType> InputTimes;

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

  // updated before each render of the view (UpdateFromRenderer)
  this->AddRendererUpdateObserver(renderer);
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
//---------------------------------------------------------------------------
// Signed angular offset (degrees) for the labels of the position axis of PV
// slices and PV diagrams: unit from the tick step (degrees, arcmin, arcsec),
// one decimal if the step is not a whole number of the unit; 0 at the center.
std::string FormatAngularOffset(double offsetDegrees, double stepDegrees)
{
  double factor = 3600.;
  const char* unit = "\x22";
  if (stepDegrees >= 1. - 1.E-9)
    {
    factor = 1.;
    unit = "\u00B0";
    }
  else if (stepDegrees >= 1. / 60. - 1.E-9)
    {
    factor = 60.;
    unit = "\x27";
    }
  const double step = stepDegrees * factor;
  double value = offsetDegrees * factor;
  if (fabs(value) < 1.E-6 * std::max(step, 1.))
    {
    value = 0.;
    }
  std::ostringstream label;
  label.setf(std::ios::fixed, std::ios::floatfield);
  label.precision(fabs(step - std::round(step)) > 1.E-6 ? 1 : 0);
  label << value << unit;
  return label.str();
}

} // end of anonymous namespace

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::AddAxisLabel(const std::string& text,
                                                                       int x, int y, int fontSize)
{
  if (this->NumberOfUsedLabels == this->Labels.size())
    {
    vtkSmartPointer<vtkTextActor> newTextActor = vtkSmartPointer<vtkTextActor>::New();
    newTextActor->PickableOff();
    newTextActor->DragableOff();
    this->MarkerRenderer->AddViewProp(newTextActor);
    this->Labels.push_back(newTextActor);
    }
  vtkTextActor* textActor = this->Labels[this->NumberOfUsedLabels];
  this->NumberOfUsedLabels++;
  // (the setters do nothing if the value is unchanged: the texture of the
  // label is updated only if its text or style changed)
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
  textActor->VisibilityOn();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::HideUnusedLabels()
{
  for (std::size_t labelIndex = this->NumberOfUsedLabels; labelIndex < this->Labels.size(); labelIndex++)
    {
    this->Labels[labelIndex]->VisibilityOff();
    }
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::AddTick(double x0, double y0, double x1, double y1)
{
  vtkIdType ids[2];
  ids[0] = this->twoDAxesPoints->InsertNextPoint(x0, y0, 0.);
  ids[1] = this->twoDAxesPoints->InsertNextPoint(x1, y1, 0.);
  this->twoDAxesCellArray->InsertNextCell(2, ids);
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::FinishTicks(int rulerType)
{
  this->twoDAxesPoints->Modified();
  this->twoDAxesCellArray->Modified();
  this->twoDAxesPolyData->SetPoints(this->twoDAxesPoints);
  this->twoDAxesPolyData->SetLines(this->twoDAxesCellArray);
  this->twoDAxesPolyData->Modified();
  this->twoDAxesMapper->SetInputData(this->twoDAxesPolyData);
  this->twoDAxesActor->SetMapper(this->twoDAxesMapper);
  this->twoDAxesActor->GetProperty()->SetLineWidth(rulerType == vtkMRMLAbstractViewNode::RulerTypeThick ? 3 : 1);
  this->twoDAxesActor->GetProperty()->SetColor(this->Color->GetValue(0),
                                               this->Color->GetValue(1),
                                               this->Color->GetValue(2));
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
      this->AddTick(xy[0], quarter == 0 ? majorTickMin : minorTickMin,
                    xy[0], quarter == 0 ? majorTickMax : minorTickMax);
      }
    if (labelX < 30 || labelX > viewWidthPixel - 30)
      {
      continue;
      }
    std::string label = FormatAngularOffset(offset, offsetStep);
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
      this->AddTick(quarter == 0 ? majorTickMin : minorTickMin, xy[1],
                    quarter == 0 ? majorTickMax : minorTickMax, xy[1]);
      }
    if (labelY < 50 || labelY > viewHeightPixel)
      {
      continue;
      }
    std::string label = displayNode->GetDisplayStringFromValueZ(velocity, oldOutputValues, outputValues, 0);
    std::copy(outputValues, outputValues + 3, oldOutputValues);
    this->AddAxisLabel(label, 20, static_cast<int>(labelY - fontSize * 0.5), fontSize);
    }

  this->FinishTicks(rulerType);
  return true;
}

//---------------------------------------------------------------------------
namespace
{
//---------------------------------------------------------------------------
/// Values of a coordinate along a side of the view (bottom: y = 0, left:
/// x = 0), sampled every few pixels: positions (pixels along the side) and
/// values (NaN where the WCS is not defined)
struct SideSamples
{
  std::vector<double> Positions;
  std::vector<double> Values;
};

const double SIDE_SAMPLING_PIXELS = 8.;

//---------------------------------------------------------------------------
/// World coordinates (WCSLIB, one call for all the samples) of the points of
/// a side of the view
template <class DisplayNodeType>
bool SampleSideWorld(DisplayNodeType* displayNode, vtkGeneralTransform* xyToIJK,
                     bool horizontal, int lengthPixel,
                     std::vector<double>& positions, std::vector<double>& world)
{
  struct wcsprm* wcs = displayNode->GetWCSStruct();
  if (!wcs || lengthPixel <= 0)
    {
    return false;
    }
  const int numberOfSamples = static_cast<int>(lengthPixel / SIDE_SAMPLING_PIXELS) + 2;
  positions.resize(numberOfSamples);
  std::vector<double> pixel(numberOfSamples * 4, 0.), imgcrd(numberOfSamples * 4, 0.);
  std::vector<double> phi(numberOfSamples, 0.), theta(numberOfSamples, 0.);
  std::vector<int> stat(numberOfSamples, 0);
  world.assign(numberOfSamples * 4, 0.);
  for (int sample = 0; sample < numberOfSamples; sample++)
    {
    const double position = std::min(sample * SIDE_SAMPLING_PIXELS, static_cast<double>(lengthPixel));
    positions[sample] = position;
    double xyz[3] = {horizontal ? position : 0., horizontal ? 0. : position, 0.};
    double ijk[3] = {0., 0., 0.};
    xyToIJK->TransformPoint(xyz, ijk);
    // IJK (voxel centers from 0) to FITS pixel coordinates (from 1)
    for (int axis = 0; axis < 3; axis++)
      {
      pixel[sample * 4 + axis] = ijk[axis] + 1.;
      }
    pixel[sample * 4 + 3] = 1.;
    }
  // points outside the domain of the projection have stat != 0 (not an error)
  const int status = wcsp2s(wcs, numberOfSamples, 4, pixel.data(), imgcrd.data(),
                            phi.data(), theta.data(), world.data(), stat.data());
  if (status && status != WCSERR_BAD_PIX)
    {
    return false;
    }
  for (int sample = 0; sample < numberOfSamples; sample++)
    {
    if (stat[sample])
      {
      for (int axis = 0; axis < 4; axis++)
        {
        world[sample * 4 + axis] = std::numeric_limits<double>::quiet_NaN();
        }
      }
    }
  return true;
}

//---------------------------------------------------------------------------
/// Angular distance (degrees) between two points of the sky (degrees)
double AngularDistance(double longitudeA, double latitudeA, double longitudeB, double latitudeB)
{
  const double latA = vtkMath::RadiansFromDegrees(latitudeA);
  const double latB = vtkMath::RadiansFromDegrees(latitudeB);
  const double sinHalfLatitude = sin((latB - latA) / 2.);
  const double sinHalfLongitude = sin(vtkMath::RadiansFromDegrees(longitudeB - longitudeA) / 2.);
  const double haversine = sinHalfLatitude * sinHalfLatitude +
                           cos(latA) * cos(latB) * sinHalfLongitude * sinHalfLongitude;
  return vtkMath::DegreesFromRadians(2. * asin(std::min(1., sqrt(haversine))));
}

//---------------------------------------------------------------------------
/// Positions along a side where the sampled values cross value (several
/// crossings are possible, e.g. near a celestial pole)
void FindCrossings(const SideSamples& side, double value, std::vector<double>& crossings)
{
  crossings.clear();
  for (std::size_t sample = 0; sample + 1 < side.Values.size(); sample++)
    {
    const double a = side.Values[sample];
    const double b = side.Values[sample + 1];
    if (!std::isfinite(a) || !std::isfinite(b) || a == b)
      {
      continue;
      }
    // half-open intervals: a value equal to a sample is found once
    if ((a <= value && value < b) || (b < value && value <= a))
      {
      const double t = (value - a) / (b - a);
      crossings.push_back(side.Positions[sample] + t * (side.Positions[sample + 1] - side.Positions[sample]));
      }
    }
}

//---------------------------------------------------------------------------
bool ValueRange(const SideSamples& side, double& minimum, double& maximum)
{
  minimum = std::numeric_limits<double>::max();
  maximum = -std::numeric_limits<double>::max();
  for (double value : side.Values)
    {
    if (std::isfinite(value))
      {
      minimum = std::min(minimum, value);
      maximum = std::max(maximum, value);
      }
    }
  return maximum > minimum;
}

/// Coordinate along a side of the view: a world axis (0: longitude, 1:
/// latitude, 2: spectral) or the angular offset from the center of the side
const int ANGULAR_OFFSET = -1;

//---------------------------------------------------------------------------
template <class DisplayNodeType>
double TickStep(DisplayNodeType* displayNode, int coordinate, double length, int* numberOfTicks)
{
  switch (coordinate)
    {
    case 0: return displayNode->GetWcsTickStepAxisX(length, numberOfTicks);
    case 2: return displayNode->GetWcsTickStepAxisZ(length, numberOfTicks);
    default: return displayNode->GetWcsTickStepAxisY(length, numberOfTicks); // latitude, offset (degrees)
    }
}

//---------------------------------------------------------------------------
template <class DisplayNodeType>
double FirstTick(DisplayNodeType* displayNode, int coordinate, double minimum, double maximum, double step)
{
  switch (coordinate)
    {
    case 0: return displayNode->GetFirstWcsTickAxisX(minimum, maximum, step);
    case 2: return displayNode->GetFirstWcsTickAxisZ(minimum, maximum, step);
    default: return displayNode->GetFirstWcsTickAxisY(minimum, maximum, step);
    }
}

//---------------------------------------------------------------------------
template <class DisplayNodeType>
std::string TickLabel(DisplayNodeType* displayNode, int coordinate, double value, double step,
                      const double oldOutputValues[3], double outputValues[3], bool horizontal)
{
  switch (coordinate)
    {
    case ANGULAR_OFFSET:
      return FormatAngularOffset(value, step);
    case 0:
      {
      // longitude in [0, 360)
      double longitude = fmod(value, 360.);
      if (longitude < 0.)
        {
        longitude += 360.;
        }
      if (longitude > 360. - 1.E-9)
        {
        longitude = 0.;
        }
      return displayNode->GetDisplayStringFromValueX(longitude, oldOutputValues, outputValues, 0, horizontal);
      }
    case 1:
      return displayNode->GetDisplayStringFromValueY(value, oldOutputValues, outputValues, 0, horizontal);
    default:
      return displayNode->GetDisplayStringFromValueZ(value, oldOutputValues, outputValues, 0, horizontal);
    }
}
} // end of anonymous namespace

//---------------------------------------------------------------------------
template <class DisplayNodeType>
bool vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::UpdateWCSAxes(
  vtkMRMLSliceNode* sliceNode, vtkMRMLSliceLayerLogic* sliceLayerLogic,
  DisplayNodeType* displayNode, int viewWidthPixel, int viewHeightPixel,
  int majorTickMin, int majorTickMax, int minorTickMin, int minorTickMax,
  int rulerType, int fontSize)
{
  vtkGeneralTransform* xyToIJK = sliceLayerLogic ? sliceLayerLogic->GetXYToIJKTransform() : nullptr;
  if (!sliceNode || !displayNode || !xyToIJK || viewWidthPixel <= 0 || viewHeightPixel <= 0)
    {
    return false;
    }

  // coordinates along the bottom (horizontal) and left (vertical) sides
  const std::string& orientation = sliceNode->GetOrientation();
  int horizontalCoordinate = 0, verticalCoordinate = 1;
  if (orientation == "XY")
    {
    horizontalCoordinate = 0;
    verticalCoordinate = 1;
    }
  else if (orientation == "XZ")
    {
    horizontalCoordinate = 0;
    verticalCoordinate = 2;
    }
  else if (orientation == "ZY")
    {
    horizontalCoordinate = 2;
    verticalCoordinate = 1;
    }
  else if (orientation == "PVMajor" || orientation == "PVMinor" || orientation == "Reformat")
    {
    // slices containing the spectral axis (vertical): angular offset along
    // the slice. A reformatted slice is shown only if its left side is
    // along the spectral axis.
    if (orientation == "Reformat")
      {
      double xyz[3] = {0., 0., 0.}, ijkBottom[3] = {0., 0., 0.}, ijkTop[3] = {0., 0., 0.};
      xyToIJK->TransformPoint(xyz, ijkBottom);
      xyz[1] = viewHeightPixel;
      xyToIJK->TransformPoint(xyz, ijkTop);
      if (static_cast<int>(ijkBottom[1]) != static_cast<int>(ijkTop[1]))
        {
        return false;
        }
      }
    horizontalCoordinate = ANGULAR_OFFSET;
    verticalCoordinate = 2;
    }
  else
    {
    return false;
    }

  // longitude (unwrapped across 0/360 degrees) and latitude of a celestial WCS
  struct wcsprm* wcs = displayNode->GetWCSStruct();
  const bool celestial = wcs && wcs->lng == 0 && wcs->lat == 1;

  bool drawn = false;
  for (int side = 0; side < 2; side++)
    {
    const bool horizontal = (side == 0);
    const int coordinate = horizontal ? horizontalCoordinate : verticalCoordinate;
    const int lengthPixel = horizontal ? viewWidthPixel : viewHeightPixel;

    std::vector<double> world;
    SideSamples samples;
    if (!SampleSideWorld(displayNode, xyToIJK, horizontal, lengthPixel, samples.Positions, world))
      {
      continue;
      }
    const std::size_t numberOfSamples = samples.Positions.size();
    samples.Values.resize(numberOfSamples);
    if (coordinate == ANGULAR_OFFSET)
      {
      if (!celestial)
        {
        continue;
        }
      // great-circle distance from the center of the side, negative on the left
      const std::size_t center = numberOfSamples / 2;
      const double centerLongitude = world[center * 4];
      const double centerLatitude = world[center * 4 + 1];
      const double centerPosition = samples.Positions[center];
      for (std::size_t sample = 0; sample < numberOfSamples; sample++)
        {
        const double distance = AngularDistance(centerLongitude, centerLatitude,
                                                world[sample * 4], world[sample * 4 + 1]);
        samples.Values[sample] = samples.Positions[sample] < centerPosition ? -distance : distance;
        }
      }
    else
      {
      double previous = std::numeric_limits<double>::quiet_NaN();
      for (std::size_t sample = 0; sample < numberOfSamples; sample++)
        {
        double value = world[sample * 4 + coordinate];
        if (coordinate == 0 && celestial && std::isfinite(value) && std::isfinite(previous))
          {
          while (value - previous > 180.)
            {
            value -= 360.;
            }
          while (value - previous < -180.)
            {
            value += 360.;
            }
          }
        if (std::isfinite(value))
          {
          previous = value;
          }
        samples.Values[sample] = value;
        }
      }

    double minimum = 0., maximum = 0.;
    if (!ValueRange(samples, minimum, maximum))
      {
      continue;
      }
    int numberOfTicks = std::max(5, static_cast<int>(lengthPixel / 150.) + 1);
    const double step = TickStep(displayNode, coordinate, maximum - minimum, &numberOfTicks);
    if (!(step > 0.) || (maximum - minimum) / step > 1000.)
      {
      continue;
      }
    const double first = FirstTick(displayNode, coordinate, minimum, maximum, step);

    // major tick, three minor ticks per step; labels of the major ticks
    // (increasing values: the repeated leading fields of the sexagesimal
    // labels are not shown)
    double oldOutputValues[3] = {0., 0., 0.}, outputValues[3] = {0., 0., 0.};
    std::vector<double> crossings;
    for (int tick = 0; first + step * tick <= maximum + step; tick++)
      {
      const double majorValue = first + step * tick;
      for (int quarter = 0; quarter < 4; quarter++)
        {
        FindCrossings(samples, majorValue + step * quarter / 4., crossings);
        const double tickMin = quarter == 0 ? majorTickMin : minorTickMin;
        const double tickMax = quarter == 0 ? majorTickMax : minorTickMax;
        for (double position : crossings)
          {
          if (horizontal)
            {
            this->AddTick(position, tickMin, position, tickMax);
            }
          else
            {
            this->AddTick(tickMin, position, tickMax, position);
            }
          drawn = true;
          if (quarter != 0)
            {
            continue;
            }
          // labels away from the corners of the view
          if (horizontal && (position < 30 || position > viewWidthPixel - 30))
            {
            continue;
            }
          if (!horizontal && (position < 50 || position > viewHeightPixel))
            {
            continue;
            }
          std::string label = TickLabel(displayNode, coordinate, majorValue, step,
                                        oldOutputValues, outputValues, horizontal);
          std::copy(outputValues, outputValues + 3, oldOutputValues);
          if (horizontal)
            {
            this->AddAxisLabel(label, static_cast<int>(position - fontSize * 2), 15, fontSize);
            }
          else
            {
            this->AddAxisLabel(label, 20, static_cast<int>(position - fontSize * 0.5), fontSize);
            }
          }
        }
      }
    }

  if (!drawn)
    {
    return false;
    }
  // axis lines along the two sides
  this->AddTick(0., majorTickMin, viewWidthPixel, majorTickMin);
  this->AddTick(majorTickMin, 0., majorTickMin, viewHeightPixel);
  this->FinishTicks(rulerType);
  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::InputsChanged()
{
  std::vector<vtkMTimeType> inputTimes;
  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(this->External->GetMRMLDisplayableNode());
  inputTimes.push_back(sliceNode ? sliceNode->GetMTime() : 0);
  qSlicerApplication* application = qSlicerApplication::application();
  vtkMRMLSliceLogic* sliceLogic = (sliceNode && application && application->applicationLogic()) ?
    application->applicationLogic()->GetSliceLogic(sliceNode) : nullptr;
  if (sliceLogic)
    {
    vtkMRMLSliceCompositeNode* compositeNode = sliceLogic->GetSliceCompositeNode();
    inputTimes.push_back(compositeNode ? compositeNode->GetMTime() : 0);
    vtkMRMLSliceLayerLogic* layers[3] =
      { sliceLogic->GetBackgroundLayer(), sliceLogic->GetForegroundLayer(), sliceLogic->GetLabelLayer() };
    for (vtkMRMLSliceLayerLogic* layer : layers)
      {
      vtkMRMLVolumeNode* volumeNode = layer ? layer->GetVolumeNode() : nullptr;
      vtkMRMLVolumeDisplayNode* displayNode = layer ? layer->GetVolumeDisplayNode() : nullptr;
      inputTimes.push_back(volumeNode ? volumeNode->GetMTime() : 0);
      inputTimes.push_back(displayNode ? displayNode->GetMTime() : 0);
      }
    }
  if (inputTimes == this->InputTimes)
    {
    return false;
    }
  this->InputTimes = inputTimes;
  return true;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::vtkInternal::UpdateAxes()
{
  this->HideSlicerRuler();

  // the layers of the slice logic are added below: the collection may still
  // hold the layers of a previous update that returned early
  this->col->RemoveAllItems();

  // the ticks and labels of this update (the labels not used are hidden when
  // the update ends, also at the early returns)
  this->twoDAxesPoints->Reset();
  this->twoDAxesCellArray->Reset();
  this->NumberOfUsedLabels = 0;
  struct HideUnusedLabelsAtExit
    {
    vtkInternal* Internal;
    ~HideUnusedLabelsAtExit() { this->Internal->HideUnusedLabels(); }
    } hideUnusedLabelsAtExit{this};

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

      // PV diagram: offset along the line and velocity (no celestial WCS)
      const char* dataModel = astroVolume ? astroVolume->GetAttribute("SlicerAstro.DATAMODEL") : nullptr;
      const bool pvDiagram = dataModel && !strcmp(dataModel, "PVDIAGRAM");

      if (!pvDiagram && strcmp(displayNode->GetSpace(), "WCS") != 0)
        {
        continue;
        }

      hasDisplay = true;

      // (the transforms of the slice layer are up to date: updating them
      // here modified the slice logic, which rendered the view again)
      if (pvDiagram || !sliceNode->GetOrientation().compare("PVDiagram"))
        {
        this->ShowActors(pvDiagram &&
          this->UpdatePVDiagramAxes(sliceLayerLogic, displayNode, astroVolume,
                                    viewWidthPixel, viewHeightPixel,
                                    MajorTickMin, MajorTickMax, MinorTickMin, MinorTickMax,
                                    type, internalFontSize));
        return;
        }

      this->ShowActors(this->UpdateWCSAxes(sliceNode, sliceLayerLogic, displayNode,
                                           viewWidthPixel, viewHeightPixel,
                                           MajorTickMin, MajorTickMax, MinorTickMin, MinorTickMax,
                                           type, internalFontSize));
      break;
      }
    else if (displayLabelNode)
      {
      if (strcmp(displayLabelNode->GetSpace(), "WCS") != 0)
        {
        continue;
        }

      hasDisplay = true;

      this->ShowActors(this->UpdateWCSAxes(sliceNode, sliceLayerLogic, displayLabelNode,
                                           viewWidthPixel, viewHeightPixel,
                                           MajorTickMin, MajorTickMax, MinorTickMin, MinorTickMax,
                                           type, internalFontSize));
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
  this->Internal->InputTimes.clear();
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::SetAnnotationsFontStyle(const char *font)
{
  if (!font)
    {
    return;
    }

  this->Internal->fontStyle = font;
  this->Internal->InputTimes.clear();
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroTwoDAxesDisplayableManager::SetAnnotationsFontSize(int size)
{
  this->Internal->fontSize = size;
  this->Internal->InputTimes.clear();
  this->RequestRender();
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
  // The view node is changed (e.g. the marker type, size or orientation, or
  // the slice position on every mouse move while panning): the view is
  // rendered again and the axes are updated before the render
  // (UpdateFromRenderer), once per render. Updating them here, for every
  // modification, made panning slow when the mouse moves are not compressed
  // (Qt on Wayland); requesting a render here made a second render.
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
  // Rendering is performed: update the axes if the view or the data changed
  // (not for renders of e.g. the crosshair or the data probe)
  if (this->Internal->InputsChanged())
    {
    this->Internal->UpdateAxes();
    }
}
