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
#include <vtkMRMLAstroStatisticsParametersNode.h>

//------------------------------------------------------------------------------
const char* vtkMRMLAstroStatisticsParametersNode::TABLE_REFERENCE_ROLE = "Table";
const char* vtkMRMLAstroStatisticsParametersNode::ROI_REFERENCE_ROLE = "ROI";

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroStatisticsParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroStatisticsParametersNode::vtkMRMLAstroStatisticsParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = NULL;
  this->MaskVolumeNodeID = NULL;
  this->Mode = NULL;
  this->SetMode("ROI");
  this->SetCores(0);
  this->Max = true;
  this->Mean = true;
  this->Median = false;
  this->Min = true;
  this->Npixels = true;
  this->Std = true;
  this->Sum = true;
  this->TotalFlux = true;
  this->OutputSerial = 1;
  this->SetStatus(0);
}

//----------------------------------------------------------------------------
vtkMRMLAstroStatisticsParametersNode::~vtkMRMLAstroStatisticsParametersNode()
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

  if (this->Mode)
    {
    delete [] this->Mode;
    this->Mode = NULL;
    }
}

//----------------------------------------------------------------------------
const char *vtkMRMLAstroStatisticsParametersNode::GetTableNodeReferenceRole()
{
  return vtkMRMLAstroStatisticsParametersNode::TABLE_REFERENCE_ROLE;
}

//----------------------------------------------------------------------------
const char *vtkMRMLAstroStatisticsParametersNode::GetROINodeReferenceRole()
{
  return vtkMRMLAstroStatisticsParametersNode::ROI_REFERENCE_ROLE;
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
void vtkMRMLAstroStatisticsParametersNode::SetTableNode(vtkMRMLTableNode* node)
{
  this->SetNodeReferenceID(this->GetTableNodeReferenceRole(), (node ? node->GetID() : NULL));
}

//----------------------------------------------------------------------------
vtkMRMLTableNode *vtkMRMLAstroStatisticsParametersNode::GetTableNode()
{
  if (!this->Scene)
    {
    return NULL;
    }

  return vtkMRMLTableNode::SafeDownCast(this->GetNodeReference(this->GetTableNodeReferenceRole()));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroStatisticsParametersNode::SetROINode(vtkMRMLAnnotationROINode* node)
{
  this->SetNodeReferenceID(this->GetROINodeReferenceRole(), (node ? node->GetID() : NULL));
}

//----------------------------------------------------------------------------
vtkMRMLAnnotationROINode *vtkMRMLAstroStatisticsParametersNode::GetROINode()
{
  if (!this->Scene)
    {
    return NULL;
    }

  return vtkMRMLAnnotationROINode::SafeDownCast(this->GetNodeReference(this->GetROINodeReferenceRole()));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroStatisticsParametersNode::ReadXMLAttributes(const char** atts)
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

    if (!strcmp(attName, "Mode"))
      {
      this->SetMode(attValue);
      continue;
      }

    if (!strcmp(attName, "Cores"))
      {
      this->Cores = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Max"))
      {
      this->Max = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Mean"))
      {
      this->Mean = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Median"))
      {
      this->Median = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Min"))
      {
      this->Min = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Npixels"))
      {
      this->Npixels = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Std"))
      {
      this->Std = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Sum"))
      {
      this->Sum = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "TotalFlux"))
      {
      this->TotalFlux = StringToInt(attValue);
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
void vtkMRMLAstroStatisticsParametersNode::WriteXML(ostream& of, int nIndent)
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

  if (this->Mode != NULL)
    {
    of << indent << " Mode=\"" << this->Mode << "\"";
    }

  of << indent << " Cores=\"" << this->Cores << "\"";
  of << indent << " Max=\"" << this->Max << "\"";
  of << indent << " Mean=\"" << this->Mean << "\"";
  of << indent << " Median=\"" << this->Median << "\"";
  of << indent << " Min=\"" << this->Min << "\"";
  of << indent << " Npixels=\"" << this->Npixels << "\"";
  of << indent << " Std=\"" << this->Std << "\"";
  of << indent << " Sum=\"" << this->Sum << "\"";
  of << indent << " TotalFlux=\"" << this->TotalFlux << "\"";
  of << indent << " OutputSerial=\"" << this->OutputSerial << "\"";
  of << indent << " Status=\"" << this->Status << "\"";
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroStatisticsParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLAstroStatisticsParametersNode *node = vtkMRMLAstroStatisticsParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetMaskVolumeNodeID(node->GetMaskVolumeNodeID());
  this->SetMode(node->GetMode());
  this->SetCores(node->GetCores());
  this->SetMax(node->GetMax());
  this->SetMean(node->GetMean());
  this->SetMedian(node->GetMedian());
  this->SetMin(node->GetMin());
  this->SetNpixels(node->GetNpixels());
  this->SetStd(node->GetStd());
  this->SetSum(node->GetSum());
  this->SetTotalFlux(node->GetTotalFlux());
  this->SetOutputSerial(node->GetOutputSerial());
  this->SetStatus(node->GetStatus());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroStatisticsParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << "MaskVolumeNodeID: " << ( (this->MaskVolumeNodeID) ? this->MaskVolumeNodeID : "None" ) << "\n";
  os << "Mode: " << ( (this->Mode) ? this->Mode : "None" ) << "\n";
  os << "Max: " << this->Max << "\n";
  os << "Mean: " << this->Mean << "\n";
  os << "Median: " << this->Median << "\n";
  os << "Min: " << this->Min << "\n";
  os << "Npixels: " << this->Npixels << "\n";
  os << "Std: " << this->Std << "\n";
  os << "Sum: " << this->Sum << "\n";
  os << "TotalFlux: " << this->TotalFlux << "\n";
  os << "OutputSerial: " << this->OutputSerial << "\n";
  os << "Status: " << this->Status << "\n";
  if (this->Cores != 0)
    {
    os << "Number of CPU cores: "<< this->Cores<< "\n";
    }
}
