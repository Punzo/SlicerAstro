/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroWebGPUImageKernelSmooth_h
#define __vtkAstroWebGPUImageKernelSmooth_h

#include "vtkAstroWebGPUFiltersExport.h"
#include "vtkAstroWebGPUDevice.h"

// STD includes
#include <string>

#include "vtkAstroImageKernelSmooth.h"

/// \ingroup SlicerAstro_vtkAstroWebGPUFilters
/// \brief Smoothing with an arbitrary 3D kernel (box or rotated Gaussian) on the GPU.
///
/// WebGPU (WGSL compute shader) implementation of vtkAstroImageKernelSmooth: same parameters
/// and same results (computed in float32). The output has the scalar type of
/// the input. If WebGPU is not available, or the cube exceeds the GPU buffer
/// limits, the CPU implementation of the superclass is used (with a warning)
/// and GetExecutedOnGPU() returns false.
class VTK_ASTRO_WEBGPU_FILTERS_EXPORT vtkAstroWebGPUImageKernelSmooth : public vtkAstroImageKernelSmooth
{
public:
  static vtkAstroWebGPUImageKernelSmooth* New();
  vtkTypeMacro(vtkAstroWebGPUImageKernelSmooth, vtkAstroImageKernelSmooth);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// True if the last execution ran on the GPU.
  vtkGetMacro(ExecutedOnGPU, bool);

  /// Device shared with other filters (optional): without it, a device is
  /// created for each execution.
  void SetDevice(vtkAstroWebGPUDevice* device);
  vtkAstroWebGPUDevice* GetDevice();

protected:
  vtkAstroWebGPUImageKernelSmooth();
  ~vtkAstroWebGPUImageKernelSmooth() override;

  bool Smooth(vtkImageData* input, vtkImageData* output) override;
  bool SmoothOnGPU(vtkImageData* input, vtkImageData* output, std::string& error);

  bool ExecutedOnGPU{false};
  vtkSmartPointer<vtkAstroWebGPUDevice> Device;

private:
  vtkAstroWebGPUImageKernelSmooth(const vtkAstroWebGPUImageKernelSmooth&) = delete;
  void operator=(const vtkAstroWebGPUImageKernelSmooth&) = delete;
};

#endif
