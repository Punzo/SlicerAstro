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

#include "vtkAstroFrames.h"

// VTK includes
#include <vtkObjectFactory.h>

// ERFA includes
#include <erfa.h>
#include <erfam.h>

// STD includes
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

vtkStandardNewMacro(vtkAstroFrames);

namespace
{
const double DEG2RAD = ERFA_DD2R;
const double SPEED_OF_LIGHT = 299792458.0; // m/s

//----------------------------------------------------------------------------
// Velocities of the Sun relative to the kinematic frames (m/s, J2000
// equatorial unit vectors): casacore MeasTable (velocityLSRK,
// velocityLSR, velocityLSRGal, velocityLGROUP, velocityCMB, index 0), the
// conventions of the FITS WCS paper III.
const double LSRK_VELOCITY = 20.0e3;
const double LSRK_DIRECTION[3] = {0.0145021, -0.865863, 0.500071};
const double LSRD_VELOCITY = 16.552945357246848e3; // sqrt(9^2 + 12^2 + 7^2) km/s
const double LSRD_DIRECTION[3] = {-0.0385568, -0.881138, 0.471285};
const double GALACTIC_ROTATION_VELOCITY = 220.0e3;
const double GALACTIC_ROTATION_DIRECTION[3] = {0.494109, -0.44483, 0.746982};
const double LOCAL_GROUP_VELOCITY = 308.0e3;
const double LOCAL_GROUP_DIRECTION[3] = {0.593553979227, -0.177954636914, 0.784873124106};
const double CMB_VELOCITY = 369.5e3;
const double CMB_DIRECTION[3] = {-0.97176985257, 0.202393953108, -0.121243727187};


//----------------------------------------------------------------------------
std::string UpperCase(const char* text)
{
  std::string result = text ? text : "";
  for (std::size_t index = 0; index < result.size(); index++)
    {
    result[index] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[index])));
    }
  // trailing blanks of FITS strings
  while (!result.empty() && result.back() == ' ')
    {
    result.pop_back();
    }
  return result;
}

//----------------------------------------------------------------------------
void ToVector(double longitudeDegrees, double latitudeDegrees, double vector[3])
{
  eraS2c(longitudeDegrees * DEG2RAD, latitudeDegrees * DEG2RAD, vector);
}

//----------------------------------------------------------------------------
void FromVector(const double vector[3], double result[2])
{
  double longitude = 0., latitude = 0.;
  eraC2s(const_cast<double*>(vector), &longitude, &latitude);
  result[0] = eraAnp(longitude) / DEG2RAD;
  result[1] = latitude / DEG2RAD;
}

//----------------------------------------------------------------------------
void Normalize(double vector[3])
{
  const double norm = std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
  if (norm > 0.)
    {
    for (int index = 0; index < 3; index++)
      {
      vector[index] /= norm;
      }
    }
}

//----------------------------------------------------------------------------
// E-terms of aberration (radians) of FK4 positions at a Besselian equinox:
// Explanatory Supplement to the Astronomical Almanac (Seidelmann 1992), as
// astropy. At B1950 they are the constant vector of ERFA eraFk425.
void ETerms(double equinox, double eterms[3])
{
  double jd1 = 0., jd2 = 0., b1950jd1 = 0., b1950jd2 = 0.;
  eraEpb2jd(equinox, &jd1, &jd2);
  eraEpb2jd(1950.0, &b1950jd1, &b1950jd2);
  const double T = ((jd1 - b1950jd1) + (jd2 - b1950jd2)) / 36525.0;
  // constant of aberration (degrees) times the eccentricity of the orbit
  const double k = 0.0056932 * DEG2RAD;
  const double ek = k * ((-0.000000126 * T - 0.00004193) * T + 0.01673011);
  // mean longitude of perigee of the solar orbit
  const double g = ((((0.012 * T + 1.65) * T + 6190.67) * T + 1015489.951) / 3600.0) * DEG2RAD;
  const double obliquity = eraObl80(jd1, jd2);
  eterms[0] = ek * std::sin(g);
  eterms[1] = -ek * std::cos(g) * std::cos(obliquity);
  eterms[2] = -ek * std::cos(g) * std::sin(obliquity);
}

//----------------------------------------------------------------------------
// FK4 E-terms of aberration (as SLALIB ADDET/SUBET, astropy)
void AddETerms(double vector[3], double equinox)
{
  double eterms[3];
  ETerms(equinox, eterms);
  const double dot = vector[0] * eterms[0] + vector[1] * eterms[1] + vector[2] * eterms[2];
  for (int index = 0; index < 3; index++)
    {
    vector[index] = vector[index] + eterms[index] - dot * vector[index];
    }
  Normalize(vector);
}

