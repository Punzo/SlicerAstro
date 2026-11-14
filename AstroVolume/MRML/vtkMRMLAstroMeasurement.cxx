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
#include "vtkMRMLAstroMeasurement.h"
#include "vtkMRMLAstroVolumeDisplayNode.h"
#include "vtkMRMLAstroVolumeNode.h"

// MRML includes
#include <vtkMRMLMarkupsAngleNode.h>
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLMarkupsCurveNode.h>
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
#include <vtkPoints.h>

// STD includes
#include <cmath>
#include <cstring>
#include <string>

vtkStandardNewMacro(vtkMRMLAstroMeasurement);

namespace
{
//----------------------------------------------------------------------------
/// Great circle distance (degrees) of two sky positions (degrees; Vincenty:
/// accurate at all distances)
double GreatCircleDistance(const double world1[3], const double world2[3])
{
  const double lon1 = vtkMath::RadiansFromDegrees(world1[0]), lat1 = vtkMath::RadiansFromDegrees(world1[1]);
  const double lon2 = vtkMath::RadiansFromDegrees(world2[0]), lat2 = vtkMath::RadiansFromDegrees(world2[1]);
  const double dlon = lon2 - lon1;
  const double a = std::cos(lat2) * std::sin(dlon);
  const double b = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dlon);
  const double c = std::sin(lat1) * std::sin(lat2) + std::cos(lat1) * std::cos(lat2) * std::cos(dlon);
  return vtkMath::DegreesFromRadians(std::atan2(std::sqrt(a * a + b * b), c));
}

//----------------------------------------------------------------------------
/// Position angle (degrees, from north through east) of the great circle from
/// world1 to world2
double PositionAngle(const double world1[3], const double world2[3])
{
  const double lon1 = vtkMath::RadiansFromDegrees(world1[0]), lat1 = vtkMath::RadiansFromDegrees(world1[1]);
  const double lon2 = vtkMath::RadiansFromDegrees(world2[0]), lat2 = vtkMath::RadiansFromDegrees(world2[1]);
  const double dlon = lon2 - lon1;
  return vtkMath::DegreesFromRadians(std::atan2(std::sin(dlon) * std::cos(lat2),
    std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dlon)));
}

//----------------------------------------------------------------------------
/// Points (world RAS) to IJK of the volume and to WCS; false if a point has
/// no world coordinates
bool PointsToWCS(vtkMRMLAstroVolumeNode* volume, const std::vector<double>& positions,
                 std::vector<double>& ijk, std::vector<double>& world)
{
  vtkMRMLAstroVolumeDisplayNode* display = volume->GetAstroVolumeDisplayNode();
  vtkNew<vtkGeneralTransform> worldToVolume;
  vtkMRMLTransformNode::GetTransformBetweenNodes(nullptr, volume->GetParentTransformNode(), worldToVolume);
  vtkNew<vtkMatrix4x4> rasToIJK;
  volume->GetRASToIJKMatrix(rasToIJK);
  const std::size_t count = positions.size() / 3;
  ijk.resize(3 * count);
  world.resize(3 * count);
  for (std::size_t point = 0; point < count; point++)
    {
    double ras[3] = {0., 0., 0.};
    worldToVolume->TransformPoint(&positions[3 * point], ras);
    const double homogeneous[4] = {ras[0], ras[1], ras[2], 1.};
    double index[4] = {0., 0., 0., 1.};
    rasToIJK->MultiplyPoint(homogeneous, index);
    std::copy(index, index + 3, &ijk[3 * point]);
    if (!display->GetReferenceSpace(&ijk[3 * point], &world[3 * point]))
      {
      return false;
      }
    }
  return true;
}

