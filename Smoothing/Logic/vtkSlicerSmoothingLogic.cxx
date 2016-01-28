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
  vtkSmartPointer<vtkImageData> tempVolumeData;
};

//----------------------------------------------------------------------------
vtkSlicerSmoothingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = 0;
  tempVolumeData = vtkImageData::New();
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
        if (fabs(pnode->GetParameterX() - pnode->GetParameterY()) < 0.001 &&
            fabs(pnode->GetParameterY() - pnode->GetParameterZ()) < 0.001)
          {
          success = this->IsotropicGaussianCPUFilter(pnode);
          }
        else
          {
          success = this->AnisotropicGaussianCPUFilter(pnode);
          }
      break;
      }
    case 1:
      {
      success = this->GradientCPUFilter(pnode);
      break;
      }
    case 2:
      {
      success = this->HaarWaveletThresholdingCPUFilter(pnode);
      break;
      }
    case 3:
      {
      success = this->GallWaveletThresholdingCPUFilter(pnode);
      break;
      }
    }
  return success;
}


//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::AnisotropicGaussianCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  int Xmax = (int) (pnode->GetKernelLengthX() - 1) / 2.;
  int Ymax = (int) (pnode->GetKernelLengthY() - 1) / 2.;
  int Zmax = (int) (pnode->GetKernelLengthZ() - 1) / 2.;
  const int numKernelSlice = pnode->GetKernelLengthX() * pnode->GetKernelLengthY();
  float *outFPixel = NULL;
  float *tempFPixel = NULL;
  double *outDPixel = NULL;
  double *tempDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  double *GaussKernel = static_cast<double*> (pnode->GetGaussianKernel3D()->GetVoidPointer(0));

  bool cancel = false;
  int status = 0;
  int numProcs = omp_get_num_procs();
  omp_set_num_threads(numProcs);

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);
  #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel, status)
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int stat = pnode->GetStatus();

    if (stat == -1 && omp_get_thread_num() == 0)
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

      for (int k = -Zmax; k <= Zmax; k++)
        {
        for (int j = -Ymax; j <= Ymax; j++)
          {
          for (int i = -Xmax; i <= Xmax; i++)
            {
            int posData = elemCnt + i;
            int ref = (int) floor(elemCnt / dims[0]);
            ref *= dims[0];
            if(posData < ref)
              {
              continue;
              }
            if(posData >= ref + dims[0])
              {
              break;
              }

            posData += j * dims[0];
            ref = (int) floor(elemCnt / numSlice);
            ref *= numSlice;
            if(posData < ref)
              {
              continue;
              }
            if(posData >= ref + numSlice)
              {
              break;
              }

            posData += k * numSlice;
            if(posData < 0)
              {
              continue;
              }
            if(posData >= numElements)
              {
              break;
              }

            int posKernel = (k + Zmax) * numKernelSlice
                          + (j + Ymax) * pnode->GetKernelLengthX() + (i + Xmax);

            switch (DataType)
              {
              case VTK_FLOAT:
                *(outFPixel + elemCnt) += *(tempFPixel + posData) * *(GaussKernel + posKernel);
                break;
              case VTK_DOUBLE:
                *(outDPixel + elemCnt) += *(tempDPixel + posData) * *(GaussKernel + posKernel);
                break;
              }
            }
          }
        }
      if (omp_get_thread_num() == 0)
        {
        if(elemCnt / (numElements / (numProcs * 100)) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        }
      }
    }

  outFPixel = NULL;
  tempFPixel = NULL;
  outDPixel = NULL;
  tempDPixel = NULL;

  delete outFPixel;
  delete tempFPixel;
  delete outDPixel;
  delete tempDPixel;

  if (cancel)
    {
    this->Internal->tempVolumeData->Initialize();
    pnode->SetStatus(0);
    return 0;
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttribute();
  this->Internal->tempVolumeData->Initialize();
  pnode->SetStatus(0);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;

  return 1;

}


//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::IsotropicGaussianCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  const int ix = (int) (pnode->GetKernelLengthX());
  const int ix2 = (int) - ((ix - 1)/ 2);
  const int iy = (int) (pnode->GetKernelLengthY());
  const int iy2 = (int) - ((iy - 1)/ 2);
  const int iz = (int) (pnode->GetKernelLengthZ());
  const int iz2 = (int) - ((iz - 1)/ 2);
  float *outFPixel = NULL;
  float *tempFPixel = NULL;
  double *outDPixel = NULL;
  double *tempDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
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

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

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
    outFPixel = NULL;
    tempFPixel = NULL;
    outDPixel = NULL;
    tempDPixel = NULL;

    delete outFPixel;
    delete tempFPixel;
    delete outDPixel;
    delete tempDPixel;
    this->Internal->tempVolumeData->Initialize();
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
    this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
    this->Internal->tempVolumeData->Modified();
    this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

    switch (DataType)
      {
      case VTK_FLOAT:
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      default:
        vtkErrorMacro("Attempt to allocate scalars of type not allowed");
        return 0;
      }
    }

  if (cancel)
    {  
    outFPixel = NULL;
    tempFPixel = NULL;
    outDPixel = NULL;
    tempDPixel = NULL;

    delete outFPixel;
    delete tempFPixel;
    delete outDPixel;
    delete tempDPixel;
    this->Internal->tempVolumeData->Initialize();
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
    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);
    }

  outFPixel = NULL;
  tempFPixel = NULL;
  outDPixel = NULL;
  tempDPixel = NULL;

  delete outFPixel;
  delete tempFPixel;
  delete outDPixel;
  delete tempDPixel;

  if (cancel)
    {
    this->Internal->tempVolumeData->Initialize();
    pnode->SetStatus(0);
    return 0;
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttribute();
  this->Internal->tempVolumeData->Initialize();
  pnode->SetStatus(0);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;

  return 1;

}