void RemoveETerms(double vector[3], double equinox)
{
  double eterms[3];
  ETerms(equinox, eterms);
  const double dot = vector[0] * eterms[0] + vector[1] * eterms[1] + vector[2] * eterms[2];
  for (int index = 0; index < 3; index++)
    {
    vector[index] = vector[index] - eterms[index] + dot * vector[index];
    }
  Normalize(vector);
}

//----------------------------------------------------------------------------
bool IsFK4(int frame)
{
  return frame == vtkAstroFrames::FK4 || frame == vtkAstroFrames::FK4NoETerms;
}

//----------------------------------------------------------------------------
// Precession matrix between two Besselian epochs (Newcomb, as SLALIB PREBN
// and astropy): v(epoch1) = matrix * v(epoch0)
void NewcombPrecessionMatrix(double epoch0, double epoch1, double matrix[3][3])
{
  const double t1 = (epoch0 - 1850.0) / 1000.0;
  const double t2 = (epoch1 - epoch0) / 1000.0;
  const double t1Squared = t1 * t1;
  const double t2Squared = t2 * t2;
  const double t2Cubed = t2Squared * t2;
  const double arcsec = ERFA_DAS2R;
  const double w = 23035.545 + 139.720 * t1 + 0.060 * t1Squared;
  const double zeta = (w * t2 + (30.240 - 0.270 * t1) * t2Squared + 17.995 * t2Cubed) * arcsec;
  const double z = (w * t2 + (109.480 + 0.390 * t1) * t2Squared + 18.325 * t2Cubed) * arcsec;
  const double theta = ((20051.12 - 85.29 * t1 - 0.37 * t1Squared) * t2 +
                        (-42.65 - 0.37 * t1) * t2Squared - 41.80 * t2Cubed) * arcsec;
  eraIr(matrix);
  eraRz(-zeta, matrix);
  eraRy(theta, matrix);
  eraRz(-z, matrix);
}

//----------------------------------------------------------------------------
// FK5 precession (IAU 1976) between two Julian epochs:
// v(epoch1) = matrix * v(epoch0)
void FK5PrecessionMatrix(double epoch0, double epoch1, double matrix[3][3])
{
  double date1 = 0., date2 = 0.;
  double toEpoch0[3][3], toEpoch1[3][3], fromEpoch0[3][3];
  eraEpj2jd(epoch0, &date1, &date2);
  eraPmat76(date1, date2, toEpoch0); // J2000 -> epoch0
  eraEpj2jd(epoch1, &date1, &date2);
  eraPmat76(date1, date2, toEpoch1); // J2000 -> epoch1
  eraTr(toEpoch0, fromEpoch0);
  eraRxr(toEpoch1, fromEpoch0, matrix);
}

//----------------------------------------------------------------------------
// ICRS -> FK5 J2000: orientation of the FK5 frame with respect to the
// Hipparcos frame (= ICRS), ERFA eraFk5hip (Mignard & Froeschle 2000; USNO
// circular 179, as astropy): fk5 = matrix * icrs
void ICRSToFK5J2000Matrix(double matrix[3][3])
{
  double fk5ToHipparcos[3][3], spin[3];
  eraFk5hip(fk5ToHipparcos, spin);
  eraTr(fk5ToHipparcos, matrix);
}

//----------------------------------------------------------------------------
// FK5 J2000 -> Galactic: north galactic pole and longitude of the north
// celestial pole of the IAU 1958 definition in FK5 J2000 (as astropy and
// the FITS WCS paper II): galactic = matrix * fk5
void FK5J2000ToGalacticMatrix(double matrix[3][3])
{
  const double poleRA = 192.8594812065348;
  const double poleDec = 27.12825118085622;
  const double celestialPoleLongitude = 122.9319185680026;
  eraIr(matrix);
  eraRz(poleRA * DEG2RAD, matrix);
  eraRy((90.0 - poleDec) * DEG2RAD, matrix);
  eraRz((180.0 - celestialPoleLongitude) * DEG2RAD, matrix);
}

