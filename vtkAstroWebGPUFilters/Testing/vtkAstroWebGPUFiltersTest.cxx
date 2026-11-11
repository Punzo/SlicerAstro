/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

// Compares the WebGPU smoothing filters with the CPU (vtkSMPTools) filters on
// synthetic cubes containing blank (NaN) voxels. Returns 77 (skipped) when no
// WebGPU adapter is available.

#include "vtkAstroWebGPUDevice.h"
#include "vtkAstroWebGPUImageGradientSmooth.h"
#include "vtkAstroWebGPUImageKernelSmooth.h"
#include "vtkAstroWebGPUImageSeparableSmooth.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

// STD includes
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>
#include <functional>

namespace
{
int failures = 0;
// Device shared by the double cases (the float cases create one per execution)
vtkSmartPointer<vtkAstroWebGPUDevice> SharedDevice;

//----------------------------------------------------------------------------
vtkSmartPointer<vtkImageData> MakeCube(int scalarType, const int dims[3], bool blanks)
{
  vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(dims[0], dims[1], dims[2]);
  image->SetSpacing(1., 2., 3.);
  image->AllocateScalars(scalarType, 1);
  std::mt19937 generator(7);
  std::normal_distribution<double> noise(0., 1.);
  vtkDataArray* scalars = image->GetPointData()->GetScalars();
  for (vtkIdType i = 0; i < image->GetNumberOfPoints(); ++i)
    {
    const bool blank = blanks && (i * 2654435761u) % 29 == 0;
    scalars->SetTuple1(i, blank ? std::numeric_limits<double>::quiet_NaN() : 3. * noise(generator));
    }
  return image;
}

//----------------------------------------------------------------------------
std::vector<double> Values(vtkImageData* image)
{
  vtkDataArray* scalars = image->GetPointData()->GetScalars();
  std::vector<double> values(scalars->GetNumberOfTuples());
  for (vtkIdType i = 0; i < scalars->GetNumberOfTuples(); ++i)
    {
    values[i] = scalars->GetTuple1(i);
    }
  return values;
}

//----------------------------------------------------------------------------
bool SameValues(const std::vector<double>& a, const std::vector<double>& b)
{
  if (a.size() != b.size())
    {
    return false;
    }
  for (size_t i = 0; i < a.size(); ++i)
    {
    if (!(a[i] == b[i] || (std::isnan(a[i]) && std::isnan(b[i]))))
      {
      return false;
      }
    }
  return true;
}

//----------------------------------------------------------------------------
// Runs the CPU and the GPU filter (configured by `configure`) and compares.
template <typename GPUFilter, typename CPUFilter, typename Configure>
void Compare(const std::string& name, int scalarType, const int dims[3], bool blanks, Configure&& configure)
{
  vtkSmartPointer<vtkImageData> cube = MakeCube(scalarType, dims, blanks);
  const std::vector<double> before = Values(cube);

  vtkNew<CPUFilter> cpu;
  cpu->SetInputData(cube);
  configure(cpu.GetPointer());
  cpu->Update();

  vtkNew<GPUFilter> gpu;
  gpu->SetInputData(cube);
  if (scalarType == VTK_DOUBLE)
    {
    gpu->SetDevice(SharedDevice);
    }
  configure(gpu.GetPointer());
  gpu->Update();

  const std::string label = name + (scalarType == VTK_FLOAT ? " float" : " double") + (blanks ? " with blanks" : "");
  if (!gpu->GetExecutedOnGPU())
    {
    std::cerr << "FAILED " << label << ": not executed on the GPU" << std::endl;
    ++failures;
    return;
    }
  if (gpu->GetOutput()->GetScalarType() != scalarType)
    {
    std::cerr << "FAILED " << label << ": output scalar type changed" << std::endl;
    ++failures;
    }

  const std::vector<double> expected = Values(cpu->GetOutput());
  const std::vector<double> computed = Values(gpu->GetOutput());
  double scale = 0., maxDiff = 0.;
  size_t worst = 0;
  for (size_t i = 0; i < expected.size(); ++i)
    {
    if (std::isnan(expected[i]) != std::isnan(computed[i]))
      {
      std::cerr << "FAILED " << label << ": blank voxels differ at " << i << std::endl;
      ++failures;
      return;
      }
    if (std::isnan(expected[i]))
      {
      continue;
      }
    scale = std::max(scale, std::fabs(expected[i]));
    const double diff = std::fabs(expected[i] - computed[i]);
    if (diff > maxDiff)
      {
      maxDiff = diff;
      worst = i;
      }
    }
  const bool ok = maxDiff <= 1e-5 * scale;
  std::cout << (ok ? "ok     " : "FAILED ") << label << ": max difference " << maxDiff << " (scale " << scale << ")";
  if (!ok)
    {
    std::cout << " at voxel " << worst << " gpu " << computed[worst] << " cpu " << expected[worst];
    ++failures;
    }
  std::cout << std::endl;

  if (!SameValues(before, Values(cube)))
    {
    std::cerr << "FAILED " << label << ": the input was modified" << std::endl;
    ++failures;
    }
}
} // namespace

