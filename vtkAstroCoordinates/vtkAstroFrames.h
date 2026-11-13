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

#ifndef __vtkAstroFrames_h
#define __vtkAstroFrames_h

#include "vtkAstroCoordinatesExport.h"

#include <vtkObject.h>

/// \brief Astronomical reference frames.
///
/// Celestial frames: ICRS, FK5 (any Julian equinox, IAU 1976 precession),
/// FK4 (any Besselian equinox, Newcomb precession, with or without the
/// E-terms of aberration), Galactic (IAU 1958, defined from FK5 J2000 as
/// astropy and the FITS WCS paper II) and Supergalactic (de Vaucouleurs).
/// ICRS <-> FK5: the FK5 frame tie (ERFA eraFk5hip). The conventions are
/// those of astropy; casacore differs (Galactic defined from FK4 B1950:
/// up to ~0.5 arcsec). FK4 <-> FK5 assumes zero proper motion in FK5 at the
/// epoch of observation (ERFA eraFk45z / eraFk54z), as astropy and casacore.
///
/// Spectral frames (FITS SPECSYS): the frequency measured by an observer
/// at rest in each frame. The frames are defined by the velocity of their
/// observer relative to the solar system barycentre, as in casacore
/// (MCFrequency, MeasTable) and the FITS WCS paper III (Greisen et al. 2006):
/// - geocentric: velocity of the Earth (ERFA eraEpv00);
/// - topocentric: plus the rotation of the Earth at the observatory
///   (OBSGEO-X/Y/Z, ERFA eraPvtob);
/// - LSRK: the Sun moves at 20 km/s towards (18h, +30 deg) B1900;
/// - LSRD: the Sun moves at (9, 12, 7) km/s (galactic U, V, W);
/// - galactocentric: LSRD plus 220 km/s towards (l, b) = (90, 0) deg;
/// - Local Group: 308 km/s; CMB: 369.5 km/s (casacore values).
/// Each frame change is a relativistic Doppler factor
/// sqrt((1 + beta) / (1 - beta)) with beta the projected velocity / c,
/// applied frame by frame as casacore does.
class VTK_ASTRO_COORDINATES_EXPORT vtkAstroFrames : public vtkObject
{
public:
  static vtkAstroFrames* New();
  vtkTypeMacro(vtkAstroFrames, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  enum CelestialFrame
    {
    CelestialUnknown = -1,
    ICRS = 0,
    FK5,
    FK4,
    FK4NoETerms,
    Galactic,
    Supergalactic
    };

  enum SpectralFrame
    {
    SpectralUnknown = -1,
    Topocentric = 0,
    Geocentric,
    Barycentric,
    LSRK,
    LSRD,
    Galactocentric,
    LocalGroup,
    CMB
    };

  /// Celestial frame of a WCS: CTYPE of the longitude (e.g. "RA---SIN",
  /// "GLON-CAR", "SLON-TAN"), RADESYS (may be empty) and EQUINOX (0: not
  /// given). Defaults of the FITS standard (WCS paper II): no RADESYS ->
  /// ICRS if there is no EQUINOX, FK4 if EQUINOX < 1984, FK5 otherwise;
  /// no EQUINOX -> 2000 for FK5, 1950 for FK4. equinoxOut: the equinox used (Julian for
  /// FK5, Besselian for FK4; 0 for the other frames).
  static int GetCelestialFrame(const char* longitudeCType, const char* radesys,
                               double equinox, double& equinoxOut);
  static const char* GetCelestialFrameName(int frame);

  /// Convert a sky position (degrees). Equinoxes: Julian years for FK5,
  /// Besselian years for FK4 (ignored for the other frames).
  /// epochMJD: MJD of the observation, for FK4 <-> FK5 (if <= 0, the
  /// equinox of the FK4 frame is used as epoch, as astropy). The longitude of the result is in [0, 360).
  static bool ConvertCelestial(double longitude, double latitude,
                               int fromFrame, double fromEquinox,
                               int toFrame, double toEquinox,
                               double epochMJD, double result[2]);

  /// Spectral frame of a FITS SPECSYS (or VELREF/CTYPE suffix) value:
  /// TOPOCENT, GEOCENTR, BARYCENT, HELIOCEN (as barycentric), LSRK, LSR,
  /// LSRD, GALACTOC, LOCALGRP, CMBDIPOL (case insensitive).
  static int GetSpectralFrame(const char* specsys);
  static const char* GetSpectralFrameName(int frame);

  /// Velocity (m/s) of the observer at rest in the frame, relative to the
  /// solar system barycentre, projected on the direction of the source
  /// (ICRS right ascension and declination, degrees): positive when the
  /// observer moves towards the source. mjdUTC and the observatory
  /// position (ITRF, metres) are used only by the topocentric and
  /// geocentric frames. For the frames reached through another one
  /// (topocentric through geocentric, galactocentric through LSRD) it is
  /// the sum of the two velocities (ConvertFrequency applies the two
  /// Doppler factors, as casacore).
  static bool GetFrameVelocity(int frame, double ra, double dec, double mjdUTC,
                               const double observatory[3], double& velocity);

  /// Frequency (any unit) in toFrame of a signal measured with frequency
  /// in fromFrame, from the direction (ICRS ra, dec in degrees).
  static bool ConvertFrequency(double frequency, int fromFrame, int toFrame,
                               double ra, double dec, double mjdUTC,
                               const double observatory[3], double& result);

  /// Besselian epoch and Julian epoch of an MJD
  static double BesselianEpoch(double mjd);
  static double JulianEpoch(double mjd);

protected:
  vtkAstroFrames() = default;
  ~vtkAstroFrames() override = default;

private:
  vtkAstroFrames(const vtkAstroFrames&) = delete;
  void operator=(const vtkAstroFrames&) = delete;
};

#endif
