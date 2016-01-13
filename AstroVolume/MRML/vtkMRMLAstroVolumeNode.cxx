#include <string>
#include <cstdlib>
#include <math.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkObjectFactory.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

// MRML includes
#include "vtkMRMLVolumeNode.h"
#include "vtkMRMLAstroVolumeNode.h"
#include "vtkMRMLAstroVolumeDisplayNode.h"
#include "vtkMRMLAstroVolumeStorageNode.h"

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode::vtkMRMLAstroVolumeNode()
{
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode::~vtkMRMLAstroVolumeNode()
{
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
void vtkMRMLAstroVolumeNode::ReadXMLAttributes(const char** atts)
{
  this->Superclass::ReadXMLAttributes(atts);

  this->WriteXML(std::cout,0);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::Copy(vtkMRMLNode *anode)
{
  vtkMRMLAstroVolumeNode *astroVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(anode);
  if (!astroVolumeNode)
    {
    return;
    }

  this->Superclass::Copy(anode);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//---------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode* vtkMRMLAstroVolumeNode::GetAstroVolumeDisplayNode()
{
  return vtkMRMLAstroVolumeDisplayNode::SafeDownCast(this->GetDisplayNode());
}

//---------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::UpdateRangeAttributes()
{
  this->GetImageData()->Modified();
  this->GetImageData()->GetPointData()->GetScalars()->Modified();
  double range[2];
  this->GetImageData()->GetScalarRange(range);
  this->SetAttribute("SlicerAstro.DATAMAX", DoubleToString(range[1]).c_str());
  this->SetAttribute("SlicerAstro.DATAMIN", DoubleToString(range[0]).c_str());
}

//---------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::UpdateNoiseAttribute()
{
  int *dims = this->GetImageData()->GetDimensions();
  float *outPixel = static_cast<float*> (this->GetImageData()->GetScalarPointer(0,0,0));
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
  this->SetAttribute("SlicerAstro.NOISE", DoubleToString(noise).c_str());
}

//---------------------------------------------------------------------------
vtkMRMLStorageNode* vtkMRMLAstroVolumeNode::CreateDefaultStorageNode()
{
  return vtkMRMLAstroVolumeStorageNode::New();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::CreateDefaultDisplayNodes()
{
  vtkMRMLAstroVolumeDisplayNode *displayNode = 
    vtkMRMLAstroVolumeDisplayNode::SafeDownCast(this->GetDisplayNode());
  if(displayNode == NULL)
  {
    displayNode = vtkMRMLAstroVolumeDisplayNode::New();
    if(this->GetScene())
    {
      displayNode->SetScene(this->GetScene());
      this->GetScene()->AddNode(displayNode);
      displayNode->SetDefaultColorMap();
      displayNode->Delete();

      this->SetAndObserveDisplayNodeID(displayNode->GetID());
      std::cout << "Display node set and observed" << std::endl;
    }
  }
}
