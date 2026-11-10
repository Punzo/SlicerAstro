/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

// Compares the SMP smoothing filters with straightforward serial reference
// implementations on small synthetic cubes containing blank (NaN) voxels, and
// checks that the results do not depend on the number of threads.

#include "vtkAstroImageGradientSmooth.h"
#include "vtkAstroImageKernelSmooth.h"
#include "vtkAstroImageSeparableSmooth.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSMPTools.h>
#include <vtkSmartPointer.h>

// STD includes
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace
{
const int Dims[3] = {23, 17, 11};

int failures = 0;
#define CHECK(condition, message) \
  if (!(condition)) { std::cerr << "FAILED: " << message << " (line " << __LINE__ << ")" << std::endl; ++failures; }

//----------------------------------------------------------------------------
vtkSmartPointer<vtkImageData> MakeCube(int scalarType)
{
  auto image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(Dims[0], Dims[1], Dims[2]);
  image->AllocateScalars(scalarType, 1);
  std::mt19937 generator(42);
  std::normal_distribution<double> noise(0., 1.);
  const vtkIdType n = image->GetNumberOfPoints();
  vtkDataArray* scalars = image->GetPointData()->GetScalars();
  for (vtkIdType i = 0; i < n; ++i)
    {
    // ~3% blank voxels
    const bool blank = (i * 2654435761u) % 31 == 0;
    scalars->SetTuple1(i, blank ? std::numeric_limits<double>::quiet_NaN() : noise(generator));
    }
  return image;
}

//----------------------------------------------------------------------------
double Value(vtkImageData* image, int x, int y, int z)
{
  return image->GetScalarComponentAsDouble(x, y, z, 0);
}

//----------------------------------------------------------------------------
bool Inside(int x, int y, int z)
{
  return x >= 0 && y >= 0 && z >= 0 && x < Dims[0] && y < Dims[1] && z < Dims[2];
}

//----------------------------------------------------------------------------
// Serial reference: out(x) = sum in(x + k) * kernel(k), NaN and outside = 0
std::vector<double> ReferenceKernel(vtkImageData* in, const std::vector<double>& kernel, const int kd[3])
{
  std::vector<double> out(static_cast<size_t>(Dims[0]) * Dims[1] * Dims[2]);
  const int r[3] = {(kd[0] - 1) / 2, (kd[1] - 1) / 2, (kd[2] - 1) / 2};
  for (int z = 0; z < Dims[2]; ++z)
    for (int y = 0; y < Dims[1]; ++y)
      for (int x = 0; x < Dims[0]; ++x)
        {
        double sum = 0.;
        for (int k = -r[2]; k <= r[2]; ++k)
          for (int j = -r[1]; j <= r[1]; ++j)
            for (int i = -r[0]; i <= r[0]; ++i)
              {
              if (!Inside(x + i, y + j, z + k))
                continue;
              const double v = Value(in, x + i, y + j, z + k);
              if (std::isnan(v))
                continue;
              sum += v * kernel[(k + r[2]) * kd[0] * kd[1] + (j + r[1]) * kd[0] + (i + r[0])];
              }
        out[(static_cast<size_t>(z) * Dims[1] + y) * Dims[0] + x] = sum;
        }
  return out;
}

//----------------------------------------------------------------------------
double MaxDifference(vtkImageData* image, const std::vector<double>& reference)
{
  double maxDiff = 0.;
  for (size_t i = 0; i < reference.size(); ++i)
    {
    const double v = image->GetPointData()->GetScalars()->GetTuple1(static_cast<vtkIdType>(i));
    const double r = reference[i];
    if (std::isnan(v) != std::isnan(r))
      return std::numeric_limits<double>::infinity();
    if (!std::isnan(v))
      maxDiff = std::max(maxDiff, std::fabs(v - r));
    }
  return maxDiff;
}

//----------------------------------------------------------------------------
std::vector<double> Copy(vtkImageData* image)
{
  std::vector<double> values(image->GetNumberOfPoints());
  for (vtkIdType i = 0; i < image->GetNumberOfPoints(); ++i)
    values[i] = image->GetPointData()->GetScalars()->GetTuple1(i);
  return values;
}

//----------------------------------------------------------------------------
void TestKernel(int scalarType, double tolerance)
{
  vtkSmartPointer<vtkImageData> cube = MakeCube(scalarType);
  // non-symmetric kernel: checks the orientation of the correlation
  const int kd[3] = {5, 3, 3};
  std::vector<double> kernel(kd[0] * kd[1] * kd[2]);
  vtkNew<vtkDoubleArray> kernelArray;
  kernelArray->SetNumberOfValues(static_cast<vtkIdType>(kernel.size()));
  for (size_t i = 0; i < kernel.size(); ++i)
    {
    kernel[i] = 0.01 * (i + 1);
    kernelArray->SetValue(static_cast<vtkIdType>(i), kernel[i]);
    }
  vtkNew<vtkAstroImageKernelSmooth> filter;
  filter->SetInputData(cube);
  filter->SetKernel(kernelArray, kd[0], kd[1], kd[2]);
  filter->Update();
  const double diff = MaxDifference(filter->GetOutput(), ReferenceKernel(cube, kernel, kd));
  CHECK(diff < tolerance, "kernel smooth differs from reference: " << diff);
}

//----------------------------------------------------------------------------
void TestSeparable(int scalarType, double tolerance)
{
  vtkSmartPointer<vtkImageData> cube = MakeCube(scalarType);
  // reference: X pass on the input (NaN = 0), then Y, then Z on the results
  const double kx[3] = {0.25, 0.5, 0.25}, kz[5] = {0.1, 0.2, 0.4, 0.2, 0.1};
  std::vector<double> step(Copy(cube));
  auto at = [](int x, int y, int z) { return (static_cast<size_t>(z) * Dims[1] + y) * Dims[0] + x; };
  auto pass = [&](const std::vector<double>& src, int axis, const double* k, int length)
    {
    std::vector<double> dst(src.size());
    const int r = (length - 1) / 2;
    for (int z = 0; z < Dims[2]; ++z)
      for (int y = 0; y < Dims[1]; ++y)
        for (int x = 0; x < Dims[0]; ++x)
          {
          double sum = 0.;
          for (int i = -r; i <= r; ++i)
            {
            int p[3] = {x, y, z};
            p[axis] += i;
            if (!Inside(p[0], p[1], p[2]))
              continue;
            const double v = src[at(p[0], p[1], p[2])];
            if (!std::isnan(v))
              sum += v * k[i + r];
            }
          dst[at(x, y, z)] = sum;
          }
    return dst;
    };
  step = pass(step, 0, kx, 3);
  step = pass(step, 2, kz, 5); // Y disabled

  vtkNew<vtkDoubleArray> kernelX, kernelZ;
  for (double v : kx) kernelX->InsertNextValue(v);
  for (double v : kz) kernelZ->InsertNextValue(v);
  vtkNew<vtkAstroImageSeparableSmooth> filter;
  filter->SetInputData(cube);
  filter->SetAxisKernel(0, kernelX);
  filter->SetAxisKernel(2, kernelZ);
  filter->Update();
  const double diff = MaxDifference(filter->GetOutput(), step);
  CHECK(diff < tolerance, "separable smooth differs from reference: " << diff);
}

//----------------------------------------------------------------------------
void TestGradient(int scalarType, double tolerance)
{
  vtkSmartPointer<vtkImageData> cube = MakeCube(scalarType);
  const double c[3] = {0.7, 0.5, 0.3}, timeStep = 0.05, noise = 0.8, K = 2.;
  const int iterations = 4;
  std::vector<double> cur(Copy(cube));
  auto at = [](int x, int y, int z) { return (static_cast<size_t>(z) * Dims[1] + y) * Dims[0] + x; };
  for (int it = 0; it < iterations; ++it)
    {
    std::vector<double> next(cur.size());
    for (int z = 0; z < Dims[2]; ++z)
      for (int y = 0; y < Dims[1]; ++y)
        for (int x = 0; x < Dims[0]; ++x)
          {
          auto nb = [&](int dx, int dy, int dz)
            {
            int p[3] = {x + dx, y + dy, z + dz};
            return Inside(p[0], p[1], p[2]) ? cur[at(p[0], p[1], p[2])] : cur[at(x, y, z)];
            };
          const double v = cur[at(x, y, z)];
          const double n[6] = {nb(-1, 0, 0), nb(1, 0, 0), nb(0, -1, 0), nb(0, 1, 0), nb(0, 0, -1), nb(0, 0, 1)};
          bool blank = std::isnan(v);
          for (double w : n) blank = blank || std::isnan(w);
          if (blank)
            {
            next[at(x, y, z)] = v;
            continue;
            }
          const double norm = 1. + v * v / (noise * noise * K * K);
          const double flux = c[0] * (n[0] + n[1] - 2 * v) + c[1] * (n[2] + n[3] - 2 * v) + c[2] * (n[4] + n[5] - 2 * v);
          next[at(x, y, z)] = v + timeStep * flux / norm;
          }
    cur = next;
    }

  vtkNew<vtkAstroImageGradientSmooth> filter;
  filter->SetInputData(cube);
  filter->SetConductance(c[0], c[1], c[2]);
  filter->SetTimeStep(timeStep);
  filter->SetNoise(noise);
  filter->SetK(K);
  filter->SetNumberOfIterations(iterations);
  filter->Update();
  const double diff = MaxDifference(filter->GetOutput(), cur);
  CHECK(diff < tolerance, "gradient smooth differs from reference: " << diff);
}

//----------------------------------------------------------------------------
void TestThreadsAndAbort()
{
  vtkSmartPointer<vtkImageData> cube = MakeCube(VTK_FLOAT);
  vtkNew<vtkAstroImageSeparableSmooth> filter;
  filter->SetInputData(cube);
  filter->SetAxisBoxKernel(0, 5);
  filter->SetAxisBoxKernel(1, 3);
  filter->SetAxisBoxKernel(2, 7);

  vtkSMPTools::Initialize(1);
  filter->Update();
  std::vector<double> serial(Copy(filter->GetOutput()));
  vtkSMPTools::Initialize(0); // default number of threads
  filter->Modified();
  filter->Update();
  CHECK(MaxDifference(filter->GetOutput(), serial) == 0., "results depend on the number of threads");

  // Abort from a progress observer
  vtkNew<vtkCallbackCommand> abortCallback;
  abortCallback->SetCallback([](vtkObject* caller, unsigned long, void*, void*)
    {
    vtkAlgorithm::SafeDownCast(caller)->SetAbortExecute(1);
    });
  filter->AddObserver(vtkCommand::ProgressEvent, abortCallback);
  filter->Modified();
  filter->Update();
  CHECK(filter->GetAborted(), "abort request ignored");
}
} // namespace

//----------------------------------------------------------------------------
int main(int, char*[])
{
  TestKernel(VTK_DOUBLE, 1e-12);
  TestKernel(VTK_FLOAT, 1e-5);
  TestSeparable(VTK_DOUBLE, 1e-12);
  TestSeparable(VTK_FLOAT, 1e-5);
  TestGradient(VTK_DOUBLE, 1e-12);
  TestGradient(VTK_FLOAT, 1e-4);
  TestThreadsAndAbort();
  if (failures)
    {
    std::cerr << failures << " check(s) failed" << std::endl;
    return EXIT_FAILURE;
    }
  std::cout << "vtkAstroImageFiltersTest passed" << std::endl;
  return EXIT_SUCCESS;
}
