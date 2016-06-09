// Logic includes
#include "vtkSlicerAstroVolumeLogic.h"
#include "vtkSlicerSmoothingLogic.h"
#include "vtkSlicerAstroConfigure.h"

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLSmoothingParametersNode.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkVersion.h>

// STD includes
#include <cassert>
#include <iostream>

// OpenMP includes
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
#include <omp.h>
#endif

// vtkOpenGL includes
#include <vtkOpenGLAstroShaderComputation.h>
#include <vtkOpenGLAstroTextureImage.h>

// Qt includes
#include <QtDebug>

#include <iostream>
#include <sys/time.h>

#define SigmatoFWHM 2.3548200450309493

//----------------------------------------------------------------------------
class vtkSlicerSmoothingLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkImageData> tempVolumeData;
  vtkSmartPointer<vtkOpenGLAstroShaderComputation> shaderComputation;
  vtkSmartPointer<vtkOpenGLAstroTextureImage> iterationVolumeTexture;
  vtkSmartPointer<vtkOpenGLAstroTextureImage> outputVolumeTexture;
};

//----------------------------------------------------------------------------
vtkSlicerSmoothingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = vtkSmartPointer<vtkSlicerAstroVolumeLogic>::New();
  this->tempVolumeData = vtkSmartPointer<vtkImageData>::New();
  this->shaderComputation = vtkSmartPointer<vtkOpenGLAstroShaderComputation>::New();
  this->iterationVolumeTexture = vtkSmartPointer<vtkOpenGLAstroTextureImage>::New();
  this->outputVolumeTexture = vtkSmartPointer<vtkOpenGLAstroTextureImage>::New();
}

//---------------------------------------------------------------------------
vtkSlicerSmoothingLogic::vtkInternal::~vtkInternal()
{
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
        if (fabs(pnode->GetParameterX() - pnode->GetParameterY()) < 0.001 &&
            fabs(pnode->GetParameterY() - pnode->GetParameterZ()) < 0.001)
          {
          success = this->IsotropicBoxGPUFilter(pnode);
          }
        else
          {
          success = this->AnisotropicBoxGPUFilter(pnode);
          }
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
          if (fabs(pnode->GetParameterX() - pnode->GetParameterY()) < 0.001 &&
              fabs(pnode->GetParameterY() - pnode->GetParameterZ()) < 0.001)
            {
            success = this->IsotropicGaussianGPUFilter(pnode);
            }
          else
            {
            success = this->AnisotropicGaussianGPUFilter(pnode);
            }
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
        success = this->GradientGPUFilter(pnode);
        }
      break;
      }
    }
  return success;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::AnisotropicBoxCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("Smoothing warning: AnisotropicBoxCPUFilter: "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results taht "
                  "the smoothing algorithm will show poor perfomrance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableAniBoxCPU.txt", ios::out | ios::app);

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  int nItemsX = pnode->GetParameterX();
  if (nItemsX % 2 == 0)
    {
    nItemsX++;
    }
  const int Xmax = (int) ((nItemsX - 1) / 2.);
  int nItemsY = pnode->GetParameterY();
  if (nItemsY % 2 == 0)
    {
    nItemsY++;
    }
  const int Ymax = (int) ((nItemsY - 1) / 2.);
  int nItemsZ = pnode->GetParameterZ();
  if (nItemsZ % 2 == 0)
    {
    nItemsZ++;
    }
  const int Zmax = (int) ((nItemsZ - 1) / 2.);
  const int cont = nItemsX * nItemsY * nItemsZ;
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
  int status = 0;

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel, status)
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int stat = pnode->GetStatus();

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    if (stat == -1 && omp_get_thread_num() == 0)
    #else
    if (stat == -1)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      {
      cancel = true;
      }
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp flush (cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
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

            switch (DataType)
              {
              case VTK_FLOAT:
                *(outFPixel + elemCnt) += *(tempFPixel + posData);
                break;
              case VTK_DOUBLE:
                *(outDPixel + elemCnt) += *(tempDPixel + posData);
                break;
              }
            }
          }
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          *(outFPixel + elemCnt) /= cont;
          break;
        case VTK_DOUBLE:
          *(outDPixel + elemCnt) /= cont;
          break;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (omp_get_thread_num() == 0)
        {
        if(elemCnt / (numElements / (numProcs * 100)) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        }
      #else
      if(elemCnt / (numElements / 100) > status)
        {
        status += 10;
        pnode->SetStatus(status);
        }
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      }
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  qDebug()<<"Box Filter (CPU) Kernel Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << numElements << " | " << numProcs << " | " << mtime << " | ";

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

  if (cancel)
    {
    return 0;
    }

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) <<mtime<<endl;
  //outputFile.close();
  return 1;
}


