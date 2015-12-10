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
      success = this->GaussianFilter(pnode);
      break;
      }
    case 1:
      {
      break;
      }
    case 2:
      {
      break;
      }
    }
  return success;
}

//----------------------------------------------------------------------------
int vtkSlicerSmoothingLogic::GaussianFilter(vtkMRMLSmoothingParametersNode* pnode)
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
  int cont = 0;

  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int status = pnode->GetStatus();
    if(status == -1)
      {
      pnode->SetStatus(0);
      return 0;
      }
    if((elemCnt+1) > (int) (numElements * status/ 100))
      {
      status++;
      pnode->SetStatus(status);
      }
      cont++;

    //int i1, i2, ext;
    //i1 = i - (int) (pnode->GetGaussianKernelX()->GetDataSize() - 1 / 2.);
    //i2 = i + (int) (pnode->GetGaussianKernelX()->GetDataSize() - 1 / 2.);
    //*(outPixel+elemCnt) = *(inPixel+elemCnt) * pnode->GetParameterX();
    }
  pnode->SetStatus(0);

  return 1;
}

