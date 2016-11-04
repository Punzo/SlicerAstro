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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

// VTK includes
#include <vtkCommand.h>
#include <vtkObjectFactory.h>
#include <vtkDoubleArray.h>

// MRML includes
#include <vtkMRMLVolumeNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroModelingParametersNode.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroModelingParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroModelingParametersNode::vtkMRMLAstroModelingParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = NULL;
  this->OutputVolumeNodeID = NULL;
  this->OutputSerial = 1;
  this->SetStatus(0);
  this->SetFitSuccess(false);
}

//----------------------------------------------------------------------------
vtkMRMLAstroModelingParametersNode::~vtkMRMLAstroModelingParametersNode()
{
  if (this->InputVolumeNodeID)
    {
    delete [] this->InputVolumeNodeID;
    this->InputVolumeNodeID = NULL;
    }

  if (this->OutputVolumeNodeID)
    {
    delete [] this->OutputVolumeNodeID;
    this->OutputVolumeNodeID = NULL;
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
void vtkMRMLAstroModelingParametersNode::ReadXMLAttributes(const char** atts)
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

    if (!strcmp(attName, "outputVolumeNodeID"))
      {
      this->SetOutputVolumeNodeID(attValue);
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

    if (!strcmp(attName, "FitSuccess"))
      {
      this->FitSuccess = StringToInt(attValue);
      continue;
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroModelingParametersNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  if (this->InputVolumeNodeID != NULL)
    {
    of << indent << " inputVolumeNodeID=\"" << this->InputVolumeNodeID << "\"";
    }

  if (this->OutputVolumeNodeID != NULL)
    {
    of << indent << " outputVolumeNodeID=\"" << this->OutputVolumeNodeID << "\"";
    }

  of << indent << " OutputSerial=\"" << this->OutputSerial << "\"";
  of << indent << " Status=\"" << this->Status << "\"";
  of << indent << " FitSuccess=\"" << this->FitSuccess << "\"";
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroModelingParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLAstroModelingParametersNode *node = vtkMRMLAstroModelingParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetOutputVolumeNodeID(node->GetOutputVolumeNodeID());
  this->SetOutputSerial(node->GetOutputSerial());
  this->SetStatus(node->GetStatus());
  this->SetFitSuccess(node->GetFitSuccess());
  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroModelingParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << "OutputVolumeNodeID: " << ( (this->OutputVolumeNodeID) ? this->OutputVolumeNodeID : "None" ) << "\n";
  os << "OutputSerial: " << this->OutputSerial << "\n";
  os << "Status: " << this->Status << "\n";
  os << "FitSuccess: " << this->FitSuccess << "\n";
}
