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
#include <vtkMRMLVolumeNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroReprojectParametersNode.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroReprojectParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroReprojectParametersNode::vtkMRMLAstroReprojectParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = nullptr;
  this->ReferenceVolumeNodeID = nullptr;
  this->OutputVolumeNodeID = nullptr;
  this->ReprojectRotation = false;
  this->ReprojectData = false;
  this->OutputSerial = 1;
  this->InterpolationOrder = 1;
  this->Cores = 0;
  this->Status = 0;
}

//----------------------------------------------------------------------------
vtkMRMLAstroReprojectParametersNode::~vtkMRMLAstroReprojectParametersNode()
{
  if (this->InputVolumeNodeID)
    {
    delete [] this->InputVolumeNodeID;
    this->InputVolumeNodeID = nullptr;
    }

  if (this->ReferenceVolumeNodeID)
    {
    delete [] this->ReferenceVolumeNodeID;
    this->ReferenceVolumeNodeID = nullptr;
    }

  if (this->OutputVolumeNodeID)
    {
    delete [] this->OutputVolumeNodeID;
    this->OutputVolumeNodeID = nullptr;
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

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroReprojectParametersNode::ReadXMLAttributes(const char** atts)
{
  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;
  while (*atts != nullptr)
    {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, "inputVolumeNodeID"))
      {
      this->SetInputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "referenceVolumeNodeID"))
      {
      this->SetReferenceVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "outputVolumeNodeID"))
      {
      this->SetOutputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "ReprojectRotation"))
      {
      this->ReprojectRotation = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "ReprojectData"))
      {
      this->ReprojectData = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "InterpolationOrder"))
      {
      this->InterpolationOrder = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "OutputSerial"))
      {
      this->OutputSerial = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Cores"))
      {
      this->Cores = StringToInt(attValue);
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
void vtkMRMLAstroReprojectParametersNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  if (this->InputVolumeNodeID != nullptr)
    {
    of << indent << " inputVolumeNodeID=\"" << this->InputVolumeNodeID << "\"";
    }

  if (this->ReferenceVolumeNodeID != nullptr)
    {
    of << indent << " referenceVolumeNodeID=\"" << this->ReferenceVolumeNodeID << "\"";
    }

  if (this->OutputVolumeNodeID != nullptr)
    {
    of << indent << " outputVolumeNodeID=\"" << this->OutputVolumeNodeID << "\"";
    }

  of << indent << " ReprojectRotation=\"" << this->ReprojectRotation << "\"";
  of << indent << " ReprojectData=\"" << this->ReprojectData << "\"";
  of << indent << " OutputSerial=\"" << this->OutputSerial << "\"";
  of << indent << " InterpolationOrder=\"" << this->InterpolationOrder << "\"";
  of << indent << " Cores=\"" << this->Cores << "\"";
  of << indent << " Status=\"" << this->Status << "\"";
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroReprojectParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLAstroReprojectParametersNode *node = vtkMRMLAstroReprojectParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetReferenceVolumeNodeID(node->GetReferenceVolumeNodeID());
  this->SetOutputVolumeNodeID(node->GetOutputVolumeNodeID());
  this->SetReprojectRotation(node->GetReprojectRotation());
  this->SetReprojectData(node->GetReprojectData());
  this->SetInterpolationOrder(node->GetInterpolationOrder());
  this->SetOutputSerial(node->GetOutputSerial());
  this->SetCores(node->GetCores());
  this->SetStatus(node->GetStatus());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroReprojectParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << indent << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << indent << "ReferenceVolumeNodeID: " << ( (this->ReferenceVolumeNodeID) ? this->ReferenceVolumeNodeID : "None" ) << "\n";
  os << indent << "OutputVolumeNodeID: " << ( (this->OutputVolumeNodeID) ? this->OutputVolumeNodeID : "None" ) << "\n";
  os << indent << "ReprojectRotation: " << this->ReprojectRotation << "\n";
  os << indent << "ReprojectData: " << this->ReprojectData << "\n";
  os << indent << "InterpolationOrder: " << this->InterpolationOrder << "\n";
  os << indent << "OutputSerial: " << this->OutputSerial << "\n";
  os << indent << "Cores: " << this->Cores << "\n";
  os << indent << "Status: " << this->Status << "\n";
}
