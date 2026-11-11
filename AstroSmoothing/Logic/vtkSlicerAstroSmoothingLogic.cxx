/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroSmoothingLogic.h>
#include <vtkSlicerAstroConfigure.h>

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroSmoothingParametersNode.h>

// VTK includes
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENGL
#include <vtkAstroOpenGLImageBox.h>
#include <vtkAstroOpenGLImageGaussian.h>
#include <vtkAstroOpenGLImageGradient.h>
#endif
#include <vtkAstroImageGradientSmooth.h>
#include <vtkAstroImageKernelSmooth.h>
#include <vtkAstroImageSeparableSmooth.h>
#ifdef SLICERASTRO_HAS_WEBGPU
#include <vtkAstroWebGPUDevice.h>
#include <vtkAstroWebGPUImageGradientSmooth.h>
#include <vtkAstroWebGPUImageKernelSmooth.h>
#include <vtkAstroWebGPUImageSeparableSmooth.h>
#endif
#include <vtkCallbackCommand.h>
#include <vtkImageAlgorithm.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkVersion.h>

// STD includes
#include <cassert>
#include <iostream>
#include <chrono>

#define UNUSED(expr) (void)(expr)

namespace
{
//----------------------------------------------------------------------------
// WebGPU implementation of each CPU smoothing filter (same parameters).
template <class CPUFilter>
struct WebGPUFilter
{
  using Type = CPUFilter;
};
#ifdef SLICERASTRO_HAS_WEBGPU
template <>
struct WebGPUFilter<vtkAstroImageKernelSmooth>
{
  using Type = vtkAstroWebGPUImageKernelSmooth;
};
template <>
struct WebGPUFilter<vtkAstroImageSeparableSmooth>
{
  using Type = vtkAstroWebGPUImageSeparableSmooth;
};
template <>
struct WebGPUFilter<vtkAstroImageGradientSmooth>
{
  using Type = vtkAstroWebGPUImageGradientSmooth;
};
#endif

//----------------------------------------------------------------------------
template <class CPUFilter>
vtkSmartPointer<CPUFilter> NewSmoothFilter(vtkObject* webGPUDevice)
{
#ifdef SLICERASTRO_HAS_WEBGPU
  if (webGPUDevice)
    {
    vtkSmartPointer<typename WebGPUFilter<CPUFilter>::Type> filter =
      vtkSmartPointer<typename WebGPUFilter<CPUFilter>::Type>::New();
    filter->SetDevice(vtkAstroWebGPUDevice::SafeDownCast(webGPUDevice));
    return filter;
    }
#else
  (void)webGPUDevice;
#endif
  return vtkSmartPointer<CPUFilter>::New();
}
} // namespace

//----------------------------------------------------------------------------
class vtkSlicerAstroSmoothingLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
#ifdef SLICERASTRO_HAS_WEBGPU
  // WebGPU device reused by all the smoothing runs
  vtkSmartPointer<vtkAstroWebGPUDevice> WebGPUDevice;
#endif
};

//----------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
}

//---------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroSmoothingLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic::vtkSlicerAstroSmoothingLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic::~vtkSlicerAstroSmoothingLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroSmoothingLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroSmoothingLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkSlicerAstroSmoothingLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroSmoothingLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroSmoothingLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroSmoothingParametersNode* pNode = vtkMRMLAstroSmoothingParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::Apply(vtkMRMLAstroSmoothingParametersNode* pnode,
                                        vtkRenderWindow* renderWindow)
{
  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::Apply : "
                  "parameterNode not found.");
    return 0;
    }

  // GPU backends: WebGPU (if available) runs the same code paths as the CPU
  // with the WebGPU implementations of the filters; otherwise OpenGL (the
  // render window, if any, provides the OpenGL context).
  bool openGL = pnode->GetHardware() != 0;
  this->UseWebGPU = false;
#ifdef SLICERASTRO_HAS_WEBGPU
  if (openGL && vtkAstroWebGPUDevice::IsAvailable())
    {
    this->UseWebGPU = true;
    openGL = false;
    }
