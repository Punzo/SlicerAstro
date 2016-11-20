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

#include <vtkCollection.h>
#include <vtkCollectionIterator.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// MRML includes
#include <vtkMRMLChartNode.h>
#include <vtkMRMLDoubleArrayNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLVolumeNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroModelingParametersNode.h>

//------------------------------------------------------------------------------
const char* vtkMRMLAstroModelingParametersNode::PARAMS_TABLE_REFERENCE_ROLE = "paramsTableRef";
const char* vtkMRMLAstroModelingParametersNode::CHART_XPOS_REFERENCE_ROLE = "chartXPosRef";
const char* vtkMRMLAstroModelingParametersNode::CHART_YPOS_REFERENCE_ROLE = "chartYPosRef";
const char* vtkMRMLAstroModelingParametersNode::CHART_VSYS_REFERENCE_ROLE = "chartVSysRef";
const char* vtkMRMLAstroModelingParametersNode::CHART_VROT_REFERENCE_ROLE = "chartVRotRef";
const char* vtkMRMLAstroModelingParametersNode::CHART_VDISP_REFERENCE_ROLE = "chartVDispRef";
const char* vtkMRMLAstroModelingParametersNode::CHART_DENS_REFERENCE_ROLE = "chartDensRef";
const char* vtkMRMLAstroModelingParametersNode::CHART_Z0_REFERENCE_ROLE = "chartZ0Ref";
const char* vtkMRMLAstroModelingParametersNode::CHART_INC_REFERENCE_ROLE = "chartIncRef";
const char* vtkMRMLAstroModelingParametersNode::CHART_PHI_REFERENCE_ROLE = "chartPhiRef";
const char* vtkMRMLAstroModelingParametersNode::ARRAY_XPOS_REFERENCE_ROLE = "arrayXPosRef";
const char* vtkMRMLAstroModelingParametersNode::ARRAY_YPOS_REFERENCE_ROLE = "arrayYPosRef";
const char* vtkMRMLAstroModelingParametersNode::ARRAY_VSYS_REFERENCE_ROLE = "arrayVSysRef";
const char* vtkMRMLAstroModelingParametersNode::ARRAY_VROT_REFERENCE_ROLE = "arrayVRotRef";
const char* vtkMRMLAstroModelingParametersNode::ARRAY_VDISP_REFERENCE_ROLE = "arrayVDispRef";
const char* vtkMRMLAstroModelingParametersNode::ARRAY_DENS_REFERENCE_ROLE = "arrayDensRef";
const char* vtkMRMLAstroModelingParametersNode::ARRAY_Z0_REFERENCE_ROLE = "arrayZ0Ref";
const char* vtkMRMLAstroModelingParametersNode::ARRAY_INC_REFERENCE_ROLE = "arrayIncRef";
const char* vtkMRMLAstroModelingParametersNode::ARRAY_PHI_REFERENCE_ROLE = "arrayPhiRef";
const char* vtkMRMLAstroModelingParametersNode::FIRST_ARRAY_XPOS_REFERENCE_ROLE = "firstArrayXPosRef";
const char* vtkMRMLAstroModelingParametersNode::FIRST_ARRAY_YPOS_REFERENCE_ROLE = "firstArrayYPosRef";
const char* vtkMRMLAstroModelingParametersNode::FIRST_ARRAY_VSYS_REFERENCE_ROLE = "firstArrayVSysRef";
const char* vtkMRMLAstroModelingParametersNode::FIRST_ARRAY_VROT_REFERENCE_ROLE = "firstArrayVRotRef";
const char* vtkMRMLAstroModelingParametersNode::FIRST_ARRAY_VDISP_REFERENCE_ROLE = "firstArrayVDispRef";
const char* vtkMRMLAstroModelingParametersNode::FIRST_ARRAY_DENS_REFERENCE_ROLE = "firstArrayDensRef";
const char* vtkMRMLAstroModelingParametersNode::FIRST_ARRAY_Z0_REFERENCE_ROLE = "firstArrayZ0Ref";
const char* vtkMRMLAstroModelingParametersNode::FIRST_ARRAY_INC_REFERENCE_ROLE = "firstArrayIncRef";
const char* vtkMRMLAstroModelingParametersNode::FIRST_ARRAY_PHI_REFERENCE_ROLE = "firstArrayPhiRef";
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
  this->SetMaskActive(true);
  this->OutputSerial = 1;
  this->Mode = NULL;
  this->SetMode("Automatic");
  this->SetRadSep(0.);
  this->SetXCenter(0.);
  this->SetYCenter(0.);
  this->SetSystemicVelocity(0.);
  this->SetRotationVelocity(0.);
  this->SetVelocityDispersion(0.);
  this->SetInclination(0.);
  this->SetInclinationError(5.);
  this->SetPositionAngle(0.);
  this->SetPositionAngleError(15.);
  this->SetScaleHeight(0.);
  this->SetDistance(0.);
  this->SetColumnDensity(1.);
  this->SetPositionAngleFit(true);
  this->SetRotationVelocityFit(true);
  this->SetVelocityDispersionFit(true);
  this->SetInclinationFit(true);
  this->SetXCenterFit(false);
  this->SetYCenterFit(false);
  this->SetSystemicVelocityFit(false);
  this->SetScaleHeightFit(false);
  this->SetLayerType(0);
  this->SetFittingFunction(1);
  this->SetWeightingFunction(1);
  this->SetNumberOfClounds(0);
  this->SetCloudsColumnDensity(10.);
  this->SetStatus(0);
  this->SetFitSuccess(false);
  this->SetFirstPlot(true);
  this->SetNumberOfRings(0);
  this->SetContourLevel(3.);

  this->chartNodes = vtkSmartPointer<vtkCollection>::New();
  this->arrayNodes = vtkSmartPointer<vtkCollection>::New();
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

  this->chartNodes->RemoveAllItems();
  this->arrayNodes->RemoveAllItems();
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

    if (!strcmp(attName, "FirstPlot"))
      {
      this->FirstPlot = StringToInt(attValue);
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

  of << indent << " MaskActive=\"" << this->MaskActive << "\"";
  of << indent << " OutputSerial=\"" << this->OutputSerial << "\"";
  of << indent << " NumberOfRings=\"" << this->NumberOfRings << "\"";
  of << indent << " RadSep=\"" << this->RadSep << "\"";
  of << indent << " XCenter=\"" << this->XCenter << "\"";
  of << indent << " YCenter=\"" << this->YCenter << "\"";
  of << indent << " SystemicVelocity=\"" << this->SystemicVelocity << "\"";
  of << indent << " RotationVelocity=\"" << this->RotationVelocity << "\"";
  of << indent << " VelocityDispersion=\"" << this->VelocityDispersion << "\"";
  of << indent << " InclinationError=\"" << this->InclinationError << "\"";
  of << indent << " PositionAngle=\"" << this->PositionAngle << "\"";
  of << indent << " PositionAngleError=\"" << this->PositionAngleError << "\"";
  of << indent << " ScaleHeight=\"" << this->ScaleHeight << "\"";
  of << indent << " ColumnDensity=\"" << this->ColumnDensity << "\"";
  of << indent << " Distance=\"" << this->Distance << "\"";
  of << indent << " PositionAngleFit=\"" << this->PositionAngleFit << "\"";
  of << indent << " RotationVelocityFit=\"" << this->RotationVelocityFit << "\"";
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
  of << indent << " Status=\"" << this->Status << "\"";
  of << indent << " FitSuccess=\"" << this->FitSuccess << "\"";
  of << indent << " FirstPlot=\"" << this->FirstPlot << "\"";
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
  this->SetNumberOfRings(node->GetNumberOfRings());
  this->SetRadSep(node->GetRadSep());
  this->SetXCenter(node->GetXCenter());
  this->SetYCenter(node->GetYCenter());
  this->SetSystemicVelocity(node->GetSystemicVelocity());
  this->SetRotationVelocity(node->GetRotationVelocity());
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
  this->SetFitSuccess(node->GetFitSuccess());
  this->SetFirstPlot(node->GetFirstPlot());
  this->SetContourLevel(node->GetContourLevel());

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
vtkMRMLTableNode *vtkMRMLAstroModelingParametersNode::GetParamsTableNode()
{
  if (!this->Scene)
    {
    vtkErrorMacro("vtkMRMLAstroModelingParametersNode::GetParamsTableNode : Invalid MRML scene!");
    return NULL;
    }
  vtkMRMLTableNode* paramsTableNode = vtkMRMLTableNode::SafeDownCast(
              this->GetNodeReference(PARAMS_TABLE_REFERENCE_ROLE) );

  if (!paramsTableNode)
    {
    paramsTableNode = vtkMRMLTableNode::New();
    std::string paramsTableNodeName = this->Scene->GenerateUniqueName("ModelingParamsTable");
    paramsTableNode->SetName(paramsTableNodeName.c_str());
    this->Scene->AddNode(paramsTableNode);
    this->SetAndObserveParamsTableNode(paramsTableNode);
    paramsTableNode->Delete();
    paramsTableNode = vtkMRMLTableNode::SafeDownCast(
                      this->GetNodeReference(PARAMS_TABLE_REFERENCE_ROLE) );
    }
  return paramsTableNode;
}

//----------------------------------------------------------------------------
vtkCollection *vtkMRMLAstroModelingParametersNode::GetChartNodes()
{
  if (!this->Scene)
    {
    vtkErrorMacro("vtkMRMLAstroModelingParametersNode::GetChartNodes : Invalid MRML scene!");
    return NULL;
    }

  this->chartNodes->RemoveAllItems();

  vtkMRMLChartNode* chartXPosNode = vtkMRMLChartNode::SafeDownCast(
              this->GetNodeReference(CHART_XPOS_REFERENCE_ROLE) );

  if (!chartXPosNode)
    {
    chartXPosNode = vtkMRMLChartNode::New();
    std::string chartXPosNodeName = this->Scene->GenerateUniqueName("chartXPos");
    chartXPosNode->SetName(chartXPosNodeName.c_str());
    chartXPosNode->SetProperty("default", "title", " ");
    chartXPosNode->SetProperty("default", "xAxisLabel", "Radius (arcsec)");
    chartXPosNode->SetProperty("default", "yAxisLabel", "X Center (pixels)");
    this->Scene->AddNode(chartXPosNode);
    this->SetAndObserveChartNode(chartXPosNode, "chartXPos");
    chartXPosNode->Delete();
    chartXPosNode = vtkMRMLChartNode::SafeDownCast(
                  this->GetNodeReference(CHART_XPOS_REFERENCE_ROLE) );
    }

  this->chartNodes->AddItem(chartXPosNode);

  vtkMRMLChartNode* chartYPosNode = vtkMRMLChartNode::SafeDownCast(
              this->GetNodeReference(CHART_YPOS_REFERENCE_ROLE) );

  if (!chartYPosNode)
    {
    chartYPosNode = vtkMRMLChartNode::New();
    std::string chartYPosName = this->Scene->GenerateUniqueName("chartYPos");
    chartYPosNode->SetName(chartYPosName.c_str());
    chartYPosNode->SetProperty("default", "title", " ");
    chartYPosNode->SetProperty("default", "xAxisLabel", "Radius (arcsec)");
    chartYPosNode->SetProperty("default", "yAxisLabel", "Y Center (pixels)");
    this->Scene->AddNode(chartYPosNode);
    this->SetAndObserveChartNode(chartYPosNode, "chartYPos");
    chartYPosNode->Delete();
    chartYPosNode = vtkMRMLChartNode::SafeDownCast(
                  this->GetNodeReference(CHART_YPOS_REFERENCE_ROLE) );
    }
  this->chartNodes->AddItem(chartYPosNode);

  vtkMRMLChartNode* chartVSysNode = vtkMRMLChartNode::SafeDownCast(
              this->GetNodeReference(CHART_VSYS_REFERENCE_ROLE) );

  if (!chartVSysNode)
    {
    chartVSysNode = vtkMRMLChartNode::New();
    std::string chartVSysNodeName = this->Scene->GenerateUniqueName("chartVSys");
    chartVSysNode->SetName(chartVSysNodeName.c_str());
    chartVSysNode->SetProperty("default", "title", " ");
    chartVSysNode->SetProperty("default", "xAxisLabel", "Radius (arcsec)");
    chartVSysNode->SetProperty("default", "yAxisLabel", "Systemic Velocity (km/s)");
    this->Scene->AddNode(chartVSysNode);
    this->SetAndObserveChartNode(chartVSysNode, "chartVSys");
    chartVSysNode->Delete();
    chartVSysNode = vtkMRMLChartNode::SafeDownCast(
                  this->GetNodeReference(CHART_VSYS_REFERENCE_ROLE) );
    }
  this->chartNodes->AddItem(chartVSysNode);

  vtkMRMLChartNode* chartVRotNode = vtkMRMLChartNode::SafeDownCast(
              this->GetNodeReference(CHART_VROT_REFERENCE_ROLE) );

  if (!chartVRotNode)
    {
    chartVRotNode = vtkMRMLChartNode::New();
    std::string chartVRotNodeName = this->Scene->GenerateUniqueName("chartVRot");
    chartVRotNode->SetName(chartVRotNodeName.c_str());
    chartVRotNode->SetProperty("default", "title", " ");
    chartVRotNode->SetProperty("default", "xAxisLabel", "Radius (arcsec)");
    chartVRotNode->SetProperty("default", "yAxisLabel", "Rotational Velocity (km/s)");
    this->Scene->AddNode(chartVRotNode);
    this->SetAndObserveChartNode(chartVRotNode, "chartVRot");
    chartVRotNode->Delete();
    chartVRotNode = vtkMRMLChartNode::SafeDownCast(
                  this->GetNodeReference(CHART_VROT_REFERENCE_ROLE) );
    }
  this->chartNodes->AddItem(chartVRotNode);

  vtkMRMLChartNode* chartVDispNode = vtkMRMLChartNode::SafeDownCast(
              this->GetNodeReference(CHART_VDISP_REFERENCE_ROLE) );

  if (!chartVDispNode)
    {
    chartVDispNode = vtkMRMLChartNode::New();
    std::string chartVDispNodeName = this->Scene->GenerateUniqueName("chartVDisp");
    chartVDispNode->SetName(chartVDispNodeName.c_str());
    chartVDispNode->SetProperty("default", "title", " ");
    chartVDispNode->SetProperty("default", "xAxisLabel", "Radius (arcsec)");
    chartVDispNode->SetProperty("default", "yAxisLabel", "Dispersion Velocity (km/s)");
    this->Scene->AddNode(chartVDispNode);
    this->SetAndObserveChartNode(chartVDispNode, "chartVDisp");
    chartVDispNode->Delete();
    chartVDispNode = vtkMRMLChartNode::SafeDownCast(
                  this->GetNodeReference(CHART_VDISP_REFERENCE_ROLE) );
    }
  this->chartNodes->AddItem(chartVDispNode);

  vtkMRMLChartNode* chartDensNode = vtkMRMLChartNode::SafeDownCast(
              this->GetNodeReference(CHART_DENS_REFERENCE_ROLE) );

  if (!chartDensNode)
    {
    chartDensNode = vtkMRMLChartNode::New();
    std::string chartDensNodeName = this->Scene->GenerateUniqueName("chartDens");
    chartDensNode->SetName(chartDensNodeName.c_str());
    chartDensNode->SetProperty("default", "title", " ");
    chartDensNode->SetProperty("default", "xAxisLabel", "Radius (arcsec)");
    chartDensNode->SetProperty("default", "yAxisLabel", "Column Density (10^20 cm^-2)");
    this->Scene->AddNode(chartDensNode);
    this->SetAndObserveChartNode(chartDensNode, "chartDens");
    chartDensNode->Delete();
    chartDensNode = vtkMRMLChartNode::SafeDownCast(
                  this->GetNodeReference(CHART_DENS_REFERENCE_ROLE) );
    }
  this->chartNodes->AddItem(chartDensNode);

  vtkMRMLChartNode* chartZ0Node = vtkMRMLChartNode::SafeDownCast(
              this->GetNodeReference(CHART_Z0_REFERENCE_ROLE) );

  if (!chartZ0Node)
    {
    chartZ0Node = vtkMRMLChartNode::New();
    std::string chartZ0NodeName = this->Scene->GenerateUniqueName("chartZ0");
    chartZ0Node->SetName(chartZ0NodeName.c_str());
    chartZ0Node->SetProperty("default", "title", " ");
    chartZ0Node->SetProperty("default", "xAxisLabel", "Radius (arcsec)");
    chartZ0Node->SetProperty("default", "yAxisLabel", "Scale Heigth (Kpc)");
    this->Scene->AddNode(chartZ0Node);
    this->SetAndObserveChartNode(chartZ0Node, "chartZ0");
    chartZ0Node->Delete();
    chartZ0Node = vtkMRMLChartNode::SafeDownCast(
                  this->GetNodeReference(CHART_Z0_REFERENCE_ROLE) );
    }
  this->chartNodes->AddItem(chartZ0Node);

  vtkMRMLChartNode* chartIncNode = vtkMRMLChartNode::SafeDownCast(
              this->GetNodeReference(CHART_INC_REFERENCE_ROLE) );

  if (!chartIncNode)
    {
    chartIncNode = vtkMRMLChartNode::New();
    std::string chartIncNodeName = this->Scene->GenerateUniqueName("chartInc");
    chartIncNode->SetName(chartIncNodeName.c_str());
    chartIncNode->SetProperty("default", "title", " ");
    chartIncNode->SetProperty("default", "xAxisLabel", "Radius (arcsec)");
    chartIncNode->SetProperty("default", "yAxisLabel", "Inclination (degree)");
    this->Scene->AddNode(chartIncNode);
    this->SetAndObserveChartNode(chartIncNode, "chartInc");
    chartIncNode->Delete();
    chartIncNode = vtkMRMLChartNode::SafeDownCast(
                  this->GetNodeReference(CHART_INC_REFERENCE_ROLE) );
    }
  this->chartNodes->AddItem(chartIncNode);

  vtkMRMLChartNode* chartPhiNode = vtkMRMLChartNode::SafeDownCast(
              this->GetNodeReference(CHART_PHI_REFERENCE_ROLE) );

  if (!chartPhiNode)
    {
    chartPhiNode = vtkMRMLChartNode::New();
    std::string chartPhiNodeName = this->Scene->GenerateUniqueName("chartPhi");
    chartPhiNode->SetName(chartPhiNodeName.c_str());
    chartPhiNode->SetProperty("default", "title", " ");
    chartPhiNode->SetProperty("default", "xAxisLabel", "Radius (arcsec)");
    chartPhiNode->SetProperty("default", "yAxisLabel", "Orientation Angle (degree)");
    this->Scene->AddNode(chartPhiNode);
    this->SetAndObserveChartNode(chartPhiNode, "chartPhi");
    chartPhiNode->Delete();
    chartPhiNode = vtkMRMLChartNode::SafeDownCast(
                  this->GetNodeReference(CHART_PHI_REFERENCE_ROLE) );
    }
  this->chartNodes->AddItem(chartPhiNode);

  return this->chartNodes;
}

//----------------------------------------------------------------------------
vtkCollection *vtkMRMLAstroModelingParametersNode::GetArrayNodes()
{
  if (!this->Scene)
    {
    vtkErrorMacro("vtkMRMLAstroModelingParametersNode::GetChartNodes : Invalid MRML scene!");
    return NULL;
    }

  this->arrayNodes->RemoveAllItems();

  vtkMRMLDoubleArrayNode* arrayXPosNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(ARRAY_XPOS_REFERENCE_ROLE) );

  if (!arrayXPosNode)
    {
    arrayXPosNode = vtkMRMLDoubleArrayNode::New();
    std::string arrayXPosNodeName = this->Scene->GenerateUniqueName("arrayXPos");
    arrayXPosNode->SetName(arrayXPosNodeName.c_str());
    vtkDoubleArray *data = arrayXPosNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(arrayXPosNode);
    this->SetAndObserveArrayNode(arrayXPosNode, "arrayXPos");
    arrayXPosNode->Delete();
    arrayXPosNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(ARRAY_XPOS_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(arrayXPosNode);

  vtkMRMLDoubleArrayNode* firstArrayXPosNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(FIRST_ARRAY_XPOS_REFERENCE_ROLE) );

  if (!firstArrayXPosNode)
    {
    firstArrayXPosNode = vtkMRMLDoubleArrayNode::New();
    std::string firstArrayXPosNodeName = this->Scene->GenerateUniqueName("firstArrayXPos");
    firstArrayXPosNode->SetName(firstArrayXPosNodeName.c_str());
    vtkDoubleArray *data = firstArrayXPosNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(firstArrayXPosNode);
    this->SetAndObserveArrayNode(firstArrayXPosNode, "firstArrayXPos");
    firstArrayXPosNode->Delete();
    firstArrayXPosNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(FIRST_ARRAY_XPOS_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(firstArrayXPosNode);


  vtkMRMLDoubleArrayNode* arrayYPosNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(ARRAY_YPOS_REFERENCE_ROLE) );

  if (!arrayYPosNode)
    {
    arrayYPosNode = vtkMRMLDoubleArrayNode::New();
    std::string arrayYPosNodeName = this->Scene->GenerateUniqueName("arrayYPos");
    arrayYPosNode->SetName(arrayYPosNodeName.c_str());
    vtkDoubleArray *data = arrayYPosNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(arrayYPosNode);
    this->SetAndObserveArrayNode(arrayYPosNode, "arrayYPos");
    arrayYPosNode->Delete();
    arrayYPosNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(ARRAY_YPOS_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(arrayYPosNode);

  vtkMRMLDoubleArrayNode* firstArrayYPosNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(FIRST_ARRAY_YPOS_REFERENCE_ROLE) );

  if (!firstArrayYPosNode)
    {
    firstArrayYPosNode = vtkMRMLDoubleArrayNode::New();
    std::string firstArrayYPosNodeName = this->Scene->GenerateUniqueName("firstArrayYPos");
    firstArrayYPosNode->SetName(firstArrayYPosNodeName.c_str());
    vtkDoubleArray *data = firstArrayYPosNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(firstArrayYPosNode);
    this->SetAndObserveArrayNode(firstArrayYPosNode, "firstArrayYPos");
    firstArrayYPosNode->Delete();
    firstArrayYPosNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(FIRST_ARRAY_YPOS_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(firstArrayYPosNode);


  vtkMRMLDoubleArrayNode* arrayVSysNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(ARRAY_VSYS_REFERENCE_ROLE) );

  if (!arrayVSysNode)
    {
    arrayVSysNode = vtkMRMLDoubleArrayNode::New();
    std::string arrayVSysNodeName = this->Scene->GenerateUniqueName("arrayVSys");
    arrayVSysNode->SetName(arrayVSysNodeName.c_str());
    vtkDoubleArray *data = arrayVSysNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(arrayVSysNode);
    this->SetAndObserveArrayNode(arrayVSysNode, "arrayVSys");
    arrayVSysNode->Delete();
    arrayVSysNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(ARRAY_VSYS_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(arrayVSysNode);

  vtkMRMLDoubleArrayNode* firstArrayVSysNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(FIRST_ARRAY_VSYS_REFERENCE_ROLE) );

  if (!firstArrayVSysNode)
    {
    firstArrayVSysNode = vtkMRMLDoubleArrayNode::New();
    std::string firstArrayVSysNodeName = this->Scene->GenerateUniqueName("firstArrayVSys");
    firstArrayVSysNode->SetName(firstArrayVSysNodeName.c_str());
    vtkDoubleArray *data = firstArrayVSysNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(firstArrayVSysNode);
    this->SetAndObserveArrayNode(firstArrayVSysNode, "firstArrayVSys");
    firstArrayVSysNode->Delete();
    firstArrayVSysNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(FIRST_ARRAY_VSYS_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(firstArrayVSysNode);


  vtkMRMLDoubleArrayNode* arrayVRotNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(ARRAY_VROT_REFERENCE_ROLE) );

  if (!arrayVRotNode)
    {
    arrayVRotNode = vtkMRMLDoubleArrayNode::New();
    std::string arrayVRotNodeName = this->Scene->GenerateUniqueName("arrayVRot");
    arrayVRotNode->SetName(arrayVRotNodeName.c_str());
    vtkDoubleArray *data = arrayVRotNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(arrayVRotNode);
    this->SetAndObserveArrayNode(arrayVRotNode, "arrayVRot");
    arrayVRotNode->Delete();
    arrayVRotNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(ARRAY_VROT_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(arrayVRotNode);

  vtkMRMLDoubleArrayNode* firstArrayVRotNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(FIRST_ARRAY_VROT_REFERENCE_ROLE) );

  if (!firstArrayVRotNode)
    {
    firstArrayVRotNode = vtkMRMLDoubleArrayNode::New();
    std::string firstArrayVRotNodeName = this->Scene->GenerateUniqueName("firstArrayVRot");
    firstArrayVRotNode->SetName(firstArrayVRotNodeName.c_str());
    vtkDoubleArray *data = firstArrayVRotNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(firstArrayVRotNode);
    this->SetAndObserveArrayNode(firstArrayVRotNode, "firstArrayVRot");
    firstArrayVRotNode->Delete();
    firstArrayVRotNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(FIRST_ARRAY_VROT_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(firstArrayVRotNode);


  vtkMRMLDoubleArrayNode* arrayVDispNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(ARRAY_VDISP_REFERENCE_ROLE) );

  if (!arrayVDispNode)
    {
    arrayVDispNode = vtkMRMLDoubleArrayNode::New();
    std::string arrayVDispNodeName = this->Scene->GenerateUniqueName("arrayVDisp");
    arrayVDispNode->SetName(arrayVDispNodeName.c_str());
    vtkDoubleArray *data = arrayVDispNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(arrayVDispNode);
    this->SetAndObserveArrayNode(arrayVDispNode, "arrayVDisp");
    arrayVDispNode->Delete();
    arrayVDispNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(ARRAY_VDISP_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(arrayVDispNode);

  vtkMRMLDoubleArrayNode* firstArrayVDispNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(FIRST_ARRAY_VDISP_REFERENCE_ROLE) );

  if (!firstArrayVDispNode)
    {
    firstArrayVDispNode = vtkMRMLDoubleArrayNode::New();
    std::string firstArrayVDispNodeName = this->Scene->GenerateUniqueName("firstArrayVDisp");
    firstArrayVDispNode->SetName(firstArrayVDispNodeName.c_str());
    vtkDoubleArray *data = firstArrayVDispNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(firstArrayVDispNode);
    this->SetAndObserveArrayNode(firstArrayVDispNode, "firstArrayVDisp");
    firstArrayVDispNode->Delete();
    firstArrayVDispNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(FIRST_ARRAY_VDISP_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(firstArrayVDispNode);


  vtkMRMLDoubleArrayNode* arrayDensNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(ARRAY_DENS_REFERENCE_ROLE) );

  if (!arrayDensNode)
    {
    arrayDensNode = vtkMRMLDoubleArrayNode::New();
    std::string arrayDensNodeName = this->Scene->GenerateUniqueName("arrayDens");
    arrayDensNode->SetName(arrayDensNodeName.c_str());
    vtkDoubleArray *data = arrayDensNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(arrayDensNode);
    this->SetAndObserveArrayNode(arrayDensNode, "arrayDens");
    arrayDensNode->Delete();
    arrayDensNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(ARRAY_DENS_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(arrayDensNode);

  vtkMRMLDoubleArrayNode* firstArrayDensNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(FIRST_ARRAY_DENS_REFERENCE_ROLE) );

  if (!firstArrayDensNode)
    {
    firstArrayDensNode = vtkMRMLDoubleArrayNode::New();
    std::string firstArrayDensNodeName = this->Scene->GenerateUniqueName("firstArrayDens");
    firstArrayDensNode->SetName(firstArrayDensNodeName.c_str());
    vtkDoubleArray *data = firstArrayDensNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(firstArrayDensNode);
    this->SetAndObserveArrayNode(firstArrayDensNode, "firstArrayDens");
    firstArrayDensNode->Delete();
    firstArrayDensNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(FIRST_ARRAY_DENS_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(firstArrayDensNode);


  vtkMRMLDoubleArrayNode* arrayZ0Node = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(ARRAY_Z0_REFERENCE_ROLE) );

  if (!arrayZ0Node)
    {
    arrayZ0Node = vtkMRMLDoubleArrayNode::New();
    std::string arrayZ0NodeName = this->Scene->GenerateUniqueName("arrayZ0");
    arrayZ0Node->SetName(arrayZ0NodeName.c_str());
    vtkDoubleArray *data = arrayZ0Node->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(arrayZ0Node);
    this->SetAndObserveArrayNode(arrayZ0Node, "arrayZ0");
    arrayZ0Node->Delete();
    arrayZ0Node = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(ARRAY_Z0_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(arrayZ0Node);

  vtkMRMLDoubleArrayNode* firstArrayZ0Node = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(FIRST_ARRAY_Z0_REFERENCE_ROLE) );

  if (!firstArrayZ0Node)
    {
    firstArrayZ0Node = vtkMRMLDoubleArrayNode::New();
    std::string firstArrayZ0NodeName = this->Scene->GenerateUniqueName("firstArrayZ0");
    firstArrayZ0Node->SetName(firstArrayZ0NodeName.c_str());
    vtkDoubleArray *data = firstArrayZ0Node->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(firstArrayZ0Node);
    this->SetAndObserveArrayNode(firstArrayZ0Node, "firstArrayZ0");
    firstArrayZ0Node->Delete();
    firstArrayZ0Node = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(FIRST_ARRAY_Z0_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(firstArrayZ0Node);


  vtkMRMLDoubleArrayNode* arrayIncNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(ARRAY_INC_REFERENCE_ROLE) );

  if (!arrayIncNode)
    {
    arrayIncNode = vtkMRMLDoubleArrayNode::New();
    std::string arrayIncNodeName = this->Scene->GenerateUniqueName("arrayInc");
    arrayIncNode->SetName(arrayIncNodeName.c_str());
    vtkDoubleArray *data = arrayIncNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(arrayIncNode);
    this->SetAndObserveArrayNode(arrayIncNode, "arrayInc");
    arrayIncNode->Delete();
    arrayIncNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                this->GetNodeReference(ARRAY_INC_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(arrayIncNode);

  vtkMRMLDoubleArrayNode* firstArrayIncNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(FIRST_ARRAY_INC_REFERENCE_ROLE) );

  if (!firstArrayIncNode)
    {
    firstArrayIncNode = vtkMRMLDoubleArrayNode::New();
    std::string firstArrayIncNodeName = this->Scene->GenerateUniqueName("firstArrayInc");
    firstArrayIncNode->SetName(firstArrayIncNodeName.c_str());
    vtkDoubleArray *data = firstArrayIncNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(firstArrayIncNode);
    this->SetAndObserveArrayNode(firstArrayIncNode, "firstArrayInc");
    firstArrayIncNode->Delete();
    firstArrayIncNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                this->GetNodeReference(FIRST_ARRAY_INC_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(firstArrayIncNode);


  vtkMRMLDoubleArrayNode* arrayPhiNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(ARRAY_PHI_REFERENCE_ROLE) );

  if (!arrayPhiNode)
    {
    arrayPhiNode = vtkMRMLDoubleArrayNode::New();
    std::string arrayPhiNodeName = this->Scene->GenerateUniqueName("arrayPhi");
    arrayPhiNode->SetName(arrayPhiNodeName.c_str());
    vtkDoubleArray *data = arrayPhiNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(arrayPhiNode);
    this->SetAndObserveArrayNode(arrayPhiNode, "arrayPhi");
    arrayPhiNode->Delete();
    arrayPhiNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(ARRAY_PHI_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(arrayPhiNode);

  vtkMRMLDoubleArrayNode* firstArrayPhiNode = vtkMRMLDoubleArrayNode::SafeDownCast(
              this->GetNodeReference(FIRST_ARRAY_PHI_REFERENCE_ROLE) );

  if (!firstArrayPhiNode)
    {
    firstArrayPhiNode = vtkMRMLDoubleArrayNode::New();
    std::string firstArrayPhiNodeName = this->Scene->GenerateUniqueName("firstArrayPhi");
    firstArrayPhiNode->SetName(firstArrayPhiNodeName.c_str());
    vtkDoubleArray *data = firstArrayPhiNode->GetArray();
    data->SetNumberOfComponents(3);
    data->SetNumberOfTuples(this->GetNumberOfRings());
    this->Scene->AddNode(firstArrayPhiNode);
    this->SetAndObserveArrayNode(firstArrayPhiNode, "firstArrayPhi");
    firstArrayPhiNode->Delete();
    firstArrayPhiNode = vtkMRMLDoubleArrayNode::SafeDownCast(
                    this->GetNodeReference(FIRST_ARRAY_PHI_REFERENCE_ROLE) );
    }
  this->arrayNodes->AddItem(firstArrayPhiNode);

  return this->arrayNodes;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroModelingParametersNode::SetAndObserveParamsTableNode(vtkMRMLTableNode* node)
{
  this->SetNodeReferenceID(PARAMS_TABLE_REFERENCE_ROLE, (node ? node->GetID() : NULL));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroModelingParametersNode::SetAndObserveChartNode(vtkMRMLChartNode *node,
                                                                const char *chartName)
{
  if (!strcmp(chartName, "chartXPos"))
    {
    this->SetNodeReferenceID(CHART_XPOS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(chartName, "chartYPos"))
    {
    this->SetNodeReferenceID(CHART_YPOS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(chartName, "chartVSys"))
    {
    this->SetNodeReferenceID(CHART_VSYS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(chartName, "chartVRot"))
    {
    this->SetNodeReferenceID(CHART_VROT_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(chartName, "chartVDisp"))
    {
    this->SetNodeReferenceID(CHART_VDISP_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(chartName, "chartDens"))
    {
    this->SetNodeReferenceID(CHART_DENS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(chartName, "chartZ0"))
    {
    this->SetNodeReferenceID(CHART_Z0_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(chartName, "chartInc"))
    {
    this->SetNodeReferenceID(CHART_INC_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(chartName, "chartPhi"))
    {
    this->SetNodeReferenceID(CHART_PHI_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else
    {
    vtkErrorMacro("vtkMRMLAstroModelingParametersNode::SetAndObserveChartNode : chartName invalid!");
    return;
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroModelingParametersNode::SetAndObserveArrayNode(vtkMRMLDoubleArrayNode *node, const char *arrayName)
{
  if (!strcmp(arrayName, "arrayXPos"))
    {
    this->SetNodeReferenceID(ARRAY_XPOS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "firstArrayXPos"))
    {
    this->SetNodeReferenceID(FIRST_ARRAY_XPOS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "arrayYPos"))
    {
    this->SetNodeReferenceID(ARRAY_YPOS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "firstArrayYPos"))
    {
    this->SetNodeReferenceID(FIRST_ARRAY_YPOS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "arrayVSys"))
    {
    this->SetNodeReferenceID(ARRAY_VSYS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "firstArrayVSys"))
    {
    this->SetNodeReferenceID(FIRST_ARRAY_VSYS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "arrayVRot"))
    {
    this->SetNodeReferenceID(ARRAY_VROT_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "firstArrayVRot"))
    {
    this->SetNodeReferenceID(FIRST_ARRAY_VROT_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "arrayVDisp"))
    {
    this->SetNodeReferenceID(ARRAY_VDISP_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "firstArrayVDisp"))
    {
    this->SetNodeReferenceID(FIRST_ARRAY_VDISP_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "arrayDens"))
    {
    this->SetNodeReferenceID(ARRAY_DENS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "firstArrayDens"))
    {
    this->SetNodeReferenceID(FIRST_ARRAY_DENS_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "arrayZ0"))
    {
    this->SetNodeReferenceID(ARRAY_Z0_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "firstArrayZ0"))
    {
    this->SetNodeReferenceID(FIRST_ARRAY_Z0_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "arrayInc"))
    {
    this->SetNodeReferenceID(ARRAY_INC_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "firstArrayInc"))
    {
    this->SetNodeReferenceID(FIRST_ARRAY_INC_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "arrayPhi"))
    {
    this->SetNodeReferenceID(ARRAY_PHI_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else if (!strcmp(arrayName, "firstArrayPhi"))
    {
    this->SetNodeReferenceID(FIRST_ARRAY_PHI_REFERENCE_ROLE, (node ? node->GetID() : NULL));
    }
  else
    {
    vtkErrorMacro("vtkMRMLAstroModelingParametersNode::SetAndObserveArrayNode : arrayName invalid!");
    return;
    }
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
  os << "OutputSerial: " << this->OutputSerial << "\n";
  os << "NumberOfRings: " << this->NumberOfRings << "\n";
  os << "RadSep: " << this->RadSep << "\n";
  os << "XCenter: " << this->XCenter << "\n";
  os << "YCenter: " << this->YCenter << "\n";
  os << "SystemicVelocity: " << this->SystemicVelocity << "\n";
  os << "RotationVelocity: " << this->RotationVelocity << "\n";
  os << "VelocityDispersion: " << this->VelocityDispersion << "\n";
  os << "InclinationError: " << this->InclinationError << "\n";
  os << "PositionAngle: " << this->PositionAngle << "\n";
  os << "PositionAngleError: " << this->PositionAngleError << "\n";
  os << "ScaleHeight: " << this->ScaleHeight << "\n";
  os << "ColumnDensity: " << this->ColumnDensity << "\n";
  os << "Distance: " << this->Distance << "\n";

  os << "PositionAngleFit: " << this->PositionAngleFit << "\n";
  os << "RotationVelocityFit: " << this->RotationVelocityFit << "\n";
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
  os << "FitSuccess: " << this->FitSuccess << "\n";
  os << "FirstPlot: " << this->FirstPlot << "\n";
  os << "ContourLevel: " << this->ContourLevel << "\n";
}
