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
#include <vtkMRMLAstroPVSliceParametersNode.h>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroPVSliceParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroPVSliceParametersNode::vtkMRMLAstroPVSliceParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = NULL;
  this->MomentMapNodeID = NULL;
  this->RulerNodeID = NULL;

  this->SetRulerAngle(0.);
  this->SetRulerOldAngle(0.);
  this->SetRulerShiftX(0.);
  this->SetRulerOldShiftX(0.);
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
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}
}// end namespace

//----------------------------------------------------------------------------
vtkMRMLAstroPVSliceParametersNode::~vtkMRMLAstroPVSliceParametersNode()
{
  if (this->InputVolumeNodeID)
    {
    delete [] this->InputVolumeNodeID;
    this->InputVolumeNodeID = NULL;
    }

  if (this->MomentMapNodeID)
    {
    delete [] this->MomentMapNodeID;
    this->MomentMapNodeID = NULL;
    }

  if (this->RulerNodeID)
    {
    delete [] this->RulerNodeID;
    this->RulerNodeID = NULL;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::ReadXMLAttributes(const char** atts)
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

    if (!strcmp(attName, "MomentMapNodeID"))
      {
      this->SetMomentMapNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "RulerNodeID"))
      {
      this->SetRulerNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "RulerAngle"))
      {
      this->RulerAngle = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "RulerOldAngle"))
      {
      this->RulerOldAngle = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "RulerShiftX"))
      {
      this->RulerShiftX = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "RulerOldShiftX"))
      {
      this->RulerOldShiftX = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "RulerShiftY"))
      {
      this->RulerShiftY = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "RulerOldShiftY"))
      {
      this->RulerOldShiftY = StringToDouble(attValue);
      continue;
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  if (this->InputVolumeNodeID != NULL)
    {
    of << indent << " inputVolumeNodeID=\"" << this->InputVolumeNodeID << "\"";
    }

  if (this->MomentMapNodeID != NULL)
    {
    of << indent << " MomentMapNodeID=\"" << this->MomentMapNodeID << "\"";
    }

  if (this->RulerNodeID != NULL)
    {
    of << indent << " RulerNodeID=\"" << this->RulerNodeID << "\"";
    }

  of << indent << " RulerAngle=\"" << this->RulerAngle << "\"";
  of << indent << " RulerOldAngle=\"" << this->RulerOldAngle << "\"";
  of << indent << " RulerShiftX=\"" << this->RulerShiftX << "\"";
  of << indent << " RulerOldShiftX=\"" << this->RulerOldShiftX << "\"";
  of << indent << " RulerShiftY=\"" << this->RulerShiftY << "\"";
  of << indent << " RulerOldShiftY=\"" << this->RulerOldShiftY << "\"";
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroPVSliceParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLAstroPVSliceParametersNode *node = vtkMRMLAstroPVSliceParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetMomentMapNodeID(node->GetMomentMapNodeID());
  this->SetRulerNodeID(node->GetRulerNodeID());
  this->SetRulerAngle(node->GetRulerAngle());
  this->SetRulerOldAngle(node->GetRulerOldAngle());
  this->SetRulerShiftX(node->GetRulerShiftX());
  this->SetRulerOldShiftX(node->GetRulerOldShiftX());
  this->SetRulerShiftY(node->GetRulerShiftY());
  this->SetRulerOldShiftY(node->GetRulerOldShiftY());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << "MomentMapNodeID: " << ( (this->MomentMapNodeID) ? this->MomentMapNodeID : "None" ) << "\n";
  os << "RulerNodeID: " << ( (this->RulerNodeID) ? this->RulerNodeID : "None" ) << "\n";
  os << "RulerAngle: " << this->RulerAngle << "\n";
  os << "RulerOldAngle: " << this->RulerOldAngle << "\n";
  os << "RulerShiftX: " << this->RulerShiftX << "\n";
  os << "RulerOldShiftX: " << this->RulerOldShiftX << "\n";
  os << "RulerShiftY: " << this->RulerShiftY << "\n";
  os << "RulerOldShiftY: " << this->RulerOldShiftY << "\n";
}
