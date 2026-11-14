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

#ifndef __vtkMRMLAstroMeasurementLine_h
#define __vtkMRMLAstroMeasurementLine_h

// MRML includes
#include <vtkMRMLMeasurement.h>

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLAstroVolumeNode;

/// \ingroup SlicerAstro_QtModules_AstroVolume_MRML
/// Astronomical measurement of a markups line, with the WCS of the active
/// volume (selection node): the angular separation of the two points on the
/// sky (great circle; in arcsec, arcmin or degrees), or the difference of the
/// spectral coordinate (velocity, km/s) of the two points. The velocity
/// difference is defined only if the two points are in different channels.
/// The AstroVolume logic adds both to the line nodes in place of the length
/// (in millimetres, meaningless for astronomical data).
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroMeasurementLine : public vtkMRMLMeasurement
{
public:
  enum Quantities
  {
    AngularSeparation = 0,
    VelocityDifference
  };

  static vtkMRMLAstroMeasurementLine* New();
  vtkTypeMacro(vtkMRMLAstroMeasurementLine, vtkMRMLMeasurement);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Create a new instance of this measurement type.
  VTK_NEWINSTANCE
  vtkMRMLMeasurement* CreateInstance() const override { return vtkMRMLAstroMeasurementLine::New(); }

  void Copy(vtkMRMLMeasurement* source) override;

  /// Quantity (AngularSeparation or VelocityDifference); also sets the name
  /// ("separation" or "velocity difference")
  void SetQuantity(int quantity);
  vtkGetMacro(Quantity, int);

  /// Compute the measurement (input: a markups line node)
  void Compute() override;

  /// Astronomical volume whose WCS is used: the active volume of the scene of
  /// the node (nullptr if not astronomical)
  static vtkMRMLAstroVolumeNode* GetMeasurementVolume(vtkMRMLNode* markupsNode);

protected:
  vtkMRMLAstroMeasurementLine();
  ~vtkMRMLAstroMeasurementLine() override;
  vtkMRMLAstroMeasurementLine(const vtkMRMLAstroMeasurementLine&);
  void operator=(const vtkMRMLAstroMeasurementLine&);

  int Quantity{AngularSeparation};
};

#endif
