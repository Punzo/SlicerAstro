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
#include <vtkMRMLAstroModelingParametersNode.h>

//------------------------------------------------------------------------------
const char* vtkMRMLAstroModelingParametersNode::PARAMS_TABLE_REFERENCE_ROLE = "paramsTable";

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroModelingParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroModelingParametersNode::vtkMRMLAstroModelingParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = NULL;
  this->OutputVolumeNodeID = NULL;
  this->ResidualVolumeNodeID = NULL;
  this->MaskVolumeNodeID = NULL;
  this->MaskActive = false;
  this->OutputSerial = 1;
  this->Mode = NULL;
  this->SetMode("Automatic");
  this->Normalize = NULL;
  this->SetNormalize("LOCAL");
  this->RadSep = 0.;
  this->XCenter = 0.;
  this->YCenter = 0.;
  this->SystemicVelocity = 0.;
  this->RotationVelocity = 0.;
  this->RadialVelocity = 0.;
  this->VelocityDispersion = 0.;
  this->Inclination = 0.;
  this->InclinationError = 5.;
  this->PositionAngle = 0.;
  this->PositionAngleError = 15.;
  this->ScaleHeight = 0.;
  this->Distance = 0.;
  this->ColumnDensity = 1.;
  this->PositionAngleFit = true;
  this->RotationVelocityFit = true;
  this->RadialVelocityFit = false;
  this->VelocityDispersionFit = true;
  this->InclinationFit = true;
  this->XCenterFit = false;
  this->YCenterFit = false;
  this->SystemicVelocityFit = false;
  this->ScaleHeightFit = false;
  this->LayerType = 0;
  this->FittingFunction = 1;
  this->WeightingFunction = 1;
  this->NumberOfClounds = 0;
  this->CloudsColumnDensity = 10.;
  this->XPosCenterIJK = 0.;
  this->YPosCenterIJK = 0.;
  this->XPosCenterRAS = 0.;
  this->YPosCenterRAS = 0.;
  this->ZPosCenterRAS = 0.;
  this->PVPhi = 0.;
  this->YellowRotOldValue = 0.;
  this->YellowRotValue = 0.;
  this->GreenRotOldValue = 0.;
  this->GreenRotValue = 0.;
  this->Status = 0;
  this->Operation = vtkMRMLAstroModelingParametersNode::ESTIMATE;
  this->FitSuccess = false;
  this->ForceSliceUpdate = true;
  this->NumberOfRings = 0;
  this->ContourLevel = 3.;
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

  if (this->ResidualVolumeNodeID)
    {
    delete [] this->ResidualVolumeNodeID;
    this->ResidualVolumeNodeID = NULL;
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

  if (this->Normalize)
    {
    delete [] this->Normalize;
    this->Normalize = NULL;
    }
}

