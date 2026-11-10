/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#include "vtkAstroWebGPUImageSeparableSmooth.h"

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
vtkStandardNewMacro(vtkAstroWebGPUImageSeparableSmooth);

//----------------------------------------------------------------------------
vtkAstroWebGPUImageSeparableSmooth::vtkAstroWebGPUImageSeparableSmooth() = default;

//----------------------------------------------------------------------------
vtkAstroWebGPUImageSeparableSmooth::~vtkAstroWebGPUImageSeparableSmooth() = default;

//----------------------------------------------------------------------------
void vtkAstroWebGPUImageSeparableSmooth::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "ExecutedOnGPU: " << this->ExecutedOnGPU << "\n";
}

//----------------------------------------------------------------------------
void vtkAstroWebGPUImageSeparableSmooth::SetDevice(vtkAstroWebGPUDevice* device)
{
  if (this->Device != device)
    {
    this->Device = device;
    this->Modified();
    }
}

//----------------------------------------------------------------------------
vtkAstroWebGPUDevice* vtkAstroWebGPUImageSeparableSmooth::GetDevice()
{
  return this->Device;
}

//----------------------------------------------------------------------------
bool vtkAstroWebGPUImageSeparableSmooth::Smooth(vtkImageData* input, vtkImageData* output)
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
bool vtkAstroWebGPUImageSeparableSmooth::SmoothOnGPU(vtkImageData* input, vtkImageData* output, std::string& error)
{
  vtkAstroWebGPUSmoothingEngine engine;
  if (!engine.Initialize(input, this->Device ? this->Device->GetConfiguration() : nullptr, error))
    {
    return false;
    }
  const int* dims = engine.GetDimensions();

  // All the 1D kernels in one buffer
  std::vector<float> weights;
  unsigned int offsets[3] = {0, 0, 0};
  std::vector<int> axes;
  for (int axis = 0; axis < 3; ++axis)
    {
    vtkDoubleArray* kernel = this->AxisKernels[axis];
    if (!kernel || kernel->GetNumberOfTuples() == 0)
      {
      continue;
      }
    offsets[axis] = static_cast<unsigned int>(weights.size());
    weights.insert(weights.end(), kernel->GetPointer(0), kernel->GetPointer(0) + kernel->GetNumberOfTuples());
    axes.push_back(axis);
    }
  engine.SetKernelWeights(weights);

  // Ping-pong: pass i reads buffer i % 2
  std::vector<vtkWebGPUComputePass*> passes;
  int source = 0;
  for (int axis : axes)
    {
    vtkAstroWebGPUSmoothingEngine::Parameters parameters;
    for (int d = 0; d < 3; ++d)
      {
      parameters.Dims[d] = static_cast<unsigned int>(dims[d]);
      }
    parameters.Dims[3] = static_cast<unsigned int>(axis);
    parameters.Radius[axis] = static_cast<unsigned int>((this->AxisKernels[axis]->GetNumberOfTuples() - 1) / 2);
    parameters.Radius[3] = offsets[axis];
    passes.push_back(engine.CreatePass(
      "SlicerAstro separable smooth", vtkAstroWebGPUShaders::Separable, "axisSmooth", source, parameters, true));
    source = 1 - source;
    }
  if (passes.empty())
    {
    // nothing to smooth: copy of the input
    output->GetPointData()->GetScalars()->DeepCopy(input->GetPointData()->GetScalars());
    return true;
    }

  this->ResetProgress(static_cast<double>(passes.size()));
  for (vtkWebGPUComputePass* pass : passes)
    {
    engine.Dispatch(pass);
    engine.Wait();
    this->AddProgress(1.);
    if (this->IsAborting())
      {
      this->Aborted = true;
      return false;
      }
    }
  if (!engine.Download(source, output))
    {
    error = "WebGPU smoothing: download failed.";
    return false;
    }
  return true;
}
