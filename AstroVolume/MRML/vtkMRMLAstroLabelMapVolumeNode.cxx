/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

#include <string>

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkVolume.h>

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
}//end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::ReadXMLAttributes(const char** atts)
{
  this->Superclass::ReadXMLAttributes(atts);

  this->WriteXML(std::cout,0);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::Copy(vtkMRMLNode *anode)
{
  vtkMRMLAstroLabelMapVolumeNode *astroVolumeNode = vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(anode);
  if (!astroVolumeNode)
    {
    return;
    }

  this->Superclass::Copy(astroVolumeNode);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
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
  vtkNew<vtkMRMLLabelMapVolumeDisplayNode> dispNode;
  this->GetScene()->AddNode(dispNode.GetPointer());
  dispNode->SetDefaultColorMap();
  this->SetAndObserveDisplayNodeID(dispNode->GetID());
}

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::CreateNoneNode(vtkMRMLScene *scene)
{
  vtkNew<vtkImageData> id;
  id->SetDimensions(1, 1, 1);
  id->AllocateScalars(VTK_FLOAT, 1);
  id->GetPointData()->GetScalars()->FillComponent(0, 0);

  vtkNew<vtkMRMLAstroLabelMapVolumeNode> n;
  n->SetName("None");
  // the scene will set the id
  n->SetAndObserveImageData(id.GetPointer());
  scene->AddNode(n.GetPointer());
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

//----------------------------------------------------------------------------
void vtkMRMLAstroLabelMapVolumeNode::UpdateRangeAttributes()
{
  this->GetImageData()->Modified();
  this->GetImageData()->GetPointData()->GetScalars()->Modified();
  double range[2];
  this->GetImageData()->GetScalarRange(range);
  this->SetAttribute("SlicerAstro.DATAMAX", DoubleToString(range[1]).c_str());
  this->SetAttribute("SlicerAstro.DATAMIN", DoubleToString(range[0]).c_str());
}

