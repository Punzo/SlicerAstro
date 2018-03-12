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

// VTK includes

#include <vtkCollection.h>
#include <vtkCollectionIterator.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLVolumeNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroMaskingParametersNode.h>

//------------------------------------------------------------------------------
const char* vtkMRMLAstroMaskingParametersNode::ROI_REFERENCE_ROLE = "ROI";

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroMaskingParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroMaskingParametersNode::vtkMRMLAstroMaskingParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = NULL;
  this->MaskVolumeNodeID = NULL;
  this->OutputVolumeNodeID = NULL;
  this->Mode = NULL;
  this->SetMode("ROI");
  this->Operation = NULL;
  this->SetOperation("Blank");
  this->BlankRegion = NULL;
  this->SetBlankRegion("Outside");
  this->BlankValue = NULL;
  this->SetBlankValue("NaN");
  this->OutputSerial = 1;
  this->Status = 0;
}

//----------------------------------------------------------------------------
vtkMRMLAstroMaskingParametersNode::~vtkMRMLAstroMaskingParametersNode()
{
  if (this->InputVolumeNodeID)
    {
    delete [] this->InputVolumeNodeID;
    this->InputVolumeNodeID = NULL;
    }

  if (this->MaskVolumeNodeID)
    {
    delete [] this->MaskVolumeNodeID;
    this->MaskVolumeNodeID = NULL;
    }

  if (this->OutputVolumeNodeID)
    {
    delete [] this->OutputVolumeNodeID;
    this->OutputVolumeNodeID = NULL;
    }

  if (this->Mode)
    {
    delete [] this->Mode;
    this->Mode = NULL;
    }

  if (this->Operation)
    {
    delete [] this->Operation;
    this->Operation = NULL;
    }

  if (this->BlankRegion)
    {
    delete [] this->BlankRegion;
    this->BlankRegion = NULL;
    }

  if (this->BlankValue)
    {
    delete [] this->BlankValue;
    this->BlankValue = NULL;
    }
}

//----------------------------------------------------------------------------
const char *vtkMRMLAstroMaskingParametersNode::GetROINodeReferenceRole()
{
  return vtkMRMLAstroMaskingParametersNode::ROI_REFERENCE_ROLE;
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
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}
}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroMaskingParametersNode::SetROINode(vtkMRMLAnnotationROINode* node)
{
  this->SetNodeReferenceID(this->GetROINodeReferenceRole(), (node ? node->GetID() : NULL));
}

//----------------------------------------------------------------------------
vtkMRMLAnnotationROINode *vtkMRMLAstroMaskingParametersNode::GetROINode()
{
  if (!this->Scene)
    {
    return NULL;
    }

  return vtkMRMLAnnotationROINode::SafeDownCast(this->GetNodeReference(this->GetROINodeReferenceRole()));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMaskingParametersNode::ReadXMLAttributes(const char** atts)
{
  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;
  while (*atts != NULL)
    {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, "inputVolumeNodeID"))
      {
      this->SetInputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "MaskVolumeNodeID"))
      {
      this->SetMaskVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "outputVolumeNodeID"))
      {
      this->SetOutputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "Mode"))
      {
      this->SetMode(attValue);
      continue;
      }

    if (!strcmp(attName, "Operation"))
      {
      this->SetOperation(attValue);
      continue;
      }

    if (!strcmp(attName, "BlankRegion"))
      {
      this->SetBlankRegion(attValue);
      continue;
      }

    if (!strcmp(attName, "BlankValue"))
      {
      this->SetBlankValue(attValue);
      continue;
      }

    if (!strcmp(attName, "OutputSerial"))
      {
      this->OutputSerial = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Status"))
      {
      this->Status = StringToInt(attValue);
      continue;
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMaskingParametersNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  if (this->InputVolumeNodeID != NULL)
    {
    of << indent << " inputVolumeNodeID=\"" << this->InputVolumeNodeID << "\"";
    }

  if (this->MaskVolumeNodeID != NULL)
    {
    of << indent << " MaskVolumeNodeID=\"" << this->MaskVolumeNodeID << "\"";
    }

  if (this->OutputVolumeNodeID != NULL)
    {
    of << indent << " outputVolumeNodeID=\"" << this->OutputVolumeNodeID << "\"";
    }

  if (this->Mode != NULL)
    {
    of << indent << " Mode=\"" << this->Mode << "\"";
    }

  if (this->Operation != NULL)
    {
    of << indent << " Operation=\"" << this->Operation << "\"";
    }

  if (this->BlankRegion != NULL)
    {
    of << indent << " BlankRegion=\"" << this->BlankRegion << "\"";
    }

  if (this->BlankValue != NULL)
    {
    of << indent << " BlankValue=\"" << this->BlankValue << "\"";
    }

  of << indent << " OutputSerial=\"" << this->OutputSerial << "\"";
  of << indent << " Status=\"" << this->Status << "\"";
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroMaskingParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLAstroMaskingParametersNode *node = vtkMRMLAstroMaskingParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetMaskVolumeNodeID(node->GetMaskVolumeNodeID());
  this->SetOutputVolumeNodeID(node->GetOutputVolumeNodeID());
  this->SetMode(node->GetMode());
  this->SetOperation(node->GetOperation());
  this->SetBlankRegion(node->GetBlankRegion());
  this->SetBlankValue(node->GetBlankValue());
  this->SetOutputSerial(node->GetOutputSerial());
  this->SetStatus(node->GetStatus());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroMaskingParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << indent << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << indent << "MaskVolumeNodeID: " << ( (this->MaskVolumeNodeID) ? this->MaskVolumeNodeID : "None" ) << "\n";
  os << indent << "OutputVolumeNodeID: " << ( (this->OutputVolumeNodeID) ? this->OutputVolumeNodeID : "None" ) << "\n";
  os << indent << "Mode: " << ( (this->Mode) ? this->Mode : "None" ) << "\n";
  os << indent << "Operation: " << ( (this->Operation) ? this->Operation : "None" ) << "\n";
  os << indent << "BlankRegion: " << ( (this->BlankRegion) ? this->BlankRegion : "None" ) << "\n";
  os << indent << "BlankValue: " << ( (this->BlankValue) ? this->BlankValue : "None" ) << "\n";
  os << indent << "OutputSerial: " << this->OutputSerial << "\n";
  os << indent << "Status: " << this->Status << "\n";
}
