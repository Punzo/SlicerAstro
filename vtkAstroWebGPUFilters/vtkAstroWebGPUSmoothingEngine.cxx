/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#include "vtkAstroWebGPUSmoothingEngine.h"
#include "vtkAstroWebGPUShaders.h"

// VTK includes
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkWebGPUComputeBuffer.h>
#include <vtkWebGPUComputePass.h>
#include <vtkWebGPUComputePipeline.h>
#include <vtkWebGPUConfiguration.h>
#include <vtk_wgpu.h>

// STD includes
#include <algorithm>
#include <cstring>
#include <mutex>
#include <sstream>

namespace
{
// Workgroup size of the shaders
const unsigned int WorkgroupSize[3] = {8, 8, 4};
// Voxels converted per chunk when uploading double cubes (16 MB)
const vtkIdType UploadChunk = 4 * 1024 * 1024;

std::once_flag ProbeFlag;
bool Available = false;
std::string Description;

//----------------------------------------------------------------------------
void Probe()
{
  vtkNew<vtkWebGPUConfiguration> configuration;
  configuration->SetTimeout(5000.);
  Available = configuration->Initialize();
  if (Available)
    {
    std::ostringstream os;
    os << configuration->GetBackendInUseAsString();
    wgpu::AdapterInfo info;
    if (configuration->GetDevice().GetAdapterInfo(&info) == wgpu::Status::Success)
      {
      os << " - " << std::string(info.device.data, info.device.length);
      }
    Description = os.str();
    configuration->Finalize();
    }
}
} // namespace

//----------------------------------------------------------------------------
vtkAstroWebGPUSmoothingEngine::vtkAstroWebGPUSmoothingEngine() = default;

