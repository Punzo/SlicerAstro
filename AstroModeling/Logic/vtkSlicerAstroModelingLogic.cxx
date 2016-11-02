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
#include "galfit.hh"

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroModelingParametersNode.h>

// VTK includes
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
  int success = 0;
  pnode->SetStatus(1);

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();

  Param *par = new Param;
  std::string file = inputVolume->GetName();
  par->setImageFile(file);
  par->setBeamFWHM(par->getBeamFWHM() / 3600.);
  par->setSearch(true);
  par->setflagGalFit(true);
  par->setMASK("NONE");

  Cube<float> *cubeF;
  Cube<double> *cubeD;

  switch (DataType)
    {
    case VTK_FLOAT:
      cubeF = new Cube<float>;
      cubeF->saveParam(*par);
      break;
    case VTK_DOUBLE:
      cubeD = new Cube<double>;
      cubeD->saveParam(*par);
      break;
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
  head->setEpoch(StringToFloat(inputVolume->GetAttribute("SlicerAstro.EPOCH")));
  head->setFreq0(StringToDouble(inputVolume->GetAttribute("SlicerAstro.FREQ0")));
  inputString = inputVolume->GetAttribute("SlicerAstro.BUNIT");
  head->setBunit(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.BTYPE");
  head->setBtype(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.OBJECT");
  head->setName(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.OBJECT");
  head->setTelesc(inputString);
  inputString = inputVolume->GetAttribute("SlicerAstro.OBJECT");
  head->setDunit3(inputString);
  head->setDrval3(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DRVAL3")));
  head->setDataMin(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMIN")));
  head->setDataMax(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMAX")));
  if(!head->saveWCSStruct(inputVolume->GetAstroVolumeDisplayNode()->GetWCSStruct()))
    {
    vtkErrorMacro("vtkSlicerAstroModelingLogic::FitModel : the WCS copy failed!")
    }

  head->calcArea();

  int *dims = outputVolume->GetImageData()->GetDimensions();
  int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  switch (DataType)
    {
    case VTK_FLOAT:
      {
      cubeF->saveHead(*head);

      cubeF->setCube(static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer()), dims);

      ///<<<<< Searching stuff
      if (par->getSearch())
        {
        cubeF->Search();
        }

      ///<<<<< Cube Fitting
      if (par->getflagGalFit())
        {
        Model::Galfit<float> *fit = new Model::Galfit<float>(cubeF);
        fit->galfit();
        if (par->getTwoStage())
          {
          fit->SecondStage();
          }

        cout<<"bella"<<endl;

        // Calculate the total flux inside last ring in data
        float *ringreg = fit->getFinalRingsRegion();
        float totflux_data = 0, totflux_model = 0;
        MomentMap<float> *totalmap = new MomentMap<float>;
        totalmap->input(cubeF);
        totalmap->SumMap(true);

        for (size_t ii = 0; ii < (size_t)cubeF->DimX() * cubeF->DimY(); ii++)
          {
          if (!isNaN(ringreg[ii]) && !isNaN(totalmap->Array(ii)))
            {
            totflux_data += totalmap->Array(ii);
            }
          }

        cout<<"bella1"<<endl;
        Model::Galmod<float> *mod = fit->getModel();
        mod->Out()->Head().setMinMax(0.,0.);
        float *outarray = mod->Out()->Array();

        std::string normtype = par->getNORM();

        if (normtype == "AZIM" || normtype == "BOTH")
          {

          // Calculate total flux of model within last ring
          for (size_t ii = 0; ii < (size_t)cubeF->DimX() * cubeF->DimY(); ii++)
            {
            if (!isNaN(ringreg[ii]))
              {
              for (size_t z = 0; z < (size_t)cubeF->DimZ(); z++)
                {
                totflux_model += outarray[ii + z * cubeF->DimY() * cubeF->DimX()];
                }
              }
            }
          }

        cout<<"bella2"<<endl;
        double factor = totflux_data/totflux_model;
        for (int ii = 0; ii < cubeF->NumPix(); ii++)
          {
          outarray[ii] *= factor;
          }

        if (normtype=="LOCAL" || normtype=="BOTH")
          {

          // The final model is normalized pixel-by-pixel, i.e. in each spaxel, the total
          // flux of observation and model is eventually equal.

          for (int y = 0; y < cubeF->DimY(); y++)
            {
            for (int x = 0; x < cubeF->DimX(); x++)
              {
              float factor = 0;
              float modSum = 0;
              float obsSum = 0;
              for (int z = 0; z < cubeF->DimZ(); z++)
                {
                long Pix = cubeF->nPix(x,y,z);
                modSum += outarray[Pix];
                obsSum += cubeF->Array(Pix) * cubeF->Mask()[Pix];
                }
              if (modSum!=0)
                {
                factor = obsSum/modSum;
                }

              for (int z=0; z<cubeF->DimZ(); z++)
                {
                outarray[cubeF->nPix(x,y,z)] *= factor;
                }
              }
            }
          }

        cout<<"bella3"<<endl;
        float *outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer());

        for (int ii = 0; ii < numElements; ii++)
          {
          *(outFPixel + ii) = *(outarray + ii);
          }

        cout<<"bella4"<<endl;
        delete totalmap;
        delete fit;
        delete mod;
        }
      break;
      }
    case VTK_DOUBLE:
      {

      }
    }

  cout<<"bella5"<<endl;
  outputVolume->UpdateNoiseAttributes();
  cout<<"bella6"<<endl;
  outputVolume->UpdateRangeAttributes();
  cout<<"bella7: min and max"<<endl;
  cout<<StringToDouble(outputVolume->GetAttribute("SlicerAstro.DATAMIN"))<<endl;
  cout<<StringToDouble(outputVolume->GetAttribute("SlicerAstro.DATAMAX"))<<endl;
  outputVolume->SetAttribute("SlicerAstro.DATATYPE", "MODEL");
  pnode->SetStatus(100);

  delete par;
  delete head;

  delete cubeF;
  delete cubeD;

  cout<<"bella8"<<endl;
  pnode->SetStatus(0);
  success = 1;
  return success;
}