//----------------------------------------------------------------------------
/// All the points in one channel (2D data: always)
bool InOneChannel(vtkMRMLAstroVolumeNode* volume, const std::vector<double>& ijk)
{
  int dims[3] = {1, 1, 1};
  volume->GetImageData()->GetDimensions(dims);
  if (dims[2] < 2)
    {
    return true;
    }
  for (std::size_t point = 1; point < ijk.size() / 3; point++)
    {
    if (std::fabs(ijk[3 * point + 2] - ijk[2]) >= 0.5)
      {
      return false;
      }
    }
  return true;
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
vtkMRMLAstroMeasurement::vtkMRMLAstroMeasurement()
{
  this->SetQuantity(Separation);
}

//----------------------------------------------------------------------------
vtkMRMLAstroMeasurement::~vtkMRMLAstroMeasurement() = default;

//----------------------------------------------------------------------------
void vtkMRMLAstroMeasurement::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Quantity: " << this->Quantity << "\n";
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMeasurement::Copy(vtkMRMLMeasurement* source)
{
  this->Superclass::Copy(source);
  vtkMRMLAstroMeasurement* astroSource = vtkMRMLAstroMeasurement::SafeDownCast(source);
  if (astroSource)
    {
    this->Quantity = astroSource->Quantity;
    }
}

//----------------------------------------------------------------------------
const char* vtkMRMLAstroMeasurement::GetQuantityName(int quantity)
{
  switch (quantity)
    {
    case VelocityDifference: return "velocity difference";
    case SkyLength: return "sky length";
    case SkyArea: return "sky area";
    case SkyAngle: return "sky angle";
    default: return "separation";
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMeasurement::SetQuantity(int quantity)
{
  this->Quantity = (quantity >= Separation && quantity <= SkyAngle) ? quantity : Separation;
  this->SetName(vtkMRMLAstroMeasurement::GetQuantityName(this->Quantity));
  this->SetPrintFormat(this->Quantity == VelocityDifference ? "%-#.4g %s" : "%-#.4g%s");
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkMRMLAstroMeasurement::GetMeasurementVolume(vtkMRMLNode* markupsNode)
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
void vtkMRMLAstroMeasurement::SetAngularValue(double degrees)
{
  this->SetValue(degrees);
  if (degrees < 1. / 60.)
    {
    this->SetUnits("\"");
    this->SetDisplayCoefficient(3600.);
    }
  else if (degrees < 1.)
    {
    this->SetUnits("'");
    this->SetDisplayCoefficient(60.);
    }
  else
    {
    this->SetUnits("°");
    this->SetDisplayCoefficient(1.);
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMeasurement::SetAreaValue(double squareDegrees)
{
  this->SetValue(squareDegrees);
  if (squareDegrees < 1. / 3600.)
    {
    this->SetUnits(" arcsec²");
    this->SetDisplayCoefficient(3600. * 3600.);
    }
  else if (squareDegrees < 1.)
    {
    this->SetUnits(" arcmin²");
    this->SetDisplayCoefficient(3600.);
    }
  else
    {
    this->SetUnits(" deg²");
    this->SetDisplayCoefficient(1.);
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMeasurement::Compute()
{
  vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(this->InputMRMLNode);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroMeasurement::GetMeasurementVolume(markupsNode);
  vtkMRMLAstroVolumeDisplayNode* display = volume ? volume->GetAstroVolumeDisplayNode() : nullptr;
  if (!markupsNode || !display || !volume->GetImageData() || !display->GetSpace() ||
      !strcmp(display->GetSpace(), "IJK"))
    {
    this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
    return;
    }
  const int defined = markupsNode->GetNumberOfDefinedControlPoints(true);
  const bool angle = markupsNode->IsA("vtkMRMLMarkupsAngleNode");
  const bool closed = markupsNode->IsA("vtkMRMLMarkupsClosedCurveNode");
  if (defined < (angle ? 3 : 2) || (this->Quantity == SkyArea && defined < 3))
    {
    this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
    return;
    }

  // control points, or the points of the curve (length and area of curves)
  std::vector<double> positions;
  vtkMRMLMarkupsCurveNode* curve = vtkMRMLMarkupsCurveNode::SafeDownCast(markupsNode);
  vtkPoints* curvePoints = curve ? curve->GetCurvePointsWorld() : nullptr;
  if ((this->Quantity == SkyLength || this->Quantity == SkyArea) && curvePoints &&
      curvePoints->GetNumberOfPoints() > 1)
    {
    positions.resize(3 * curvePoints->GetNumberOfPoints());
    for (vtkIdType point = 0; point < curvePoints->GetNumberOfPoints(); point++)
      {
      curvePoints->GetPoint(point, &positions[3 * point]);
      }
    }
  else
    {
    const int count = markupsNode->GetNumberOfControlPoints();
    for (int point = 0; point < count; point++)
      {
      double position[3] = {0., 0., 0.};
      markupsNode->GetNthControlPointPositionWorld(point, position);
      positions.insert(positions.end(), position, position + 3);
      }
    }
  std::vector<double> ijk, world;
  if (!PointsToWCS(volume, positions, ijk, world))
    {
    this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
    return;
    }
  const std::size_t count = world.size() / 3;
  const std::size_t last = count - 1;

  switch (this->Quantity)
    {
    case Separation:
      this->SetAngularValue(GreatCircleDistance(&world[0], &world[3 * last]));
      break;
    case VelocityDifference:
      {
      // data cubes, end points in different channels; display velocities in m/s
      int dims[3] = {1, 1, 1};
      volume->GetImageData()->GetDimensions(dims);
      if (dims[2] < 2 || closed || std::fabs(ijk[3 * last + 2] - ijk[2]) < 0.5)
        {
        this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
        return;
        }
      this->SetValue(world[3 * last + 2] - world[2]);
      const std::string definition = display->GetVelocityDefinition();
      this->SetUnits(definition.empty() ? std::string("km/s") : "km/s (" + definition + ")");
      this->SetDisplayCoefficient(0.001);
      break;
      }
    case SkyLength:
      {
      double length = 0.;
      for (std::size_t point = 1; point < count; point++)
        {
        length += GreatCircleDistance(&world[3 * (point - 1)], &world[3 * point]);
        }
      if (closed && count > 2)
        {
        length += GreatCircleDistance(&world[3 * last], &world[0]);
        }
      this->SetAngularValue(length);
      break;
      }
    case SkyArea:
      {
      // closed curve in one channel: area of the polygon of its points on the
      // plane tangent to the sky at their mean direction (gnomonic projection;
      // the curves are small compared to the sphere)
      if (!InOneChannel(volume, ijk))
        {
        this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
        return;
        }
      double mean[3] = {0., 0., 0.};
      std::vector<double> unit(3 * count);
      for (std::size_t point = 0; point < count; point++)
        {
        const double lon = vtkMath::RadiansFromDegrees(world[3 * point]);
        const double lat = vtkMath::RadiansFromDegrees(world[3 * point + 1]);
        unit[3 * point] = std::cos(lat) * std::cos(lon);
        unit[3 * point + 1] = std::cos(lat) * std::sin(lon);
        unit[3 * point + 2] = std::sin(lat);
        for (int axis = 0; axis < 3; axis++)
          {
          mean[axis] += unit[3 * point + axis];
          }
        }
      vtkMath::Normalize(mean);
      // tangent plane basis: east and north at the mean direction
      double east[3] = {-mean[1], mean[0], 0.};
      if (vtkMath::Normalize(east) == 0.)
        {
        east[0] = 1.;
        east[1] = 0.;
        }
      double north[3];
      vtkMath::Cross(mean, east, north);
      double area = 0., previousX = 0., previousY = 0.;
      for (std::size_t point = 0; point <= count; point++)
        {
        const double* u = &unit[3 * (point % count)];
        const double dot = vtkMath::Dot(u, mean);
        const double x = vtkMath::Dot(u, east) / dot, y = vtkMath::Dot(u, north) / dot;
        if (point > 0)
          {
          area += previousX * y - x * previousY;
          }
        previousX = x;
        previousY = y;
        }
      // radians^2 (small areas: gnomonic plane area = solid angle)
      this->SetAreaValue(std::fabs(0.5 * area) * vtkMath::DegreesFromRadians(1.) * vtkMath::DegreesFromRadians(1.));
      break;
      }
    case SkyAngle:
      {
      // angle at the vertex (second point) between the great circles to the
      // first and third points, all in one channel
      if (count < 3 || !InOneChannel(volume, ijk))
        {
        this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
        return;
        }
      double difference = std::fabs(PositionAngle(&world[3], &world[0]) - PositionAngle(&world[3], &world[6]));
      if (difference > 180.)
        {
        difference = 360. - difference;
        }
      this->SetValue(difference);
      this->SetUnits("°");
      this->SetDisplayCoefficient(1.);
      break;
      }
    default:
      this->ClearValue(vtkMRMLMeasurement::InsufficientInput);
      return;
    }
  this->LastComputationResult = vtkMRMLMeasurement::OK;
}
