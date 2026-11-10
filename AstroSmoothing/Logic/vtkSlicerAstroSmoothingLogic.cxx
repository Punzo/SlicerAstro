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
#include <vtkCallbackCommand.h>
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

// OpenMP includes

#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENGL
// vtkOpenGL includes
#include "vtk_glew.h"
#endif

#define UNUSED(expr) (void)(expr)

//----------------------------------------------------------------------------
class vtkSlicerAstroSmoothingLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkImageData> tempVolumeData;
};

//----------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
  this->tempVolumeData = vtkSmartPointer<vtkImageData>::New();
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

//----------------------------------------------------------------------------
template <typename T> bool isNaN(T value)
{
  return value != value;
}

//----------------------------------------------------------------------------
bool DoubleIsNaN(double Value)
{
  return isNaN<double>(Value);
}

//----------------------------------------------------------------------------
bool FloatIsNaN(float Value)
{
  return isNaN<float>(Value);
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

  if (!renderWindow)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::Apply : "
                  "renderWindow not found.");
    return 0;
    }

  int success = 0;
  switch (pnode->GetFilter())
    {
    case 0:
      {
      if (!(pnode->GetHardware()))
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
        if (!(pnode->GetHardware()))
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
      if (!(pnode->GetHardware()))
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
int vtkSlicerAstroSmoothingLogic::RunCPUSmoothing(vtkMRMLAstroSmoothingParametersNode* pnode,
                                                  vtkAstroImageSmoothBase* filter,
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
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::RunCPUSmoothing : inputVolume not found.");
    return 0;
    }
  vtkMRMLAstroVolumeNode *outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::RunCPUSmoothing : outputVolume not found.");
    return 0;
    }
  if (inputVolume->GetImageData()->GetNumberOfScalarComponents() != 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::RunCPUSmoothing : "
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
  const auto start = std::chrono::steady_clock::now();
  filter->SetInputData(inputVolume->GetImageData());
  filter->Update();
  const auto end = std::chrono::steady_clock::now();
  vtkDebugMacro(<< filterName << " Time : "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms.");

  if (filter->GetAborted() || pnode->GetStatus() == -1)
    {
    pnode->SetStatus(100);
    return 0;
    }
  if (filter->GetErrorCode() || !filter->GetOutput() || !filter->GetOutput()->GetPointData()->GetScalars())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::RunCPUSmoothing : " << filterName << " failed.");
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
int vtkSlicerAstroSmoothingLogic::AnisotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  if (!pnode)
    {
    return 0;
    }
  vtkNew<vtkAstroImageKernelSmooth> filter;
  filter->SetBoxKernel(static_cast<int>(pnode->GetParameterX()),
                       static_cast<int>(pnode->GetParameterY()),
                       static_cast<int>(pnode->GetParameterZ()));
  return this->RunCPUSmoothing(pnode, filter, "Box Filter (CPU)");
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
  vtkNew<vtkAstroImageSeparableSmooth> filter;
  filter->SetAxisBoxKernel(0, pnode->GetParameterX() > 0.001 ? size : 0);
  filter->SetAxisBoxKernel(1, pnode->GetParameterY() > 0.001 ? size : 0);
  filter->SetAxisBoxKernel(2, pnode->GetParameterZ() > 0.001 ? size : 0);
  return this->RunCPUSmoothing(pnode, filter, "Isotropic Box Filter (CPU)");
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::BoxGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode,
                                               vtkRenderWindow* renderWindow)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  UNUSED(renderWindow);
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::BoxGPUFilter "
                  "this release of SlicerAstro has been built "
                  "without OpenGL filtering support.");
  return 0;
  #else

  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::BoxGPUFilter : "
                  "parameterNode not found.");
    return 0;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::BoxGPUFilter :"
                  " scene not found.");
    return 0;
    }

  pnode->SetStatus(1);

  bool cancel = false;

  std::chrono::steady_clock::time_point start, end;
  long mtime = 0;
  start = std::chrono::steady_clock::now();

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::BoxGPUFilter : "
                  "inputVolume not found.");
    pnode->SetStatus(100);
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume || !outputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::BoxGPUFilter : "
                  "outputVolume not found.");
    pnode->SetStatus(100);
    return 0;
    }

  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::BoxGPUFilter : "
                  "imageData with more than one components.");
    pnode->SetStatus(100);
    return 0;
    }

  vtkNew<vtkAstroOpenGLImageBox> filter;
  outputVolume->GetImageData()->DeepCopy(inputVolume->GetImageData());
  filter->SetInputData(outputVolume->GetImageData());
  filter->SetKernelLength(pnode->GetParameterX(),
                          pnode->GetParameterY(),
                          pnode->GetParameterZ());
  filter->SetRenderWindow(renderWindow);

  pnode->SetStatus(20);

  if (pnode->GetStatus() == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {
    filter->Update();
    }

  pnode->SetStatus(70);

  outputVolume->GetImageData()->DeepCopy(filter->GetOutput());

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  vtkDebugMacro(" Box Filter (GPU, OpenGL) Time : "<<mtime<<" ms.");

  start = std::chrono::steady_clock::now();

  int wasModifying = outputVolume->StartModify();
  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();
  outputVolume->EndModify(wasModifying);

  pnode->SetStatus(100);

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Update Time : "<<mtime<<" ms.");


  if(cancel)
    {
    return 0;
    }

  return 1;
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENGL
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::AnisotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  if (!pnode || !pnode->GetGaussianKernel3D())
    {
    return 0;
    }
  // 3D (possibly rotated) Gaussian kernel computed by the parameters node
  vtkNew<vtkAstroImageKernelSmooth> filter;
  filter->SetKernel(pnode->GetGaussianKernel3D(),
                    static_cast<int>(pnode->GetKernelLengthX()),
                    static_cast<int>(pnode->GetKernelLengthY()),
                    static_cast<int>(pnode->GetKernelLengthZ()));
  return this->RunCPUSmoothing(pnode, filter, "Gaussian Filter (CPU)");
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
  vtkNew<vtkAstroImageSeparableSmooth> filter;
  filter->SetAxisKernel(0, pnode->GetParameterX() > 0.001 ? kernel1D : nullptr);
  filter->SetAxisKernel(1, pnode->GetParameterY() > 0.001 ? kernel1D : nullptr);
  filter->SetAxisKernel(2, pnode->GetParameterZ() > 0.001 ? kernel1D : nullptr);
  return this->RunCPUSmoothing(pnode, filter, "Isotropic Gaussian Filter (CPU)");
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::GaussianGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode,
                                                    vtkRenderWindow *renderWindow)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  UNUSED(renderWindow);
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::GaussianGPUFilter "
                  "this release of SlicerAstro has been built "
                  "without OpenGL filtering support.");
  return 0;
  #else

  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GaussianGPUFilter : "
                  "parameterNode not found.");
    return 0;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GaussianGPUFilter :"
                  " scene not found.");
    return 0;
    }

  pnode->SetStatus(1);

  bool cancel = false;

  std::chrono::steady_clock::time_point start, end;
  long mtime = 0;
  start = std::chrono::steady_clock::now();

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GaussianGPUFilter : "
                  "inputVolume not found.");
    pnode->SetStatus(100);
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume || !outputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GaussianGPUFilter : "
                  "outputVolume not found.");
    pnode->SetStatus(100);
    return 0;
    }

  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GaussianGPUFilter : "
                  "imageData with more than one components.");
    pnode->SetStatus(100);
    return 0;
    }

  vtkNew<vtkAstroOpenGLImageGaussian> filter;
  outputVolume->GetImageData()->DeepCopy(inputVolume->GetImageData());
  filter->SetInputData(outputVolume->GetImageData());
  filter->SetKernelLength(pnode->GetKernelLengthX(),
                          pnode->GetKernelLengthY(),
                          pnode->GetKernelLengthZ());
  filter->SetFWHM(pnode->GetParameterX(),
                  pnode->GetParameterY(),
                  pnode->GetParameterZ());
  filter->SetRotationAngles(pnode->GetRx(),
                            pnode->GetRy(),
                            pnode->GetRz());
  filter->SetRenderWindow(renderWindow);

  pnode->SetStatus(20);

  if (pnode->GetStatus() == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {
    filter->Update();
    }

  pnode->SetStatus(70);

  outputVolume->GetImageData()->DeepCopy(filter->GetOutput());

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  vtkDebugMacro("Gaussian Filter (GPU, OpenGL) Time : "<<mtime<<" ms.");

  start = std::chrono::steady_clock::now();

  int wasModifying = outputVolume->StartModify();
  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();
  outputVolume->EndModify(wasModifying);

  pnode->SetStatus(100);

  end = std::chrono::steady_clock::now();
    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  vtkDebugMacro("Update : "<<mtime<<" ms.");

  if (cancel)
    {
    return 0;
    }

  return 1;
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENGL
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
  vtkNew<vtkAstroImageGradientSmooth> filter;
  filter->SetConductance(pnode->GetParameterX(), pnode->GetParameterY(), pnode->GetParameterZ());
  filter->SetTimeStep(pnode->GetTimeStep());
  filter->SetK(pnode->GetK());
  filter->SetNoise(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DisplayThreshold")));
  filter->SetNumberOfIterations(pnode->GetAccuracy());
  return this->RunCPUSmoothing(pnode, filter, "Intensity driven Gradient Filter (CPU)");
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::GradientGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode,
                                                    vtkRenderWindow* renderWindow)
{  
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  UNUSED(renderWindow);
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::GradientGPUFilter "
                  "this release of SlicerAstro has been built "
                  "without OpenGL filtering support.");
  return 0;
  #else

  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientGPUFilter : "
                  "parameterNode not found.");
    return 0;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientGPUFilter :"
                  " scene not found.");
    return 0;
    }

  pnode->SetStatus(1);

  bool cancel = false;

  std::chrono::steady_clock::time_point start, end;
  long mtime = 0;
  start = std::chrono::steady_clock::now();

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientGPUFilter : "
                  "inputVolume not found.");
    pnode->SetStatus(100);
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume || !outputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientGPUFilter : "
                  "outputVolume not found.");
    pnode->SetStatus(100);
    return 0;
    }

  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientGPUFilter : "
                  "imageData with more than one components.");
    pnode->SetStatus(100);
    return 0;
    }

  vtkNew<vtkAstroOpenGLImageGradient> filter;
  outputVolume->GetImageData()->DeepCopy(inputVolume->GetImageData());
  filter->SetInputData(outputVolume->GetImageData());
  filter->SetCl(pnode->GetParameterX(),
                pnode->GetParameterY(),
                pnode->GetParameterZ());
  filter->SetK(pnode->GetK());
  filter->SetAccuracy(pnode->GetAccuracy());
  filter->SetTimeStep(pnode->GetTimeStep());
  filter->SetRMS(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DisplayThreshold")));

  filter->SetRenderWindow(renderWindow);

  pnode->SetStatus(20);

  if (pnode->GetStatus() == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {
    filter->Update();
    }

  pnode->SetStatus(70);

  outputVolume->GetImageData()->DeepCopy(filter->GetOutput());

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  vtkDebugMacro(" Intensity-Driven Gradient Filter (GPU, OpenGL) Time : "<<mtime<<" ms.");

  start = std::chrono::steady_clock::now();

  int wasModifying = outputVolume->StartModify();
  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();
  outputVolume->EndModify(wasModifying);

  pnode->SetStatus(100);

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Update Time : "<<mtime<<" ms.");

  if(cancel)
    {
    return 0;
    }

  return 1;
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENGL
}