//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::IsotropicBoxCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("Smoothing warning: IsotropicBoxCPUFilter: "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results taht "
                  "the smoothing algorithm will show poor perfomrance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableIsoBoxCPU.txt", ios::out | ios::app);

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  int nItems = (pnode->GetParameterX());
  if (nItems % 2 == 0)
    {
    nItems++;
    }
  const int is = (int) ((nItems - 1) / 2);

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

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  if (pnode->GetParameterX() > 0.001)
    {
    pnode->SetStatus(10);

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
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

        for (int i = -is; i <= is; i++)
          {
          int ii = elemCnt + i;
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
              *(outFPixel + elemCnt) += *(tempFPixel + ii);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii);
              break;
            }
          }

        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) /= nItems;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) /= nItems;
            break;
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
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
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

        for (int i = -is; i <= is; i++)
          {
          int ii = elemCnt + (i * dims[0]);
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
              *(tempFPixel + elemCnt) += *(outFPixel + ii);
              break;
            case VTK_DOUBLE:
              *(tempDPixel + elemCnt) += *(outDPixel + ii);
              break;
            }
          }

        switch (DataType)
          {
          case VTK_FLOAT:
            *(tempFPixel + elemCnt) /= nItems;
            break;
          case VTK_DOUBLE:
            *(tempDPixel + elemCnt) /= nItems;
            break;
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
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
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

        for (int i = -is; i <= is; i++)
          {
          int ii = elemCnt + (i * numSlice);
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
              *(outFPixel + elemCnt) += *(tempFPixel + ii);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii);
              break;
            }
          }

        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) /= nItems;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) /= nItems;
            break;
          }
        }
      }
    }
  else
    {
    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Box Filter (CPU) Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << numElements << " | " << numProcs << " | " << mtime << " | ";

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

  if (cancel)
    {
    return 0;
    }

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime <<endl;

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::AnisotropicBoxGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableAniBoxGPU.txt", ios::out | ios::app);

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const double spacingX = 1. / dims[0];
  const double spacingY = 1. / dims[1];
  const double spacingZ = 1. / dims[2];

  int nItemsX = (pnode->GetParameterX());
  if (nItemsX % 2 == 0)
    {
    nItemsX++;
    }
  const int LengthKernelX = (int) (nItemsX - 1) / 2;
  int nItemsY = (pnode->GetParameterY());
  if (nItemsY % 2 == 0)
    {
    nItemsY++;
    }
  const int LengthKernelY = (int) (nItemsY - 1) / 2;
  int nItemsZ = (pnode->GetParameterZ());
  if (nItemsZ % 2 == 0)
    {
    nItemsZ++;
    }
  const int LengthKernelZ = (int) (nItemsZ - 1) / 2;

  int cont = nItemsX * nItemsY * nItemsZ;

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  // set the shaders
  this->Internal->iterationVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);
  this->Internal->outputVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);

  this->Internal->iterationVolumeTexture->SetInterpolate(1);
  this->Internal->outputVolumeTexture->SetInterpolate(1);

  // set the ImageData in the VolumeTextures
  /* since the OpenGL texture will be floats in the range 0 to 1,
  all negative values will get clamped to zero.*/

  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  double scale = 1. / (range[1] - range[0]);
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) -= range[0];
    *(fPixel+elemCnt) *= scale;
    }

  this->Internal->iterationVolumeTexture->SetImageData(outputVolume->GetImageData());

  this->Internal->outputVolumeTexture->SetImageData(outputVolume->GetImageData());
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(outputVolume->GetImageData());
  this->Internal->shaderComputation->AcquireResultRenderbuffer();

  // set the kernels
  std::string header = "#version 120 \n"
     "vec3 transformPoint(const in vec3 samplePoint) \n"
     "  { \n"
     "  return samplePoint; \n"
     "  } \n"
     "uniform sampler3D \n"
     "textureUnit0, \n"
     "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120 \n"
      "attribute vec3 vertexAttribute; \n"
      "attribute vec2 textureCoordinateAttribute; \n"
      "varying vec3 interpolatedTextureCoordinate; \n"
      "void main() \n"
      "  { \n"
      "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5); \n"
      "  gl_Position = vec4(vertexAttribute, 1.); \n"
      "  }\n";

  this->Internal->shaderComputation->SetVertexShaderSource(vertexShaderTemplate.c_str());

  std::string fragmentShaderTemplate = "uniform float slice; \n"
      "varying vec3 interpolatedTextureCoordinate; \n"
      "void main() \n"
      "{ \n"
        "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice); \n"
        "vec4 smooth = vec4(0.); \n"
        "vec4 sample = texture3D(textureUnit%d, samplePoint); \n"
        "for (int offsetX = -%d; offsetX <= %d; offsetX++){ \n"
          "for (int offsetY = -%d; offsetY <= %d; offsetY++){ \n"
            "for (int offsetZ = -%d; offsetZ <= %d; offsetZ++){ \n"
              "vec3 offset1 = vec3(%4.16f * offsetX, %4.16f * offsetY, %4.16f * offsetZ); \n"
              "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1); \n"
              "smooth += sample1; \n"
            "} \n"
          "} \n"
        "} \n"
        "gl_FragColor = vec4(vec3(smooth / %d), 1.); \n"
      "}\n";

  int nBuffer = 2000;
  char buffer [nBuffer];
  bool cancel = false;

  int textureUnit = 0;
  sprintf(buffer,
          fragmentShaderTemplate.c_str(),
          textureUnit,
          LengthKernelX,
          LengthKernelX,
          LengthKernelY,
          LengthKernelY,
          LengthKernelZ,
          LengthKernelZ,
          spacingX,
          spacingY,
          spacingZ,
          textureUnit,
          cont);

  std::string shaders = header + buffer;
  this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());

  pnode->SetStatus(30);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Box Filter (GPU, OpenGL) Benchmark "<< endl;
  qDebug()<<"Setup Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << numElements << " | " << mtime << " | ";

  gettimeofday(&start, NULL);

  int status = pnode->GetStatus();

  if (status == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }
    }
  else
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Kernel Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";
  if (cancel)
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(60);

  gettimeofday(&start, NULL);

  // get the output
  this->Internal->outputVolumeTexture->ReadBack();
  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"ReadBack Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";
  outputVolume->SetAndObserveImageData(this->Internal->outputVolumeTexture->GetImageData());

  // porting back the range to the original one
  gettimeofday(&start, NULL);

  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) /= scale;
    *(fPixel+elemCnt) += range[0];
    }

  pnode->SetStatus(80);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Setup Output Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";
  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime <<endl;
  fPixel = NULL;
  delete fPixel;

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

  pnode->SetStatus(0);

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::IsotropicBoxGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableIsoBoxGPU.txt", ios::out | ios::app);

  int *dims = outputVolume->GetImageData()->GetDimensions();

  const double spacingX = 1. / dims[0];
  const double spacingY = 1. / dims[1];
  const double spacingZ = 1. / dims[2];

  int nItems = (pnode->GetParameterX());
  if (nItems % 2 == 0)
    {
    nItems++;
    }
  const int LengthKernel = (int) (nItems - 1) / 2;

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  // set the shaders
  this->Internal->iterationVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);
  this->Internal->outputVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);

  this->Internal->iterationVolumeTexture->SetInterpolate(1);
  this->Internal->outputVolumeTexture->SetInterpolate(1);

  // set the ImageData in the VolumeTextures
  /* since the OpenGL texture will be floats in the range 0 to 1,
  all negative values will get clamped to zero.*/

  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  double scale = 1. / (range[1] - range[0]);
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) -= range[0];
    *(fPixel+elemCnt) *= scale;
    }

  this->Internal->iterationVolumeTexture->SetImageData(outputVolume->GetImageData());

  this->Internal->outputVolumeTexture->SetImageData(outputVolume->GetImageData());
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(outputVolume->GetImageData());
  this->Internal->shaderComputation->AcquireResultRenderbuffer();

  // set the kernels
  std::string header = "#version 120 \n"
     "vec3 transformPoint(const in vec3 samplePoint) \n"
     "  { \n"
     "  return samplePoint; \n"
     "  } \n"
     "uniform sampler3D \n"
     "textureUnit0, \n"
     "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120 \n"
      "attribute vec3 vertexAttribute; \n"
      "attribute vec2 textureCoordinateAttribute; \n"
      "varying vec3 interpolatedTextureCoordinate; \n"
      "void main() \n"
      "  { \n"
      "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5); \n"
      "  gl_Position = vec4(vertexAttribute, 1.); \n"
      "  }\n";

  this->Internal->shaderComputation->SetVertexShaderSource(vertexShaderTemplate.c_str());

  std::string fragmentShaderTemplate = "uniform float slice; \n"
      "varying vec3 interpolatedTextureCoordinate; \n"
      "void main() \n"
      "{ \n"
        "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice); \n"
        "vec4 smooth = vec4(0.); \n"
        "vec4 sample = texture3D(textureUnit%d, samplePoint); \n"
        "for (int offset = -%d; offset <= %d; offset++){ \n"
          "vec3 offset1 = vec3(%4.16f * offset, %4.16f * offset, %4.16f * offset); \n"
          "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1); \n"
          "smooth += sample1; \n"
        "} \n"
        "gl_FragColor = vec4(vec3(smooth / %d), 1.); \n"
      "}\n";

  int nBuffer = 2000;
  char buffer [nBuffer];
  bool cancel = false;

  pnode->SetStatus(30);

  int status = pnode->GetStatus();

  if (status == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {

    int textureUnit = 0;
    sprintf(buffer,
            fragmentShaderTemplate.c_str(),
            textureUnit,
            LengthKernel,
            LengthKernel,
            spacingX,
            0.,
            0.,
            textureUnit,
            nItems);

    std::string shaders = header + buffer;
    this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());

    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
    qDebug()<<"Box Filter (GPU, OpenGL) Benchmark "<<endl;
    qDebug()<<"Setup Time : "<<mtime<<" ms "<<endl;
    //outputFile << setprecision(5) << numElements << " | " << mtime << " | ";
    mtime = 0.;

    gettimeofday(&start, NULL);

    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }

    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime += ((seconds) * 1000 + useconds/1000.0) + 0.5;
    }
  else
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(40);

  status = pnode->GetStatus();

  if (status == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {

    int textureUnit = 1;
    sprintf(buffer,
            fragmentShaderTemplate.c_str(),
            textureUnit,
            LengthKernel,
            LengthKernel,
            0.,
            spacingY,
            0.,
            textureUnit,
            nItems);

    std::string shaders = header + buffer;
    this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());

    gettimeofday(&start, NULL);

    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->iterationVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->outputVolumeTexture->Activate(1);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }

    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime += ((seconds) * 1000 + useconds/1000.0) + 0.5;
    }
  else
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(50);

  status = pnode->GetStatus();

  if (status == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {

    int textureUnit = 0;
    sprintf(buffer,
            fragmentShaderTemplate.c_str(),
            textureUnit,
            LengthKernel,
            LengthKernel,
            0.,
            0.,
            spacingZ,
            textureUnit,
            nItems);

    std::string shaders = header + buffer;
    this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());

    gettimeofday(&start, NULL);

    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }

    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime += ((seconds) * 1000 + useconds/1000.0) + 0.5;
    qDebug()<<"Kernel Time"<<mtime<<" ms "<<endl;
    //outputFile << setprecision(5) << mtime << " | ";
    }
  else
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  if (cancel)
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(60);

  // get the output
  gettimeofday(&start, NULL);

  this->Internal->outputVolumeTexture->ReadBack();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"ReadBack Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";

  outputVolume->SetAndObserveImageData(this->Internal->outputVolumeTexture->GetImageData());

  // porting backthe range to the original one
  gettimeofday(&start, NULL);

  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) /= scale;
    *(fPixel+elemCnt) += range[0];
    }

  pnode->SetStatus(80);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Setup Output Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime <<endl;

  fPixel = NULL;
  delete fPixel;

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

  pnode->SetStatus(0);

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::AnisotropicGaussianCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("Smoothing warning: AnisotropicGaussianCPUFilter: "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results taht "
                  "the smoothing algorithm will show poor perfomrance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableAniGaussCPU.txt", ios::out | ios::app);

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  const int Xmax = (int) (pnode->GetKernelLengthX() - 1) / 2.;
  const int Ymax = (int) (pnode->GetKernelLengthY() - 1) / 2.;
  const int Zmax = (int) (pnode->GetKernelLengthZ() - 1) / 2.;
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

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel, status)
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int stat = pnode->GetStatus();

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    if (stat == -1 && omp_get_thread_num() == 0)
    #else
    if (stat == -1)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      {
      cancel = true;
      }

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp flush (cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
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
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (omp_get_thread_num() == 0)
        {
        if(elemCnt / (numElements / (numProcs * 100)) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        }
      #else
      if(elemCnt / (numElements / 100) > status)
        {
        status += 10;
        pnode->SetStatus(status);
        }
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      }
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Gaussian Filter (CPU) Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << numElements << " | " << numProcs << " | " << mtime << " | ";
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

  if (cancel)
    {
    return 0;
    }

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
//outputFile << setprecision(5) << mtime <<endl;
  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::IsotropicGaussianCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("Smoothing warning: IsotropicGaussianCPUFilter: "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results taht "
                  "the smoothing algorithm will show poor perfomrance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableIsoGaussCPU.txt", ios::out | ios::app);

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  const int is = (int) (pnode->GetKernelLengthX());
  const int is2 = (int) - ((is - 1)/ 2);
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

  double *GaussKernel1D = static_cast<double*> (pnode->GetGaussianKernel1D()->GetVoidPointer(0));
  bool cancel = false;

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  if (pnode->GetParameterX() > 0.001)
    {
    pnode->SetStatus(10);

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
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

        for (int i = 0; i <= is; i++)
          {
          int ii = elemCnt + i + is2;
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
              *(outFPixel + elemCnt) += *(tempFPixel + ii) * *(GaussKernel1D + i);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii) * *(GaussKernel1D + i);
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

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
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
        for (int i = 0; i <= is; i++)
          {
          int ii = elemCnt + ((i + is2) * dims[0]);
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
              *(tempFPixel + elemCnt) += *(outFPixel + ii) * *(GaussKernel1D + i);
              break;
            case VTK_DOUBLE:
              *(tempDPixel + elemCnt) += *(outDPixel + ii) * *(GaussKernel1D + i);
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

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
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
        for (int i = 0; i <= is; i++)
          {
          int ii = elemCnt + ((i + is2) * numSlice);
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
              *(outFPixel + elemCnt) += *(tempFPixel + ii) * *(GaussKernel1D + i);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii) * *(GaussKernel1D + i);
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

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Gaussian Filter (CPU) Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << numElements << " | " << numProcs << " | " << mtime << " | ";
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

  if (cancel)
    {
    return 0;
    }

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime <<endl;

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::AnisotropicGaussianGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableAniGaussGPU.txt", ios::out | ios::app);

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const double spacingX = 1. / dims[0];
  const double spacingY = 1. / dims[1];
  const double spacingZ = 1. / dims[2];

  const int LengthKernelX = (int) (pnode->GetKernelLengthX() - 1) / 2;
  const int LengthKernelY = (int) (pnode->GetKernelLengthY() - 1) / 2;
  const int LengthKernelZ = (int) (pnode->GetKernelLengthZ() - 1) / 2;

  const double sigmaX = pnode->GetParameterX() / SigmatoFWHM;
  const double sigmaX2 = 2 * sigmaX * sigmaX;
  const double sigmaY = pnode->GetParameterY() / SigmatoFWHM;
  const double sigmaY2 = 2 * sigmaY * sigmaY;
  const double sigmaZ = pnode->GetParameterZ() / SigmatoFWHM;
  const double sigmaZ2 = 2 * sigmaZ * sigmaZ;

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  // set the shaders
  this->Internal->iterationVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);
  this->Internal->outputVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);

  this->Internal->iterationVolumeTexture->SetInterpolate(1);
  this->Internal->outputVolumeTexture->SetInterpolate(1);

  // set the ImageData in the VolumeTextures
  /* since the OpenGL texture will be floats in the range 0 to 1,
  all negative values will get clamped to zero.*/

  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  double scale = 1. / (range[1] - range[0]);
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) -= range[0];
    *(fPixel+elemCnt) *= scale;
    }

  this->Internal->iterationVolumeTexture->SetImageData(outputVolume->GetImageData());

  this->Internal->outputVolumeTexture->SetImageData(outputVolume->GetImageData());
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(outputVolume->GetImageData());
  this->Internal->shaderComputation->AcquireResultRenderbuffer();

  // set the kernels
  std::string header = "#version 120 \n"
     "vec3 transformPoint(const in vec3 samplePoint) \n"
     "  { \n"
     "  return samplePoint; \n"
     "  } \n"
     "uniform sampler3D \n"
     "textureUnit0, \n"
     "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120 \n"
      "attribute vec3 vertexAttribute; \n"
      "attribute vec2 textureCoordinateAttribute; \n"
      "varying vec3 interpolatedTextureCoordinate; \n"
      "void main() \n"
      "  { \n"
      "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5); \n"
      "  gl_Position = vec4(vertexAttribute, 1.); \n"
      "  }\n";

  this->Internal->shaderComputation->SetVertexShaderSource(vertexShaderTemplate.c_str());

  std::string fragmentShaderTemplate = "uniform float slice; \n"
      "varying vec3 interpolatedTextureCoordinate; \n"
      "void main() \n"
      "{ \n"
        "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice); \n"
        "vec4 smooth = vec4(0.); \n"
        "vec4 sum = vec4(0.); \n"
        "vec4 sample = texture3D(textureUnit%d, samplePoint); \n"
        "for (int offsetX = -%d; offsetX <= %d; offsetX++){ \n"
          "for (int offsetY = -%d; offsetY <= %d; offsetY++){ \n"
            "for (int offsetZ = -%d; offsetZ <= %d; offsetZ++){ \n"
              "vec3 offset1 = vec3(%4.16f * offsetX, %4.16f * offsetY, %4.16f * offsetZ); \n"
              "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1); \n"
              "float expA = offsetX * offsetX / %4.16f; \n"
              "float expB = offsetY * offsetY / %4.16f; \n"
              "float expC = offsetZ * offsetZ / %4.16f; \n"
              "float kernel = exp(-(expA + expB + expC)); \n"
              "smooth += sample1 * kernel; \n"
              "sum += kernel; \n"
            "} \n"
          "} \n"
        "} \n"
        "gl_FragColor = vec4(vec3(smooth / sum), 1.); \n"
      "}\n";

  std::string fragmentShaderTemplateWithRotation = "uniform float slice; \n"
      "varying vec3 interpolatedTextureCoordinate; \n"
      "void main() \n"
      "{ \n"
        "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice); \n"
        "vec4 smooth = vec4(0.); \n"
        "vec4 sum = vec4(0.); \n"
        "vec4 sample = texture3D(textureUnit%d, samplePoint); \n"
        "for (int offsetX = -%d; offsetX <= %d; offsetX++){ \n"
          "for (int offsetY = -%d; offsetY <= %d; offsetY++){ \n"
            "for (int offsetZ = -%d; offsetZ <= %d; offsetZ++){ \n"
              "float x = offsetX * %4.16f * %4.16f - offsetY * %4.16f * %4.16f + offsetZ * %4.16f; \n"
              "float y = offsetX * (%4.16f * %4.16f * %4.16f + %4.16f * %4.16f) + "
                         "offsetY * (%4.16f * %4.16f - %4.16f * %4.16f * %4.16f) - offsetZ * %4.16f * %4.16f; \n"
              "float z = offsetX * (-%4.16f * %4.16f * %4.16f + %4.16f * %4.16f) + "
                         "offsetY * (%4.16f * %4.16f + %4.16f * %4.16f * %4.16f) + offsetZ * %4.16f * %4.16f; \n"
              "vec3 offset1 = vec3(%4.16f * offsetX, %4.16f * offsetY, %4.16f * offsetZ); \n"
              "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1); \n"
              "float expA = x * x / %4.16f; \n"
              "float expB = y * y / %4.16f; \n"
              "float expC = z * z / %4.16f; \n"
              "float kernel = exp(-(expA + expB + expC)); \n"
              "smooth += sample1 * kernel; \n"
              "sum += kernel; \n"
            "} \n"
          "} \n"
        "} \n"
        "gl_FragColor = vec4(vec3(smooth / sum), 1.); \n"
      "}\n";

  int nBuffer = 2000;
  char buffer [nBuffer];
  bool cancel = false;

  int textureUnit = 0;

  if (pnode->GetRx() < 0.001 && pnode->GetRy() < 0.001 && pnode->GetRz() < 0.001)
    {
    sprintf(buffer,
            fragmentShaderTemplate.c_str(),
            textureUnit,
            LengthKernelX,
            LengthKernelX,
            LengthKernelY,
            LengthKernelY,
            LengthKernelZ,
            LengthKernelZ,
            spacingX,
            spacingY,
            spacingZ,
            textureUnit,
            sigmaX2,
            sigmaY2,
            sigmaZ2);

    std::string shaders = header + buffer;
    this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());
    }
  else
    {
    double rx = pnode->GetRx() * atan(1.) / 45.;
    double ry = pnode->GetRy() * atan(1.) / 45.;
    double rz = pnode->GetRz() * atan(1.) / 45.;

    double cx = cos(rx);
    double sx = sin(rx);
    double cy = cos(ry);
    double sy = sin(ry);
    double cz = cos(rz);
    double sz = sin(rz);
    sprintf(buffer,
            fragmentShaderTemplateWithRotation.c_str(),
            textureUnit,
            LengthKernelX,
            LengthKernelX,
            LengthKernelY,
            LengthKernelY,
            LengthKernelZ,
            LengthKernelZ,
            cy, cz, cy, sz, sy,
            cz, sx, sy, cx, sz, cx, cz, sx, sy, sz, cy, sx,
            cx, cz, sy, sx, sz, cz, sx, cx, sy, sz, cx, cy,
            spacingX,
            spacingY,
            spacingZ,
            textureUnit,
            sigmaX2,
            sigmaY2,
            sigmaZ2);

    std::string shaders = header + buffer;
    this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());
    }

  int status = pnode->GetStatus();

  if (status == -1)
    {
    cancel = true;
    }

  pnode->SetStatus(30);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Gaussian Filter (GPU, OpenGL) Benchmark "<< endl;
  qDebug()<<"Setup Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << numElements << " | " << mtime << " | ";

  if(!cancel)
    {
    gettimeofday(&start, NULL);
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }

    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
    qDebug()<<"Kernel Time : "<<mtime<<" ms "<<endl;
    //outputFile << setprecision(5) << mtime << " | ";
    }
  else
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  if (cancel)
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(60);

  // get the output
  gettimeofday(&start, NULL);

  this->Internal->outputVolumeTexture->ReadBack();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"ReadBack Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";

  outputVolume->SetAndObserveImageData(this->Internal->outputVolumeTexture->GetImageData());

  // porting back the range to the original one
  gettimeofday(&start, NULL);

  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) /= scale;
    *(fPixel+elemCnt) += range[0];
    }

  pnode->SetStatus(80);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Setup Output Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime <<endl;

  fPixel = NULL;
  delete fPixel;

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

  pnode->SetStatus(0);

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::IsotropicGaussianGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableIsoGaussGPU.txt", ios::out | ios::app);

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  double spacingX = 1. / dims[0];
  double spacingY = 1. / dims[1];
  double spacingZ = 1. / dims[2];

  const int LengthKernel = (int) (pnode->GetKernelLengthX() - 1) / 2;
  const double sigma = pnode->GetParameterX() / SigmatoFWHM;
  const double sigma2 = 2 * sigma * sigma;

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  // set the shaders
  this->Internal->iterationVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);
  this->Internal->outputVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);

  this->Internal->iterationVolumeTexture->SetInterpolate(1);
  this->Internal->outputVolumeTexture->SetInterpolate(1);

  // set the ImageData in the VolumeTextures
  /* since the OpenGL texture will be floats in the range 0 to 1,
  all negative values will get clamped to zero.*/

  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  double scale = 1. / (range[1] - range[0]);
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) -= range[0];
    *(fPixel+elemCnt) *= scale;
    }

  this->Internal->iterationVolumeTexture->SetImageData(outputVolume->GetImageData());

  this->Internal->outputVolumeTexture->SetImageData(outputVolume->GetImageData());
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(outputVolume->GetImageData());
  this->Internal->shaderComputation->AcquireResultRenderbuffer();

  // set the kernels
  std::string header = "#version 120 \n"
     "vec3 transformPoint(const in vec3 samplePoint) \n"
     "  { \n"
     "  return samplePoint; \n"
     "  } \n"
     "uniform sampler3D \n"
     "textureUnit0, \n"
     "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120 \n"
      "attribute vec3 vertexAttribute; \n"
      "attribute vec2 textureCoordinateAttribute; \n"
      "varying vec3 interpolatedTextureCoordinate; \n"
      "void main() \n"
      "  { \n"
      "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5); \n"
      "  gl_Position = vec4(vertexAttribute, 1.); \n"
      "  }\n";

  this->Internal->shaderComputation->SetVertexShaderSource(vertexShaderTemplate.c_str());

  int nBuffer = 2000;
  char buffer [nBuffer];
  bool cancel = false;

  int status = pnode->GetStatus();

  if (status == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {

    std::string fragmentShaderTemplate = "uniform float slice; \n"
        "varying vec3 interpolatedTextureCoordinate; \n"
        "void main() \n"
        "{ \n"
          "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice); \n"
          "vec4 smooth = vec4(0.); \n"
          "vec4 sum = vec4(0.); \n"
          "vec4 sample = texture3D(textureUnit%d, samplePoint); \n"
          "for (int offsetX = -%d; offsetX <= %d; offsetX++){ \n"
            "vec3 offset1 = vec3(%4.16f * offsetX, 0., 0.); \n"
            "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1); \n"
            "float kernel = exp(-(offsetX * offsetX) / %4.16f); \n"
            "smooth += sample1 * kernel; \n"
            "sum += kernel; \n"
          "} \n"
          "gl_FragColor = vec4(vec3(smooth / sum), 1.); \n"
        "}\n";

    int textureUnit = 0;
    sprintf(buffer,
            fragmentShaderTemplate.c_str(),
            textureUnit,
            LengthKernel,
            LengthKernel,
            spacingX,
            textureUnit,
            sigma2);

    std::string shaders = header + buffer;
    this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());

    pnode->SetStatus(30);

    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
    qDebug()<<"Gaussian Filter (GPU, OpenGL) Benchmark "<<endl;
    qDebug()<<"Setup Time : "<<mtime<<" ms "<<endl;
    //outputFile << setprecision(5) << numElements << " | " << mtime << " | ";

    gettimeofday(&start, NULL);
    mtime = 0.;
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }

    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime += ((seconds) * 1000 + useconds/1000.0) + 0.5;
    }
  else
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(40);

  status = pnode->GetStatus();

  if (status == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {

    std::string fragmentShaderTemplate = "uniform float slice; \n"
        "varying vec3 interpolatedTextureCoordinate; \n"
        "void main() \n"
        "{ \n"
          "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice); \n"
          "vec4 smooth = vec4(0.); \n"
          "vec4 sum = vec4(0.); \n"
          "vec4 sample = texture3D(textureUnit%d, samplePoint); \n"
          "for (int offsetY = -%d; offsetY <= %d; offsetY++){ \n"
            "vec3 offset1 = vec3(0., %4.16f * offsetY, 0.); \n"
            "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1); \n"
            "float kernel = exp(-(offsetY * offsetY) / %4.16f); \n"
            "smooth += sample1 * kernel; \n"
            "sum += kernel; \n"
          "} \n"
          "gl_FragColor = vec4(vec3(smooth / sum), 1.); \n"
        "}\n";

    int textureUnit = 1;
    sprintf(buffer,
            fragmentShaderTemplate.c_str(),
            textureUnit,
            LengthKernel,
            LengthKernel,
            spacingY,
            textureUnit,
            sigma2);

    std::string shaders = header + buffer;
    this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());
        gettimeofday(&start, NULL);
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->iterationVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->outputVolumeTexture->Activate(1);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }
    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime += ((seconds) * 1000 + useconds/1000.0) + 0.5;
    }
  else
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(50);

  status = pnode->GetStatus();

  if (status == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {

    std::string fragmentShaderTemplate = "uniform float slice; \n"
        "varying vec3 interpolatedTextureCoordinate; \n"
        "void main() \n"
        "{ \n"
          "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice); \n"
          "vec4 smooth = vec4(0.); \n"
          "vec4 sum = vec4(0.); \n"
          "vec4 sample = texture3D(textureUnit%d, samplePoint); \n"
          "for (int offsetZ = -%d; offsetZ <= %d; offsetZ++){ \n"
            "vec3 offset1 = vec3(0., 0., %4.16f * offsetZ); \n"
            "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1); \n"
            "float kernel = exp(-(offsetZ * offsetZ) / %4.16f); \n"
            "smooth += sample1 * kernel; \n"
            "sum += kernel; \n"
          "} \n"
          "gl_FragColor = vec4(vec3(smooth / sum), 1.); \n"
        "}\n";

    int textureUnit = 0;
    sprintf(buffer,
            fragmentShaderTemplate.c_str(),
            textureUnit,
            LengthKernel,
            LengthKernel,
            spacingZ,
            textureUnit,
            sigma2);

    std::string shaders = header + buffer;
    this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());
        gettimeofday(&start, NULL);
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }

    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime += ((seconds) * 1000 + useconds/1000.0) + 0.5;
    qDebug()<<"Kernel Time : "<<mtime<<" ms "<<endl;
    //outputFile << setprecision(5) << mtime << " | ";
    }
  else
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  if (cancel)
    {
    fPixel = NULL;
    delete fPixel;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(60);

  // get the output
  gettimeofday(&start, NULL);

  this->Internal->outputVolumeTexture->ReadBack();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"ReadBack Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";

  outputVolume->SetAndObserveImageData(this->Internal->outputVolumeTexture->GetImageData());

  // porting backthe range to the original one
  gettimeofday(&start, NULL);

  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) /= scale;
    *(fPixel+elemCnt) += range[0];
    }

  pnode->SetStatus(80);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Setup Output Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";
  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime <<endl;
  fPixel = NULL;
  delete fPixel;

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

  pnode->SetStatus(0);

  return 1;
}


