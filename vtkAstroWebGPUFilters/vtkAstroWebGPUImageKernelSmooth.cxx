/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#include "vtkAstroWebGPUImageKernelSmooth.h"

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
vtkStandardNewMacro(vtkAstroWebGPUImageKernelSmooth);

//----------------------------------------------------------------------------
vtkAstroWebGPUImageKernelSmooth::vtkAstroWebGPUImageKernelSmooth() = default;

//----------------------------------------------------------------------------
vtkAstroWebGPUImageKernelSmooth::~vtkAstroWebGPUImageKernelSmooth() = default;

//----------------------------------------------------------------------------
void vtkAstroWebGPUImageKernelSmooth::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "ExecutedOnGPU: " << this->ExecutedOnGPU << "\n";
}

//----------------------------------------------------------------------------
void vtkAstroWebGPUImageKernelSmooth::SetDevice(vtkAstroWebGPUDevice* device)
{
  if (this->Device != device)
    {
    this->Device = device;
    this->Modified();
    }
}

//----------------------------------------------------------------------------
vtkAstroWebGPUDevice* vtkAstroWebGPUImageKernelSmooth::GetDevice()
{
  return this->Device;
}

//----------------------------------------------------------------------------
bool vtkAstroWebGPUImageKernelSmooth::Smooth(vtkImageData* input, vtkImageData* output)
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
bool vtkAstroWebGPUImageKernelSmooth::SmoothOnGPU(vtkImageData* input, vtkImageData* output, std::string& error)
{
  vtkDoubleArray* kernel = this->GetKernel();
  if (!kernel || kernel->GetNumberOfTuples() !=
      static_cast<vtkIdType>(this->KernelDimensions[0]) * this->KernelDimensions[1] * this->KernelDimensions[2])
    {
    error = "WebGPU smoothing: invalid kernel.";
    return false;
    }
  vtkAstroWebGPUSmoothingEngine engine;
  if (!engine.Initialize(input, this->Device ? this->Device->GetConfiguration() : nullptr, error))
    {
    return false;
    }
  std::vector<float> weights(kernel->GetPointer(0), kernel->GetPointer(0) + kernel->GetNumberOfTuples());
  engine.SetKernelWeights(weights);

  const int* dims = engine.GetDimensions();
  vtkAstroWebGPUSmoothingEngine::Parameters parameters;
  for (int d = 0; d < 3; ++d)
    {
    parameters.Dims[d] = static_cast<unsigned int>(dims[d]);
    parameters.Radius[d] = static_cast<unsigned int>((this->KernelDimensions[d] - 1) / 2);
    }
  this->ResetProgress(1.);
  vtkWebGPUComputePass* pass = engine.CreatePass(
    "SlicerAstro kernel smooth", vtkAstroWebGPUShaders::Kernel, "kernelSmooth", 0, parameters, true);
  engine.Dispatch(pass);
  engine.Wait();
  this->AddProgress(1.);
  if (this->IsAborting())
    {
    this->Aborted = true;
    return false;
    }
  if (!engine.Download(1, output))
    {
    error = "WebGPU smoothing: download failed.";
    return false;
    }
  return true;
}