//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::GradientCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  const double noise = StringToDouble(outputVolume->GetAttribute("SlicerAstro.NOISE"));
  const double noise2 = noise * noise * pnode->GetK() * pnode->GetK();
  float *outFPixel = NULL;
  float *tempFPixel = NULL;
  double *outDPixel = NULL;
  double *tempDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }
  bool cancel = false;

  omp_set_num_threads(omp_get_num_procs());

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

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
        if(x2 >= ref + dims[0])
          {
          x2--;
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
        if(z2 >= numElements)
          {
          z2 -= numSlice;
          }

        double Pixel2, norm;
        double cX, cY, cZ;

        switch (DataType)
          {
          case VTK_FLOAT:
            Pixel2 = *(outFPixel + elemCnt) * *(outFPixel + elemCnt);
            norm = 1 + (Pixel2 / noise2);
            cX = ((*(outFPixel + x1) - *(outFPixel + elemCnt)) +
                  (*(outFPixel + x2) - *(outFPixel + elemCnt))) * pnode->GetParameterX();
            cY = ((*(outFPixel + y1) - *(outFPixel + elemCnt)) +
                  (*(outFPixel + y2) - *(outFPixel + elemCnt))) * pnode->GetParameterY();
            cZ = ((*(outFPixel + z1) - *(outFPixel + elemCnt)) +
                  (*(outFPixel + z2) - *(outFPixel + elemCnt))) * pnode->GetParameterZ();

            *(tempFPixel + elemCnt) = *(outFPixel + elemCnt) +
                                      pnode->GetTimeStep() * (cX + cY + cZ) / norm;
            break;
          case VTK_DOUBLE:
            Pixel2 = *(outDPixel + elemCnt) * *(outDPixel + elemCnt);
            norm = 1 + (Pixel2 / noise2);
            cX = ((*(outDPixel + x1) - *(outDPixel + elemCnt)) +
                  (*(outDPixel + x2) - *(outDPixel + elemCnt))) * pnode->GetParameterX();
            cY = ((*(outDPixel + y1) - *(outDPixel + elemCnt)) +
                  (*(outDPixel + y2) - *(outDPixel + elemCnt))) * pnode->GetParameterY();
            cZ = ((*(outDPixel + z1) - *(outDPixel + elemCnt)) +
                  (*(outDPixel + z2) - *(outDPixel + elemCnt))) * pnode->GetParameterZ();

            *(tempDPixel + elemCnt) = *(outDPixel + elemCnt) +
                                      pnode->GetTimeStep() * (cX + cY + cZ) / norm;
            break;
          }
        }
      }

    if (cancel)
      {  
      outFPixel = NULL;
      tempFPixel = NULL;
      outDPixel = NULL;
      tempDPixel = NULL;

      delete outFPixel;
      delete tempFPixel;
      delete outDPixel;
      delete tempDPixel;
      this->Internal->tempVolumeData->Initialize();
      pnode->SetStatus(0);
      return 0;
      }

    pnode->SetStatus((int) i * 100 / pnode->GetAccuracy());

    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);
    outputVolume->UpdateRangeAttributes();
    outputVolume->UpdateNoiseAttribute();

    switch (DataType)
      {
      case VTK_FLOAT:
        outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      default:
        vtkErrorMacro("Attempt to allocate scalars of type not allowed");
        return 0;
      }
    }

  outFPixel = NULL;
  tempFPixel = NULL;
  outDPixel = NULL;
  tempDPixel = NULL;

  delete outFPixel;
  delete tempFPixel;
  delete outDPixel;
  delete tempDPixel;

  pnode->SetStatus(0);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::HaarWaveletThresholdingCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  // This method use Wavelet Lifting (Haar Wavelet):
  // 1) It applys a forward trasform to level l;
  // 2) It thresholdds the coefficients;
  // 3) It applys an inverse trasform restoring the full resolution image;

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  int numSlice = dims[0] * dims[1];
  float *outFPixel = NULL;
  double *outDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  double sigma = StringToDouble(outputVolume->GetAttribute("SlicerAstro.NOISE"));
  double delta = pnode->GetParameterX() * sigma;

  bool reduceX = false;
  bool reduceY = false;
  bool reduceZ = false;

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  int oldDims [3] = {dims[0], dims[1], dims[2]};
  int oldNumElements = numElements;

  int maxLevel = pow(2, pnode->GetAccuracy());

  while ((dims[0] % maxLevel) > 0.)
    {
    dims[0]++;
    reduceX = true;
    }

  while ((dims[1] % maxLevel) > 0.)
    {
    dims[1]++;
    reduceY = true;
    }

  while ((dims[2] % maxLevel) > 0.)
    {
    dims[2]++;
    reduceZ = true;
    }

  if (reduceX || reduceY || reduceZ)
    {
    numElements = dims[0] * dims[1] * dims[2] * numComponents;
    numSlice = dims[0] * dims[1];
    int tempNumElements = dims[0] * dims[1] * oldDims[2];
    int tempNumSlice = dims[0] * oldDims[1];

    this->Internal->tempVolumeData->Initialize();
    this->Internal->tempVolumeData->SetDimensions(dims);
    float *tempFPixel = NULL;
    double *tempDPixel = NULL;
    switch (DataType)
      {
      case VTK_FLOAT:
        this->Internal->tempVolumeData->AllocateScalars(VTK_FLOAT,1);
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        this->Internal->tempVolumeData->AllocateScalars(VTK_DOUBLE,1);
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      }

    int ii = 0;

    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {

      if(elemCnt >= tempNumElements)
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
        continue;
        }

      int ref =  (int) floor(elemCnt / numSlice);
      ref *= numSlice;
      ref = elemCnt - ref;
      if (ref >= tempNumSlice)
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
        continue;
        }

      ref = (int) floor(elemCnt / dims[0]);
      ref *= dims[0];
      ref = elemCnt - ref;
      if (ref >= oldDims[0])
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
        continue;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          *(tempFPixel + elemCnt) = *(outFPixel + ii);
          break;
        case VTK_DOUBLE:
          *(tempDPixel + elemCnt) = *(outDPixel + ii);
          break;
        }

      ii++;

      }
    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);
    dims = outputVolume->GetImageData()->GetDimensions();

    switch (DataType)
      {
      case VTK_FLOAT:
        outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      default:
        vtkErrorMacro("Attempt to allocate scalars of type not allowed");
        return 0;
      }

    tempFPixel = NULL;
    tempDPixel = NULL;

    delete tempFPixel;
    delete tempDPixel;

    this->Internal->tempVolumeData->Initialize();
    }

  numElements = dims[0] * dims[1] * oldDims[2] * numComponents;

  // Forward Transform to level l:
  for (int l = 1; l <= pnode->GetAccuracy(); l++)
    {
    if (pnode->GetStatus() == -1)
      {
      break;
      }
    pnode->SetStatus((int) l * 100 / (pnode->GetAccuracy() * 2));

    int inc = pow(2, l);
    int inc2 = (int) inc / 2;;
    int inc3 = (inc * dims[0]) - dims[0];
    int inc4 = inc2 * dims[0];
    int inc5 = (inc * numSlice) - numSlice;
    int inc6 = inc2 * numSlice;

    // Along X
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {

      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int ii = elemCnt + inc2;

      if(ii > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Predict step
          *(outFPixel + ii) -= (*(outFPixel + elemCnt));
          // Update step
          *(outFPixel + elemCnt) += 0.5 * *(outFPixel + ii);
          break;
        case VTK_DOUBLE:
          // Predict step
          *(outDPixel + ii) -= (*(outDPixel + elemCnt));
          // Update step
          *(outDPixel + elemCnt) += 0.5 * *(outDPixel + ii);
          break;
        }
      }

    // Along Y
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {
      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int row = (int) floor(elemCnt / dims[0]);

      if ((row % inc) > 0.)
        {
        elemCnt = elemCnt + inc3;
        }
      int ii = elemCnt + inc4;
      int res1 = elemCnt + inc2;
      int res2 = ii + inc2;

      if(res2 > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Predict step
          *(outFPixel + ii) -= (*(outFPixel + elemCnt));
          // Update step
          *(outFPixel + elemCnt) += 0.5 * *(outFPixel + ii);
          // Predict step for residuals
          *(outFPixel + res2) -= (*(outFPixel + res1));
          // Update step for residuals
          *(outFPixel + res1) += 0.5 * *(outFPixel + res2);
          break;
        case VTK_DOUBLE:
          // Predict step
          *(outDPixel + ii) -= (*(outDPixel + elemCnt));
          // Update step
          *(outDPixel + elemCnt) += 0.5 * *(outDPixel + ii);
          // Predict step for residuals
          *(outDPixel + res2) -= (*(outDPixel + res1));
          // Update step for residuals
          *(outDPixel + res1) += 0.5 * *(outDPixel + res2);
          break;
        }
      }

    // Along Z
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {

      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int row = (int) floor(elemCnt / dims[0]);
      if ((row % inc) > 0.)
        {
        elemCnt = elemCnt + inc3;
        }
      int slice = (int) floor(elemCnt / numSlice);
      if ((slice % inc) > 0.)
        {
        elemCnt = elemCnt + inc5;
        }
      int ii = elemCnt + inc6;
      int a1 = elemCnt + inc2;
      int b1 = ii + inc2;
      int a2 = elemCnt + dims[0];
      int b2 = ii + dims[0];
      int a3 = a2 + inc2;
      int b3 = b2 + inc2;

      if(b3 > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Predict step
          *(outFPixel + ii) -= (*(outFPixel + elemCnt));
          // Update step
          *(outFPixel + elemCnt) += 0.5 * *(outFPixel + ii);
          // Predict step
          *(outFPixel + b1) -= (*(outFPixel + a1));
          // Update step
          *(outFPixel + a1) += 0.5 * *(outFPixel + b1);
          // Predict step
          *(outFPixel + b2) -= (*(outFPixel + a2));
          // Update step
          *(outFPixel + a2) += 0.5 * *(outFPixel + b2);
          // Predict step
          *(outFPixel + b3) -= (*(outFPixel + a3));
          // Update step
          *(outFPixel + a3) += 0.5 * *(outFPixel + b3);
          break;
        case VTK_DOUBLE:
          // Predict step
          *(outDPixel + ii) -= (*(outDPixel + elemCnt));
          // Update step
          *(outDPixel + elemCnt) += 0.5 * *(outDPixel + ii);
          // Predict step
          *(outDPixel + b1) -= (*(outDPixel + a1));
          // Update step
          *(outDPixel + a1) += 0.5 * *(outDPixel + b1);
          // Predict step
          *(outDPixel + b2) -= (*(outDPixel + a2));
          // Update step
          *(outDPixel + a2) += 0.5 * *(outDPixel + b2);
          // Predict step
          *(outDPixel + b3) -= (*(outDPixel + a3));
          // Update step
          *(outDPixel + a3) += 0.5 * *(outDPixel + b3);
          break;
        }
      }
    }

  // Inverse Trasform
  int m = 0;
  for (int l = pnode->GetAccuracy(); l >= 1; l--)
    {
    if (pnode->GetStatus() == -1)
      {
      break;
      }
    m++;
    pnode->SetStatus((int) m * 100 / pnode->GetAccuracy());

    int inc = pow(2, l);
    int inc2 = (int) inc / 2;
    int inc3 = (inc * dims[0]) - dims[0];
    int inc4 = inc2 * dims[0];
    int inc5 = (inc * numSlice) - numSlice;
    int inc6 = inc2 * numSlice;

    // Along Z
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {

      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int row = (int) floor(elemCnt / dims[0]);
      if ((row % inc) > 0.)
        {
        elemCnt = elemCnt + inc3;
        }
      int slice = (int) floor(elemCnt / numSlice);
      if ((slice % inc) > 0.)
        {
        elemCnt = elemCnt + inc5;
        }
      int ii = elemCnt + inc6;
      int a1 = elemCnt + inc2;
      int b1 = ii + inc2;
      int a2 = elemCnt + dims[0];
      int b2 = ii + dims[0];
      int a3 = a2 + inc2;
      int b3 = b2 + inc2;

      if(b3 > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Thresholding
          if (fabs(*(outFPixel + ii)) < delta)
            {
            *(outFPixel + ii) = 0.;
            }
          if (fabs(*(outFPixel + b1)) < delta)
            {
            *(outFPixel + b1) = 0.;
            }
          if (fabs(*(outFPixel + b2)) < delta)
            {
            *(outFPixel + b2) = 0.;
            }
          if (fabs(*(outFPixel + b3)) < delta)
            {
            *(outFPixel + b3) = 0.;
            }
          // Update step
          *(outFPixel + elemCnt) -= 0.5 * *(outFPixel + ii);
          // Predict step
          *(outFPixel + ii) += (*(outFPixel + elemCnt));
          // Update step
          *(outFPixel + a1) -= 0.5 * *(outFPixel + b1);
          // Predict step
          *(outFPixel + b1) += (*(outFPixel + a1));
          // Update step
          *(outFPixel + a2) -= 0.5 * *(outFPixel + b2);
          // Predict step
          *(outFPixel + b2) += (*(outFPixel + a2));
          // Update step
          *(outFPixel + a3) -= 0.5 * *(outFPixel + b3);
          // Predict step
          *(outFPixel + b3) += (*(outFPixel + a3));
          break;
        case VTK_DOUBLE:
          // Thresholding
          if (fabs(*(outDPixel + ii)) < delta)
            {
            *(outDPixel + ii) = 0.;
            }
          if (fabs(*(outDPixel + b1)) < delta)
            {
            *(outDPixel + b1) = 0.;
            }
          if (fabs(*(outDPixel + b2)) < delta)
            {
            *(outDPixel + b2) = 0.;
            }
          if (fabs(*(outDPixel + b3)) < delta)
            {
            *(outDPixel + 3) = 0.;
            }
          // Update step
          *(outDPixel + elemCnt) -= 0.5 * *(outDPixel + ii);
          // Predict step
          *(outDPixel + ii) += (*(outDPixel + elemCnt));
          // Update step
          *(outDPixel + a1) -= 0.5 * *(outDPixel + b1);
          // Predict step
          *(outDPixel + b1) += (*(outDPixel + a1));
          // Update step
          *(outDPixel + a2) -= 0.5 * *(outDPixel + b2);
          // Predict step
          *(outDPixel + b2) += (*(outDPixel + a2));
          // Update step
          *(outDPixel + a3) -= 0.5 * *(outDPixel + b3);
          // Predict step
          *(outDPixel + b3) += (*(outDPixel + a3));
          break;
        }
      }

    // Along Y
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {

      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int row = (int) floor(elemCnt / dims[0]);

      if ((row % inc) > 0.)
        {
        elemCnt = elemCnt + inc3;
        }
      int ii = elemCnt + inc4;
      int res1 = elemCnt + inc2;
      int res2 = ii + inc2;

      if(res2 > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Thresholding
          if (fabs(*(outFPixel + ii)) < delta)
            {
            *(outFPixel + ii) = 0.;
            }
          if (fabs(*(outFPixel + res2)) < delta)
            {
            *(outFPixel + res2) = 0.;
            }
          // Update step
          *(outFPixel + elemCnt) -= 0.5 * *(outFPixel + ii);
          // Predict step
          *(outFPixel + ii) += (*(outFPixel + elemCnt));
          // Update step for residuals
          *(outFPixel + res1) -= 0.5 * *(outFPixel + res2);
          // Predict step for residuals
          *(outFPixel + res2) += (*(outFPixel + res1));
          break;
        case VTK_DOUBLE:
          // Thresholding
          if (fabs(*(outDPixel + ii)) < delta)
            {
            *(outDPixel + ii) = 0.;
            }
          if (fabs(*(outDPixel + res2)) < delta)
            {
            *(outDPixel + res2) = 0.;
            }
          // Update step
          *(outDPixel + elemCnt) -= 0.5 * *(outDPixel + ii);
          // Predict step
          *(outDPixel + ii) += (*(outDPixel + elemCnt));
          // Update step for residuals
          *(outDPixel + res1) -= 0.5 * *(outDPixel + res2);
          // Predict step for residuals
          *(outDPixel + res2) += (*(outDPixel + res1));
          break;
        }
      }

    // Along X
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {
      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int ii = elemCnt + inc2;

      if(ii > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Thresholding
          if (fabs(*(outFPixel + ii)) < delta)
            {
            *(outFPixel + ii) = 0.;
            }
          // Update step
          *(outFPixel + elemCnt) -= 0.5 * *(outFPixel + ii);
          // Predict step
          *(outFPixel + ii) += (*(outFPixel + elemCnt));
          break;
        case VTK_DOUBLE:
          // Thresholding
          if (fabs(*(outDPixel + ii)) < delta)
            {
            *(outDPixel + ii) = 0.;
            }
          // Update step
          *(outDPixel + elemCnt) -= 0.5 * *(outDPixel + ii);
          // Predict step
          *(outDPixel + ii) += (*(outDPixel + elemCnt));
          break;
        }
      }
    }

  if (reduceX || reduceY || reduceZ)
    {
    int ii = 0;

    int diffDims [3] = {dims[0] - oldDims[0],
                        dims[1] - oldDims[1],
                        dims[2] - oldDims[2]};
    int tempNumRow = oldDims[0] - 1;
    int tempNumSlice = (dims[0] * oldDims[1]);
    for (int elemCnt = 0; elemCnt < oldNumElements; elemCnt++)
      {

      switch (DataType)
        {
        case VTK_FLOAT:
          *(outFPixel + elemCnt) = *(outFPixel + elemCnt + ii);
          break;
        case VTK_DOUBLE:
          *(outDPixel + elemCnt) = *(outDPixel + elemCnt + ii);
          break;
        }

      int ref =  (int) floor((elemCnt + ii) / numSlice);
      ref *= numSlice;
      ref = elemCnt + ii - ref;
      if (ref == tempNumSlice)
        {
        ii += (diffDims[1] * dims[0]);
        }

      ref = (int) floor((elemCnt + ii) / dims[0]);
      ref *= dims[0];
      ref = elemCnt + ii - ref;
      if (ref == tempNumRow)
        {
        ii += diffDims[0];
        }

      }
    outputVolume->GetImageData()->SetDimensions(oldDims);
    }

  outFPixel = NULL;
  outDPixel = NULL;

  delete outFPixel;
  delete outDPixel;

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttribute();
  pnode->SetStatus(0);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::GallWaveletThresholdingCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  // This method use Wavelet Lifting (Le Gall (5,3) Wavelet):
  // 1) It applys a forward trasform to level l;
  // 2) It thresholdds the coefficients;
  // 3) It applys an inverse trasform restoring the full resolution image;

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  int numSlice = dims[0] * dims[1];
  float *outFPixel = NULL;
  double *outDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  double sigma = StringToDouble(outputVolume->GetAttribute("SlicerAstro.NOISE"));
  double delta = pnode->GetParameterX() * sigma;

  bool reduceX = false;
  bool reduceY = false;
  bool reduceZ = false;

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  int oldDims [3] = {dims[0], dims[1], dims[2]};
  int oldNumElements = numElements;

  int maxLevel = pow(2, pnode->GetAccuracy());

  while ((dims[0] % maxLevel) > 0.)
    {
    dims[0]++;
    reduceX = true;
    }

  while ((dims[1] % maxLevel) > 0.)
    {
    dims[1]++;
    reduceY = true;
    }

  while ((dims[2] % maxLevel) > 0.)
    {
    dims[2]++;
    reduceZ = true;
    }

  numElements = dims[0] * dims[1] * dims[2] * numComponents;
  numSlice = dims[0] * dims[1];
  int tempNumElements = dims[0] * dims[1] * oldDims[2];
  int tempNumSlice = dims[0] * oldDims[1];

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->SetDimensions(dims);
  float *tempFPixel = NULL;
  double *tempDPixel = NULL;
  switch (DataType)
    {
    case VTK_FLOAT:
      this->Internal->tempVolumeData->AllocateScalars(VTK_FLOAT,1);
      tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      this->Internal->tempVolumeData->AllocateScalars(VTK_DOUBLE,1);
      tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    }

  int ii = 0;
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {

    if(elemCnt >= tempNumElements)
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
      continue;
      }

    int ref =  (int) floor(elemCnt / numSlice);
    ref *= numSlice;
    ref = elemCnt - ref;
    if (ref >= tempNumSlice)
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
      continue;
      }
    ref = (int) floor(elemCnt / dims[0]);
    ref *= dims[0];
    ref = elemCnt - ref;
    if (ref >= oldDims[0])
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
      continue;
      }

    switch (DataType)
      {
      case VTK_FLOAT:
        *(tempFPixel + elemCnt) = *(outFPixel + ii);
        break;
      case VTK_DOUBLE:
        *(tempDPixel + elemCnt) = *(outDPixel + ii);
        break;
      }

    ii++;

    }
    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);
    dims = outputVolume->GetImageData()->GetDimensions();

    switch (DataType)
      {
      case VTK_FLOAT:
        outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      default:
        vtkErrorMacro("Attempt to allocate scalars of type not allowed");
        return 0;
      }

  numElements = dims[0] * dims[1] * oldDims[2] * numComponents;

  // Forward Transform to level l:
  for (int l = 1; l <= pnode->GetAccuracy(); l++)
    {
    if (pnode->GetStatus() == -1)
      {
      break;
      }
    pnode->SetStatus((int) l * 100 / (pnode->GetAccuracy() * 2));

    int inc = pow(2, l);
    int inc2 = (int) inc / 2;;
    int inc3 = (inc * dims[0]) - dims[0];
    int inc4 = inc2 * dims[0];
    int inc5 = (inc * numSlice) - numSlice;
    int inc6 = inc2 * numSlice;

    // Along X
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {

      if (pnode->GetStatus() == -1)
        {
        break;
        }

      int a = elemCnt + inc2;
      int b = a + inc2;
      int c = elemCnt - inc2;

      if (c < 0)
        {
        continue;
        }

      if(b > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Predict
          *(outFPixel + a) -= 0.5 * (*(tempFPixel + elemCnt) + *(tempFPixel + b));
          // Update
          *(outFPixel + elemCnt) += 0.25 * (*(tempFPixel + c) + *(outFPixel + a));
          break;
        case VTK_DOUBLE:
          // Predict
          *(outDPixel + a) -= 0.5 * (*(tempDPixel + elemCnt) + *(tempDPixel + b));
          // Update
          *(outDPixel + elemCnt) += 0.25 * (*(tempDPixel + c) + *(outDPixel + a));
          break;
        }
      }

    this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
    switch (DataType)
      {
      case VTK_FLOAT:
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      }


    // Along Y
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {
      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int row = (int) floor(elemCnt / dims[0]);

      if ((row % inc) > 0.)
        {
        elemCnt = elemCnt + inc3;
        }

      int a = elemCnt + inc4;
      int b = a + inc4;
      int c = elemCnt - inc4;

      int res = elemCnt + inc2;
      int resa = res + inc4;
      int resb = resa + inc4;
      int resc = res - inc4;

      if (c < 0)
        {
        continue;
        }

      if(resb > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Predict
          *(outFPixel + a) -= 0.5 * (*(tempFPixel + elemCnt) + *(tempFPixel + b));
          // Update
          *(outFPixel + elemCnt) += 0.25 * (*(tempFPixel + c) + *(outFPixel + a));
          // Predict residuals
          *(outFPixel + resa) -= 0.5 * (*(tempFPixel + res) + *(tempFPixel + resb));
          // Update residuals
          *(outFPixel + res) += 0.25 * (*(tempFPixel + resc) + *(outFPixel + resa));
          break;
        case VTK_DOUBLE:
          // Predict
          *(outDPixel + a) -= 0.5 * (*(tempDPixel + elemCnt) + *(tempDPixel + b));
          // Update
          *(outDPixel + elemCnt) += 0.25 * (*(tempDPixel + c) + *(outDPixel + a));
          // Predict residuals
          *(outDPixel + resa) -= 0.5 * (*(tempDPixel + res) + *(tempDPixel + resb));
          // Update residuals
          *(outDPixel + res) += 0.25 * (*(tempDPixel + resc) + *(outDPixel + resa));
          break;
        }
      }

    this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
    switch (DataType)
      {
      case VTK_FLOAT:
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      }

    // Along Z
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {

      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int row = (int) floor(elemCnt / dims[0]);
      if ((row % inc) > 0.)
        {
        elemCnt = elemCnt + inc3;
        }
      int slice = (int) floor(elemCnt / numSlice);
      if ((slice % inc) > 0.)
        {
        elemCnt = elemCnt + inc5;
        }
      int a = elemCnt + inc6;
      int b = a + inc6;
      int c = elemCnt - inc6;

      int e1 = elemCnt + inc2;
      int a1 = a + inc2;
      int b1 = b + inc2;
      int c1 = c + inc2;

      int e2 = elemCnt + dims[0];
      int a2 = a + dims[0];
      int b2 = b + dims[0];
      int c2 = c + dims[0];

      int e3 = e2 + inc2;
      int a3 = a2 + inc2;
      int b3 = b2 + inc2;
      int c3 = c2 + inc2;

      if (c < 0)
        {
        continue;
        }

      if(b3 > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Predict
          *(outFPixel + a) -= 0.5 * (*(tempFPixel + elemCnt) + *(tempFPixel + b));
          // Update
          *(outFPixel + elemCnt) += 0.25 * (*(tempFPixel + c) + *(outFPixel + a));
          // Predict
          *(outFPixel + a1) -= 0.5 * (*(tempFPixel + e1) + *(tempFPixel + b1));
          // Update
          *(outFPixel + e1) += 0.25 * (*(tempFPixel + c1) + *(outFPixel + a1));
          // Predict
          *(outFPixel + a2) -= 0.5 * (*(tempFPixel + e2) + *(tempFPixel + b2));
          // Update
          *(outFPixel + e2) += 0.25 * (*(tempFPixel + c2) + *(outFPixel + a2));
          // Predict
          *(outFPixel + a3) -= 0.5 * (*(tempFPixel + e3) + *(tempFPixel + b3));
          // Update
          *(outFPixel + e3) += 0.25 * (*(tempFPixel + c3) + *(outFPixel + a3));
          break;
        case VTK_DOUBLE:
          // Predict
          *(outDPixel + a) -= 0.5 * (*(tempDPixel + elemCnt) + *(tempDPixel + b));
          // Update
          *(outDPixel + elemCnt) += 0.25 * (*(tempDPixel + c) + *(outDPixel + a));
          // Predict
          *(outDPixel + a1) -= 0.5 * (*(tempDPixel + e1) + *(tempDPixel + b1));
          // Update
          *(outDPixel + e1) += 0.25 * (*(tempDPixel + c1) + *(outDPixel + a1));
          // Predict
          *(outDPixel + a2) -= 0.5 * (*(tempDPixel + e2) + *(tempDPixel + b2));
          // Update
          *(outDPixel + e2) += 0.25 * (*(tempDPixel + c2) + *(outDPixel + a2));
          // Predict
          *(outDPixel + a3) -= 0.5 * (*(tempDPixel + e3) + *(tempDPixel + b3));
          // Update
          *(outDPixel + e3) += 0.25 * (*(tempDPixel + c3) + *(outDPixel + a3));
          break;
        }
      }

    this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
    switch (DataType)
      {
      case VTK_FLOAT:
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      }

    }

  // Inverse Trasform
  int m = 0;
  for (int l = pnode->GetAccuracy(); l >= 1; l--)
    {
    if (pnode->GetStatus() == -1)
      {
      break;
      }
    m++;
    pnode->SetStatus((int) m * 100 / pnode->GetAccuracy());

    int inc = pow(2, l);
    int inc2 = (int) inc / 2;
    int inc3 = (inc * dims[0]) - dims[0];
    int inc4 = inc2 * dims[0];
    int inc5 = (inc * numSlice) - numSlice;
    int inc6 = inc2 * numSlice;

    // Along Z
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {

      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int row = (int) floor(elemCnt / dims[0]);
      if ((row % inc) > 0.)
        {
        elemCnt = elemCnt + inc3;
        }
      int slice = (int) floor(elemCnt / numSlice);
      if ((slice % inc) > 0.)
        {
        elemCnt = elemCnt + inc5;
        }
      int a = elemCnt + inc6;
      int b = a + inc6;
      int c = elemCnt - inc6;

      int e1 = elemCnt + inc2;
      int a1 = a + inc2;
      int b1 = b + inc2;
      int c1 = c + inc2;

      int e2 = elemCnt + dims[0];
      int a2 = a + dims[0];
      int b2 = b + dims[0];
      int c2 = c + dims[0];

      int e3 = e2 + inc2;
      int a3 = a2 + inc2;
      int b3 = b2 + inc2;
      int c3 = c2 + inc2;

      if (c < 0)
        {
        continue;
        }

      if(b3 > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Thresholding
          if (fabs(*(tempFPixel + a)) < delta)
            {
            *(tempFPixel + a) = 0.;
            }
          if (fabs(*(tempFPixel + b)) < delta)
            {
            *(tempFPixel + b) = 0.;
            }
          if (fabs(*(tempFPixel + c)) < delta)
            {
            *(tempFPixel + c) = 0.;
            }
          if (fabs(*(tempFPixel + a1)) < delta)
            {
            *(tempFPixel + a1) = 0.;
            }
          if (fabs(*(tempFPixel + b1)) < delta)
            {
            *(tempFPixel + b1) = 0.;
            }
          if (fabs(*(tempFPixel + c1)) < delta)
            {
            *(tempFPixel + c1) = 0.;
            }
          if (fabs(*(tempFPixel + a2)) < delta)
            {
            *(tempFPixel + a2) = 0.;
            }
          if (fabs(*(tempFPixel + b2)) < delta)
            {
            *(tempFPixel + b2) = 0.;
            }
          if (fabs(*(tempFPixel + c2)) < delta)
            {
            *(tempFPixel + c2) = 0.;
            }
          if (fabs(*(tempFPixel + a3)) < delta)
            {
            *(tempFPixel + a3) = 0.;
            }
          if (fabs(*(tempFPixel + b3)) < delta)
            {
            *(tempFPixel + b3) = 0.;
            }
          if (fabs(*(tempFPixel + c3)) < delta)
            {
            *(tempFPixel + c3) = 0.;
            }
          // Update
          *(outFPixel + elemCnt) -= 0.25 * (*(tempFPixel + c) + *(tempFPixel + a));
          // Predict
          *(outFPixel + a) += 0.5 * (*(outFPixel + elemCnt) + *(tempFPixel + b));
          // Update
          *(outFPixel + e1) -= 0.25 * (*(tempFPixel + c1) + *(tempFPixel + a1));
          // Predict
          *(outFPixel + a1) += 0.5 * (*(outFPixel + e1) + *(tempFPixel + b1));
          // Update
          *(outFPixel + e2) -= 0.25 * (*(tempFPixel + c2) + *(tempFPixel + a2));
          // Predict
          *(outFPixel + a2) += 0.5 * (*(outFPixel + e2) + *(tempFPixel + b2));
          // Update
          *(outFPixel + e3) -= 0.25 * (*(tempFPixel + c3) + *(tempFPixel + a3));
          // Predict
          *(outFPixel + a3) += 0.5 * (*(outFPixel + e3) + *(tempFPixel + b3));
          break;
        case VTK_DOUBLE:
          // Thresholding
          if (fabs(*(tempDPixel + a)) < delta)
            {
            *(tempDPixel + a) = 0.;
            }
          if (fabs(*(tempDPixel + b)) < delta)
            {
            *(tempDPixel + b) = 0.;
            }
          if (fabs(*(tempDPixel + c)) < delta)
            {
            *(tempDPixel + c) = 0.;
            }
          if (fabs(*(tempDPixel + a1)) < delta)
            {
            *(tempDPixel + a1) = 0.;
            }
          if (fabs(*(tempDPixel + b1)) < delta)
            {
            *(tempDPixel + b1) = 0.;
            }
          if (fabs(*(tempDPixel + c1)) < delta)
            {
            *(tempDPixel + c1) = 0.;
            }
          if (fabs(*(tempDPixel + a2)) < delta)
            {
            *(tempDPixel + a2) = 0.;
            }
          if (fabs(*(tempDPixel + b2)) < delta)
            {
            *(tempDPixel + b2) = 0.;
            }
          if (fabs(*(tempDPixel + c2)) < delta)
            {
            *(tempDPixel + c2) = 0.;
            }
          if (fabs(*(tempDPixel + a3)) < delta)
            {
            *(tempDPixel + a3) = 0.;
            }
          if (fabs(*(tempDPixel + b3)) < delta)
            {
            *(tempDPixel + b3) = 0.;
            }
          if (fabs(*(tempDPixel + c3)) < delta)
            {
            *(tempDPixel + c3) = 0.;
            }
          // Update
          *(outDPixel + elemCnt) -= 0.25 * (*(tempDPixel + c) + *(tempDPixel + a));
          // Predict
          *(outDPixel + a) += 0.5 * (*(outDPixel + elemCnt) + *(tempDPixel + b));
          // Update
          *(outDPixel + e1) -= 0.25 * (*(tempDPixel + c1) + *(tempDPixel + a1));
          // Predict
          *(outDPixel + a1) += 0.5 * (*(outDPixel + e1) + *(tempDPixel + b1));
          // Update
          *(outDPixel + e2) -= 0.25 * (*(tempDPixel + c2) + *(tempDPixel + a2));
          // Predict
          *(outDPixel + a2) += 0.5 * (*(outDPixel + e2) + *(tempDPixel + b2));
          // Update
          *(outDPixel + e3) -= 0.25 * (*(tempDPixel + c3) + *(tempDPixel + a3));
          // Predict
          *(outDPixel + a3) += 0.5 * (*(outDPixel + e3) + *(tempDPixel + b3));
          break;
        }
      }

    this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
    switch (DataType)
      {
      case VTK_FLOAT:
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      }

    // Along Y
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {

      if (pnode->GetStatus() == -1)
        {
        break;
        }
      int row = (int) floor(elemCnt / dims[0]);

      if ((row % inc) > 0.)
        {
        elemCnt = elemCnt + inc3;
        }
      int a = elemCnt + inc4;
      int b = a + inc4;
      int c = elemCnt - inc4;

      int res = elemCnt + inc2;
      int resa = res + inc4;
      int resb = resa + inc4;
      int resc = res - inc4;

      if (c < 0)
        {
        continue;
        }

      if(resb > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Thresholding
          if (fabs(*(tempFPixel + a)) < delta)
            {
            *(tempFPixel + a) = 0.;
            }
          if (fabs(*(outFPixel + c)) < delta)
            {
            *(tempFPixel + c) = 0.;
            }
          if (fabs(*(tempFPixel + b)) < delta)
            {
            *(tempFPixel + b) = 0.;
            }
          if (fabs(*(tempFPixel + resa)) < delta)
            {
            *(tempFPixel + resa) = 0.;
            }
          if (fabs(*(tempFPixel + resb)) < delta)
            {
            *(tempFPixel + resb) = 0.;
            }
          if (fabs(*(outFPixel + c)) < delta)
            {
            *(tempFPixel + resc) = 0.;
            }
          // Update
          *(outFPixel + elemCnt) -= 0.25 * (*(tempFPixel + c) + *(tempFPixel + a));
          // Predict
          *(outFPixel + a) += 0.5 * (*(outFPixel + elemCnt) + *(tempFPixel + b));
          // Update residuals
          *(outFPixel + res) -= 0.25 * (*(tempFPixel + resc) + *(tempFPixel + resa));
          // Predict residuals
          *(outFPixel + resa) += 0.5 * (*(outFPixel + res) + *(tempFPixel + resb));
          break;
        case VTK_DOUBLE:
          // Thresholding
          if (fabs(*(tempDPixel + a)) < delta)
            {
            *(tempDPixel + a) = 0.;
            }
          if (fabs(*(outFPixel + c)) < delta)
            {
            *(tempDPixel + c) = 0.;
            }
          if (fabs(*(tempDPixel + b)) < delta)
            {
            *(tempDPixel + b) = 0.;
            }
          if (fabs(*(tempDPixel + resa)) < delta)
            {
            *(tempDPixel + resa) = 0.;
            }
          if (fabs(*(tempDPixel + resb)) < delta)
            {
            *(tempDPixel + resb) = 0.;
            }
          if (fabs(*(outFPixel + c)) < delta)
            {
            *(tempDPixel + resc) = 0.;
            }
          // Update
          *(outDPixel + elemCnt) -= 0.25 * (*(tempDPixel + c) + *(tempDPixel + a));
          // Predict
          *(outDPixel + a) += 0.5 * (*(outDPixel + elemCnt) + *(tempDPixel + b));
          // Update residuals
          *(outDPixel + res) -= 0.25 * (*(tempDPixel + resc) + *(tempDPixel + resa));
          // Predict residuals
          *(outDPixel + resa) += 0.5 * (*(outDPixel + res) + *(tempDPixel + resb));
          break;
        }
      }

    this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
    switch (DataType)
      {
      case VTK_FLOAT:
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      }

    // Along X
    for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + inc)
      {
      if (pnode->GetStatus() == -1)
        {
        break;
        }

      int a = elemCnt + inc2;
      int b = a + inc2;
      int c = elemCnt - inc2;

      if (c < 0)
        {
        continue;
        }
      if(b > numElements)
        {
        break;
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          // Thresholding
          if (fabs(*(tempFPixel + a)) < delta)
            {
            *(tempFPixel + a) = 0.;
            }
          if (fabs(*(tempFPixel + b)) < delta)
            {
            *(tempFPixel + b) = 0.;
            }
          if (fabs(*(tempFPixel + c)) < delta)
            {
            *(tempFPixel + c) = 0.;
            }
          // Update
          *(outFPixel + elemCnt) -= 0.25 * (*(tempFPixel + c) + *(tempFPixel + a));
          // Predict
          *(outFPixel + a) += 0.5 * (*(outFPixel + elemCnt) + *(tempFPixel + b));

          break;
        case VTK_DOUBLE:
          // Thresholding
          if (fabs(*(tempDPixel + a)) < delta)
            {
            *(tempDPixel + a) = 0.;
            }
          if (fabs(*(tempDPixel + b)) < delta)
            {
            *(tempDPixel + b) = 0.;
            }
          if (fabs(*(tempDPixel + c)) < delta)
            {
            *(tempDPixel + c) = 0.;
            }
          // Update
          *(outDPixel + elemCnt) -= 0.25 * (*(tempDPixel + c) + *(tempDPixel + a));
          // Predict
          *(outDPixel + a) += 0.5 * (*(outDPixel + elemCnt) + *(tempDPixel + b));
          break;
        }
      }
    this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
    switch (DataType)
      {
      case VTK_FLOAT:
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      }
    }

  if (reduceX || reduceY || reduceZ)
    {
    int ii = 0;

    int diffDims [3] = {dims[0] - oldDims[0],
                        dims[1] - oldDims[1],
                        dims[2] - oldDims[2]};
    int tempNumRow = oldDims[0] - 1;
    int tempNumSlice = (dims[0] * oldDims[1]);
    for (int elemCnt = 0; elemCnt < oldNumElements; elemCnt++)
      {

      switch (DataType)
        {
        case VTK_FLOAT:
          *(outFPixel + elemCnt) = *(outFPixel + elemCnt + ii);
          break;
        case VTK_DOUBLE:
          *(outDPixel + elemCnt) = *(outDPixel + elemCnt + ii);
          break;
        }

      int ref =  (int) floor((elemCnt + ii) / numSlice);
      ref *= numSlice;
      ref = elemCnt + ii - ref;
      if (ref == tempNumSlice)
        {
        ii += (diffDims[1] * dims[0]);
        }

      ref = (int) floor((elemCnt + ii) / dims[0]);
      ref *= dims[0];
      ref = elemCnt + ii - ref;
      if (ref == tempNumRow)
        {
        ii += diffDims[0];
        }

      }
    outputVolume->GetImageData()->SetDimensions(oldDims);
    }

  outFPixel = NULL;
  outDPixel = NULL;

  delete outFPixel;
  delete outDPixel;

  tempFPixel = NULL;
  tempDPixel = NULL;

  delete tempFPixel;
  delete tempDPixel;

  this->Internal->tempVolumeData->Initialize();

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttribute();
  pnode->SetStatus(0);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;

  return 1;
}