//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::GradientCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("Smoothing warning: GradientCPUFilter: "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results taht "
                  "the smoothing algorithm will show poor perfomrance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableGradientCPU.txt", ios::out | ios::app);

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  const int numSlice = dims[0] * dims[1];
  const double noise = StringToDouble(outputVolume->GetAttribute("SlicerAstro.RMS"));
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

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  for (int i = 1; i <= pnode->GetAccuracy(); i++)
    {

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
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
            norm = 1. + (Pixel2 / noise2);
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
            norm = 1. + (Pixel2 / noise2);
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

    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);

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

    pnode->SetStatus((int) i * 100 / pnode->GetAccuracy());
    }


  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Intensity driven Gradient Filter (CPU) Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << numElements << " | " << numProcs << " | " << mtime << " | ";
  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  double noiseMean = StringToDouble(outputVolume->GetAttribute("SlicerAstro.NOISEMEAN"));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    switch (DataType)
      {
      case VTK_FLOAT:
        *(outFPixel+elemCnt) -= noiseMean;
        break;
      case VTK_DOUBLE:
        *(outDPixel+elemCnt) -= noiseMean;
        break;
      }
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime <<endl;

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

  return 1;
}

typedef struct float4 {
    float x;
    float y;
    float z;
    float w;
} float4;

