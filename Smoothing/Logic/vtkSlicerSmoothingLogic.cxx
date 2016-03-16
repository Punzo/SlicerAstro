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
#include <vtkImageExtractComponents.h>
#include <vtkImageShiftScale.h>

// STD includes
#include <cassert>
#include <iostream>

// OpenMP includes
#include <omp.h>

// vtkOpenGL includes
#include <vtkOpenGLShaderComputation.h>
#include <vtkOpenGLTextureImage.h>

#include <iostream>
#include <sys/time.h>

#define SigmatoFWHM 2.3548200450309493

//----------------------------------------------------------------------------
class vtkSlicerSmoothingLogic::vtkInternal
{
public:
  vtkInternal();

  vtkSlicerAstroVolumeLogic* AstroVolumeLogic;
  vtkSmartPointer<vtkImageData> tempVolumeData;
  vtkSmartPointer<vtkOpenGLShaderComputation> shaderComputation;
  vtkSmartPointer<vtkOpenGLTextureImage> iterationVolumeTexture;
  vtkSmartPointer<vtkOpenGLTextureImage> outputVolumeTexture;
  vtkSmartPointer<vtkImageExtractComponents> extractComponents;
  vtkSmartPointer<vtkImageShiftScale> shiftScale;
};

//----------------------------------------------------------------------------
vtkSlicerSmoothingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = 0;
  tempVolumeData = vtkImageData::New();
  shaderComputation = vtkOpenGLShaderComputation::New();
  iterationVolumeTexture = vtkOpenGLTextureImage::New();
  outputVolumeTexture = vtkOpenGLTextureImage::New();
  extractComponents = vtkImageExtractComponents::New();
  shiftScale = vtkImageShiftScale::New();
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
    case 3:
      {
      success = this->HaarWaveletThresholdingCPUFilter(pnode);
      break;
      }
    case 4:
      {
      success = this->GallWaveletThresholdingCPUFilter(pnode);
      break;
      }
    }
  return success;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::AnisotropicBoxCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

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
  const int Zmax = (int) ((nItemsY - 1) / 2.);
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
  outputVolume->UpdateNoiseAttributes();
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
int vtkSlicerSmoothingLogic::IsotropicBoxCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

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

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;
  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::AnisotropicBoxGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

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
  all negative values will get clamped to zero.
  Also if the sample values aren't evenly spread through
  the zero-to-one space we may run into numerical issues.
  So rescale the data to the
  to fit in the full range of the a 16 bit short.
  (any vtkImageData scalar type should work with this approach,
  of course there will approximation error)*/

  this->Internal->shiftScale->SetInputData(outputVolume->GetImageData());
  this->Internal->shiftScale->SetOutputScalarTypeToFloat();
  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  this->Internal->shiftScale->SetShift(-range[0]);
  double scale = 1. / (range[1] - range[0]);
  this->Internal->shiftScale->SetScale(scale);
  this->Internal->shiftScale->Update();

  this->Internal->iterationVolumeTexture->SetImageData
      (vtkImageData::SafeDownCast(this->Internal->shiftScale->GetOutputDataObject(0)));

  this->Internal->outputVolumeTexture->SetImageData(outputVolume->GetImageData());
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(outputVolume->GetImageData());
  this->Internal->shaderComputation->AcquireResultRenderbuffer();

  // set the kernels
  std::string header = "#version 120\n";
  header += "vec3 transformPoint(const in vec3 samplePoint)\n";
  header += "  {\n";
  header += "  return samplePoint;\n";
  header += "  }\n";
  header += "uniform sampler3D\n";
  header += "textureUnit0,\n";
  header += "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120\n";
  vertexShaderTemplate +=  "attribute vec3 vertexAttribute;\n";
  vertexShaderTemplate +=  "attribute vec2 textureCoordinateAttribute;\n";
  vertexShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
  vertexShaderTemplate +=  "void main() ";
  vertexShaderTemplate +=  "  {\n";
  vertexShaderTemplate +=  "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5);\n";
  vertexShaderTemplate +=  "  gl_Position = vec4(vertexAttribute, 1.);\n";
  vertexShaderTemplate +=  "  }\n";

  this->Internal->shaderComputation->SetVertexShaderSource(vertexShaderTemplate.c_str());

  std::string fragmentShaderTemplate = "uniform float slice;\n";
  fragmentShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
  fragmentShaderTemplate +=  "void main()\n";
  fragmentShaderTemplate +=  "{\n";
  fragmentShaderTemplate +=    "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice);\n";
  fragmentShaderTemplate +=    "vec4 smooth = vec4(0.);\n";
  fragmentShaderTemplate +=    "vec4 sample = texture3D(textureUnit%d, samplePoint);\n";
  fragmentShaderTemplate +=    "for (int offsetX = -%d; offsetX <= %d; offsetX++){\n";
  fragmentShaderTemplate +=      "for (int offsetY = -%d; offsetY <= %d; offsetY++){\n";
  fragmentShaderTemplate +=        "for (int offsetZ = -%d; offsetZ <= %d; offsetZ++){\n";
  fragmentShaderTemplate +=          "vec3 offset1 = vec3(%4.16f * offsetX, %4.16f * offsetY, %4.16f * offsetZ);\n";
  fragmentShaderTemplate +=          "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1);\n";
  fragmentShaderTemplate +=          "smooth += sample1;\n";
  fragmentShaderTemplate +=        "}\n";
  fragmentShaderTemplate +=      "}\n";
  fragmentShaderTemplate +=    "}\n";
  fragmentShaderTemplate +=    "gl_FragColor = vec4(vec3(smooth / %d), 1.);\n";
  fragmentShaderTemplate +=  "}\n";

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
    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);
    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(100);

  // get the output
  this->Internal->outputVolumeTexture->ReadBack();
  outputVolume->SetAndObserveImageData(this->Internal->outputVolumeTexture->GetImageData());

  // porting back the range to the original one
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) /= scale;
    *(fPixel+elemCnt) += range[0];
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(0);

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::IsotropicBoxGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

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
  all negative values will get clamped to zero.
  Also if the sample values aren't evenly spread through
  the zero-to-one space we may run into numerical issues.
  So rescale the data to the
  to fit in the full range of the a 16 bit short.
  (any vtkImageData scalar type should work with this approach,
  of course there will approximation error)*/

  this->Internal->shiftScale->SetInputData(outputVolume->GetImageData());
  this->Internal->shiftScale->SetOutputScalarTypeToFloat();
  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  this->Internal->shiftScale->SetShift(-range[0]);
  double scale = 1. / (range[1] - range[0]);
  this->Internal->shiftScale->SetScale(scale);
  this->Internal->shiftScale->Update();

  this->Internal->iterationVolumeTexture->SetImageData
      (vtkImageData::SafeDownCast(this->Internal->shiftScale->GetOutputDataObject(0)));

  this->Internal->outputVolumeTexture->SetImageData(outputVolume->GetImageData());
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(outputVolume->GetImageData());
  this->Internal->shaderComputation->AcquireResultRenderbuffer();

  // set the kernels
  std::string header = "#version 120\n";
  header += "vec3 transformPoint(const in vec3 samplePoint)\n";
  header += "  {\n";
  header += "  return samplePoint;\n";
  header += "  }\n";
  header += "uniform sampler3D\n";
  header += "textureUnit0,\n";
  header += "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120\n";
  vertexShaderTemplate +=  "attribute vec3 vertexAttribute;\n";
  vertexShaderTemplate +=  "attribute vec2 textureCoordinateAttribute;\n";
  vertexShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
  vertexShaderTemplate +=  "void main() ";
  vertexShaderTemplate +=  "  {\n";
  vertexShaderTemplate +=  "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5);\n";
  vertexShaderTemplate +=  "  gl_Position = vec4(vertexAttribute, 1.);\n";
  vertexShaderTemplate +=  "  }\n";

  this->Internal->shaderComputation->SetVertexShaderSource(vertexShaderTemplate.c_str());

  std::string fragmentShaderTemplate = "uniform float slice;\n";
  fragmentShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
  fragmentShaderTemplate +=  "void main()\n";
  fragmentShaderTemplate +=  "{\n";
  fragmentShaderTemplate +=    "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice);\n";
  fragmentShaderTemplate +=    "vec4 smooth = vec4(0.);\n";
  fragmentShaderTemplate +=    "vec4 sample = texture3D(textureUnit%d, samplePoint);\n";
  fragmentShaderTemplate +=    "for (int offset = -%d; offset <= %d; offset++){\n";
  fragmentShaderTemplate +=      "vec3 offset1 = vec3(%4.16f * offset, %4.16f * offset, %4.16f * offset);\n";
  fragmentShaderTemplate +=      "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1);\n";
  fragmentShaderTemplate +=      "smooth += sample1;\n";
  fragmentShaderTemplate +=    "}\n";
  fragmentShaderTemplate +=    "gl_FragColor = vec4(vec3(smooth / %d), 1.);\n";
  fragmentShaderTemplate +=  "}\n";

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
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }
    }
  else
    {
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
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->iterationVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->outputVolumeTexture->Activate(1);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }
    }
  else
    {
    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);
    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(70);

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
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }
    }
  else
    {
    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);
    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(100);

  // get the output
  this->Internal->outputVolumeTexture->ReadBack();
  outputVolume->SetAndObserveImageData(this->Internal->outputVolumeTexture->GetImageData());

  // porting backthe range to the original one

  double shift = range[0];

  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) /= scale;
    *(fPixel+elemCnt) += shift;
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(0);

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;

  return 1;
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
  outputVolume->UpdateNoiseAttributes();
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

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;
  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::AnisotropicGaussianGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

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
  all negative values will get clamped to zero.
  Also if the sample values aren't evenly spread through
  the zero-to-one space we may run into numerical issues.
  So rescale the data to the
  to fit in the full range of the a 16 bit short.
  (any vtkImageData scalar type should work with this approach,
  of course there will approximation error)*/

  this->Internal->shiftScale->SetInputData(outputVolume->GetImageData());
  this->Internal->shiftScale->SetOutputScalarTypeToFloat();
  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  this->Internal->shiftScale->SetShift(-range[0]);
  double scale = 1. / (range[1] - range[0]);
  this->Internal->shiftScale->SetScale(scale);
  this->Internal->shiftScale->Update();

  this->Internal->iterationVolumeTexture->SetImageData
      (vtkImageData::SafeDownCast(this->Internal->shiftScale->GetOutputDataObject(0)));

  this->Internal->outputVolumeTexture->SetImageData(outputVolume->GetImageData());
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(outputVolume->GetImageData());
  this->Internal->shaderComputation->AcquireResultRenderbuffer();

  // set the kernels
  std::string header = "#version 120\n";
  header += "vec3 transformPoint(const in vec3 samplePoint)\n";
  header += "  {\n";
  header += "  return samplePoint;\n";
  header += "  }\n";
  header += "uniform sampler3D\n";
  header += "textureUnit0,\n";
  header += "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120\n";
  vertexShaderTemplate +=  "attribute vec3 vertexAttribute;\n";
  vertexShaderTemplate +=  "attribute vec2 textureCoordinateAttribute;\n";
  vertexShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
  vertexShaderTemplate +=  "void main() ";
  vertexShaderTemplate +=  "  {\n";
  vertexShaderTemplate +=  "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5);\n";
  vertexShaderTemplate +=  "  gl_Position = vec4(vertexAttribute, 1.);\n";
  vertexShaderTemplate +=  "  }\n";

  this->Internal->shaderComputation->SetVertexShaderSource(vertexShaderTemplate.c_str());

  std::string fragmentShaderTemplate = "uniform float slice;\n";
  fragmentShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
  fragmentShaderTemplate +=  "void main()\n";
  fragmentShaderTemplate +=  "{\n";
  fragmentShaderTemplate +=    "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice);\n";
  fragmentShaderTemplate +=    "vec4 smooth = vec4(0.);\n";
  fragmentShaderTemplate +=    "vec4 sum = vec4(0.);\n";
  fragmentShaderTemplate +=    "vec4 sample = texture3D(textureUnit%d, samplePoint);\n";
  fragmentShaderTemplate +=    "for (int offsetX = -%d; offsetX <= %d; offsetX++){\n";
  fragmentShaderTemplate +=      "for (int offsetY = -%d; offsetY <= %d; offsetY++){\n";
  fragmentShaderTemplate +=        "for (int offsetZ = -%d; offsetZ <= %d; offsetZ++){\n";
  fragmentShaderTemplate +=          "vec3 offset1 = vec3(%4.16f * offsetX, %4.16f * offsetY, %4.16f * offsetZ);\n";
  fragmentShaderTemplate +=          "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1);\n";
  fragmentShaderTemplate +=          "float expA = offsetX * offsetX / %4.16f;\n";
  fragmentShaderTemplate +=          "float expB = offsetY * offsetY / %4.16f;\n";
  fragmentShaderTemplate +=          "float expC = offsetZ * offsetZ / %4.16f;\n";
  fragmentShaderTemplate +=          "float kernel = exp(-(expA + expB + expC));\n";
  fragmentShaderTemplate +=          "smooth += sample1 * kernel;\n";
  fragmentShaderTemplate +=          "sum += kernel;\n";
  fragmentShaderTemplate +=        "}\n";
  fragmentShaderTemplate +=      "}\n";
  fragmentShaderTemplate +=    "}\n";
  fragmentShaderTemplate +=    "gl_FragColor = vec4(vec3(smooth / sum), 1.);\n";
  fragmentShaderTemplate +=  "}\n";

  std::string fragmentShaderTemplateWithRotation = "uniform float slice;\n";
  fragmentShaderTemplateWithRotation +=  "varying vec3 interpolatedTextureCoordinate;\n";
  fragmentShaderTemplateWithRotation +=  "void main()\n";
  fragmentShaderTemplateWithRotation +=  "{\n";
  fragmentShaderTemplateWithRotation +=    "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice);\n";
  fragmentShaderTemplateWithRotation +=    "vec4 smooth = vec4(0.);\n";
  fragmentShaderTemplateWithRotation +=    "vec4 sum = vec4(0.);\n";
  fragmentShaderTemplateWithRotation +=    "vec4 sample = texture3D(textureUnit%d, samplePoint);\n";
  fragmentShaderTemplateWithRotation +=    "for (int offsetX = -%d; offsetX <= %d; offsetX++){\n";
  fragmentShaderTemplateWithRotation +=      "for (int offsetY = -%d; offsetY <= %d; offsetY++){\n";
  fragmentShaderTemplateWithRotation +=        "for (int offsetZ = -%d; offsetZ <= %d; offsetZ++){\n";
  fragmentShaderTemplateWithRotation +=          "float x = offsetX * %4.16f * %4.16f - offsetY * %4.16f * %4.16f + offsetZ * %4.16f;\n";
  fragmentShaderTemplateWithRotation +=          "float y = offsetX * (%4.16f * %4.16f * %4.16f + %4.16f * %4.16f) + "
                                                 "offsetY * (%4.16f * %4.16f - %4.16f * %4.16f * %4.16f) - offsetZ * %4.16f * %4.16f;\n";
  fragmentShaderTemplateWithRotation +=          "float z = offsetX * (-%4.16f * %4.16f * %4.16f + %4.16f * %4.16f) + "
                                                 "offsetY * (%4.16f * %4.16f + %4.16f * %4.16f * %4.16f) + offsetZ * %4.16f * %4.16f;\n";
  fragmentShaderTemplateWithRotation +=          "vec3 offset1 = vec3(%4.16f * x, %4.16f * y, %4.16f * z);\n";
  fragmentShaderTemplateWithRotation +=          "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1);\n";
  fragmentShaderTemplateWithRotation +=          "float expA = x * x / %4.16f;\n";
  fragmentShaderTemplateWithRotation +=          "float expB = y * y / %4.16f;\n";
  fragmentShaderTemplateWithRotation +=          "float expC = z * z / %4.16f;\n";
  fragmentShaderTemplateWithRotation +=          "float kernel = exp(-(expA + expB + expC));\n";
  fragmentShaderTemplateWithRotation +=          "smooth += sample1 * kernel;\n";
  fragmentShaderTemplateWithRotation +=          "sum += kernel;\n";
  fragmentShaderTemplateWithRotation +=        "}\n";
  fragmentShaderTemplateWithRotation +=      "}\n";
  fragmentShaderTemplateWithRotation +=    "}\n";
  fragmentShaderTemplateWithRotation +=    "gl_FragColor = vec4(vec3(smooth / sum), 1.);\n";
  fragmentShaderTemplateWithRotation +=  "}\n";

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
    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);
    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(100);

  // get the output
  this->Internal->outputVolumeTexture->ReadBack();
  outputVolume->SetAndObserveImageData(this->Internal->outputVolumeTexture->GetImageData());

  // porting back the range to the original one
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) /= scale;
    *(fPixel+elemCnt) += range[0];
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(0);

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::IsotropicGaussianGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

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
  all negative values will get clamped to zero.
  Also if the sample values aren't evenly spread through
  the zero-to-one space we may run into numerical issues.
  So rescale the data to the
  to fit in the full range of the a 16 bit short.
  (any vtkImageData scalar type should work with this approach,
  of course there will approximation error)*/

  this->Internal->shiftScale->SetInputData(outputVolume->GetImageData());
  this->Internal->shiftScale->SetOutputScalarTypeToFloat();
  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  this->Internal->shiftScale->SetShift(-range[0]);
  double scale = 1. / (range[1] - range[0]);
  this->Internal->shiftScale->SetScale(scale);
  this->Internal->shiftScale->Update();

  this->Internal->iterationVolumeTexture->SetImageData
      (vtkImageData::SafeDownCast(this->Internal->shiftScale->GetOutputDataObject(0)));

  this->Internal->outputVolumeTexture->SetImageData(outputVolume->GetImageData());
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(outputVolume->GetImageData());
  this->Internal->shaderComputation->AcquireResultRenderbuffer();

  // set the kernels
  std::string header = "#version 120\n";
  header += "vec3 transformPoint(const in vec3 samplePoint)\n";
  header += "  {\n";
  header += "  return samplePoint;\n";
  header += "  }\n";
  header += "uniform sampler3D\n";
  header += "textureUnit0,\n";
  header += "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120\n";
  vertexShaderTemplate +=  "attribute vec3 vertexAttribute;\n";
  vertexShaderTemplate +=  "attribute vec2 textureCoordinateAttribute;\n";
  vertexShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
  vertexShaderTemplate +=  "void main() ";
  vertexShaderTemplate +=  "  {\n";
  vertexShaderTemplate +=  "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5);\n";
  vertexShaderTemplate +=  "  gl_Position = vec4(vertexAttribute, 1.);\n";
  vertexShaderTemplate +=  "  }\n";

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

    std::string fragmentShaderTemplate = "uniform float slice;\n";
    fragmentShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
    fragmentShaderTemplate +=  "void main()\n";
    fragmentShaderTemplate +=  "{\n";
    fragmentShaderTemplate +=    "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice);\n";
    fragmentShaderTemplate +=    "vec4 smooth = vec4(0.);\n";
    fragmentShaderTemplate +=    "vec4 sum = vec4(0.);\n";
    fragmentShaderTemplate +=    "vec4 sample = texture3D(textureUnit%d, samplePoint);\n";
    fragmentShaderTemplate +=    "for (int offsetX = -%d; offsetX <= %d; offsetX++){\n";
    fragmentShaderTemplate +=      "vec3 offset1 = vec3(%4.16f * offsetX, 0., 0.);\n";
    fragmentShaderTemplate +=      "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1);\n";
    fragmentShaderTemplate +=      "float kernel = exp(-(offsetX * offsetX) / %4.16f);\n";
    fragmentShaderTemplate +=      "smooth += sample1 * kernel;\n";
    fragmentShaderTemplate +=      "sum += kernel;\n";
    fragmentShaderTemplate +=    "}\n";
    fragmentShaderTemplate +=    "gl_FragColor = vec4(vec3(smooth / sum), 1.);\n";
    fragmentShaderTemplate +=  "}\n";

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
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }
    }
  else
    {
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

    std::string fragmentShaderTemplate = "uniform float slice;\n";
    fragmentShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
    fragmentShaderTemplate +=  "void main()\n";
    fragmentShaderTemplate +=  "{\n";
    fragmentShaderTemplate +=    "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice);\n";
    fragmentShaderTemplate +=    "vec4 smooth = vec4(0.);\n";
    fragmentShaderTemplate +=    "vec4 sum = vec4(0.);\n";
    fragmentShaderTemplate +=    "vec4 sample = texture3D(textureUnit%d, samplePoint);\n";
    fragmentShaderTemplate +=    "for (int offsetY = -%d; offsetY <= %d; offsetY++){\n";
    fragmentShaderTemplate +=      "vec3 offset1 = vec3(0., %4.16f * offsetY, 0.);\n";
    fragmentShaderTemplate +=      "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1);\n";
    fragmentShaderTemplate +=      "float kernel = exp(-(offsetY * offsetY) / %4.16f);\n";
    fragmentShaderTemplate +=      "smooth += sample1 * kernel;\n";
    fragmentShaderTemplate +=      "sum += kernel;\n";
    fragmentShaderTemplate +=    "}\n";
    fragmentShaderTemplate +=    "gl_FragColor = vec4(vec3(smooth / sum), 1.);\n";
    fragmentShaderTemplate +=  "}\n";

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
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->iterationVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->outputVolumeTexture->Activate(1);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }
    }
  else
    {
    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);
    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(70);

  status = pnode->GetStatus();

  if (status == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {

    std::string fragmentShaderTemplate = "uniform float slice;\n";
    fragmentShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
    fragmentShaderTemplate +=  "void main()\n";
    fragmentShaderTemplate +=  "{\n";
    fragmentShaderTemplate +=    "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice);\n";
    fragmentShaderTemplate +=    "vec4 smooth = vec4(0.);\n";
    fragmentShaderTemplate +=    "vec4 sum = vec4(0.);\n";
    fragmentShaderTemplate +=    "vec4 sample = texture3D(textureUnit%d, samplePoint);\n";
    fragmentShaderTemplate +=    "for (int offsetZ = -%d; offsetZ <= %d; offsetZ++){\n";
    fragmentShaderTemplate +=      "vec3 offset1 = vec3(0., 0., %4.16f * offsetZ);\n";
    fragmentShaderTemplate +=      "vec4 sample1 = texture3D(textureUnit%d, samplePoint + offset1);\n";
    fragmentShaderTemplate +=      "float kernel = exp(-(offsetZ * offsetZ) / %4.16f);\n";
    fragmentShaderTemplate +=      "smooth += sample1 * kernel;\n";
    fragmentShaderTemplate +=      "sum += kernel;\n";
    fragmentShaderTemplate +=    "}\n";
    fragmentShaderTemplate +=    "gl_FragColor = vec4(vec3(smooth / sum), 1.);\n";
    fragmentShaderTemplate +=  "}\n";

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
    for (int slice = 0; slice < dims[2]; slice++)
      {
      this->Internal->outputVolumeTexture->AttachAsDrawTarget(0, slice);
      this->Internal->iterationVolumeTexture->Activate(0);
      this->Internal->shaderComputation->Compute((slice + 0.5) / (1. * (dims[2])));
      }
    }
  else
    {
    this->Internal->outputVolumeTexture->SetImageData(NULL);
    this->Internal->iterationVolumeTexture->SetImageData(NULL);
    pnode->SetStatus(0);
    return 0;
    }

  pnode->SetStatus(100);

  // get the output
  this->Internal->outputVolumeTexture->ReadBack();
  outputVolume->SetAndObserveImageData(this->Internal->outputVolumeTexture->GetImageData());

  // porting backthe range to the original one

  double shift = range[0];

  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) /= scale;
    *(fPixel+elemCnt) += shift;
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(0);

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

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

    pnode->SetStatus((int) i * 100 / pnode->GetAccuracy());

    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);
    outputVolume->UpdateRangeAttributes();
    outputVolume->UpdateNoiseAttributes();

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
int vtkSlicerSmoothingLogic::GradientGPUFilter(vtkMRMLSmoothingParametersNode *pnode)
{
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

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
  /* since the OpenGL texture will be floats in the range 0 to 1,
  all negative values will get clamped to zero.
  Also if the sample values aren't evenly spread through
  the zero-to-one space we may run into numerical issues.
  So rescale the data to the
  to fit in the full range of the a 16 bit short.
  (any vtkImageData scalar type should work with this approach,
  of course there will approximation error)*/

  this->Internal->shiftScale->SetInputData(outputVolume->GetImageData());
  this->Internal->shiftScale->SetOutputScalarTypeToFloat();
  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  this->Internal->shiftScale->SetShift(-range[0]);
  double scale = 1. / (range[1] - range[0]);
  this->Internal->shiftScale->SetScale(scale);
  this->Internal->shiftScale->Update();
  this->Internal->iterationVolumeTexture->SetImageData
      (vtkImageData::SafeDownCast(this->Internal->shiftScale->GetOutputDataObject(0)));
  this->Internal->outputVolumeTexture->SetImageData(outputVolume->GetImageData());
  this->Internal->iterationVolumeTexture->Activate(0);
  this->Internal->outputVolumeTexture->Activate(1);
  this->Internal->shaderComputation->SetResultImageData(outputVolume->GetImageData());
  this->Internal->shaderComputation->AcquireResultRenderbuffer();


  //set Intensity-Driven normalization
  double noise = StringToDouble(outputVolume->GetAttribute("SlicerAstro.NOISE"));
  noise *= scale;
  double norm = noise * noise * pnode->GetK() * pnode->GetK();

  // set the kernels
  std::string header = "#version 120\n";
  header += "vec3 transformPoint(const in vec3 samplePoint)\n";
  header += "  {\n";
  header += "  return samplePoint;\n";
  header += "  }\n";
  header += "uniform sampler3D\n";
  header += "textureUnit0,\n";
  header += "textureUnit1;\n";

  std::string vertexShaderTemplate = "#version 120\n";
  vertexShaderTemplate +=  "attribute vec3 vertexAttribute;\n";
  vertexShaderTemplate +=  "attribute vec2 textureCoordinateAttribute;\n";
  vertexShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
  vertexShaderTemplate +=  "void main() ";
  vertexShaderTemplate +=  "  {\n";
  vertexShaderTemplate +=  "  interpolatedTextureCoordinate = vec3(textureCoordinateAttribute, .5);\n";
  vertexShaderTemplate +=  "  gl_Position = vec4(vertexAttribute, 1.);\n";
  vertexShaderTemplate +=  "  }\n";

  this->Internal->shaderComputation->SetVertexShaderSource(vertexShaderTemplate.c_str());

  std::string fragmentShaderTemplate = "uniform float slice;\n";
  fragmentShaderTemplate +=  "varying vec3 interpolatedTextureCoordinate;\n";
  fragmentShaderTemplate +=  "void main()\n";
  fragmentShaderTemplate +=  "{\n";
  fragmentShaderTemplate +=    "vec3 samplePoint = vec3(interpolatedTextureCoordinate.xy,slice);\n";
  fragmentShaderTemplate +=    "vec4 sum = vec4(0.);\n";
  fragmentShaderTemplate +=    "vec4 sample = texture3D(textureUnit%d, samplePoint);\n";
  fragmentShaderTemplate +=    "vec4 sample2 = sample * sample;\n";
  fragmentShaderTemplate +=    "vec4 norm = 1. + (sample2 / %4.16f);\n";
  fragmentShaderTemplate +=    "vec3 offsetA1 = %4.16f * vec3(-1., 0., 0.);\n";
  fragmentShaderTemplate +=    "vec3 offsetB1 = %4.16f * vec3(+1., 0., 0.);\n";
  fragmentShaderTemplate +=    "vec4 sampleA1 = texture3D(textureUnit%d, samplePoint + offsetA1);\n";
  fragmentShaderTemplate +=    "vec4 sampleB1 = texture3D(textureUnit%d, samplePoint + offsetB1);\n";
  fragmentShaderTemplate +=    "sum += ((sampleA1 - sample) + (sampleB1 - sample)) * %4.16f;\n";
  fragmentShaderTemplate +=    "vec3 offsetA2 = %4.16f * vec3(0., -1., 0.);\n";
  fragmentShaderTemplate +=    "vec3 offsetB2 = %4.16f * vec3(0., +1., 0.);\n";
  fragmentShaderTemplate +=    "vec4 sampleA2 = texture3D(textureUnit%d, samplePoint + offsetA2);\n";
  fragmentShaderTemplate +=    "vec4 sampleB2 = texture3D(textureUnit%d, samplePoint + offsetB2);\n";
  fragmentShaderTemplate +=    "sum += ((sampleA2 - sample) + (sampleB2 - sample)) * %4.16f;\n";
  fragmentShaderTemplate +=    "vec3 offsetA3 = %4.16f * vec3(0., 0., -1.);\n";
  fragmentShaderTemplate +=    "vec3 offsetB3 = %4.16f * vec3(0., 0., +1.);\n";
  fragmentShaderTemplate +=    "vec4 sampleA3 = texture3D(textureUnit%d, samplePoint + offsetA3);\n";
  fragmentShaderTemplate +=    "vec4 sampleB3 = texture3D(textureUnit%d, samplePoint + offsetB3);\n";
  fragmentShaderTemplate +=    "sum += ((sampleA3 - sample) + (sampleB3 - sample)) * %4.16f;\n";
  fragmentShaderTemplate +=    "vec4 smooth = sample + (%4.16f * sum / norm);\n";
  fragmentShaderTemplate +=    "gl_FragColor = vec4(vec3(smooth), 1.);\n";
  fragmentShaderTemplate +=  "}\n";

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
      }
    else
      {
      this->Internal->outputVolumeTexture->SetImageData(NULL);
      this->Internal->iterationVolumeTexture->SetImageData(NULL);
      pnode->SetStatus(0);
      return 0;
      }

    pnode->SetStatus((int) i * 100 / pnode->GetAccuracy());
    }

  // get the output
  this->Internal->outputVolumeTexture->ReadBack();
  outputVolume->SetAndObserveImageData(this->Internal->outputVolumeTexture->GetImageData());

  // porting backthe range to the original one

  double shift = range[0];
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *fPixel;
  fPixel = static_cast<float*>(outputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(fPixel+elemCnt) /= scale;
    *(fPixel+elemCnt) += shift;
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(0);

  this->Internal->outputVolumeTexture->SetImageData(NULL);
  this->Internal->iterationVolumeTexture->SetImageData(NULL);

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
    pnode->SetStatus((int) l * 50 / pnode->GetAccuracy());

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
  int m = pnode->GetAccuracy();
  for (int l = pnode->GetAccuracy(); l >= 1; l--)
    {
    if (pnode->GetStatus() == -1)
      {
      break;
      }
    m++;
    pnode->SetStatus((int) m * 50 / pnode->GetAccuracy());

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
  outputVolume->UpdateNoiseAttributes();
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
    pnode->SetStatus((int) l * 50 / pnode->GetAccuracy());

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
  int m = pnode->GetAccuracy();
  for (int l = pnode->GetAccuracy(); l >= 1; l--)
    {
    if (pnode->GetStatus() == -1)
      {
      break;
      }
    m++;
    pnode->SetStatus((int) m * 50 / pnode->GetAccuracy());

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
          float tempA, tempA1, tempA2, tempA3;
          float tempB, tempB1, tempB2, tempB3;
          float tempC, tempC1, tempC2, tempC3;
          tempA = *(tempFPixel + a);
          tempA1 = *(tempFPixel + a1);
          tempA2 = *(tempFPixel + a2);
          tempA3 = *(tempFPixel + a3);
          tempB = *(tempFPixel + b);
          tempB1 = *(tempFPixel + b1);
          tempB2 = *(tempFPixel + b2);
          tempB3 = *(tempFPixel + b3);
          tempC = *(tempFPixel + c);
          tempC1 = *(tempFPixel + c1);
          tempC2 = *(tempFPixel + c2);
          tempC3 = *(tempFPixel + c3);
          if (fabs(tempA) < delta)
            {
            tempA = 0.;
            }
          if (fabs(tempB) < delta)
            {
            tempB = 0.;
            }
          if (fabs(tempC) < delta)
            {
            tempC = 0.;
            }
          if (fabs(tempA1) < delta)
            {
            tempA1 = 0.;
            }
          if (fabs(tempB1) < delta)
            {
            tempB1 = 0.;
            }
          if (fabs(tempC1) < delta)
            {
            tempC1 = 0.;
            }
          if (fabs(tempA2) < delta)
            {
            tempA2 = 0.;
            }
          if (fabs(tempB2) < delta)
            {
            tempB2 = 0.;
            }
          if (fabs(tempC2) < delta)
            {
            tempC2 = 0.;
            }
          if (fabs(tempA3) < delta)
            {
            tempA3 = 0.;
            }
          if (fabs(tempB3) < delta)
            {
            tempB3 = 0.;
            }
          if (fabs(tempC3) < delta)
            {
            tempC3 = 0.;
            }
          if (fabs(*(outFPixel + elemCnt)) < delta)
            {
            *(outFPixel + elemCnt) = 0.;
            }
          if (fabs(*(outFPixel + e1)) < delta)
            {
            *(outFPixel + e1) = 0.;
            }
          if (fabs(*(outFPixel + e2)) < delta)
            {
            *(outFPixel + e2) = 0.;
            }
          if (fabs(*(outFPixel + e3)) < delta)
            {
            *(outFPixel + e3) = 0.;
            }
          // Update
          *(outFPixel + elemCnt) -= 0.25 * (tempC + tempA);
          // Predict
          *(outFPixel + a) += 0.5 * (*(outFPixel + elemCnt) + tempB);
          // Update
          *(outFPixel + e1) -= 0.25 * (tempC1 + tempA1);
          // Predict
          *(outFPixel + a1) += 0.5 * (*(outFPixel + e1) + tempB1);
          // Update
          *(outFPixel + e2) -= 0.25 * (tempC2 + tempA2);
          // Predict
          *(outFPixel + a2) += 0.5 * (*(outFPixel + e2) + tempB2);
          // Update
          *(outFPixel + e3) -= 0.25 * (tempC3 + tempA3);
          // Predict
          *(outFPixel + a3) += 0.5 * (*(outFPixel + e3) + tempB3);
          break;
        case VTK_DOUBLE:
          // Thresholding
          double tempDA, tempDA1, tempDA2, tempDA3;
          double tempDB, tempDB1, tempDB2, tempDB3;
          double tempDC, tempDC1, tempDC2, tempDC3;
          tempDA = *(tempDPixel + a);
          tempDA1 = *(tempDPixel + a1);
          tempDA2 = *(tempDPixel + a2);
          tempDA3 = *(tempDPixel + a3);
          tempDB = *(tempDPixel + b);
          tempDB1 = *(tempDPixel + b1);
          tempDB2 = *(tempDPixel + b2);
          tempDB3 = *(tempDPixel + b3);
          tempDC = *(tempDPixel + c);
          tempDC1 = *(tempDPixel + c1);
          tempDC2 = *(tempDPixel + c2);
          tempDC3 = *(tempDPixel + c3);
          if (fabs(tempDA) < delta)
            {
            tempDA = 0.;
            }
          if (fabs(tempDB) < delta)
            {
            tempDB = 0.;
            }
          if (fabs(tempDC) < delta)
            {
            tempDC = 0.;
            }
          if (fabs(tempDA1) < delta)
            {
            tempDA1 = 0.;
            }
          if (fabs(tempDB1) < delta)
            {
            tempDB1 = 0.;
            }
          if (fabs(tempDC1) < delta)
            {
            tempDC1 = 0.;
            }
          if (fabs(tempDA2) < delta)
            {
            tempDA2 = 0.;
            }
          if (fabs(tempDB2) < delta)
            {
            tempDB2 = 0.;
            }
          if (fabs(tempDC2) < delta)
            {
            tempDC2 = 0.;
            }
          if (fabs(tempDA3) < delta)
            {
            tempDA3 = 0.;
            }
          if (fabs(tempDB3) < delta)
            {
            tempDB3 = 0.;
            }
          if (fabs(tempDC3) < delta)
            {
            tempDC3 = 0.;
            }
          if (fabs(*(outDPixel + elemCnt)) < delta)
            {
            *(outDPixel + elemCnt) = 0.;
            }
          if (fabs(*(outDPixel + e1)) < delta)
            {
            *(outDPixel + e1) = 0.;
            }
          if (fabs(*(outDPixel + e2)) < delta)
            {
            *(outDPixel + e2) = 0.;
            }
          if (fabs(*(outDPixel + e3)) < delta)
            {
            *(outDPixel + e3) = 0.;
            }
          // Update
          *(outDPixel + elemCnt) -= 0.25 * (tempDC + tempDA);
          // Predict
          *(outDPixel + a) += 0.5 * (*(outDPixel + elemCnt) + tempDB);
          // Update
          *(outDPixel + e1) -= 0.25 * (tempDC1 + tempDA1);
          // Predict
          *(outDPixel + a1) += 0.5 * (*(outDPixel + e1) + tempDB1);
          // Update
          *(outDPixel + e2) -= 0.25 * (tempDC2 + tempDA2);
          // Predict
          *(outDPixel + a2) += 0.5 * (*(outDPixel + e2) + tempDB2);
          // Update
          *(outDPixel + e3) -= 0.25 * (tempDC3 + tempDA3);
          // Predict
          *(outDPixel + a3) += 0.5 * (*(outDPixel + e3) + tempDB3);
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
          float tempA, tempResA;
          float tempB, tempResB;
          float tempC, tempResC;
          tempA = *(tempFPixel + a);
          tempResA = *(tempFPixel + resa);
          tempB = *(tempFPixel + b);
          tempResB = *(tempFPixel + resb);
          tempC = *(tempFPixel + c);
          tempResC = *(tempFPixel + resc);
          if (fabs(tempA) < delta)
            {
            tempA = 0.;
            }
          if (fabs(tempC) < delta)
            {
            tempC = 0.;
            }
          if (fabs(tempB) < delta)
            {
            tempB = 0.;
            }
          if (fabs(tempResA) < delta)
            {
            tempResA = 0.;
            }
          if (fabs(tempResB) < delta)
            {
            tempResB = 0.;
            }
          if (fabs(tempResC) < delta)
            {
            tempResC = 0.;
            }
          if (fabs(*(outFPixel + elemCnt)) < delta)
            {
            *(outFPixel + elemCnt) = 0.;
            }
          if (fabs(*(outFPixel + res)) < delta)
            {
            *(outFPixel + res) = 0.;
            }
          // Update
          *(outFPixel + elemCnt) -= 0.25 * (tempC + tempA);
          // Predict
          *(outFPixel + a) += 0.5 * (*(outFPixel + elemCnt) + tempB);
          // Update residuals
          *(outFPixel + res) -= 0.25 * (tempResC + tempResA);
          // Predict residuals
          *(outFPixel + resa) += 0.5 * (*(outFPixel + res) + tempResB);
          break;
        case VTK_DOUBLE:
          // Thresholding
          double tempDA, tempResDA;
          double tempDB, tempResDB;
          double tempDC, tempResDC;
          tempDA = *(tempDPixel + a);
          tempResDA = *(tempDPixel + resa);
          tempDB = *(tempDPixel + b);
          tempResDB = *(tempDPixel + resb);
          tempDC = *(tempDPixel + c);
          tempResDC = *(tempDPixel + resc);
          if (fabs(tempDA) < delta)
            {
            tempDA = 0.;
            }
          if (fabs(tempDC) < delta)
            {
            tempDC = 0.;
            }
          if (fabs(tempDB) < delta)
            {
            tempDB = 0.;
            }
          if (fabs(tempResDA) < delta)
            {
            tempResDA = 0.;
            }
          if (fabs(tempResDB) < delta)
            {
            tempResDB = 0.;
            }
          if (fabs(tempResDC) < delta)
            {
            tempResDC = 0.;
            }
          if (fabs(*(outDPixel + elemCnt)) < delta)
            {
            *(outDPixel + elemCnt) = 0.;
            }
          if (fabs(*(outDPixel + res)) < delta)
            {
            *(outDPixel + res) = 0.;
            }
          // Update
          *(outDPixel + elemCnt) -= 0.25 * (tempDC + tempDA);
          // Predict
          *(outDPixel + a) += 0.5 * (*(outDPixel + elemCnt) + tempDB);
          // Update residuals
          *(outDPixel + res) -= 0.25 * (tempResDC + tempResDA);
          // Predict residuals
          *(outDPixel + resa) += 0.5 * (*(outDPixel + res) + tempResDB);
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
          float tempA, tempB, tempC;
          tempA = *(tempFPixel + a);
          tempB = *(tempFPixel + b);
          tempC = *(tempFPixel + c);
          if (fabs(tempA) < delta)
            {
            tempA = 0.;
            }
          if (fabs(tempB) < delta)
            {
            tempB = 0.;
            }
          if (fabs(tempC) < delta)
            {
            tempC = 0.;
            }
          if (fabs(*(outFPixel + elemCnt)) < delta)
            {
            *(outFPixel + elemCnt) = 0.;
            }
          // Update
          *(outFPixel + elemCnt) -= 0.25 * (tempC + tempA);
          // Predict
          *(outFPixel + a) += 0.5 * (*(outFPixel + elemCnt) + tempB);
          break;
        case VTK_DOUBLE:
          // Thresholding
          double tempDA, tempDB, tempDC;
          tempDA = *(tempDPixel + a);
          tempDB = *(tempDPixel + b);
          tempDC = *(tempDPixel + c);
          if (fabs(tempDA) < delta)
            {
            tempDA = 0.;
            }
          if (fabs(tempDB) < delta)
            {
            tempDB = 0.;
            }
          if (fabs(tempDC) < delta)
            {
            tempDC = 0.;
            }
          if (fabs(*(outDPixel + elemCnt)) < delta)
            {
            *(outDPixel + elemCnt) = 0.;
            }
          // Update
          *(outDPixel + elemCnt) -= 0.25 * (tempDC + tempDA);
          // Predict
          *(outDPixel + a) += 0.5 * (*(outDPixel + elemCnt) + tempDB);
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
  outputVolume->UpdateNoiseAttributes();
  pnode->SetStatus(0);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  cout<<"tempo : "<<mtime<<endl;

  return 1;
}

