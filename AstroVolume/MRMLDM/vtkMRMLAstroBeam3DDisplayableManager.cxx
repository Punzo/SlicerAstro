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

==============================================================================*/

// MRMLDisplayableManager includes
#include "vtkMRMLAstroBeam3DDisplayableManager.h"

// MRML includes
#include <vtkMRMLAbstractViewNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkActor.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCellArray.h>
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTextProperty.h>
#include <vtkWeakPointer.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

//---------------------------------------------------------------------------
class vtkAstroBeam3DRendererUpdateObserver : public vtkCommand
{
public:
  static vtkAstroBeam3DRendererUpdateObserver* New()
    {
    return new vtkAstroBeam3DRendererUpdateObserver;
    }
  void Execute(vtkObject* vtkNotUsed(caller), unsigned long vtkNotUsed(event), void* vtkNotUsed(callData)) override
    {
    if (this->DisplayableManager)
      {
      this->DisplayableManager->UpdateFromRenderer();
      }
    }
  vtkWeakPointer<vtkMRMLAstroBeam3DDisplayableManager> DisplayableManager;
};

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLAstroBeam3DDisplayableManager);

//---------------------------------------------------------------------------
class vtkMRMLAstroBeam3DDisplayableManager::vtkInternal
{
public:
  vtkInternal(vtkMRMLAstroBeam3DDisplayableManager* external);
  ~vtkInternal();

  void Update();
  /// The ellipsoid of the cube in RAS coordinates; false if it cannot be
  /// computed or does not fit in the cube
  bool BuildEllipsoid(vtkMRMLAstroVolumeNode* volumeNode);
  /// The inputs of the ellipsoid (to rebuild it only when they change)
  std::string InputsKey(vtkMRMLAstroVolumeNode* volumeNode);
  void RemoveRendererObserver();

  vtkMRMLAstroBeam3DDisplayableManager* External;
  vtkSmartPointer<vtkAstroBeam3DRendererUpdateObserver> RendererObserver;
  vtkWeakPointer<vtkRenderer> ObservedRenderer;
  unsigned long RendererObservationId{0};

  vtkSmartPointer<vtkPolyData> Ellipsoid;
  vtkSmartPointer<vtkPolyDataNormals> Normals;
  vtkSmartPointer<vtkPolyDataMapper> Mapper;
  vtkSmartPointer<vtkActor> Actor;
  /// "beam", facing the camera, right of the ellipsoid on the screen
  vtkSmartPointer<vtkBillboardTextActor3D> Label;
  std::string BuiltKey;

  /// The label next to the ellipsoid: offset on the screen by the projected
  /// half width of the ellipsoid (it changes with the camera)
  void PlaceLabel(vtkRenderer* renderer);
};

