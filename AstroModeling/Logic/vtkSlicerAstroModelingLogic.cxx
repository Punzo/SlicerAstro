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
#include "stats.hh"
#include "moment.hh"
#include "ringmodel.hh"
#include "smooth3D.hh"
#include "galfit.hh"
#include "spacepar.hh"
#include "utils.hh"
#include "ellprof.hh"

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
int StringToLong(const char* str)
{
  return StringToNumber<long>(str);
}

//----------------------------------------------------------------------------
double StringToFloat(const char* str)
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
/* now I have to modify this:
 * make it working without mask
 * give parameters hard coded
 * then give parameters from interface
 * then give mask from segmentationNode
 * look for the best layout for the output
 * (Convetional quantitative:
 * background data; foreground model;
 * 3-D data + segmentation (white) of model (make a LabelMap and convert to segmentation);
 * chart: fitted parameters. )*/

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();

  Param *par = new Param;
  std::string file = "";
  par->setImageFile(file);
  par->setBeamFWHM(par->getBeamFWHM() / 3600.);
  //par->setflagSearch(true);
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

  //check that input dimensionality is 3
  // set header
  Header *head = new Header;
  head->setBitpix(StringToInt(inputVolume->GetAttribute("SlicerAstro.BITPIX")));
  int numAxes = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  head->setNumAx(numAxes);
  string inputString;
  for (int ii = 0; ii < numAxes; ii++)
    {
    inputString = "SlicerAstro.NAXIS" + IntToString(ii);
    head->setDimAx(ii,StringToLong(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CRPIX" + IntToString(ii);
    head->setCrpix(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CRVAL" + IntToString(ii);
    head->setCrval(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CDELT" + IntToString(ii);
    head->setCdelt(ii, StringToFloat(inputVolume->GetAttribute(inputString.c_str())));
    inputString = "SlicerAstro.CTYPE" + IntToString(ii);
    head->setCtype(ii, inputVolume->GetAttribute(inputString.c_str()));
    inputString = "SlicerAstro.CUNIT" + IntToString(ii);
    head->setCunit(ii, inputVolume->GetAttribute(inputString.c_str()));
    }
  head->setBmaj(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BMAJ")));
  head->setBmin(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BMIN")));
  head->setBpa(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BPA")));
  head->setBzero(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BZERO")));
  head->setBscale(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BSCALE")));
  head->setBlank(StringToFloat(inputVolume->GetAttribute("SlicerAstro.BLANK")));
  head->setEpoch(StringToFloat(inputVolume->GetAttribute("SlicerAstro.EPOCH")));
  head->setFreq0(StringToDouble(inputVolume->GetAttribute("SlicerAstro.FREQ0")));
  head->setBunit(inputVolume->GetAttribute("SlicerAstro.BUNIT"));
  head->setBtype(inputVolume->GetAttribute("SlicerAstro.BTYPE"));
  head->setName(inputVolume->GetAttribute("SlicerAstro.OBJECT"));
  head->setTelesc(inputVolume->GetAttribute("SlicerAstro.TELESC"));
  head->setDunit3(inputVolume->GetAttribute("SlicerAstro.DUNIT3"));
  head->setDrval3(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DRVAL3")));
  head->setDataMin(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMIN")));
  head->setDataMax(StringToDouble(inputVolume->GetAttribute("SlicerAstro.DATAMAX")));

  //head->setWCSStruct(inputVolume->GetAstroVolumeDisplayNode()->GetWCSStruct());
  head->calcArea();

  int *dims = outputVolume->GetImageData()->GetDimensions();
  int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;
  switch (DataType)
    {
    case VTK_FLOAT:
      {
      //cubeF->setHead(head);

      cubeF->setCube(static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer()), dims);

      if (par->getCheckCh())
        {
        cubeF->CheckChannels();
        }

      if (par->getflagSmooth())
        {
        Smooth3D<float> *sm = new Smooth3D<float>;
        sm->cubesmooth(cubeF);
        delete sm;
        }

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
        if (par->getTwoStage()) fit->SecondStage();
        Model::Galmod<float> *mod = fit->getModel();
        cubeF = mod->Out();

        delete fit;
        delete mod;
        }

      float *cubeFPtr = cubeF->Array();
      float *outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer());

      for (int ii = 0; ii < numElements; ii++)
        {
        *(outFPixel + ii) = *(cubeFPtr + ii);
        }
      break;
      }
    case VTK_DOUBLE:
      {
      //cubeD->setHead(head);

      cubeD->setCube(static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer()), dims);

      if (par->getCheckCh())
        {
        cubeD->CheckChannels();
        }

      if (par->getflagSmooth())
        {
        Smooth3D<double> *sm = new Smooth3D<double>;
        sm->cubesmooth(cubeD);
        delete sm;
        }

      ///<<<<< Searching stuff
      if (par->getSearch())
        {
        cubeD->Search();
        }

      ///<<<<< Cube Fitting
      if (par->getflagGalFit())
        {
        Model::Galfit<double> *fit = new Model::Galfit<double>(cubeD);
        fit->galfit();
        if (par->getTwoStage()) fit->SecondStage();
        Model::Galmod<double> *mod = fit->getModel();
        cubeD = mod->Out();

        delete fit;
        delete mod;
        }

      double *cubeDPtr = cubeD->Array();
      double *outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer());
      for (int ii = 0; ii < numElements; ii++)
        {
        *(outDPixel + ii) = *(cubeDPtr + ii);
        }
      break;
      }
    }

  outputVolume->UpdateNoiseAttributes();
  outputVolume->UpdateRangeAttributes();
  outputVolume->SetAttribute("SlicerAstro.DATATYPE", "MODEL");
  pnode->SetStatus(100);

  delete par;
  delete head;

  delete cubeF;
  delete cubeD;

  pnode->SetStatus(0);
  success = 1;
  return success;
}