//----------------------------------------------------------------------------
const char *vtkMRMLAstroModelingParametersNode::GetTableNodeReferenceRole()
{
  return vtkMRMLAstroModelingParametersNode::PARAMS_TABLE_REFERENCE_ROLE;
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

    if (!strcmp(attName, "ResidualVolumeNodeID"))
      {
      this->SetResidualVolumeNodeID(attValue);
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

    if (!strcmp(attName, "Normalize"))
      {
      this->SetNormalize(attValue);
      continue;
      }

    if (!strcmp(attName, "NumberOfRings"))
      {
      this->NumberOfRings = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "RadSep"))
      {
      this->RadSep = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "XCenter"))
      {
      this->XCenter = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "YCenter"))
      {
      this->YCenter = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "SystemicVelocity"))
      {
      this->SystemicVelocity = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "RotationVelocity"))
      {
      this->RotationVelocity = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "RadialVelocity"))
      {
      this->RadialVelocity = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "VelocityDispersion"))
      {
      this->VelocityDispersion = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "Inclination"))
      {
      this->Inclination = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "InclinationError"))
      {
      this->InclinationError = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "PositionAngle"))
      {
      this->PositionAngle = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "PositionAngleError"))
      {
      this->PositionAngleError = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "ScaleHeight"))
      {
      this->ScaleHeight = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "ColumnDensity"))
      {
      this->ColumnDensity = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "Distance"))
      {
      this->Distance = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "PositionAngleFit"))
      {
      this->PositionAngleFit = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "RotationVelocityFit"))
      {
      this->RotationVelocityFit = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "RadialVelocityFit"))
      {
      this->RadialVelocityFit = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "VelocityDispersionFit"))
      {
      this->VelocityDispersionFit = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "InclinationFit"))
      {
      this->InclinationFit = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "XCenterFit"))
      {
      this->XCenterFit = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "YCenterFit"))
      {
      this->YCenterFit = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "SystemicVelocityFit"))
      {
      this->SystemicVelocityFit = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "ScaleHeightFit"))
      {
      this->ScaleHeightFit = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "LayerType"))
      {
      this->LayerType = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "FittingFunction"))
      {
      this->FittingFunction = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "WeightingFunction"))
      {
      this->WeightingFunction = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "NumberOfClounds"))
      {
      this->NumberOfClounds = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "CloudsColumnDensity"))
      {
      this->CloudsColumnDensity = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "XPosCenterIJK"))
      {
      this->XPosCenterIJK = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "YPosCenterIJK"))
      {
      this->YPosCenterIJK = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "XPosCenterRAS"))
      {
      this->XPosCenterRAS = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "YPosCenterRAS"))
      {
      this->YPosCenterRAS = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "ZPosCenterRAS"))
      {
      this->ZPosCenterRAS = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "PVPhi"))
      {
      this->PVPhi = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "YellowRotOldValue"))
      {
      this->YellowRotOldValue = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "YellowRotValue"))
      {
      this->YellowRotValue = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "GreenRotOldValue"))
      {
      this->GreenRotOldValue = StringToDouble(attValue);
      continue;
      }

    if (!strcmp(attName, "GreenRotValue"))
      {
      this->GreenRotValue = StringToDouble(attValue);
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

    if (!strcmp(attName, "Operation"))
      {
      this->Operation = this->GetOperationFromString(attValue);
      continue;
      }

    if (!strcmp(attName, "FitSuccess"))
      {
      this->FitSuccess = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "MaskActive"))
      {
      this->MaskActive = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "ContourLevel"))
      {
      this->ContourLevel = StringToDouble(attValue);
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

  if (this->ResidualVolumeNodeID != NULL)
    {
    of << indent << " ResidualVolumeNodeID=\"" << this->ResidualVolumeNodeID << "\"";
    }

  if (this->MaskVolumeNodeID != NULL)
    {
    of << indent << " maskVolumeNodeID=\"" << this->MaskVolumeNodeID << "\"";
    }

  if (this->Mode != NULL)
    {
    of << indent << " Mode=\"" << this->Mode << "\"";
    }

  if (this->Normalize != NULL)
    {
    of << indent << " Normalize=\"" << this->Normalize << "\"";
    }

  of << indent << " MaskActive=\"" << this->MaskActive << "\"";
  of << indent << " OutputSerial=\"" << this->OutputSerial << "\"";
  of << indent << " NumberOfRings=\"" << this->NumberOfRings << "\"";
  of << indent << " RadSep=\"" << this->RadSep << "\"";
  of << indent << " XCenter=\"" << this->XCenter << "\"";
  of << indent << " YCenter=\"" << this->YCenter << "\"";
  of << indent << " SystemicVelocity=\"" << this->SystemicVelocity << "\"";
  of << indent << " RotationVelocity=\"" << this->RotationVelocity << "\"";
  of << indent << " RadialVelocity=\"" << this->RadialVelocity << "\"";
  of << indent << " VelocityDispersion=\"" << this->VelocityDispersion << "\"";
  of << indent << " Inclination=\"" << this->Inclination << "\"";
  of << indent << " InclinationError=\"" << this->InclinationError << "\"";
  of << indent << " PositionAngle=\"" << this->PositionAngle << "\"";
  of << indent << " PositionAngleError=\"" << this->PositionAngleError << "\"";
  of << indent << " ScaleHeight=\"" << this->ScaleHeight << "\"";
  of << indent << " ColumnDensity=\"" << this->ColumnDensity << "\"";
  of << indent << " Distance=\"" << this->Distance << "\"";
  of << indent << " PositionAngleFit=\"" << this->PositionAngleFit << "\"";
  of << indent << " RotationVelocityFit=\"" << this->RotationVelocityFit << "\"";
  of << indent << " RadialVelocityFit=\"" << this->RadialVelocityFit << "\"";
  of << indent << " VelocityDispersionFit=\"" << this->VelocityDispersionFit << "\"";
  of << indent << " InclinationFit=\"" << this->InclinationFit << "\"";
  of << indent << " XCenterFit=\"" << this->XCenterFit << "\"";
  of << indent << " YCenterFit=\"" << this->YCenterFit << "\"";
  of << indent << " SystemicVelocityFit=\"" << this->SystemicVelocityFit << "\"";
  of << indent << " ScaleHeightFit=\"" << this->ScaleHeightFit << "\"";
  of << indent << " LayerType=\"" << this->LayerType << "\"";
  of << indent << " FittingFunction=\"" << this->FittingFunction << "\"";
  of << indent << " WeightingFunction=\"" << this->WeightingFunction << "\"";
  of << indent << " NumberOfClounds=\"" << this->NumberOfClounds << "\"";
  of << indent << " CloudsColumnDensity=\"" << this->CloudsColumnDensity << "\"";
  of << indent << " XPosCenterIJK=\"" << this->XPosCenterIJK << "\"";
  of << indent << " YPosCenterIJK=\"" << this->YPosCenterIJK << "\"";
  of << indent << " XPosCenterRAS=\"" << this->XPosCenterRAS << "\"";
  of << indent << " YPosCenterRAS=\"" << this->YPosCenterRAS << "\"";
  of << indent << " ZPosCenterRAS=\"" << this->ZPosCenterRAS << "\"";
  of << indent << " PVPhi=\"" << this->PVPhi << "\"";
  of << indent << " YellowRotOldValue=\"" << this->YellowRotOldValue << "\"";
  of << indent << " YellowRotValue=\"" << this->YellowRotValue << "\"";
  of << indent << " GreenRotOldValue=\"" << this->GreenRotOldValue << "\"";
  of << indent << " GreenRotValue=\"" << this->GreenRotValue << "\"";
  of << indent << " Status=\"" << this->Status << "\"";
  of << indent << " Operation=\"" << this->GetOperationAsString(this->Operation) << "\"";
  of << indent << " FitSuccess=\"" << this->FitSuccess << "\"";
  of << indent << " ContourLevel=\"" << this->ContourLevel << "\"";
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
  this->SetResidualVolumeNodeID(node->GetResidualVolumeNodeID());
  this->SetMaskVolumeNodeID(node->GetMaskVolumeNodeID());
  this->SetMaskActive(node->GetMaskActive());
  this->SetMode(node->GetMode());
  this->SetNormalize(node->GetNormalize());
  this->SetNumberOfRings(node->GetNumberOfRings());
  this->SetRadSep(node->GetRadSep());
  this->SetXCenter(node->GetXCenter());
  this->SetYCenter(node->GetYCenter());
  this->SetSystemicVelocity(node->GetSystemicVelocity());
  this->SetRotationVelocity(node->GetRotationVelocity());
  this->SetRadialVelocity(node->GetRadialVelocity());
  this->SetVelocityDispersion(node->GetVelocityDispersion());
  this->SetInclination(node->GetInclination());
  this->SetInclinationError(node->GetInclinationError());
  this->SetPositionAngle(node->GetPositionAngle());
  this->SetPositionAngleError(node->GetPositionAngleError());
  this->SetScaleHeight(node->GetScaleHeight());
  this->SetColumnDensity(node->GetColumnDensity());
  this->SetDistance(node->GetDistance());

  this->SetPositionAngleFit(node->GetPositionAngleFit());
  this->SetRotationVelocityFit(node->GetRotationVelocityFit());
  this->SetRadialVelocityFit(node->GetRadialVelocityFit());
  this->SetVelocityDispersionFit(node->GetVelocityDispersionFit());
  this->SetInclinationFit(node->GetInclinationFit());
  this->SetXCenterFit(node->GetXCenterFit());
  this->SetYCenterFit(node->GetYCenterFit());
  this->SetSystemicVelocityFit(node->GetSystemicVelocityFit());
  this->SetScaleHeightFit(node->GetScaleHeightFit());
  this->SetLayerType(node->GetLayerType());
  this->SetFittingFunction(node->GetFittingFunction());
  this->SetWeightingFunction(node->GetWeightingFunction());
  this->SetNumberOfClounds(node->GetNumberOfClounds());
  this->SetCloudsColumnDensity(node->GetCloudsColumnDensity());
  this->SetOutputSerial(node->GetOutputSerial());
  this->SetStatus(node->GetStatus());
  this->SetOperation(node->GetOperation());
  this->SetFitSuccess(node->GetFitSuccess());
  this->SetContourLevel(node->GetContourLevel());
  this->SetXPosCenterIJK(node->GetXPosCenterIJK());
  this->SetYPosCenterIJK(node->GetYPosCenterIJK());
  this->SetXPosCenterRAS(node->GetXPosCenterRAS());
  this->SetYPosCenterRAS(node->GetYPosCenterRAS());
  this->SetZPosCenterRAS(node->GetZPosCenterRAS());
  this->SetPVPhi(node->GetPVPhi());
  this->SetYellowRotOldValue(node->GetYellowRotOldValue());
  this->SetYellowRotValue(node->GetYellowRotValue());
  this->SetGreenRotOldValue(node->GetGreenRotOldValue());
  this->SetGreenRotValue(node->GetGreenRotValue());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroModelingParametersNode::SetYellowRotValue(double rot)
{
  this->YellowRotValue = rot;
  this->InvokeCustomModifiedEvent(vtkMRMLAstroModelingParametersNode::YellowRotationModifiedEvent);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroModelingParametersNode::SetGreenRotValue(double rot)
{
  this->GreenRotValue = rot;
  this->InvokeCustomModifiedEvent(vtkMRMLAstroModelingParametersNode::GreenRotationModifiedEvent);
}

//----------------------------------------------------------------------------
const char *vtkMRMLAstroModelingParametersNode::GetOperationAsString(int id)
{
  switch (id)
    {
    case ESTIMATE: return "Estimate";
    case CREATE: return "Create";
    case FIT: return "Fit";
    default:
      // invalid id
      return "";
    }
}

//----------------------------------------------------------------------------
int vtkMRMLAstroModelingParametersNode::GetOperationFromString(const char *name)
{
  if (name == NULL)
    {
    // invalid name
    return -1;
    }
  for (int ii = 0; ii < 3; ii++)
    {
    if (strcmp(name, GetOperationAsString(ii)) == 0)
      {
      // found a matching name
      return ii;
      }
    }
  // unknown name
  return -1;
}

//----------------------------------------------------------------------------
vtkMRMLTableNode *vtkMRMLAstroModelingParametersNode::GetParamsTableNode()
{
  if (!this->Scene)
    {
    return NULL;
    }

  return vtkMRMLTableNode::SafeDownCast(this->GetNodeReference(this->GetTableNodeReferenceRole()));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroModelingParametersNode::SetParamsTableNode(vtkMRMLTableNode* node)
{
  this->SetNodeReferenceID(this->GetTableNodeReferenceRole(), (node ? node->GetID() : NULL));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroModelingParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << "OutputVolumeNodeID: " << ( (this->OutputVolumeNodeID) ? this->OutputVolumeNodeID : "None" ) << "\n";
  os << "ResidualVolumeNodeID: " << ( (this->ResidualVolumeNodeID) ? this->ResidualVolumeNodeID : "None" ) << "\n";
  os << "MaskVolumeNodeID: " << ( (this->MaskVolumeNodeID) ? this->MaskVolumeNodeID : "None" ) << "\n";
  os << "MaskActive: " << this->MaskActive << "\n";
  os << "Mode: " << ( (this->Mode) ? this->Mode : "None" ) << "\n";
  os << "Normalize: " << ( (this->Normalize) ? this->Normalize : "None" ) << "\n";
  os << "OutputSerial: " << this->OutputSerial << "\n";
  os << "NumberOfRings: " << this->NumberOfRings << "\n";
  os << "RadSep: " << this->RadSep << "\n";
  os << "XCenter: " << this->XCenter << "\n";
  os << "YCenter: " << this->YCenter << "\n";
  os << "SystemicVelocity: " << this->SystemicVelocity << "\n";
  os << "RotationVelocity: " << this->RotationVelocity << "\n";
  os << "RadialVelocity: " << this->RadialVelocity << "\n";
  os << "VelocityDispersion: " << this->VelocityDispersion << "\n";
  os << "InclinationError: " << this->InclinationError << "\n";
  os << "PositionAngle: " << this->PositionAngle << "\n";
  os << "PositionAngleError: " << this->PositionAngleError << "\n";
  os << "ScaleHeight: " << this->ScaleHeight << "\n";
  os << "ColumnDensity: " << this->ColumnDensity << "\n";
  os << "Distance: " << this->Distance << "\n";

  os << "PositionAngleFit: " << this->PositionAngleFit << "\n";
  os << "RotationVelocityFit: " << this->RotationVelocityFit << "\n";
  os << "RadialVelocityFit: " << this->RadialVelocityFit << "\n";
  os << "VelocityDispersionFit: " << this->VelocityDispersionFit << "\n";
  os << "InclinationFit: " << this->InclinationFit << "\n";
  os << "XCenterFit: " << this->XCenterFit << "\n";
  os << "YCenterFit: " << this->YCenterFit << "\n";
  os << "SystemicVelocityFit: " << this->SystemicVelocityFit << "\n";
  os << "ScaleHeightFit: " << this->ScaleHeightFit << "\n";
  os << "LayerType: " << this->LayerType << "\n";
  os << "FittingFunction: " << this->FittingFunction << "\n";
  os << "WeightingFunction: " << this->WeightingFunction << "\n";
  os << "NumberOfClounds: " << this->NumberOfClounds << "\n";
  os << "CloudsColumnDensity: " << this->CloudsColumnDensity << "\n";
  os << "Status: " << this->Status << "\n";
  os << "Operation: " << this->GetOperationAsString(this->Operation) << "\n";
  os << "FitSuccess: " << this->FitSuccess << "\n";
  os << "ContourLevel: " << this->ContourLevel << "\n";
  os << "XPosCenterIJK: " << this->XPosCenterIJK << "\n";
  os << "YPosCenterIJK: " << this->YPosCenterIJK << "\n";
  os << "XPosCenterRAS: " << this->XPosCenterRAS << "\n";
  os << "YPosCenterRAS: " << this->YPosCenterRAS << "\n";
  os << "ZPosCenterRAS: " << this->ZPosCenterRAS << "\n";
  os << "PVPhi: " << this->PVPhi << "\n";
  os << "YellowRotOldValue: " << this->YellowRotOldValue << "\n";
  os << "YellowRotValue: " << this->YellowRotValue << "\n";
  os << "GreenRotOldValue: " << this->GreenRotOldValue << "\n";
  os << "GreenRotValue: " << this->GreenRotValue << "\n";
}