#endif

  int success = 0;
  switch (pnode->GetFilter())
    {
    case 0:
      {
      if (!openGL)
        {
        if (fabs(pnode->GetParameterX() - pnode->GetParameterY()) < 0.001 &&
            fabs(pnode->GetParameterY() - pnode->GetParameterZ()) < 0.001)
          {
          success = this->IsotropicBoxCPUFilter(pnode);
          }
        else
          {
          success = this->AnisotropicBoxCPUFilter(pnode);
          }
        }
      else
        {
        success = this->BoxGPUFilter(pnode, renderWindow);
        }
      break;
      }
    case 1:
      {
        if (!openGL)
          {
          if (fabs(pnode->GetParameterX() - pnode->GetParameterY()) < 0.001 &&
              fabs(pnode->GetParameterY() - pnode->GetParameterZ()) < 0.001)
            {
            success = this->IsotropicGaussianCPUFilter(pnode);
            }
          else
            {
            success = this->AnisotropicGaussianCPUFilter(pnode);
            }
          }
        else
          {
          success = this->GaussianGPUFilter(pnode, renderWindow);
          }
      break;
      }
    case 2:
      {
      if (!openGL)
        {
        success = this->GradientCPUFilter(pnode);
        }
      else
        {
        success = this->GradientGPUFilter(pnode, renderWindow);
        }
      break;
      }
    }
  return success;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::RunSmoothingFilter(vtkMRMLAstroSmoothingParametersNode* pnode,
                                                     vtkImageAlgorithm* filter,
                                                  const char* filterName)
{
  if (!pnode || !filter || !this->GetMRMLScene())
    {
    return 0;
    }

  vtkMRMLAstroVolumeNode *inputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::RunSmoothingFilter : inputVolume not found.");
    return 0;
    }
  vtkMRMLAstroVolumeNode *outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::RunSmoothingFilter : outputVolume not found.");
    return 0;
    }
  if (inputVolume->GetImageData()->GetNumberOfScalarComponents() != 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::RunSmoothingFilter : "
                  "imageData with more than one components.");
    return 0;
    }

  // Progress events are invoked from this thread only: report them to the
  // parameters node (GUI progress bar) and forward the cancel request.
  vtkNew<vtkCallbackCommand> progressCallback;
  progressCallback->SetClientData(pnode);
  progressCallback->SetCallback([](vtkObject* caller, unsigned long, void* clientData, void* callData)
    {
    vtkMRMLAstroSmoothingParametersNode* parametersNode =
      static_cast<vtkMRMLAstroSmoothingParametersNode*>(clientData);
    vtkAlgorithm* algorithm = vtkAlgorithm::SafeDownCast(caller);
    if (parametersNode->GetStatus() == -1)
      {
      algorithm->SetAbortExecute(1);
      return;
      }
    const double progress = callData ? *static_cast<double*>(callData) : 0.;
    const int status = 1 + static_cast<int>(progress * 98.);
    if (status > parametersNode->GetStatus())
      {
      parametersNode->SetStatus(status);
      }
    });
  filter->AddObserver(vtkCommand::ProgressEvent, progressCallback);

  pnode->SetStatus(1);
  const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  filter->SetInputData(inputVolume->GetImageData());
  filter->Update();
  const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  vtkDebugMacro(<< filterName << " Time : "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms.");

  if (filter->GetAbortExecute() || pnode->GetStatus() == -1)
    {
    pnode->SetStatus(100);
    return 0;
    }
  if (filter->GetErrorCode() || !filter->GetOutput() || !filter->GetOutput()->GetPointData()->GetScalars())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::RunSmoothingFilter : " << filterName << " failed.");
    pnode->SetStatus(100);
    return 0;
    }

  // The output volume takes the filter output buffer (shallow copy, no voxel copy)
  vtkNew<vtkImageData> outputImage;
  outputImage->ShallowCopy(filter->GetOutput());
  outputImage->SetOrigin(0., 0., 0.);
  outputImage->SetSpacing(1., 1., 1.);
  int wasModifying = outputVolume->StartModify();
  outputVolume->SetAndObserveImageData(outputImage);
  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();
  outputVolume->EndModify(wasModifying);

  pnode->SetStatus(100);
  return 1;
}

