/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#include "vtkAstroWebGPUDevice.h"
#include "vtkAstroWebGPUSmoothingEngine.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkWebGPUConfiguration.h>

vtkStandardNewMacro(vtkAstroWebGPUDevice);

//----------------------------------------------------------------------------
vtkAstroWebGPUDevice::vtkAstroWebGPUDevice() = default;

//----------------------------------------------------------------------------
vtkAstroWebGPUDevice::~vtkAstroWebGPUDevice()
{
  this->ReleaseDevice();
}

//----------------------------------------------------------------------------
vtkWebGPUConfiguration* vtkAstroWebGPUDevice::GetConfiguration()
{
  if (!this->Configuration && vtkAstroWebGPUSmoothingEngine::IsAvailable())
    {
    this->Configuration = vtkAstroWebGPUSmoothingEngine::NewConfiguration();
    }
  return this->Configuration;
}

//----------------------------------------------------------------------------
void vtkAstroWebGPUDevice::ReleaseDevice()
{
  if (this->Configuration)
    {
    this->Configuration->Finalize();
    this->Configuration = nullptr;
    }
}

//----------------------------------------------------------------------------
bool vtkAstroWebGPUDevice::IsAvailable()
{
  return vtkAstroWebGPUSmoothingEngine::IsAvailable();
}

//----------------------------------------------------------------------------
std::string vtkAstroWebGPUDevice::GetDescription()
{
  return vtkAstroWebGPUSmoothingEngine::GetDescription();
}
