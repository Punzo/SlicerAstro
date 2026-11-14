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

#ifndef __vtkMRMLAstroMeasurement_h
#define __vtkMRMLAstroMeasurement_h

// MRML includes
#include <vtkMRMLMeasurement.h>

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

// STD includes
#include <vector>

class vtkMRMLAstroVolumeNode;

/// \ingroup SlicerAstro_QtModules_AstroVolume_MRML
/// Astronomical measurements of markups with the WCS of the active volume
/// (selection node), in place of the geometric ones (millimetres, angles in
/// the plane of the slice):
/// - Separation (lines): angular distance of the two points on the sky
///   (great circle; arcsec, arcmin or degrees);
/// - VelocityDifference (lines; curves: first and last point): difference of
///   the velocities (km/s), only for points in different channels;
/// - SkyLength (open and closed curves): length of the curve on the sky (sum
///   of the great circle distances of the points of the curve);
/// - SkyArea (closed curves in one channel): solid angle enclosed by the curve
///   (arcsec^2, arcmin^2 or deg^2);
/// - SkyAngle (angles in one channel): angle at the vertex on the sky, between
///   the great circles to the two other points (degrees).
/// The AstroVolume logic adds them to the markups (vtkSlicerAstroVolumeLogic::
/// UpdateAstroMeasurements).
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroMeasurement : public vtkMRMLMeasurement
{
public:
  enum Quantities
  {
    Separation = 0,
    VelocityDifference,
    SkyLength,
    SkyArea,
    SkyAngle
  };

  static vtkMRMLAstroMeasurement* New();
  vtkTypeMacro(vtkMRMLAstroMeasurement, vtkMRMLMeasurement);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Create a new instance of this measurement type.
  VTK_NEWINSTANCE
  vtkMRMLMeasurement* CreateInstance() const override { return vtkMRMLAstroMeasurement::New(); }

  void Copy(vtkMRMLMeasurement* source) override;

  /// Quantity (one of Quantities); also sets the name of the measurement
  void SetQuantity(int quantity);
  vtkGetMacro(Quantity, int);

  /// Name of the measurement of a quantity
  static const char* GetQuantityName(int quantity);

  /// Compute the measurement (input: a markups node)
  void Compute() override;

  /// Astronomical volume whose WCS is used: the active volume of the scene of
  /// the node (nullptr if not astronomical)
  static vtkMRMLAstroVolumeNode* GetMeasurementVolume(vtkMRMLNode* markupsNode);

protected:
  vtkMRMLAstroMeasurement();
  ~vtkMRMLAstroMeasurement() override;
  vtkMRMLAstroMeasurement(const vtkMRMLAstroMeasurement&);
  void operator=(const vtkMRMLAstroMeasurement&);

  /// Angular value (degrees) with arcsec / arcmin / degree units; area
  /// (square degrees) with arcsec^2 / arcmin^2 / deg^2 units
  void SetAngularValue(double degrees);
  void SetAreaValue(double squareDegrees);

  int Quantity{Separation};
};

#endif
