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

// Logic includes
#include "vtkSlicerAstroVolumeLogic.h"
#include "vtkSlicerAstroModelingLogic.h"
#include "vtkSlicerAstroConfigure.h"
#include "vtkSlicerMarkupsLogic.h"

//3DBarolo includes
#include "param.hh"
#include "cube.hh"
#include "header.hh"
#include "galfit.hh"

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLTableNode.h>

// VTK includes
#include <vtkCacheManager.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>
#include <vtkVersion.h>

// STD includes
#include <cassert>
#include <iostream>

// Qt includes
#include <QtDebug>

#include <iostream>
#include <sys/time.h>

//----------------------------------------------------------------------------
class vtkSlicerAstroModelingLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkSlicerMarkupsLogic> MarkupsLogic;
  Param *par;
  Header *head;
  Cube<float> *cubeF;
  Cube<double> *cubeD;
  Model::Galmod<float> *modF;
  Model::Galmod<double> *modD;
  Model::Galfit<float> *fitF;
  Model::Galfit<double> *fitD;
  double totflux_data;
};

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = 0;
  this->MarkupsLogic = 0;
  this->par = NULL;
  this->head = NULL;
  this->cubeF = NULL;
  this->cubeD = NULL;
  this->modF = NULL;
  this->modD = NULL;
  this->fitF = NULL;
  this->fitD = NULL;
  this->totflux_data = 0.;
}

//---------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkInternal::~vtkInternal()
{
  if (head != NULL)
    {
    delete head;
    }
  head = NULL;

  if (par != NULL)
    {
    delete par;
    }
  par = NULL;

  if (cubeF != NULL)
    {
    delete cubeF;
    }
  cubeF = NULL;

  if (cubeD != NULL)
    {
    delete cubeD;
    }
  cubeD = NULL;

  if (modF != NULL)
    {
    delete modF;
    }
  modF = NULL;

  if (modD != NULL)
    {
    delete modD;
    }
  modD = NULL;

  if (fitF != NULL)
    {
    delete fitF;
    }
  fitF = NULL;

  if (fitD != NULL)
    {
    delete fitD;
    }
  fitD = NULL;
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroModelingLogic);

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
long StringToLong(const char* str)
{
  return StringToNumber<long>(str);
}

