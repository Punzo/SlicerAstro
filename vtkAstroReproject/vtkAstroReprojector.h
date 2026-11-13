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

#ifndef __vtkAstroReprojector_h
#define __vtkAstroReprojector_h

#include "vtkAstroReprojectExport.h"

// VTK includes
#include <vtkSmartPointer.h>

// STD includes
#include <atomic>
#include <functional>
#include <limits>
#include <string>
#include <vector>

class vtkImageData;
struct wcsprm;

/// \brief Reprojection, regridding and mosaicking of astronomical data cubes
/// and images (C++ only, no MRML).
///
/// The inputs (2D images or 3D cubes with a celestial WCS and optionally a
/// spectral axis) are resampled on an output grid (a WCS and dimensions) and
/// combined. For every output channel:
/// 1. spectral regridding of each input: the output channel is converted to
///    frequency (or velocity, as CASA imregrid specAsVelocity, to align
///    different lines with their rest frequencies), to the spectral frame
///    (SPECSYS) of the input (vtkAstroFrames, as casacore MFrequency, in the
///    direction of the centre of the output) and to the input channels;
///    the input channels are combined with nearest, linear, cubic
///    (Catmull-Rom) weights, or with the overlap of the channels
///    (flux-conserving averaging, for wider output channels: CASA only
///    interpolates and warns);
/// 2. spatial resampling on the output pixels: output pixel -> world ->
///    celestial frame of the input (vtkAstroFrames) -> input pixel; nearest,
///    bilinear, bicubic (Catmull-Rom = casacore Interpolate2D cubic) and
///    Lanczos-3 interpolation, the mean over the area of the output pixel
///    (supersampling), or the exact spherical overlap of the pixels
///    (Montage / astropy reproject algorithm). Jy/pixel data are scaled by
///    the ratio of the pixel areas (exact, from the WCS, at every pixel);
/// 3. combination of the inputs (linear mosaic, Sault, Staveley-Smith &
///    Brouw 1996, as CASA linearmosaic / Miriad linmos):
///    optimal: sum(w A (I A)) / sum(w A^2), pbweight: sum(w (I A)) / sum(w A),
///    mean: sum(w I) / sum(w), with A the primary beam (Gaussian or Airy model
///    of the dish, or a primary beam volume), I the primary beam corrected
///    input and w = 1 or 1 / noise^2.
/// Optionally, every input is first convolved to a target resolution
/// (common beam of the inputs, CASA imsmooth targetres/commonbeam).
/// Blank voxels (NaN) are missing data. The computations run with
/// vtkSMPTools; memory: the output, the spatial maps of the inputs (one plane
/// each) and a few planes.
class VTK_ASTRO_REPROJECT_EXPORT vtkAstroReprojector
{
public:
  enum SpatialMethod
    {
    Nearest = 0,
    Bilinear,
    Bicubic,
    Lanczos3,
    AreaAverage,   ///< mean over the output pixel (supersampled)
    ExactOverlap   ///< spherical overlap of the pixels (Montage)
    };

  enum SpectralMethod
    {
    SpectralNearest = 0,
    SpectralLinear,
    SpectralCubic,
    SpectralOverlap, ///< overlap of the channels (flux-conserving average)
    SpectralAuto     ///< overlap for wider output channels, linear otherwise
    };

  enum SpectralMatching
    {
    MatchFrequency = 0,
    MatchVelocity    ///< same velocity (each input with its rest frequency)
    };

  enum Combination
    {
    CombineMean = 0,       ///< sum(w I) / sum(w), the primary beam ignored
    CombineOptimal,        ///< sum(w A (I A)) / sum(w A^2)
    CombinePrimaryBeam     ///< sum(w (I A)) / sum(w A)
    };

  enum InputCorrection
    {
    FluxCorrect = 0,       ///< inputs corrected for the primary beam (I)
    PrimaryBeamWeighted,   ///< inputs not corrected (flat noise, I A)
    PrimaryBeamSquaredWeighted ///< I A^2
    };

  enum PrimaryBeamModel
    {
    NoPrimaryBeam = 0,
    GaussianPrimaryBeam,   ///< FWHM = factor * lambda / D
    AiryPrimaryBeam,       ///< (2 J1(x) / x)^2, x = pi D theta / lambda
    VolumePrimaryBeam      ///< Input::PrimaryBeam (same grid as the data)
    };

  struct Input
    {
    vtkImageData* Data = nullptr;  ///< float or double, one component
    const wcsprm* WCS = nullptr;   ///< celestial axes 1-2, spectral axis 3 (optional)
    std::string Name;
    std::string BUnit;
    double Beam[3] = {0., 0., 0.}; ///< FWHM major, minor (deg), position angle (deg)
    double Noise = 0.;             ///< rms (data units), <= 0: unknown
    vtkImageData* PrimaryBeam = nullptr;
    /// pointing centre (degrees, celestial frame of the input); NaN: the
    /// reference value of the WCS (CRVAL)
    double Pointing[2] = {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()};
    double DishDiameter = 25.;     ///< metres
    /// observed frequency (Hz) of images without a spectral axis (e.g. the
    /// removed single-channel axis of continuum images); 0: unknown
    double Frequency = 0.;
    };

