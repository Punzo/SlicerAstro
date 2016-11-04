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
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroModelingParametersNode.h>

// VTK includes
#include <vtkCacheManager.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
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
};

//----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = vtkSmartPointer<vtkSlicerAstroVolumeLogic>::New();
  this->tempVolumeData = vtkSmartPointer<vtkImageData>::New();
}

//---------------------------------------------------------------------------
vtkSlicerAstroModelingLogic::vtkInternal::~vtkInternal()
{
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
  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();

  Param *par = new Param;
  string file = inputVolume->GetName();
  par->setImageFile(file);
  string outputFolder = this->GetMRMLScene()->GetCacheManager()->GetRemoteCacheDirectory();
  outputFolder += "/";
  par->setOutfolder(outputFolder);
  par->setVerbosity(true);
  par->setShowbar(false);
  par->setSearch(false);
  par->setflagGalFit(true);
  /*par->setNRADII(20);
  par->setRADSEP(30);
  par->setXPOS("77");
  par->setYPOS("77");
  par->setVSYS("132.8");
  par->setVDISP("8");
  par->setVROT("120");
  par->setINC("60");
  par->setPHI("123.7");
  par->setZ0("10.");
  par->setLTYPE(2);
  par->setFTYPE(2);
  par->setDistance(3.2);
  par->setWFUNC(2);
  par->setFREE("VROT VDISP PA");*/

  if ((par->getRADII() == "-1" && (par->getNRADII() == -1 || par->getRADSEP() == -1)) ||
       par->getXPOS() == "-1" || par->getYPOS() == "-1" || par->getVSYS() == "-1" ||
       par->getVROT() == "-1" || par->getPHI() == "-1"  || par->getINC() == "-1")
    {
    par->setSearch(true);
    par->setMASK("SEARCH");
    par->setBeamFWHM(par->getBeamFWHM()/3600.);
    }


  // set header
  Header *head = new Header;

  head->setBitpix(StringToInt(inputVolume->GetAttribute("SlicerAstro.BITPIX")));
  int numAxes = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  head->setNumAx(numAxes);
  string inputString;

  for (int ii = 0; ii < numAxes; ii++)
    {
    inputString = "SlicerAstro.NAXIS";
    inputString += IntToString(ii + 1);
    head->setDimAx(ii,StringToLong(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CRPIX";
    inputString += IntToString(ii + 1);
    head->setCrpix(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CRVAL";
    inputString += IntToString(ii + 1);
    head->setCrval(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CDELT";
    inputString += IntToString(ii + 1);
    head->setCdelt(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CTYPE";
    inputString += IntToString(ii + 1);
    inputString = inputVolume->GetAttribute(inputString.c_str());
    head->setCtype(ii, inputString);
    inputString = "SlicerAstro.CUNIT";
    inputString += IntToString(ii + 1);
    inputString = inputVolume->GetAttribute(inputString.c_str());
    head->setCunit(ii, inputString);
    }

  head->setBmaj(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BMAJ")));
  head->setBmin(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BMIN")));
  head->setBpa(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BPA")));
  head->setBzero(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BZERO")));
  head->setBscale(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BSCALE")));
  head->setBlank(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BLANK")));
  head->setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
  head->setEpoch(StringToFloat(inputVolume->GetAttribute("SlicerAstro.EPOCH")));
  head->setFreq0(StringToDouble(inputVolume->GetAttribute("SlicerAstro.RESTFREQ")));
  inputString = inputVolume->GetAttribute("SlicerAstro.BUNIT");
  head->setBunit(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.BTYPE");
  head->setBtype(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.OBJECT");
  head->setName(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.TELESCOP");
  head->setTelesc(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.DUNIT3");
  head->setDunit3(inputString);
  head->setDrval3(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DRVAL3")));
  head->setDataMin(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMIN")));
  head->setDataMax(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMAX")));
  if(!head->saveWCSStruct(inputVolume->GetAstroVolumeDisplayNode()->GetWCSStruct()))
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel : the WCS copy failed!")
    }

  head->calcArea();

  if (pnode->GetStatus() == -1)
    {
    delete head;
    delete par;
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
      Cube<float> *cube = new Cube<float>;
      cube->saveParam(*par);
      cube->saveHead(*head);
      cube->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
      cube->setCube(static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer()), dims);

      // Searching stuff
      if (par->getSearch())
        {
        cube->Search();
        }

      // Cube Fitting
      if (par->getflagGalFit())
        {
        Model::Galfit<float> *fit = new Model::Galfit<float>(cube);

        int rangeUpdate [2] = { 10, 60 };
        bool success = fit->galfit(pnode->GetStatusPointer(), rangeUpdate);

        if (!success)
          {
          delete head;
          delete par;
          delete cube;
          pnode->SetStatus(0);
          pnode->SetFitSuccess(false);
          return 0;
          }

        if (par->getTwoStage())
          {
          if (pnode->GetStatus() == -1)
            {
            delete head;
            delete par;
            delete cube;
            pnode->SetStatus(0);
            pnode->SetFitSuccess(false);
            return 0;
            }
          pnode->SetStatus(60);
          rangeUpdate[0] = 60;
          rangeUpdate[0] = 80;
          bool success = fit->SecondStage(pnode->GetStatusPointer(), rangeUpdate);
          if (!success)
            {
            delete head;
            delete par;
            delete cube;
            pnode->SetStatus(0);
            pnode->SetFitSuccess(false);
            return 0;
            }
          }

        if (pnode->GetStatus() == -1)
          {
          delete head;
          delete par;
          delete cube;
          pnode->SetStatus(0);
          pnode->SetFitSuccess(false);
          return 0;
          }
        pnode->SetStatus(80);

        // Calculate the total flux inside last ring in data
        float *ringreg = fit->getFinalRingsRegion();
        float totflux_data = 0, totflux_model = 0;
        MomentMap<float> *totalmap = new MomentMap<float>;
        totalmap->input(cube);
        totalmap->SumMap(true);

        for (size_t ii = 0; ii < (size_t)cube->DimX() * cube->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]) && !isNaN(totalmap->Array(ii)))
            {
            totflux_data += totalmap->Array(ii);
            }
          }

        Model::Galmod<float> *mod = fit->getModel();
        float *outarray = mod->Out()->Array();

        // Calculate total flux of model within last ring
        for (size_t ii = 0; ii < (size_t)cube->DimX() * cube->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]))
            {
            for (size_t z = 0; z < (size_t)cube->DimZ(); z++)
              {
              totflux_model += outarray[ii + z * cube->DimY() * cube->DimX()];
              }
            }
          }

        double factor = totflux_data/totflux_model;
        for (int ii = 0; ii < cube->NumPix(); ii++)
          {
          outarray[ii] *= factor;
          }

        // The final model is normalized pixel-by-pixel, i.e. in each spaxel, the total
        // flux of observation and model is eventually equal.

        for (int y = 0; y < cube->DimY(); y++)
          {
          for (int x = 0; x < cube->DimX(); x++)
            {
            float factor = 0;
            float modSum = 0;
            float obsSum = 0;
            for (int z = 0; z < cube->DimZ(); z++)
              {
              long Pix = cube->nPix(x,y,z);
              modSum += outarray[Pix];
              obsSum += cube->Array(Pix) * cube->Mask()[Pix];
              }
            if (modSum!=0)
              {
              factor = obsSum/modSum;
              }

            for (int z=0; z<cube->DimZ(); z++)
              {
              outarray[cube->nPix(x,y,z)] *= factor;
              }
            }
          }

        float *outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer());

        for (int ii = 0; ii < numElements; ii++)
          {
          *(outFPixel + ii) = *(outarray + ii);
          }

        delete totalmap;
        delete fit;
        delete mod;
        }

      delete cube;
      break;
      }
    case VTK_DOUBLE:
      {
      Cube<double> *cube = new Cube<double>;
      cube->saveParam(*par);
      cube->saveHead(*head);
      cube->Head().setCrota(StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA")));
      cube->setCube(static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer()), dims);

      // Searching stuff
      if (par->getSearch())
        {
        cube->Search();
        }

      // Cube Fitting
      if (par->getflagGalFit())
        {
        Model::Galfit<double> *fit = new Model::Galfit<double>(cube);

        int rangeUpdate [2] = { 10, 60 };
        bool success = fit->galfit(pnode->GetStatusPointer(), rangeUpdate);

        if (!success)
          {
          delete head;
          delete par;
          delete cube;
          pnode->SetStatus(0);
          pnode->SetFitSuccess(false);
          return 0;
          }

        if (par->getTwoStage())
          {
          if (pnode->GetStatus() == -1)
            {
            delete head;
            delete par;
            delete cube;
            pnode->SetStatus(0);
            pnode->SetFitSuccess(false);
            return 0;
            }
          pnode->SetStatus(60);
          rangeUpdate[0] = 60;
          rangeUpdate[0] = 80;
          bool success = fit->SecondStage(pnode->GetStatusPointer(), rangeUpdate);
          if (!success)
            {
            delete head;
            delete par;
            delete cube;
            pnode->SetStatus(0);
            pnode->SetFitSuccess(false);
            return 0;
            }
          }

        if (pnode->GetStatus() == -1)
          {
          delete head;
          delete par;
          delete cube;
          pnode->SetStatus(0);
          pnode->SetFitSuccess(false);
          return 0;
          }
        pnode->SetStatus(80);

        // Calculate the total flux inside last ring in data
        double *ringreg = fit->getFinalRingsRegion();
        double totflux_data = 0, totflux_model = 0;
        MomentMap<double> *totalmap = new MomentMap<double>;
        totalmap->input(cube);
        totalmap->SumMap(true);

        for (size_t ii = 0; ii < (size_t)cube->DimX() * cube->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]) && !isNaN(totalmap->Array(ii)))
            {
            totflux_data += totalmap->Array(ii);
            }
          }

        Model::Galmod<double> *mod = fit->getModel();
        double *outarray = mod->Out()->Array();

        // Calculate total flux of model within last ring
        for (size_t ii = 0; ii < (size_t)cube->DimX() * cube->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]))
            {
            for (size_t z = 0; z < (size_t)cube->DimZ(); z++)
              {
              totflux_model += outarray[ii + z * cube->DimY() * cube->DimX()];
              }
            }
          }

        double factor = totflux_data/totflux_model;
        for (int ii = 0; ii < cube->NumPix(); ii++)
          {
          outarray[ii] *= factor;
          }

        // The final model is normalized pixel-by-pixel, i.e. in each spaxel, the total
        // flux of observation and model is eventually equal.

        for (int y = 0; y < cube->DimY(); y++)
          {
          for (int x = 0; x < cube->DimX(); x++)
            {
            double factor = 0;
            double modSum = 0;
            double obsSum = 0;
            for (int z = 0; z < cube->DimZ(); z++)
              {
              long Pix = cube->nPix(x,y,z);
              modSum += outarray[Pix];
              obsSum += cube->Array(Pix) * cube->Mask()[Pix];
              }
            if (modSum!=0)
              {
              factor = obsSum/modSum;
              }

            for (int z=0; z<cube->DimZ(); z++)
              {
              outarray[cube->nPix(x,y,z)] *= factor;
              }
            }
          }

        double *outFPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer());

        for (int ii = 0; ii < numElements; ii++)
          {
          *(outFPixel + ii) = *(outarray + ii);
          }

        delete totalmap;
        delete fit;
        delete mod;
        }

      delete cube;
      break;
      }
    }

  pnode->SetStatus(100);

  delete par;
  delete head;

  outputVolume->UpdateNoiseAttributes();
  outputVolume->UpdateRangeAttributes();
  outputVolume->SetAttribute("SlicerAstro.DATATYPE", "MODEL");
  pnode->SetStatus(0);

  pnode->SetFitSuccess(true);
  return 1;
}
