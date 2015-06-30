// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkObjectFactory.h>

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroVolumeNode.h>

// STD includes
#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

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


//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::ReadXMLAttributes(const char** atts)
{
  this->Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;
  while (*atts != NULL)
    {
    attName = *(atts++);
    attValue = *(atts++);
    //if (!strcmp(attName, ""))
      //{
      //this-> = attValue;
      //continue;
      //}
    }
  this->WriteXML(std::cout,1);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  //of << indent << " =\"" << this-> << "\"";
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

  //this-> = astroVolumeNode->;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  //os << ": " << this-> << std::endl;
}

vtkMRMLAstroVolumeDisplayNode* vtkMRMLAstroVolumeNode::GetAstroVolumeDisplayNode()
{
  return vtkMRMLAstroVolumeDisplayNode::SafeDownCast(this->GetDisplayNode());
}

vtkMRMLStorageNode* vtkMRMLAstroVolumeNode::CreateDefaultStorageNode()
{
  return vtkMRMLAstroVolumeStorageNode::New();
}

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