  struct Options
    {
    int Spatial = Bilinear;
    int Spectral = SpectralAuto;
    int Matching = MatchFrequency;
    int Combine = CombineOptimal;
    int Correction = FluxCorrect;
    int PrimaryBeam = NoPrimaryBeam;
    bool NoiseWeighting = false;     ///< w = 1 / noise^2 (Miriad linmos), else 1
    double PrimaryBeamFactor = 1.13; ///< Gaussian FWHM in lambda / D
    double PrimaryBeamCutoff = 0.2;  ///< primary beam level below which inputs are ignored
    double MinimumCoverage = 0.5;    ///< fraction of an output pixel covered by data
    int Supersampling = 0;           ///< AreaAverage: subpixels per axis (0: automatic)
    bool TargetBeamEnabled = false;  ///< convolve the inputs to TargetBeam
    double TargetBeam[3] = {0., 0., 0.};
    double SaultFraction = 0.;       ///< > 0: Sault image (flux correct above this fraction of the peak weight)
    bool ComputeWeight = false;
    bool ComputeSensitivity = false;
    };

  struct Outputs
    {
    vtkSmartPointer<vtkImageData> Data;
    vtkSmartPointer<vtkImageData> Weight;      ///< sum(w A^2), sum(w A) or sum(w)
    vtkSmartPointer<vtkImageData> Sensitivity; ///< noise of the result (noise weighting)
    vtkSmartPointer<vtkImageData> Sault;       ///< Sault-weighted result
    double Beam[3] = {0., 0., 0.};
    std::string BUnit;
    std::vector<std::string> Warnings;
    std::string Error;
    };

  typedef std::function<void(double)> ProgressFunction; ///< fraction 0-1

  /// Resample the inputs on the output grid (WCS with 2 or 3 axes, the
  /// dimensions of the output data) and combine them. cancel: set to true
  /// (by another thread) to stop. Returns false on error (Outputs::Error).
  static bool Run(const std::vector<Input>& inputs, const wcsprm* outputWCS, const int outputDimensions[3],
                  const Options& options, Outputs& outputs,
                  const ProgressFunction& progress = ProgressFunction(),
                  const std::atomic<bool>* cancel = nullptr);

  /// Celestial grid: frame ("ICRS", "FK5", "FK4", "GALACTIC",
  /// "SUPERGALACTIC"), equinox (FK5/FK4), projection code ("TAN", "SIN",
  /// "ARC", "ZEA", "CAR", "AIT", "SFL", ...), centre (degrees, at the centre
  /// of the image), pixel sizes (degrees, positive: the longitude increases
  /// to the left), rotation (degrees, as the FITS CROTA2: the +y axis is at
  /// -Rotation from north through east) and size (pixels).
  struct CelestialGrid
    {
    std::string Frame = "ICRS";
    double Equinox = 0.;
    std::string Projection = "SIN";
    double Center[2] = {0., 0.};
    double PixelSize[2] = {1. / 3600., 1. / 3600.};
    double Rotation = 0.;
    int Size[2] = {100, 100};
    };

  /// Spectral grid: type ("FREQ", "VRAD", "VOPT", "WAVE", "VELO"), SPECSYS,
  /// rest frequency (Hz), value of the first channel and step (SI units:
  /// Hz, m/s, m), number of channels.
  struct SpectralGrid
    {
    std::string Type = "VRAD";
    std::string SpecSys = "BARYCENT";
    double RestFrequency = 1.420405751768e9;
    double Start = 0.;
    double Step = 1000.;
    int Count = 1;
    };

  /// New WCS (wcsfree + free it, or use FreeWCS) of a celestial grid and
  /// optionally a spectral axis (spectral = nullptr: 2D). The observation
  /// date and position (MJD-OBS, OBSGEO) are copied from like (optional).
  static wcsprm* MakeWCS(const CelestialGrid& celestial, const SpectralGrid* spectral,
                         const wcsprm* like = nullptr);
  static void FreeWCS(wcsprm* wcs);

  /// Celestial grid of a WCS (frame, centre of the image of size
  /// dimensions, pixel sizes and rotation); false if not celestial.
  static bool GetCelestialGrid(const wcsprm* wcs, const int dimensions[2], CelestialGrid& grid);

  /// Spectral grid of a WCS with dimensions[2] channels (type and SPECSYS
  /// of the WCS, or of type/specsys if given)
  static bool GetSpectralGrid(const wcsprm* wcs, int channels, SpectralGrid& grid);

  /// Spectral grid covering the channels of all the inputs (union) or their
  /// common range (intersection), with the narrowest channel width (step > 0
  /// to impose it), in the type, frame and rest frequency of grid (in/out).
  static bool FitSpectralGrid(const std::vector<Input>& inputs, bool unionOfInputs, double step,
                              SpectralGrid& grid, std::string& error);

  /// Celestial grid covering all the inputs (union) or their common area
  /// (intersection), in the frame, projection, pixel size and rotation of
  /// grid (in/out: Center and Size are computed). pixelSize <= 0: the
  /// smallest pixel of the inputs.
  static bool FitCelestialGrid(const std::vector<Input>& inputs, bool unionOfInputs, double pixelSize,
                               CelestialGrid& grid, std::string& error);

  /// Value (in the spectral type of the WCS) of a frequency, and frequency of
  /// a spectral value (SI units); false if not possible (no rest frequency).
  static bool SpectralValueToFrequency(const char* type, double value, double restFrequency, double& frequency);
  static bool FrequencyToSpectralValue(const char* type, double frequency, double restFrequency, double& value);
};

#endif