//----------------------------------------------------------------------------
vtkAstroWebGPUSmoothingEngine::~vtkAstroWebGPUSmoothingEngine()
{
  for (auto& pass : this->Passes)
    {
    pass->ReleaseResources();
    }
  this->Passes.clear();
  if (this->Pipeline)
    {
    this->Pipeline->ReleaseResources();
    }
  if (this->Configuration && this->OwnsConfiguration)
    {
    this->Configuration->Finalize();
    }
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkWebGPUConfiguration> vtkAstroWebGPUSmoothingEngine::NewConfiguration()
{
  auto configuration = vtkSmartPointer<vtkWebGPUConfiguration>::New();
  configuration->SetTimeout(5000.);
  if (!configuration->Initialize())
    {
    return nullptr;
    }
  return configuration;
}

//----------------------------------------------------------------------------
bool vtkAstroWebGPUSmoothingEngine::IsAvailable()
{
  std::call_once(ProbeFlag, Probe);
  return Available;
}

//----------------------------------------------------------------------------
std::string vtkAstroWebGPUSmoothingEngine::GetDescription()
{
  std::call_once(ProbeFlag, Probe);
  return Description;
}

//----------------------------------------------------------------------------
bool vtkAstroWebGPUSmoothingEngine::Initialize(vtkImageData* input, vtkWebGPUConfiguration* configuration,
                                               std::string& error)
{
  vtkDataArray* scalars = input ? input->GetPointData()->GetScalars() : nullptr;
  if (!scalars || scalars->GetNumberOfComponents() != 1 ||
      (scalars->GetDataType() != VTK_FLOAT && scalars->GetDataType() != VTK_DOUBLE))
    {
    error = "WebGPU smoothing: single-component float or double input required.";
    return false;
    }
  if (!vtkAstroWebGPUSmoothingEngine::IsAvailable())
    {
    error = "WebGPU smoothing: no WebGPU adapter available.";
    return false;
    }

  this->OwnsConfiguration = (configuration == nullptr);
  this->Configuration = configuration ? configuration : vtkAstroWebGPUSmoothingEngine::NewConfiguration().GetPointer();
  if (!this->Configuration)
    {
    error = "WebGPU smoothing: the WebGPU device could not be created.";
    return false;
    }

  input->GetDimensions(this->Dimensions);
  const unsigned long long byteSize = static_cast<unsigned long long>(this->Dimensions[0]) *
    this->Dimensions[1] * this->Dimensions[2] * sizeof(float);
  wgpu::Limits limits;
  this->Configuration->GetDevice().GetLimits(&limits);
  const unsigned long long maxSize = std::min<unsigned long long>(
    limits.maxStorageBufferBindingSize, limits.maxBufferSize);
  if (byteSize > maxSize || byteSize >= (1ull << 32))
    {
    std::ostringstream os;
    os << "WebGPU smoothing: the cube (" << byteSize << " bytes as float32) exceeds the GPU buffer limit ("
       << maxSize << " bytes).";
    error = os.str();
    return false;
    }
  for (int d = 0; d < 3; ++d)
    {
    const unsigned int groups = (this->Dimensions[d] + WorkgroupSize[d] - 1) / WorkgroupSize[d];
    if (groups > limits.maxComputeWorkgroupsPerDimension)
      {
      error = "WebGPU smoothing: the cube exceeds the maximum number of workgroups.";
      return false;
      }
    }

  this->Pipeline = vtkSmartPointer<vtkWebGPUComputePipeline>::New();
  this->Pipeline->SetWGPUConfiguration(this->Configuration);

  for (int i = 0; i < 2; ++i)
    {
    this->Cubes[i] = vtkSmartPointer<vtkWebGPUComputeBuffer>::New();
    this->Cubes[i]->SetLabel(i == 0 ? "SlicerAstro cube 0" : "SlicerAstro cube 1");
    this->Cubes[i]->SetGroup(0);
    this->Cubes[i]->SetMode(vtkWebGPUComputeBuffer::BufferMode::READ_WRITE_MAP_COMPUTE_STORAGE);
    }
  // The voxels are uploaded in chunks after the buffer is created (note: the
  // VTK_DATA_ARRAY upload of vtkWebGPUComputeBuffer copies the whole array).
  for (int i = 0; i < 2; ++i)
    {
    this->Cubes[i]->SetDataType(vtkWebGPUComputeBuffer::BufferDataType::STD_VECTOR);
    this->Cubes[i]->SetByteSize(byteSize);
    }

  this->Input = input;
  this->InputUploaded = false;
  return true;
}

//----------------------------------------------------------------------------
void vtkAstroWebGPUSmoothingEngine::SetKernelWeights(const std::vector<float>& weights)
{
  this->KernelWeights = weights;
  if (this->KernelWeights.empty())
    {
    this->KernelWeights.push_back(0.f);
    }
  this->Kernel = vtkSmartPointer<vtkWebGPUComputeBuffer>::New();
  this->Kernel->SetLabel("SlicerAstro kernel");
  this->Kernel->SetGroup(0);
  this->Kernel->SetBinding(2);
  this->Kernel->SetMode(vtkWebGPUComputeBuffer::BufferMode::READ_ONLY_COMPUTE_STORAGE);
  this->Kernel->SetDataType(vtkWebGPUComputeBuffer::BufferDataType::STD_VECTOR);
  this->Kernel->SetData(this->KernelWeights);
}

//----------------------------------------------------------------------------
vtkWebGPUComputePass* vtkAstroWebGPUSmoothingEngine::CreatePass(
  const char* label, const char* shader, const char* entryPoint, int source,
  const Parameters& parameters, bool useKernel)
{
  vtkSmartPointer<vtkWebGPUComputePass> pass = this->Pipeline->CreateComputePass();
  pass->SetLabel(label);
  pass->SetShaderSource(std::string(vtkAstroWebGPUShaders::Common) + shader);
  pass->SetShaderEntryPoint(entryPoint);

  std::array<int, 2> indices{{-1, -1}};
  const int destination = 1 - source;
  this->Cubes[source]->SetBinding(0);
  indices[source] = pass->AddBuffer(this->Cubes[source]);
  this->Cubes[destination]->SetBinding(1);
  indices[destination] = pass->AddBuffer(this->Cubes[destination]);

  if (!this->InputUploaded)
    {
    this->InputUploaded = true;
    vtkDataArray* scalars = this->Input->GetPointData()->GetScalars();
    const vtkIdType count = scalars->GetNumberOfTuples();
    std::vector<float> chunk;
    for (vtkIdType first = 0; first < count; first += UploadChunk)
      {
      const vtkIdType last = std::min(count, first + UploadChunk);
      if (scalars->GetDataType() == VTK_FLOAT)
        {
        const float* values = static_cast<const float*>(scalars->GetVoidPointer(0));
        chunk.assign(values + first, values + last);
        }
      else
        {
        const double* values = static_cast<const double*>(scalars->GetVoidPointer(0));
        chunk.assign(values + first, values + last);
        }
      pass->UpdateBufferData(indices[0], first * static_cast<vtkIdType>(sizeof(float)), chunk);
      }
    }

  if (useKernel && this->Kernel)
    {
    pass->AddBuffer(this->Kernel);
    }

  // Uniform parameters (16 x 32 bits)
  std::vector<unsigned int> data(16);
  std::memcpy(data.data(), parameters.Dims.data(), 4 * sizeof(unsigned int));
  std::memcpy(data.data() + 4, parameters.Radius.data(), 4 * sizeof(unsigned int));
  std::memcpy(data.data() + 8, parameters.Coefficients.data(), 4 * sizeof(float));
  std::memcpy(data.data() + 12, parameters.Extra.data(), 4 * sizeof(float));
  this->UniformData.push_back(std::move(data));
  vtkNew<vtkWebGPUComputeBuffer> uniforms;
  uniforms->SetLabel(std::string(label) + " parameters");
  uniforms->SetGroup(0);
  uniforms->SetBinding(3);
  uniforms->SetMode(vtkWebGPUComputeBuffer::BufferMode::UNIFORM_BUFFER);
  uniforms->SetDataType(vtkWebGPUComputeBuffer::BufferDataType::STD_VECTOR);
  uniforms->SetData(this->UniformData.back());
  pass->AddBuffer(uniforms);
  this->Uniforms.emplace_back(uniforms);

  pass->SetWorkgroups(
    static_cast<int>((this->Dimensions[0] + WorkgroupSize[0] - 1) / WorkgroupSize[0]),
    static_cast<int>((this->Dimensions[1] + WorkgroupSize[1] - 1) / WorkgroupSize[1]),
    static_cast<int>((this->Dimensions[2] + WorkgroupSize[2] - 1) / WorkgroupSize[2]));

  this->Passes.push_back(pass);
  this->PassCubeIndices.push_back(indices);
  return pass;
}

//----------------------------------------------------------------------------
void vtkAstroWebGPUSmoothingEngine::Dispatch(vtkWebGPUComputePass* pass)
{
  pass->Dispatch();
}

//----------------------------------------------------------------------------
void vtkAstroWebGPUSmoothingEngine::Wait()
{
  this->Pipeline->Update();
}

//----------------------------------------------------------------------------
bool vtkAstroWebGPUSmoothingEngine::Download(int buffer, vtkImageData* output)
{
  if (this->Passes.empty() || !output || !output->GetPointData()->GetScalars())
    {
    return false;
    }
  vtkDataArray* scalars = output->GetPointData()->GetScalars();
  const vtkIdType count = static_cast<vtkIdType>(this->Dimensions[0]) * this->Dimensions[1] * this->Dimensions[2];
  if (scalars->GetNumberOfTuples() != count)
    {
    return false;
    }

  struct Target
  {
    vtkDataArray* Scalars;
    vtkIdType Count;
    bool Done;
  } target{scalars, count, false};

  auto onMapped = [](const void* mapped, void* userData)
    {
    Target* t = static_cast<Target*>(userData);
    const float* values = static_cast<const float*>(mapped);
    if (t->Scalars->GetDataType() == VTK_FLOAT)
      {
      std::memcpy(t->Scalars->GetVoidPointer(0), values, t->Count * sizeof(float));
      }
    else
      {
      double* out = static_cast<double*>(t->Scalars->GetVoidPointer(0));
      std::copy(values, values + t->Count, out);
      }
    t->Done = true;
    };

  vtkWebGPUComputePass* pass = this->Passes.back();
  const int index = this->PassCubeIndices.back()[buffer];
  pass->ReadBufferFromGPU(index, onMapped, &target);
  this->Pipeline->Update();
  // the map callback may be delivered after the queue work is done
  for (int i = 0; i < 100000 && !target.Done; ++i)
    {
    this->Configuration->ProcessEvents();
    }
  scalars->Modified();
  return target.Done;
}
