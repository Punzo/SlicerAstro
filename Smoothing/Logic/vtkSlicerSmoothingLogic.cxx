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

#include <iostream>
#include <sys/time.h>
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
  pnode->SetStatus(1);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  vtkSmartPointer<vtkImageData> tempVolumeData = vtkImageData::New();
  tempVolumeData->DeepCopy(outputVolume->GetImageData());

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  const int ix = (int) (pnode->GetGaussianKernelX()->GetNumberOfTuples());
  const int ix2 = (int) - ((ix - 1)/ 2);
  const int iy = (int) (pnode->GetGaussianKernelY()->GetNumberOfTuples());
  const int iy2 = (int) - ((iy - 1)/ 2);
  const int iz = (int) (pnode->GetGaussianKernelZ()->GetNumberOfTuples());
  const int iz2 = (int) - ((iz - 1)/ 2);
  float *outFPixel = NULL;
  float *tempFPixel = NULL;
  double *outDPixel = NULL;
  double *tempDPixel = NULL;
  int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempFPixel = static_cast<float*> (tempVolumeData->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempDPixel = static_cast<double*> (tempVolumeData->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  double *GaussKernelX = static_cast<double*> (pnode->GetGaussianKernelX()->GetVoidPointer(0));
  double *GaussKernelY = static_cast<double*> (pnode->GetGaussianKernelY()->GetVoidPointer(0));
  double *GaussKernelZ = static_cast<double*> (pnode->GetGaussianKernelZ()->GetVoidPointer(0));
  bool cancel = false;

  omp_set_num_threads(omp_get_num_procs());

  /*struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);*/

  if (pnode->GetParameterX() > 0.001)
    {
    pnode->SetStatus(10);
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      if (status == -1 && omp_get_thread_num() == 0)
        {
        cancel = true;
        }

      #pragma omp flush (cancel)
      if (!cancel)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) = 0.;
            break;
          }

        for (int i = 0; i < ix; i++)
          {
          int ii = elemCnt + i + ix2;
          int ref = (int) floor(elemCnt / dims[0]);
          ref *= dims[0];
          if(ii < ref)
            {
            continue;
            }
          if(ii >= ref + dims[0])
            {
            break;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) += *(tempFPixel + ii) * *(GaussKernelX + i);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii) * *(GaussKernelX + i);
              break;
            }
          }
        }
      }
    }

  if (cancel)
    {
    pnode->SetStatus(0);
    return 0;
    }

  if (pnode->GetParameterY() > 0.001)
    {
    pnode->SetStatus(40);
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      if (status == -1 && omp_get_thread_num() == 0)
        {
        cancel = true;
        }

      #pragma omp flush (cancel)
      if (!cancel)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(tempFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
            *(tempDPixel + elemCnt) = 0.;
            break;
          }
        for (int i = 0; i < iy; i++)
          {
          int ii = elemCnt + ((i + iy2) * dims[0]);
          int ref = (int) floor(elemCnt / numSlice);
          ref *= numSlice;
          if(ii < ref)
            {
            continue;
            }
          if(ii >= ref + numSlice)
            {
            break;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              *(tempFPixel + elemCnt) += *(outFPixel + ii) * *(GaussKernelY + i);
              break;
            case VTK_DOUBLE:
              *(tempDPixel + elemCnt) += *(outDPixel + ii) * *(GaussKernelY + i);
              break;
            }
          }
        }
      }
    }
  else
    {
    tempVolumeData->DeepCopy(outputVolume->GetImageData());
    }

  if (cancel)
    {
    pnode->SetStatus(0);
    return 0;
    }

  if (pnode->GetParameterZ() > 0.001)
    {
    pnode->SetStatus(70);
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      if (status == -1 && omp_get_thread_num() == 0)
        {
        cancel = true;
        }

      #pragma omp flush (cancel)
      if (!cancel)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) = 0.;
            break;
          }
        for (int i = 0; i < iz; i++)
          {
          int ii = elemCnt + ((i + iz2) * numSlice);
          if(ii < 0)
            {
            continue;
            }
          if(ii >= numElements)
            {
            break;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) += *(tempFPixel + ii) * *(GaussKernelZ + i);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii) * *(GaussKernelZ + i);
              break;
            }
          }
        }
      }
    }
  else
    {
    outputVolume->GetImageData()->DeepCopy(tempVolumeData);
    }

  /*gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;*/

  pnode->SetStatus(0);
  if (cancel)
    {
    return 0;
    }
  else
    {
    outputVolume->UpdateRangeAttributes();
    outputVolume->UpdateNoiseAttribute();
    return 1;
    }
}


