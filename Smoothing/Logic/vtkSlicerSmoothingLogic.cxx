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
        if ((pnode->GetParameterX() - pnode->GetParameterY()) < 0.001 &&
            (pnode->GetParameterY() - pnode->GetParameterZ()) < 0.001)
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
      success = this->WaveletThresholdingCPUFilter(pnode);
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
  int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
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
  int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
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
int vtkSlicerSmoothingLogic::WaveletThresholdingCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  float *outFPixel = NULL;
  double *outDPixel = NULL;
  int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
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

  bool cancel = false;

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  //rewrite:
  //first of all split: even in the first half, odd in the other (optmized memory access)
  //update: calculate the odd
  //predict: calculate the even
  //to e understanded is if the update and predict can be done in the same cicle

  for (int elemCnt = 0; elemCnt < numElements; elemCnt = elemCnt + 2)
    {
    int stat = pnode->GetStatus();

    if (stat == -1 && omp_get_thread_num() == 0)
      {
      cancel = true;
      }

    if (!cancel)
      {
      int ii = elemCnt + 1;
      *(outFPixel + ii) -= (*(outFPixel + elemCnt));
      *(outFPixel + elemCnt) += 0.5 * *(outFPixel + ii);
      }
    }

  pnode->SetStatus(33);

  int m = 0;
  for (int elemCnt = 0; elemCnt < numElements - dims[0]; elemCnt = elemCnt + 2)
    {
    int stat = pnode->GetStatus();

    if (stat == -1 && omp_get_thread_num() == 0)
      {
      cancel = true;
      }

    if (!cancel)
      {

      int ii = elemCnt + dims[0];
      *(outFPixel + ii) -= (*(outFPixel + elemCnt));
      *(outFPixel + elemCnt) += 0.5 * *(outFPixel + ii);
       m++;
       if(m == dims[0] * 0.5)
         {
         elemCnt += dims[0] * 2;
         m = 0;
         }
       }
     }

  pnode->SetStatus(66);

  m = 0;
  int l = 0;
  for (int elemCnt = 0; elemCnt < numElements - numSlice; elemCnt = elemCnt + 2)
    {
    int stat = pnode->GetStatus();

    if (stat == -1 && omp_get_thread_num() == 0)
      {
      cancel = true;
      }

    if (!cancel)
      {
      int ii = elemCnt + numSlice;
      *(outFPixel + ii) -= (*(outFPixel + elemCnt));
      *(outFPixel + elemCnt) += 0.5 * *(outFPixel + ii);
      m++;
      if (m == dims[0] * 0.5)
        {
        l++;
        elemCnt += dims[0] * 2;
        m = 0;
        if (l == dims[1] * 0.5)
          {
          elemCnt += numSlice * 2;
          l = 0;
          }
        }
      }
    }


  outFPixel = NULL;
  outDPixel = NULL;

  delete outFPixel;
  delete outDPixel;

  if (cancel)
    {
    pnode->SetStatus(0);
    return 0;
    }

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
