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
#include <vtkStringArray.h>
#include <vtkNew.h>

//------------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::vtkMRMLAstroVolumeDisplayNode()
{
  this->SpaceQuantities = vtkStringArray::New();
  this->SpaceQuantities->SetName("Tokens");
  this->SpaceQuantities->SetNumberOfValues(3);
  this->SpaceQuantities->SetValue(0, "length");
  this->SpaceQuantities->SetValue(1, "length");
  this->SpaceQuantities->SetValue(2, "velocity");
  this->Space = 0;
  this->SetSpace("WCS");
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::~vtkMRMLAstroVolumeDisplayNode()
{

  if (this->SpaceQuantities)
    {
    this->SpaceQuantities->Delete();
    }

  if (this->Space)
    {
    delete [] this->Space;
    }
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

//----------------------------------------------------------------------------
//std::string DoubleToString(double Value)
//{
//  return NumberToString<double>(Value);
//}

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  std::string quantities = "";
  if (this->SpaceQuantities)
    {
    for(int i = 0; i < this->SpaceQuantities->GetNumberOfValues(); i++)
      {
      quantities +=  this->SpaceQuantities->GetValue(i) + ";";
      }
    }

  of << indent << " SpaceQuantities=\"" << quantities << "\"";
  of << indent << " Space=\"" << (this->Space ? this->Space : "") << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);


  const char* attName;
  const char* attValue;

  while (*atts != NULL)
    {
    attName = *(atts++);
    attValue = *(atts++);

    if (!strcmp(attName, "SpaceQuantities"))
      {
      std::istringstream f(attValue);
      std::string s;
      int i = 0;
      while (std::getline(f, s, ';'))
        {
        this->SetSpaceQuantity(i, s.c_str());
        i++;
        }
      continue;
      }

    if (!strcmp(attName, "Space"))
      {
      this->SetSpace(attValue);
      continue;
      }
    }

  this->WriteXML(std::cout,0);

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, VolumeID
void vtkMRMLAstroVolumeDisplayNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();

  Superclass::Copy(anode);
  vtkMRMLAstroVolumeDisplayNode *node =vtkMRMLAstroVolumeDisplayNode::SafeDownCast(anode);

  if (node)
    {
#if VTK_MAJOR_VERSION > 5
    this->SetInputImageDataConnection(node->GetInputImageDataConnection());
#endif
    this->SetSpaceQuantities(node->GetSpaceQuantities());
    this->SetSpace(node->GetSpace());
    }

#if VTK_MAJOR_VERSION > 5
  this->UpdateImageDataPipeline();
#endif

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
int vtkMRMLAstroVolumeDisplayNode::SetSpaceQuantity(int ind, const char *name)
{

  if (ind >= this->SpaceQuantities->GetNumberOfValues())
    {
    this->SpaceQuantities->SetNumberOfValues(ind+1);
    }

  vtkStdString SpaceQuantities(name);
  if (this->SpaceQuantities->GetValue(ind) != SpaceQuantities)
    {
    this->SpaceQuantities->SetValue(ind, SpaceQuantities);
    return 1;
    }
  return 0;

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

  std::string quantities = "";
  if (this->SpaceQuantities)
    {
    for(int i = 0; i < this->SpaceQuantities->GetNumberOfValues(); i++)
      {
      quantities +=  this->SpaceQuantities->GetValue(i) + ";";
      }
    }

  os << indent << " SpaceQuantities=\"" << quantities << "\n";
  os << indent << "Space: " << (this->Space ? this->Space : "(none)") << "\n";

}
