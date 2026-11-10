/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#include "vtkAstroImageKernelSmooth.h"

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSMPTools.h>

// STD includes
#include <algorithm>

vtkStandardNewMacro(vtkAstroImageKernelSmooth);

//----------------------------------------------------------------------------
vtkAstroImageKernelSmooth::vtkAstroImageKernelSmooth()
{
  this->Kernel = vtkSmartPointer<vtkDoubleArray>::New();
  this->Kernel->InsertNextValue(1.);
}

//----------------------------------------------------------------------------
vtkAstroImageKernelSmooth::~vtkAstroImageKernelSmooth() = default;

//----------------------------------------------------------------------------
void vtkAstroImageKernelSmooth::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "KernelDimensions: " << this->KernelDimensions[0] << " "
     << this->KernelDimensions[1] << " " << this->KernelDimensions[2] << "\n";
}

//----------------------------------------------------------------------------
void vtkAstroImageKernelSmooth::SetKernel(vtkDoubleArray* kernel, int dimX, int dimY, int dimZ)
{
  if (!kernel || dimX < 1 || dimY < 1 || dimZ < 1 ||
      kernel->GetNumberOfTuples() != static_cast<vtkIdType>(dimX) * dimY * dimZ)
    {
    vtkErrorMacro("SetKernel: invalid kernel or kernel dimensions.");
    return;
    }
  this->Kernel = kernel;
  this->KernelDimensions[0] = dimX;
  this->KernelDimensions[1] = dimY;
  this->KernelDimensions[2] = dimZ;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkDoubleArray* vtkAstroImageKernelSmooth::GetKernel()
{
  return this->Kernel;
}

//----------------------------------------------------------------------------
void vtkAstroImageKernelSmooth::SetBoxKernel(int sizeX, int sizeY, int sizeZ)
{
  auto odd = [](int size) { size = std::max(size, 1); return size % 2 == 0 ? size + 1 : size; };
  const int dims[3] = {odd(sizeX), odd(sizeY), odd(sizeZ)};
  const vtkIdType n = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2];
  vtkNew<vtkDoubleArray> kernel;
  kernel->SetNumberOfValues(n);
  kernel->Fill(1. / n);
  this->SetKernel(kernel, dims[0], dims[1], dims[2]);
}

namespace
{
//----------------------------------------------------------------------------
template <typename T, typename Progress, typename Abort>
void KernelSmooth(const T* in, T* out, const int dims[3],
                  const double* kernel, const int kernelDims[3],
                  Progress&& addProgress, Abort&& isAborting)
{
  const vtkIdType nx = dims[0], ny = dims[1], nz = dims[2];
  const vtkIdType nxy = nx * ny;
  const int rx = (kernelDims[0] - 1) / 2, ry = (kernelDims[1] - 1) / 2, rz = (kernelDims[2] - 1) / 2;
  const vtkIdType kx = kernelDims[0], kxy = static_cast<vtkIdType>(kernelDims[0]) * kernelDims[1];

  vtkSMPTools::For(0, nz, [&](vtkIdType zFirst, vtkIdType zLast)
    {
    for (vtkIdType z = zFirst; z < zLast; ++z)
      {
      if (isAborting())
        {
        return;
        }
      // kernel z range clipped to the image (zero padding)
      const int kzMin = static_cast<int>(std::max<vtkIdType>(-rz, -z));
      const int kzMax = static_cast<int>(std::min<vtkIdType>(rz, nz - 1 - z));
      for (vtkIdType y = 0; y < ny; ++y)
        {
        const int kyMin = static_cast<int>(std::max<vtkIdType>(-ry, -y));
        const int kyMax = static_cast<int>(std::min<vtkIdType>(ry, ny - 1 - y));
        for (vtkIdType x = 0; x < nx; ++x)
          {
          const int kxMin = static_cast<int>(std::max<vtkIdType>(-rx, -x));
          const int kxMax = static_cast<int>(std::min<vtkIdType>(rx, nx - 1 - x));
          double sum = 0.;
          for (int k = kzMin; k <= kzMax; ++k)
            {
            const T* inSlice = in + (z + k) * nxy;
            const double* kernelSlice = kernel + (k + rz) * kxy;
            for (int j = kyMin; j <= kyMax; ++j)
              {
              const T* inRow = inSlice + (y + j) * nx + x;
              const double* kernelRow = kernelSlice + (j + ry) * kx + rx;
              for (int i = kxMin; i <= kxMax; ++i)
                {
                const T value = inRow[i];
                if (value == value) // skip NaN
                  {
                  sum += value * kernelRow[i];
                  }
                }
              }
            }
          out[z * nxy + y * nx + x] = static_cast<T>(sum);
          }
        }
      }
    addProgress(static_cast<double>(zLast - zFirst));
    });
}
} // namespace

//----------------------------------------------------------------------------
bool vtkAstroImageKernelSmooth::Smooth(vtkImageData* input, vtkImageData* output)
{
  int dims[3];
  input->GetDimensions(dims);
  const double* kernel = this->Kernel->GetPointer(0);
  this->ResetProgress(dims[2]);
  auto addProgress = [this](double work) { this->AddProgress(work); };
  auto isAborting = [this]() { return this->IsAborting(); };

  switch (input->GetScalarType())
    {
    case VTK_FLOAT:
      KernelSmooth(static_cast<const float*>(input->GetScalarPointer()),
                   static_cast<float*>(output->GetScalarPointer()), dims, kernel,
                   this->KernelDimensions, addProgress, isAborting);
      break;
    case VTK_DOUBLE:
      KernelSmooth(static_cast<const double*>(input->GetScalarPointer()),
                   static_cast<double*>(output->GetScalarPointer()), dims, kernel,
                   this->KernelDimensions, addProgress, isAborting);
      break;
    default:
      return false;
    }
  return true;
}
