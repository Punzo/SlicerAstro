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

// STD includes
#include <cassert>
#include <iostream>

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

  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int status = pnode->GetStatus();
    if(status == -1)
      {
      pnode->SetStatus(0);
      return 0;
      }
    if ((elemCnt + 1) > (int) (numElements * status / 33))
      {
      status++;
      pnode->SetStatus(status);
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

  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int status = pnode->GetStatus();
    if(status == -1)
      {
      pnode->SetStatus(0);
      return 0;
      }
    if ((elemCnt + numElements + 1) > (int) (numElements * status / 33))
      {
      status++;
      pnode->SetStatus(status);
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

  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int status = pnode->GetStatus();
    if(status == -1)
      {
      pnode->SetStatus(0);
      return 0;
      }
    if ((elemCnt + (numElements * 2) + 1) > (int) (numElements * status / 33))
      {
      status++;
      pnode->SetStatus(status);
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

  outputVolume->SetAttribute("SlicerAstro.DATAMAX", DoubleToString(max).c_str());
  outputVolume->SetAttribute("SlicerAstro.DATAMIN", DoubleToString(min).c_str());
  outputVolume->SetAttribute("SlicerAstro.NOISE", DoubleToString(noise).c_str());

  pnode->SetStatus(0);

  return 1;
}


//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::GradientCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *outPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
  float *inPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));


  double range[2];
  outputVolume->GetImageData()->GetScalarRange(range);
  cout<<"dentro1  : "<< range[0] <<" "<<range[1]<<endl;

  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int status = pnode->GetStatus();
    if(status == -1)
      {
      pnode->SetStatus(0);
      return 0;
      }
    if((elemCnt+1) > (int) (numElements * status / 100))
      {
      status++;
      pnode->SetStatus(status);
      }
    //here implement GradientCPUFilter
    *(outPixel+elemCnt) = *(inPixel+elemCnt) *10.;
    }

  //investigate how to update scalarrange in imagedata af having modified the pointers
  //outputVolume->GetImageData()->SetInformation(//here the info about range are chached);
  outputVolume->GetImageData()->GetScalarRange(range);
  cout<<"dentro2  : "<<range[0]<<" "<<range[1]<<endl;

  pnode->SetStatus(0);

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::WaveletLiftingCPUFilter(vtkMRMLSmoothingParametersNode* pnode)
{
  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  float *outPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
  float *inPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));

  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int status = pnode->GetStatus();
    if(status == -1)
      {
      pnode->SetStatus(0);
      return 0;
      }
    if((elemCnt+1) > (int) (numElements * status / 100))
      {
      status++;
      pnode->SetStatus(status);
      }
    //here implement WaveletLiftingCPUFilter
    *(outPixel+elemCnt) = *(inPixel+elemCnt);
    }
  pnode->SetStatus(0);

  return 1;
}
