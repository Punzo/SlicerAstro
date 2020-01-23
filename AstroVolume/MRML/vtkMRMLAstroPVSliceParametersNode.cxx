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
  this->LineNodeID = NULL;

  this->LineAngle = 0.;
  this->LineOldAngle = 0.;

  for(int ii = 0; ii < 2; ii++)
    {
    this->LineCenter[ii] = 0;
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

  if (this->LineNodeID)
    {
    delete [] this->LineNodeID;
    this->LineNodeID = NULL;
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

    if (!strcmp(attName, "LineNodeID"))
      {
      this->SetLineNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "LineAngle"))
      {
      this->LineAngle = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "LineOldAngle"))
      {
      this->LineOldAngle = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "LineCenter"))
      {
      std::stringstream ss;
      int val;
      int LineCenter[2];
      ss << attValue;
      for(int ii = 0; ii < 2; ii++)
        {
        ss >> val;
        LineCenter[ii] = val;
        }
      this->SetLineCenter(LineCenter);
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

  if (this->LineNodeID != NULL)
    {
    of << indent << " LineNodeID=\"" << this->LineNodeID << "\"";
    }

  of << indent << " LineAngle=\"" << this->LineAngle << "\"";
  of << indent << " LineOldAngle=\"" << this->LineOldAngle << "\"";
  of << indent << " LineCenter=\"" << this->LineCenter[0] << " " << this->LineCenter[1] << "\"";
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
  this->SetLineNodeID(node->GetLineNodeID());
  this->SetLineAngle(node->GetLineAngle());
  this->SetLineOldAngle(node->GetLineOldAngle());
  this->SetLineCenter(node->GetLineCenter());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::SetLineCenterRightAscension(int value)
{
  if (!vtkMathUtilities::FuzzyCompare<int>(this->LineCenter[0], value))
    {
    this->LineCenter[0] = value;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::SetLineCenterDeclination(int value)
{
  if (!vtkMathUtilities::FuzzyCompare<int>(this->LineCenter[1], value))
    {
    this->LineCenter[1] = value;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::SetLineCenter(int arg1, int arg2)
{
  if (!vtkMathUtilities::FuzzyCompare<int>(this->LineCenter[0], arg1) ||
      !vtkMathUtilities::FuzzyCompare<int>(this->LineCenter[1], arg2))
    {
    this->LineCenter[0] = arg1;
    this->LineCenter[1] = arg2;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::SetLineCenter(int arg[])
{
  this->SetLineCenter(arg[0], arg[1]);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroPVSliceParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << indent << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << indent << "MomentMapNodeID: " << ( (this->MomentMapNodeID) ? this->MomentMapNodeID : "None" ) << "\n";
  os << indent << "LineNodeID: " << ( (this->LineNodeID) ? this->LineNodeID : "None" ) << "\n";
  os << indent << "LineAngle: " << this->LineAngle << "\n";
  os << indent << "LineOldAngle: " << this->LineOldAngle << "\n";

  os << "LineCenter:";
  for(int jj = 0; jj < 2; jj++)
    {
    os << indent << " " << this->LineCenter[jj];
    }
  os << "\n";
}