//----------------------------------------------------------------------------
// Galactic -> Supergalactic (de Vaucouleurs: north pole at galactic
// (47.37, 6.32) degrees, origin at (137.37, 0) degrees)
void GalacticToSupergalacticMatrix(double matrix[3][3])
{
  double pole[3], origin[3], yAxis[3];
  ToVector(47.37, 6.32, pole);
  ToVector(137.37, 0., origin);
  eraPxp(pole, origin, yAxis);
  Normalize(yAxis);
  for (int index = 0; index < 3; index++)
    {
    matrix[0][index] = origin[index];
    matrix[1][index] = yAxis[index];
    matrix[2][index] = pole[index];
    }
}


//----------------------------------------------------------------------------
// any frame -> ICRS unit vector
// fk4Epoch: Besselian epoch of the observation (FK4 <-> FK5)
bool ToICRS(double longitude, double latitude, int frame, double equinox, double fk4Epoch, double icrs[3])
{
  double vector[3];
  ToVector(longitude, latitude, vector);
  double matrix[3][3];
  switch (frame)
    {
    case vtkAstroFrames::ICRS:
      eraCp(vector, icrs);
      return true;
    case vtkAstroFrames::FK5:
      {
      double fk5J2000[3];
      FK5PrecessionMatrix(equinox, 2000.0, matrix);
      eraRxp(matrix, vector, fk5J2000);
      ICRSToFK5J2000Matrix(matrix);
      eraTrxp(matrix, fk5J2000, icrs);
      return true;
      }
    case vtkAstroFrames::FK4:
    case vtkAstroFrames::FK4NoETerms:
      {
      // to FK4 B1950 (E-terms removed during the precession)
      if (frame == vtkAstroFrames::FK4)
        {
        RemoveETerms(vector, equinox);
        }
      double b1950[3];
      NewcombPrecessionMatrix(equinox, 1950.0, matrix);
      eraRxp(matrix, vector, b1950);
      // eraFk45z: B1950 positions with the E-terms
      AddETerms(b1950, 1950.0);
      double ra1950 = 0., dec1950 = 0., ra2000 = 0., dec2000 = 0.;
      eraC2s(b1950, &ra1950, &dec1950);
      eraFk45z(ra1950, dec1950, fk4Epoch, &ra2000, &dec2000);
      double fk5J2000[3];
      eraS2c(ra2000, dec2000, fk5J2000);
      ICRSToFK5J2000Matrix(matrix);
      eraTrxp(matrix, fk5J2000, icrs);
      return true;
      }
    case vtkAstroFrames::Galactic:
    case vtkAstroFrames::Supergalactic:
      {
      double galactic[3];
      if (frame == vtkAstroFrames::Supergalactic)
        {
        GalacticToSupergalacticMatrix(matrix);
        eraTrxp(matrix, vector, galactic);
        }
      else
        {
        eraCp(vector, galactic);
        }
      double fk5J2000[3];
      FK5J2000ToGalacticMatrix(matrix);
      eraTrxp(matrix, galactic, fk5J2000);
      ICRSToFK5J2000Matrix(matrix);
      eraTrxp(matrix, fk5J2000, icrs);
      return true;
      }
    default:
      return false;
    }
}

//----------------------------------------------------------------------------
// ICRS unit vector -> any frame
bool FromICRS(const double icrs[3], int frame, double equinox, double fk4Epoch, double result[2])
{
  double matrix[3][3];
  switch (frame)
    {
    case vtkAstroFrames::ICRS:
      FromVector(icrs, result);
      return true;
    case vtkAstroFrames::FK5:
      {
      double fk5J2000[3], fk5[3];
      ICRSToFK5J2000Matrix(matrix);
      eraRxp(matrix, const_cast<double*>(icrs), fk5J2000);
      FK5PrecessionMatrix(2000.0, equinox, matrix);
      eraRxp(matrix, fk5J2000, fk5);
      FromVector(fk5, result);
      return true;
      }
    case vtkAstroFrames::FK4:
    case vtkAstroFrames::FK4NoETerms:
      {
      double fk5J2000[3];
      ICRSToFK5J2000Matrix(matrix);
      eraRxp(matrix, const_cast<double*>(icrs), fk5J2000);
      double ra2000 = 0., dec2000 = 0., ra1950 = 0., dec1950 = 0., dra = 0., ddec = 0.;
      eraC2s(fk5J2000, &ra2000, &dec2000);
      eraFk54z(ra2000, dec2000, fk4Epoch, &ra1950, &dec1950, &dra, &ddec);
      double b1950[3], vector[3];
      eraS2c(ra1950, dec1950, b1950);
      RemoveETerms(b1950, 1950.0);
      NewcombPrecessionMatrix(1950.0, equinox, matrix);
      eraRxp(matrix, b1950, vector);
      if (frame == vtkAstroFrames::FK4)
        {
        AddETerms(vector, equinox);
        }
      FromVector(vector, result);
      return true;
      }
    case vtkAstroFrames::Galactic:
    case vtkAstroFrames::Supergalactic:
      {
      double fk5J2000[3], galactic[3];
      ICRSToFK5J2000Matrix(matrix);
      eraRxp(matrix, const_cast<double*>(icrs), fk5J2000);
      FK5J2000ToGalacticMatrix(matrix);
      eraRxp(matrix, fk5J2000, galactic);
      if (frame == vtkAstroFrames::Supergalactic)
        {
        double supergalactic[3];
        GalacticToSupergalacticMatrix(matrix);
        eraRxp(matrix, galactic, supergalactic);
        FromVector(supergalactic, result);
        }
      else
        {
        FromVector(galactic, result);
        }
      return true;
      }
    default:
      return false;
    }
}

