
//MRML includes
#include "vtkMRMLAstroLabelMapVolumeDisplayNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLVolumeNode.h"
#include "vtkMRMLUnitNode.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkVersion.h>
#include <vtkStringArray.h>
#include <vtkMRMLColorNode.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
#include <cassert>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroLabelMapVolumeDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeDisplayNode::vtkMRMLAstroLabelMapVolumeDisplayNode()
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
vtkMRMLAstroLabelMapVolumeDisplayNode::~vtkMRMLAstroLabelMapVolumeDisplayNode()
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
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeDisplayNode::WriteXML(ostream& of, int nIndent)
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
void vtkMRMLAstroLabelMapVolumeDisplayNode::ReadXMLAttributes(const char** atts)
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
void vtkMRMLAstroLabelMapVolumeDisplayNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();

  Superclass::Copy(anode);
  vtkMRMLAstroLabelMapVolumeDisplayNode *node =vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast(anode);

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
int vtkMRMLAstroLabelMapVolumeDisplayNode::SetSpaceQuantity(int ind, const char *name)
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
std::string vtkMRMLAstroLabelMapVolumeDisplayNode::GetPixelString(double *ijk)
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
  std::string labelValue = "Unknown";
  int labelIndex = int(this->GetVolumeNode()->GetImageData()->GetScalarComponentAsDouble(ijk[0],ijk[1],ijk[2],0));
  vtkMRMLColorNode *colornode = this->GetColorNode();
  if(colornode)
    {
    labelValue = colornode->GetColorName(labelIndex);
    }
  labelValue += "(" + IntToString(labelIndex) + ")";

  return labelValue;
}

//----------------------------------------------------------------------------
const char *vtkMRMLAstroLabelMapVolumeDisplayNode::GetDisplayStringFromValue(const double world, vtkMRMLUnitNode *node)
{
  std::string value = "";
  if(node && !strcmp(node->GetAttribute("DisplayHint"), "FractionsAsArcMinutesArcSeconds"))
    {
    double fractpart, intpart, displayValue;
    std::string displayValueString;
    std::stringstream strstream;
    strstream.setf(ios::fixed,ios::floatfield);

    fractpart = modf(world, &intpart);
    displayValue = node->GetDisplayValueFromValue(intpart);
    value = DoubleToString(displayValue) + node->GetSuffix() + " ";

    fractpart = (modf(fractpart * 60., &intpart)) * 60.;
    displayValueString = DoubleToString(intpart);
    if(intpart < 10.)
      {
       displayValueString = " " + displayValueString;
      }
    value = value + displayValueString +"\x27 ";
    displayValueString = "";
    strstream.precision(node->GetPrecision());
    strstream << fractpart;
    strstream >> displayValueString;
    if(fractpart < 10.)
      {
      displayValueString = " " + displayValueString;
      }

    value = value + displayValueString + "\x22";
    }
  return value.c_str();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
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
