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

// AstroVolume MRML includes
#include "vtkMRMLAstroMeasurementLine.h"
#include "vtkMRMLAstroVolumeDisplayNode.h"
#include "vtkMRMLAstroVolumeNode.h"

// MRML includes
#include <vtkMRMLMarkupsLineNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
#include <cmath>
#include <cstring>
#include <string>

vtkStandardNewMacro(vtkMRMLAstroMeasurementLine);

//----------------------------------------------------------------------------
vtkMRMLAstroMeasurementLine::vtkMRMLAstroMeasurementLine()
{
  this->SetQuantity(AngularSeparation);
}

//----------------------------------------------------------------------------
vtkMRMLAstroMeasurementLine::~vtkMRMLAstroMeasurementLine() = default;

//----------------------------------------------------------------------------
void vtkMRMLAstroMeasurementLine::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Quantity: " << this->Quantity << "\n";
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMeasurementLine::Copy(vtkMRMLMeasurement* source)
{
  this->Superclass::Copy(source);
  vtkMRMLAstroMeasurementLine* astroSource = vtkMRMLAstroMeasurementLine::SafeDownCast(source);
  if (astroSource)
    {
    this->Quantity = astroSource->Quantity;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMeasurementLine::SetQuantity(int quantity)
{
  this->Quantity = quantity == VelocityDifference ? VelocityDifference : AngularSeparation;
  this->SetName(this->Quantity == VelocityDifference ? "velocity difference" : "separation");
  this->SetPrintFormat(this->Quantity == VelocityDifference ? "%-#.4g %s" : "%-#.4g%s");
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkMRMLAstroMeasurementLine::GetMeasurementVolume(vtkMRMLNode* markupsNode)
{
  vtkMRMLScene* scene = markupsNode ? markupsNode->GetScene() : nullptr;
  vtkMRMLSelectionNode* selectionNode = scene ?
    vtkMRMLSelectionNode::SafeDownCast(scene->GetNodeByID("vtkMRMLSelectionNodeSingleton")) : nullptr;
  if (!selectionNode || !selectionNode->GetActiveVolumeID())
    {
    return nullptr;
    }
  return vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(selectionNode->GetActiveVolumeID()));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMeasurementLine::Compute()
{
  vtkMRMLMarkupsLineNode* lineNode = vtkMRMLMarkupsLineNode::SafeDownCast(this->InputMRMLNode);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroMeasurementLine::GetMeasurementVolume(lineNode);
  vtkMRMLAstroVolumeDisplayNode* display = volume ? volume->GetAstroVolumeDisplayNode() : nullptr;
  if (!lineNode || lineNode->GetNumberOfDefinedControlPoints(true) < 2 || !display || !volume->GetImageData() ||
      !display->GetSpace() || !strcmp(display->GetSpace(), "IJK"))
    {
    this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
    return;
    }

  // world (RAS) -> IJK of the volume (also through its transform) -> WCS
  vtkNew<vtkGeneralTransform> worldToVolume;
  vtkMRMLTransformNode::GetTransformBetweenNodes(nullptr, volume->GetParentTransformNode(), worldToVolume);
  vtkNew<vtkMatrix4x4> rasToIJK;
  volume->GetRASToIJKMatrix(rasToIJK);
  double world[2][3], ijk[2][3];
  for (int point = 0; point < 2; point++)
    {
    double position[3] = {0., 0., 0.}, ras[3] = {0., 0., 0.};
    lineNode->GetNthControlPointPositionWorld(point, position);
    worldToVolume->TransformPoint(position, ras);
    const double homogeneous[4] = {ras[0], ras[1], ras[2], 1.};
    double index[4] = {0., 0., 0., 1.};
    rasToIJK->MultiplyPoint(homogeneous, index);
    for (int axis = 0; axis < 3; axis++)
      {
      ijk[point][axis] = index[axis];
      }
    if (!display->GetReferenceSpace(ijk[point], world[point]))
      {
      this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
      return;
      }
    }

  if (this->Quantity == AngularSeparation)
    {
    // great circle distance (Vincenty: accurate at all distances)
    const double lon1 = vtkMath::RadiansFromDegrees(world[0][0]), lat1 = vtkMath::RadiansFromDegrees(world[0][1]);
    const double lon2 = vtkMath::RadiansFromDegrees(world[1][0]), lat2 = vtkMath::RadiansFromDegrees(world[1][1]);
    const double dlon = lon2 - lon1;
    const double a = std::cos(lat2) * std::sin(dlon);
    const double b = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dlon);
    const double c = std::sin(lat1) * std::sin(lat2) + std::cos(lat1) * std::cos(lat2) * std::cos(dlon);
    const double separation = vtkMath::DegreesFromRadians(std::atan2(std::sqrt(a * a + b * b), c));
    // units: arcsec below 1 arcmin, arcmin below 1 degree
    this->SetValue(separation);
    if (separation < 1. / 60.)
      {
      this->SetUnits("\"");
      this->SetDisplayCoefficient(3600.);
      }
    else if (separation < 1.)
      {
      this->SetUnits("'");
      this->SetDisplayCoefficient(60.);
      }
    else
      {
      this->SetUnits("°");
      this->SetDisplayCoefficient(1.);
      }
    this->LastComputationResult = vtkMRMLMeasurement::OK;
    return;
    }

  // velocity difference: only for data cubes and points in different channels
  int dims[3] = {1, 1, 1};
  volume->GetImageData()->GetDimensions(dims);
  if (dims[2] < 2 || std::fabs(ijk[1][2] - ijk[0][2]) < 0.5)
    {
    this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
    return;
    }
  // display spectral axis: velocities in m/s (shown in km/s)
  const double difference = world[1][2] - world[0][2];
  this->SetValue(difference);
  const std::string definition = display->GetVelocityDefinition();
  this->SetUnits(definition.empty() ? std::string("km/s") : "km/s (" + definition + ")");
  this->SetDisplayCoefficient(0.001);
  this->LastComputationResult = vtkMRMLMeasurement::OK;
}
