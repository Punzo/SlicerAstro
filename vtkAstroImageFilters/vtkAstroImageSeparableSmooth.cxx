/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#include "vtkAstroImageSeparableSmooth.h"

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSMPTools.h>

// STD includes
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>
#include <functional>

vtkStandardNewMacro(vtkAstroImageSeparableSmooth);

//----------------------------------------------------------------------------
vtkAstroImageSeparableSmooth::vtkAstroImageSeparableSmooth() = default;

//----------------------------------------------------------------------------
vtkAstroImageSeparableSmooth::~vtkAstroImageSeparableSmooth() = default;

//----------------------------------------------------------------------------
void vtkAstroImageSeparableSmooth::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  for (int axis = 0; axis < 3; ++axis)
    {
    os << indent << "AxisKernel" << axis << " length: "
       << (this->AxisKernels[axis] ? this->AxisKernels[axis]->GetNumberOfTuples() : 0) << "\n";
    }
}

//----------------------------------------------------------------------------
void vtkAstroImageSeparableSmooth::SetAxisKernel(int axis, vtkDoubleArray* kernel)
{
  if (axis < 0 || axis > 2)
    {
    vtkErrorMacro("SetAxisKernel: invalid axis " << axis);
    return;
    }
  if (kernel && kernel->GetNumberOfTuples() % 2 == 0)
    {
    vtkErrorMacro("SetAxisKernel: the kernel length must be odd.");
    return;
    }
  this->AxisKernels[axis] = kernel;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkDoubleArray* vtkAstroImageSeparableSmooth::GetAxisKernel(int axis)
{
  return (axis >= 0 && axis <= 2) ? this->AxisKernels[axis].GetPointer() : nullptr;
}

//----------------------------------------------------------------------------
void vtkAstroImageSeparableSmooth::SetAxisBoxKernel(int axis, int size)
{
  if (size <= 0)
    {
    this->SetAxisKernel(axis, nullptr);
    return;
    }
  if (size % 2 == 0)
    {
    ++size;
    }
  vtkNew<vtkDoubleArray> kernel;
  kernel->SetNumberOfValues(size);
  kernel->Fill(1. / size);
  this->SetAxisKernel(axis, kernel);
}

namespace
{
//----------------------------------------------------------------------------
// One 1D pass along 'axis' from src to dst (both with dimensions dims).
template <typename T, typename Progress, typename Abort>
void AxisPass(const T* src, T* dst, const int dims[3], int axis,
              const double* kernel, int kernelLength,
              Progress&& addProgress, Abort&& isAborting)
{
  const vtkIdType nx = dims[0], ny = dims[1], nz = dims[2];
  const vtkIdType strides[3] = {1, nx, nx * ny};
  const vtkIdType stride = strides[axis];
  const vtkIdType length = dims[axis];
  const int radius = (kernelLength - 1) / 2;

  vtkSMPTools::For(0, nz, [&](vtkIdType zFirst, vtkIdType zLast)
    {
    for (vtkIdType z = zFirst; z < zLast; ++z)
      {
      if (isAborting())
        {
        return;
        }
      for (vtkIdType y = 0; y < ny; ++y)
        {
        for (vtkIdType x = 0; x < nx; ++x)
          {
          const vtkIdType index = z * nx * ny + y * nx + x;
          const vtkIdType position = (axis == 0) ? x : (axis == 1 ? y : z);
          const int kMin = static_cast<int>(std::max<vtkIdType>(-radius, -position));
          const int kMax = static_cast<int>(std::min<vtkIdType>(radius, length - 1 - position));
          double sum = 0.;
          for (int k = kMin; k <= kMax; ++k)
            {
            const T value = src[index + k * stride];
            if (value == value) // skip NaN
              {
              sum += value * kernel[k + radius];
              }
            }
          dst[index] = static_cast<T>(sum);
          }
        }
      }
    addProgress(static_cast<double>(zLast - zFirst));
    });
}

//----------------------------------------------------------------------------
template <typename T, typename Progress, typename Abort>
void SeparableSmooth(const T* in, T* out, const int dims[3],
                     vtkSmartPointer<vtkDoubleArray> const kernels[3],
                     Progress&& addProgress, Abort&& isAborting)
{
  const vtkIdType numberOfVoxels = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2];
  std::vector<int> axes;
  for (int axis = 0; axis < 3; ++axis)
    {
    if (kernels[axis] && kernels[axis]->GetNumberOfTuples() > 0)
      {
      axes.push_back(axis);
      }
    }
  if (axes.empty())
    {
    std::memcpy(out, in, sizeof(T) * numberOfVoxels);
    return;
    }

  // Ping-pong between the output and one scratch buffer, so that the last
  // pass writes into the output (the input is only read).
  std::unique_ptr<T[]> scratch;
  if (axes.size() > 1)
    {
    scratch.reset(new T[numberOfVoxels]);
    }
  const T* src = in;
  for (size_t pass = 0; pass < axes.size(); ++pass)
    {
    const size_t remaining = axes.size() - pass; // passes left including this one
    T* dst = (remaining % 2 == 1) ? out : scratch.get();
    const int axis = axes[pass];
    AxisPass(src, dst, dims, axis, kernels[axis]->GetPointer(0),
             static_cast<int>(kernels[axis]->GetNumberOfTuples()), addProgress, isAborting);
    if (isAborting())
      {
      return;
      }
    src = dst;
    }
}
} // namespace

//----------------------------------------------------------------------------
bool vtkAstroImageSeparableSmooth::Smooth(vtkImageData* input, vtkImageData* output)
{
  int dims[3];
  input->GetDimensions(dims);
  int passes = 0;
  for (int axis = 0; axis < 3; ++axis)
    {
    passes += (this->AxisKernels[axis] && this->AxisKernels[axis]->GetNumberOfTuples() > 0) ? 1 : 0;
    }
  this->ResetProgress(static_cast<double>(dims[2]) * std::max(passes, 1));
  std::function<void(double)> addProgress = [this](double work) { this->AddProgress(work); };
  std::function<bool()> isAborting = [this]() { return this->IsAborting(); };

  switch (input->GetScalarType())
    {
    case VTK_FLOAT:
      SeparableSmooth(static_cast<const float*>(input->GetScalarPointer()),
                      static_cast<float*>(output->GetScalarPointer()), dims,
                      this->AxisKernels, addProgress, isAborting);
      break;
    case VTK_DOUBLE:
      SeparableSmooth(static_cast<const double*>(input->GetScalarPointer()),
                      static_cast<double*>(output->GetScalarPointer()), dims,
                      this->AxisKernels, addProgress, isAborting);
      break;
    default:
      return false;
    }
  return true;
}
