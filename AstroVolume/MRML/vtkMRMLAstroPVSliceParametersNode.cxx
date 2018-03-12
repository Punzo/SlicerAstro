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

  this->RulerAngle = 0.;
  this->RulerOldAngle = 0.;
  this->RulerShiftX = 0.;
  this->RulerOldShiftX = 0.;

  for(int ii = 0; ii < 2; ii++)
    {
    this->RulerCenter[ii] = 0;
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

    if (!strcmp(attName, "RulerCenter"))
      {
      std::stringstream ss;
      int val;
      int RulerCenter[2];
      ss << attValue;
      for(int ii = 0; ii < 2; ii++)
        {
        ss >> val;
        RulerCenter[ii] = val;
        }
      this->SetRulerCenter(RulerCenter);
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
  of << indent << " RulerCenter=\"" << this->RulerCenter[0] << " " << this->RulerCenter[1] << "\"";
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
  this->SetRulerCenter(node->GetRulerCenter());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::SetRulerCenterRightAscension(int value)
{
  if (!vtkMathUtilities::FuzzyCompare<int>(this->RulerCenter[0], value))
    {
    this->RulerCenter[0] = value;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::SetRulerCenterDeclination(int value)
{
  if (!vtkMathUtilities::FuzzyCompare<int>(this->RulerCenter[1], value))
    {
    this->RulerCenter[1] = value;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::SetRulerCenter(int arg1, int arg2)
{
  if (!vtkMathUtilities::FuzzyCompare<int>(this->RulerCenter[0], arg1) ||
      !vtkMathUtilities::FuzzyCompare<int>(this->RulerCenter[1], arg2))
    {
    this->RulerCenter[0] = arg1;
    this->RulerCenter[1] = arg2;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::SetRulerCenter(int arg[])
{
  this->SetRulerCenter(arg[0], arg[1]);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << indent << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << indent << "MomentMapNodeID: " << ( (this->MomentMapNodeID) ? this->MomentMapNodeID : "None" ) << "\n";
  os << indent << "RulerNodeID: " << ( (this->RulerNodeID) ? this->RulerNodeID : "None" ) << "\n";
  os << indent << "RulerAngle: " << this->RulerAngle << "\n";
  os << indent << "RulerOldAngle: " << this->RulerOldAngle << "\n";
  os << indent << "RulerShiftX: " << this->RulerShiftX << "\n";
  os << indent << "RulerOldShiftX: " << this->RulerOldShiftX << "\n";
  os << indent << "RulerShiftY: " << this->RulerShiftY << "\n";
  os << indent << "RulerOldShiftY: " << this->RulerOldShiftY << "\n";

  os << "RulerCenter:";
  for(int jj = 0; jj < 2; jj++)
    {
    os << indent << " " << this->RulerCenter[jj];
    }
  os << "\n";
}
