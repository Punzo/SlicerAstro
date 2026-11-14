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

#include "vtkAstroReprojector.h"
#include "vtkAstroBeams.h"
#include "ThirdParty/overlap/overlapArea.h"

// SlicerAstro includes
#include <vtkAstroFrames.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkFFT.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSMPThreadLocal.h>
#include <vtkSMPTools.h>

// WCSLIB includes
#include <wcslib.h>

// STD includes
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>

namespace
{
const double NaN = std::numeric_limits<double>::quiet_NaN();
const double DEG2RAD = vtkMath::Pi() / 180.;
const double SPEED_OF_LIGHT = 299792458.0;

//----------------------------------------------------------------------------
std::string UpperCase(const std::string& text)
{
  std::string result = text;
  for (std::size_t index = 0; index < result.size(); index++)
    {
    result[index] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[index])));
    }
  while (!result.empty() && result.back() == ' ')
    {
    result.pop_back();
    }
  return result;
}

//----------------------------------------------------------------------------
/// Deep copy of a wcsprm (one per thread: WCSLIB functions modify it)
class WCSCopy
{
public:
  explicit WCSCopy(const wcsprm* source)
    {
    this->WCS.flag = -1;
    if (source && !wcssub(1, const_cast<wcsprm*>(source), nullptr, nullptr, &this->WCS) && !wcsset(&this->WCS))
      {
      this->Valid = true;
      }
    }
  ~WCSCopy()
    {
    wcsfree(&this->WCS);
    }
  WCSCopy(const WCSCopy&) = delete;
  WCSCopy& operator=(const WCSCopy&) = delete;
  wcsprm WCS;
  bool Valid = false;
};

//----------------------------------------------------------------------------
int CelestialFrameOf(const wcsprm* wcs, double& equinox)
{
  equinox = 0.;
  if (!wcs || wcs->lng < 0)
    {
    return vtkAstroFrames::CelestialUnknown;
    }
  const double headerEquinox = (wcs->equinox == UNDEFINED || !(wcs->equinox > 0.)) ? 0. : wcs->equinox;
  return vtkAstroFrames::GetCelestialFrame(wcs->ctype[wcs->lng], wcs->radesys, headerEquinox, equinox);
}

//----------------------------------------------------------------------------
double ObservationMJD(const wcsprm* wcs)
{
  if (!wcs)
    {
    return 0.;
    }
  if (wcs->mjdobs != UNDEFINED && wcs->mjdobs > 0.)
    {
    return wcs->mjdobs;
    }
  if (wcs->mjdavg != UNDEFINED && wcs->mjdavg > 0.)
    {
    return wcs->mjdavg;
    }
  return 0.;
}

//----------------------------------------------------------------------------
/// Conversion of celestial positions (degrees) between the frames of two
/// WCS: a rotation matrix when possible, vtkAstroFrames per point for FK4.
class CelestialConverter
{
public:
  CelestialConverter(const wcsprm* from, const wcsprm* to)
    {
    this->FromFrame = CelestialFrameOf(from, this->FromEquinox);
    this->ToFrame = CelestialFrameOf(to, this->ToEquinox);
    this->EpochMJD = ObservationMJD(from) > 0. ? ObservationMJD(from) : ObservationMJD(to);
    if (this->FromFrame == vtkAstroFrames::CelestialUnknown || this->ToFrame == vtkAstroFrames::CelestialUnknown)
      {
      // unknown systems: the same only if the axis types are the same
      const std::string fromType = (from && from->lng >= 0) ? std::string(from->ctype[from->lng]).substr(0, 4) : "";
      const std::string toType = (to && to->lng >= 0) ? std::string(to->ctype[to->lng]).substr(0, 4) : "";
      this->Identity = !fromType.empty() && fromType == toType;
      this->Valid = this->Identity;
      return;
      }
    this->Valid = true;
    if (this->FromFrame == this->ToFrame &&
        (this->FromEquinox == this->ToEquinox || this->FromFrame == vtkAstroFrames::ICRS ||
         this->FromFrame == vtkAstroFrames::Galactic || this->FromFrame == vtkAstroFrames::Supergalactic))
      {
      this->Identity = true;
      return;
      }
    const bool fk4 = this->FromFrame == vtkAstroFrames::FK4 || this->FromFrame == vtkAstroFrames::FK4NoETerms ||
                     this->ToFrame == vtkAstroFrames::FK4 || this->ToFrame == vtkAstroFrames::FK4NoETerms;
    if (!fk4)
      {
      // rotation: images of the unit vectors
      const double axes[3][2] = {{0., 0.}, {90., 0.}, {0., 90.}};
      for (int axis = 0; axis < 3; axis++)
        {
        double result[2];
        vtkAstroFrames::ConvertCelestial(axes[axis][0], axes[axis][1], this->FromFrame, this->FromEquinox,
                                         this->ToFrame, this->ToEquinox, this->EpochMJD, result);
        const double lon = result[0] * DEG2RAD, lat = result[1] * DEG2RAD;
        this->Matrix[0][axis] = std::cos(lat) * std::cos(lon);
        this->Matrix[1][axis] = std::cos(lat) * std::sin(lon);
        this->Matrix[2][axis] = std::sin(lat);
        }
      this->Rotation = true;
      }
    }

  bool Convert(double& lon, double& lat) const
    {
    if (this->Identity)
      {
      return true;
      }
    if (this->Rotation)
      {
      const double l = lon * DEG2RAD, b = lat * DEG2RAD;
      const double v[3] = {std::cos(b) * std::cos(l), std::cos(b) * std::sin(l), std::sin(b)};
      double w[3];
      for (int row = 0; row < 3; row++)
        {
        w[row] = this->Matrix[row][0] * v[0] + this->Matrix[row][1] * v[1] + this->Matrix[row][2] * v[2];
        }
      lon = std::atan2(w[1], w[0]) / DEG2RAD;
      if (lon < 0.)
        {
        lon += 360.;
        }
      lat = std::asin(std::max(-1., std::min(1., w[2]))) / DEG2RAD;
      return true;
      }
    double result[2];
    if (!vtkAstroFrames::ConvertCelestial(lon, lat, this->FromFrame, this->FromEquinox, this->ToFrame,
                                          this->ToEquinox, this->EpochMJD, result))
      {
      return false;
      }
    lon = result[0];
    lat = result[1];
    return true;
    }

  int FromFrame = vtkAstroFrames::CelestialUnknown;
  int ToFrame = vtkAstroFrames::CelestialUnknown;
  double FromEquinox = 0.;
  double ToEquinox = 0.;
  double EpochMJD = 0.;
  bool Valid = false;
  bool Identity = false;
  bool Rotation = false;
  double Matrix[3][3] = {{1., 0., 0.}, {0., 1., 0.}, {0., 0., 1.}};
};

//----------------------------------------------------------------------------
/// Maps pixels of one grid (FITS pixel coordinates, celestial axes) to the
/// pixels of another one, row by row (thread local WCS copies).
class PixelMapper
{
public:
  PixelMapper(const wcsprm* from, const wcsprm* to)
    : From(from), To(to), Converter(from, to)
    {
    }

  /// fromPixels: n (x, y) FITS pixel coordinates of the celestial axes of
  /// from; toPixels: the (x, y) FITS pixel coordinates in to (NaN where not
  /// defined); world (optional): the (lon, lat) in the frame of from;
  /// convertedWorld (optional): the (lon, lat) in the frame of to.
  void Map(int n, const double* fromPixels, double* toPixels, double* world = nullptr,
           double* convertedWorld = nullptr)
    {
    std::shared_ptr<WCSCopy>& fromCopy = this->FromCopies.Local();
    std::shared_ptr<WCSCopy>& toCopy = this->ToCopies.Local();
    if (!fromCopy)
      {
      fromCopy = std::make_shared<WCSCopy>(this->From);
      toCopy = std::make_shared<WCSCopy>(this->To);
      }
    std::fill(toPixels, toPixels + 2 * n, NaN);
    if (world)
      {
      std::fill(world, world + 2 * n, NaN);
      }
    if (convertedWorld)
      {
      std::fill(convertedWorld, convertedWorld + 2 * n, NaN);
      }
    if (!fromCopy->Valid || !toCopy->Valid || !this->Converter.Valid || n <= 0)
      {
      return;
      }
    wcsprm* fromWCS = &fromCopy->WCS;
    wcsprm* toWCS = &toCopy->WCS;
    const int fromAxes = fromWCS->naxis, toAxes = toWCS->naxis;
    std::vector<double> pixel(n * fromAxes), imgcrd(n * std::max(fromAxes, toAxes)), worldFrom(n * fromAxes);
    std::vector<double> phi(n), theta(n), worldTo(n * toAxes), pixelTo(n * toAxes);
    std::vector<int> stat(n);
    for (int point = 0; point < n; point++)
      {
      for (int axis = 0; axis < fromAxes; axis++)
        {
        pixel[point * fromAxes + axis] = fromWCS->crpix[axis];
        }
      pixel[point * fromAxes + fromWCS->lng] = fromPixels[2 * point];
      pixel[point * fromAxes + fromWCS->lat] = fromPixels[2 * point + 1];
      }
    int status = wcsp2s(fromWCS, n, fromAxes, pixel.data(), imgcrd.data(), phi.data(), theta.data(),
                        worldFrom.data(), stat.data());
    if (status && status != WCSERR_BAD_PIX)
      {
      return;
      }
    std::vector<char> valid(n, 1);
    for (int point = 0; point < n; point++)
      {
      double lon = worldFrom[point * fromAxes + fromWCS->lng];
      double lat = worldFrom[point * fromAxes + fromWCS->lat];
      if (stat[point] || !this->Converter.Convert(lon, lat))
        {
        valid[point] = 0;
        lon = toWCS->crval[toWCS->lng];
        lat = toWCS->crval[toWCS->lat];
        }
      if (world)
        {
        world[2 * point] = valid[point] ? worldFrom[point * fromAxes + fromWCS->lng] : NaN;
        world[2 * point + 1] = valid[point] ? worldFrom[point * fromAxes + fromWCS->lat] : NaN;
        }
      if (convertedWorld)
        {
        convertedWorld[2 * point] = valid[point] ? lon : NaN;
        convertedWorld[2 * point + 1] = valid[point] ? lat : NaN;
        }
      for (int axis = 0; axis < toAxes; axis++)
        {
        worldTo[point * toAxes + axis] = toWCS->crval[axis];
        }
      worldTo[point * toAxes + toWCS->lng] = lon;
      worldTo[point * toAxes + toWCS->lat] = lat;
      }
    std::fill(stat.begin(), stat.end(), 0);
    status = wcss2p(toWCS, n, toAxes, worldTo.data(), phi.data(), theta.data(), imgcrd.data(), pixelTo.data(),
                    stat.data());
    if (status && status != WCSERR_BAD_WORLD)
      {
      return;
      }
    for (int point = 0; point < n; point++)
      {
      if (valid[point] && !stat[point])
        {
        toPixels[2 * point] = pixelTo[point * toAxes + toWCS->lng];
        toPixels[2 * point + 1] = pixelTo[point * toAxes + toWCS->lat];
        }
      }
    }

  const wcsprm* From;
  const wcsprm* To;
  CelestialConverter Converter;
  vtkSMPThreadLocal<std::shared_ptr<WCSCopy> > FromCopies;
  vtkSMPThreadLocal<std::shared_ptr<WCSCopy> > ToCopies;
};

//----------------------------------------------------------------------------
double RestFrequencyOf(const wcsprm* wcs)
{
  if (!wcs)
    {
    return 0.;
    }
  if (wcs->restfrq > 0.)
    {
    return wcs->restfrq;
    }
  if (wcs->restwav > 0.)
    {
    return SPEED_OF_LIGHT / wcs->restwav;
    }
  return 0.;
}

//----------------------------------------------------------------------------
std::string SpectralTypeOf(const wcsprm* wcs)
{
  if (!wcs || wcs->spec < 0)
    {
    return "";
    }
  return UpperCase(std::string(wcs->ctype[wcs->spec]).substr(0, 4));
}

//----------------------------------------------------------------------------
bool IsVelocityType(const std::string& type)
{
  return type == "VRAD" || type == "VOPT" || type == "VELO" || type == "ZOPT" || type == "FELO";
}

//----------------------------------------------------------------------------
/// World value of the spectral axis (FITS pixel coordinate) at the
/// reference point of the celestial axes
bool SpectralWorld(wcsprm* wcs, double pixel, double& world)
{
  const int n = wcs->naxis;
  std::vector<double> pixcrd(n), imgcrd(n), worldcrd(n);
  double phi = 0., theta = 0.;
  int stat = 0;
  for (int axis = 0; axis < n; axis++)
    {
    pixcrd[axis] = wcs->crpix[axis];
    }
  pixcrd[wcs->spec] = pixel;
  if (wcsp2s(wcs, 1, n, pixcrd.data(), imgcrd.data(), &phi, &theta, worldcrd.data(), &stat))
    {
    return false;
    }
  world = worldcrd[wcs->spec];
  return true;
}

