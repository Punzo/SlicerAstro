#include <string>

// MRML includes
#include "vtkMRMLAstroLabelMapVolumeNode.h"
#include "vtkMRMLAstroLabelMapVolumeDisplayNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLAstroVolumeStorageNode.h"

// VTK includes
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroLabelMapVolumeNode);

//----------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeNode::vtkMRMLAstroLabelMapVolumeNode()
{
}

//----------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeNode::~vtkMRMLAstroLabelMapVolumeNode()
{
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::ReadXMLAttributes(const char **atts)
{
  this->Superclass::ReadXMLAttributes(atts);

  this->WriteXML(std::cout,0);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::WriteXML(ostream &of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::Copy(vtkMRMLNode *anode)
{
  vtkMRMLAstroLabelMapVolumeNode *astroLabelMapVolumeNode = vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(anode);
  if (!astroLabelMapVolumeNode)
    {
    return;
    }

  this->Superclass::Copy(anode);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::CreateDefaultDisplayNodes()
{
  if (vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast(this->GetDisplayNode())!=NULL)
    {
    // display node already exists
    return;
    }
  if (this->GetScene()==NULL)
    {
    vtkErrorMacro("vtkMRMLAstroLabelMapVolumeNode::CreateDefaultDisplayNodes failed: scene is invalid");
    return;
    }
  vtkNew<vtkMRMLAstroLabelMapVolumeDisplayNode> dispNode;
  dispNode->SetAndObserveColorNodeID("vtkMRMLColorTableNodeLabels");
  this->GetScene()->AddNode(dispNode.GetPointer());
  this->SetAndObserveDisplayNodeID(dispNode->GetID());
}

//----------------------------------------------------------------------------
vtkMRMLStorageNode *vtkMRMLAstroLabelMapVolumeNode::CreateDefaultStorageNode()
{
  return vtkMRMLAstroVolumeStorageNode::New();
}

//----------------------------------------------------------------------------
vtkMRMLAstroLabelMapVolumeDisplayNode *vtkMRMLAstroLabelMapVolumeNode::GetAstroLabelMapVolumeDisplayNode()
{
  return vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast(this->GetDisplayNode());
}

