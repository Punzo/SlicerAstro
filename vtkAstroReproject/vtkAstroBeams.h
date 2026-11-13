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

  This file was originally developed by Davide Punzo.

==============================================================================*/

#ifndef __vtkAstroBeams_h
#define __vtkAstroBeams_h

#include "vtkAstroReprojectExport.h"

#include <vtkObject.h>

class vtkDoubleArray;

/// \brief Elliptical Gaussian beams (restoring beams of radio data).
///
/// A beam is (major, minor, position angle): full widths at half maximum in
/// any angular unit (the same for all the beams of a call) and the position
/// angle of the major axis in degrees, from north through east.
///
/// The algorithms are those of CASA (reimplemented, no CASA code):
/// - Deconvolve: GaussianDeconvolver::deconvolve (Wild 1970), with its
///   tolerance for "point source" results;
/// - CommonBeam: CasaImageBeamSet::getCommonBeam (the largest beam if all
///   the beams can be deconvolved from it, otherwise iterative enclosing
///   ellipses grown by 0.1% until every beam can be deconvolved);
/// - the kernel to reach a target beam is deconvolve(target, input), and the
///   scale factor for Jy/beam data keeps the peak of point sources
///   (Image2DConvolver: pixel area / kernel area * target area / input area).
class VTK_ASTRO_REPROJECT_EXPORT vtkAstroBeams : public vtkObject
{
public:
  static vtkAstroBeams* New();
  vtkTypeMacro(vtkAstroBeams, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  enum DeconvolutionResult
    {
    Deconvolved = 0,
    PointSource,       ///< convolved (nearly) equal to the beam: result = beam
    NotDeconvolvable   ///< convolved smaller than the beam in some direction
    };

  /// Area of a Gaussian beam: pi / (4 ln 2) * major * minor
  static double Area(double major, double minor);

  /// Source size such that source (x) beam = convolved. Returns one of
  /// DeconvolutionResult.
  static int Deconvolve(const double convolved[3], const double beam[3], double result[3]);

  /// Convolution of two Gaussian beams
  static void Convolve(const double beam1[3], const double beam2[3], double result[3]);

  /// Smallest beam to which all the beams can be convolved (CASA
  /// algorithm). beams: 3 components (major, minor, position angle) per
  /// tuple. Returns false if there is no valid beam.
  static bool CommonBeam(vtkDoubleArray* beams, double result[3]);

  /// Gaussian to convolve data with beam input to reach beam target.
  /// Returns false if the target can not be reached (target not larger
  /// than input in every direction); kernel is zero if target == input.
  static bool ConvolvingBeam(const double target[3], const double input[3], double kernel[3]);

private:
  vtkAstroBeams() = default;
  ~vtkAstroBeams() override = default;
  vtkAstroBeams(const vtkAstroBeams&) = delete;
  void operator=(const vtkAstroBeams&) = delete;
};

#endif
