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
#include <vtkImageAlgorithm.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <cmath>
#include <algorithm>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkSMPTools.h>
#include <vtkAstroBeams.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkMath.h>
#include <cstdio>
#include <functional>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkVersion.h>

// STD includes
#include <cassert>
#include <iostream>
#include <chrono>

#define UNUSED(expr) (void)(expr)


//----------------------------------------------------------------------------
class vtkSlicerAstroSmoothingLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  /// done and synchronous of the Apply in progress (StartApply), taken by
  /// RunSmoothingFilter for its job
  std::function<void(bool)> PendingDone;
  bool PendingSynchronous{true};
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
  int result = 0;
  const bool started = this->StartApply(pnode, renderWindow, [&result](bool success) { result = success ? 1 : 0; },
                                        true);
  return started ? result : 0;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroSmoothingLogic::StartApply(vtkMRMLAstroSmoothingParametersNode* pnode,
                                              vtkRenderWindow* renderWindow,
                                              const std::function<void(bool)>& done, bool synchronous)
{
  this->Internal->PendingDone = done;
  this->Internal->PendingSynchronous = synchronous;
  const int started = this->ApplyFilter(pnode, renderWindow);
  if (!started && this->Internal->PendingDone)
    {
    // failed before the filter ran: done is not called
    this->Internal->PendingDone = nullptr;
    }
  this->Internal->PendingSynchronous = true;
  return started != 0;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::ApplyFilter(vtkMRMLAstroSmoothingParametersNode* pnode,
                                              vtkRenderWindow* renderWindow)
{
  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::Apply : "
                  "parameterNode not found.");
    return 0;
    }

  // GPU backend: OpenGL (the render window, if any, provides the OpenGL
  // context; otherwise the filters create their own)
  bool openGL = pnode->GetHardware() != 0;

  // images (2D) and spectra (1D): no smoothing along the missing axes (and
  // no rotation out of the plane); the parameters of the user are restored
  vtkMRMLAstroVolumeNode* input = this->GetMRMLScene() && pnode->GetInputVolumeNodeID() ?
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID())) : nullptr;
  int dims[3] = {1, 1, 1};
  if (input && input->GetImageData())
    {
    input->GetImageData()->GetDimensions(dims);
    }
  const double userParameters[5] = {pnode->GetParameterY(), pnode->GetParameterZ(), pnode->GetRx(),
                                    pnode->GetRy(), pnode->GetRz()};
  const bool lowerDimensions = dims[2] == 1 || dims[1] == 1;
  if (lowerDimensions && openGL)
    {
    // the OpenGL filters handle cubes only (one-slice textures give wrong
    // results): images and spectra are smoothed by the CPU filters
    vtkInfoMacro("Smoothing: images and spectra are smoothed on the CPU (the OpenGL filters handle cubes).");
    openGL = false;
    }
  if (lowerDimensions)
    {
    const int wasModifying = pnode->StartModify();
    pnode->SetParameterZ(0.);
    pnode->SetRx(0.);
    pnode->SetRy(0.);
    if (dims[1] == 1)
      {
      pnode->SetParameterY(0.);
      pnode->SetRz(0.);
      }
    pnode->SetGaussianKernels();
    pnode->EndModify(wasModifying);
    }

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
  if (lowerDimensions)
    {
    const int wasModifying = pnode->StartModify();
    pnode->SetParameterY(userParameters[0]);
    pnode->SetParameterZ(userParameters[1]);
    pnode->SetRx(userParameters[2]);
    pnode->SetRy(userParameters[3]);
    pnode->SetRz(userParameters[4]);
    pnode->SetGaussianKernels();
    pnode->EndModify(wasModifying);
    }
  return success;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::RunSmoothingFilter(vtkMRMLAstroSmoothingParametersNode* pnode,
                                                     vtkImageAlgorithm* filter,
                                                     const char* filterName,
                                                     bool inBackground)
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

  // The filter runs in the background runner of the AstroVolume logic (e.g. a worker thread,
  // the GUI responsive with a progress dialog). Its progress goes to an atomic
  // counter (no MRML events from a worker thread) and the cancel request
  // (CancelSmoothing, or the status -1 of the parameters node) aborts it.
  this->SmoothingProgress = 0;
  this->SmoothingCancelRequested = pnode->GetStatus() == -1;
  vtkNew<vtkCallbackCommand> progressCallback;
  progressCallback->SetClientData(this);
  progressCallback->SetCallback([](vtkObject* caller, unsigned long, void* clientData, void* callData)
    {
    vtkSlicerAstroSmoothingLogic* self = static_cast<vtkSlicerAstroSmoothingLogic*>(clientData);
    vtkAlgorithm* algorithm = vtkAlgorithm::SafeDownCast(caller);
    if (self->SmoothingCancelRequested)
      {
      algorithm->SetAbortExecute(1);
      return;
      }
    const double progress = callData ? *static_cast<double*>(callData) : 0.;
    const int value = 1 + static_cast<int>(progress * 98.);
    if (value > self->SmoothingProgress)
      {
      self->SmoothingProgress = value;
      }
    });
  filter->AddObserver(vtkCommand::ProgressEvent, progressCallback);

  pnode->SetStatus(1);
  const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  filter->SetInputData(inputVolume->GetImageData());
  // the job: the filter (it holds its input), the nodes and a copy of the
  // parameters (Apply restores the parameters of 1D/2D data before the end of
  // a job that does not block the user interface)
  std::function<void(bool)> done = this->Internal->PendingDone;
  const bool synchronous = this->Internal->PendingSynchronous || !inBackground ||
                           !this->Internal->AstroVolumeLogic;
  this->Internal->PendingDone = nullptr;
  vtkSmartPointer<vtkImageAlgorithm> filterHold = filter;
  vtkSmartPointer<vtkMRMLAstroSmoothingParametersNode> parameters = pnode;
  vtkSmartPointer<vtkMRMLAstroSmoothingParametersNode> parametersCopy =
    vtkSmartPointer<vtkMRMLAstroSmoothingParametersNode>::New();
  parametersCopy->Copy(pnode);
  vtkSmartPointer<vtkMRMLAstroVolumeNode> inputHold = inputVolume;
  vtkSmartPointer<vtkMRMLAstroVolumeNode> outputHold = outputVolume;
  vtkSmartPointer<vtkCallbackCommand> progressHold = progressCallback.GetPointer();
  const std::string name = filterName ? filterName : "";
  std::function<void()> update = [filterHold]() { filterHold->Update(); };
  std::function<void()> finish = [this, filterHold, parameters, parametersCopy, inputHold, outputHold,
                                  progressHold, name, start, done]()
    {
    vtkImageAlgorithm* filter = filterHold;
    vtkMRMLAstroSmoothingParametersNode* pnode = parameters;
    vtkMRMLAstroVolumeNode* inputVolume = inputHold;
    vtkMRMLAstroVolumeNode* outputVolume = outputHold;
    vtkCallbackCommand* progressCallback = progressHold;
    const char* filterName = name.c_str();
      filter->RemoveObserver(progressCallback);
      const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
      vtkDebugMacro(<< filterName << " Time : "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms.");
      if (this->SmoothingCancelRequested)
        {
        pnode->SetStatus(-1);
        }

      if (filter->GetAbortExecute() || pnode->GetStatus() == -1)
        {
        pnode->SetStatus(100);
        if (done)
          {
          done(false);
          }
        return;
        }
      if (filter->GetErrorCode() || !filter->GetOutput() || !filter->GetOutput()->GetPointData()->GetScalars())
        {
        vtkErrorMacro("vtkSlicerAstroSmoothingLogic::RunSmoothingFilter : " << filterName << " failed.");
        pnode->SetStatus(100);
        if (done)
          {
          done(false);
          }
        return;
        }

      // The output volume takes the filter output buffer (shallow copy, no voxel copy)
      vtkNew<vtkImageData> outputImage;
      outputImage->ShallowCopy(filter->GetOutput());
      outputImage->SetOrigin(0., 0., 0.);
      outputImage->SetSpacing(1., 1., 1.);
      int wasModifying = outputVolume->StartModify();
      outputVolume->SetAndObserveImageData(outputImage);
      // Gaussian smoothing: the beam of the output is the beam of the input
      // convolved with the kernel; Jy/beam data are scaled by the ratio of the
      // beam areas, so that point sources keep their peak (CASA imsmooth)
      double newBeam[3], kernel[3];
      if (ComputeSmoothedBeam(inputVolume, parametersCopy, newBeam, kernel))
        {
        const char* unit = inputVolume->GetAttribute("SlicerAstro.BUNIT");
        std::string bunit = unit ? unit : "";
        std::transform(bunit.begin(), bunit.end(), bunit.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        bunit.erase(std::remove(bunit.begin(), bunit.end(), ' '), bunit.end());
        double oldBeam[2] = {atof(inputVolume->GetAttribute("SlicerAstro.BMAJ")) * 3600.,
                             atof(inputVolume->GetAttribute("SlicerAstro.BMIN")) * 3600.};
        if (bunit == "JY/BEAM")
          {
          const double scale = vtkAstroBeams::Area(newBeam[0], newBeam[1]) / vtkAstroBeams::Area(oldBeam[0], oldBeam[1]);
          vtkDataArray* scalars = outputImage->GetPointData()->GetScalars();
          vtkFloatArray* floats = vtkFloatArray::SafeDownCast(scalars);
          vtkDoubleArray* doubles = vtkDoubleArray::SafeDownCast(scalars);
          if (floats)
            {
            float* values = floats->GetPointer(0);
            const float factor = static_cast<float>(scale);
            vtkSMPTools::For(0, floats->GetNumberOfValues(), [values, factor](vtkIdType begin, vtkIdType end)
              {
              for (vtkIdType i = begin; i < end; i++)
                {
                values[i] *= factor;
                }
              });
            }
          else if (doubles)
            {
            double* values = doubles->GetPointer(0);
            vtkSMPTools::For(0, doubles->GetNumberOfValues(), [values, scale](vtkIdType begin, vtkIdType end)
              {
              for (vtkIdType i = begin; i < end; i++)
                {
                values[i] *= scale;
                }
              });
            }
          scalars->Modified();
          }
        char value[64];
        snprintf(value, sizeof(value), "%.17g", newBeam[0] / 3600.);
        outputVolume->SetAttribute("SlicerAstro.BMAJ", value);
        snprintf(value, sizeof(value), "%.17g", newBeam[1] / 3600.);
        outputVolume->SetAttribute("SlicerAstro.BMIN", value);
        snprintf(value, sizeof(value), "%.17g", newBeam[2]);
        outputVolume->SetAttribute("SlicerAstro.BPA", value);
        }
      // spectral smoothing: the spectral resolution of the output (3D
      // resolution element of the 3D views)
      const double spectralFWHM = ComputeSmoothedSpectralFWHM(inputVolume, parametersCopy);
      if (spectralFWHM > 0.)
        {
        char spectralValue[64];
        snprintf(spectralValue, sizeof(spectralValue), "%.17g", spectralFWHM);
        outputVolume->SetAttribute("SlicerAstro.SpectralFWHM", spectralValue);
        }
      outputVolume->UpdateRangeAttributes();
      outputVolume->UpdateDisplayThresholdAttributes();
      outputVolume->EndModify(wasModifying);

    pnode->SetStatus(100);
    if (done)
      {
      done(true);
      }
    };
  if (!synchronous)
    {
    this->Internal->AstroVolumeLogic->StartInBackground("Smoothing the data...", update,
      [this]() { return this->GetSmoothingProgress(); },
      [this]() { this->CancelSmoothing(); }, finish, false);
    }
  else
    {
    if (inBackground && this->Internal->AstroVolumeLogic)
      {
      this->Internal->AstroVolumeLogic->RunInBackground("Smoothing the data...", update,
        [this]() { return this->GetSmoothingProgress(); },
        [this]() { this->CancelSmoothing(); });
      }
    else
      {
      update();
      }
    finish();
    }
  return 1;
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroSmoothingLogic::FilterName(const char* name) const
{
  return std::string(name) + " (CPU)";
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroSmoothingLogic::GetGPUBackend()
{
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  return "OpenGL";
#else
  return "";
#endif
}

//----------------------------------------------------------------------------
std::string vtkSlicerAstroSmoothingLogic::GetGPUBackendDescription()
{
  return vtkSlicerAstroSmoothingLogic::GetGPUBackend();
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::AnisotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  if (!pnode)
    {
    return 0;
    }
  vtkSmartPointer<vtkAstroImageKernelSmooth> filter = vtkSmartPointer<vtkAstroImageKernelSmooth>::New();
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
  vtkSmartPointer<vtkAstroImageSeparableSmooth> filter = vtkSmartPointer<vtkAstroImageSeparableSmooth>::New();
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
  return this->RunSmoothingFilter(pnode, filter, "Box Filter (GPU, OpenGL)", false);
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
  vtkSmartPointer<vtkAstroImageKernelSmooth> filter = vtkSmartPointer<vtkAstroImageKernelSmooth>::New();
  // the filter gets its own copy of the kernel (a few values, not data): the
  // parameters node recomputes its kernels in place when the parameters
  // change, also while the filter runs in the background
  vtkNew<vtkDoubleArray> kernel3D;
  kernel3D->DeepCopy(pnode->GetGaussianKernel3D());
  filter->SetKernel(kernel3D,
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
  // own copy of the kernel (see AnisotropicGaussianCPUFilter)
  vtkNew<vtkDoubleArray> kernel1D;
  kernel1D->DeepCopy(pnode->GetGaussianKernel1D());
  vtkSmartPointer<vtkAstroImageSeparableSmooth> filter = vtkSmartPointer<vtkAstroImageSeparableSmooth>::New();
  filter->SetAxisKernel(0, pnode->GetParameterX() > 0.001 ? kernel1D.GetPointer() : nullptr);
  filter->SetAxisKernel(1, pnode->GetParameterY() > 0.001 ? kernel1D.GetPointer() : nullptr);
  filter->SetAxisKernel(2, pnode->GetParameterZ() > 0.001 ? kernel1D.GetPointer() : nullptr);
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
  return this->RunSmoothingFilter(pnode, filter, "Gaussian Filter (GPU, OpenGL)", false);
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
  vtkSmartPointer<vtkAstroImageGradientSmooth> filter = vtkSmartPointer<vtkAstroImageGradientSmooth>::New();
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
  return this->RunSmoothingFilter(pnode, filter, "Intensity driven Gradient Filter (GPU, OpenGL)", false);
#endif
}

//----------------------------------------------------------------------------
double vtkSlicerAstroSmoothingLogic::ComputeSmoothedSpectralFWHM(vtkMRMLAstroVolumeNode* input,
                                                                 vtkMRMLAstroSmoothingParametersNode* pnode)
{
  if (!input || !pnode || !input->GetImageData() || input->GetImageData()->GetDimensions()[2] < 2)
    {
    return 0.;
    }
  double kernel = 0.;
  if (pnode->GetFilter() == 0)
    {
    // box: the width of the box (channels), the FWHM of a top-hat
    kernel = pnode->GetParameterZ();
    }
  else if (pnode->GetFilter() == 1)
    {
    // Gaussian: FWHM along the spectral axis of the rotated kernel (rows of
    // the rotation M as in ComputeSmoothedBeam: variance_zz = sum_k M_k2^2 s_k^2)
    const double degToRad = vtkMath::Pi() / 180.;
    const double rx = pnode->GetRx() * degToRad, ry = pnode->GetRy() * degToRad;
    const double column[3] = {std::sin(ry), -std::cos(ry) * std::sin(rx), std::cos(rx) * std::cos(ry)};
    const double fwhm[3] = {pnode->GetParameterX(), pnode->GetParameterY(), pnode->GetParameterZ()};
    double variance = 0.;
    for (int k = 0; k < 3; k++)
      {
      variance += column[k] * column[k] * fwhm[k] * fwhm[k];
      }
    kernel = std::sqrt(variance);
    }
  if (!(kernel > 0.001) || !(pnode->GetParameterZ() > 0.001))
    {
    return 0.;
    }
  const double inputFWHM = input->GetSpectralFWHM();
  return std::sqrt(inputFWHM * inputFWHM + kernel * kernel);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroSmoothingLogic::ComputeSmoothedBeam(vtkMRMLAstroVolumeNode* input,
                                                       vtkMRMLAstroSmoothingParametersNode* pnode,
                                                       double newBeam[3], double kernel[3])
{
  newBeam[0] = newBeam[1] = newBeam[2] = 0.;
  kernel[0] = kernel[1] = kernel[2] = 0.;
  if (!input || !pnode || pnode->GetFilter() != 1 || !input->GetImageData() || !input->GetAstroVolumeDisplayNode())
    {
    return false;
    }
  double beam[3];
  const char* keys[3] = {"SlicerAstro.BMAJ", "SlicerAstro.BMIN", "SlicerAstro.BPA"};
  for (int i = 0; i < 3; i++)
    {
    const char* value = input->GetAttribute(keys[i]);
    if (!value || !strcmp(value, "UNDEFINED"))
      {
      return false;
      }
    beam[i] = atof(value);
    }
  if (!(beam[0] > 0.) || !(beam[1] > 0.))
    {
    return false;
    }
  beam[0] *= 3600.;
  beam[1] *= 3600.;

  // covariance (pixels^2) of the kernel: exponent in the rotated coordinates
  // r' = M r (the rows of M as in the kernel of the parameters node), so the
  // covariance in pixel coordinates is M^T D M
  const double fwhmToSigma = 1. / (2. * std::sqrt(2. * std::log(2.)));
  const double degToRad = vtkMath::Pi() / 180.;
  int dims[3];
  input->GetImageData()->GetDimensions(dims);
  const double sigma[3] = {pnode->GetParameterX() * fwhmToSigma,
                           dims[1] > 1 ? pnode->GetParameterY() * fwhmToSigma : 0.,
                           dims[2] > 1 ? pnode->GetParameterZ() * fwhmToSigma : 0.};
  const bool planar = dims[2] == 1;
  const double rx = (planar ? 0. : pnode->GetRx()) * degToRad;
  const double ry = (planar ? 0. : pnode->GetRy()) * degToRad;
  const double rz = (dims[1] > 1 ? pnode->GetRz() : 0.) * degToRad;
  const double cx = std::cos(rx), sx = std::sin(rx), cy = std::cos(ry), sy = std::sin(ry);
  const double cz = std::cos(rz), sz = std::sin(rz);
  const double M[3][3] = {{cy * cz, -cy * sz, sy},
                          {cz * sx * sy + cx * sz, cx * cz - sx * sy * sz, -cy * sx},
                          {-cx * cz * sy + sx * sz, cz * sx + cx * sy * sz, cx * cy}};
  double covariancePixels[2][2] = {{0., 0.}, {0., 0.}};
  for (int a = 0; a < 2; a++)
    {
    for (int b = 0; b < 2; b++)
      {
      for (int k = 0; k < 3; k++)
        {
        covariancePixels[a][b] += M[k][a] * sigma[k] * sigma[k] * M[k][b];
        }
      }
    }

  // pixel steps on the sky (arc seconds, east and north) at the centre
  vtkMRMLAstroVolumeDisplayNode* displayNode = input->GetAstroVolumeDisplayNode();
  if (!displayNode->GetWCSStruct() || displayNode->GetSpace() != std::string("WCS"))
    {
    return false;
    }
  const double centre[3] = {0.5 * (dims[0] - 1), 0.5 * (dims[1] - 1), 0.};
  double world0[3], worldI[3], worldJ[3];
  double ijk[3] = {centre[0], centre[1], centre[2]};
  displayNode->GetReferenceSpace(ijk, world0);
  ijk[0] = centre[0] + 1.;
  displayNode->GetReferenceSpace(ijk, worldI);
  ijk[0] = centre[0];
  ijk[1] = centre[1] + 1.;
  displayNode->GetReferenceSpace(ijk, worldJ);
  const double cosDec = std::cos(world0[1] * degToRad);
  // offset to the east in arc seconds (RA unwrapped across 0h)
  const std::function<double(const double*)> east = [&world0, cosDec](const double* w)
    {
    double d = w[0] - world0[0];
    if (d > 180.)
      {
      d -= 360.;
      }
    if (d < -180.)
      {
      d += 360.;
      }
    return d * cosDec * 3600.;
    };
  const double J[2][2] = {{east(worldI), east(worldJ)},
                          {(worldI[1] - world0[1]) * 3600., (worldJ[1] - world0[1]) * 3600.}};
  double covarianceSky[2][2] = {{0., 0.}, {0., 0.}};
  for (int a = 0; a < 2; a++)
    {
    for (int b = 0; b < 2; b++)
      {
      for (int m = 0; m < 2; m++)
        {
        for (int n = 0; n < 2; n++)
          {
          covarianceSky[a][b] += J[a][m] * covariancePixels[m][n] * J[b][n];
          }
        }
      }
    }
  // principal axes: major, minor (FWHM) and the angle of the major axis
  // from north (y) through east (x)
  const double xx = covarianceSky[0][0], yy = covarianceSky[1][1], xy = covarianceSky[0][1];
  const double mean = 0.5 * (xx + yy);
  const double delta = std::sqrt(0.25 * (xx - yy) * (xx - yy) + xy * xy);
  const double major = std::sqrt(std::max(mean + delta, 0.)) / fwhmToSigma;
  const double minor = std::sqrt(std::max(mean - delta, 0.)) / fwhmToSigma;
  // direction of the major axis (vx east, vy north)
  double vx = xy, vy = mean + delta - xx;
  if (std::abs(vx) < 1e-12 && std::abs(vy) < 1e-12)
    {
    vx = xx >= yy ? 1. : 0.;
    vy = xx >= yy ? 0. : 1.;
    }
  double pa = std::atan2(vx, vy) / degToRad;
  while (pa > 90.)
    {
    pa -= 180.;
    }
  while (pa <= -90.)
    {
    pa += 180.;
    }
  kernel[0] = major;
  kernel[1] = minor;
  kernel[2] = pa;
  vtkAstroBeams::Convolve(beam, kernel, newBeam);
  return true;
}
