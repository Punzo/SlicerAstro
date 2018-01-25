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
#include <vtkMRMLTableNode.h>
#include <vtkMRMLVolumeNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroProfilesParametersNode.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroProfilesParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroProfilesParametersNode::vtkMRMLAstroProfilesParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = NULL;
  this->ProfileVolumeNodeID = NULL;
  this->MaskVolumeNodeID = NULL;
  this->SetCores(0);
  this->SetMaskActive(false);
  this->SetIntensityMin(-1.);
  this->SetIntensityMax(1.);
  this->SetVelocityMin(-1.);
  this->SetVelocityMax(1.);
  this->OutputSerial = 1;
  this->SetStatus(0);
}

//----------------------------------------------------------------------------
vtkMRMLAstroProfilesParametersNode::~vtkMRMLAstroProfilesParametersNode()
{
  if (this->InputVolumeNodeID)
    {
    delete [] this->InputVolumeNodeID;
    this->InputVolumeNodeID = NULL;
    }

  if (this->ProfileVolumeNodeID)
    {
    delete [] this->ProfileVolumeNodeID;
    this->ProfileVolumeNodeID = NULL;
    }

  if (this->MaskVolumeNodeID)
    {
    delete [] this->MaskVolumeNodeID;
    this->MaskVolumeNodeID = NULL;
    }
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

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}
}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroProfilesParametersNode::ReadXMLAttributes(const char** atts)
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

    if (!strcmp(attName, "ProfileVolumeNodeID"))
      {
      this->SetProfileVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "MaskVolumeNodeID"))
      {
      this->SetMaskVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "Cores"))
      {
      this->Cores = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "MaskActive"))
      {
      this->MaskActive = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "IntensityMin"))
      {
      this->IntensityMin = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "IntensityMax"))
      {
      this->IntensityMax = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "VelocityMin"))
      {
      this->VelocityMin = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "VelocityMax"))
      {
      this->VelocityMax = StringToDouble(attValue);
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
void vtkMRMLAstroProfilesParametersNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  if (this->InputVolumeNodeID != NULL)
    {
    of << indent << " inputVolumeNodeID=\"" << this->InputVolumeNodeID << "\"";
    }

  if (this->ProfileVolumeNodeID != NULL)
    {
    of << indent << " ProfileVolumeNodeID=\"" << this->ProfileVolumeNodeID << "\"";
    }

  if (this->MaskVolumeNodeID != NULL)
    {
    of << indent << " MaskVolumeNodeID=\"" << this->MaskVolumeNodeID << "\"";
    }

  of << indent << " Cores=\"" << this->Cores << "\"";
  of << indent << " MaskActive=\"" << this->MaskActive << "\"";
  of << indent << " IntensityMin=\"" << this->IntensityMin << "\"";
  of << indent << " IntensityMax=\"" << this->IntensityMax << "\"";
  of << indent << " VelocityMin=\"" << this->VelocityMin << "\"";
  of << indent << " VelocityMax=\"" << this->VelocityMax << "\"";
  of << indent << " OutputSerial=\"" << this->OutputSerial << "\"";
  of << indent << " Status=\"" << this->Status << "\"";
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroProfilesParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLAstroProfilesParametersNode *node = vtkMRMLAstroProfilesParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetProfileVolumeNodeID(node->GetProfileVolumeNodeID());
  this->SetMaskVolumeNodeID(node->GetMaskVolumeNodeID());
  this->SetCores(node->GetCores());
  this->SetMaskActive(node->GetMaskActive());
  this->SetIntensityMin(node->GetIntensityMin());
  this->SetIntensityMax(node->GetIntensityMax());
  this->SetVelocityMin(node->GetVelocityMin());
  this->SetVelocityMax(node->GetVelocityMax());
  this->SetOutputSerial(node->GetOutputSerial());
  this->SetStatus(node->GetStatus());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroProfilesParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << "ProfileVolumeNodeID: " << ( (this->ProfileVolumeNodeID) ? this->ProfileVolumeNodeID : "None" ) << "\n";
  os << "MaskVolumeNodeID: " << ( (this->MaskVolumeNodeID) ? this->MaskVolumeNodeID : "None" ) << "\n";
  os << "MaskActive: " << this->MaskActive << "\n";
  os << "IntensityMin: " << this->IntensityMin << "\n";
  os << "IntensityMax: " << this->IntensityMax << "\n";
  os << "VelocityMin: " << this->VelocityMin << "\n";
  os << "VelocityMax: " << this->VelocityMax << "\n";
  os << "OutputSerial: " << this->OutputSerial << "\n";
  os << "Status: " << this->Status << "\n";
  if (this->Cores != 0)
    {
    os << "Number of CPU cores: "<< this->Cores<< "\n";
    }
}
