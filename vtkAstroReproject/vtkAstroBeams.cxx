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

#include "vtkAstroBeams.h"

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <vector>

vtkStandardNewMacro(vtkAstroBeams);

namespace
{
const double DEG2RAD = vtkMath::Pi() / 180.;

struct Beam
{
  double Major = 0.;
  double Minor = 0.;
  double PA = 0.; // radians
  bool IsNull() const { return !(this->Major > 0.) || !(this->Minor > 0.); }
  double Area() const { return vtkAstroBeams::Area(this->Major, this->Minor); }
  bool operator==(const Beam& other) const
    {
    return this->Major == other.Major && this->Minor == other.Minor && this->PA == other.PA;
    }
};

Beam ToBeam(const double beam[3])
{
  Beam result;
  result.Major = std::max(beam[0], beam[1]);
  result.Minor = std::min(beam[0], beam[1]);
  result.PA = beam[2] * DEG2RAD + (beam[1] > beam[0] ? vtkMath::Pi() / 2. : 0.);
  return result;
}

//----------------------------------------------------------------------------
// position angle in (-90, 90] degrees (radians here)
double UnwrapPA(double pa)
{
  const double pi = vtkMath::Pi();
  pa = std::fmod(pa, pi);
  if (pa > pi / 2.)
    {
    pa -= pi;
    }
  else if (pa <= -pi / 2.)
    {
    pa += pi;
    }
  return pa;
}

void FromBeam(const Beam& beam, double result[3])
{
  result[0] = beam.Major;
  result[1] = beam.Minor;
  result[2] = UnwrapPA(beam.PA) / DEG2RAD;
}

//----------------------------------------------------------------------------
// GaussianDeconvolver::deconvolve (CASA): Wild (1970)
int DeconvolveBeams(const Beam& convolved, const Beam& beam, Beam& result)
{
  const double majorSource = convolved.Major, minorSource = convolved.Minor, thetaSource = convolved.PA;
  const double majorBeam = beam.Major, minorBeam = beam.Minor, thetaBeam = beam.PA;
  const double alpha = std::pow(majorSource * std::cos(thetaSource), 2) + std::pow(minorSource * std::sin(thetaSource), 2) -
                       std::pow(majorBeam * std::cos(thetaBeam), 2) - std::pow(minorBeam * std::sin(thetaBeam), 2);
  const double beta = std::pow(majorSource * std::sin(thetaSource), 2) + std::pow(minorSource * std::cos(thetaSource), 2) -
                      std::pow(majorBeam * std::sin(thetaBeam), 2) - std::pow(minorBeam * std::cos(thetaBeam), 2);
  const double gamma = 2. * ((minorSource * minorSource - majorSource * majorSource) * std::sin(thetaSource) * std::cos(thetaSource) -
                             (minorBeam * minorBeam - majorBeam * majorBeam) * std::sin(thetaBeam) * std::cos(thetaBeam));
  const double s = alpha + beta;
  const double t = std::sqrt((alpha - beta) * (alpha - beta) + gamma * gamma);
  double limit = std::min(std::min(majorSource, minorSource), std::min(majorBeam, minorBeam));
  limit = 0.1 * limit * limit;
  if (alpha < 0. || beta < 0. || s < t)
    {
    if (0.5 * (s - t) < limit && alpha > -limit && beta > -limit)
      {
      result = beam;
      return vtkAstroBeams::PointSource;
      }
    return vtkAstroBeams::NotDeconvolvable;
    }
  result.Major = std::sqrt(0.5 * (s + t));
  result.Minor = std::sqrt(0.5 * (s - t));
  result.PA = (std::fabs(gamma) + std::fabs(alpha - beta) == 0.) ? 0. : 0.5 * std::atan2(-gamma, alpha - beta);
  result.PA = UnwrapPA(result.PA);
  return vtkAstroBeams::Deconvolved;
}

//----------------------------------------------------------------------------
// Scale an ellipse (axes major and minor, major axis at angle pa from the x
// axis) by xScale along x and yScale along y: as
// CasaImageBeamSet::_transformEllipseByScaling, by the eigen decomposition
// of the quadratic form of the ellipse.
void TransformEllipseByScaling(double& major, double& minor, double& pa,
                               double inMajor, double inMinor, double inPA, double xScale, double yScale)
{
  const double c = std::cos(inPA), s = std::sin(inPA);
  const double inverseMajor2 = 1. / (inMajor * inMajor), inverseMinor2 = 1. / (inMinor * inMinor);
  // quadratic form [[A, B], [B, C]] of the ellipse, x' = xScale x, y' = yScale y
  const double A = (c * c * inverseMajor2 + s * s * inverseMinor2) / (xScale * xScale);
  const double B = (c * s * (inverseMajor2 - inverseMinor2)) / (xScale * yScale);
  const double C = (s * s * inverseMajor2 + c * c * inverseMinor2) / (yScale * yScale);
  const double mean = 0.5 * (A + C);
  const double radius = std::sqrt(0.25 * (A - C) * (A - C) + B * B);
  const double small = mean - radius; // eigenvalue of the major axis
  const double large = mean + radius;
  major = 1. / std::sqrt(small);
  minor = 1. / std::sqrt(large);
  // eigenvector of the larger eigenvalue at 0.5 atan2(2B, A - C): the major
  // axis is perpendicular to it
  pa = (radius == 0.) ? 0. : UnwrapPA(0.5 * std::atan2(2. * B, A - C) + vtkMath::Pi() / 2.);
}

//----------------------------------------------------------------------------
bool Deconvolvable(const Beam& large, const Beam& small)
{
  Beam unused;
  return DeconvolveBeams(large, small, unused) == vtkAstroBeams::Deconvolved;
}

//----------------------------------------------------------------------------
// CasaImageBeamSet::getCommonBeam (iterative)
bool CommonBeamOf(std::vector<Beam> beams, Beam& result)
{
  std::vector<Beam> valid;
  for (const Beam& beam : beams)
    {
    if (!beam.IsNull())
      {
      valid.push_back(beam);
      }
    }
  if (valid.empty())
    {
    return false;
    }
  // (CASA recurses; each iteration replaces the largest beam by a larger
  // enclosing one: bounded)
  for (int iteration = 0; iteration < 1000; iteration++)
    {
    bool allEqual = true;
    for (const Beam& beam : valid)
      {
      allEqual = allEqual && beam == valid[0];
      }
    if (allEqual)
      {
      result = valid[0];
      return true;
      }
    std::size_t maxIndex = 0;
    for (std::size_t index = 1; index < valid.size(); index++)
      {
      if (valid[index].Area() > valid[maxIndex].Area())
        {
        maxIndex = index;
        }
      }
    const Beam maxBeam = valid[maxIndex];
    bool largestBeamWorks = true;
    Beam problemBeam;
    for (const Beam& beam : valid)
      {
      if (!(beam == maxBeam) && !Deconvolvable(maxBeam, beam))
        {
        largestBeamWorks = false;
        problemBeam = beam;
        }
      }
    if (largestBeamWorks)
      {
      result = maxBeam;
      return true;
      }

    // rotate so that the major axis of the largest beam (A) is along x
    const double tB1 = problemBeam.PA - maxBeam.PA;
    if (std::fabs(std::fabs(tB1) - vtkMath::Pi() / 2.) < 1.e-12)
      {
      // right angles: the major axes of the two beams
      const bool maxHasMajor = maxBeam.Major >= problemBeam.Major;
      result.Major = maxHasMajor ? maxBeam.Major : problemBeam.Major;
      result.Minor = maxHasMajor ? problemBeam.Major : maxBeam.Major;
      result.PA = maxHasMajor ? maxBeam.PA : problemBeam.PA;
      // (CASA returns here)
      return true;
      }
    // squeeze along x and stretch along y: A becomes a circle (same area)
    const double aA2 = std::sqrt(maxBeam.Major * maxBeam.Minor);
    const double p = aA2 / maxBeam.Major;
    const double q = aA2 / maxBeam.Minor;
    double aB2 = 0., bB2 = 0., tB2 = 0.;
    TransformEllipseByScaling(aB2, bB2, tB2, problemBeam.Major, problemBeam.Minor, tB1, p, q);
    // enclosing ellipse C: major axis of B, minor axis the radius of A
    double aC1 = 0., bC1 = 0., tC1 = 0.;
    TransformEllipseByScaling(aC1, bC1, tC1, std::max(aB2, aA2), aA2, tB2, 1. / p, 1. / q);
    Beam enclosing;
    enclosing.Major = aC1;
    enclosing.Minor = bC1;
    enclosing.PA = UnwrapPA(tC1 + maxBeam.PA);
    // grow by 0.1% until both beams can be deconvolved
    bool ok = false;
    for (int retry = 0; retry <= 1000; retry++)
      {
      if (Deconvolvable(enclosing, maxBeam) && Deconvolvable(enclosing, problemBeam))
        {
        ok = true;
        break;
        }
      enclosing.Major *= 1.001;
      enclosing.Minor *= 1.001;
      }
    if (!ok)
      {
      result = maxBeam;
      return true;
      }
    valid[maxIndex] = enclosing;
    }
  return false;
}
} // end of anonymous namespace