//----------------------------------------------------------------------------
vtkObject* vtkSlicerAstroSmoothingLogic::WebGPUDevice()
{
#ifdef SLICERASTRO_HAS_WEBGPU
  if (this->UseWebGPU)
    {
    if (!this->Internal->WebGPUDevice)
      {
      this->Internal->WebGPUDevice = vtkSmartPointer<vtkAstroWebGPUDevice>::New();
      }
    return this->Internal->WebGPUDevice;
    }
#endif
  return nullptr;
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroSmoothingLogic::FilterName(const char* name) const
{
  return std::string(name) + (this->UseWebGPU ? " (GPU, WebGPU)" : " (CPU)");
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroSmoothingLogic::GetGPUBackend()
{
#ifdef SLICERASTRO_HAS_WEBGPU
  if (vtkAstroWebGPUDevice::IsAvailable())
    {
    return "WebGPU";
    }
#endif
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  return "OpenGL";
#else
  return "";
#endif
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroSmoothingLogic::GetGPUBackendDescription()
{
  const std::string backend = vtkSlicerAstroSmoothingLogic::GetGPUBackend();
#ifdef SLICERASTRO_HAS_WEBGPU
  if (backend == "WebGPU")
    {
    return backend + ": " + vtkAstroWebGPUDevice::GetDescription();
    }
#endif
  return backend;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::AnisotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  if (!pnode)
    {
    return 0;
    }
  vtkSmartPointer<vtkAstroImageKernelSmooth> filter = NewSmoothFilter<vtkAstroImageKernelSmooth>(this->WebGPUDevice());
  filter->SetBoxKernel(static_cast<int>(pnode->GetParameterX()),
                       static_cast<int>(pnode->GetParameterY()),
                       static_cast<int>(pnode->GetParameterZ()));
  return this->RunSmoothingFilter(pnode, filter, this->FilterName("Box Filter").c_str());
}


//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::IsotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  if (!pnode)
    {
    return 0;
    }
  // Separable box: the kernel size is ParameterX along the axes with a
  // non-zero parameter.
  const int size = static_cast<int>(pnode->GetParameterX());
  vtkSmartPointer<vtkAstroImageSeparableSmooth> filter = NewSmoothFilter<vtkAstroImageSeparableSmooth>(this->WebGPUDevice());
  filter->SetAxisBoxKernel(0, pnode->GetParameterX() > 0.001 ? size : 0);
  filter->SetAxisBoxKernel(1, pnode->GetParameterY() > 0.001 ? size : 0);
  filter->SetAxisBoxKernel(2, pnode->GetParameterZ() > 0.001 ? size : 0);
  return this->RunSmoothingFilter(pnode, filter, this->FilterName("Isotropic Box Filter").c_str());
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::BoxGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode,
                                               vtkRenderWindow* renderWindow)
{
  if (!pnode || !this->GetMRMLScene())
    {
    return 0;
    }
#ifndef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic: SlicerAstro has been built without OpenGL smoothing support.");
  return 0;
#else
  vtkNew<vtkAstroOpenGLImageBox> filter;
  filter->SetKernelLength(pnode->GetParameterX(), pnode->GetParameterY(), pnode->GetParameterZ());
  // a null render window lets the filter create its own offscreen context
  filter->SetRenderWindow(renderWindow);
  return this->RunSmoothingFilter(pnode, filter, "Box Filter (GPU, OpenGL)");
#endif
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::AnisotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  if (!pnode || !pnode->GetGaussianKernel3D())
    {
    return 0;
    }
  // 3D (possibly rotated) Gaussian kernel computed by the parameters node
  vtkSmartPointer<vtkAstroImageKernelSmooth> filter = NewSmoothFilter<vtkAstroImageKernelSmooth>(this->WebGPUDevice());
  filter->SetKernel(pnode->GetGaussianKernel3D(),
                    static_cast<int>(pnode->GetKernelLengthX()),
                    static_cast<int>(pnode->GetKernelLengthY()),
                    static_cast<int>(pnode->GetKernelLengthZ()));
  return this->RunSmoothingFilter(pnode, filter, this->FilterName("Gaussian Filter").c_str());
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::IsotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  if (!pnode || !pnode->GetGaussianKernel1D())
    {
    return 0;
    }
  // Separable Gaussian: 1D kernel computed by the parameters node, applied
  // along the axes with a non-zero parameter.
  vtkDoubleArray* kernel1D = pnode->GetGaussianKernel1D();
  vtkSmartPointer<vtkAstroImageSeparableSmooth> filter = NewSmoothFilter<vtkAstroImageSeparableSmooth>(this->WebGPUDevice());
  filter->SetAxisKernel(0, pnode->GetParameterX() > 0.001 ? kernel1D : nullptr);
  filter->SetAxisKernel(1, pnode->GetParameterY() > 0.001 ? kernel1D : nullptr);
  filter->SetAxisKernel(2, pnode->GetParameterZ() > 0.001 ? kernel1D : nullptr);
  return this->RunSmoothingFilter(pnode, filter, this->FilterName("Isotropic Gaussian Filter").c_str());
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::GaussianGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode,
                                                    vtkRenderWindow* renderWindow)
{
  if (!pnode || !this->GetMRMLScene())
    {
    return 0;
    }
#ifndef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic: SlicerAstro has been built without OpenGL smoothing support.");
  return 0;
#else
  vtkNew<vtkAstroOpenGLImageGaussian> filter;
  filter->SetKernelLength(pnode->GetKernelLengthX(), pnode->GetKernelLengthY(), pnode->GetKernelLengthZ());
  filter->SetFWHM(pnode->GetParameterX(), pnode->GetParameterY(), pnode->GetParameterZ());
  filter->SetRotationAngles(pnode->GetRx(), pnode->GetRy(), pnode->GetRz());
  filter->SetRenderWindow(renderWindow);
  return this->RunSmoothingFilter(pnode, filter, "Gaussian Filter (GPU, OpenGL)");
#endif
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::GradientCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  if (!pnode || !this->GetMRMLScene())
    {
    return 0;
    }
  vtkMRMLAstroVolumeNode *inputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume)
    {
    return 0;
    }
  vtkSmartPointer<vtkAstroImageGradientSmooth> filter = NewSmoothFilter<vtkAstroImageGradientSmooth>(this->WebGPUDevice());
  filter->SetConductance(pnode->GetParameterX(), pnode->GetParameterY(), pnode->GetParameterZ());
  filter->SetTimeStep(pnode->GetTimeStep());
  filter->SetK(pnode->GetK());
  filter->SetNoise(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DisplayThreshold")));
  filter->SetNumberOfIterations(pnode->GetAccuracy());
  return this->RunSmoothingFilter(pnode, filter, this->FilterName("Intensity driven Gradient Filter").c_str());
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::GradientGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode,
                                                    vtkRenderWindow* renderWindow)
{
  if (!pnode || !this->GetMRMLScene())
    {
    return 0;
    }
#ifndef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic: SlicerAstro has been built without OpenGL smoothing support.");
  return 0;
#else
  vtkMRMLAstroVolumeNode *inputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume)
    {
    return 0;
    }
  vtkNew<vtkAstroOpenGLImageGradient> filter;
  filter->SetCl(pnode->GetParameterX(), pnode->GetParameterY(), pnode->GetParameterZ());
  filter->SetK(pnode->GetK());
  filter->SetAccuracy(pnode->GetAccuracy());
  filter->SetTimeStep(pnode->GetTimeStep());
  filter->SetRMS(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DisplayThreshold")));
  filter->SetRenderWindow(renderWindow);
  return this->RunSmoothingFilter(pnode, filter, "Intensity driven Gradient Filter (GPU, OpenGL)");
#endif
}