//----------------------------------------------------------------------------
inline float4 EncodeFloatRGBA(float v) {
  float4 enc;
  float intpart;
  enc.x = modff(16777216. * v, &intpart);
  enc.y = modff(65536. * v, &intpart);
  enc.z = modff(256. * v, &intpart);
  enc.w = modff(v, &intpart);
  float frac = 1./256.;
  enc.y -= enc.x * frac;
  enc.z -= enc.y * frac;
  enc.w -= enc.z * frac;
  return enc;
}

//----------------------------------------------------------------------------
inline float DecodeFloatRGBA(float4 v) {
  return v.x / 16777216. + v.y / 65536. + v.z / 256. + v.w;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::GradientGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  // ofstream outputFile("./tableGradientGPU.txt", ios::out | ios::app);

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);
  int *dims = outputVolume->GetImageData()->GetDimensions();
  double spacingX = 1. / dims[0];
  double spacingY = 1. / dims[1];
  double spacingZ = 1. / dims[2];

  // set the shaders
  this->Internal->iterationVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);
  this->Internal->outputVolumeTexture->SetShaderComputation(this->Internal->shaderComputation);

  this->Internal->iterationVolumeTexture->SetInterpolate(1);
  this->Internal->outputVolumeTexture->SetInterpolate(1);

  // set the ImageData in the VolumeTextures
  // since the OpenGL texture will be floats in the range 0 to 1,
  // all negative values will get clamped to zero.

  this->Internal->tempVolumeData->SetDimensions(dims);
  this->Internal->tempVolumeData->AllocateScalars(VTK_FLOAT, 4);

  //create a 4 componet vtkImageData with the value packed
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));
  float *fPixelTemp;
  fPixelTemp =  static_cast<float*>(this->Internal->tempVolumeData->GetScalarPointer(0,0,0));

  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  double scale = 1. / (range[1] - range[0]);

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int i = elemCnt * 4;
    int i2 = i + 1;
    int i3 = i + 2;
    int i4 = i + 3;

    *(fPixel+elemCnt) -= range[0];
    *(fPixel+elemCnt) *= scale;

    float4 value = EncodeFloatRGBA(*(fPixel+elemCnt));

    *(fPixelTemp+i) = value.x;
    *(fPixelTemp+i2) = value.y;
    *(fPixelTemp+i3) = value.z;
    *(fPixelTemp+i4) = value.w;
    }

  this->Internal->iterationVolumeTexture->SetImageData(this->Internal->tempVolumeData);
  this->Internal->outputVolumeTexture->SetImageData(this->Internal->tempVolumeData);
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(this->Internal->tempVolumeData);
  this->Internal->shaderComputation->AcquireResultRenderbuffer();


  //set Intensity-Driven normalization
  double noise = StringToDouble(outputVolume->GetAttribute("SlicerAstro.RMS"));
  noise *= scale;
  double norm = noise * noise * pnode->GetK() * pnode->GetK();

  // set the kernels
  std::string header = "#version 120 \n"
     "vec3 transformPoint(const in vec3 samplePoint) \n"
     "  { \n"
     "  return samplePoint; \n"
     "  } \n"
     "uniform sampler3D \n"
     "textureUnit0, \n"
     "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120\n"
      "attribute vec3 vertexAttribute;\n"
      "attribute vec2 textureCoordinateAttribute;\n"
      "varying vec3 interpolatedTextureCoordinate;\n"
      "void main() \n"
      "  {\n"
      "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5);\n"
      "  gl_Position = vec4(vertexAttribute, 1.);\n"
      "  }\n";

  this->Internal->shaderComputation->SetVertexShaderSource(vertexShaderTemplate.c_str());

  std::string fragmentShaderTemplate = "uniform float slice;\n"
    "varying vec3 interpolatedTextureCoordinate;\n"
    "vec4 pack_float(const in float value)\n"
    "{\n"
       "const vec4 bit_shift = vec4(256.0*256.0*256.0, 256.0*256.0, 256.0, 1.0);\n"
       "const vec4 bit_mask  = vec4(0.0, 1.0/256.0, 1.0/256.0, 1.0/256.0);\n"
       "vec4 res = fract(value * bit_shift);\n"
       "res -= res.xxyz * bit_mask;\n"
       "return res;\n"
     "}\n"

     "float unpack_float(const in vec4 rgba_value)\n"
     "{\n"
       "const vec4 bit_shift = vec4(1.0/(256.0*256.0*256.0), 1.0/(256.0*256.0), 1.0/256.0, 1.0);\n"
       "float value = dot(rgba_value, bit_shift);\n"
       "return value;\n"
     "}\n"

     "void main()\n"
     "{\n"
       "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice);\n"
       "float sum = 0.;\n"
       "float sample = unpack_float(texture3D(textureUnit%d, samplePoint));\n"
       "float sample2 = sample * sample;\n"
       "float norm = 1. + (sample2 / %4.16f);\n"
       "vec3 offsetA1 = %4.16f * vec3(-1., 0., 0.);\n"
       "vec3 offsetB1 = %4.16f * vec3(+1., 0., 0.);\n"
       "float sampleA1 = unpack_float(texture3D(textureUnit%d, samplePoint + offsetA1));\n"
       "float sampleB1 = unpack_float(texture3D(textureUnit%d, samplePoint + offsetB1));\n"
       "sum += ((sampleA1 - sample) + (sampleB1 - sample)) * %4.16f;\n"
       "vec3 offsetA2 = %4.16f * vec3(0., -1., 0.);\n"
       "vec3 offsetB2 = %4.16f * vec3(0., +1., 0.);\n"
       "float sampleA2 = unpack_float(texture3D(textureUnit%d, samplePoint + offsetA2));\n"
       "float sampleB2 = unpack_float(texture3D(textureUnit%d, samplePoint + offsetB2));\n"
       "sum += ((sampleA2 - sample) + (sampleB2 - sample)) * %4.16f;\n"
       "vec3 offsetA3 = %4.16f * vec3(0., 0., -1.);\n"
       "vec3 offsetB3 = %4.16f * vec3(0., 0., +1.);\n"
       "float sampleA3 = unpack_float(texture3D(textureUnit%d, samplePoint + offsetA3));\n"
       "float sampleB3 = unpack_float(texture3D(textureUnit%d, samplePoint + offsetB3));\n"
       "sum += ((sampleA3 - sample) + (sampleB3 - sample)) * %4.16f;\n"
       "float smooth = sample + (%4.16f * sum / norm);\n"
       "gl_FragColor = pack_float(smooth);\n"
     "}\n";


  pnode->SetStatus(30);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Intensity-Driven Gradient Filter (GPU, OpenGL) Benchmark "<<endl;
  qDebug()<<"Setup Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << numElements << " | " << mtime << " | ";
  mtime = 0.;

  int nBuffer = 2000;
  char buffer [nBuffer];
  bool cancel = false;

  //starting the computing
  int iterations = pnode->GetAccuracy();
  if (!(iterations % 2))
    {
    iterations++;
    }

  for (int i = 1; i <= iterations; i++)
    {
    int status = pnode->GetStatus();

    if (status == -1)
      {
      cancel = true;
      }

    if(!cancel)
      {

      int textureUnit = 1 - (i%2);
      sprintf(buffer,
              fragmentShaderTemplate.c_str(),
              textureUnit,
              norm,
              spacingX,
              spacingX,
              textureUnit,
              textureUnit,
              pnode->GetParameterX(),
              spacingY,
              spacingY,
              textureUnit,
              textureUnit,
              pnode->GetParameterY(),
              spacingZ,
              spacingZ,
              textureUnit,
              textureUnit,
              pnode->GetParameterZ(),
              pnode->GetTimeStep());

      std::string shaders = header + buffer;
      this->Internal->shaderComputation->SetFragmentShaderSource(shaders.c_str());

      gettimeofday(&start, NULL);
      for (int slice = 0; slice < dims[2]; slice++)
        {
        if (!(textureUnit))
          {
          this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
          this->Internal->iterationVolumeTexture->Activate(0);
          this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
          }
        else
          {
          this->Internal->iterationVolumeTexture->AttachAsDrawTarget(0, slice);
          this->Internal->outputVolumeTexture->Activate(1);
          this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
          }
        }

      gettimeofday(&end, NULL);

      seconds  = end.tv_sec  - start.tv_sec;
      useconds = end.tv_usec - start.tv_usec;

      mtime += ((seconds) * 1000 + useconds/1000.0) + 0.5;
      }
    else
      {
      this->Internal->outputVolumeTexture->SetImageData(NULL);
      this->Internal->iterationVolumeTexture->SetImageData(NULL);
      pnode->SetStatus(0);
      return 0;
      }

    pnode->SetStatus((int) 30 + (i * 10 / pnode->GetAccuracy()));
    }

  qDebug()<<"Kernel Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";

  if (cancel)
    {
    fPixel = NULL;
    fPixelTemp = NULL;

    delete fPixel;
    delete fPixelTemp;

    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);

    this->Internal->tempVolumeData->Initialize();

    pnode->SetStatus(0);

    return 0;
    }

  // get the output
  gettimeofday(&start, NULL);

  this->Internal->outputVolumeTexture->ReadBack();

  pnode->SetStatus(60);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"ReadBack Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";

  gettimeofday(&start, NULL);
  fPixelTemp = static_cast<float*>(this->Internal->outputVolumeTexture->GetImageData()->GetScalarPointer(0,0,0));
  fPixel =  static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int i = elemCnt * 4;
    int i2 = i + 1;
    int i3 = i + 2;
    int i4 = i + 3;

    float4 value4;
    value4.x = *(fPixelTemp+i);
    value4.y = *(fPixelTemp+i2);
    value4.z = *(fPixelTemp+i3);
    value4.w = *(fPixelTemp+i4);
    float value = DecodeFloatRGBA(value4);
    *(fPixel+elemCnt) = value / scale;
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();
  double noiseMean = StringToDouble(outputVolume->GetAttribute("SlicerAstro.NOISEMEAN"));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) -= noiseMean;
    }

  pnode->SetStatus(80);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Setup Output Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime << " | ";

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  qDebug()<<"Update Time : "<<mtime<<" ms "<<endl;
  //outputFile << setprecision(5) << mtime <<endl;

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

  fPixel = NULL;
  fPixelTemp = NULL;
  delete fPixel;
  delete fPixelTemp;

  this->Internal->tempVolumeData->Initialize();

  pnode->SetStatus(0);

  return 1;
}