//----------------------------------------------------------------------------
void vtkAstroBeams::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
double vtkAstroBeams::Area(double major, double minor)
{
  return vtkMath::Pi() / (4. * std::log(2.)) * major * minor;
}

//----------------------------------------------------------------------------
int vtkAstroBeams::Deconvolve(const double convolved[3], const double beam[3], double result[3])
{
  Beam source;
  const int status = DeconvolveBeams(ToBeam(convolved), ToBeam(beam), source);
  if (status == vtkAstroBeams::NotDeconvolvable)
    {
    result[0] = result[1] = result[2] = 0.;
    }
  else
    {
    FromBeam(source, result);
    }
  return status;
}

//----------------------------------------------------------------------------
void vtkAstroBeams::Convolve(const double beam1[3], const double beam2[3], double result[3])
{
  // sum of the covariances: as the deconvolution formula with + signs
  const Beam a = ToBeam(beam1), b = ToBeam(beam2);
  const double alpha = std::pow(a.Major * std::cos(a.PA), 2) + std::pow(a.Minor * std::sin(a.PA), 2) +
                       std::pow(b.Major * std::cos(b.PA), 2) + std::pow(b.Minor * std::sin(b.PA), 2);
  const double beta = std::pow(a.Major * std::sin(a.PA), 2) + std::pow(a.Minor * std::cos(a.PA), 2) +
                      std::pow(b.Major * std::sin(b.PA), 2) + std::pow(b.Minor * std::cos(b.PA), 2);
  const double gamma = 2. * ((a.Minor * a.Minor - a.Major * a.Major) * std::sin(a.PA) * std::cos(a.PA) +
                             (b.Minor * b.Minor - b.Major * b.Major) * std::sin(b.PA) * std::cos(b.PA));
  const double s = alpha + beta;
  const double t = std::sqrt((alpha - beta) * (alpha - beta) + gamma * gamma);
  Beam convolved;
  convolved.Major = std::sqrt(0.5 * (s + t));
  convolved.Minor = std::sqrt(std::max(0., 0.5 * (s - t)));
  convolved.PA = (std::fabs(gamma) + std::fabs(alpha - beta) == 0.) ? 0. : 0.5 * std::atan2(-gamma, alpha - beta);
  FromBeam(convolved, result);
}

//----------------------------------------------------------------------------
bool vtkAstroBeams::CommonBeam(vtkDoubleArray* beams, double result[3])
{
  if (!beams || beams->GetNumberOfComponents() != 3)
    {
    return false;
    }
  std::vector<Beam> values;
  for (vtkIdType index = 0; index < beams->GetNumberOfTuples(); index++)
    {
    double tuple[3];
    beams->GetTuple(index, tuple);
    values.push_back(ToBeam(tuple));
    }
  Beam common;
  if (!CommonBeamOf(values, common))
    {
    return false;
    }
  FromBeam(common, result);
  return true;
}

//----------------------------------------------------------------------------
bool vtkAstroBeams::ConvolvingBeam(const double target[3], const double input[3], double kernel[3])
{
  Beam source;
  const int status = DeconvolveBeams(ToBeam(target), ToBeam(input), source);
  if (status == vtkAstroBeams::PointSource)
    {
    // target (nearly) equal to input: no convolution
    kernel[0] = kernel[1] = kernel[2] = 0.;
    return true;
    }
  if (status != vtkAstroBeams::Deconvolved)
    {
    return false;
    }
  FromBeam(source, kernel);
  return true;
}