//----------------------------------------------------------------------------
float StringToFloat(const char* str)
{
  return StringToNumber<float>(str);
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

} // end namespace

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkSlicerAstroModelingLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::~vtkSlicerAstroModelingLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
double vtkSlicerAstroModelingLogic::CalculateCentralVelocity(vtkMRMLAstroVolumeDisplayNode *volumeDisplayNode)
{
  if (!volumeDisplayNode)
    {
    return 0.;
    }

  vtkMRMLAstroVolumeNode* inputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (volumeDisplayNode->GetVolumeNode());
  if (!inputVolume)
    {
    return 0.;
    }

  double world [] = {0., 0., 0.}, ijk [] = {0., 0., 0.};
  ijk[0] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) * 0.5;
  ijk[1] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) * 0.5;
  ijk[2] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3")) * 0.5;
  volumeDisplayNode->GetReferenceSpace(ijk, world);
  return world[2];
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::cleanPointers()
{
  if (this->Internal->fitF != NULL)
    {
    delete this->Internal->fitF;
    this->Internal->fitF = NULL;
    }

  if (this->Internal->cubeF != NULL)
    {
    delete this->Internal->cubeF;
    this->Internal->cubeF = NULL;
    }

  if (this->Internal->modF != NULL)
    {
    delete this->Internal->modF;
    this->Internal->modF = NULL;
    }

  if (this->Internal->fitD != NULL)
    {
    delete this->Internal->fitD;
    this->Internal->fitD = NULL;
    }

  if (this->Internal->cubeD != NULL)
    {
    delete this->Internal->cubeD;
    this->Internal->cubeD = NULL;
    }

  if (this->Internal->modD != NULL)
    {
    delete this->Internal->modD;
    this->Internal->modD = NULL;
    }

  if (this->Internal->par != NULL)
    {
    delete this->Internal->par;
    this->Internal->par = NULL;
    }

  if (this->Internal->head != NULL)
    {
    delete this->Internal->head;
    this->Internal->head = NULL;
    }
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroModelingLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::SetMarkupsLogic(vtkSlicerMarkupsLogic *logic)
{
  this->Internal->MarkupsLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerMarkupsLogic *vtkSlicerAstroModelingLogic::GetMarkupsLogic()
{
  return this->Internal->MarkupsLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroModelingLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroModelingLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroModelingParametersNode* pNode = vtkMRMLAstroModelingParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::OperateModel(vtkMRMLAstroModelingParametersNode* pnode,
                                              vtkMRMLTableNode *tnode)
{
  int wasModifying = 0;

  if(!pnode)
    {
    vtkWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " parameter node not found!");
    return 0;
    }

  if(!tnode)
    {
    vtkWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " table node not found!");
    return 0;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " scene not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " inputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if(!outputVolume || !outputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " outputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *residualVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetResidualVolumeNodeID()));
  if(!residualVolume || !residualVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " residualVolume not found!");
    return 0;
    }

  vtkMRMLAstroLabelMapVolumeNode *maskVolume =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetMaskVolumeNodeID()));

  bool maskActive = pnode->GetMaskActive();

  if((!maskVolume || !maskVolume->GetImageData()) && maskActive)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " maskVolume not found!");
    return 0;
    }

  vtkImageData* imageData = inputVolume->GetImageData();
  if (!imageData)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " input imageData NULL!");
    return 0;
    }

  vtkPointData* pointData = imageData->GetPointData();
  if (!pointData)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " input pointData NULL!");
    return 0;
    }

  vtkDataArray *dataArray = pointData->GetScalars();
  if (!dataArray)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " input dataArray NULL!");
    return 0;
    }

  vtkMRMLAstroVolumeDisplayNode* inputVolumeDisplay =
    inputVolume->GetAstroVolumeDisplayNode();
  if (!inputVolumeDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " inputVolumeDisplay NULL!");
    return 0;
    }

  // Calculate Vsys shift between Optical and Radio velocity definitions
  // 3DBarolo works on input with the radio velocity definition
  double VsysShift = 0;

  if (!strncmp(inputVolumeDisplay->GetVelocityDefinition().c_str(), "VOPT", 4))
    {
    double VSys1 = this->CalculateCentralVelocity(inputVolumeDisplay);
    if (!inputVolumeDisplay->SetRadioVelocityDefinition(false))
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                    " Conversion to radio velocity definition failed!");
      return 0;
      }

    double VSys2 = this->CalculateCentralVelocity(inputVolumeDisplay);
    VsysShift = (VSys1 - VSys2) * 0.001;
    }
  else if (!strncmp(inputVolumeDisplay->GetVelocityDefinition().c_str(), "VRAD", 4))
    {
    if (!inputVolumeDisplay->SetOpticalVelocityDefinition(false))
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                    " Conversion to radio velocity definition failed!");
      return 0;
      }

    double VSys1 = this->CalculateCentralVelocity(inputVolumeDisplay);
    if (!inputVolumeDisplay->SetRadioVelocityDefinition(false))
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                    " Conversion to radio velocity definition failed!");
      return 0;
      }

    double VSys2 = this->CalculateCentralVelocity(inputVolumeDisplay);
    VsysShift = (VSys1 - VSys2) * 0.001;
    }

  const int DataType = dataArray->GetDataType();

  string file = inputVolume->GetName();

  if (this->Internal->par != NULL)
    {
    delete this->Internal->par;
    this->Internal->par = NULL;
    }

  this->Internal->par = new Param;

  if (this->Internal->head != NULL)
    {
    delete this->Internal->head;
    this->Internal->head = NULL;
    }

  this->Internal->head = new Header;

  this->Internal->par->setImageFile(file);
  string outputFolder = this->GetMRMLScene()->GetCacheManager()->GetRemoteCacheDirectory();
  outputFolder += "/";
  this->Internal->par->setOutfolder(outputFolder);
  this->Internal->par->setVerbosity(true);
  this->Internal->par->setShowbar(false);
  this->Internal->par->setSearch(false);
  this->Internal->par->setflagGalFit(true);

  if (!strcmp(pnode->GetMode(), "Manual"))
    {
    if (pnode->GetNumberOfRings() > 0 && fabs(pnode->GetNumberOfRings() - 1) > 0.001)
      {
      this->Internal->par->setNRADII(pnode->GetNumberOfRings());
      }
    else if (pnode->GetNumberOfRings() > 0 && fabs(pnode->GetNumberOfRings() - 1) < 0.001 )
      {
      pnode->SetStatus(100);
      pnode->SetFitSuccess(false);
      inputVolumeDisplay->SetOpticalVelocityDefinition(false);
      vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                    " insufficient number of rings."
                    " 3DBarolo needs at least two rings!");
      return 0;
      }
    else
      {
      this->Internal->par->setNRADII(-1);
      }

    if (pnode->GetRadSep() > 0.)
      {
      this->Internal->par->setRADSEP(pnode->GetRadSep());
      }
    else
      {
      this->Internal->par->setRADSEP(-1);
      }

    if (pnode->GetXCenter() > 0.)
      {
      this->Internal->par->setXPOS(DoubleToString(pnode->GetXCenter()));
      }
    else
      {
      this->Internal->par->setXPOS("-1");
      }

    if (pnode->GetYCenter() > 0.)
      {
      this->Internal->par->setYPOS(DoubleToString(pnode->GetYCenter()));
      }
    else
      {
      this->Internal->par->setYPOS("-1");
      }

    if (pnode->GetSystemicVelocity() > 0.)
      {
      this->Internal->par->setVSYS(DoubleToString(pnode->GetSystemicVelocity()- VsysShift));
      }
    else
      {
      this->Internal->par->setVSYS("-1");
      }

    if (pnode->GetRotationVelocity() > 0.)
      {
      this->Internal->par->setVROT(DoubleToString(pnode->GetRotationVelocity()));
      }
    else
      {
      this->Internal->par->setVROT("-1");
      }

    this->Internal->par->setVRAD(DoubleToString(pnode->GetRadialVelocity()));

    if (pnode->GetVelocityDispersion() > 0.)
      {
      this->Internal->par->setVDISP(DoubleToString(pnode->GetVelocityDispersion()));
      }
    else
      {
      this->Internal->par->setVDISP("-1");
      }

    if (pnode->GetInclination() > 0.)
      {
      this->Internal->par->setINC(DoubleToString(pnode->GetInclination()));
      }
    else
      {
      this->Internal->par->setINC("-1");
      }

    this->Internal->par->setDELTAINC(pnode->GetInclinationError());

    if (pnode->GetPositionAngle() > 0.)
      {
      this->Internal->par->setPHI(DoubleToString(pnode->GetPositionAngle()));
      }
    else
      {
      this->Internal->par->setPHI("-1");
      }

    this->Internal->par->setDELTAPHI(pnode->GetPositionAngleError());

    if (pnode->GetScaleHeight() > 0.)
      {
      this->Internal->par->setZ0(DoubleToString(pnode->GetScaleHeight()));
      }
    else
      {
      this->Internal->par->setZ0("-1");
      }

    if (pnode->GetDistance() > 0.)
      {
      this->Internal->par->setDistance(pnode->GetDistance());
      }
    else
      {
      this->Internal->par->setDistance(-1);
      }

    if (pnode->GetColumnDensity() > 0.)
      {
      this->Internal->par->setDENS(DoubleToString(pnode->GetColumnDensity()));
      }
    else
      {
      this->Internal->par->setDENS("-1");
      }

    string freeParameters;

    if (pnode->GetPositionAngleFit())
      {
      freeParameters += "PA";
      }

    if (pnode->GetRotationVelocityFit())
      {
      freeParameters += "VROT";
      }

    if (pnode->GetRadialVelocityFit())
      {
      freeParameters += "VRAD";
      }

    if (pnode->GetVelocityDispersionFit())
      {
      freeParameters += "VDISP";
      }

    if (pnode->GetInclinationFit())
      {
      freeParameters += "INC";
      }

    if (pnode->GetXCenterFit())
      {
      freeParameters += "XPOS";
      }

    if (pnode->GetYCenterFit())
      {
      freeParameters += "YPOS";
      }

    if (pnode->GetSystemicVelocityFit())
      {
      freeParameters += "VSYS";
      }

    if (pnode->GetScaleHeightFit())
      {
      freeParameters += "Z0";
      }

    this->Internal->par->setFREE(freeParameters);
    this->Internal->par->setLTYPE(pnode->GetLayerType() + 1);
    this->Internal->par->setFTYPE(pnode->GetFittingFunction() + 1);
    this->Internal->par->setWFUNC(pnode->GetWeightingFunction());
    this->Internal->par->setCDENS(pnode->GetCloudsColumnDensity());

    if (pnode->GetNumberOfClounds() > 0)
      {
      this->Internal->par->setNV(pnode->GetNumberOfClounds());
      }
    else
      {
      this->Internal->par->setNV(-1);
      }
    }
  else
    {
    this->Internal->par->setNRADII(-1);
    this->Internal->par->setRADSEP(-1);
    this->Internal->par->setXPOS("-1");
    this->Internal->par->setYPOS("-1");
    this->Internal->par->setVSYS("-1");
    this->Internal->par->setVROT("-1");
    this->Internal->par->setVRAD("0");
    this->Internal->par->setPHI("-1");
    this->Internal->par->setINC("-1");
    this->Internal->par->setZ0("-1");
    this->Internal->par->setDistance(-1);
    this->Internal->par->setVDISP("-1");
    this->Internal->par->setDENS("-1");
    this->Internal->par->setNV(-1);
    }

  if (maskActive)
    {
    if (this->Internal->par->getNRADII() == -1 ||
        this->Internal->par->getRADSEP() == -1 ||
        this->Internal->par->getXPOS() == "-1" ||
        this->Internal->par->getYPOS() == "-1" ||
        this->Internal->par->getVSYS() == "-1" ||
        this->Internal->par->getVROT() == "-1" ||
        this->Internal->par->getPHI() == "-1" ||
        this->Internal->par->getINC() == "-1" ||
        this->Internal->par->getZ0() == "-1" ||
        this->Internal->par->getDistance() == -1 ||
        this->Internal->par->getVDISP() == "-1" ||
        this->Internal->par->getDENS() == "-1" ||
        this->Internal->par->getNV() == -1)
      {
      this->Internal->par->setSearch(true);
      }
    else
      {
      this->Internal->par->setSearch(false);
      }
    }
  else
    {
    this->Internal->par->setSearch(true);
    }

  this->Internal->par->setNORM(pnode->GetNormalize());

  // set header
  this->Internal->head->setBitpix(StringToInt(inputVolume->GetAttribute("SlicerAstro.BITPIX")));
  int numAxes = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  this->Internal->head->setNumAx(numAxes);
  string inputString;

  for (int ii = 0; ii < numAxes; ii++)
    {
    inputString = "SlicerAstro.NAXIS";
    inputString += IntToString(ii + 1);
    this->Internal->head->setDimAx(ii,StringToLong(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CRPIX";
    inputString += IntToString(ii + 1);
    this->Internal->head->setCrpix(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CRVAL";
    inputString += IntToString(ii + 1);
    this->Internal->head->setCrval(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CDELT";
    inputString += IntToString(ii + 1);
    this->Internal->head->setCdelt(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CTYPE";
    inputString += IntToString(ii + 1);
    inputString = inputVolume->GetAttribute(inputString.c_str());
    this->Internal->head->setCtype(ii, inputString);
    inputString = "SlicerAstro.CUNIT";
    inputString += IntToString(ii + 1);
    inputString = inputVolume->GetAttribute(inputString.c_str());
    this->Internal->head->setCunit(ii, inputString);
    }

  this->Internal->head->setBmaj(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BMAJ")));
  this->Internal->head->setBmin(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BMIN")));
  this->Internal->head->setBpa(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BPA")));
  this->Internal->head->setBzero(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BZERO")));
  this->Internal->head->setBscale(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BSCALE")));
  this->Internal->head->setBlank(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BLANK")));
  this->Internal->head->setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BBCROTA")));
  this->Internal->head->setEpoch(StringToFloat(inputVolume->GetAttribute("SlicerAstro.EPOCH")));
  this->Internal->head->setFreq0(StringToDouble(inputVolume->GetAttribute("SlicerAstro.RESTFREQ")));
  inputString = inputVolume->GetAttribute("SlicerAstro.BUNIT");
  this->Internal->head->setBunit(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.BTYPE");
  this->Internal->head->setBtype(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.OBJECT");
  this->Internal->head->setName(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.TELESCOP");
  this->Internal->head->setTelesc(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.DUNIT3");
  this->Internal->head->setDunit3(inputString);
  this->Internal->head->setDrval3(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DRVAL3")));
  this->Internal->head->setDataMin(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMIN")));
  this->Internal->head->setDataMax(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMAX")));
  if(!this->Internal->head->saveWCSStruct(inputVolume->GetAstroVolumeDisplayNode()->GetWCSStruct()))
    {
    pnode->SetStatus(100);
    pnode->SetFitSuccess(false);
    inputVolumeDisplay->SetOpticalVelocityDefinition(false);
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : the WCS copy failed!");
    return 0;
    }

  this->Internal->head->calcArea();

  if (pnode->GetStatus() == -1)
    {
    this->cleanPointers();
    pnode->SetStatus(100);
    pnode->SetFitSuccess(false);
    return 0;
    }
  pnode->SetStatus(10);

  int *dims = inputVolume->GetImageData()->GetDimensions();
  int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;

  switch (DataType)
    {
    case VTK_FLOAT:
      {
      if (this->Internal->cubeF != NULL)
        {
        delete this->Internal->cubeF;
        this->Internal->cubeF = NULL;
        }

      this->Internal->cubeF = new Cube<float>;

      this->Internal->cubeF->saveParam(*this->Internal->par);
      this->Internal->cubeF->saveHead(*this->Internal->head);
      this->Internal->cubeF->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BBCROTA")));
      float *inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer());
      if (!inFPixel)
        {
        this->cleanPointers();
        pnode->SetStatus(100);
        pnode->SetFitSuccess(false);
        inputVolumeDisplay->SetOpticalVelocityDefinition(false);
        vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                      "Unable to find inFPixel pointer.");
        return 0;
        }
      this->Internal->cubeF->setCube(inFPixel, dims);

      // Feed segmentation mask to cube
      if (maskActive)
        {
        short* segmentationMaskPointer = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer());
        if (!segmentationMaskPointer)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find segmentationMask.");
          return 0;
          }
        bool* mask = new bool[numElements];
        for(int ii = 0; ii < numElements; ii++)
          {
          if (*(segmentationMaskPointer + ii) > 0)
            {
            *(mask + ii) = true;
            }
          else
            {
            *(mask + ii) = false;
            }
          }

        this->Internal->cubeF->setMask(mask);
        delete mask;
        mask = NULL;
        }

      // Searching stuff if the user has not provided a mask
      if (this->Internal->par->getSearch())
        {
        this->Internal->cubeF->Search();
        }

      // Cube Fitting
      if (this->Internal->par->getflagGalFit())
        {
        if (this->Internal->fitF != NULL)
          {
          delete this->Internal->fitF;
          this->Internal->fitF = NULL;
          }

        this->Internal->fitF = new Model::Galfit<float>();
        int error = this->Internal->fitF->input(this->Internal->cubeF);
        if (error == 0)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "3DBarolo could not find the source.");
          return 0;
          }
        else if (error == 2)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                        " insufficient number of rings. "
                        " The identified source has not enough"
                        " spatial resolution elements.");
          return 0;
          }

        *this->Internal->par = this->Internal->cubeF->pars();

        wasModifying = pnode->StartModify();

        pnode->SetNumberOfRings(this->Internal->par->getNRADII());
        pnode->SetRadSep(this->Internal->par->getRADSEP());
        pnode->SetXCenter(StringToDouble(this->Internal->par->getXPOS().c_str()));
        pnode->SetYCenter(StringToDouble(this->Internal->par->getYPOS().c_str()));
        pnode->SetSystemicVelocity(StringToDouble(this->Internal->par->getVSYS().c_str()) + VsysShift);
        pnode->SetRotationVelocity(StringToDouble(this->Internal->par->getVROT().c_str()));
        pnode->SetRadialVelocity(StringToDouble(this->Internal->par->getVRAD().c_str()));
        pnode->SetPositionAngle(StringToDouble(this->Internal->par->getPHI().c_str()));
        pnode->SetInclination(StringToDouble(this->Internal->par->getINC().c_str()));
        pnode->SetScaleHeight(StringToDouble(this->Internal->par->getZ0().c_str()));
        pnode->SetDistance(this->Internal->par->getDistance());
        pnode->SetVelocityDispersion(StringToDouble(this->Internal->par->getVDISP().c_str()));
        pnode->SetColumnDensity(StringToDouble(this->Internal->par->getDENS().c_str()));
        pnode->SetCloudsColumnDensity((double) this->Internal->par->getCDENS());
        pnode->SetNumberOfClounds(this->Internal->par->getNV());

        pnode->EndModify(wasModifying);

        if (pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::ESTIMATE)
          {
          // Exit. Only the estimation of the parameters was requested.
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          return 0;
          }

        if (pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::FIT)
          {
          this->Internal->fitF->galfit(pnode->GetStatusPointer());

          if (pnode->GetStatus() == -1)
            {
            this->cleanPointers();
            pnode->SetStatus(100);
            pnode->SetFitSuccess(false);
            return 0;
            }

          pnode->SetStatus(60);

          if (this->Internal->par->getTwoStage())
            {
            this->Internal->fitF->SecondStage(pnode->GetStatusPointer());
            if (pnode->GetStatus() == -1)
              {
              this->cleanPointers();
              pnode->SetStatus(100);
              pnode->SetFitSuccess(false);
              return 0;
              }
            }
          }

        pnode->SetStatus(80);

        float *ringreg = this->Internal->fitF->getFinalRingsRegion();
        if (!ringreg)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find ringreg.");
          return 0;
          }

        float totflux_model = 0.;
        this->Internal->totflux_data = 0.;
        MomentMap<float> *totalmap = new MomentMap<float>;
        totalmap->input(this->Internal->cubeF);
        totalmap->SumMap(true);

        // Calculate the total flux inside last ring in data
        for (size_t ii = 0; ii < (size_t)this->Internal->cubeF->DimX() * this->Internal->cubeF->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]) && !isNaN(totalmap->Array(ii)))
            {
            this->Internal->totflux_data += totalmap->Array(ii);
            }
          }

        float mass = 2.365E5 * FluxtoJy(this->Internal->totflux_data, *this->Internal->head)
          * fabs(DeltaVel<float>(*this->Internal->head))
          * this->Internal->par->getDistance() * this->Internal->par->getDistance();

        // Calculate radial profile along the output rings
        float meanPA = findMean<float>(&this->Internal->fitF->Outrings()->phi[0],
                                       this->Internal->fitF->Outrings()->nr);
        int nseg = 1;
        float segments[4] = {0, 360., 0., 0};
        if (this->Internal->par->getSIDE() != "A")
          {
          nseg = 2;
          segments[2] = -90;
          segments[3] = 90;
          }
        else if (this->Internal->par->getSIDE() == "R")
          {
          nseg = 2;
          segments[2] = 90;
          segments[3] = -90;
          }
        if (meanPA > 180)
          {
          std::swap(segments[2], segments[3]);
          }

        Tasks::Ellprof<float> ell(totalmap, this->Internal->fitF->Outrings(), nseg, segments);
        ell.setOptions(mass, this->Internal->par->getDistance());  //To set the mass and the distance
        ell.RadialProfile();

        if (!strcmp(pnode->GetNormalize(), "AZIM"))
          {
          double profmin = FLT_MAX;
          for (size_t i = 0; i < (size_t)this->Internal->fitF->Outrings()->nr; i++)
            {
            double mean = ell.getMean(i);
            if (!isNaN(mean) && profmin > mean && mean > 0)
              {
              profmin = mean;
              }
            }
          float factor = 1;
          while(profmin < 0.1)
            {
            profmin *= 10;
            factor *= 10;
            }
          while (profmin > 10)
            {
            profmin /= 10;
            factor /= 10;
            }
          for (size_t i = 0; i < (size_t)this->Internal->fitF->Outrings()->nr; i++)
            {
            this->Internal->fitF->Outrings()->dens[i] = factor * fabs(ell.getMean(i)) * 1E20;
            if (this->Internal->fitF->Outrings()->dens[i] == 0)
              {
              this->Internal->fitF->Outrings()->dens[i] = profmin * 1E20;
              }
            }
          }

        this->Internal->modF = this->Internal->fitF->getModel();
        if (!this->Internal->modF || !this->Internal->modF->Out())
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find modF.");
          return 0;
          }

        this->Internal->modF->Out()->Head().setMinMax(0.,0.);
        float *outarray = this->Internal->modF->Out()->Array();
        if (!outarray)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find modF array.");
          return 0;
          }

        if (!strcmp(pnode->GetNormalize(), "AZIM") ||
            !strcmp(pnode->GetNormalize(), "NONE"))
          {
          // The final model has been build from the azimuthal profile calculated before,
          // thus just need to rescale the model to the total flux of data inside last ring.

          // Calculate total flux of model within last ring
          for (size_t ii = 0; ii < (size_t)this->Internal->cubeF->DimX() * this->Internal->cubeF->DimY(); ii++)
            {
            if (!isNaN(ringreg[ii]))
              {
              for (size_t z = 0; z < (size_t)this->Internal->cubeF->DimZ(); z++)
                {
                totflux_model += outarray[ii + z * this->Internal->cubeF->DimY() * this->Internal->cubeF->DimX()];
                }
              }
            }

          double factor = this->Internal->totflux_data/totflux_model;
          for (int ii = 0; ii < this->Internal->cubeF->NumPix(); ii++)
            {
            outarray[ii] *= factor;
            }
          }
        if (!strcmp(pnode->GetNormalize(), "LOCAL"))
          {
          // The final model is normalized pixel-by-pixel, i.e. in each spaxel, the total
          // flux of observation and model is eventually equal.

          for (int y = 0; y < this->Internal->cubeF->DimY(); y++)
            {
            for (int x = 0; x < this->Internal->cubeF->DimX(); x++)
              {
              float factor = 0;
              float modSum = 0;
              float obsSum = 0;
              for (int z = 0; z < this->Internal->cubeF->DimZ(); z++)
                {
                long Pix = this->Internal->cubeF->nPix(x,y,z);
                modSum += outarray[Pix];
                obsSum += this->Internal->cubeF->Array(Pix) * this->Internal->cubeF->Mask()[Pix];
                }
              if (modSum != 0)
                {
                factor = obsSum / modSum;
                }

              for (int z = 0; z < this->Internal->cubeF->DimZ(); z++)
                {
                outarray[this->Internal->cubeF->nPix(x,y,z)] *= factor;
                }
              }
            }
          }

        float *outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer());
        if (!outFPixel)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find outFPixel pointer.");
          return 0;
          }
        for (int ii = 0; ii < numElements; ii++)
          {
          *(outFPixel + ii) = *(outarray + ii);
          }

        float *residualFPixel = static_cast<float*> (residualVolume->GetImageData()->GetScalarPointer());
        if (!residualFPixel)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find residualFPixel pointer.");
          return 0;
          }

        for (int ii = 0; ii < numElements; ii++)
          {
          if (*(outFPixel + ii) < 1.E-6)
            {
            *(residualFPixel + ii) = *(inFPixel + ii);
            }
          else
            {
            *(residualFPixel + ii) = 0.;
            }
          }

        delete totalmap;
        totalmap = NULL;
        delete ringreg;
        ringreg = NULL;
        outarray = NULL;
        if (this->Internal->modF != NULL)
          {
          delete this->Internal->modF;
          this->Internal->modF = NULL;
          }
        outFPixel = NULL;
        inFPixel = NULL;
        residualFPixel = NULL;
        }
      break;
      }
    case VTK_DOUBLE:
      {
      if (this->Internal->cubeD != NULL)
        {
        delete this->Internal->cubeD;
        this->Internal->cubeD = NULL;
        }

      this->Internal->cubeD = new Cube<double>;

      this->Internal->cubeD->saveParam(*this->Internal->par);
      this->Internal->cubeD->saveHead(*this->Internal->head);
      this->Internal->cubeD->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BBCROTA")));
      double *inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer());
      if (!inDPixel)
        {
        this->cleanPointers();
        pnode->SetStatus(100);
        pnode->SetFitSuccess(false);
        inputVolumeDisplay->SetOpticalVelocityDefinition(false);
        vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                      "Unable to find inDPixel pointer.");
        return 0;
        }
      this->Internal->cubeD->setCube(inDPixel, dims);

      // Feed segmentation mask to cube
      if (maskActive)
        {
        short* segmentationMaskPointer = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer());
        if (!segmentationMaskPointer)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find segmentationMask.");
          return 0;
          }
        bool* mask = new bool[numElements];
        for(int ii = 0; ii < numElements; ii++)
          {
          if (*(segmentationMaskPointer + ii) > 0)
            {
            *(mask + ii) = true;
            }
          else
            {
            *(mask + ii) = false;
            }
          }

        this->Internal->cubeD->setMask(mask);
        delete mask;
        mask = NULL;
        }

      // Searching stuff if the user has not provided a mask
      if (this->Internal->par->getSearch())
        {
        this->Internal->cubeD->Search();
        }

      // Cube Fitting
      if (this->Internal->par->getflagGalFit())
        {
        if (this->Internal->fitD != NULL)
          {
          delete this->Internal->fitD;
          this->Internal->fitD = NULL;
          }

        this->Internal->fitD = new Model::Galfit<double>();
        int error = this->Internal->fitD->input(this->Internal->cubeD);

        if (error == 0)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "3DBarolo could not find the source.");
          return 0;
          }
        else if (error == 2)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                        " insufficient number of rings. "
                        " The identified source has not enough"
                        " spatial resolution elements.");
          return 0;
          }

        *this->Internal->par = this->Internal->cubeD->pars();

        wasModifying = pnode->StartModify();

        pnode->SetNumberOfRings(this->Internal->par->getNRADII());
        pnode->SetRadSep(this->Internal->par->getRADSEP());
        pnode->SetXCenter(StringToDouble(this->Internal->par->getXPOS().c_str()));
        pnode->SetYCenter(StringToDouble(this->Internal->par->getYPOS().c_str()));
        pnode->SetSystemicVelocity(StringToDouble(this->Internal->par->getVSYS().c_str()) + VsysShift);
        pnode->SetRotationVelocity(StringToDouble(this->Internal->par->getVROT().c_str()));
        pnode->SetRadialVelocity(StringToDouble(this->Internal->par->getVRAD().c_str()));
        pnode->SetPositionAngle(StringToDouble(this->Internal->par->getPHI().c_str()));
        pnode->SetInclination(StringToDouble(this->Internal->par->getINC().c_str()));
        pnode->SetScaleHeight(StringToDouble(this->Internal->par->getZ0().c_str()));
        pnode->SetDistance(this->Internal->par->getDistance());
        pnode->SetVelocityDispersion(StringToDouble(this->Internal->par->getVDISP().c_str()));
        pnode->SetColumnDensity(StringToDouble(this->Internal->par->getDENS().c_str()));
        pnode->SetCloudsColumnDensity((double) this->Internal->par->getCDENS());
        pnode->SetNumberOfClounds(this->Internal->par->getNV());

        pnode->EndModify(wasModifying);

        if (pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::ESTIMATE)
          {
          //Exit. Only the estimation of the parameters was requested.
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          return 0;
          }

        if (pnode->GetOperation() == vtkMRMLAstroModelingParametersNode::FIT)
          {
          this->Internal->fitD->galfit(pnode->GetStatusPointer());

          if (pnode->GetStatus() == -1)
            {
            this->cleanPointers();
            pnode->SetStatus(100);
            pnode->SetFitSuccess(false);
            return 0;
            }

          pnode->SetStatus(60);

          if (this->Internal->par->getTwoStage())
            {
            this->Internal->fitD->SecondStage(pnode->GetStatusPointer());
            if (pnode->GetStatus() == -1)
              {
              this->cleanPointers();
              pnode->SetStatus(100);
              pnode->SetFitSuccess(false);
              return 0;
              }
            }
          }

        pnode->SetStatus(80);

        // Calculate the total flux inside last ring in data
        double *ringreg = this->Internal->fitD->getFinalRingsRegion();
        if (!ringreg)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find ringreg.");
          return 0;
          }
        double totflux_model = 0.;
        this->Internal->totflux_data = 0.;
        MomentMap<double> *totalmap = new MomentMap<double>;
        totalmap->input(this->Internal->cubeD);
        totalmap->SumMap(true);

        // Calculate the total flux inside last ring in data
        for (size_t ii = 0; ii < (size_t)this->Internal->cubeD->DimX() * this->Internal->cubeD->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]) && !isNaN(totalmap->Array(ii)))
            {
            this->Internal->totflux_data += totalmap->Array(ii);
            }
          }

        double mass = 2.365E5 * FluxtoJy(this->Internal->totflux_data, *this->Internal->head)
          * fabs(DeltaVel<double>(*this->Internal->head))
          * this->Internal->par->getDistance() * this->Internal->par->getDistance();

        // Calculate radial profile along the output rings
        double meanPA = findMean<double>(&this->Internal->fitD->Outrings()->phi[0],
                                         this->Internal->fitD->Outrings()->nr);
        int nseg = 1;
        float segments[4] = {0, 360., 0., 0};
        if (this->Internal->par->getSIDE() != "A")
          {
          nseg = 2;
          segments[2] = -90;
          segments[3] = 90;
          }
        else if (this->Internal->par->getSIDE() == "R")
          {
          nseg = 2;
          segments[2] = 90;
          segments[3] = -90;
          }
        if (meanPA > 180)
          {
          std::swap(segments[2], segments[3]);
          }

        Tasks::Ellprof<double> ell(totalmap, this->Internal->fitD->Outrings(), nseg, segments);
        ell.setOptions(mass, this->Internal->par->getDistance());  //To set the mass and the distance
        ell.RadialProfile();

        if (!strcmp(pnode->GetNormalize(), "AZIM"))
          {
          double profmin = FLT_MAX;
          for (size_t i = 0; i < (size_t)this->Internal->fitD->Outrings()->nr; i++)
            {
            double mean = ell.getMean(i);
            if (!isNaN(mean) && profmin > mean && mean > 0)
              {
              profmin = mean;
              }
            }
          float factor = 1;
          while(profmin < 0.1)
            {
            profmin *= 10;
            factor *= 10;
            }
          while (profmin > 10)
            {
            profmin /= 10;
            factor /= 10;
            }
          for (size_t i = 0; i < (size_t)this->Internal->fitD->Outrings()->nr; i++)
            {
            this->Internal->fitD->Outrings()->dens[i] = factor * fabs(ell.getMean(i)) * 1E20;
            if (this->Internal->fitD->Outrings()->dens[i] == 0)
              {
              this->Internal->fitD->Outrings()->dens[i] = profmin * 1E20;
              }
            }
          }

        this->Internal->modD = this->Internal->fitD->getModel();
        if (!this->Internal->modD || !this->Internal->modD->Out())
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find modF.");
          return 0;
          }
        this->Internal->modD->Out()->Head().setMinMax(0.,0.);
        double *outarray = this->Internal->modD->Out()->Array();
        if (!outarray)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find modF array.");
          return 0;
          }

        if (!strcmp(pnode->GetNormalize(), "AZIM") ||
            !strcmp(pnode->GetNormalize(), "NONE"))
          {
          // The final model has been build from the azimuthal profile calculated before,
          // thus just need to rescale the model to the total flux of data inside last ring.

          // Calculate total flux of model within last ring
          for (size_t ii = 0; ii < (size_t)this->Internal->cubeD->DimX() * this->Internal->cubeD->DimY(); ii++)
            {
            if (!isNaN(ringreg[ii]))
              {
              for (size_t z = 0; z < (size_t)this->Internal->cubeD->DimZ(); z++)
                {
                totflux_model += outarray[ii + z * this->Internal->cubeD->DimY() * this->Internal->cubeD->DimX()];
                }
              }
            }

          double factor = this->Internal->totflux_data / totflux_model;
          for (int ii = 0; ii < this->Internal->cubeD->NumPix(); ii++)
            {
            outarray[ii] *= factor;
            }
          }
        if (!strcmp(pnode->GetNormalize(), "LOCAL"))
          {
          // The final model is normalized pixel-by-pixel, i.e. in each spaxel, the total
          // flux of observation and model is eventually equal.

          for (int y = 0; y < this->Internal->cubeD->DimY(); y++)
            {
            for (int x = 0; x < this->Internal->cubeD->DimX(); x++)
              {
              float factor = 0;
              double modSum = 0;
              double obsSum = 0;
              for (int z = 0; z < this->Internal->cubeD->DimZ(); z++)
                {
                long Pix = this->Internal->cubeD->nPix(x,y,z);
                modSum += outarray[Pix];
                obsSum += this->Internal->cubeD->Array(Pix) * this->Internal->cubeD->Mask()[Pix];
                }
              if (modSum != 0)
                {
                factor = obsSum / modSum;
                }

              for (int z = 0; z < this->Internal->cubeD->DimZ(); z++)
                {
                outarray[this->Internal->cubeD->nPix(x,y,z)] *= factor;
                }
              }
            }
          }

        double *outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer());
        if (!outDPixel)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find outDPixel pointer.");
          return 0;
          }
        for (int ii = 0; ii < numElements; ii++)
          {
          *(outDPixel + ii) = *(outarray + ii);
          }

        double *residualDPixel = static_cast<double*> (residualVolume->GetImageData()->GetScalarPointer());
        if (!residualDPixel)
          {
          this->cleanPointers();
          pnode->SetStatus(100);
          pnode->SetFitSuccess(false);
          inputVolumeDisplay->SetOpticalVelocityDefinition(false);
          vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                        "Unable to find residualDPixel pointer.");
          return 0;
          }

        for (int ii = 0; ii < numElements; ii++)
          {
          if (*(outDPixel + ii) < 1.E-6)
            {
            *(residualDPixel + ii) = *(inDPixel + ii);
            }
          else
            {
            *(residualDPixel + ii) = 0.;
            }
          }

        delete totalmap;
        totalmap = NULL;
        delete ringreg;
        ringreg = NULL;
        outarray = NULL;
        if (this->Internal->modD != NULL)
          {
          delete this->Internal->modD;
          this->Internal->modD = NULL;
          }
        outDPixel = NULL;
        inDPixel = NULL;
        residualDPixel = NULL;
        }
      break;
      }
    }

  // Transform back the velocity definition to optical one
  if (!inputVolumeDisplay->SetOpticalVelocityDefinition(false))
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " Conversion to optical velocity definition failed!");
    return 0;
    }

  pnode->SetStatus(95);

  if (!tnode)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                  "Unable to find the internal parameters table node.");
    return 0;
    }

  wasModifying = tnode->StartModify();

  vtkTable* paramsTable = tnode->GetTable();
  if (!paramsTable)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                  "Unable to find the parameters table.");
    return 0;
    }

  vtkDoubleArray* XPos = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("XPos"));
  XPos->Initialize();
  vtkDoubleArray* YPos = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("YPos"));
  YPos->Initialize();
  vtkDoubleArray* VSys = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("VSys"));
  VSys->Initialize();
  vtkDoubleArray* Radii = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Radii"));
  Radii->Initialize();
  vtkDoubleArray* VRot = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("VRot"));
  VRot->Initialize();
  vtkDoubleArray* VRad = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("VRad"));
  VRad->Initialize();
  vtkDoubleArray* VDisp = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("VDisp"));
  VDisp->Initialize();
  vtkDoubleArray* Dens = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Dens"));
  Dens->Initialize();
  vtkDoubleArray* Z0 = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Z0"));
  Z0->Initialize();
  vtkDoubleArray* Inc = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Inc"));
  Inc->Initialize();
  vtkDoubleArray* Phi = vtkDoubleArray::SafeDownCast
    (paramsTable->GetColumnByName("Phi"));
  Phi->Initialize();

  if (!XPos || !YPos || !VSys || !Radii ||
      !VRot || !VRad || !VDisp || !Dens ||
      !Z0 || !Inc || !Phi)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel : "
                  "Unable to find one or more table columns.");
    return 0;
    }

  for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
    {
    tnode->AddEmptyRow();
    }

  if(this->Internal->fitF && this->Internal->fitF->Outrings())
    {
    if (this->Internal->fitF->Outrings()->xpos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        XPos->SetValue(ii, this->Internal->fitF->Outrings()->xpos[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->ypos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        YPos->SetValue(ii, this->Internal->fitF->Outrings()->ypos[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->vsys.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VSys->SetValue(ii, this->Internal->fitF->Outrings()->vsys[ii] + VsysShift);
        }
      }

    if (this->Internal->fitF->Outrings()->radii.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Radii->SetValue(ii, this->Internal->fitF->Outrings()->radii[ii] * 2.);
        }
      }

    if (this->Internal->fitF->Outrings()->vrot.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRot->SetValue(ii, this->Internal->fitF->Outrings()->vrot[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->vrad.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRad->SetValue(ii, this->Internal->fitF->Outrings()->vrad[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->vdisp.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VDisp->SetValue(ii, this->Internal->fitF->Outrings()->vdisp[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->dens.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Dens->SetValue(ii, this->Internal->fitF->Outrings()->dens[ii] * 1.E-20);
        }
      }

    if (this->Internal->fitF->Outrings()->z0.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Z0->SetValue(ii, this->Internal->fitF->Outrings()->z0[ii]
                         * KpcPerArc(this->Internal->par->getDistance()));
        }
      }

    if (this->Internal->fitF->Outrings()->inc.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Inc->SetValue(ii, this->Internal->fitF->Outrings()->inc[ii]);
        }
      }

    if (this->Internal->fitF->Outrings()->phi.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Phi->SetValue(ii, this->Internal->fitF->Outrings()->phi[ii]);
        }
      }
    }
  else if (this->Internal->fitD && this->Internal->fitD->Outrings())
    {
    if (this->Internal->fitD->Outrings()->xpos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        XPos->SetValue(ii, this->Internal->fitD->Outrings()->xpos[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->ypos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        YPos->SetValue(ii, this->Internal->fitD->Outrings()->ypos[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->vsys.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VSys->SetValue(ii, this->Internal->fitD->Outrings()->vsys[ii] + VsysShift);
        }
      }

    if (this->Internal->fitD->Outrings()->radii.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Radii->SetValue(ii, this->Internal->fitD->Outrings()->radii[ii] * 2.);
        }
      }

    if (this->Internal->fitD->Outrings()->vrot.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRot->SetValue(ii, this->Internal->fitD->Outrings()->vrot[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->vrad.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRad->SetValue(ii, this->Internal->fitD->Outrings()->vrad[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->vdisp.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VDisp->SetValue(ii, this->Internal->fitD->Outrings()->vdisp[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->dens.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Dens->SetValue(ii, this->Internal->fitD->Outrings()->dens[ii] * 1.E-20);
        }
      }

    if (this->Internal->fitD->Outrings()->z0.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Z0->SetValue(ii, this->Internal->fitD->Outrings()->z0[ii]
                         * KpcPerArc(this->Internal->par->getDistance()));
        }
      }

    if (this->Internal->fitD->Outrings()->inc.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Inc->SetValue(ii, this->Internal->fitD->Outrings()->inc[ii]);
        }
      }

    if (this->Internal->fitD->Outrings()->phi.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Phi->SetValue(ii, this->Internal->fitD->Outrings()->phi[ii]);
        }
      }
    }
  else
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " galfit not found! \n");
    return 0;
    }

  tnode->EndModify(wasModifying);

  this->cleanPointers();
  pnode->SetStatus(100);
  pnode->SetFitSuccess(true);

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::UpdateModelFromTable(vtkMRMLAstroModelingParametersNode *pnode)
{
  if(!pnode)
    {
    vtkWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " parameter node not found!");
    return 0;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " scene not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    vtkWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " inputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if(!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " outputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *residualVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetResidualVolumeNodeID()));
  if(!residualVolume)
    {
    vtkWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " residualVolume not found!");
    }

  vtkImageData* imageData = inputVolume->GetImageData();
  if (!imageData)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " input imageData NULL!");
    return 0;
    }

  vtkPointData* pointData = imageData->GetPointData();
  if (!pointData)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " input pointData NULL!");
    return 0;
    }

  vtkDataArray *dataArray = pointData->GetScalars();
  if (!dataArray)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " input dataArray NULL!");
    return 0;
    }

  vtkMRMLAstroVolumeDisplayNode* inputVolumeDisplay =
    inputVolume->GetAstroVolumeDisplayNode();
  if (!inputVolumeDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                  " inputVolumeDisplay NULL!");
    return 0;
    }

  // Calculate Vsys shift between Optical and Radio velocity definitions
  // 3DBarolo works on input with the radio velocity definition
  double VsysShift = 0;

  wcsprm* WCS = inputVolumeDisplay->GetWCSStruct();
  if (!WCS)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " WCS NULL!");
    return 0;
    }

  if (!strncmp(WCS->ctype[2], "VOPT", 4))
    {
    double VSys1 = this->CalculateCentralVelocity(inputVolumeDisplay);

    if (!inputVolumeDisplay->SetRadioVelocityDefinition(false))
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                    " Conversion to radio velocity definition failed!");
      return 0;
      }

    double VSys2 = this->CalculateCentralVelocity(inputVolumeDisplay);

    VsysShift = (VSys1 - VSys2) * 0.001;
    }
  else if (!strncmp(WCS->ctype[2], "VRAD", 4))
    {
    if (!inputVolumeDisplay->SetOpticalVelocityDefinition(false))
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                    " Conversion to radio velocity definition failed!");
      return 0;
      }

    double VSys1 = this->CalculateCentralVelocity(inputVolumeDisplay);

    if (!inputVolumeDisplay->SetRadioVelocityDefinition(false))
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                    " Conversion to radio velocity definition failed!");
      return 0;
      }

    double VSys2 = this->CalculateCentralVelocity(inputVolumeDisplay);

    VsysShift = (VSys1 - VSys2) * 0.001;
    }

  const int DataType = dataArray->GetDataType();

  int *dims = inputVolume->GetImageData()->GetDimensions();
  int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;

  if (!this->Internal->fitF && !this->Internal->fitD)
    {
    if (this->Internal->par != NULL)
      {
      delete this->Internal->par;
      this->Internal->par = NULL;
      }

    this->Internal->par = new Param;

    if (this->Internal->head != NULL)
      {
      delete this->Internal->head;
      this->Internal->head = NULL;
      }

    this->Internal->head = new Header;

    string file = inputVolume->GetName();
    this->Internal->par->setImageFile(file);
    string outputFolder = this->GetMRMLScene()->GetCacheManager()->GetRemoteCacheDirectory();
    outputFolder += "/";
    this->Internal->par->setOutfolder(outputFolder);
    this->Internal->par->setVerbosity(false);
    this->Internal->par->setShowbar(false);
    this->Internal->par->setflagGalFit(true);

    if (pnode->GetNumberOfRings() > 0)
      {
      this->Internal->par->setNRADII(pnode->GetNumberOfRings());
      }
    else
      {
      this->Internal->par->setNRADII(-1);
      }

    if (pnode->GetRadSep() > 0.)
      {
      this->Internal->par->setRADSEP(pnode->GetRadSep());
      }
    else
      {
      this->Internal->par->setRADSEP(-1);
      }

    if (pnode->GetXCenter() > 0.)
      {
      this->Internal->par->setXPOS(DoubleToString(pnode->GetXCenter()));
      }
    else
      {
      this->Internal->par->setXPOS("-1");
      }

    if (pnode->GetYCenter() > 0.)
      {
      this->Internal->par->setYPOS(DoubleToString(pnode->GetYCenter()));
      }
    else
      {
      this->Internal->par->setYPOS("-1");
      }

    if (pnode->GetSystemicVelocity() > 0.)
      {
      this->Internal->par->setVSYS(DoubleToString(pnode->GetSystemicVelocity() - VsysShift));
      }
    else
      {
      this->Internal->par->setVSYS("-1");
      }

    if (pnode->GetRotationVelocity() > 0.)
      {
      this->Internal->par->setVROT(DoubleToString(pnode->GetRotationVelocity()));
      }
    else
      {
      this->Internal->par->setVROT("-1");
      }

    this->Internal->par->setVRAD(DoubleToString(pnode->GetRadialVelocity()));

    if (pnode->GetVelocityDispersion() > 0.)
      {
      this->Internal->par->setVDISP(DoubleToString(pnode->GetVelocityDispersion()));
      }
    else
      {
      this->Internal->par->setVDISP("-1");
      }

    if (pnode->GetInclination() > 0.)
      {
      this->Internal->par->setINC(DoubleToString(pnode->GetInclination()));
      }
    else
      {
      this->Internal->par->setINC("-1");
      }

    this->Internal->par->setDELTAINC(pnode->GetInclinationError());

    if (pnode->GetPositionAngle() > 0.)
      {
      this->Internal->par->setPHI(DoubleToString(pnode->GetPositionAngle()));
      }
    else
      {
      this->Internal->par->setPHI("-1");
      }

    this->Internal->par->setDELTAPHI(pnode->GetPositionAngleError());

    if (pnode->GetScaleHeight() > 0.)
      {
      this->Internal->par->setZ0(DoubleToString(pnode->GetScaleHeight()));
      }
    else
      {
      this->Internal->par->setZ0("-1");
      }

    if (pnode->GetDistance() > 0.)
      {
      this->Internal->par->setDistance(pnode->GetDistance());
      }
    else
      {
      this->Internal->par->setDistance(-1);
      }

    if (pnode->GetColumnDensity() > 0.)
      {
      this->Internal->par->setDENS(DoubleToString(pnode->GetColumnDensity()));
      }
    else
      {
      this->Internal->par->setDENS("-1");
      }

    if (pnode->GetNumberOfClounds() > 0)
      {
      this->Internal->par->setNV(pnode->GetNumberOfClounds());
      }
    else
      {
      this->Internal->par->setNV(-1);
      }

    string freeParameters;

    if (pnode->GetPositionAngleFit())
      {
      freeParameters += "PA";
      }

    if (pnode->GetRotationVelocityFit())
      {
      freeParameters += "VROT";
      }

    if (pnode->GetRadialVelocityFit())
      {
      freeParameters += "VRAD";
      }

    if (pnode->GetVelocityDispersionFit())
      {
      freeParameters += "VDISP";
      }

    if (pnode->GetInclinationFit())
      {
      freeParameters += "INC";
      }

    if (pnode->GetXCenterFit())
      {
      freeParameters += "XPOS";
      }

    if (pnode->GetYCenterFit())
      {
      freeParameters += "YPOS";
      }

    if (pnode->GetSystemicVelocityFit())
      {
      freeParameters += "VSYS";
      }

    if (pnode->GetScaleHeightFit())
      {
      freeParameters += "Z0";
      }

    if (this->Internal->par->getNRADII() == -1 ||
        this->Internal->par->getRADSEP() == -1 ||
        this->Internal->par->getXPOS() == "-1" ||
        this->Internal->par->getYPOS() == "-1" ||
        this->Internal->par->getVSYS() == "-1" ||
        this->Internal->par->getVROT() == "-1" ||
        this->Internal->par->getPHI() == "-1" ||
        this->Internal->par->getINC() == "-1" ||
        this->Internal->par->getZ0() == "-1" ||
        this->Internal->par->getDistance() == -1 ||
        this->Internal->par->getVDISP() == "-1" ||
        this->Internal->par->getDENS() == "-1" ||
        this->Internal->par->getNV() == -1)
      {
      inputVolumeDisplay->SetOpticalVelocityDefinition();
      vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                    "Run again the estimation of the input parameters. ");
      return 0;
      }

    this->Internal->par->setFREE(freeParameters);
    this->Internal->par->setLTYPE(pnode->GetLayerType() + 1);
    this->Internal->par->setFTYPE(pnode->GetFittingFunction() + 1);
    this->Internal->par->setWFUNC(pnode->GetWeightingFunction());
    this->Internal->par->setCDENS(pnode->GetCloudsColumnDensity());

    this->Internal->par->setSearch(false);
    this->Internal->par->setMASK("SMOOTH");
    this->Internal->par->setBeamFWHM(this->Internal->par->getBeamFWHM()/3600.);
    this->Internal->par->setTOL(1.E-3);

    this->Internal->par->setNORM(pnode->GetNormalize());

    // set header
    this->Internal->head->setBitpix(StringToInt(inputVolume->GetAttribute("SlicerAstro.BITPIX")));
    int numAxes = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
    this->Internal->head->setNumAx(numAxes);
    string inputString;

    for (int ii = 0; ii < numAxes; ii++)
      {
      inputString = "SlicerAstro.NAXIS";
      inputString += IntToString(ii + 1);
      this->Internal->head->setDimAx(ii,StringToLong(inputVolume->GetAttribute(inputString.c_str())));
      inputString = "SlicerAstro.CRPIX";
      inputString += IntToString(ii + 1);
      this->Internal->head->setCrpix(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
      inputString = "SlicerAstro.CRVAL";
      inputString += IntToString(ii + 1);
      this->Internal->head->setCrval(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
      inputString = "SlicerAstro.CDELT";
      inputString += IntToString(ii + 1);
      this->Internal->head->setCdelt(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
      inputString = "SlicerAstro.CTYPE";
      inputString += IntToString(ii + 1);
      inputString = inputVolume->GetAttribute(inputString.c_str());
      this->Internal->head->setCtype(ii, inputString);
      inputString = "SlicerAstro.CUNIT";
      inputString += IntToString(ii + 1);
      inputString = inputVolume->GetAttribute(inputString.c_str());
      this->Internal->head->setCunit(ii, inputString);
      }

    this->Internal->head->setBmaj(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BMAJ")));
    this->Internal->head->setBmin(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BMIN")));
    this->Internal->head->setBpa(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BPA")));
    this->Internal->head->setBzero(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BZERO")));
    this->Internal->head->setBscale(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BSCALE")));
    this->Internal->head->setBlank(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BLANK")));
    this->Internal->head->setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BBCROTA")));
    this->Internal->head->setEpoch(StringToFloat(inputVolume->GetAttribute("SlicerAstro.EPOCH")));
    this->Internal->head->setFreq0(StringToDouble(inputVolume->GetAttribute("SlicerAstro.RESTFREQ")));
    inputString = inputVolume->GetAttribute("SlicerAstro.BUNIT");
    this->Internal->head->setBunit(inputString);
    inputString = inputVolume->GetAttribute("SlicerAstro.BTYPE");
    this->Internal->head->setBtype(inputString);
    inputString = inputVolume->GetAttribute("SlicerAstro.OBJECT");
    this->Internal->head->setName(inputString);
    inputString = inputVolume->GetAttribute("SlicerAstro.TELESCOP");
    this->Internal->head->setTelesc(inputString);
    inputString = inputVolume->GetAttribute("SlicerAstro.DUNIT3");
    this->Internal->head->setDunit3(inputString);
    this->Internal->head->setDrval3(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DRVAL3")));
    this->Internal->head->setDataMin(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMIN")));
    this->Internal->head->setDataMax(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMAX")));
    if(!this->Internal->head->saveWCSStruct(inputVolume->GetAstroVolumeDisplayNode()->GetWCSStruct()))
      {
      inputVolumeDisplay->SetOpticalVelocityDefinition();
      vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : the WCS copy failed!");
      return 0;
      }

    this->Internal->head->calcArea();

    switch (DataType)
      {
      case VTK_FLOAT:
        {
        if (!this->Internal->cubeF)
          {
          this->Internal->cubeF = new Cube<float>;
          }

        this->Internal->cubeF->saveParam(*this->Internal->par);
        this->Internal->cubeF->saveHead(*this->Internal->head);
        this->Internal->cubeF->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BBCROTA")));
        this->Internal->cubeF->setCube(static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer()), dims);

        this->Internal->fitF = new Model::Galfit<float>(this->Internal->cubeF);
        // Calculate the total flux inside last ring in data
        float *ringreg = this->Internal->fitF->getFinalRingsRegion();
        this->Internal->totflux_data = 0.;
        MomentMap<float> *totalmap = new MomentMap<float>;
        totalmap->input(this->Internal->cubeF);
        totalmap->SumMap(true);

        for (size_t ii = 0; ii < (size_t)this->Internal->cubeF->DimX() * this->Internal->cubeF->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]) && !isNaN(totalmap->Array(ii)))
            {
            this->Internal->totflux_data += totalmap->Array(ii);
            }
          }
        delete totalmap;
        totalmap = NULL;
        delete ringreg;
        ringreg = NULL;
        break;
        }
      case VTK_DOUBLE:
        {
        if (!this->Internal->cubeD)
          {
          this->Internal->cubeD = new Cube<double>;
          }

        this->Internal->cubeD->saveParam(*this->Internal->par);
        this->Internal->cubeD->saveHead(*this->Internal->head);
        this->Internal->cubeD->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BBCROTA")));
        this->Internal->cubeD->setCube(static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer()), dims);

        this->Internal->fitD = new Model::Galfit<double>(this->Internal->cubeD);
        // Calculate the total flux inside last ring in data
        double *ringreg = this->Internal->fitD->getFinalRingsRegion();
        this->Internal->totflux_data = 0.;
        MomentMap<double> *totalmap = new MomentMap<double>;
        totalmap->input(this->Internal->cubeD);
        totalmap->SumMap(true);

        for (size_t ii = 0; ii < (size_t)this->Internal->cubeD->DimX() * this->Internal->cubeD->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]) && !isNaN(totalmap->Array(ii)))
            {
            this->Internal->totflux_data += totalmap->Array(ii);
            }
          }
        delete totalmap;
        totalmap = NULL;
        delete ringreg;
        ringreg = NULL;
        break;
        }
      }
    }

  vtkMRMLTableNode* paramsTableNode = pnode->GetParamsTableNode();
  if (!paramsTableNode)
    {
    inputVolumeDisplay->SetOpticalVelocityDefinition();
    vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                  "Unable to find the parameters table.");
    return 0;
    }

  switch (DataType)
    {
    case VTK_FLOAT:
      {
      if (!this->Internal->fitF || !this->Internal->fitF->Outrings())
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find fitF pointer.");
        return 0;
        }

      float *inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer());
      if (!inFPixel)
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find inFPixel pointer.");
        return 0;
        }

      this->Internal->fitF->Outrings()->xpos.clear();
      this->Internal->fitF->Outrings()->ypos.clear();
      this->Internal->fitF->Outrings()->vsys.clear();
      this->Internal->fitF->Outrings()->radii.clear();
      this->Internal->fitF->Outrings()->vrot.clear();
      this->Internal->fitF->Outrings()->vdisp.clear();
      this->Internal->fitF->Outrings()->dens.clear();
      this->Internal->fitF->Outrings()->z0.clear();
      this->Internal->fitF->Outrings()->inc.clear();
      this->Internal->fitF->Outrings()->phi.clear();
      this->Internal->fitF->Outrings()->pa.clear();
      this->Internal->fitF->Outrings()->nv.clear();
      this->Internal->fitF->Outrings()->vrad.clear();

      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        this->Internal->fitF->Outrings()->xpos.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnXPos).c_str()));
        this->Internal->fitF->Outrings()->ypos.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnYPos).c_str()));
        this->Internal->fitF->Outrings()->vsys.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVSys).c_str()) - VsysShift);
        this->Internal->fitF->Outrings()->radii.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnRadii).c_str()) * 0.5);
        this->Internal->fitF->Outrings()->vrot.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVRot).c_str()));
        this->Internal->fitF->Outrings()->vrad.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVRad).c_str()));
        this->Internal->fitF->Outrings()->vdisp.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVDisp).c_str()));
        this->Internal->fitF->Outrings()->dens.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnDens).c_str()) * 1.E+20);
        this->Internal->fitF->Outrings()->z0.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnZ0).c_str()) /
                    KpcPerArc(this->Internal->par->getDistance()));
        this->Internal->fitF->Outrings()->inc.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnInc).c_str()));
        this->Internal->fitF->Outrings()->phi.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnPhi).c_str()));
        this->Internal->fitF->Outrings()->pa.push_back(0.);
        this->Internal->fitF->Outrings()->nv.push_back(0);
        }

      // Check if Radii are in ascending order
      for (int ii = 1; ii < pnode->GetNumberOfRings(); ii++)
        {
        if (this->Internal->fitF->Outrings()->radii[ii] < this->Internal->fitF->Outrings()->radii[ii-1])
          {
          inputVolumeDisplay->SetOpticalVelocityDefinition();
          vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                        "Radii are not in ascending order.");
          return 0;
          }
        }

      this->Internal->fitF->In()->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BBCROTA")));
      float *ringreg = this->Internal->fitF->getFinalRingsRegion(); 
      if (!ringreg)
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find ringreg pointer.");
        return 0;
        }

      float totflux_model = 0.;
      this->Internal->totflux_data = 0.;
      MomentMap<float> *totalmap = new MomentMap<float>;
      totalmap->input(this->Internal->cubeF);
      totalmap->SumMap(true);

      // Calculate the total flux inside last ring in data
      for (size_t ii = 0; ii < (size_t)this->Internal->cubeF->DimX() * this->Internal->cubeF->DimY(); ii++)
        {
        if (!isNaN(ringreg[ii]) && !isNaN(totalmap->Array(ii)))
          {
          this->Internal->totflux_data += totalmap->Array(ii);
          }
        }

      float mass = 2.365E5 * FluxtoJy(this->Internal->totflux_data, *this->Internal->head)
        * fabs(DeltaVel<float>(*this->Internal->head))
        * this->Internal->par->getDistance() * this->Internal->par->getDistance();

      // Calculate radial profile along the output rings
      float meanPA = findMean<float>(&this->Internal->fitF->Outrings()->phi[0],
                                     this->Internal->fitF->Outrings()->nr);
      int nseg = 1;
      float segments[4] = {0, 360., 0., 0};
      if (this->Internal->par->getSIDE() != "A")
        {
        nseg = 2;
        segments[2] = -90;
        segments[3] = 90;
        }
      else if (this->Internal->par->getSIDE() == "R")
        {
        nseg = 2;
        segments[2] = 90;
        segments[3] = -90;
        }
      if (meanPA > 180)
        {
        std::swap(segments[2], segments[3]);
        }

      Tasks::Ellprof<float> ell(totalmap, this->Internal->fitF->Outrings(), nseg, segments);
      ell.setOptions(mass, this->Internal->par->getDistance());  //To set the mass and the distance
      ell.RadialProfile();

      if (!strcmp(pnode->GetNormalize(), "AZIM"))
        {
        double profmin = FLT_MAX;
        for (size_t i = 0; i < (size_t)this->Internal->fitF->Outrings()->nr; i++)
          {
          double mean = ell.getMean(i);
          if (!isNaN(mean) && profmin > mean && mean > 0)
            {
            profmin = mean;
            }
          }
        float factor = 1;
        while(profmin < 0.1)
          {
          profmin *= 10;
          factor *= 10;
          }
        while (profmin > 10)
          {
          profmin /= 10;
          factor /= 10;
          }
        for (size_t i = 0; i < (size_t)this->Internal->fitF->Outrings()->nr; i++)
          {
          this->Internal->fitF->Outrings()->dens[i] = factor * fabs(ell.getMean(i)) * 1E20;
          if (this->Internal->fitF->Outrings()->dens[i] == 0)
            {
            this->Internal->fitF->Outrings()->dens[i] = profmin * 1E20;
            }
          }
        }

      this->Internal->modF = this->Internal->fitF->getModel();
      if (!this->Internal->modF || !this->Internal->modF->Out())
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find modF pointer.");
        return 0;
        }

      this->Internal->modF->Out()->Head().setMinMax(0.,0.);
      float *outarray = this->Internal->modF->Out()->Array();
      if (!outarray)
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find outarray pointer.");
        return 0;
        }

      if (!strcmp(pnode->GetNormalize(), "AZIM") ||
          !strcmp(pnode->GetNormalize(), "NONE"))
        {
        // The final model has been build from the azimuthal profile calculated before,
        // thus just need to rescale the model to the total flux of data inside last ring.

        // Calculate total flux of model within last ring
        for (size_t ii = 0; ii < (size_t)this->Internal->cubeF->DimX() * this->Internal->cubeF->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]))
            {
            for (size_t z = 0; z < (size_t)this->Internal->cubeF->DimZ(); z++)
              {
              totflux_model += outarray[ii + z * this->Internal->cubeF->DimY() * this->Internal->cubeF->DimX()];
              }
            }
          }

        double factor = this->Internal->totflux_data/totflux_model;
        for (int ii = 0; ii < this->Internal->cubeF->NumPix(); ii++)
          {
          outarray[ii] *= factor;
          }
        }
      if (!strcmp(pnode->GetNormalize(), "LOCAL"))
        {
        // The final model is normalized pixel-by-pixel, i.e. in each spaxel, the total
        // flux of observation and model is eventually equal.

        for (int y = 0; y < this->Internal->cubeF->DimY(); y++)
          {
          for (int x = 0; x < this->Internal->cubeF->DimX(); x++)
            {
            float factor = 0;
            float modSum = 0;
            float obsSum = 0;
            for (int z = 0; z < this->Internal->cubeF->DimZ(); z++)
              {
              long Pix = this->Internal->cubeF->nPix(x,y,z);
              modSum += outarray[Pix];
              obsSum += this->Internal->cubeF->Array(Pix) * this->Internal->cubeF->Mask()[Pix];
              }
            if (modSum != 0)
              {
              factor = obsSum / modSum;
              }

            for (int z = 0; z < this->Internal->cubeF->DimZ(); z++)
              {
              outarray[this->Internal->cubeF->nPix(x,y,z)] *= factor;
              }
            }
          }
        }

      float *outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer());
      if (!outFPixel)
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find outFPixel pointer.");
        return 0;
        }
      for (int ii = 0; ii < numElements; ii++)
        {
        *(outFPixel + ii) = *(outarray + ii);
        }

      if (inputVolume && residualVolume)
        {
        float *residualFPixel = static_cast<float*> (residualVolume->GetImageData()->GetScalarPointer());
        if (!residualFPixel)
          {
          inputVolumeDisplay->SetOpticalVelocityDefinition();
          vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                        "Unable to find residualFPixel pointer.");
          return 0;
          }
        for (int ii = 0; ii < numElements; ii++)
          {
          if (*(outFPixel + ii) < 1.E-6)
            {
            *(residualFPixel + ii) = *(inFPixel + ii);
            }
          else
            {
            *(residualFPixel + ii) = 0.;
            }
          }
        residualFPixel = NULL;
        }

      delete totalmap;
      totalmap = NULL;
      delete ringreg;
      ringreg = NULL;
      outarray = NULL;
      if (this->Internal->modF != NULL)
        {
        delete this->Internal->modF;
        this->Internal->modF = NULL;
        }
      outFPixel = NULL;
      inFPixel = NULL;
      break;
      }
    case VTK_DOUBLE:
      {
      if (!this->Internal->fitD || !this->Internal->fitD->Outrings())
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find fitD pointer.");
        return 0;
        }

      double *inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer());
      if (!inDPixel)
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find inDPixel pointer.");
        return 0;
        }

      this->Internal->fitD->Outrings()->xpos.clear();
      this->Internal->fitD->Outrings()->ypos.clear();
      this->Internal->fitD->Outrings()->vsys.clear();
      this->Internal->fitD->Outrings()->radii.clear();
      this->Internal->fitD->Outrings()->vrot.clear();
      this->Internal->fitD->Outrings()->vdisp.clear();
      this->Internal->fitD->Outrings()->dens.clear();
      this->Internal->fitD->Outrings()->z0.clear();
      this->Internal->fitD->Outrings()->inc.clear();
      this->Internal->fitD->Outrings()->phi.clear();
      this->Internal->fitD->Outrings()->pa.clear();
      this->Internal->fitD->Outrings()->nv.clear();
      this->Internal->fitD->Outrings()->vrad.clear();

      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        this->Internal->fitD->Outrings()->xpos.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnXPos).c_str()));
        this->Internal->fitD->Outrings()->ypos.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnYPos).c_str()));
        this->Internal->fitD->Outrings()->vsys.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVSys).c_str()) - VsysShift);
        this->Internal->fitD->Outrings()->radii.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnRadii).c_str()) * 0.5);
        this->Internal->fitD->Outrings()->vrot.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVRot).c_str()));
        this->Internal->fitD->Outrings()->vrad.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVRad).c_str()));
        this->Internal->fitD->Outrings()->vdisp.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVDisp).c_str()));
        this->Internal->fitD->Outrings()->dens.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnDens).c_str()) * 1.E+20);
        this->Internal->fitD->Outrings()->z0.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnZ0).c_str()) /
                    KpcPerArc(this->Internal->par->getDistance()));
        this->Internal->fitD->Outrings()->inc.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnInc).c_str()));
        this->Internal->fitD->Outrings()->phi.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnPhi).c_str()));
        this->Internal->fitD->Outrings()->pa.push_back(0.);
        this->Internal->fitD->Outrings()->nv.push_back(0);
        }

      // Check if Radii are in ascending order
      for (int ii = 1; ii < pnode->GetNumberOfRings(); ii++)
        {
        if (this->Internal->fitD->Outrings()->radii[ii] < this->Internal->fitD->Outrings()->radii[ii-1])
          {
          inputVolumeDisplay->SetOpticalVelocityDefinition();
          vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                        "Radii are not in ascending order.");
          return 0;
          }
        }

      this->Internal->fitD->In()->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.BBCROTA")));
      double *ringreg = this->Internal->fitD->getFinalRingsRegion();
      if (!ringreg)
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find ringreg pointer.");
        return 0;
        }

      double totflux_model = 0.;
      this->Internal->totflux_data = 0.;
      MomentMap<double> *totalmap = new MomentMap<double>;
      totalmap->input(this->Internal->cubeD);
      totalmap->SumMap(true);

      // Calculate the total flux inside last ring in data
      for (size_t ii = 0; ii < (size_t)this->Internal->cubeD->DimX() * this->Internal->cubeD->DimY(); ii++)
        {
        if (!isNaN(ringreg[ii]) && !isNaN(totalmap->Array(ii)))
          {
          this->Internal->totflux_data += totalmap->Array(ii);
          }
        }

      double mass = 2.365E5 * FluxtoJy(this->Internal->totflux_data, *this->Internal->head)
        * fabs(DeltaVel<double>(*this->Internal->head))
        * this->Internal->par->getDistance() * this->Internal->par->getDistance();

      // Calculate radial profile along the output rings
      double meanPA = findMean<double>(&this->Internal->fitD->Outrings()->phi[0],
                                       this->Internal->fitD->Outrings()->nr);
      int nseg = 1;
      float segments[4] = {0, 360., 0., 0};
      if (this->Internal->par->getSIDE() != "A")
        {
        nseg = 2;
        segments[2] = -90;
        segments[3] = 90;
        }
      else if (this->Internal->par->getSIDE() == "R")
        {
        nseg = 2;
        segments[2] = 90;
        segments[3] = -90;
        }
      if (meanPA > 180)
        {
        std::swap(segments[2], segments[3]);
        }

      Tasks::Ellprof<double> ell(totalmap, this->Internal->fitD->Outrings(), nseg, segments);
      ell.setOptions(mass, this->Internal->par->getDistance());  //To set the mass and the distance
      ell.RadialProfile();

      if (!strcmp(pnode->GetNormalize(), "AZIM"))
        {
        double profmin = FLT_MAX;
        for (size_t i = 0; i < (size_t)this->Internal->fitD->Outrings()->nr; i++)
          {
          double mean = ell.getMean(i);
          if (!isNaN(mean) && profmin > mean && mean > 0)
            {
            profmin = mean;
            }
          }
        double factor = 1;
        while(profmin < 0.1)
          {
          profmin *= 10;
          factor *= 10;
          }
        while (profmin > 10)
          {
          profmin /= 10;
          factor /= 10;
          }
        for (size_t i = 0; i < (size_t)this->Internal->fitD->Outrings()->nr; i++)
          {
          this->Internal->fitD->Outrings()->dens[i] = factor * fabs(ell.getMean(i)) * 1E20;
          if (this->Internal->fitD->Outrings()->dens[i] == 0)
            {
            this->Internal->fitD->Outrings()->dens[i] = profmin * 1E20;
            }
          }
        }

      this->Internal->modD = this->Internal->fitD->getModel();
      if (!this->Internal->modD || !this->Internal->modD->Out())
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find modF pointer.");
        return 0;
        }

      this->Internal->modD->Out()->Head().setMinMax(0.,0.);
      double *outarray = this->Internal->modD->Out()->Array();
      if (!outarray)
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find outarray pointer.");
        return 0;
        }

      if (!strcmp(pnode->GetNormalize(), "AZIM") ||
          !strcmp(pnode->GetNormalize(), "NONE"))
        {
        // The final model has been build from the azimuthal profile calculated before,
        // thus just need to rescale the model to the total flux of data inside last ring.

        // Calculate total flux of model within last ring
        for (size_t ii = 0; ii < (size_t)this->Internal->cubeD->DimX() * this->Internal->cubeD->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]))
            {
            for (size_t z = 0; z < (size_t)this->Internal->cubeD->DimZ(); z++)
              {
              totflux_model += outarray[ii + z * this->Internal->cubeD->DimY() * this->Internal->cubeD->DimX()];
              }
            }
          }

        double factor = this->Internal->totflux_data / totflux_model;
        for (int ii = 0; ii < this->Internal->cubeD->NumPix(); ii++)
          {
          outarray[ii] *= factor;
          }
        }
      if (!strcmp(pnode->GetNormalize(), "LOCAL"))
        {
        // The final model is normalized pixel-by-pixel, i.e. in each spaxel, the total
        // flux of observation and model is eventually equal.

        for (int y = 0; y < this->Internal->cubeD->DimY(); y++)
          {
          for (int x = 0; x < this->Internal->cubeD->DimX(); x++)
            {
            double factor = 0;
            double modSum = 0;
            double obsSum = 0;
            for (int z = 0; z < this->Internal->cubeD->DimZ(); z++)
              {
              long Pix = this->Internal->cubeD->nPix(x,y,z);
              modSum += outarray[Pix];
              obsSum += this->Internal->cubeD->Array(Pix) * this->Internal->cubeD->Mask()[Pix];
              }
            if (modSum != 0)
              {
              factor = obsSum / modSum;
              }

            for (int z = 0; z < this->Internal->cubeD->DimZ(); z++)
              {
              outarray[this->Internal->cubeD->nPix(x,y,z)] *= factor;
              }
            }
          }
        }

      double *outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer());
      if (!outDPixel)
        {
        inputVolumeDisplay->SetOpticalVelocityDefinition();
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                      "Unable to find outDPixel pointer.");
        return 0;
        }
      for (int ii = 0; ii < numElements; ii++)
        {
        *(outDPixel + ii) = *(outarray + ii);
        }

      if (inputVolume && residualVolume)
        {
        double *residualDPixel = static_cast<double*> (residualVolume->GetImageData()->GetScalarPointer());
        if (!residualDPixel)
          {
          inputVolumeDisplay->SetOpticalVelocityDefinition();
          vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : "
                        "Unable to find residualDPixel pointer.");
          return 0;
          }
        for (int ii = 0; ii < numElements; ii++)
          {
          if (*(outDPixel + ii) < 1.E-6)
            {
            *(residualDPixel + ii) = *(inDPixel + ii);
            }
          else
            {
            *(residualDPixel + ii) = 0.;
            }
          }
        residualDPixel = NULL;
        }

      delete totalmap;
      totalmap = NULL;
      delete ringreg;
      ringreg = NULL;
      outarray = NULL;
      if (this->Internal->modD != NULL)
        {
        delete this->Internal->modD;
        this->Internal->modD = NULL;
        }
      outDPixel = NULL;
      inDPixel = NULL;
      break;
      }
    }

  // Transform back the velocity definition to optical one
  if (!inputVolumeDisplay->SetOpticalVelocityDefinition())
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::OperateModel :"
                  " Conversion to optical velocity definition failed!");
    return 0;
    }

  return 1;
}