//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::GradientCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  vtkSmartPointer<vtkImageData> tempVolumeData = vtkImageData::New();
  tempVolumeData->DeepCopy(outputVolume->GetImageData());

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  const double noise = StringToDouble(outputVolume->GetAttribute("SlicerAstro.NOISE"));
  const double noise2 = noise * noise * 0.5 * 0.5;
  float *outFPixel = NULL;
  float *tempFPixel = NULL;
  double *outDPixel = NULL;
  double *tempDPixel = NULL;
  int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempFPixel = static_cast<float*> (tempVolumeData->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempDPixel = static_cast<double*> (tempVolumeData->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }
  bool cancel = false;

  omp_set_num_threads(omp_get_num_procs());

  pnode->SetStatus(1);
  for (int i = 1; i <= pnode->GetAccuracy(); i++)
    {
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, tempFPixel, outDPixel, tempDPixel, cancel)
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      if (status == -1 && omp_get_thread_num() == 0)
        {
        cancel = true;
        }

      #pragma omp flush (cancel)
      if (!cancel)
        {
        int x1 = elemCnt - 1;
        int ref = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        if(x1 < ref)
          {
          x1++;
          }
        int x2 = elemCnt + 1;
        if(x1 >= ref + dims[0])
          {
          x1--;
          }

        int y1 = elemCnt - dims[0];
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        if(y1 < ref)
          {
          y1 += dims[0];
          }
        int y2 = elemCnt + dims[0];
        if(y2 >= ref + numSlice)
          {
          y2 -= dims[0];
          }

        int z1 = elemCnt - numSlice;
        if(z1 < 0)
          {
          z1 += numSlice;
          }
        int z2 = elemCnt + numSlice;
        if(z2 > numElements)
          {
          z2 -= numSlice;
          }

        double FPixel2, norm;
        double cX, cY, cZ;

        switch (DataType)
          {
          case VTK_FLOAT:
            //add in the interface and MRML timestep (0.03 line 461)
            //(should go from 0.0025 to 0.0625 step 0.003, defualt 0.0325)
            //and K (0.5 in line 391)
            //(K should go from 0.5 to 2, step 0.5, default 1)
            FPixel2 = *(tempFPixel + elemCnt) * *(tempFPixel + elemCnt);
            norm = 1 + (FPixel2 / noise2);
            cX = ((*(outFPixel + x1) - *(tempFPixel + elemCnt)) +
                  (*(outFPixel + x2) - *(tempFPixel + elemCnt))) * pnode->GetParameterX();
            cY = ((*(outFPixel + y1) - *(tempFPixel + elemCnt)) +
                  (*(outFPixel + y2) - *(tempFPixel + elemCnt))) * pnode->GetParameterY();
            cZ = ((*(outFPixel + z1) - *(tempFPixel + elemCnt)) +
                  (*(outFPixel + z2) - *(tempFPixel + elemCnt))) * pnode->GetParameterZ();

            *(tempFPixel + elemCnt) += 0.03 * (cX + cY + cZ) / norm;
            break;
          case VTK_DOUBLE:
            break;
          }

        }
      }

    if (cancel)
      {
      pnode->SetStatus(0);
      return 0;
      }
    else
      {
      pnode->SetStatus((int) i * 100 / pnode->GetAccuracy());
      }

    outputVolume->GetImageData()->DeepCopy(tempVolumeData);
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttribute();
  pnode->SetStatus(0);

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::WaveletLiftingCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  //implement it
  return 1;
}

//add Wavelet Thresholding
