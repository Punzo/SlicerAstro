// MRML includes
#include "vtkMRMLAstroVolumeDisplayNode.h"
#include "vtkMRMLSelectionNode.h"
#include "vtkMRMLUnitNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLVolumeNode.h"

// VTK includes
#include <vtkAlgorithmOutput.h>
#include <vtkImageAppendComponents.h>
#include <vtkImageData.h>
#include <vtkImageExtractComponents.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkImageThreshold.h>
#include <vtkObjectFactory.h>

//------------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::vtkMRMLAstroVolumeDisplayNode()
{
  this->SetSpaceQuantities("length;length;velocity");
  this->SetSpace("WCS");
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::~vtkMRMLAstroVolumeDisplayNode()
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
std::string IntToString(int Value)
{
    return NumberToString<int>(Value);
}
}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, VolumeID
void vtkMRMLAstroVolumeDisplayNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();

  Superclass::Copy(anode);

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetPixelString(double *ijk)
{
  if(this->GetVolumeNode()->GetImageData() == NULL)
    {
    return "No Image";
    }

  for(int i = 0; i < 3; i++)
    {
    if(ijk[i] < 0 or ijk[i] >=  this->GetVolumeNode()->GetImageData()->GetDimensions()[i])
      {
      return "Out of Frame";
      }
    }

  int numberOfComponents = this->GetVolumeNode()->GetImageData()->GetNumberOfScalarComponents();
  if(numberOfComponents > 3)
    {
    std::string s = IntToString(numberOfComponents) + " components";
    return s.c_str();
    }

  std::string pixel;

  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNthNodeByClass(0, "vtkMRMLSelectionNode"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode("intensity");

    for(int i = 0; i < numberOfComponents; i++)
      {
      double component = this->GetVolumeNode()->GetImageData()->GetScalarComponentAsDouble(ijk[0],ijk[1],ijk[2],i);
      pixel += unitNode->GetDisplayStringFromValue(component);
      pixel += ",";
      }
      pixel.erase(pixel.size()-1);
    }

  return pixel;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);
}
