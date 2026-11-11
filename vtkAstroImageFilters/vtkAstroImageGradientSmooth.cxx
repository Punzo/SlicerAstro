/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#include "vtkAstroImageGradientSmooth.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkSMPTools.h>

// STD includes
#include <cstring>
#include <memory>
#include <functional>

vtkStandardNewMacro(vtkAstroImageGradientSmooth);

//----------------------------------------------------------------------------
vtkAstroImageGradientSmooth::vtkAstroImageGradientSmooth() = default;

//----------------------------------------------------------------------------
vtkAstroImageGradientSmooth::~vtkAstroImageGradientSmooth() = default;

//----------------------------------------------------------------------------
void vtkAstroImageGradientSmooth::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Conductance: " << this->Conductance[0] << " " << this->Conductance[1]
     << " " << this->Conductance[2] << "\n";
  os << indent << "TimeStep: " << this->TimeStep << "\n";
  os << indent << "K: " << this->K << "\n";
  os << indent << "Noise: " << this->Noise << "\n";
  os << indent << "NumberOfIterations: " << this->NumberOfIterations << "\n";
}

namespace
{
//----------------------------------------------------------------------------
template <typename T, typename Progress, typename Abort>
void GradientIteration(const T* src, T* dst, const int dims[3], const double conductance[3],
                       double timeStep, double noise2, Progress&& addProgress, Abort&& isAborting)
{
  const vtkIdType nx = dims[0], ny = dims[1], nz = dims[2];
  const vtkIdType nxy = nx * ny;
  vtkSMPTools::For(0, nz, [&](vtkIdType zFirst, vtkIdType zLast)
    {
    for (vtkIdType z = zFirst; z < zLast; ++z)
      {
      if (isAborting())
        {
        return;
        }
      // reflecting borders: the missing neighbour is the voxel itself
      const vtkIdType dzMinus = (z > 0) ? -nxy : 0;
      const vtkIdType dzPlus = (z < nz - 1) ? nxy : 0;
      for (vtkIdType y = 0; y < ny; ++y)
        {
        const vtkIdType dyMinus = (y > 0) ? -nx : 0;
        const vtkIdType dyPlus = (y < ny - 1) ? nx : 0;
        for (vtkIdType x = 0; x < nx; ++x)
          {
          const vtkIdType index = z * nxy + y * nx + x;
          const vtkIdType dxMinus = (x > 0) ? -1 : 0;
          const vtkIdType dxPlus = (x < nx - 1) ? 1 : 0;
          const double v = src[index];
          const double x1 = src[index + dxMinus], x2 = src[index + dxPlus];
          const double y1 = src[index + dyMinus], y2 = src[index + dyPlus];
          const double z1 = src[index + dzMinus], z2 = src[index + dzPlus];
          if (v != v || x1 != x1 || x2 != x2 || y1 != y1 || y2 != y2 || z1 != z1 || z2 != z2)
            {
            dst[index] = src[index]; // blank voxel or blank neighbour: unchanged
            continue;
            }
          const double norm = 1. + (v * v) / noise2;
          const double cX = ((x1 - v) + (x2 - v)) * conductance[0];
          const double cY = ((y1 - v) + (y2 - v)) * conductance[1];
          const double cZ = ((z1 - v) + (z2 - v)) * conductance[2];
          dst[index] = static_cast<T>(v + timeStep * (cX + cY + cZ) / norm);
          }
        }
      }
    addProgress(static_cast<double>(zLast - zFirst));
    });
}

//----------------------------------------------------------------------------
template <typename T, typename Progress, typename Abort>
void GradientSmooth(const T* in, T* out, const int dims[3], const double conductance[3],
                    double timeStep, double noise2, int iterations,
                    Progress&& addProgress, Abort&& isAborting)
{
  const vtkIdType numberOfVoxels = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2];
  if (iterations <= 0)
    {
    std::memcpy(out, in, sizeof(T) * numberOfVoxels);
    return;
    }
  // Ping-pong between the output and one scratch buffer; the first iteration
  // reads the input directly and the last one writes into the output.
  std::unique_ptr<T[]> scratch;
  if (iterations > 1)
    {
    scratch.reset(new T[numberOfVoxels]);
    }
  const T* src = in;
  for (int iteration = 0; iteration < iterations; ++iteration)
    {
    const int remaining = iterations - iteration;
    T* dst = (remaining % 2 == 1) ? out : scratch.get();
    GradientIteration(src, dst, dims, conductance, timeStep, noise2, addProgress, isAborting);
    if (isAborting())
      {
      return;
      }
    src = dst;
    }
}
} // namespace

//----------------------------------------------------------------------------
bool vtkAstroImageGradientSmooth::Smooth(vtkImageData* input, vtkImageData* output)
{
  int dims[3];
  input->GetDimensions(dims);
  const double noise2 = this->Noise * this->Noise * this->K * this->K;
  if (noise2 <= 0.)
    {
    vtkErrorMacro("Smooth: Noise and K must be positive.");
    return false;
    }
  const int iterations = this->NumberOfIterations;
  this->ResetProgress(static_cast<double>(dims[2]) * (iterations > 0 ? iterations : 1));
  std::function<void(double)> addProgress = [this](double work) { this->AddProgress(work); };
  std::function<bool()> isAborting = [this]() { return this->IsAborting(); };

  switch (input->GetScalarType())
    {
    case VTK_FLOAT:
      GradientSmooth(static_cast<const float*>(input->GetScalarPointer()),
                     static_cast<float*>(output->GetScalarPointer()), dims, this->Conductance,
                     this->TimeStep, noise2, iterations, addProgress, isAborting);
      break;
    case VTK_DOUBLE:
      GradientSmooth(static_cast<const double*>(input->GetScalarPointer()),
                     static_cast<double*>(output->GetScalarPointer()), dims, this->Conductance,
                     this->TimeStep, noise2, iterations, addProgress, isAborting);
      break;
    default:
      return false;
    }
  return true;
}