//----------------------------------------------------------------------------
/// FITS pixel coordinate of a spectral world value (reference point of the
/// celestial axes)
bool SpectralPixel(wcsprm* wcs, double world, double& pixel)
{
  const int n = wcs->naxis;
  std::vector<double> pixcrd(n), imgcrd(n), worldcrd(n);
  double phi = 0., theta = 0.;
  int stat = 0;
  for (int axis = 0; axis < n; axis++)
    {
    worldcrd[axis] = wcs->crval[axis];
    }
  worldcrd[wcs->spec] = world;
  if (wcss2p(wcs, 1, n, worldcrd.data(), &phi, &theta, imgcrd.data(), pixcrd.data(), &stat))
    {
    return false;
    }
  pixel = pixcrd[wcs->spec];
  return true;
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
bool vtkAstroReprojector::SpectralValueToFrequency(const char* type, double value, double restFrequency,
                                                   double& frequency)
{
  const std::string spectralType = UpperCase(type ? type : "").substr(0, 4);
  if (spectralType == "FREQ")
    {
    frequency = value;
    return true;
    }
  spxprm spx;
  std::memset(&spx, 0, sizeof(spx));
  const char* specxType = spectralType == "FELO" ? "VOPT" : spectralType.c_str();
  if (specx(specxType, value, restFrequency, 0., &spx) || !(spx.freq > 0.))
    {
    return false;
    }
  if (IsVelocityType(spectralType) && !(restFrequency > 0.))
    {
    return false;
    }
  frequency = spx.freq;
  return true;
}

//----------------------------------------------------------------------------
bool vtkAstroReprojector::FrequencyToSpectralValue(const char* type, double frequency, double restFrequency,
                                                   double& value)
{
  const std::string spectralType = UpperCase(type ? type : "").substr(0, 4);
  if (spectralType == "FREQ")
    {
    value = frequency;
    return true;
    }
  spxprm spx;
  std::memset(&spx, 0, sizeof(spx));
  if (!(frequency > 0.) || specx("FREQ", frequency, restFrequency, 0., &spx))
    {
    return false;
    }
  if (IsVelocityType(spectralType) && !(restFrequency > 0.))
    {
    return false;
    }
  if (spectralType == "WAVE") { value = spx.wave; return true; }
  if (spectralType == "AWAV") { value = spx.awav; return true; }
  if (spectralType == "VRAD") { value = spx.vrad; return true; }
  if (spectralType == "VOPT" || spectralType == "FELO") { value = spx.vopt; return true; }
  if (spectralType == "ZOPT") { value = spx.zopt; return true; }
  if (spectralType == "VELO") { value = spx.velo; return true; }
  if (spectralType == "WAVN") { value = spx.wavn; return true; }
  if (spectralType == "ENER") { value = spx.ener; return true; }
  if (spectralType == "AFRQ") { value = spx.afrq; return true; }
  if (spectralType == "BETA") { value = spx.beta; return true; }
  return false;
}

//----------------------------------------------------------------------------
wcsprm* vtkAstroReprojector::MakeWCS(const CelestialGrid& celestial, const SpectralGrid* spectral, const wcsprm* like)
{
  const int naxis = spectral ? 3 : 2;
  wcsprm* wcs = static_cast<wcsprm*>(calloc(1, sizeof(wcsprm)));
  wcs->flag = -1;
  if (wcsini(1, naxis, wcs))
    {
    free(wcs);
    return nullptr;
    }
  const std::string frame = UpperCase(celestial.Frame);
  const std::string projection = UpperCase(celestial.Projection).substr(0, 3);
  std::string lon = "RA--", lat = "DEC-";
  if (frame == "GALACTIC")
    {
    lon = "GLON";
    lat = "GLAT";
    }
  else if (frame == "SUPERGALACTIC")
    {
    lon = "SLON";
    lat = "SLAT";
    }
  std::snprintf(wcs->ctype[0], 72, "%s-%s", lon.c_str(), projection.c_str());
  std::snprintf(wcs->ctype[1], 72, "%s-%s", lat.c_str(), projection.c_str());
  std::strcpy(wcs->cunit[0], "deg");
  std::strcpy(wcs->cunit[1], "deg");
  wcs->crval[0] = celestial.Center[0];
  wcs->crval[1] = celestial.Center[1];
  // centre of the image (FITS pixels from 1)
  wcs->crpix[0] = 0.5 * (celestial.Size[0] + 1);
  wcs->crpix[1] = 0.5 * (celestial.Size[1] + 1);
  // the longitude increases to the left
  wcs->cdelt[0] = -std::fabs(celestial.PixelSize[0]);
  wcs->cdelt[1] = std::fabs(celestial.PixelSize[1]);
  // rotation of the axes: PC matrix of CROTA2 (WCS paper II, eq. 189, with
  // the signed CDELT)
  const double rotation = celestial.Rotation * DEG2RAD;
  wcs->pc[0 * naxis + 0] = std::cos(rotation);
  wcs->pc[0 * naxis + 1] = -std::sin(rotation) * wcs->cdelt[1] / wcs->cdelt[0];
  wcs->pc[1 * naxis + 0] = std::sin(rotation) * wcs->cdelt[0] / wcs->cdelt[1];
  wcs->pc[1 * naxis + 1] = std::cos(rotation);
  wcs->altlin = 1;
  if (frame == "FK5" || frame == "FK4" || frame == "FK4-NO-E")
    {
    std::strcpy(wcs->radesys, frame.c_str());
    wcs->equinox = celestial.Equinox > 0. ? celestial.Equinox : (frame == "FK5" ? 2000. : 1950.);
    }
  else if (frame == "ICRS")
    {
    std::strcpy(wcs->radesys, "ICRS");
    }
  if (spectral)
    {
    const std::string type = UpperCase(spectral->Type).substr(0, 4);
    std::snprintf(wcs->ctype[2], 72, "%s", type.c_str());
    if (type == "FREQ")
      {
      std::strcpy(wcs->cunit[2], "Hz");
      }
    else if (type == "WAVE" || type == "AWAV")
      {
      std::strcpy(wcs->cunit[2], "m");
      }
    else
      {
      std::strcpy(wcs->cunit[2], "m/s");
      }
    wcs->crval[2] = spectral->Start;
    wcs->cdelt[2] = spectral->Step;
    wcs->crpix[2] = 1.;
    if (spectral->RestFrequency > 0.)
      {
      wcs->restfrq = spectral->RestFrequency;
      }
    std::snprintf(wcs->specsys, 72, "%s", UpperCase(spectral->SpecSys).c_str());
    }
  if (like)
    {
    wcs->mjdobs = like->mjdobs;
    wcs->mjdavg = like->mjdavg;
    std::snprintf(wcs->dateobs, 72, "%s", like->dateobs);
    for (int index = 0; index < 6; index++)
      {
      wcs->obsgeo[index] = like->obsgeo[index];
      }
    }
  if (wcsset(wcs))
    {
    vtkAstroReprojector::FreeWCS(wcs);
    return nullptr;
    }
  return wcs;
}

//----------------------------------------------------------------------------
void vtkAstroReprojector::FreeWCS(wcsprm* wcs)
{
  if (wcs)
    {
    wcsfree(wcs);
    free(wcs);
    }
}

//----------------------------------------------------------------------------
bool vtkAstroReprojector::GetCelestialGrid(const wcsprm* wcs, const int dimensions[2], CelestialGrid& grid)
{
  if (!wcs || wcs->lng < 0 || wcs->lat < 0)
    {
    return false;
    }
  WCSCopy copy(wcs);
  if (!copy.Valid)
    {
    return false;
    }
  double equinox = 0.;
  const int frame = CelestialFrameOf(wcs, equinox);
  grid.Frame = frame == vtkAstroFrames::Galactic ? "GALACTIC" :
               frame == vtkAstroFrames::Supergalactic ? "SUPERGALACTIC" :
               frame == vtkAstroFrames::FK5 ? "FK5" :
               frame == vtkAstroFrames::FK4 ? "FK4" :
               frame == vtkAstroFrames::FK4NoETerms ? "FK4-NO-E" : "ICRS";
  grid.Equinox = equinox;
  const std::string ctype = wcs->ctype[wcs->lng];
  grid.Projection = ctype.size() >= 8 ? UpperCase(ctype.substr(5, 3)) : "SIN";
  if (grid.Projection == "NCP")
    {
    grid.Projection = "SIN";
    }
  grid.Size[0] = dimensions[0];
  grid.Size[1] = dimensions[1];
  // centre, pixel sizes and rotation from the pixel <-> world mapping at the
  // centre of the image (central differences: the convergence of the
  // meridians cancels to first order)
  PixelMapper identity(wcs, wcs);
  const double cx = 0.5 * (dimensions[0] + 1), cy = 0.5 * (dimensions[1] + 1);
  const double pixels[10] = {cx, cy, cx - 0.5, cy, cx + 0.5, cy, cx, cy - 0.5, cx, cy + 0.5};
  double mapped[10], world[10];
  identity.Map(5, pixels, mapped, world);
  for (int point = 0; point < 5; point++)
    {
    if (std::isnan(world[2 * point]) || std::isnan(world[2 * point + 1]))
      {
      return false;
      }
    }
  grid.Center[0] = world[0];
  grid.Center[1] = world[1];
  const double cosLat = std::cos(world[1] * DEG2RAD);
  double dx = world[4] - world[2], dy = world[8] - world[6];
  dx -= 360. * std::round(dx / 360.);
  dy -= 360. * std::round(dy / 360.);
  // sky offsets (east, north) of one pixel along x and y
  const double xEast = dx * cosLat, xNorth = world[5] - world[3];
  const double yEast = dy * cosLat, yNorth = world[9] - world[7];
  grid.PixelSize[0] = std::sqrt(xEast * xEast + xNorth * xNorth);
  grid.PixelSize[1] = std::sqrt(yEast * yEast + yNorth * yNorth);
  // rotation (CROTA2): the +y axis is at -CROTA2 from north through east
  grid.Rotation = -std::atan2(yEast, yNorth) / DEG2RAD;
  if (std::fabs(grid.Rotation) < 1.e-10)
    {
    grid.Rotation = 0.;
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkAstroReprojector::GetSpectralGrid(const wcsprm* wcs, int channels, SpectralGrid& grid)
{
  if (!wcs || wcs->spec < 0 || channels < 1)
    {
    return false;
    }
  WCSCopy copy(wcs);
  if (!copy.Valid)
    {
    return false;
    }
  double first = 0., second = 0.;
  if (!SpectralWorld(&copy.WCS, 1., first) || !SpectralWorld(&copy.WCS, 2., second))
    {
    return false;
    }
  grid.Type = SpectralTypeOf(wcs);
  if (grid.Type == "FELO")
    {
    grid.Type = "VOPT";
    }
  grid.SpecSys = UpperCase(wcs->specsys);
  grid.RestFrequency = RestFrequencyOf(wcs);
  grid.Start = first;
  grid.Step = second - first;
  grid.Count = channels;
  return true;
}

namespace
{
//----------------------------------------------------------------------------
// Bessel function J1 (Abramowitz & Stegun 9.4.4 and 9.4.6, |error| < 1e-7):
// std::cyl_bessel_j is not available on every platform (libc++)
double BesselJ1(double x)
{
  const double ax = std::fabs(x);
  if (ax <= 3.)
    {
    const double y = (x / 3.) * (x / 3.);
    return x * (0.5 + y * (-0.56249985 + y * (0.21093573 + y * (-0.03954289 + y * (0.00443319 +
               y * (-0.00031761 + y * 0.00001109))))));
    }
  const double y = 3. / ax;
  const double f1 = 0.79788456 + y * (0.00000156 + y * (0.01659667 + y * (0.00017105 + y * (-0.00249511 +
                    y * (0.00113653 - y * 0.00020033)))));
  const double theta1 = ax - 2.35619449 + y * (0.12499612 + y * (0.00005650 + y * (-0.00637879 + y * (0.00074348 +
                        y * (0.00079824 - y * 0.00029166)))));
  const double result = f1 * std::cos(theta1) / std::sqrt(ax);
  return x < 0. ? -result : result;
}

//----------------------------------------------------------------------------
double AngularDistanceRadians(double lon1, double lat1, double lon2, double lat2)
{
  const double l1 = lon1 * DEG2RAD, b1 = lat1 * DEG2RAD, l2 = lon2 * DEG2RAD, b2 = lat2 * DEG2RAD;
  const double sinHalfLat = std::sin(0.5 * (b2 - b1)), sinHalfLon = std::sin(0.5 * (l2 - l1));
  const double h = sinHalfLat * sinHalfLat + std::cos(b1) * std::cos(b2) * sinHalfLon * sinHalfLon;
  return 2. * std::asin(std::min(1., std::sqrt(h)));
}

//----------------------------------------------------------------------------
// Catmull-Rom (Keys a = -0.5) weights of the offsets -1, 0, 1, 2: the
// bicubic interpolation of casacore (Interpolate2D cubic, central
// differences for the derivatives)
void CubicWeights(double t, double w[4])
{
  const double t2 = t * t, t3 = t2 * t;
  w[0] = 0.5 * (-t3 + 2. * t2 - t);
  w[1] = 0.5 * (3. * t3 - 5. * t2 + 2.);
  w[2] = 0.5 * (-3. * t3 + 4. * t2 + t);
  w[3] = 0.5 * (t3 - t2);
}

double Sinc(double x)
{
  if (x == 0.)
    {
    return 1.;
    }
  const double px = vtkMath::Pi() * x;
  return std::sin(px) / px;
}

//----------------------------------------------------------------------------
/// Interpolation of a plane (nx * ny, x fastest) at (x, y), voxel centres at
/// integers. Outside the pixels ([-0.5, n - 0.5]) or with a blank voxel in
/// the support of the kernel: NaN (as the masks of casacore). Near the
/// edges the cubic falls back to linear (casacore), Lanczos to cubic
/// (casacore returns 0).
double Interpolate(const double* plane, int nx, int ny, double x, double y, int method)
{
  if (!(x >= -0.5 && x <= nx - 0.5 && y >= -0.5 && y <= ny - 0.5))
    {
    return NaN;
    }
  // positions at a pixel within the rounding of the WCS transformations (e.g.
  // the same grid): the pixel value, no weight for the neighbours (a blank
  // neighbour must not blank it)
  const double xr = std::round(x), yr = std::round(y);
  if (std::fabs(x - xr) < 1.e-6)
    {
    x = xr;
    }
  if (std::fabs(y - yr) < 1.e-6)
    {
    y = yr;
    }
  if (x == xr && y == yr && xr >= 0. && yr >= 0. && xr <= nx - 1. && yr <= ny - 1.)
    {
    // all the kernels interpolate: the value of the pixel
    return plane[static_cast<std::size_t>(xr) + static_cast<std::size_t>(nx) * static_cast<std::size_t>(yr)];
    }
  const double xc = std::min(std::max(x, 0.), nx - 1.);
  const double yc = std::min(std::max(y, 0.), ny - 1.);
  if (method == vtkAstroReprojector::Nearest)
    {
    const int i = std::min(static_cast<int>(std::floor(xc + 0.5)), nx - 1);
    const int j = std::min(static_cast<int>(std::floor(yc + 0.5)), ny - 1);
    return plane[i + static_cast<std::size_t>(nx) * j];
    }
  const int i0 = std::max(0, std::min(static_cast<int>(std::floor(xc)), nx - 2));
  const int j0 = std::max(0, std::min(static_cast<int>(std::floor(yc)), ny - 2));
  const double tx = nx > 1 ? xc - i0 : 0.;
  const double ty = ny > 1 ? yc - j0 : 0.;
  if (method == vtkAstroReprojector::Lanczos3)
    {
    const int a = 3;
    const int fx = static_cast<int>(std::floor(xc)), fy = static_cast<int>(std::floor(yc));
    if (fx - a + 1 >= 0 && fx + a <= nx - 1 && fy - a + 1 >= 0 && fy + a <= ny - 1)
      {
      double sum = 0., weights = 0.;
      for (int j = fy - a + 1; j <= fy + a; j++)
        {
        const double wy = Sinc(yc - j) * Sinc((yc - j) / a);
        for (int i = fx - a + 1; i <= fx + a; i++)
          {
          const double w = wy * Sinc(xc - i) * Sinc((xc - i) / a);
          const double value = plane[i + static_cast<std::size_t>(nx) * j];
          if (w != 0. && std::isnan(value))
            {
            return NaN;
            }
          sum += w * value;
          weights += w;
          }
        }
      // (normalized: casacore does not normalize the Lanczos weights)
      return weights != 0. ? sum / weights : NaN;
      }
    method = vtkAstroReprojector::Bicubic;
    }
  if (method == vtkAstroReprojector::Bicubic && i0 >= 1 && i0 <= nx - 3 && j0 >= 1 && j0 <= ny - 3)
    {
    double wx[4], wy[4];
    CubicWeights(tx, wx);
    CubicWeights(ty, wy);
    double sum = 0.;
    for (int b = 0; b < 4; b++)
      {
      const double* row = plane + static_cast<std::size_t>(nx) * (j0 - 1 + b) + (i0 - 1);
      for (int a = 0; a < 4; a++)
        {
        const double w = wx[a] * wy[b];
        if (w == 0.)
          {
          continue; // (e.g. at a pixel: a blank neighbour does not blank it)
          }
        if (std::isnan(row[a]))
          {
          return NaN;
          }
        sum += w * row[a];
        }
      }
    return sum;
    }
  // bilinear (also the edges of the cubic and of degenerate axes)
  const int i1 = nx > 1 ? i0 + 1 : i0, j1 = ny > 1 ? j0 + 1 : j0;
  const double v00 = plane[i0 + static_cast<std::size_t>(nx) * j0];
  const double v10 = plane[i1 + static_cast<std::size_t>(nx) * j0];
  const double v01 = plane[i0 + static_cast<std::size_t>(nx) * j1];
  const double v11 = plane[i1 + static_cast<std::size_t>(nx) * j1];
  const double w00 = (1. - tx) * (1. - ty), w10 = tx * (1. - ty), w01 = (1. - tx) * ty, w11 = tx * ty;
  if ((w00 != 0. && std::isnan(v00)) || (w10 != 0. && std::isnan(v10)) ||
      (w01 != 0. && std::isnan(v01)) || (w11 != 0. && std::isnan(v11)))
    {
    return NaN;
    }
  return (w00 != 0. ? w00 * v00 : 0.) + (w10 != 0. ? w10 * v10 : 0.) + (w01 != 0. ? w01 * v01 : 0.) +
         (w11 != 0. ? w11 * v11 : 0.);
}

//----------------------------------------------------------------------------
/// ICRS direction (degrees) of the reference point of a WCS
bool ReferenceDirectionICRS(const wcsprm* wcs, double direction[2])
{
  if (!wcs || wcs->lng < 0)
    {
    return false;
    }
  double equinox = 0.;
  const int frame = CelestialFrameOf(wcs, equinox);
  if (frame == vtkAstroFrames::CelestialUnknown)
    {
    direction[0] = wcs->crval[wcs->lng];
    direction[1] = wcs->crval[wcs->lat];
    return true;
    }
  return vtkAstroFrames::ConvertCelestial(wcs->crval[wcs->lng], wcs->crval[wcs->lat], frame, equinox,
                                          vtkAstroFrames::ICRS, 0., ObservationMJD(wcs), direction);
}

//----------------------------------------------------------------------------
/// Frequency in the spectral frame of to of a frequency measured in the
/// frame of from, in the direction (ICRS) of the source. Unknown frames
/// (no SPECSYS): no conversion (noted in sameFrame = false).
bool ConvertSpectralFrame(double frequency, const wcsprm* from, const wcsprm* to, const double direction[2],
                          double& result, std::string& warning)
{
  const int fromFrame = vtkAstroFrames::GetSpectralFrame(from->specsys);
  const int toFrame = vtkAstroFrames::GetSpectralFrame(to->specsys);
  if (fromFrame == vtkAstroFrames::SpectralUnknown || toFrame == vtkAstroFrames::SpectralUnknown ||
      fromFrame == toFrame)
    {
    if (fromFrame != toFrame && warning.empty())
      {
      warning = "spectral reference frame (SPECSYS) unknown or not supported: no frame conversion";
      }
    result = frequency;
    return true;
    }
  // to the barycentre with the time and place of the observation of from,
  // then to the frame of to with those of to
  double barycentric = 0.;
  if (!vtkAstroFrames::ConvertFrequency(frequency, fromFrame, vtkAstroFrames::Barycentric, direction[0],
                                        direction[1], ObservationMJD(from), from->obsgeo, barycentric) ||
      !vtkAstroFrames::ConvertFrequency(barycentric, vtkAstroFrames::Barycentric, toFrame, direction[0],
                                        direction[1], ObservationMJD(to), to->obsgeo, result))
    {
    warning = "topocentric or geocentric spectral frame without the date (MJD-OBS) or the position "
              "(OBSGEO) of the observation";
    return false;
    }
  return true;
}

//----------------------------------------------------------------------------
/// Channel of from (FITS pixel coordinate) -> channel of to (FITS pixel
/// coordinate), at the reference positions: spectral value -> frequency
/// (or velocity) -> frame conversion -> spectral value of to
class SpectralMapper
{
public:
  SpectralMapper(const wcsprm* from, const wcsprm* to, int matching, const double direction[2])
    : FromCopy(from), ToCopy(to), Matching(matching)
    {
    this->Direction[0] = direction[0];
    this->Direction[1] = direction[1];
    this->FromType = SpectralTypeOf(from);
    this->ToType = SpectralTypeOf(to);
    this->FromRest = RestFrequencyOf(from);
    this->ToRest = RestFrequencyOf(to);
    this->VelocityType = IsVelocityType(this->FromType) ? (this->FromType == "FELO" ? "VOPT" : this->FromType) : "VRAD";
    this->Valid = this->FromCopy.Valid && this->ToCopy.Valid && from->spec >= 0 && to->spec >= 0;
    if (this->Valid && matching == vtkAstroReprojector::MatchVelocity && (!(this->FromRest > 0.) || !(this->ToRest > 0.)))
      {
      this->Error = "matching by velocity needs the rest frequencies of the data";
      this->Valid = false;
      }
    }

  bool Map(double fromPixel, double& toPixel)
    {
    if (!this->Valid)
      {
      return false;
      }
    double value = 0., frequency = 0.;
    if (!SpectralWorld(&this->FromCopy.WCS, fromPixel, value) ||
        !vtkAstroReprojector::SpectralValueToFrequency(this->FromType.c_str(), value, this->FromRest, frequency))
      {
      return false;
      }
    double converted = 0.;
    if (!ConvertSpectralFrame(frequency, &this->FromCopy.WCS, &this->ToCopy.WCS, this->Direction, converted,
                              this->Warning))
      {
      return false;
      }
    double toFrequency = converted;
    if (this->Matching == vtkAstroReprojector::MatchVelocity)
      {
      // same velocity (convention of from), each with its rest frequency
      double velocity = 0.;
      if (!vtkAstroReprojector::FrequencyToSpectralValue(this->VelocityType.c_str(), converted, this->FromRest, velocity) ||
          !vtkAstroReprojector::SpectralValueToFrequency(this->VelocityType.c_str(), velocity, this->ToRest, toFrequency))
        {
        return false;
        }
      }
    double toValue = 0.;
    if (!vtkAstroReprojector::FrequencyToSpectralValue(this->ToType.c_str(), toFrequency, this->ToRest, toValue))
      {
      return false;
      }
    return SpectralPixel(&this->ToCopy.WCS, toValue, toPixel);
    }

  WCSCopy FromCopy;
  WCSCopy ToCopy;
  int Matching;
  double Direction[2];
  std::string FromType, ToType, VelocityType;
  double FromRest = 0., ToRest = 0.;
  bool Valid = false;
  std::string Error;
  std::string Warning;
};

//----------------------------------------------------------------------------
typedef std::vector<std::pair<int, double> > ChannelWeights;

/// Weights of the input channels (from 0) of each output channel
bool MakeSpectralWeights(const wcsprm* outputWCS, int outputChannels, const wcsprm* inputWCS, int inputChannels,
                         int method, int matching, const double direction[2], std::vector<ChannelWeights>& weights,
                         std::vector<char>& overlapChannels, std::string& error, std::string& warning)
{
  weights.assign(outputChannels, ChannelWeights());
  overlapChannels.assign(outputChannels, 0);
  const bool inputSpectral = inputWCS->spec >= 0 && inputWCS->naxis > 2;
  const bool outputSpectral = outputWCS->spec >= 0 && outputWCS->naxis > 2;
  if (!inputSpectral || inputChannels <= 1)
    {
    if (inputChannels > 1)
      {
      error = "the data have several channels but no spectral axis";
      return false;
      }
    for (int channel = 0; channel < outputChannels; channel++)
      {
      weights[channel].push_back(std::make_pair(0, 1.));
      }
    return true;
    }
  if (!outputSpectral)
    {
    error = "the output grid has no spectral axis for the data cube";
    return false;
    }
  SpectralMapper mapper(outputWCS, inputWCS, matching, direction);
  if (!mapper.Valid)
    {
    error = mapper.Error.empty() ? "spectral axes not supported" : mapper.Error;
    return false;
    }
  for (int channel = 0; channel < outputChannels; channel++)
    {
    double centre = 0., low = 0., high = 0.;
    // FITS pixel coordinates (from 1) -> input channels (from 0)
    if (!mapper.Map(channel + 1., centre) || !mapper.Map(channel + 0.5, low) || !mapper.Map(channel + 1.5, high))
      {
      continue;
      }
    centre -= 1.;
    low -= 1.;
    high -= 1.;
    if (low > high)
      {
      std::swap(low, high);
      }
    int channelMethod = method;
    if (method == vtkAstroReprojector::SpectralAuto)
      {
      channelMethod = (high - low) > 1. + 1.e-6 ? vtkAstroReprojector::SpectralOverlap
                                                 : vtkAstroReprojector::SpectralLinear;
      }
    ChannelWeights& channelWeights = weights[channel];
    if (channelMethod == vtkAstroReprojector::SpectralOverlap)
      {
      overlapChannels[channel] = 1;
      const double width = high - low;
      double covered = 0.;
      const int first = std::max(0, static_cast<int>(std::floor(low + 0.5)));
      const int last = std::min(inputChannels - 1, static_cast<int>(std::floor(high + 0.5)));
      for (int input = first; input <= last; input++)
        {
        const double overlap = std::min(high, input + 0.5) - std::max(low, input - 0.5);
        if (overlap > 0.)
          {
          channelWeights.push_back(std::make_pair(input, overlap));
          covered += overlap;
          }
        }
      if (!(width > 0.) || covered < 0.5 * width)
        {
        channelWeights.clear();
        continue;
        }
      for (std::size_t index = 0; index < channelWeights.size(); index++)
        {
        channelWeights[index].second /= covered;
        }
      continue;
      }
    if (!(centre >= -0.5 && centre <= inputChannels - 0.5))
      {
      continue;
      }
    const double c = std::min(std::max(centre, 0.), inputChannels - 1.);
    if (channelMethod == vtkAstroReprojector::SpectralNearest)
      {
      channelWeights.push_back(std::make_pair(std::min(static_cast<int>(std::floor(c + 0.5)), inputChannels - 1), 1.));
      continue;
      }
    const int k0 = std::max(0, std::min(static_cast<int>(std::floor(c)), inputChannels - 2));
    const double t = inputChannels > 1 ? c - k0 : 0.;
    if (channelMethod == vtkAstroReprojector::SpectralCubic && k0 >= 1 && k0 <= inputChannels - 3)
      {
      double w[4];
      CubicWeights(t, w);
      for (int offset = 0; offset < 4; offset++)
        {
        if (w[offset] != 0.)
          {
          channelWeights.push_back(std::make_pair(k0 - 1 + offset, w[offset]));
          }
        }
      continue;
      }
    if (1. - t != 0.)
      {
      channelWeights.push_back(std::make_pair(k0, 1. - t));
      }
    if (t != 0.)
      {
      channelWeights.push_back(std::make_pair(k0 + 1, t));
      }
    }
  warning = mapper.Warning;
  return true;
}

//----------------------------------------------------------------------------
/// Input data (float or double) as planes of doubles
double VoxelValue(vtkImageData* data, std::size_t index)
{
  const int type = data->GetScalarType();
  if (type == VTK_FLOAT)
    {
    return static_cast<float*>(data->GetScalarPointer())[index];
    }
  if (type == VTK_DOUBLE)
    {
    return static_cast<double*>(data->GetScalarPointer())[index];
    }
  return data->GetPointData()->GetScalars()->GetComponent(static_cast<vtkIdType>(index), 0);
}

bool ContainsCaseInsensitive(const std::string& text, const char* part)
{
  return UpperCase(text).find(part) != std::string::npos;
}

//----------------------------------------------------------------------------
/// Sky offsets -> pixel offsets at the reference pixel of a WCS: M (2x2,
/// pixels per degree; columns: east, north), pixel solid angle (deg^2)
bool PixelJacobian(const wcsprm* wcs, double M[2][2], double& pixelArea)
{
  WCSCopy copy(wcs);
  if (!copy.Valid)
    {
    return false;
    }
  wcsprm* w = &copy.WCS;
  const int n = w->naxis;
  std::vector<double> world(3 * n), pixel(3 * n), imgcrd(3 * n);
  std::vector<double> phi(3), theta(3);
  std::vector<int> stat(3);
  const double lon = w->crval[w->lng], lat = w->crval[w->lat];
  const double step = std::max(1.e-7, std::fabs(w->cdelt[w->lng]) * 0.5);
  for (int point = 0; point < 3; point++)
    {
    for (int axis = 0; axis < n; axis++)
      {
      world[point * n + axis] = w->crval[axis];
      }
    }
  world[1 * n + w->lng] = lon + step / std::cos(lat * DEG2RAD);
  world[2 * n + w->lat] = lat + step;
  if (wcss2p(w, 3, n, world.data(), phi.data(), theta.data(), imgcrd.data(), pixel.data(), stat.data()))
    {
    return false;
    }
  M[0][0] = (pixel[1 * n + w->lng] - pixel[w->lng]) / step;
  M[1][0] = (pixel[1 * n + w->lat] - pixel[w->lat]) / step;
  M[0][1] = (pixel[2 * n + w->lng] - pixel[w->lng]) / step;
  M[1][1] = (pixel[2 * n + w->lat] - pixel[w->lat]) / step;
  const double determinant = std::fabs(M[0][0] * M[1][1] - M[0][1] * M[1][0]);
  if (!(determinant > 0.))
    {
    return false;
    }
  pixelArea = 1. / determinant;
  return true;
}

//----------------------------------------------------------------------------
/// Smallest size >= n with the prime factors 2, 3 and 5 (fast FFT)
std::size_t NextFastFFTSize(std::size_t n)
{
  const std::size_t factors[3] = {2, 3, 5};
  for (std::size_t m = std::max<std::size_t>(n, 1);; m++)
    {
    std::size_t rest = m;
    for (std::size_t factor : factors)
      {
      while (rest % factor == 0)
        {
        rest /= factor;
        }
      }
    if (rest == 1)
      {
      return m;
      }
    }
}

//----------------------------------------------------------------------------
/// In-place 2D FFT of a complex array of px x py values (rows of px). Only
/// the first rows rows are nonzero (forward) or needed (inverse). The inverse
/// is not normalized (divide by px * py).
void FFT2D(std::vector<vtkFFT::ComplexNumber>& values, std::size_t px, std::size_t py, std::size_t rows,
           bool inverse)
{
  std::function<void(vtkIdType, vtkIdType)> rowPass = [&](vtkIdType first, vtkIdType last)
    {
    std::vector<vtkFFT::ComplexNumber> buffer(px);
    for (vtkIdType j = first; j < last; j++)
      {
      vtkFFT::ComplexNumber* row = values.data() + static_cast<std::size_t>(j) * px;
      vtkFFT::Fft(row, px, buffer.data());
      std::copy(buffer.begin(), buffer.end(), row);
      }
    };
  std::function<void(vtkIdType, vtkIdType)> columnPass = [&](vtkIdType first, vtkIdType last)
    {
    std::vector<vtkFFT::ComplexNumber> column(py), buffer(py);
    for (vtkIdType i = first; i < last; i++)
      {
      for (std::size_t j = 0; j < py; j++)
        {
        column[j] = values[static_cast<std::size_t>(i) + j * px];
        }
      vtkFFT::Fft(column.data(), py, buffer.data());
      for (std::size_t j = 0; j < py; j++)
        {
        values[static_cast<std::size_t>(i) + j * px] = buffer[j];
        }
      }
    };
  std::function<void(std::size_t)> conjugate = [&](std::size_t count)
    {
    for (std::size_t index = 0; index < count; index++)
      {
      values[index].i = -values[index].i;
      }
    };
  if (!inverse)
    {
    vtkSMPTools::For(0, static_cast<vtkIdType>(rows), rowPass);
    vtkSMPTools::For(0, static_cast<vtkIdType>(px), columnPass);
    return;
    }
  // inverse FFT = conjugate of the FFT of the conjugate
  conjugate(values.size());
  vtkSMPTools::For(0, static_cast<vtkIdType>(px), columnPass);
  vtkSMPTools::For(0, static_cast<vtkIdType>(rows), rowPass);
  conjugate(rows * px);
}

//----------------------------------------------------------------------------
/// Convolve an input to a target beam (CASA Image2DConvolver targetres):
/// a new float image, or the input if no convolution is needed
bool ConvolveToBeam(const vtkAstroReprojector::Input& input, const double target[3],
                    vtkSmartPointer<vtkImageData>& result, std::string& bunit, std::string& error,
                    const std::atomic<bool>* cancel)
{
  result = input.Data;
  bunit = input.BUnit;
  const bool perPixel = ContainsCaseInsensitive(input.BUnit, "/PIXEL");
  const bool perBeam = ContainsCaseInsensitive(input.BUnit, "/BEAM");
  double kernel[3] = {0., 0., 0.};
  if (perPixel)
    {
    // Jy/pixel: the kernel is the target beam (unit peak), Jy/beam result
    std::copy(target, target + 3, kernel);
    bunit = "JY/BEAM";
    }
  else
    {
    if (!(input.Beam[0] > 0.) || !(input.Beam[1] > 0.))
      {
      if (perBeam)
        {
        error = input.Name + ": the beam (BMAJ, BMIN, BPA) is needed to convolve the data to the target resolution";
        return false;
        }
      std::copy(target, target + 3, kernel);
      }
    else if (!vtkAstroBeams::ConvolvingBeam(target, input.Beam, kernel))
      {
      std::ostringstream text;
      text << input.Name << ": the target resolution (" << target[0] * 3600. << "\" x " << target[1] * 3600.
           << "\") is smaller than the beam of the data (" << input.Beam[0] * 3600. << "\" x "
           << input.Beam[1] * 3600. << "\") in some direction";
      error = text.str();
      return false;
      }
    }
  if (!(kernel[0] > 0.) || !(kernel[1] > 0.))
    {
    return true; // already at the target resolution
    }

  double M[2][2], pixelArea = 0.;
  if (!PixelJacobian(input.WCS, M, pixelArea))
    {
    error = input.Name + ": the pixel size could not be computed";
    return false;
    }
  // covariance of the kernel on the sky (east, north; deg^2), in pixels
  const double fwhmToSigma = 1. / (2. * std::sqrt(2. * std::log(2.)));
  const double sMajor = kernel[0] * fwhmToSigma, sMinor = kernel[1] * fwhmToSigma, pa = kernel[2] * DEG2RAD;
  const double major[2] = {std::sin(pa), std::cos(pa)}, minor[2] = {std::cos(pa), -std::sin(pa)};
  double sky[2][2];
  for (int r = 0; r < 2; r++)
    {
    for (int c = 0; c < 2; c++)
      {
      sky[r][c] = sMajor * sMajor * major[r] * major[c] + sMinor * sMinor * minor[r] * minor[c];
      }
    }
  double pix[2][2];
  for (int r = 0; r < 2; r++)
    {
    for (int c = 0; c < 2; c++)
      {
      pix[r][c] = 0.;
      for (int a = 0; a < 2; a++)
        {
        for (int b = 0; b < 2; b++)
          {
          pix[r][c] += M[r][a] * sky[a][b] * M[c][b];
          }
        }
      }
    }
  const double determinant = pix[0][0] * pix[1][1] - pix[0][1] * pix[1][0];
  if (!(determinant > 0.))
    {
    error = input.Name + ": invalid convolution kernel";
    return false;
    }
  const double inverse[2][2] = {{pix[1][1] / determinant, -pix[0][1] / determinant},
                                {-pix[1][0] / determinant, pix[0][0] / determinant}};
  const double largest = 0.5 * (pix[0][0] + pix[1][1]) +
                         std::sqrt(0.25 * (pix[0][0] - pix[1][1]) * (pix[0][0] - pix[1][1]) + pix[0][1] * pix[0][1]);
  const int half = std::min(256, static_cast<int>(std::ceil(4. * std::sqrt(largest))));
  const int size = 2 * half + 1;
  std::vector<double> weights(static_cast<std::size_t>(size) * size);
  double volume = 0.;
  for (int b = -half; b <= half; b++)
    {
    for (int a = -half; a <= half; a++)
      {
      const double q = inverse[0][0] * a * a + 2. * inverse[0][1] * a * b + inverse[1][1] * b * b;
      const double w = std::exp(-0.5 * q);
      weights[(a + half) + static_cast<std::size_t>(size) * (b + half)] = w;
      volume += w;
      }
    }
  // scale factor: Jy/beam keeps the peak of point sources (CASA: pixel area /
  // kernel area * target area / input area; the sum of the sampled kernel is
  // used for kernel area / pixel area, which is also right for kernels
  // smaller than a pixel, e.g. a target beam close to the beam of the data),
  // Jy/pixel: unit peak kernel (Jy/beam result), other units: unit volume
  double scale = 1. / volume;
  if (perPixel)
    {
    scale = 1.;
    }
  else if (perBeam)
    {
    scale = vtkAstroBeams::Area(target[0], target[1]) / vtkAstroBeams::Area(input.Beam[0], input.Beam[1]) / volume;
    }

  int dims[3];
  input.Data->GetDimensions(dims);
  vtkNew<vtkImageData> output;
  output->SetDimensions(dims);
  output->AllocateScalars(VTK_FLOAT, 1);
  float* out = static_cast<float*>(output->GetScalarPointer());
  const std::size_t nx = static_cast<std::size_t>(dims[0]), ny = static_cast<std::size_t>(dims[1]);
  const std::size_t planeSize = nx * ny;
  vtkImageData* data = input.Data;

  // Convolution of the data and of the mask of the valid voxels (normalized
  // convolution, blanks and edges): with the 2D kernel (small kernels), with
  // two 1D kernels if the axes of the kernel are along the pixel axes (the
  // kernel is exactly their product), with the FFT for large rotated kernels
  // (the kernel is even: its FFT is real; data and mask are the real and
  // imaginary parts of one complex FFT).
  const bool separable = std::fabs(pix[0][1]) <= 1.e-12 * std::max(pix[0][0], pix[1][1]);
  const bool useFFT = !separable && size > 15;
  std::vector<double> kernelX, kernelY;
  if (separable)
    {
    kernelX.resize(size);
    kernelY.resize(size);
    for (int a = -half; a <= half; a++)
      {
      kernelX[a + half] = std::exp(-0.5 * inverse[0][0] * a * a);
      kernelY[a + half] = std::exp(-0.5 * inverse[1][1] * a * a);
      }
    }
  std::size_t px = 0, py = 0;
  std::vector<double> kernelFFT;
  std::vector<vtkFFT::ComplexNumber> planeFFT;
  if (useFFT)
    {
    // padding: no wrap-around of the kernel into the data
    px = NextFastFFTSize(nx + half);
    py = NextFastFFTSize(ny + half);
    std::vector<vtkFFT::ComplexNumber> kernel(px * py);
    for (vtkFFT::ComplexNumber& value : kernel)
      {
      value.r = 0.;
      value.i = 0.;
      }
    for (int b = -half; b <= half; b++)
      {
      for (int a = -half; a <= half; a++)
        {
        const std::size_t x = static_cast<std::size_t>((a + static_cast<long long>(px)) % static_cast<long long>(px));
        const std::size_t y = static_cast<std::size_t>((b + static_cast<long long>(py)) % static_cast<long long>(py));
        kernel[x + px * y].r = weights[(a + half) + static_cast<std::size_t>(size) * (b + half)];
        }
      }
    FFT2D(kernel, px, py, py, false);
    kernelFFT.resize(px * py);
    for (std::size_t index = 0; index < px * py; index++)
      {
      kernelFFT[index] = kernel[index].r;
      }
    planeFFT.resize(px * py);
    }
  std::vector<double> plane(planeSize), sums(planeSize), valids(planeSize), rowSums, rowValids;
  if (separable)
    {
    rowSums.resize(planeSize);
    rowValids.resize(planeSize);
    }

  for (int k = 0; k < dims[2]; k++)
    {
    if (cancel && *cancel)
      {
      return false;
      }
    for (std::size_t index = 0; index < planeSize; index++)
      {
      plane[index] = VoxelValue(data, planeSize * k + index);
      }
    if (separable)
      {
      // along x, then along y
      vtkSMPTools::For(0, static_cast<vtkIdType>(ny), [&](vtkIdType firstRow, vtkIdType lastRow)
        {
        for (vtkIdType j = firstRow; j < lastRow; j++)
          {
          for (std::size_t i = 0; i < nx; i++)
            {
            double sum = 0., valid = 0.;
            for (int a = -half; a <= half; a++)
              {
              const long long x = static_cast<long long>(i) - a;
              if (x < 0 || x >= static_cast<long long>(nx))
                {
                continue;
                }
              const double value = plane[static_cast<std::size_t>(x) + nx * j];
              if (std::isnan(value))
                {
                continue;
                }
              sum += kernelX[a + half] * value;
              valid += kernelX[a + half];
              }
            rowSums[i + nx * j] = sum;
            rowValids[i + nx * j] = valid;
            }
          }
        });
      vtkSMPTools::For(0, static_cast<vtkIdType>(ny), [&](vtkIdType firstRow, vtkIdType lastRow)
        {
        for (vtkIdType j = firstRow; j < lastRow; j++)
          {
          for (std::size_t i = 0; i < nx; i++)
            {
            double sum = 0., valid = 0.;
            for (int b = -half; b <= half; b++)
              {
              const long long y = static_cast<long long>(j) - b;
              if (y < 0 || y >= static_cast<long long>(ny))
                {
                continue;
                }
              sum += kernelY[b + half] * rowSums[i + nx * static_cast<std::size_t>(y)];
              valid += kernelY[b + half] * rowValids[i + nx * static_cast<std::size_t>(y)];
              }
            sums[i + nx * j] = sum;
            valids[i + nx * j] = valid;
            }
          }
        });
      }
    else if (useFFT)
      {
      for (vtkFFT::ComplexNumber& value : planeFFT)
        {
        value.r = 0.;
        value.i = 0.;
        }
      for (std::size_t j = 0; j < ny; j++)
        {
        for (std::size_t i = 0; i < nx; i++)
          {
          const double value = plane[i + nx * j];
          if (!std::isnan(value))
            {
            planeFFT[i + px * j].r = value;
            planeFFT[i + px * j].i = 1.;
            }
          }
        }
      FFT2D(planeFFT, px, py, ny, false);
      for (std::size_t index = 0; index < px * py; index++)
        {
        planeFFT[index].r *= kernelFFT[index];
        planeFFT[index].i *= kernelFFT[index];
        }
      FFT2D(planeFFT, px, py, ny, true);
      const double normalization = 1. / static_cast<double>(px * py);
      for (std::size_t j = 0; j < ny; j++)
        {
        for (std::size_t i = 0; i < nx; i++)
          {
          sums[i + nx * j] = planeFFT[i + px * j].r * normalization;
          valids[i + nx * j] = planeFFT[i + px * j].i * normalization;
          }
        }
      }
    else
      {
      vtkSMPTools::For(0, static_cast<vtkIdType>(ny), [&](vtkIdType firstRow, vtkIdType lastRow)
        {
        for (vtkIdType j = firstRow; j < lastRow; j++)
          {
          for (std::size_t i = 0; i < nx; i++)
            {
            double sum = 0., valid = 0.;
            for (int b = -half; b <= half; b++)
              {
              const long long y = static_cast<long long>(j) - b;
              if (y < 0 || y >= static_cast<long long>(ny))
                {
                continue;
                }
              for (int a = -half; a <= half; a++)
                {
                const long long x = static_cast<long long>(i) - a;
                if (x < 0 || x >= static_cast<long long>(nx))
                  {
                  continue;
                  }
                const double value = plane[static_cast<std::size_t>(x) + nx * static_cast<std::size_t>(y)];
                if (std::isnan(value))
                  {
                  continue;
                  }
                const double w = weights[(a + half) + static_cast<std::size_t>(size) * (b + half)];
                sum += w * value;
                valid += w;
                }
              }
            sums[i + nx * j] = sum;
            valids[i + nx * j] = valid;
            }
          }
        });
      }
    // blanks stay blank; elsewhere at least half of the kernel on valid data
    for (std::size_t index = 0; index < planeSize; index++)
      {
      const double valid = valids[index];
      out[planeSize * k + index] = (!std::isnan(plane[index]) && valid >= 0.5 * volume) ?
        static_cast<float>(scale * sums[index] * volume / valid) : std::numeric_limits<float>::quiet_NaN();
      }
    }
  result = output;
  return true;
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
bool vtkAstroReprojector::FitCelestialGrid(const std::vector<Input>& inputs, bool unionOfInputs, double pixelSize,
                                           CelestialGrid& grid, std::string& error)
{
  if (inputs.empty())
    {
    error = "no input data";
    return false;
    }
  // boundary of each input (every few pixels along the sides)
  std::vector<std::vector<double> > boundaries(inputs.size());
  double smallestPixel = std::numeric_limits<double>::max();
  for (std::size_t index = 0; index < inputs.size(); index++)
    {
    const Input& input = inputs[index];
    if (!input.Data || !input.WCS)
      {
      error = "input without data or WCS";
      return false;
      }
    int dims[3];
    input.Data->GetDimensions(dims);
    CelestialGrid inputGrid;
    if (!vtkAstroReprojector::GetCelestialGrid(input.WCS, dims, inputGrid))
      {
      error = input.Name + ": no celestial coordinates";
      return false;
      }
    smallestPixel = std::min(smallestPixel, std::min(inputGrid.PixelSize[0], inputGrid.PixelSize[1]));
    std::vector<double>& boundary = boundaries[index];
    const int stepX = std::max(1, dims[0] / 64), stepY = std::max(1, dims[1] / 64);
    for (int i = 0; i <= dims[0]; i += stepX)
      {
      boundary.insert(boundary.end(), {i + 0.5, 0.5, i + 0.5, dims[1] + 0.5});
      }
    for (int j = 0; j <= dims[1]; j += stepY)
      {
      boundary.insert(boundary.end(), {0.5, j + 0.5, dims[0] + 0.5, j + 0.5});
      }
    boundary.insert(boundary.end(), {dims[0] + 0.5, dims[1] + 0.5});
    }
  if (!(pixelSize > 0.))
    {
    pixelSize = smallestPixel;
    }
  grid.PixelSize[0] = grid.PixelSize[1] = pixelSize;

  // centre: mean direction of the boundaries, in the frame of the grid
  grid.Size[0] = grid.Size[1] = 1;
  grid.Center[0] = grid.Center[1] = 0.;
  {
  wcsprm* frameWCS = vtkAstroReprojector::MakeWCS(grid, nullptr, inputs[0].WCS);
  if (!frameWCS)
    {
    error = "invalid output grid (projection or frame)";
    return false;
    }
  double sum[3] = {0., 0., 0.};
  for (std::size_t index = 0; index < inputs.size(); index++)
    {
    PixelMapper mapper(inputs[index].WCS, frameWCS);
    const int n = static_cast<int>(boundaries[index].size() / 2);
    std::vector<double> mapped(2 * n), converted(2 * n);
    mapper.Map(n, boundaries[index].data(), mapped.data(), nullptr, converted.data());
    for (int point = 0; point < n; point++)
      {
      if (std::isnan(converted[2 * point]))
        {
        continue;
        }
      const double l = converted[2 * point] * DEG2RAD, b = converted[2 * point + 1] * DEG2RAD;
      sum[0] += std::cos(b) * std::cos(l);
      sum[1] += std::cos(b) * std::sin(l);
      sum[2] += std::sin(b);
      }
    }
  vtkAstroReprojector::FreeWCS(frameWCS);
  const double norm = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
  if (!(norm > 0.))
    {
    error = "the inputs could not be placed on the output grid";
    return false;
    }
  grid.Center[0] = std::atan2(sum[1], sum[0]) / DEG2RAD;
  if (grid.Center[0] < 0.)
    {
    grid.Center[0] += 360.;
    }
  grid.Center[1] = std::asin(sum[2] / norm) / DEG2RAD;
  }

  // extent of the inputs on the grid (twice: the centre moves)
  for (int iteration = 0; iteration < 2; iteration++)
    {
    wcsprm* gridWCS = vtkAstroReprojector::MakeWCS(grid, nullptr, inputs[0].WCS);
    if (!gridWCS)
      {
      error = "invalid output grid (projection or frame)";
      return false;
      }
    double extent[4] = {std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(),
                        std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()};
    bool first = true;
    for (std::size_t index = 0; index < inputs.size(); index++)
      {
      PixelMapper mapper(inputs[index].WCS, gridWCS);
      const int n = static_cast<int>(boundaries[index].size() / 2);
      std::vector<double> mapped(2 * n);
      mapper.Map(n, boundaries[index].data(), mapped.data());
      double box[4] = {std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(),
                       std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()};
      for (int point = 0; point < n; point++)
        {
        if (std::isnan(mapped[2 * point]))
          {
          continue;
          }
        box[0] = std::min(box[0], mapped[2 * point]);
        box[1] = std::max(box[1], mapped[2 * point]);
        box[2] = std::min(box[2], mapped[2 * point + 1]);
        box[3] = std::max(box[3], mapped[2 * point + 1]);
        }
      if (box[0] > box[1])
        {
        continue;
        }
      if (first || unionOfInputs)
        {
        extent[0] = std::min(extent[0], box[0]);
        extent[1] = std::max(extent[1], box[1]);
        extent[2] = std::min(extent[2], box[2]);
        extent[3] = std::max(extent[3], box[3]);
        if (first && !unionOfInputs)
          {
          std::copy(box, box + 4, extent);
          }
        }
      else
        {
        extent[0] = std::max(extent[0], box[0]);
        extent[1] = std::min(extent[1], box[1]);
        extent[2] = std::max(extent[2], box[2]);
        extent[3] = std::min(extent[3], box[3]);
        }
      first = false;
      }
    if (first || extent[0] >= extent[1] || extent[2] >= extent[3])
      {
      vtkAstroReprojector::FreeWCS(gridWCS);
      error = unionOfInputs ? "the inputs could not be placed on the output grid" : "the inputs do not overlap";
      return false;
      }
    // FITS pixel coordinates of the corners of the pixels: size in pixels,
    // centre of the extent
    const int sizeX = std::max(1, static_cast<int>(std::ceil(extent[1] - extent[0] - 1e-6)));
    const int sizeY = std::max(1, static_cast<int>(std::ceil(extent[3] - extent[2] - 1e-6)));
    const double centre[2] = {0.5 * (extent[0] + extent[1]), 0.5 * (extent[2] + extent[3])};
    PixelMapper identity(gridWCS, gridWCS);
    double mapped[2], world[2];
    identity.Map(1, centre, mapped, world);
    vtkAstroReprojector::FreeWCS(gridWCS);
    if (std::isnan(world[0]))
      {
      error = "the centre of the output grid is not defined in the projection";
      return false;
      }
    grid.Center[0] = world[0];
    grid.Center[1] = world[1];
    grid.Size[0] = sizeX;
    grid.Size[1] = sizeY;
    if (static_cast<double>(sizeX) * sizeY > 1.e9)
      {
      error = "the output grid is too large (more than 10^9 pixels per channel)";
      return false;
      }
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkAstroReprojector::FitSpectralGrid(const std::vector<Input>& inputs, bool unionOfInputs, double step,
                                          SpectralGrid& grid, std::string& error)
{
  // reference WCS of the grid (type, frame, rest frequency): values of the
  // channels of the inputs in it
  CelestialGrid celestial;
  int dims0[3] = {1, 1, 1};
  if (!inputs.empty() && inputs[0].Data)
    {
    inputs[0].Data->GetDimensions(dims0);
    }
  if (inputs.empty() || !inputs[0].Data || !vtkAstroReprojector::GetCelestialGrid(inputs[0].WCS, dims0, celestial))
    {
    error = "no input data with celestial coordinates";
    return false;
    }
  SpectralGrid unit = grid;
  unit.Start = 0.;
  unit.Step = 1.;
  unit.Count = 1;
  wcsprm* gridWCS = vtkAstroReprojector::MakeWCS(celestial, &unit, inputs[0].WCS);
  if (!gridWCS)
    {
    error = "invalid spectral axis (type " + grid.Type + ")";
    return false;
    }
  double low = 0., high = 0., narrowest = std::numeric_limits<double>::max();
  bool first = true;
  for (const Input& input : inputs)
    {
    int dims[3];
    input.Data->GetDimensions(dims);
    if (!input.WCS || input.WCS->spec < 0 || dims[2] < 1)
      {
      continue;
      }
    double direction[2] = {0., 0.};
    ReferenceDirectionICRS(input.WCS, direction);
    // input channel -> grid value (the grid pixel is its value: step 1, start 0 at pixel 1)
    SpectralMapper mapper(input.WCS, gridWCS, MatchFrequency, direction);
    if (!mapper.Valid)
      {
      vtkAstroReprojector::FreeWCS(gridWCS);
      error = input.Name + ": " + (mapper.Error.empty() ? "spectral axis not supported" : mapper.Error);
      return false;
      }
    double values[3];
    if (!mapper.Map(0.5, values[0]) || !mapper.Map(dims[2] + 0.5, values[1]) || !mapper.Map(1.5, values[2]))
      {
      vtkAstroReprojector::FreeWCS(gridWCS);
      error = input.Name + ": the channels could not be converted (rest frequency, frame)";
      return false;
      }
    const double a = std::min(values[0], values[1]) - 1., b = std::max(values[0], values[1]) - 1.;
    narrowest = std::min(narrowest, std::fabs(values[2] - values[0]));
    if (first)
      {
      low = a;
      high = b;
      }
    else if (unionOfInputs)
      {
      low = std::min(low, a);
      high = std::max(high, b);
      }
    else
      {
      low = std::max(low, a);
      high = std::min(high, b);
      }
    first = false;
    }
  vtkAstroReprojector::FreeWCS(gridWCS);
  if (first)
    {
    error = "no input with a spectral axis";
    return false;
    }
  if (!(high > low))
    {
    error = "the spectral ranges of the inputs do not overlap";
    return false;
    }
  const double width = step > 0. ? step : narrowest;
  grid.Step = width;
  grid.Count = std::max(1, static_cast<int>(std::floor((high - low) / width + 1.e-6)));
  // channels centred in the range (value at the centre of the first channel)
  grid.Start = low + 0.5 * ((high - low) - (grid.Count - 1) * width);
  return true;
}

//----------------------------------------------------------------------------
namespace
{
/// Per input: data (convolved if needed) and resampling information
struct PreparedInput
{
  const vtkAstroReprojector::Input* Source = nullptr;
  vtkSmartPointer<vtkImageData> Data;
  int Dims[3] = {1, 1, 1};
  bool PerPixel = false;
  double Weight = 1.;
  // output pixel -> input pixel (from 0; NaN outside)
  std::vector<double> X, Y;
  // Jy/pixel: output pixel area / input pixel area
  std::vector<double> AreaRatio;
  // area average and exact overlap: input pixels of each output pixel
  std::vector<std::size_t> Offsets;
  std::vector<std::int64_t> Indices;
  std::vector<double> Weights;
  std::vector<double> OutputArea; // exact: steradians
  std::vector<double> InputArea;  // exact, Jy/pixel: steradians
  std::vector<ChannelWeights> Spectral;
  std::vector<char> SpectralOverlapChannels;
  double Pointing[2] = {0., 0.};  // frame of the output
  vtkImageData* PrimaryBeamData = nullptr;
};

//----------------------------------------------------------------------------
/// Combination of the input channels of an output channel (NaN rules:
/// interpolation kernels: any blank -> blank; overlap: mean of the valid
/// channels if they cover half of the channel)
void CombineChannels(vtkImageData* data, const int dims[3], const ChannelWeights& weights, bool overlap,
                     std::vector<double>& plane)
{
  const std::size_t planeSize = static_cast<std::size_t>(dims[0]) * dims[1];
  plane.assign(planeSize, 0.);
  if (weights.size() == 1 && weights[0].second == 1.)
    {
    const std::size_t offset = planeSize * weights[0].first;
    for (std::size_t index = 0; index < planeSize; index++)
      {
      plane[index] = VoxelValue(data, offset + index);
      }
    return;
    }
  vtkSMPTools::For(0, static_cast<vtkIdType>(planeSize), [&](vtkIdType first, vtkIdType last)
    {
    for (vtkIdType index = first; index < last; index++)
      {
      double sum = 0., valid = 0.;
      bool blank = false;
      for (const std::pair<int, double>& weight : weights)
        {
        const double value = VoxelValue(data, planeSize * weight.first + index);
        if (std::isnan(value))
          {
          blank = true;
          continue;
          }
        sum += weight.second * value;
        valid += weight.second;
        }
      if (overlap)
        {
        plane[index] = valid >= 0.5 ? sum / valid : NaN;
        }
      else
        {
        plane[index] = blank ? NaN : sum;
        }
      }
    });
}

//----------------------------------------------------------------------------
double Power(double value, int exponent)
{
  switch (exponent)
    {
    case 0: return 1.;
    case 1: return value;
    case 2: return value * value;
    case -1: return 1. / value;
    default: return std::pow(value, exponent);
    }
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
bool vtkAstroReprojector::Run(const std::vector<Input>& inputs, const wcsprm* outputWCS, const int outputDimensions[3],
                              const Options& options, Outputs& outputs, const ProgressFunction& progress,
                              const std::atomic<bool>* cancel)
{
  outputs = Outputs();
  if (inputs.empty() || !outputWCS || outputWCS->lng < 0 || outputWCS->lat < 0)
    {
    outputs.Error = inputs.empty() ? "no input data" : "the output grid has no celestial coordinates";
    return false;
    }
  const int nx = outputDimensions[0], ny = outputDimensions[1];
  const int nz = std::max(1, outputDimensions[2]);
  const std::size_t planeSize = static_cast<std::size_t>(nx) * ny;
  if (nx < 1 || ny < 1)
    {
    outputs.Error = "invalid output size";
    return false;
    }
  const bool usesPrimaryBeam = options.Combine != CombineMean && options.PrimaryBeam != NoPrimaryBeam;
  if (options.Combine != CombineMean && options.PrimaryBeam == NoPrimaryBeam && inputs.size() > 1)
    {
    outputs.Warnings.push_back("no primary beam model: the inputs are averaged");
    }
  const std::atomic<bool> notCancelled{false};
  const std::atomic<bool>& cancelled = cancel ? *cancel : notCancelled;
  double progressDone = 0.;
  const double progressTotal = 2. * inputs.size() + static_cast<double>(nz) * inputs.size();
  std::function<void(double)> advance = [&](double amount)
    {
    progressDone += amount;
    if (progress)
      {
      progress(std::min(1., progressDone / progressTotal));
      }
    };

  // output pixel centres (FITS pixels) and their sky positions (frame of the output)
  std::vector<double> outputPixels(2 * planeSize), outputWorld(2 * planeSize);
  for (int j = 0; j < ny; j++)
    {
    for (int i = 0; i < nx; i++)
      {
      outputPixels[2 * (i + static_cast<std::size_t>(nx) * j)] = i + 1.;
      outputPixels[2 * (i + static_cast<std::size_t>(nx) * j) + 1] = j + 1.;
      }
    }
  {
  PixelMapper identity(outputWCS, outputWCS);
  std::vector<double> mapped(2 * planeSize);
  vtkSMPTools::For(0, ny, [&](vtkIdType first, vtkIdType last)
    {
    for (vtkIdType j = first; j < last; j++)
      {
      const std::size_t offset = 2 * static_cast<std::size_t>(nx) * j;
      identity.Map(nx, outputPixels.data() + offset, mapped.data() + offset, outputWorld.data() + offset);
      }
    });
  }
  double outputDirection[2] = {0., 0.};
  ReferenceDirectionICRS(outputWCS, outputDirection);

  // frequencies of the output channels (primary beam models)
  std::vector<double> channelFrequencies(nz, 0.);
  if (usesPrimaryBeam && (options.PrimaryBeam == GaussianPrimaryBeam || options.PrimaryBeam == AiryPrimaryBeam))
    {
    WCSCopy copy(outputWCS);
    for (int k = 0; k < nz; k++)
      {
      double value = 0.;
      if (copy.Valid && copy.WCS.spec >= 0 && SpectralWorld(&copy.WCS, k + 1., value))
        {
        vtkAstroReprojector::SpectralValueToFrequency(SpectralTypeOf(outputWCS).c_str(), value,
                                                      RestFrequencyOf(outputWCS), channelFrequencies[k]);
        }
      else
        {
        // images: the observed frequency of a (single channel) spectral axis
        // of the inputs; the rest frequency otherwise
        double frequency = 0.;
        for (const Input& input : inputs)
          {
          if (input.Frequency > 0.)
            {
            frequency = input.Frequency;
            break;
            }
          WCSCopy inputCopy(input.WCS);
          double inputValue = 0.;
          if (inputCopy.Valid && inputCopy.WCS.spec >= 0 && SpectralWorld(&inputCopy.WCS, 1., inputValue) &&
              vtkAstroReprojector::SpectralValueToFrequency(SpectralTypeOf(input.WCS).c_str(), inputValue,
                                                            RestFrequencyOf(input.WCS), frequency) &&
              frequency > 0.)
            {
            break;
            }
          frequency = 0.;
          }
        if (!(frequency > 0.))
          {
          frequency = RestFrequencyOf(outputWCS) > 0. ? RestFrequencyOf(outputWCS) : RestFrequencyOf(inputs[0].WCS);
          }
        channelFrequencies[k] = frequency;
        }
      if (!(channelFrequencies[k] > 0.))
        {
        outputs.Error = "the primary beam model needs the frequency of the data";
        return false;
        }
      }
    }

  // --- preparation of the inputs
  std::vector<PreparedInput> prepared(inputs.size());
  std::string beamUnit = inputs[0].BUnit;
  for (std::size_t p = 0; p < inputs.size(); p++)
    {
    const Input& input = inputs[p];
    PreparedInput& item = prepared[p];
    item.Source = &input;
    if (!input.Data || !input.WCS || input.WCS->lng < 0)
      {
      outputs.Error = input.Name + ": no data or no celestial coordinates";
      return false;
      }
    std::string bunit = input.BUnit;
    item.Data = input.Data;
    if (options.TargetBeamEnabled)
      {
      std::string error;
      if (!ConvolveToBeam(input, options.TargetBeam, item.Data, bunit, error, &cancelled))
        {
        outputs.Error = cancelled ? "cancelled" : error;
        return false;
        }
      }
    if (p == 0)
      {
      beamUnit = bunit;
      }
    item.Data->GetDimensions(item.Dims);
    item.PerPixel = ContainsCaseInsensitive(bunit, "/PIXEL");
    item.Weight = 1.;
    if (options.NoiseWeighting)
      {
      if (!(input.Noise > 0.))
        {
        outputs.Error = input.Name + ": the noise of the data is needed for noise weighting";
        return false;
        }
      item.Weight = 1. / (input.Noise * input.Noise);
      }
    advance(1.);
    if (cancelled)
      {
      outputs.Error = "cancelled";
      return false;
      }

    // spatial map: output pixel centres -> input pixels
    PixelMapper mapper(outputWCS, input.WCS);
    if (!mapper.Converter.Valid)
      {
      outputs.Error = input.Name + ": its celestial coordinate system can not be converted to the one of the output";
      return false;
      }
    item.X.assign(planeSize, NaN);
    item.Y.assign(planeSize, NaN);
    {
    std::vector<double> mapped(2 * planeSize);
    vtkSMPTools::For(0, ny, [&](vtkIdType first, vtkIdType last)
      {
      for (vtkIdType j = first; j < last; j++)
        {
        const std::size_t offset = 2 * static_cast<std::size_t>(nx) * j;
        mapper.Map(nx, outputPixels.data() + offset, mapped.data() + offset);
        }
      });
    for (std::size_t o = 0; o < planeSize; o++)
      {
      if (!std::isnan(mapped[2 * o]))
        {
        item.X[o] = mapped[2 * o] - 1.;
        item.Y[o] = mapped[2 * o + 1] - 1.;
        }
      }
    }
    // local ratio of the pixel areas (Jacobian of the map)
    item.AreaRatio.assign(planeSize, 1.);
    {
    std::vector<double>& X = item.X;
    std::vector<double>& Y = item.Y;
    vtkSMPTools::For(0, ny, [&](vtkIdType first, vtkIdType last)
      {
      for (vtkIdType j = first; j < last; j++)
        {
        for (int i = 0; i < nx; i++)
          {
          const std::size_t o = i + static_cast<std::size_t>(nx) * j;
          const int i0 = std::max(0, i - 1), i1 = std::min(nx - 1, i + 1);
          const int j0 = std::max(0, static_cast<int>(j) - 1), j1 = std::min(ny - 1, static_cast<int>(j) + 1);
          const std::size_t a = i0 + static_cast<std::size_t>(nx) * j, b = i1 + static_cast<std::size_t>(nx) * j;
          const std::size_t c = i + static_cast<std::size_t>(nx) * j0, d = i + static_cast<std::size_t>(nx) * j1;
          if (i1 == i0 || j1 == j0 || std::isnan(X[a]) || std::isnan(X[b]) || std::isnan(X[c]) || std::isnan(X[d]))
            {
            continue;
            }
          const double dxdi = (X[b] - X[a]) / (i1 - i0), dydi = (Y[b] - Y[a]) / (i1 - i0);
          const double dxdj = (X[d] - X[c]) / (j1 - j0), dydj = (Y[d] - Y[c]) / (j1 - j0);
          item.AreaRatio[o] = std::fabs(dxdi * dydj - dxdj * dydi);
          }
        }
      });
    }

    // area average: supersampled output pixels -> input pixels
    if (options.Spatial == AreaAverage)
      {
      int S = options.Supersampling;
      if (S <= 0)
        {
        // two subpixels per input pixel
        const double ratio = item.AreaRatio[nx / 2 + static_cast<std::size_t>(nx) * (ny / 2)];
        S = std::max(2, std::min(16, static_cast<int>(std::ceil(2. * std::sqrt(std::max(ratio, 0.))))));
        }
      std::vector<std::vector<std::pair<std::int64_t, double> > > rows(ny);
      vtkSMPTools::For(0, ny, [&](vtkIdType first, vtkIdType last)
        {
        std::vector<double> subpixels(2 * static_cast<std::size_t>(nx) * S * S);
        std::vector<double> mapped(subpixels.size());
        for (vtkIdType j = first; j < last; j++)
          {
          std::size_t count = 0;
          for (int i = 0; i < nx; i++)
            {
            for (int b = 0; b < S; b++)
              {
              for (int a = 0; a < S; a++)
                {
                subpixels[count++] = i + 1. - 0.5 + (a + 0.5) / S;
                subpixels[count++] = j + 1. - 0.5 + (b + 0.5) / S;
                }
              }
            }
          mapper.Map(static_cast<int>(count / 2), subpixels.data(), mapped.data());
          std::vector<std::pair<std::int64_t, double> >& row = rows[j];
          // per output pixel: -1 separators are not needed, row[...] keeps
          // (input index, weight) runs; offsets are rebuilt below
          for (int i = 0; i < nx; i++)
            {
            std::vector<std::pair<std::int64_t, double> > local;
            for (int sub = 0; sub < S * S; sub++)
              {
              const std::size_t m = 2 * (static_cast<std::size_t>(i) * S * S + sub);
              const double x = mapped[m] - 1., y = mapped[m + 1] - 1.;
              if (!(x >= -0.5 && x < item.Dims[0] - 0.5 && y >= -0.5 && y < item.Dims[1] - 0.5))
                {
                continue;
                }
              const std::int64_t index = static_cast<std::int64_t>(std::floor(x + 0.5)) +
                                         static_cast<std::int64_t>(item.Dims[0]) * static_cast<std::int64_t>(std::floor(y + 0.5));
              local.push_back(std::make_pair(index, 1. / (S * S)));
              }
            std::sort(local.begin(), local.end());
            std::vector<std::pair<std::int64_t, double> > merged;
            for (const std::pair<std::int64_t, double>& entry : local)
              {
              if (!merged.empty() && merged.back().first == entry.first)
                {
                merged.back().second += entry.second;
                }
              else
                {
                merged.push_back(entry);
                }
              }
            row.push_back(std::make_pair(-1, static_cast<double>(merged.size())));
            row.insert(row.end(), merged.begin(), merged.end());
            }
          }
        });
      item.Offsets.assign(planeSize + 1, 0);
      for (int j = 0; j < ny; j++)
        {
        const std::vector<std::pair<std::int64_t, double> >& row = rows[j];
        std::size_t position = 0;
        for (int i = 0; i < nx; i++)
          {
          const std::size_t o = i + static_cast<std::size_t>(nx) * j;
          const std::size_t count = static_cast<std::size_t>(row[position].second);
          position++;
          item.Offsets[o] = item.Indices.size();
          for (std::size_t entry = 0; entry < count; entry++, position++)
            {
            item.Indices.push_back(row[position].first);
            item.Weights.push_back(row[position].second);
            }
          }
        std::vector<std::pair<std::int64_t, double> >().swap(rows[j]);
        }
      item.Offsets[planeSize] = item.Indices.size();
      }

    // exact overlap: spherical quadrilaterals of the pixels (Montage)
    if (options.Spatial == ExactOverlap)
      {
      const int cnx = nx + 1, cny = ny + 1;
      const int inx = item.Dims[0], iny = item.Dims[1];
      // corners of the output pixels: sky (frame of the output) and input pixels
      std::vector<double> outputCorners(2 * static_cast<std::size_t>(cnx) * cny);
      std::vector<double> outputCornersWorld(outputCorners.size()), outputCornersInput(outputCorners.size());
      for (int j = 0; j < cny; j++)
        {
        for (int i = 0; i < cnx; i++)
          {
          outputCorners[2 * (i + static_cast<std::size_t>(cnx) * j)] = i + 0.5;
          outputCorners[2 * (i + static_cast<std::size_t>(cnx) * j) + 1] = j + 0.5;
          }
        }
      vtkSMPTools::For(0, cny, [&](vtkIdType first, vtkIdType last)
        {
        for (vtkIdType j = first; j < last; j++)
          {
          const std::size_t offset = 2 * static_cast<std::size_t>(cnx) * j;
          mapper.Map(cnx, outputCorners.data() + offset, outputCornersInput.data() + offset,
                     outputCornersWorld.data() + offset);
          }
        });
      // corners of the input pixels in the frame of the output
      const int icnx = inx + 1, icny = iny + 1;
      std::vector<double> inputCorners(2 * static_cast<std::size_t>(icnx) * icny);
      std::vector<double> inputCornersOutput(inputCorners.size()), inputCornersWorld(inputCorners.size());
      for (int j = 0; j < icny; j++)
        {
        for (int i = 0; i < icnx; i++)
          {
          inputCorners[2 * (i + static_cast<std::size_t>(icnx) * j)] = i + 0.5;
          inputCorners[2 * (i + static_cast<std::size_t>(icnx) * j) + 1] = j + 0.5;
          }
        }
      PixelMapper inverse(input.WCS, outputWCS);
      vtkSMPTools::For(0, icny, [&](vtkIdType first, vtkIdType last)
        {
        for (vtkIdType j = first; j < last; j++)
          {
          const std::size_t offset = 2 * static_cast<std::size_t>(icnx) * j;
          inverse.Map(icnx, inputCorners.data() + offset, inputCornersOutput.data() + offset, nullptr,
                      inputCornersWorld.data() + offset);
          }
        });
      // corners (radians) of an output or input pixel
      std::function<bool(const std::vector<double>&, int, int, int, double*, double*)> quad =
        [](const std::vector<double>& corners, int stride, int i, int j, double* lon, double* lat)
        {
        const int ci[4] = {i, i + 1, i + 1, i}, cj[4] = {j, j, j + 1, j + 1};
        for (int c = 0; c < 4; c++)
          {
          const std::size_t index = 2 * (ci[c] + static_cast<std::size_t>(stride) * cj[c]);
          if (std::isnan(corners[index]))
            {
            return false;
            }
          lon[c] = corners[index] * DEG2RAD;
          lat[c] = corners[index + 1] * DEG2RAD;
          }
        return true;
        };
      if (item.PerPixel)
        {
        item.InputArea.assign(static_cast<std::size_t>(inx) * iny, 0.);
        vtkSMPTools::For(0, iny, [&](vtkIdType first, vtkIdType last)
          {
          for (vtkIdType j = first; j < last; j++)
            {
            for (int i = 0; i < inx; i++)
              {
              double lon[4], lat[4], ratio = 1.;
              if (quad(inputCornersWorld, icnx, i, static_cast<int>(j), lon, lat))
                {
                item.InputArea[i + static_cast<std::size_t>(inx) * j] =
                  SlicerAstroComputeOverlap(lon, lat, lon, lat, 0, 1., &ratio);
                }
              }
            }
          });
        }
      item.OutputArea.assign(planeSize, 0.);
      std::vector<std::vector<std::pair<std::int64_t, double> > > rows(ny);
      vtkSMPTools::For(0, ny, [&](vtkIdType first, vtkIdType last)
        {
        for (vtkIdType j = first; j < last; j++)
          {
          std::vector<std::pair<std::int64_t, double> >& row = rows[j];
          for (int i = 0; i < nx; i++)
            {
            const std::size_t o = i + static_cast<std::size_t>(nx) * j;
            double olon[4], olat[4], ratio = 1.;
            std::size_t countIndex = row.size();
            row.push_back(std::make_pair(-1, 0.));
            if (!quad(outputCornersWorld, cnx, i, static_cast<int>(j), olon, olat))
              {
              continue;
              }
            item.OutputArea[o] = SlicerAstroComputeOverlap(olon, olat, olon, olat, 0, 1., &ratio);
            // candidate input pixels: box of the output corners in the input
            double xmin = std::numeric_limits<double>::max(), xmax = -xmin, ymin = xmin, ymax = -xmin;
            bool inside = true;
            const int ci[4] = {i, i + 1, i + 1, i}, cj[4] = {static_cast<int>(j), static_cast<int>(j), static_cast<int>(j) + 1, static_cast<int>(j) + 1};
            for (int c = 0; c < 4; c++)
              {
              const std::size_t index = 2 * (ci[c] + static_cast<std::size_t>(cnx) * cj[c]);
              const double x = outputCornersInput[index] - 1., y = outputCornersInput[index + 1] - 1.;
              if (std::isnan(x))
                {
                inside = false;
                break;
                }
              xmin = std::min(xmin, x);
              xmax = std::max(xmax, x);
              ymin = std::min(ymin, y);
              ymax = std::max(ymax, y);
              }
            if (!inside)
              {
              continue;
              }
            const int i0 = std::max(0, static_cast<int>(std::floor(xmin + 0.5)) - 1);
            const int i1 = std::min(inx - 1, static_cast<int>(std::floor(xmax + 0.5)) + 1);
            const int j0 = std::max(0, static_cast<int>(std::floor(ymin + 0.5)) - 1);
            const int j1 = std::min(iny - 1, static_cast<int>(std::floor(ymax + 0.5)) + 1);
            int count = 0;
            for (int jj = j0; jj <= j1; jj++)
              {
              for (int ii = i0; ii <= i1; ii++)
                {
                double ilon[4], ilat[4];
                if (!quad(inputCornersWorld, icnx, ii, jj, ilon, ilat))
                  {
                  continue;
                  }
                const double overlap = SlicerAstroComputeOverlap(ilon, ilat, olon, olat, 0, 1., &ratio);
                if (overlap > 0. && !std::isnan(overlap))
                  {
                  row.push_back(std::make_pair(ii + static_cast<std::int64_t>(inx) * jj, overlap));
                  count++;
                  }
                }
              }
            row[countIndex].second = count;
            }
          }
        });
      item.Offsets.assign(planeSize + 1, 0);
      for (int j = 0; j < ny; j++)
        {
        const std::vector<std::pair<std::int64_t, double> >& row = rows[j];
        std::size_t position = 0;
        for (int i = 0; i < nx; i++)
          {
          const std::size_t o = i + static_cast<std::size_t>(nx) * j;
          const std::size_t count = static_cast<std::size_t>(row[position].second);
          position++;
          item.Offsets[o] = item.Indices.size();
          for (std::size_t entry = 0; entry < count; entry++, position++)
            {
            item.Indices.push_back(row[position].first);
            item.Weights.push_back(row[position].second);
            }
          }
        std::vector<std::pair<std::int64_t, double> >().swap(rows[j]);
        }
      item.Offsets[planeSize] = item.Indices.size();
      }

    // spectral weights of the output channels
    {
    std::string error, warning;
    if (!MakeSpectralWeights(outputWCS, nz, input.WCS, item.Dims[2], options.Spectral, options.Matching,
                             outputDirection, item.Spectral, item.SpectralOverlapChannels, error, warning))
      {
      outputs.Error = input.Name + ": " + error;
      return false;
      }
    if (!warning.empty())
      {
      outputs.Warnings.push_back(input.Name + ": " + warning);
      }
    }

    // primary beam: pointing in the frame of the output
    if (usesPrimaryBeam)
      {
      if (options.PrimaryBeam == VolumePrimaryBeam)
        {
        item.PrimaryBeamData = input.PrimaryBeam;
        int pbDims[3] = {0, 0, 0};
        if (input.PrimaryBeam)
          {
          input.PrimaryBeam->GetDimensions(pbDims);
          }
        if (!input.PrimaryBeam || pbDims[0] != item.Dims[0] || pbDims[1] != item.Dims[1] ||
            (pbDims[2] != item.Dims[2] && pbDims[2] != 1))
          {
          outputs.Error = input.Name + ": a primary beam volume with the grid of the data is needed";
          return false;
          }
        }
      else
        {
        double lon = input.Pointing[0], lat = input.Pointing[1];
        if (std::isnan(lon) || std::isnan(lat))
          {
          lon = input.WCS->crval[input.WCS->lng];
          lat = input.WCS->crval[input.WCS->lat];
          }
        CelestialConverter toOutput(input.WCS, outputWCS);
        if (!toOutput.Valid || !toOutput.Convert(lon, lat))
          {
          outputs.Error = input.Name + ": the pointing centre could not be converted";
          return false;
          }
        item.Pointing[0] = lon;
        item.Pointing[1] = lat;
        if (!(input.DishDiameter > 0.))
          {
          outputs.Error = input.Name + ": the dish diameter is needed for the primary beam model";
          return false;
          }
        }
      }
    advance(1.);
    if (cancelled)
      {
      outputs.Error = "cancelled";
      return false;
      }
    }

  // --- outputs
  std::function<vtkSmartPointer<vtkImageData>()> newImage = [&]()
    {
    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(nx, ny, nz);
    image->AllocateScalars(VTK_FLOAT, 1);
    return image;
    };
  outputs.Data = newImage();
  if (options.ComputeWeight)
    {
    outputs.Weight = newImage();
    }
  bool sensitivity = options.ComputeSensitivity;
  if (sensitivity)
    {
    for (const Input& input : inputs)
      {
      sensitivity = sensitivity && input.Noise > 0.;
      }
    if (!sensitivity)
      {
      outputs.Warnings.push_back("the sensitivity needs the noise of every input: not computed");
      }
    else
      {
      outputs.Sensitivity = newImage();
      }
    }
  if (options.SaultFraction > 0.)
    {
    outputs.Sault = newImage();
    }
  float* outData = static_cast<float*>(outputs.Data->GetScalarPointer());
  float* outWeight = outputs.Weight ? static_cast<float*>(outputs.Weight->GetScalarPointer()) : nullptr;
  float* outSensitivity = outputs.Sensitivity ? static_cast<float*>(outputs.Sensitivity->GetScalarPointer()) : nullptr;
  float* outSault = outputs.Sault ? static_cast<float*>(outputs.Sault->GetScalarPointer()) : nullptr;
  const float blank = std::numeric_limits<float>::quiet_NaN();

  // exponents of the combinations (CASA linearmosaic, Sault et al. 1996):
  // numerator weight A^alpha, denominator A^beta, noise of the input A^-gamma
  int alpha = 0, beta = 0, gamma = 0;
  if (options.Combine == CombineOptimal)
    {
    alpha = options.Correction == FluxCorrect ? 2 : (options.Correction == PrimaryBeamWeighted ? 1 : 0);
    beta = 2;
    }
  else if (options.Combine == CombinePrimaryBeam)
    {
    alpha = options.Correction == FluxCorrect ? 1 : (options.Correction == PrimaryBeamWeighted ? 0 : -1);
    beta = 1;
    }
  gamma = options.Correction == FluxCorrect ? 1 : (options.Correction == PrimaryBeamWeighted ? 0 : -1);

  // --- channels
  std::vector<double> numerator(planeSize), denominator(planeSize), coverage(planeSize), variance(planeSize);
  std::vector<double> plane, beamPlane;
  for (int k = 0; k < nz; k++)
    {
    std::fill(numerator.begin(), numerator.end(), 0.);
    std::fill(denominator.begin(), denominator.end(), 0.);
    std::fill(coverage.begin(), coverage.end(), 0.);
    std::fill(variance.begin(), variance.end(), 0.);
    for (std::size_t p = 0; p < prepared.size(); p++)
      {
      if (cancelled)
        {
        outputs.Error = "cancelled";
        return false;
        }
      PreparedInput& item = prepared[p];
      const ChannelWeights& channelWeights = item.Spectral[k];
      if (channelWeights.empty())
        {
        advance(1.);
        continue;
        }
      const bool overlap = item.SpectralOverlapChannels[k] != 0;
      CombineChannels(item.Data, item.Dims, channelWeights, overlap, plane);
      const bool beamVolume = usesPrimaryBeam && options.PrimaryBeam == VolumePrimaryBeam;
      if (beamVolume)
        {
        int pbDims[3];
        item.PrimaryBeamData->GetDimensions(pbDims);
        if (pbDims[2] == 1)
          {
          ChannelWeights first(1, std::make_pair(0, 1.));
          CombineChannels(item.PrimaryBeamData, pbDims, first, false, beamPlane);
          }
        else
          {
          CombineChannels(item.PrimaryBeamData, pbDims, channelWeights, overlap, beamPlane);
          }
        }
      const double frequency = channelFrequencies[k];
      const double noise2 = item.Source->Noise * item.Source->Noise;
      vtkSMPTools::For(0, ny, [&](vtkIdType first, vtkIdType last)
        {
        for (vtkIdType j = first; j < last; j++)
          {
          for (int i = 0; i < nx; i++)
            {
            const std::size_t o = i + static_cast<std::size_t>(nx) * j;
            double value = NaN, covered = 1.;
            if (options.Spatial == AreaAverage || options.Spatial == ExactOverlap)
              {
              double sum = 0., valid = 0., flux = 0.;
              for (std::size_t entry = item.Offsets[o]; entry < item.Offsets[o + 1]; entry++)
                {
                const double v = plane[static_cast<std::size_t>(item.Indices[entry])];
                if (std::isnan(v))
                  {
                  continue;
                  }
                const double w = item.Weights[entry];
                sum += w * v;
                valid += w;
                if (item.PerPixel)
                  {
                  flux += options.Spatial == ExactOverlap ?
                    w * v / item.InputArea[static_cast<std::size_t>(item.Indices[entry])] :
                    w * v * item.AreaRatio[o];
                  }
                }
              covered = options.Spatial == ExactOverlap ?
                (item.OutputArea[o] > 0. ? valid / item.OutputArea[o] : 0.) : valid;
              if (!(valid > 0.))
                {
                continue;
                }
              value = item.PerPixel ? flux / std::max(covered, 1.e-12) : sum / valid;
              }
            else
              {
              if (std::isnan(item.X[o]))
                {
                continue;
                }
              value = Interpolate(plane.data(), item.Dims[0], item.Dims[1], item.X[o], item.Y[o], options.Spatial);
              if (item.PerPixel)
                {
                value *= item.AreaRatio[o];
                }
              }
            if (std::isnan(value) || !(covered > 1.e-6))
              {
              continue;
              }
            covered = std::min(covered, 1.);
            // primary beam
            double A = 1.;
            if (usesPrimaryBeam)
              {
              if (beamVolume)
                {
                A = Interpolate(beamPlane.data(), item.Dims[0], item.Dims[1], item.X[o], item.Y[o], Bilinear);
                }
              else
                {
                const double rho = AngularDistanceRadians(outputWorld[2 * o], outputWorld[2 * o + 1],
                                                          item.Pointing[0], item.Pointing[1]);
                const double lambda = SPEED_OF_LIGHT / frequency;
                if (options.PrimaryBeam == GaussianPrimaryBeam)
                  {
                  const double fwhm = options.PrimaryBeamFactor * lambda / item.Source->DishDiameter;
                  A = std::exp(-4. * std::log(2.) * rho * rho / (fwhm * fwhm));
                  }
                else
                  {
                  const double x = vtkMath::Pi() * item.Source->DishDiameter * std::sin(rho) / lambda;
                  const double airy = x < 1.e-8 ? 1. : 2. * BesselJ1(x) / x;
                  A = airy * airy;
                  }
                }
              if (std::isnan(A) || A < options.PrimaryBeamCutoff)
                {
                continue;
                }
              }
            const double w = item.Weight * covered;
            const double coefficient = w * Power(A, alpha);
            numerator[o] += coefficient * value;
            denominator[o] += w * Power(A, beta);
            coverage[o] += covered;
            if (outSensitivity)
              {
              variance[o] += coefficient * coefficient * noise2 * Power(A, -2 * gamma);
              }
            }
          }
        });
      advance(1.);
      }
    // normalization (CASA: weights below 1e-6 of the peak are not trusted)
    double peak = 0.;
    for (std::size_t o = 0; o < planeSize; o++)
      {
      peak = std::max(peak, denominator[o]);
      }
    const std::size_t offset = planeSize * k;
    for (std::size_t o = 0; o < planeSize; o++)
      {
      const bool good = denominator[o] > 1.e-6 * peak && denominator[o] > 0. &&
                        coverage[o] >= options.MinimumCoverage - 1.e-9;
      const double result = good ? numerator[o] / denominator[o] : NaN;
      outData[offset + o] = good ? static_cast<float>(result) : blank;
      if (outWeight)
        {
        outWeight[offset + o] = static_cast<float>(denominator[o]);
        }
      if (outSensitivity)
        {
        outSensitivity[offset + o] = good ? static_cast<float>(std::sqrt(variance[o]) / denominator[o]) : blank;
        }
      if (outSault)
        {
        // Sault, Staveley-Smith & Brouw (1996): flux correct where the
        // weight is above SaultFraction of its peak, flat noise below
        const double threshold = options.SaultFraction * peak;
        const double factor = threshold > 0. ? std::min(1., std::sqrt(denominator[o] / threshold)) : 1.;
        outSault[offset + o] = good ? static_cast<float>(result * factor) : blank;
        }
      }
    }

  // beam and unit of the result
  outputs.BUnit = beamUnit;
  if (options.TargetBeamEnabled)
    {
    std::copy(options.TargetBeam, options.TargetBeam + 3, outputs.Beam);
    }
  else
    {
    std::copy(inputs[0].Beam, inputs[0].Beam + 3, outputs.Beam);
    bool different = false;
    vtkNew<vtkDoubleArray> beams;
    beams->SetNumberOfComponents(3);
    for (const Input& input : inputs)
      {
      different = different || std::fabs(input.Beam[0] - inputs[0].Beam[0]) > 1.e-9 ||
                  std::fabs(input.Beam[1] - inputs[0].Beam[1]) > 1.e-9 || std::fabs(input.Beam[2] - inputs[0].Beam[2]) > 1.e-6;
      beams->InsertNextTuple(input.Beam);
      }
    if (different)
      {
      vtkAstroBeams::CommonBeam(beams, outputs.Beam);
      outputs.Warnings.push_back("the inputs have different beams and were not convolved to a common "
                                 "resolution: the resolution of the result varies (beam: common beam)");
      }
    }
  return true;
}
