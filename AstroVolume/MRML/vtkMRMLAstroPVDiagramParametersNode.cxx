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
#include <vtkMathUtilities.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// MRML includes
#include <vtkMRMLTableNode.h>
#include <vtkMRMLVolumeNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroPVDiagramParametersNode.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroPVDiagramParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroPVDiagramParametersNode::vtkMRMLAstroPVDiagramParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = nullptr;
  this->MomentMapNodeID = nullptr;
  this->OutputVolumeNodeID = nullptr;
  this->FiducialsMarkupsID = nullptr;
  this->ModelID = nullptr;
  this->Interpolation = true;
  this->AutoUpdate = true;
}

//----------------------------------------------------------------------------
vtkMRMLAstroPVDiagramParametersNode::~vtkMRMLAstroPVDiagramParametersNode()
{
  if (this->InputVolumeNodeID)
    {
    delete [] this->InputVolumeNodeID;
    this->InputVolumeNodeID = nullptr;
    }

  if (this->MomentMapNodeID)
    {
    delete [] this->MomentMapNodeID;
    this->MomentMapNodeID = nullptr;
    }

  if (this->OutputVolumeNodeID)
    {
    delete [] this->OutputVolumeNodeID;
    this->OutputVolumeNodeID = nullptr;
    }

  if (this->FiducialsMarkupsID)
    {
    delete [] this->FiducialsMarkupsID;
    this->FiducialsMarkupsID = nullptr;
    }

  if (this->ModelID)
    {
    delete [] this->ModelID;
    this->ModelID = nullptr;
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
void vtkMRMLAstroPVDiagramParametersNode::ReadXMLAttributes(const char** atts)
{
  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;
  while (*atts != nullptr)
    {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, "InputVolumeNodeID"))
      {
      this->SetInputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "MomentMapNodeID"))
      {
      this->SetMomentMapNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "OutputVolumeNodeID"))
      {
      this->SetOutputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "FiducialsMarkupsID"))
      {
      this->SetFiducialsMarkupsID(attValue);
      continue;
      }

    if (!strcmp(attName, "ModelID"))
      {
      this->SetModelID(attValue);
      continue;
      }

    if (!strcmp(attName, "Interpolation"))
      {
      this->Interpolation = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "AutoUpdate"))
      {
      this->AutoUpdate = StringToInt(attValue);
      continue;
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVDiagramParametersNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  if (this->InputVolumeNodeID != nullptr)
    {
    of << indent << " InputVolumeNodeID=\"" << this->InputVolumeNodeID << "\"";
    }

  if (this->MomentMapNodeID != nullptr)
    {
    of << indent << " MomentMapNodeID=\"" << this->MomentMapNodeID << "\"";
    }

  if (this->OutputVolumeNodeID != nullptr)
    {
    of << indent << " OutputVolumeNodeID=\"" << this->OutputVolumeNodeID << "\"";
    }

  if (this->FiducialsMarkupsID != nullptr)
    {
    of << indent << " FiducialsMarkupsID=\"" << this->FiducialsMarkupsID << "\"";
    }

  if (this->ModelID != nullptr)
    {
    of << indent << " ModelID=\"" << this->ModelID << "\"";
    }

  of << indent << " Interpolation=\"" << this->Interpolation << "\"";
  of << indent << " AutoUpdate=\"" << this->AutoUpdate << "\"";
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroPVDiagramParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLAstroPVDiagramParametersNode *node = vtkMRMLAstroPVDiagramParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetMomentMapNodeID(node->GetMomentMapNodeID());
  this->SetOutputVolumeNodeID(node->GetOutputVolumeNodeID());
  this->SetFiducialsMarkupsID(node->GetFiducialsMarkupsID());
  this->SetModelID(node->GetModelID());
  this->SetInterpolation(node->GetInterpolation());
  this->SetAutoUpdate(node->GetAutoUpdate());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVDiagramParametersNode::SetInterpolation(bool interpolation)
{
  this->Interpolation = interpolation;
  this->InvokeCustomModifiedEvent(vtkMRMLAstroPVDiagramParametersNode::InterpolationModifiedEvent);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVDiagramParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << indent << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << indent << "MomentMapNodeID: " << ( (this->MomentMapNodeID) ? this->MomentMapNodeID : "None" ) << "\n";
  os << indent << "OutputVolumeNodeID: " << ( (this->OutputVolumeNodeID) ? this->OutputVolumeNodeID : "None" ) << "\n";
  os << indent << "FiducialsMarkupsID: " << ( (this->FiducialsMarkupsID) ? this->FiducialsMarkupsID : "None" ) << "\n";
  os << indent << "ModelID: " << ( (this->ModelID) ? this->ModelID : "None" ) << "\n";

  if(this->Interpolation)
    {
    os << indent << "Interpolation: Active (BSpline)\n";
    }
  else
    {
    os << indent << "Interpolation: Inactive\n";
    }

  if(this->AutoUpdate)
    {
    os << indent << "AutoUpdate: Active\n";
    }
  else
    {
    os << indent << "AutoUpdate: Inactive\n";
    }
}
