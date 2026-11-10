/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#include "vtkAstroWebGPUImageGradientSmooth.h"

// SlicerAstro includes
#include "vtkAstroWebGPUShaders.h"
#include "vtkAstroWebGPUSmoothingEngine.h"

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>

// STD includes
#include <vector>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkAstroWebGPUImageGradientSmooth);

//----------------------------------------------------------------------------
vtkAstroWebGPUImageGradientSmooth::vtkAstroWebGPUImageGradientSmooth() = default;

//----------------------------------------------------------------------------
vtkAstroWebGPUImageGradientSmooth::~vtkAstroWebGPUImageGradientSmooth() = default;

//----------------------------------------------------------------------------
void vtkAstroWebGPUImageGradientSmooth::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "ExecutedOnGPU: " << this->ExecutedOnGPU << "\n";
}

//----------------------------------------------------------------------------
void vtkAstroWebGPUImageGradientSmooth::SetDevice(vtkAstroWebGPUDevice* device)
{
  if (this->Device != device)
    {
    this->Device = device;
    this->Modified();
    }
}

//----------------------------------------------------------------------------
vtkAstroWebGPUDevice* vtkAstroWebGPUImageGradientSmooth::GetDevice()
{
  return this->Device;
}

//----------------------------------------------------------------------------
bool vtkAstroWebGPUImageGradientSmooth::Smooth(vtkImageData* input, vtkImageData* output)
{
  std::string error;
  this->ExecutedOnGPU = this->SmoothOnGPU(input, output, error);
  if (this->ExecutedOnGPU || this->Aborted || this->IsAborting())
    {
    return this->ExecutedOnGPU;
    }
  vtkWarningMacro(<< error << " Using the CPU implementation.");
  return this->Superclass::Smooth(input, output);
}

//----------------------------------------------------------------------------
bool vtkAstroWebGPUImageGradientSmooth::SmoothOnGPU(vtkImageData* input, vtkImageData* output, std::string& error)
{
  if (this->NumberOfIterations <= 0)
    {
    output->GetPointData()->GetScalars()->DeepCopy(input->GetPointData()->GetScalars());
    return true;
    }
  const double noise2 = this->Noise * this->Noise * this->K * this->K;
  if (!(noise2 > 0.))
    {
    error = "WebGPU smoothing: the noise and K must be positive.";
    return false;
    }
  vtkAstroWebGPUSmoothingEngine engine;
  if (!engine.Initialize(input, this->Device ? this->Device->GetConfiguration() : nullptr, error))
    {
    return false;
    }
  const int* dims = engine.GetDimensions();
  vtkAstroWebGPUSmoothingEngine::Parameters parameters;
  for (int d = 0; d < 3; ++d)
    {
    parameters.Dims[d] = static_cast<unsigned int>(dims[d]);
    parameters.Coefficients[d] = static_cast<float>(this->Conductance[d]);
    }
  parameters.Coefficients[3] = static_cast<float>(this->TimeStep);
  parameters.Extra[0] = static_cast<float>(noise2);

  vtkWebGPUComputePass* passes[2] = {
    engine.CreatePass("SlicerAstro gradient smooth (0)", vtkAstroWebGPUShaders::Gradient, "gradientSmooth", 0, parameters, false),
    engine.CreatePass("SlicerAstro gradient smooth (1)", vtkAstroWebGPUShaders::Gradient, "gradientSmooth", 1, parameters, false)};

  this->ResetProgress(static_cast<double>(this->NumberOfIterations));
  for (int iteration = 0; iteration < this->NumberOfIterations; ++iteration)
    {
    engine.Dispatch(passes[iteration % 2]);
    engine.Wait();
    this->AddProgress(1.);
    if (this->IsAborting())
      {
      this->Aborted = true;
      return false;
      }
    }
  // iteration i writes buffer 1 - i % 2
  const int result = (this->NumberOfIterations % 2 == 1) ? 1 : 0;
  if (!engine.Download(result, output))
    {
    error = "WebGPU smoothing: download failed.";
    return false;
    }
  return true;
}