//---------------------------------------------------------------------------
vtkMRMLAstroBeam3DDisplayableManager::vtkInternal::vtkInternal(vtkMRMLAstroBeam3DDisplayableManager* external)
  : External(external)
{
  this->RendererObserver = vtkSmartPointer<vtkAstroBeam3DRendererUpdateObserver>::New();
  this->RendererObserver->DisplayableManager = external;
  this->Ellipsoid = vtkSmartPointer<vtkPolyData>::New();
  this->Normals = vtkSmartPointer<vtkPolyDataNormals>::New();
  this->Normals->SetInputData(this->Ellipsoid);
  this->Normals->SplittingOff();
  this->Mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->Mapper->SetInputConnection(this->Normals->GetOutputPort());
  this->Actor = vtkSmartPointer<vtkActor>::New();
  this->Actor->SetMapper(this->Mapper);
  this->Actor->PickableOff();
  this->Actor->DragableOff();
  this->Actor->VisibilityOff();
  // the colour of the beam of the slice views (SlicerAstro annotations)
  this->Actor->GetProperty()->SetColor(1., 0.731, 0.078);
  this->Actor->GetProperty()->SetOpacity(0.45);
  this->Actor->GetProperty()->SetSpecular(0.2);
  this->Label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
  this->Label->SetInput("beam");
  this->Label->PickableOff();
  this->Label->DragableOff();
  this->Label->VisibilityOff();
  this->Label->GetTextProperty()->SetFontSize(12);
  this->Label->GetTextProperty()->SetFontFamilyToArial();
  this->Label->GetTextProperty()->SetColor(1., 0.731, 0.078);
  this->Label->GetTextProperty()->SetJustificationToLeft();
  this->Label->GetTextProperty()->SetVerticalJustificationToCentered();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeam3DDisplayableManager::vtkInternal::PlaceLabel(vtkRenderer* renderer)
{
  if (!renderer || !this->Actor->GetVisibility())
    {
    this->Label->SetVisibility(false);
    return;
    }
  double bounds[6] = {0., 0., 0., 0., 0., 0.};
  this->Ellipsoid->GetBounds(bounds);
  const double center[3] = {(bounds[0] + bounds[1]) / 2., (bounds[2] + bounds[3]) / 2., (bounds[4] + bounds[5]) / 2.};
  // projected half width: the corners of the bounding box on the screen
  double centerDisplay[3] = {0., 0., 0.};
  renderer->SetWorldPoint(center[0], center[1], center[2], 1.);
  renderer->WorldToDisplay();
  renderer->GetDisplayPoint(centerDisplay);
  double halfWidth = 0.;
  for (int corner = 0; corner < 8; corner++)
    {
    renderer->SetWorldPoint(bounds[corner % 2], bounds[2 + (corner / 2) % 2], bounds[4 + corner / 4], 1.);
    renderer->WorldToDisplay();
    double display[3] = {0., 0., 0.};
    renderer->GetDisplayPoint(display);
    halfWidth = std::max(halfWidth, fabs(display[0] - centerDisplay[0]));
    }
  this->Label->SetPosition(center[0], center[1], center[2]);
  this->Label->SetDisplayOffset(static_cast<int>(halfWidth + 6.), 0);
  this->Label->SetVisibility(true);
}

//---------------------------------------------------------------------------
vtkMRMLAstroBeam3DDisplayableManager::vtkInternal::~vtkInternal()
{
  this->RemoveRendererObserver();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeam3DDisplayableManager::vtkInternal::RemoveRendererObserver()
{
  if (this->ObservedRenderer)
    {
    this->ObservedRenderer->RemoveObserver(this->RendererObservationId);
    }
  this->ObservedRenderer = nullptr;
  this->RendererObservationId = 0;
}

//---------------------------------------------------------------------------
std::string vtkMRMLAstroBeam3DDisplayableManager::vtkInternal::InputsKey(vtkMRMLAstroVolumeNode* volumeNode)
{
  std::ostringstream key;
  key << volumeNode->GetID() << "|" << volumeNode->GetMTime();
  const char* attributes[4] = {"SlicerAstro.BMAJ", "SlicerAstro.BMIN", "SlicerAstro.BPA", "SlicerAstro.SpectralFWHM"};
  for (const char* attribute : attributes)
    {
    const char* value = volumeNode->GetAttribute(attribute);
    key << "|" << (value ? value : "");
    }
  vtkImageData* imageData = volumeNode->GetImageData();
  int dimensions[3] = {0, 0, 0};
  if (imageData)
    {
    imageData->GetDimensions(dimensions);
    }
  key << "|" << dimensions[0] << "x" << dimensions[1] << "x" << dimensions[2];
  vtkMRMLAstroVolumeDisplayNode* displayNode = volumeNode->GetAstroVolumeDisplayNode();
  key << "|" << (displayNode ? displayNode->GetMTime() : 0);
  vtkMRMLTransformNode* transformNode = volumeNode->GetParentTransformNode();
  key << "|" << (transformNode ? transformNode->GetMTime() : 0);
  return key.str();
}

//---------------------------------------------------------------------------
bool vtkMRMLAstroBeam3DDisplayableManager::vtkInternal::BuildEllipsoid(vtkMRMLAstroVolumeNode* volumeNode)
{
  vtkImageData* imageData = volumeNode->GetImageData();
  if (!imageData)
    {
    return false;
    }
  int dimensions[3] = {0, 0, 0};
  imageData->GetDimensions(dimensions);
  const double halfChannels = 0.5 * volumeNode->GetSpectralFWHM();

  // size of the beam in pixels at the centre of the sky plane
  const int numberOfPoints = 61;
  vtkNew<vtkPoints> ring;
  const double reference[3] = {(dimensions[0] - 1) / 2., (dimensions[1] - 1) / 2., halfChannels - 0.5};
  if (!volumeNode->GetBeamEllipse(reference, numberOfPoints, ring))
    {
    return false;
    }
  double halfI = 0., halfJ = 0.;
  for (vtkIdType point = 0; point < ring->GetNumberOfPoints(); point++)
    {
    const double* ijk = ring->GetPoint(point);
    halfI = std::max(halfI, fabs(ijk[0] - reference[0]));
    halfJ = std::max(halfJ, fabs(ijk[1] - reference[1]));
    }

  // in the corner of the first pixel and channel, inside the data (a few
  // percent of each side from the faces of the cube)
  const double half[3] = {halfI, halfJ, halfChannels};
  double center[3] = {0., 0., 0.};
  for (int axis = 0; axis < 3; axis++)
    {
    const double padding = std::max(axis == 2 ? 0.5 : 1., 0.03 * dimensions[axis]);
    center[axis] = -0.5 + padding + half[axis];
    if (center[axis] + half[axis] + padding > dimensions[axis] - 0.5)
      {
      return false;
      }
    }
  if (!volumeNode->GetBeamEllipse(center, numberOfPoints, ring))
    {
    return false;
    }

  // IJK to RAS (with a linear parent transform)
  vtkNew<vtkMatrix4x4> ijkToRAS;
  volumeNode->GetIJKToRASMatrix(ijkToRAS);
  vtkMRMLTransformNode* transformNode = volumeNode->GetParentTransformNode();
  if (transformNode)
    {
    if (!transformNode->IsTransformToWorldLinear())
      {
      return false;
      }
    vtkNew<vtkMatrix4x4> toWorld;
    transformNode->GetMatrixTransformToWorld(toWorld);
    vtkMatrix4x4::Multiply4x4(toWorld, ijkToRAS, ijkToRAS);
    }

  // rings of the ellipsoid: the ellipse of the beam scaled by cos(latitude),
  // at sin(latitude) * half the spectral FWHM from the centre channel
  const int numberOfRings = 17;
  const int pointsPerRing = numberOfPoints - 1; // the last point is the first one again
  vtkNew<vtkPoints> points;
  vtkNew<vtkCellArray> polys;
  for (int ringIndex = 0; ringIndex < numberOfRings; ringIndex++)
    {
    const double latitude = -0.5 * vtkMath::Pi() + vtkMath::Pi() * ringIndex / (numberOfRings - 1);
    const double scale = cos(latitude);
    const double offset = sin(latitude) * halfChannels;
    for (int point = 0; point < pointsPerRing; point++)
      {
      const double* onSky = ring->GetPoint(point);
      const double ijk[4] = {center[0] + scale * (onSky[0] - center[0]),
                             center[1] + scale * (onSky[1] - center[1]),
                             center[2] + offset, 1.};
      double ras[4] = {0., 0., 0., 1.};
      ijkToRAS->MultiplyPoint(ijk, ras);
      points->InsertNextPoint(ras);
      }
    }
  for (int ringIndex = 0; ringIndex < numberOfRings - 1; ringIndex++)
    {
    for (int point = 0; point < pointsPerRing; point++)
      {
      const vtkIdType next = (point + 1) % pointsPerRing;
      const vtkIdType quad[4] = {ringIndex * pointsPerRing + point, ringIndex * pointsPerRing + next,
                                 (ringIndex + 1) * pointsPerRing + next, (ringIndex + 1) * pointsPerRing + point};
      polys->InsertNextCell(4, quad);
      }
    }
  this->Ellipsoid->SetPoints(points);
  this->Ellipsoid->SetPolys(polys);
  this->Ellipsoid->Modified();
  return true;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeam3DDisplayableManager::vtkInternal::Update()
{
  vtkMRMLAbstractViewNode* viewNode = vtkMRMLAbstractViewNode::SafeDownCast(this->External->GetMRMLDisplayableNode());
  const char* shown = viewNode ? viewNode->GetAttribute(vtkMRMLAstroBeam3DDisplayableManager::GetVisibilityAttributeName()) : nullptr;
  vtkMRMLAstroVolumeNode* volumeNode = (shown && !strcmp(shown, "on")) ?
    vtkMRMLAstroVolumeNode::GetBeam3DVolume(viewNode) : nullptr;
  if (!volumeNode)
    {
    this->Actor->SetVisibility(false);
    this->Label->SetVisibility(false);
    return;
    }
  const std::string key = this->InputsKey(volumeNode);
  if (key != this->BuiltKey)
    {
    this->BuiltKey = key;
    if (!this->BuildEllipsoid(volumeNode))
      {
      this->Ellipsoid->Initialize();
      this->BuiltKey.clear();
      }
    }
  this->Actor->SetVisibility(this->Ellipsoid->GetNumberOfPoints() > 0);
}

//---------------------------------------------------------------------------
vtkMRMLAstroBeam3DDisplayableManager::vtkMRMLAstroBeam3DDisplayableManager()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkMRMLAstroBeam3DDisplayableManager::~vtkMRMLAstroBeam3DDisplayableManager()
{
  if (this->GetRenderer())
    {
    this->GetRenderer()->RemoveViewProp(this->Internal->Actor);
    this->GetRenderer()->RemoveViewProp(this->Internal->Label);
    }
  delete this->Internal;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeam3DDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
vtkPolyData* vtkMRMLAstroBeam3DDisplayableManager::GetEllipsoidPolyData()
{
  return this->Internal->Ellipsoid;
}

//---------------------------------------------------------------------------
vtkActor* vtkMRMLAstroBeam3DDisplayableManager::GetEllipsoidActor()
{
  return this->Internal->Actor;
}

//---------------------------------------------------------------------------
vtkProp* vtkMRMLAstroBeam3DDisplayableManager::GetLabelActor()
{
  return this->Internal->Label;
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeam3DDisplayableManager::Create()
{
  vtkRenderer* renderer = this->GetRenderer();
  if (renderer)
    {
    renderer->AddViewProp(this->Internal->Actor);
    renderer->AddViewProp(this->Internal->Label);
    this->Internal->RemoveRendererObserver();
    this->Internal->ObservedRenderer = renderer;
    this->Internal->RendererObservationId =
      renderer->AddObserver(vtkCommand::StartEvent, this->Internal->RendererObserver);
    }
  this->Superclass::Create();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeam3DDisplayableManager::OnMRMLDisplayableNodeModifiedEvent(vtkObject* vtkNotUsed(caller))
{
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroBeam3DDisplayableManager::UpdateFromRenderer()
{
  this->Internal->Update();
  this->Internal->PlaceLabel(this->GetRenderer());
}
