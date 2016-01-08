// SmoothingLogic includes
#include "vtkSlicerSmoothingLogic.h"
#include "vtkSlicerAstroVolumeLogic.h"

// MRML includes
#include <vtkMRMLSmoothingParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkImageClip.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkVersion.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

// STD includes
#include <cassert>
#include <iostream>

// OpenMP includes
#include <omp.h>

//----------------------------------------------------------------------------
class vtkSlicerSmoothingLogic::vtkInternal
{
public:
  vtkInternal();

  vtkSlicerAstroVolumeLogic* AstroVolumeLogic;
};

//----------------------------------------------------------------------------
vtkSlicerSmoothingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = 0;
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerSmoothingLogic);

//----------------------------------------------------------------------------
vtkSlicerSmoothingLogic::vtkSlicerSmoothingLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerSmoothingLogic::~vtkSlicerSmoothingLogic()
{
  delete this->Internal;
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkSlicerSmoothingLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerSmoothingLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerSmoothingLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerSmoothingLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerSmoothingLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLSmoothingParametersNode* pNode = vtkMRMLSmoothingParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::Apply(vtkMRMLSmoothingParametersNode* pnode)
{
  int success;
  switch (pnode->GetFilter())
    {
    case 0:
      {
      success = this->GaussianCPUFilter(pnode);
      break;
      }
    case 1:
      {
      success = this->GradientCPUFilter(pnode);
      break;
      }
    case 2:
      {
      success = this->WaveletLiftingCPUFilter(pnode);
      break;
      }
    }
  return success;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::GaussianCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  vtkNew<vtkImageData> tempVolumeData;
  tempVolumeData->DeepCopy(outputVolume->GetImageData());

  int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *outPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
  float *tempPixel = static_cast<float*> (tempVolumeData->GetScalarPointer(0,0,0));
  float *inPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
  double max = outputVolume->GetImageData()->GetScalarTypeMin();
  double min = outputVolume->GetImageData()->GetScalarTypeMax();
  int status = 0;
  bool cancel = false;

  omp_set_num_threads(omp_get_num_procs());

  #pragma omp parallel for shared(pnode, outPixel, inPixel, status)
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    status = pnode->GetStatus();

    if (status == -1 && omp_get_thread_num() == 0)
      {
      cancel = true;
      }

    if (!cancel)
      {
      if ((elemCnt + 1) > (int) (numElements * status / 33))
        {
        #pragma omp atomic
          status++;
        if (omp_get_thread_num() == 0)
          {
          pnode->SetStatus(status);
          }
        }

      int i1, i2;

      i1 = - (int) ((pnode->GetGaussianKernelX()->GetNumberOfTuples() - 1) / 2.);
      i2 = + (int) ((pnode->GetGaussianKernelX()->GetNumberOfTuples() - 1) / 2.);
      *(outPixel + elemCnt) = 0.;
      for (int i = i1; i <= i2; i++)
        {
        int ii = elemCnt + i;
        int ref = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        if(ii < ref)
          {
          continue;
          }
        if(ii > ref + dims[0])
          {
          break;
          }
        *(outPixel + elemCnt) += *(inPixel + ii) * *(pnode->GetGaussianKernelX()->GetPointer(i-i1));
        }
      }
    }

  if (cancel)
    {
    pnode->SetStatus(0);
    return 0;
    }

  #pragma omp parallel for shared(pnode, tempPixel, outPixel, status)
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int status = pnode->GetStatus();

    if (status == -1 && omp_get_thread_num() == 0)
      {
      cancel = true;
      }

    if (!cancel)
      {
      if ((elemCnt + numElements + 1) > (int) (numElements * status / 33))
        {
        #pragma omp atomic
          status++;
        if (omp_get_thread_num() == 0)
          {
          pnode->SetStatus(status);
          }
        }

      int i1, i2;

      i1 = - (int) ((pnode->GetGaussianKernelY()->GetNumberOfTuples() - 1) / 2.);
      i2 = + (int) ((pnode->GetGaussianKernelY()->GetNumberOfTuples() - 1) / 2.);
      *(tempPixel + elemCnt) = 0.;
      for (int i = i1; i <= i2; i++)
        {
        int ii = elemCnt + (i * dims[0]);
        int ref = (int) floor(elemCnt / (dims[0] * dims[1]));
        ref *= dims[0] * dims[1];
        if(ii < ref)
          {
          continue;
          }
        if(ii > ref + (dims[0] * dims[1]))
          {
          break;
          }
        *(tempPixel + elemCnt) += *(outPixel + ii) * *(pnode->GetGaussianKernelY()->GetPointer(i-i1));
        }
      }
    }

  if (cancel)
    {
    pnode->SetStatus(0);
    return 0;
    }

  #pragma omp parallel for shared(pnode, tempPixel, outPixel, status)
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int status = pnode->GetStatus();

    if (status == -1 && omp_get_thread_num() == 0)
      {
      cancel = true;
      }

    if (!cancel)
      {
      if ((elemCnt + (numElements * 2) + 1) > (int) (numElements * status / 33))
        {
        #pragma omp atomic
          status++;
        if (omp_get_thread_num() == 0)
          {
          pnode->SetStatus(status);
          }
        }

      int i1, i2;

      i1 = - (int) ((pnode->GetGaussianKernelZ()->GetNumberOfTuples() - 1) / 2.);
      i2 = + (int) ((pnode->GetGaussianKernelZ()->GetNumberOfTuples() - 1) / 2.);
      *(outPixel + elemCnt) = 0.;
      for (int i = i1; i <= i2; i++)
        {
        int ii = elemCnt + (i * dims[0] * dims[1]);
        if(ii < 0)
          {
          continue;
          }
        if(ii > dims[0] * dims[1] * dims[2])
         {
          break;
          }
        *(outPixel + elemCnt) += *(tempPixel + ii) * *(pnode->GetGaussianKernelZ()->GetPointer(i-i1));
        }

      if(*(outPixel+elemCnt) > max)
        {
        max = *(outPixel+elemCnt);
        }
      if(*(outPixel+elemCnt) < min)
        {
        min = *(outPixel+elemCnt);
        }
      }
    }

  if (cancel)
    {
    pnode->SetStatus(0);
    return 0;
    }

  double sum = 0., noise = 0.;
  int cont = 0;
  const int lowBoundary = dims[0] * dims[1] * 2;
  const int highBoundary = dims[0] * dims[1] * 4;
  for( int elemCnt = lowBoundary; elemCnt < highBoundary; elemCnt++)
    {
    if(*(outPixel + elemCnt) < 0.)
      {
      sum += *(outPixel + elemCnt);
      cont++;
      }
    }
  sum /= cont;
  for( int elemCnt = lowBoundary; elemCnt < highBoundary; elemCnt++)
    {
    if(*(outPixel + elemCnt) < 0.)
      {
      noise += (*(outPixel + elemCnt) - sum) * (*(outPixel+elemCnt) - sum);
      }
    }
  noise = sqrt(noise / cont);

  outputVolume->GetImageData()->Modified();
  outputVolume->GetImageData()->GetPointData()->GetScalars()->Modified();

  outputVolume->SetAttribute("SlicerAstro.DATAMAX", DoubleToString(max).c_str());
  outputVolume->SetAttribute("SlicerAstro.DATAMIN", DoubleToString(min).c_str());
  outputVolume->SetAttribute("SlicerAstro.NOISE", DoubleToString(noise).c_str());

  pnode->SetStatus(0);

  return 1;
}


//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::GradientCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::WaveletLiftingCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  return 1;
}
