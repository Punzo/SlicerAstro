/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

// GPU (OpenGL) smoothing filters compared with the CPU (vtkSMPTools) filters
// on synthetic cubes with blank (NaN) voxels, for float and double inputs and
// for all the shader paths (single-pass 3D kernels, separable multi-pass
// kernels, iterative gradient with even and odd iteration counts).
// Also checks that the input image is not modified. Requires an OpenGL
// context (run with a display, e.g. xvfb-run).

#include "vtkAstroImageGradientSmooth.h"
#include "vtkAstroImageKernelSmooth.h"
#include "vtkAstroImageSeparableSmooth.h"
#include "vtkAstroOpenGLImageBox.h"
#include "vtkAstroOpenGLImageGaussian.h"
#include "vtkAstroOpenGLImageGradient.h"

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>

// STD includes
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace
{
const int Dims[3] = {31, 23, 17};
const double SigmatoFWHM = 2.3548200450309493;
int failures = 0;

//----------------------------------------------------------------------------
vtkSmartPointer<vtkImageData> MakeCube(int scalarType, bool withBlanks)
{
  auto image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(Dims[0], Dims[1], Dims[2]);
  image->AllocateScalars(scalarType, 1);
  std::mt19937 generator(7);
  std::normal_distribution<double> noise(0., 1.);
  vtkDataArray* scalars = image->GetPointData()->GetScalars();
  for (vtkIdType i = 0; i < image->GetNumberOfPoints(); ++i)
    {
    const bool blank = withBlanks && (i * 2654435761u) % 37 == 0;
    scalars->SetTuple1(i, blank ? std::numeric_limits<double>::quiet_NaN() : noise(generator));
    }
  return image;
}

//----------------------------------------------------------------------------
std::vector<double> Values(vtkImageData* image)
{
  std::vector<double> values(image->GetNumberOfPoints());
  for (vtkIdType i = 0; i < image->GetNumberOfPoints(); ++i)
    values[i] = image->GetPointData()->GetScalars()->GetTuple1(i);
  return values;
}

//----------------------------------------------------------------------------
void Compare(vtkImageData* gpu, vtkImageData* cpu, double tolerance, const char* name)
{
  const std::vector<double> g = Values(gpu), c = Values(cpu);
  double maxDiff = 0.;
  size_t maxAt = 0;
  int nanMismatch = 0;
  for (size_t i = 0; i < c.size(); ++i)
    {
    if (std::isnan(g[i]) != std::isnan(c[i]))
      {
      ++nanMismatch;
      continue;
      }
    if (!std::isnan(c[i]) && std::fabs(g[i] - c[i]) > maxDiff)
      {
      maxDiff = std::fabs(g[i] - c[i]);
      maxAt = i;
      }
    }
  const bool ok = nanMismatch == 0 && maxDiff < tolerance;
  std::cout << (ok ? "ok      " : "FAILED  ") << name << ": max difference " << maxDiff
            << ", blank mismatches " << nanMismatch;
  if (!(nanMismatch == 0 && maxDiff < tolerance))
    {
    std::cout << " at (" << maxAt % Dims[0] << ", " << (maxAt / Dims[0]) % Dims[1] << ", "
              << maxAt / (Dims[0] * Dims[1]) << ") gpu " << g[maxAt] << " cpu " << c[maxAt];
    }
  std::cout << std::endl;
  if (!ok)
    ++failures;
}

//----------------------------------------------------------------------------
void CheckUnchanged(vtkImageData* input, const std::vector<double>& before, const char* name)
{
  const std::vector<double> after = Values(input);
  bool same = after.size() == before.size();
  for (size_t i = 0; same && i < after.size(); ++i)
    same = (after[i] == before[i]) || (std::isnan(after[i]) && std::isnan(before[i]));
  double spacing[3];
  input->GetSpacing(spacing);
  same = same && spacing[0] == 1. && spacing[1] == 1. && spacing[2] == 1.;
  if (!same)
    {
    std::cout << "FAILED  " << name << ": the input image was modified" << std::endl;
    ++failures;
    }
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkDoubleArray> Gaussian1D(double fwhm, int length)
{
  auto kernel = vtkSmartPointer<vtkDoubleArray>::New();
  const double sigma = fwhm / SigmatoFWHM;
  const int r = (length - 1) / 2;
  double sum = 0.;
  for (int i = -r; i <= r; ++i)
    {
    const double v = std::exp(-i * i / (2. * sigma * sigma));
    kernel->InsertNextValue(v);
    sum += v;
    }
  for (vtkIdType i = 0; i < kernel->GetNumberOfTuples(); ++i)
    kernel->SetValue(i, kernel->GetValue(i) / sum);
  return kernel;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkDoubleArray> Gaussian3D(const double fwhm[3], const int length[3])
{
  auto kernel = vtkSmartPointer<vtkDoubleArray>::New();
  const int r[3] = {(length[0] - 1) / 2, (length[1] - 1) / 2, (length[2] - 1) / 2};
  double sum = 0.;
  for (int k = -r[2]; k <= r[2]; ++k)
    for (int j = -r[1]; j <= r[1]; ++j)
      for (int i = -r[0]; i <= r[0]; ++i)
        {
        const double sx = fwhm[0] / SigmatoFWHM, sy = fwhm[1] / SigmatoFWHM, sz = fwhm[2] / SigmatoFWHM;
        const double v = std::exp(-(i * i / (2 * sx * sx) + j * j / (2 * sy * sy) + k * k / (2 * sz * sz)));
        kernel->InsertNextValue(v);
        sum += v;
        }
  for (vtkIdType i = 0; i < kernel->GetNumberOfTuples(); ++i)
    kernel->SetValue(i, kernel->GetValue(i) / sum);
  return kernel;
}

//----------------------------------------------------------------------------
void RunAll(int scalarType, bool withBlanks, vtkRenderWindow* renderWindow)
{
  const std::string suffix = std::string(scalarType == VTK_FLOAT ? " float" : " double") +
                             (withBlanks ? " with blanks" : "");
  const double tolerance = 2e-5; // float GPU arithmetic (data ~ N(0, 1))

  // Box: anisotropic (single pass 3D) and isotropic (separable, 3 passes)
  for (int iso = 0; iso < 2; ++iso)
    {
    vtkSmartPointer<vtkImageData> cube = MakeCube(scalarType, withBlanks);
    const std::vector<double> before = Values(cube);
    const int n[3] = {iso ? 5 : 3, 5, iso ? 5 : 7};
    vtkNew<vtkAstroOpenGLImageBox> gpu;
    gpu->SetRenderWindow(renderWindow);
    gpu->SetInputData(cube);
    gpu->SetKernelLength(n[0], n[1], n[2]);
    gpu->Update();
    vtkNew<vtkAstroImageKernelSmooth> cpu;
    cpu->SetInputData(cube);
    cpu->SetBoxKernel(n[0], n[1], n[2]);
    cpu->Update();
    Compare(gpu->GetOutput(), cpu->GetOutput(), tolerance,
            ((iso ? "isotropic box" : "anisotropic box") + suffix).c_str());
    CheckUnchanged(cube, before, "box");
    }

  // Gaussian: anisotropic (single pass 3D) and isotropic (separable, 3 passes)
  for (int iso = 0; iso < 2; ++iso)
    {
    vtkSmartPointer<vtkImageData> cube = MakeCube(scalarType, withBlanks);
    const std::vector<double> before = Values(cube);
    const double fwhm[3] = {iso ? 3. : 2., 3., iso ? 3. : 4.};
    const int length[3] = {iso ? 5 : 3, 5, iso ? 5 : 7};
    vtkNew<vtkAstroOpenGLImageGaussian> gpu;
    gpu->SetRenderWindow(renderWindow);
    gpu->SetInputData(cube);
    gpu->SetKernelLength(length[0], length[1], length[2]);
    gpu->SetFWHM(fwhm[0], fwhm[1], fwhm[2]);
    gpu->SetRotationAngles(0., 0., 0.);
    gpu->Update();
    // a second execution must give the same result (no parameter drift)
    gpu->Modified();
    gpu->Update();
    vtkSmartPointer<vtkImageData> cpuOutput;
    if (iso)
      {
      vtkNew<vtkAstroImageSeparableSmooth> cpu;
      cpu->SetInputData(cube);
      vtkSmartPointer<vtkDoubleArray> kernel = Gaussian1D(fwhm[0], length[0]);
      for (int axis = 0; axis < 3; ++axis)
        cpu->SetAxisKernel(axis, kernel);
      cpu->Update();
      cpuOutput = cpu->GetOutput();
      }
    else
      {
      vtkNew<vtkAstroImageKernelSmooth> cpu;
      cpu->SetInputData(cube);
      cpu->SetKernel(Gaussian3D(fwhm, length), length[0], length[1], length[2]);
      cpu->Update();
      cpuOutput = cpu->GetOutput();
      }
    Compare(gpu->GetOutput(), cpuOutput, tolerance,
            ((iso ? "isotropic Gaussian" : "anisotropic Gaussian") + suffix).c_str());
    CheckUnchanged(cube, before, "Gaussian");
    }

  // Gradient: even and odd number of iterations (GPU ping-pong)
  for (int iterations : {4, 7})
    {
    vtkSmartPointer<vtkImageData> cube = MakeCube(scalarType, withBlanks);
    const std::vector<double> before = Values(cube);
    vtkNew<vtkAstroOpenGLImageGradient> gpu;
    gpu->SetRenderWindow(renderWindow);
    gpu->SetInputData(cube);
    gpu->SetCl(5., 5., 3.);
    gpu->SetK(1.5);
    gpu->SetTimeStep(0.0325);
    gpu->SetRMS(0.8);
    gpu->SetAccuracy(iterations);
    gpu->Update();
    vtkNew<vtkAstroImageGradientSmooth> cpu;
    cpu->SetInputData(cube);
    cpu->SetConductance(5., 5., 3.);
    cpu->SetK(1.5);
    cpu->SetTimeStep(0.0325);
    cpu->SetNoise(0.8);
    cpu->SetNumberOfIterations(iterations);
    cpu->Update();
    Compare(gpu->GetOutput(), cpu->GetOutput(), tolerance,
            ("gradient, " + std::to_string(iterations) + " iterations" + suffix).c_str());
    CheckUnchanged(cube, before, "gradient");
    }
}
} // namespace

//----------------------------------------------------------------------------
int main(int, char*[])
{
  // Filters with their own offscreen context, and with a shared render window
  RunAll(VTK_FLOAT, false, nullptr);
  RunAll(VTK_FLOAT, true, nullptr);
  RunAll(VTK_DOUBLE, true, nullptr);
  vtkNew<vtkRenderWindow> renderWindow;
  renderWindow->SetShowWindow(false);
  RunAll(VTK_FLOAT, true, renderWindow);

  if (failures)
    {
    std::cerr << failures << " check(s) failed" << std::endl;
    return EXIT_FAILURE;
    }
  std::cout << "vtkAstroOpenGLFiltersTest passed" << std::endl;
  return EXIT_SUCCESS;
}
