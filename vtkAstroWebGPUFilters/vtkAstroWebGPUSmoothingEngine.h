/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroWebGPUSmoothingEngine_h
#define __vtkAstroWebGPUSmoothingEngine_h

#include "vtkAstroWebGPUFiltersExport.h"

// VTK includes
#include <vtkSmartPointer.h>
#include <vtkWrappingHints.h>

// STD includes
#include <array>
#include <string>
#include <vector>

class vtkImageData;
class vtkWebGPUComputeBuffer;
class vtkWebGPUComputePass;
class vtkWebGPUComputePipeline;
class vtkWebGPUConfiguration;

/// \ingroup SlicerAstro_vtkAstroWebGPUFilters
/// \brief Internal helper of the WebGPU smoothing filters (not wrapped).
///
/// Holds the WebGPU device and two float32 storage buffers of the size of the
/// cube (ping-pong): buffer 0 is initialized with the input voxels. Passes read
/// one buffer and write the other one; the result is downloaded directly into
/// the output image scalars. The input is uploaded (and double values are
/// converted) in chunks of 16 MB, so no full-size temporary copy of the cube
/// is made on the CPU.
class VTK_ASTRO_WEBGPU_FILTERS_EXPORT VTK_WRAPEXCLUDE vtkAstroWebGPUSmoothingEngine
{
public:
  /// Uniform parameters of the shaders (see vtkAstroWebGPUShaders.h).
  struct Parameters
  {
    std::array<unsigned int, 4> Dims{{0, 0, 0, 0}};   // x, y, z, axis
    std::array<unsigned int, 4> Radius{{0, 0, 0, 0}}; // rx, ry, rz, kernel offset
    std::array<float, 4> Coefficients{{0.f, 0.f, 0.f, 0.f}};
    std::array<float, 4> Extra{{0.f, 0.f, 0.f, 0.f}};
  };

  vtkAstroWebGPUSmoothingEngine();
  ~vtkAstroWebGPUSmoothingEngine();

  /// Uploads the input voxels (float or double) to the device of
  /// `configuration` (a new device is created if nullptr). Returns false (and
  /// sets the error message) if no WebGPU adapter is available or if the cube
  /// exceeds the device buffer limits.
  bool Initialize(vtkImageData* input, vtkWebGPUConfiguration* configuration, std::string& error);

  /// New initialized device, nullptr on failure.
  static vtkSmartPointer<vtkWebGPUConfiguration> NewConfiguration();

  /// Kernel weights shared by the passes (bound at binding 2).
  void SetKernelWeights(const std::vector<float>& weights);

  /// Creates a pass reading buffer `source` (0 or 1) and writing the other one.
  /// `shader` is appended to the common declarations.
  vtkWebGPUComputePass* CreatePass(const char* label, const char* shader,
                                   const char* entryPoint, int source,
                                   const Parameters& parameters, bool useKernel);

  /// Queues a pass (asynchronous).
  void Dispatch(vtkWebGPUComputePass* pass);

  /// Waits for the queued passes.
  void Wait();

  /// Downloads buffer `buffer` (0 or 1) into the output scalars (float or
  /// double, same dimensions as the input). Returns false on failure.
  bool Download(int buffer, vtkImageData* output);

  /// Dimensions of the input cube.
  const int* GetDimensions() const { return this->Dimensions; }

  /// True if a WebGPU adapter can be created (the result is cached).
  static bool IsAvailable();
  /// Adapter/backend description, empty if not available.
  static std::string GetDescription();

private:
  vtkAstroWebGPUSmoothingEngine(const vtkAstroWebGPUSmoothingEngine&) = delete;
  void operator=(const vtkAstroWebGPUSmoothingEngine&) = delete;

  vtkSmartPointer<vtkWebGPUConfiguration> Configuration;
  bool OwnsConfiguration{false};
  vtkSmartPointer<vtkWebGPUComputePipeline> Pipeline;
  vtkSmartPointer<vtkWebGPUComputeBuffer> Cubes[2];
  vtkSmartPointer<vtkWebGPUComputeBuffer> Kernel;
  std::vector<float> KernelWeights;
  std::vector<vtkSmartPointer<vtkWebGPUComputeBuffer>> Uniforms;
  std::vector<std::vector<unsigned int>> UniformData;
  std::vector<vtkSmartPointer<vtkWebGPUComputePass>> Passes;
  std::vector<std::array<int, 2>> PassCubeIndices; // buffer index of cube 0/1 in each pass
  vtkImageData* Input{nullptr};
  bool InputUploaded{false};
  int Dimensions[3]{0, 0, 0};
};

#endif
