/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#ifndef __vtkAstroWebGPUDevice_h
#define __vtkAstroWebGPUDevice_h

#include "vtkAstroWebGPUFiltersExport.h"

// VTK includes
#include <vtkObject.h>
#include <vtkWrappingHints.h>

// VTK includes
#include <vtkSmartPointer.h>

// STD includes
#include <string>

class vtkWebGPUConfiguration;

/// \ingroup SlicerAstro_vtkAstroWebGPUFilters
/// \brief WebGPU device shared by the SlicerAstro WebGPU filters.
///
/// IsAvailable() probes the adapter once per process (the result is cached).
/// An instance owns a WebGPU device, created on first use and released with
/// the instance: filters given the same instance (SetDevice) reuse the device
/// instead of creating one per execution.
class VTK_ASTRO_WEBGPU_FILTERS_EXPORT vtkAstroWebGPUDevice : public vtkObject
{
public:
  static vtkAstroWebGPUDevice* New();
  vtkTypeMacro(vtkAstroWebGPUDevice, vtkObject);

  /// True if a WebGPU adapter and device can be created.
  static bool IsAvailable();

  /// Backend and adapter in use (e.g. "Vulkan - NVIDIA GeForce ..."), empty
  /// if WebGPU is not available.
  static std::string GetDescription();

#ifndef __VTK_WRAP__
  /// The device of this instance (created on first call), nullptr if WebGPU
  /// is not available.
  vtkWebGPUConfiguration* GetConfiguration();
#endif

  /// Releases the device (it is created again when needed).
  void ReleaseDevice();

protected:
  vtkAstroWebGPUDevice();
  ~vtkAstroWebGPUDevice() override;

  vtkSmartPointer<vtkWebGPUConfiguration> Configuration;

private:
  vtkAstroWebGPUDevice(const vtkAstroWebGPUDevice&) = delete;
  void operator=(const vtkAstroWebGPUDevice&) = delete;
};

#endif
