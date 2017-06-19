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

// Logic includes
#include "vtkSlicerAstroVolumeLogic.h"
#include "vtkSlicerAstroModelingLogic.h"
#include "vtkSlicerAstroConfigure.h"

//Bbarolo includes
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
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
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
  vtkSmartPointer<vtkImageData> tempVolumeData;
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
  this->AstroVolumeLogic = vtkSmartPointer<vtkSlicerAstroVolumeLogic>::New();
  this->tempVolumeData = vtkSmartPointer<vtkImageData>::New();
  this->par = new Param;
  this->head = new Header;
  this->cubeF = new Cube<float>;
  this->cubeD = new Cube<double>;
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
std::string FloatToString(float Value)
{
  return NumberToString<float>(Value);
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
int vtkSlicerAstroModelingLogic::FitModel(vtkMRMLAstroModelingParametersNode* pnode)
{
  int wasModifying = 0;

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel :"
                  " inputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if(!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel :"
                  " outputVolume not found!");
    return 0;
    }

  vtkMRMLAstroVolumeNode *residualVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetResidualVolumeNodeID()));
  if(!residualVolume)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel :"
                  " residualVolume not found!");
    return 0;
    }

  vtkMRMLAstroLabelMapVolumeNode *maskVolume =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetMaskVolumeNodeID()));

  bool maskActive = pnode->GetMaskActive();

  if(!maskVolume && maskActive)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel :"
                  " maskVolume not found!");
    return 0;
    }

  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();

  string file = inputVolume->GetName();

  if (!this->Internal->par || !this->Internal->head)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel :"
                  " Internal pointers not defined!");
    return 0;
    }

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

    if (pnode->GetNumberOfRings() > 1)
      {
      this->Internal->par->setNRADII(pnode->GetNumberOfRings());
      }
    else if (pnode->GetNumberOfRings() <= 1)
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel :"
                      " Barolo needs at least 2 Rings!");
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
      this->Internal->par->setVSYS(DoubleToString(pnode->GetSystemicVelocity()));
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
    }
  else
    {
    this->Internal->par->setSearch(true);
    this->Internal->par->setMASK("SMOOTH");
    this->Internal->par->setBeamFWHM(this->Internal->par->getBeamFWHM()/3600.);
    }

  this->Internal->par->setTOL(1.E-3);

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
  this->Internal->head->setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
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
    vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel : the WCS copy failed!")
    }

  this->Internal->head->calcArea();

  if (pnode->GetStatus() == -1)
    {
    pnode->SetStatus(0);
    pnode->SetFitSuccess(false);
    return 0;
    }
  pnode->SetStatus(10);

  int *dims = outputVolume->GetImageData()->GetDimensions();
  int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;

  switch (DataType)
    {
    case VTK_FLOAT:
      {
      if (!this->Internal->cubeF)
        {
        vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel :"
                      " Internal pointers not defined!");
        return 0;
        }

      this->Internal->cubeF->saveParam(*this->Internal->par);
      this->Internal->cubeF->saveHead(*this->Internal->head);
      this->Internal->cubeF->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
      this->Internal->cubeF->setCube(static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer()), dims);

      // Feed segmentation mask to cube
      if (maskActive)
        {
        short* segmentationMaskPointer = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer());
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
        }

      // Searching stuff if the user has not provided a mask
      if (this->Internal->par->getSearch())
        {
        this->Internal->cubeF->Search();
        }

      // Cube Fitting
      if (this->Internal->par->getflagGalFit())
        {
        this->Internal->fitF = new Model::Galfit<float>(this->Internal->cubeF);

        *this->Internal->par = this->Internal->cubeF->pars();

        wasModifying = pnode->StartModify();

        pnode->SetNumberOfRings(this->Internal->par->getNRADII());
        pnode->SetRadSep(this->Internal->par->getRADSEP());
        pnode->SetXCenter(StringToDouble(this->Internal->par->getXPOS().c_str()));
        pnode->SetYCenter(StringToDouble(this->Internal->par->getYPOS().c_str()));
        pnode->SetSystemicVelocity(StringToDouble(this->Internal->par->getVSYS().c_str()));
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

        bool success = this->Internal->fitF->galfit(pnode->GetStatusPointer());

        if (!success)
          {
          pnode->SetStatus(0);
          pnode->SetFitSuccess(false);
          return 0;
          }

        if (this->Internal->par->getTwoStage())
          {
          if (pnode->GetStatus() == -1)
            {
            pnode->SetStatus(0);
            pnode->SetFitSuccess(false);
            return 0;
            }
          pnode->SetStatus(60);

          bool success = this->Internal->fitF->SecondStage(pnode->GetStatusPointer());
          if (!success)
            {
            pnode->SetStatus(0);
            pnode->SetFitSuccess(false);
            return 0;
            }
          }

        if (pnode->GetStatus() == -1)
          {
          pnode->SetStatus(0);
          pnode->SetFitSuccess(false);
          return 0;
          }
        pnode->SetStatus(80);

        // Calculate the total flux inside last ring in data
        float *ringreg = this->Internal->fitF->getFinalRingsRegion();
        float totflux_model = 0.;
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

        this->Internal->modF = this->Internal->fitF->getModel();
        float *outarray = this->Internal->modF->Out()->Array();

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
            if (modSum!=0)
              {
              factor = obsSum/modSum;
              }

            for (int z=0; z<this->Internal->cubeF->DimZ(); z++)
              {
              outarray[this->Internal->cubeF->nPix(x,y,z)] *= factor;
              }
            }
          }

        float *outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer());

        for (int ii = 0; ii < numElements; ii++)
          {
          *(outFPixel + ii) = *(outarray + ii);
          }

        float *inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer());
        float *residualFPixel = static_cast<float*> (residualVolume->GetImageData()->GetScalarPointer());

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
        }
      break;
      }
    case VTK_DOUBLE:
      {
      if (!this->Internal->cubeD)
        {
        vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel :"
                      " Internal pointers not defined!");
        return 0;
        }

      this->Internal->cubeD->saveParam(*this->Internal->par);
      this->Internal->cubeD->saveHead(*this->Internal->head);
      this->Internal->cubeD->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
      this->Internal->cubeD->setCube(static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer()), dims);

      // Feed segmentation mask to cube
      if (maskActive)
        {
        short* segmentationMaskPointer = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer());
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
        }

      // Searching stuff
      if (this->Internal->par->getSearch())
        {
        this->Internal->cubeD->Search();
        }

      // Cube Fitting
      if (this->Internal->par->getflagGalFit())
        {
        this->Internal->fitD = new Model::Galfit<double>(this->Internal->cubeD);

        *this->Internal->par = this->Internal->cubeD->pars();

        wasModifying = pnode->StartModify();

        pnode->SetNumberOfRings(this->Internal->par->getNRADII());
        pnode->SetRadSep(this->Internal->par->getRADSEP());
        pnode->SetXCenter(StringToDouble(this->Internal->par->getXPOS().c_str()));
        pnode->SetYCenter(StringToDouble(this->Internal->par->getYPOS().c_str()));
        pnode->SetSystemicVelocity(StringToDouble(this->Internal->par->getVSYS().c_str()));
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

        bool success = this->Internal->fitD->galfit(pnode->GetStatusPointer());

        if (!success)
          {
          pnode->SetStatus(0);
          pnode->SetFitSuccess(false);
          return 0;
          }

        if (this->Internal->par->getTwoStage())
          {
          if (pnode->GetStatus() == -1)
            {
            pnode->SetStatus(0);
            pnode->SetFitSuccess(false);
            return 0;
            }
          pnode->SetStatus(60);
          bool success = this->Internal->fitD->SecondStage(pnode->GetStatusPointer());
          if (!success)
            {
            pnode->SetStatus(0);
            pnode->SetFitSuccess(false);
            return 0;
            }
          }

        if (pnode->GetStatus() == -1)
          {
          pnode->SetStatus(0);
          pnode->SetFitSuccess(false);
          return 0;
          }
        pnode->SetStatus(80);

        // Calculate the total flux inside last ring in data
        double *ringreg = this->Internal->fitD->getFinalRingsRegion();
        double totflux_model = 0.;
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

        Model::Galmod<double> *mod = this->Internal->fitD->getModel();
        double *outarray = mod->Out()->Array();

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

        double factor = this->Internal->totflux_data/totflux_model;
        for (int ii = 0; ii < this->Internal->cubeD->NumPix(); ii++)
          {
          outarray[ii] *= factor;
          }

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
            if (modSum!=0)
              {
              factor = obsSum/modSum;
              }

            for (int z=0; z<this->Internal->cubeD->DimZ(); z++)
              {
              outarray[this->Internal->cubeD->nPix(x,y,z)] *= factor;
              }
            }
          }

        double *outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer());

        for (int ii = 0; ii < numElements; ii++)
          {
          *(outDPixel + ii) = *(outarray + ii);
          }

        double *inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer());
        double *residualDPixel = static_cast<double*> (residualVolume->GetImageData()->GetScalarPointer());

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
        }
      break;
      }
    }

  pnode->SetStatus(100);
  pnode->SetStatus(0);
  pnode->SetFitSuccess(true);

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::UpdateTableFromModel(vtkMRMLAstroModelingParametersNode *pnode)
{
  vtkMRMLTableNode* paramsTableNode = pnode->GetParamsTableNode();
  if (!paramsTableNode)
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateTable : Unable to find the parameters table.");
    return 0;
    }

  int wasModifying = paramsTableNode->StartModify();

  paramsTableNode->RemoveAllColumns();
  paramsTableNode->SetUseColumnNameAsColumnHeader(true);

  if(this->Internal->fitF)
    {

    for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
      {
      paramsTableNode->AddEmptyRow();
      }

    vtkStringArray* Radii = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Radii->SetName("Radius (arcsec)");
    vtkStringArray* VRot = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    VRot->SetName("VRot (km/s)");
    vtkStringArray* VRad = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    VRad->SetName("VRad (km/s)");
    vtkStringArray* Inc = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Inc->SetName("Inc (degree)");
    vtkStringArray* Phi = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Phi->SetName("Phi (degree)");
    vtkStringArray* VSys = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    VSys->SetName("VSys (km/s)");
    vtkStringArray* VDisp = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    VDisp->SetName("VDisp (km/s)");
    vtkStringArray* Dens = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Dens->SetName("Column Density (10^20 cm^-2)");
    vtkStringArray* Z0 = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Z0->SetName("Scale Heigth (Kpc)");
    vtkStringArray* XPos = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    XPos->SetName("X Center (pixels)");
    vtkStringArray* YPos = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    YPos->SetName("Y Center (pixels)");

    paramsTableNode->RemoveColumn(0);

    if (this->Internal->fitF->Outrings()->xpos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        XPos->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->xpos[ii]).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->ypos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        YPos->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->ypos[ii]).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->vsys.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VSys->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->vsys[ii]).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->radii.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Radii->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->radii[ii]).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->vrot.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRot->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->vrot[ii]).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->vrad.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRad->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->vrad[ii]).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->vdisp.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VDisp->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->vdisp[ii]).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->dens.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Dens->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->dens[ii] * 1.E-20).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->z0.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Z0->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->z0[ii]
                                      * KpcPerArc(this->Internal->par->getDistance())).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->inc.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Inc->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->inc[ii]).c_str());
        }
      }

    if (this->Internal->fitF->Outrings()->phi.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Phi->SetValue(ii, FloatToString(this->Internal->fitF->Outrings()->phi[ii]).c_str());
        }
      }
    }
  else if (this->Internal->fitD)
    {
    for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
      {
      paramsTableNode->AddEmptyRow();
      }

    vtkStringArray* Radii = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Radii->SetName("Radius (arcsec)");
    vtkStringArray* VRot = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    VRot->SetName("VRot (km/s)");
    vtkStringArray* VRad = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    VRad->SetName("VRot (km/s)");
    vtkStringArray* Inc = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Inc->SetName("Inc (degree)");
    vtkStringArray* Phi = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Phi->SetName("Phi (degree)");
    vtkStringArray* VSys = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    VSys->SetName("VSys (km/s)");
    vtkStringArray* VDisp = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    VDisp->SetName("VDisp (km/s)");
    vtkStringArray* Dens = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Dens->SetName("Column Density (10^20 cm^-2)");
    vtkStringArray* Z0 = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    Z0->SetName("Scale Heigth (Kpc)");
    vtkStringArray* XPos = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    XPos->SetName("X Center (pixels)");
    vtkStringArray* YPos = vtkStringArray::SafeDownCast(paramsTableNode->AddColumn());
    YPos->SetName("Y Center (pixels)");

    paramsTableNode->RemoveColumn(0);

    if (this->Internal->fitD->Outrings()->xpos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        XPos->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->xpos[ii]).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->ypos.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        YPos->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->ypos[ii]).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->vsys.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VSys->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->vsys[ii]).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->radii.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Radii->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->radii[ii]).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->vrot.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRot->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->vrot[ii]).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->vrad.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VRad->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->vrad[ii]).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->vdisp.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        VDisp->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->vdisp[ii]).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->dens.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Dens->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->dens[ii] * 1.E-20).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->z0.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Z0->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->z0[ii]
                                      * KpcPerArc(this->Internal->par->getDistance())).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->inc.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Inc->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->inc[ii]).c_str());
        }
      }

    if (this->Internal->fitD->Outrings()->phi.size() > 0)
      {
      for (int ii = 0; ii < pnode->GetNumberOfRings(); ii++)
        {
        Phi->SetValue(ii, DoubleToString(this->Internal->fitD->Outrings()->phi[ii]).c_str());
        }
      }
    }
  else
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateTable : galfit not found! \n");
    return 0;
    }

  paramsTableNode->EndModify(wasModifying);

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroModelingLogic::UpdateModelFromTable(vtkMRMLAstroModelingParametersNode *pnode)
{
  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    vtkWarningMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " inputVolume not found!");
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

  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  int *dims = outputVolume->GetImageData()->GetDimensions();
  int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;

  if (!this->Internal->fitF && !this->Internal->fitD)
    { 
    vtkMRMLAstroVolumeNode *inputVolume =
      vtkMRMLAstroVolumeNode::SafeDownCast
        (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
    if(!inputVolume)
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " inputVolume not found!");
      return 0;
      }

    if (!this->Internal->par || !this->Internal->head)
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                    " Internal pointers not defined!");
      return 0;
      }

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
      this->Internal->par->setVSYS(DoubleToString(pnode->GetSystemicVelocity()));
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
      vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : /n"
                    "The scene has been imported and the fitting has not been runned. /n"
                    "You can procede to modify the model, but all the input fitting /n"
                    "parameters have to be inserted manually. /n"
                    "You may check if the parameters are stored in the ModelingParameterNode.");
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
    this->Internal->head->setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
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
      vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : the WCS copy failed!")
      }

    this->Internal->head->calcArea();

    switch (DataType)
      {
      case VTK_FLOAT:
        {
        if (!this->Internal->cubeF)
          {
          vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                        " Internal pointers not defined!");
          return 0;
          }

        this->Internal->cubeF->saveParam(*this->Internal->par);
        this->Internal->cubeF->saveHead(*this->Internal->head);
        this->Internal->cubeF->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
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
        break;
        }
      case VTK_DOUBLE:
        {
        if (!this->Internal->cubeD)
          {
          vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable :"
                        " Internal pointers not defined!");
          return 0;
          }

        this->Internal->cubeD->saveParam(*this->Internal->par);
        this->Internal->cubeD->saveHead(*this->Internal->head);
        this->Internal->cubeD->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
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
        break;
        }
      }
    }

  vtkMRMLTableNode* paramsTableNode = pnode->GetParamsTableNode();
    if (!paramsTableNode)
      {
      vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : Unable to find the parameters table.");
      return 0;
      }

  switch (DataType)
    {
    case VTK_FLOAT:
      {
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
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVSys).c_str()));
        this->Internal->fitF->Outrings()->radii.push_back(
            StringToFloat(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnRadii).c_str()));
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

      this->Internal->fitF->In()->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
      float *ringreg = this->Internal->fitF->getFinalRingsRegion();
      this->Internal->modF = this->Internal->fitF->getModel();
      float *outarray = this->Internal->modF->Out()->Array();
      float totflux_model = 0.;

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

      if (this->Internal->totflux_data < 1.E-6)
        {
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
        }

      if (this->Internal->totflux_data < 1.E-6)
        {
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : this->Internal->totflux_data is zero!");
        return 0;
        }

      double factor = this->Internal->totflux_data/totflux_model;
      for (int ii = 0; ii < this->Internal->cubeF->NumPix(); ii++)
        {
        outarray[ii] *= factor;
        }

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
          if (modSum!=0)
            {
            factor = obsSum/modSum;
            }

          for (int z=0; z<this->Internal->cubeF->DimZ(); z++)
            {
            outarray[this->Internal->cubeF->nPix(x,y,z)] *= factor;
            }
          }
        }

      float *outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer());

      for (int ii = 0; ii < numElements; ii++)
        {
        *(outFPixel + ii) = *(outarray + ii);
        }

      if (inputVolume && residualVolume)
        {
        float *inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer());
        float *residualFPixel = static_cast<float*> (residualVolume->GetImageData()->GetScalarPointer());

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
        }

      break;
      }
    case VTK_DOUBLE:
      {
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
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVSys).c_str()));
        this->Internal->fitD->Outrings()->radii.push_back(
            StringToDouble(paramsTableNode->GetCellText(
                ii, vtkMRMLAstroModelingParametersNode::ParamsColumnRadii).c_str()));
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

      this->Internal->fitD->In()->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
      double *ringreg = this->Internal->fitD->getFinalRingsRegion();
      this->Internal->modD = this->Internal->fitD->getModel();
      double *outarray = this->Internal->modD->Out()->Array();
      double totflux_model = 0.;

      if (this->Internal->totflux_data < 1.E-6)
        {
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
        }

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

      if (this->Internal->totflux_data < 1.E-6)
        {
        vtkErrorMacro("vtkSlicerAstroModelingLogic::UpdateModelFromTable : this->Internal->totflux_data is zero!");
        return 0;
        }

      double factor = this->Internal->totflux_data/totflux_model;
      for (int ii = 0; ii < this->Internal->cubeD->NumPix(); ii++)
        {
        outarray[ii] *= factor;
        }

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
          if (modSum!=0)
            {
            factor = obsSum/modSum;
            }

          for (int z=0; z<this->Internal->cubeD->DimZ(); z++)
            {
            outarray[this->Internal->cubeD->nPix(x,y,z)] *= factor;
            }
          }
        }

      double *outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer());

      for (int ii = 0; ii < numElements; ii++)
        {
        *(outDPixel + ii) = *(outarray + ii);
        }

      if (inputVolume && residualVolume)
        {
        double *inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer());
        double *residualDPixel = static_cast<double*> (residualVolume->GetImageData()->GetScalarPointer());

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
        }

      break;
      }
    }

  return 1;
}