//----------------------------------------------------------------------------
// relativistic Doppler factor of an observer moving towards the source at
// velocity (m/s)
double DopplerFactor(double velocity)
{
  const double beta = velocity / SPEED_OF_LIGHT;
  return std::sqrt((1. + beta) / (1. - beta));
}

//----------------------------------------------------------------------------
double Projection(const double velocity, const double direction[3], const double source[3])
{
  return velocity * (direction[0] * source[0] + direction[1] * source[1] + direction[2] * source[2]);
}

//----------------------------------------------------------------------------
// velocities (m/s, towards the source) of the observer of the steps from
// the barycentre to the frame (one or two steps)
bool FrameSteps(int frame, double ra, double dec, double mjdUTC, const double observatory[3],
                double steps[2], int& numberOfSteps)
{
  numberOfSteps = 0;
  // direction of the source: ICRS, and FK5 J2000 for the casacore vectors
  double source[3], sourceFK5[3], bias[3][3];
  ToVector(ra, dec, source);
  ICRSToFK5J2000Matrix(bias);
  eraRxp(bias, source, sourceFK5);

  switch (frame)
    {
    case vtkAstroFrames::Barycentric:
      return true;
    case vtkAstroFrames::LSRK:
      steps[numberOfSteps++] = -Projection(LSRK_VELOCITY, LSRK_DIRECTION, sourceFK5);
      return true;
    case vtkAstroFrames::LSRD:
      steps[numberOfSteps++] = -Projection(LSRD_VELOCITY, LSRD_DIRECTION, sourceFK5);
      return true;
    case vtkAstroFrames::Galactocentric:
      steps[numberOfSteps++] = -Projection(LSRD_VELOCITY, LSRD_DIRECTION, sourceFK5);
      steps[numberOfSteps++] = -Projection(GALACTIC_ROTATION_VELOCITY, GALACTIC_ROTATION_DIRECTION, sourceFK5);
      return true;
    case vtkAstroFrames::LocalGroup:
      steps[numberOfSteps++] = -Projection(LOCAL_GROUP_VELOCITY, LOCAL_GROUP_DIRECTION, sourceFK5);
      return true;
    case vtkAstroFrames::CMB:
      steps[numberOfSteps++] = -Projection(CMB_VELOCITY, CMB_DIRECTION, sourceFK5);
      return true;
    case vtkAstroFrames::Geocentric:
    case vtkAstroFrames::Topocentric:
      {
      if (!(mjdUTC > 0.))
        {
        return false;
        }
      // time scales (UT1 ~ UTC and TDB ~ TT: errors below 1 mm/s)
      double tai1 = 0., tai2 = 0., tt1 = 0., tt2 = 0.;
      if (eraUtctai(2400000.5, mjdUTC, &tai1, &tai2) < 0 || eraTaitt(tai1, tai2, &tt1, &tt2))
        {
        return false;
        }
      // barycentric velocity of the Earth (au/day, BCRS axes)
      double heliocentric[2][3], barycentric[2][3];
      eraEpv00(tt1, tt2, heliocentric, barycentric);
      const double auPerDayToMetresPerSecond = ERFA_DAU / ERFA_DAYSEC;
      double earth[3];
      for (int index = 0; index < 3; index++)
        {
        earth[index] = barycentric[1][index] * auPerDayToMetresPerSecond;
        }
      steps[numberOfSteps++] = eraPdp(earth, source);
      if (frame == vtkAstroFrames::Geocentric)
        {
        return true;
        }
      // rotation of the Earth at the observatory (ITRF -> CIRS -> GCRS)
      if (!observatory)
        {
        return false;
        }
      double longitude = 0., latitude = 0., height = 0.;
      double itrf[3] = {observatory[0], observatory[1], observatory[2]};
      if (eraGc2gd(ERFA_WGS84, itrf, &longitude, &latitude, &height))
        {
        return false;
        }
      const double earthRotationAngle = eraEra00(2400000.5, mjdUTC);
      double cirs[2][3];
      eraPvtob(longitude, latitude, height, 0., 0., 0., earthRotationAngle, cirs);
      double gcrsToCirs[3][3], site[3];
      eraC2i06a(tt1, tt2, gcrsToCirs);
      eraTrxp(gcrsToCirs, cirs[1], site);
      steps[numberOfSteps++] = eraPdp(site, source);
      return true;
      }
    default:
      return false;
    }
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
void vtkAstroFrames::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
int vtkAstroFrames::GetCelestialFrame(const char* longitudeCType, const char* radesys,
                                      double equinox, double& equinoxOut)
{
  equinoxOut = 0.;
  const std::string ctype = UpperCase(longitudeCType);
  if (ctype.compare(0, 4, "GLON") == 0)
    {
    return vtkAstroFrames::Galactic;
    }
  if (ctype.compare(0, 4, "SLON") == 0)
    {
    return vtkAstroFrames::Supergalactic;
    }
  if (ctype.compare(0, 2, "RA") != 0)
    {
    return vtkAstroFrames::CelestialUnknown;
    }
  std::string system = UpperCase(radesys);
  const bool hasEquinox = equinox > 0.;
  if (system.empty())
    {
    system = !hasEquinox ? "ICRS" : (equinox < 1984.0 ? "FK4" : "FK5");
    }
  if (system == "ICRS")
    {
    return vtkAstroFrames::ICRS;
    }
  if (system == "FK5")
    {
    equinoxOut = hasEquinox ? equinox : 2000.0;
    return vtkAstroFrames::FK5;
    }
  if (system == "FK4" || system == "FK4-NO-E")
    {
    equinoxOut = hasEquinox ? equinox : 1950.0;
    return system == "FK4" ? vtkAstroFrames::FK4 : vtkAstroFrames::FK4NoETerms;
    }
  return vtkAstroFrames::CelestialUnknown;
}

//----------------------------------------------------------------------------
const char* vtkAstroFrames::GetCelestialFrameName(int frame)
{
  switch (frame)
    {
    case vtkAstroFrames::ICRS: return "ICRS";
    case vtkAstroFrames::FK5: return "FK5";
    case vtkAstroFrames::FK4: return "FK4";
    case vtkAstroFrames::FK4NoETerms: return "FK4-NO-E";
    case vtkAstroFrames::Galactic: return "Galactic";
    case vtkAstroFrames::Supergalactic: return "Supergalactic";
    default: return "unknown";
    }
}

//----------------------------------------------------------------------------
bool vtkAstroFrames::ConvertCelestial(double longitude, double latitude,
                                      int fromFrame, double fromEquinox,
                                      int toFrame, double toEquinox,
                                      double epochMJD, double result[2])
{
  if (fromFrame == toFrame && (fromFrame == vtkAstroFrames::ICRS || fromFrame == vtkAstroFrames::Galactic ||
                               fromFrame == vtkAstroFrames::Supergalactic || fromEquinox == toEquinox))
    {
    double vector[3];
    ToVector(longitude, latitude, vector);
    FromVector(vector, result);
    return true;
    }
  // between FK4 frames: precession (Newcomb) in FK4, as astropy (through
  // FK5 the rotation of FK4 between the epochs would be applied)
  if (IsFK4(fromFrame) && IsFK4(toFrame))
    {
    double vector[3], precessed[3], matrix[3][3];
    ToVector(longitude, latitude, vector);
    if (fromFrame == vtkAstroFrames::FK4)
      {
      RemoveETerms(vector, fromEquinox);
      }
    NewcombPrecessionMatrix(fromEquinox, toEquinox, matrix);
    eraRxp(matrix, vector, precessed);
    if (toFrame == vtkAstroFrames::FK4)
      {
      AddETerms(precessed, toEquinox);
      }
    FromVector(precessed, result);
    return true;
    }
  // epoch of the observation for FK4 <-> FK5: given, or the equinox of the
  // FK4 frame (as astropy)
  double fk4Epoch = 1950.0;
  if (epochMJD > 0.)
    {
    fk4Epoch = eraEpb(2400000.5, epochMJD);
    }
  else if (IsFK4(fromFrame))
    {
    fk4Epoch = fromEquinox;
    }
  else if (IsFK4(toFrame))
    {
    fk4Epoch = toEquinox;
    }
  double icrs[3];
  return ToICRS(longitude, latitude, fromFrame, fromEquinox, fk4Epoch, icrs) &&
         FromICRS(icrs, toFrame, toEquinox, fk4Epoch, result);
}

//----------------------------------------------------------------------------
int vtkAstroFrames::GetSpectralFrame(const char* specsys)
{
  const std::string system = UpperCase(specsys);
  if (system == "TOPOCENT" || system == "TOPO")
    {
    return vtkAstroFrames::Topocentric;
    }
  if (system == "GEOCENTR" || system == "GEOCENT" || system == "GEO")
    {
    return vtkAstroFrames::Geocentric;
    }
  if (system == "BARYCENT" || system == "HELIOCEN" || system == "BARY" || system == "HEL" || system == "HELIO")
    {
    return vtkAstroFrames::Barycentric;
    }
  if (system == "LSRK" || system == "LSR")
    {
    return vtkAstroFrames::LSRK;
    }
  if (system == "LSRD")
    {
    return vtkAstroFrames::LSRD;
    }
  if (system == "GALACTOC" || system == "GALACTO")
    {
    return vtkAstroFrames::Galactocentric;
    }
  if (system == "LOCALGRP" || system == "LGROUP")
    {
    return vtkAstroFrames::LocalGroup;
    }
  if (system == "CMBDIPOL" || system == "CMB")
    {
    return vtkAstroFrames::CMB;
    }
  return vtkAstroFrames::SpectralUnknown;
}

//----------------------------------------------------------------------------
const char* vtkAstroFrames::GetSpectralFrameName(int frame)
{
  switch (frame)
    {
    case vtkAstroFrames::Topocentric: return "TOPOCENT";
    case vtkAstroFrames::Geocentric: return "GEOCENTR";
    case vtkAstroFrames::Barycentric: return "BARYCENT";
    case vtkAstroFrames::LSRK: return "LSRK";
    case vtkAstroFrames::LSRD: return "LSRD";
    case vtkAstroFrames::Galactocentric: return "GALACTOC";
    case vtkAstroFrames::LocalGroup: return "LOCALGRP";
    case vtkAstroFrames::CMB: return "CMBDIPOL";
    default: return "unknown";
    }
}

//----------------------------------------------------------------------------
bool vtkAstroFrames::GetFrameVelocity(int frame, double ra, double dec, double mjdUTC,
                                      const double observatory[3], double& velocity)
{
  double steps[2] = {0., 0.};
  int numberOfSteps = 0;
  if (!FrameSteps(frame, ra, dec, mjdUTC, observatory, steps, numberOfSteps))
    {
    return false;
    }
  velocity = 0.;
  for (int step = 0; step < numberOfSteps; step++)
    {
    velocity += steps[step];
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkAstroFrames::ConvertFrequency(double frequency, int fromFrame, int toFrame,
                                      double ra, double dec, double mjdUTC,
                                      const double observatory[3], double& result)
{
  if (fromFrame == toFrame)
    {
    result = frequency;
    return fromFrame != vtkAstroFrames::SpectralUnknown;
    }
  double fromSteps[2] = {0., 0.}, toSteps[2] = {0., 0.};
  int numberOfFromSteps = 0, numberOfToSteps = 0;
  if (!FrameSteps(fromFrame, ra, dec, mjdUTC, observatory, fromSteps, numberOfFromSteps) ||
      !FrameSteps(toFrame, ra, dec, mjdUTC, observatory, toSteps, numberOfToSteps))
    {
    return false;
    }
  // to the barycentre, then to the frame
  double barycentric = frequency;
  for (int step = 0; step < numberOfFromSteps; step++)
    {
    barycentric /= DopplerFactor(fromSteps[step]);
    }
  result = barycentric;
  for (int step = 0; step < numberOfToSteps; step++)
    {
    result *= DopplerFactor(toSteps[step]);
    }
  return true;
}

//----------------------------------------------------------------------------
double vtkAstroFrames::BesselianEpoch(double mjd)
{
  return eraEpb(2400000.5, mjd);
}

//----------------------------------------------------------------------------
double vtkAstroFrames::JulianEpoch(double mjd)
{
  return eraEpj(2400000.5, mjd);
}