//----------------------------------------------------------------------------
int main(int, char*[])
{
  if (!vtkAstroWebGPUDevice::IsAvailable())
    {
    std::cout << "WebGPU not available: test skipped" << std::endl;
    return 77;
    }
  std::cout << "WebGPU: " << vtkAstroWebGPUDevice::GetDescription() << std::endl;
  SharedDevice = vtkSmartPointer<vtkAstroWebGPUDevice>::New();

  // odd sizes, not multiples of the workgroup size
  const int dims[3] = {37, 29, 19};

  // Non-symmetric 3D kernel: checks the orientation of the correlation
  std::function<void(vtkAstroImageKernelSmooth*)> kernel3D = [](vtkAstroImageKernelSmooth* filter)
    {
    vtkNew<vtkDoubleArray> kernel;
    const int kd[3] = {5, 3, 7};
    for (int i = 0; i < kd[0] * kd[1] * kd[2]; ++i)
      {
      kernel->InsertNextValue(0.001 * (i + 1));
      }
    filter->SetKernel(kernel, kd[0], kd[1], kd[2]);
    };
  // Separable with different kernels per axis, Y disabled
  std::function<void(vtkAstroImageSeparableSmooth*)> separableXZ = [](vtkAstroImageSeparableSmooth* filter)
    {
    vtkNew<vtkDoubleArray> kx, kz;
    for (double v : {0.25, 0.5, 0.25}) kx->InsertNextValue(v);
    for (double v : {0.05, 0.1, 0.2, 0.3, 0.2, 0.1, 0.05}) kz->InsertNextValue(v);
    filter->SetAxisKernel(0, kx);
    filter->SetAxisKernel(2, kz);
    };
  // Separable box along the three axes (three ping-pong passes)
  std::function<void(vtkAstroImageSeparableSmooth*)> separableBox = [](vtkAstroImageSeparableSmooth* filter)
    {
    filter->SetAxisBoxKernel(0, 5);
    filter->SetAxisBoxKernel(1, 3);
    filter->SetAxisBoxKernel(2, 7);
    };
  std::function<std::function<void(vtkAstroImageGradientSmooth*)>(int)> gradient = [](int iterations)
    {
    return [iterations](vtkAstroImageGradientSmooth* filter)
      {
      filter->SetConductance(0.7, 0.5, 0.3);
      filter->SetTimeStep(0.05);
      filter->SetNoise(0.8);
      filter->SetK(2.);
      filter->SetNumberOfIterations(iterations);
      };
    };

  for (int scalarType : {VTK_FLOAT, VTK_DOUBLE})
    {
    for (bool blanks : {false, true})
      {
      Compare<vtkAstroWebGPUImageKernelSmooth, vtkAstroImageKernelSmooth>("kernel 3D", scalarType, dims, blanks, kernel3D);
      Compare<vtkAstroWebGPUImageSeparableSmooth, vtkAstroImageSeparableSmooth>("separable XZ", scalarType, dims, blanks, separableXZ);
      Compare<vtkAstroWebGPUImageSeparableSmooth, vtkAstroImageSeparableSmooth>("separable box XYZ", scalarType, dims, blanks, separableBox);
      Compare<vtkAstroWebGPUImageGradientSmooth, vtkAstroImageGradientSmooth>("gradient 4 iterations", scalarType, dims, blanks, gradient(4));
      Compare<vtkAstroWebGPUImageGradientSmooth, vtkAstroImageGradientSmooth>("gradient 7 iterations", scalarType, dims, blanks, gradient(7));
      }
    }

  // Paper defaults (K = 1.5, dt = 0.0325, 20 iterations, C = 5) on a larger cube
  const int large[3] = {128, 96, 64};
  Compare<vtkAstroWebGPUImageGradientSmooth, vtkAstroImageGradientSmooth>("gradient paper defaults", VTK_FLOAT, large, true,
    [](vtkAstroImageGradientSmooth* filter) { filter->SetNoise(3.); });

  // Abort from a progress observer
  {
  vtkSmartPointer<vtkImageData> cube = MakeCube(VTK_FLOAT, dims, true);
  vtkNew<vtkAstroWebGPUImageGradientSmooth> filter;
  filter->SetInputData(cube);
  filter->SetNumberOfIterations(50);
  vtkNew<vtkCallbackCommand> abortCallback;
  abortCallback->SetCallback([](vtkObject* caller, unsigned long, void*, void*)
    {
    vtkAlgorithm::SafeDownCast(caller)->SetAbortExecute(1);
    });
  filter->AddObserver(vtkCommand::ProgressEvent, abortCallback);
  filter->Update();
  if (!filter->GetAborted())
    {
    std::cerr << "FAILED: abort request ignored" << std::endl;
    ++failures;
    }
  }

  SharedDevice = nullptr;
  if (failures)
    {
    std::cerr << failures << " check(s) failed" << std::endl;
    return EXIT_FAILURE;
    }
  std::cout << "vtkAstroWebGPUFiltersTest passed" << std::endl;
  return EXIT_SUCCESS;
}
