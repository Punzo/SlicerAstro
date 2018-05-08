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

#include <math.h>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLVolumeNode.h>

// VTK includes
#include <vtkAlgorithm.h>
#include <vtkAlgorithmOutput.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>

//----------------------------------------------------------------------------
const double vtkMRMLAstroVolumeDisplayNode::CONTOURSCOLOR_INVALID[3] = {1., 0.731, 0.078};

//------------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::vtkMRMLAstroVolumeDisplayNode()
{
  this->SpaceQuantities = vtkStringArray::New();
  this->SpaceQuantities->SetName("Tokens");
  this->SpaceQuantities->SetNumberOfValues(3);
  this->SpaceQuantities->SetValue(0, "time");
  this->SpaceQuantities->SetValue(1, "length");
  this->SpaceQuantities->SetValue(2, "velocity");
  this->Space = NULL;
  this->SetSpace("WCS");
  this->FitSlices = false;
  this->WCSStatus = 0;
  this->WCS = new struct wcsprm;
  this->WCS->flag = -1;
  wcserr_enable(1);
  if((this->WCSStatus = wcsini(1,0,this->WCS)))
    {
    vtkErrorMacro("wcsini ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
    }
  this->ContoursColor = vtkDoubleArray::New();
  this->ContoursColor->SetNumberOfValues(3);
  this->ContoursColor->SetValue(0, CONTOURSCOLOR_INVALID[0]);
  this->ContoursColor->SetValue(1, CONTOURSCOLOR_INVALID[1]);
  this->ContoursColor->SetValue(2, CONTOURSCOLOR_INVALID[2]);
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::~vtkMRMLAstroVolumeDisplayNode()
{
  if (this->ContoursColor)
    {
    this->ContoursColor->Delete();
    }

  if (this->SpaceQuantities)
    {
    this->SpaceQuantities->Delete();
    }

  if (this->Space)
    {
    delete [] this->Space;
    }

  if(this->WCS)
    {
    if((this->WCSStatus = wcsfree(this->WCS)))
      {
      vtkErrorMacro("wcsfree ERROR "<<this->WCSStatus<<":\n"<<
                    "Message from "<<this->WCS->err->function<<
                    "at line "<<this->WCS->err->line_no<<
                    " of file "<<this->WCS->err->file<<
                    ": \n"<<this->WCS->err->msg<<"\n");
      }
    delete [] this->WCS;
    this->WCS = NULL;
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

//----------------------------------------------------------------------------
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
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

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  std::string quantities = "";

  of << indent << " FitSlices=\"" << this->FitSlices << "\"";

  if (this->ContoursColor)
    {
    for(int i = 0; i < this->ContoursColor->GetNumberOfValues(); i++)
      {
      quantities +=  DoubleToString(this->ContoursColor->GetValue(i));
      quantities += ";";
      }
    }

  of << indent << " ContoursColor=\"" << quantities << "\"";

  quantities.clear();
  if (this->SpaceQuantities)
    {
    for(int i = 0; i < this->SpaceQuantities->GetNumberOfValues(); i++)
      {
      quantities +=  this->SpaceQuantities->GetValue(i) + ";";
      }
    }

  of << indent << " SpaceQuantities=\"" << quantities << "\"";
  of << indent << " Space=\"" << (this->Space ? this->Space : "") << "\"";
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::SetWCSStruct(struct wcsprm* wcstemp)
{
  if (!strcmp(this->Space, "IJK"))
    {
    return;
    }

  if (!wcstemp)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetWCSStruct: "
                  "wcsprm is invalid!");
    return;
    }

  if (!this->WCS)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetWCSStruct: "
                  "WCS not found!");
    return;
    }

  this->WCS->flag = -1;
  if ((this->WCSStatus = wcscopy(1, wcstemp, this->WCS)))
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetWCSStruct: "
                  "wcscopy ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->WCS->err->function<<
                  "at line "<<this->WCS->err->line_no<<
                  " of file "<<this->WCS->err->file<<
                  ": \n"<<this->WCS->err->msg<<"\n");
    }
  if ((this->WCSStatus = wcsset(this->WCS)))
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetWCSStruct :"
                  "wcsset ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->WCS->err->function<<
                  "at line "<<this->WCS->err->line_no<<
                  " of file "<<this->WCS->err->file<<
                  ": \n"<<this->WCS->err->msg<<"\n");
    }

  this->Modified();
}

//----------------------------------------------------------------------------
wcsprm *vtkMRMLAstroVolumeDisplayNode::GetWCSStruct()
{
  return this->WCS;
}

//----------------------------------------------------------------------------
bool vtkMRMLAstroVolumeDisplayNode::SetRadioVelocityDefinition(bool update /*= true*/)
{
  if (!strcmp(this->Space, "IJK"))
    {
    return false;
    }

  if (!this->WCS || this->WCSStatus != 0)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetRadioVelocityDefinition :"
                  " WCS not found.");
    return false;
    }

  if (strncmp(this->WCS->ctype[2], "VRAD", 4))
    {
    int index = 2;
    char ctypeS[9];
    strcpy(ctypeS, "VRAD-???");

    if ((this->WCSStatus = wcssptr(this->WCS, &index, ctypeS)))
      {
      vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetRadioVelocityDefinition :"
                    " wcssptr ERROR "<<this->WCSStatus<<":"<<
                    "Message from "<<this->WCS->err->function<<
                    "at line "<<this->WCS->err->line_no<<
                    " of file "<<this->WCS->err->file<<
                    ": "<<this->WCS->err->msg);
      return false;
      }

    if ((this->WCSStatus = wcsset(this->WCS)))
      {
      vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetRadioVelocityDefinition :"
                    " wcsset ERROR "<<this->WCSStatus<<":"<<
                    "Message from "<<this->WCS->err->function<<
                    "at line "<<this->WCS->err->line_no<<
                    " of file "<<this->WCS->err->file<<
                    ": "<<this->WCS->err->msg);
      return false;
      }
    }

  if (update)
    {
    this->Modified();
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLAstroVolumeDisplayNode::SetOpticalVelocityDefinition(bool update /*= true*/)
{
  if (!strcmp(this->Space, "IJK"))
    {
    return false;
    }

  if (!this->WCS || this->WCSStatus != 0)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetOpticalVelocityDefinition :"
                  " WCS not found.");
    return false;
    }

  if (strncmp(this->WCS->ctype[2], "VOPT", 4))
    {
    int index = 2;
    char ctypeS[9];
    strcpy(ctypeS, "VOPT-???");

    if ((this->WCSStatus = wcssptr(this->WCS, &index, ctypeS)))
      {
      vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetOpticalVelocityDefinition"
                    " : wcssptr ERROR "<<this->WCSStatus<<":"<<
                    "Message from "<<this->WCS->err->function<<
                    "at line "<<this->WCS->err->line_no<<
                    " of file "<<this->WCS->err->file<<
                    ": "<<this->WCS->err->msg);
      return false;
      }

    if ((this->WCSStatus = wcsset(this->WCS)))
      {
      vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::SetOpticalVelocityDefinition"
                    " : wcsset ERROR "<<this->WCSStatus<<":"<<
                    "Message from "<<this->WCS->err->function<<
                    "at line "<<this->WCS->err->line_no<<
                    " of file "<<this->WCS->err->file<<
                    ": "<<this->WCS->err->msg);
      return false;
      }
    }

  if (update)
    {
    this->Modified();
    }

  return true;
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetVelocityDefinition()
{
  if (!strcmp(this->Space, "IJK"))
    {
    return "";
    }

  if (!this->WCS || this->WCSStatus != 0)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::GetVelocityDefinition :"
                  " WCS not found.");
    return "";
    }

  return std::string(this->WCS->ctype[2]);
}

//----------------------------------------------------------------------------
bool vtkMRMLAstroVolumeDisplayNode::GetReferenceSpace(const double ijk[3],
                                                      double SpaceCoordinates[3])
{
  if (!strcmp(this->Space, "IJK"))
    {
    return false;
    }

  if (!this->WCS || this->WCSStatus != 0)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::GetReferenceSpace :"
                  " WCS not found.");
    return false;
    }

  if (!strcmp(this->Space, "WCS"))
    {
    double phi[1], imgcrd[4], theta[1], ijkm [] = {0., 0., 0., 0.}, SpaceCoordinatesM [] = {0., 0., 0., 0.};
    int stati[1];

    std::copy(ijk, ijk + 3, ijkm);

    if ((this->WCSStatus = wcsp2s(this->WCS, 1, 4, ijkm, imgcrd, phi, theta, SpaceCoordinatesM, stati)))
      {
      vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::GetReferenceSpace : "
                    "wcsp2s ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
      return false;
      }

    std::copy(SpaceCoordinatesM, SpaceCoordinatesM + 3, SpaceCoordinates);
    }
  else
    {
    return false;
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLAstroVolumeDisplayNode::GetIJKSpace(const double SpaceCoordinates[3],
                                                double ijk[3])
{
  if (!strcmp(this->Space, "IJK"))
    {
    return false;
    }

  if (!this->WCS || this->WCSStatus != 0)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::GetIJKSpace :"
                  " WCS not found.");
    return false;
    }

  if (!strcmp(this->Space, "WCS"))
    {
    double phi[1], imgcrd[4], theta[1], ijkm [] = {0., 0., 0., 0.}, SpaceCoordinatesM [] = {0., 0., 0., 0.};
    int stati[1];

    std::copy(SpaceCoordinates, SpaceCoordinates + 3, SpaceCoordinatesM);

    if ((this->WCSStatus = wcss2p(this->WCS, 1, 4, SpaceCoordinatesM, phi, theta, imgcrd, ijkm, stati)))
      {
      vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::GetReferenceSpace : "
                    "wcss2p ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
      return false;
      }
    std::copy(ijkm, ijkm + 3, ijk);
    }
  else
    {
    return false;
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLAstroVolumeDisplayNode::GetIJKSpace(std::vector<double> SpaceCoordinates,
                                                double ijk[3])
{
  if (!strcmp(this->Space, "IJK"))
    {
    return false;
    }

  if (!this->WCS || this->WCSStatus != 0)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::GetIJKSpace :"
                  " WCS not found.");
    return false;
    }

  if (!strcmp(this->Space, "WCS"))
    {
    double phi[1], imgcrd[4], theta[1], ijkm [] = {0., 0., 0., 0.}, SpaceCoordinatesM [] = {0., 0., 0., 0.};
    int stati[1];

    SpaceCoordinatesM[0] = SpaceCoordinates[0];
    SpaceCoordinatesM[1] = SpaceCoordinates[1];
    SpaceCoordinatesM[2] = SpaceCoordinates[2];

    if ((this->WCSStatus = wcss2p(this->WCS, 1, 4, SpaceCoordinatesM, phi, theta, imgcrd, ijkm, stati)))
      {
      vtkErrorMacro("wcss2p ERROR "<<WCSStatus<<":\n"<<
                      "Message from "<<WCS->err->function<<
                      "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                      ": \n"<<WCS->err->msg<<"\n");
      return false;
      }
    std::copy(ijkm, ijkm + 3, ijk);
    }
  else
    {
    return false;
    }

  return true;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxis(const double worldA,
                                                          const double worldB,
                                                          const double wcsStep,
                                                          vtkMRMLUnitNode *node)
{
  if(!node)
    {
    return 0.;
    }

  double temp;

  if(worldA < worldB)
    {
    temp = worldA;
    }
  else
    {
    temp = worldB;
    }

  return temp - fmod(temp, wcsStep) - wcsStep;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxisX(const double worldA,
                                                           const double worldB,
                                                           const double wcsStep)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(0));
    return vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxis(worldA, worldB, wcsStep, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxisY(const double worldA,
                                                           const double worldB,
                                                           const double wcsStep)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(1));
    return vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxis(worldA, worldB, wcsStep, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxisZ(const double worldA,
                                                           const double worldB,
                                                           const double wcsStep)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(2));
    return vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxis(worldA, worldB, wcsStep, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxis(const double wcsLength,
                                                          int* numberOfPoints,
                                                          vtkMRMLUnitNode *node)
{
  if(!node)
    {
    return 0.;
    }

  int nPoint = numberOfPoints[0];
  double step = wcsLength / nPoint;

  if (!strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
    {
    step *= 0.066666666666667;
    }

  std::string displayValueString;
  std::stringstream strstream;

  strstream.precision(1);
  strstream << step;
  strstream >> displayValueString;
  for (int i = 9; i > 0; i--)
    {

    std::size_t found = displayValueString.find(IntToString(i));
    std::size_t foundE = displayValueString.find("e");

    if (found != std::string::npos && found > foundE)
      {
      continue;
      }

    if (found != std::string::npos)
      {
      if((!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds") ||
          !strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds")))
        {
        if(step > 0.95)
          {
          if(i >= 5)
            {
            displayValueString.replace(found, found+1, "10");
            }
          else if(i >= 3)
            {
            displayValueString.replace(found, found+1, "5");
            }
          else
            {
            displayValueString.replace(found, found+1, "2");
            }
          }
        else if(step < 0.95 && step > 0.095)
          {
          if(i > 6)
            {
            displayValueString.replace(found-1, found, "1");
            }
          else if(i > 3)
            {
            displayValueString.replace(found, found+1, "5");
            }
          else if(i > 1)
            {
            displayValueString.replace(found, found+1, "25");
            }
          else
            {
            displayValueString.replace(found, found+1, "1666666666666666666666");
            }
          }
        else if(step < 0.095 && step > 0.0095)
          {
          if(i >= 5)
            {
            displayValueString.replace(found, found+1, "8333333333333333333333");
            }
          else if(i >= 2)
            {
            displayValueString.replace(found, found+1, "3333333333333333333333");
            }
          else
            {
            displayValueString.replace(found, found+1, "1666666666666666666666");
            }
          }
        else if(step < 0.0095 && step > 0.00095)
          {
          if(i >= 5)
            {
            displayValueString.replace(found, found+1, "8333333333333333333333");
            }
          else if(i >= 3)
            {
            displayValueString.replace(found, found+1, "4166666666666666666666");
            }
          else if(i >= 2)
            {
            displayValueString.replace(found, found+1, "2083333333333333333333");
            }
          else
            {
            displayValueString.replace(found, found+1, "1388888888888888888888");
            }
          }
        else if(step < 0.00095)
          {
          if(i > 6)
            {
            displayValueString.replace(found, found+1, "8333333333333334");
            }
          else if(i > 3)
            {
            displayValueString.replace(found, found+1, "5555555555555556");
            }
          else
            {
            displayValueString.replace(found, found+1, "2777777777777778");
            }
          }
        }
      else
        {
        if(i > 6)
          {
          displayValueString.replace(found, found+1, "10");
          }
        else if(i >= 3)
          {
          displayValueString.replace(found, found+1, "5");
          }
        else
          {
          displayValueString.replace(found, found+1, "2");
          }
        }
      break;
      }
    }
  step = StringToDouble(displayValueString.c_str());
  if (!strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
    {
    step /= 0.066666666666667;
    }

  *numberOfPoints = nPoint + 3;
  return step;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxisX(const double wcsLength,
                                                          int* numberOfPoints)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(0));
    return vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxis(wcsLength, numberOfPoints, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxisY(const double wcsLength,
                                                          int* numberOfPoints)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(1));
    return vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxis(wcsLength, numberOfPoints, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxisZ(const double wcsLength,
                                                          int* numberOfPoints)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(2));
    return vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxis(wcsLength, numberOfPoints, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::ReadXMLAttributes(const char** atts)
{
  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;

  while (*atts != NULL)
    {
    attName = *(atts++);
    attValue = *(atts++);

    if (!strcmp(attName, "FitSlices"))
      {
      this->FitSlices = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "SpaceQuantities"))
      {
      std::istringstream f(attValue);
      std::string s;
      int i = 0;
      while (std::getline(f, s, ';'))
        {
        this->SetSpaceQuantity(i, s.c_str());
        i++;
        }
      continue;
      }

    if (!strcmp(attName, "ContoursColor"))
      {
      std::istringstream f(attValue);
      std::string s;
      int i = 0;
      while (std::getline(f, s, ';'))
        {
        this->SetContoursColor(i, StringToDouble(s.c_str()));
        i++;
        }
      continue;
      }

    if (!strcmp(attName, "Space"))
      {
      this->SetSpace(attValue);
      continue;
      }
    }
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, VolumeID
void vtkMRMLAstroVolumeDisplayNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();

  Superclass::Copy(anode);

  vtkMRMLAstroVolumeDisplayNode *node =
      vtkMRMLAstroVolumeDisplayNode::SafeDownCast(anode);

  this->UpdateImageDataPipeline();

  if (!node)
    {
    return;
    }

  this->SetInputImageDataConnection(node->GetInputImageDataConnection());
  this->SetFitSlices(node->GetFitSlices());
  this->SetContoursColor(node->GetContoursColor());
  this->SetSpaceQuantities(node->GetSpaceQuantities());
  this->SetSpace(node->GetSpace());
  this->SetAttribute("SlicerAstro.NAXIS", node->GetAttribute("SlicerAstro.NAXIS"));
  this->CopyWCS(node);

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::CopyWCS(vtkMRMLNode *node)
{
  if (!node || strcmp(this->Space, "WCS"))
    {
    return;
    }

  wcsprm *WCSNew = NULL;
  int WCSStatusNew = 0;

  vtkMRMLAstroVolumeDisplayNode *AstroVolumeDisplayNode =
      vtkMRMLAstroVolumeDisplayNode::SafeDownCast(node);
  vtkMRMLAstroLabelMapVolumeDisplayNode *AstroLabelMapVolumeDisplayNode =
      vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast(node);

  if (AstroVolumeDisplayNode)
    {
    WCSNew = AstroVolumeDisplayNode->GetWCSStruct();
    WCSStatusNew = AstroVolumeDisplayNode->GetWCSStatus();
    }
  else if (AstroLabelMapVolumeDisplayNode)
    {
    WCSNew = AstroLabelMapVolumeDisplayNode->GetWCSStruct();
    WCSStatusNew = AstroLabelMapVolumeDisplayNode->GetWCSStatus();
    }
  else
    {
    return;
    }

  if (!this->WCS || !WCSNew || WCSStatusNew != 0)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::CopyWCS :"
                  " WCS structs not found.");
    return;
    }

  int thisAxis = StringToInt(this->GetAttribute("SlicerAstro.NAXIS"));
  int nodeAxis = StringToInt(node->GetAttribute("SlicerAstro.NAXIS"));

  int nsub = 3;
  int axes[3];

  if (thisAxis == 3 && nodeAxis == 3)
    {
    nsub = 3;
    axes[0] = WCSSUB_LONGITUDE;
    axes[1] = WCSSUB_LATITUDE;
    axes[2] = WCSSUB_SPECTRAL;
    }
  else if (thisAxis == 2 && nodeAxis >= 2)
    {
    nsub = 2;
    axes[0] = WCSSUB_LONGITUDE;
    axes[1] = WCSSUB_LATITUDE;
    axes[2] = -WCSSUB_SPECTRAL;
    }
  else if (thisAxis == 1 && nodeAxis >= 1)
    {
    nsub = 1;
    axes[0] = WCSSUB_LONGITUDE;
    axes[1] = -WCSSUB_LATITUDE;
    axes[2] = -WCSSUB_SPECTRAL;
    }
  else
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::CopyWCS: "
                  "it is not possible to copy WCS from a volume with "
                  "naxis < than the naxis of teh current volume. ");
    return;
    }

  this->WCS->flag = -1;
  if ((this->WCSStatus = wcssub(1, WCSNew, &nsub, axes, this->WCS)))
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::CopyWCS: "
                  "wcssub ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->WCS->err->function<<
                  "at line "<<this->WCS->err->line_no<<
                  " of file "<<this->WCS->err->file<<
                  ": \n"<<this->WCS->err->msg<<"\n");

    this->SetSpace("IJK");
    return;
    }

  if ((this->WCSStatus = wcsset(this->WCS)))
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::CopyWCS : "
                  "wcsset ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->WCS->err->function<<
                  "at line "<<this->WCS->err->line_no<<
                  " of file "<<this->WCS->err->file<<
                  ": \n"<<this->WCS->err->msg<<"\n");
    this->SetSpace("IJK");
    return;
    }

  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::CopySpatialWCS(vtkMRMLNode *node)
{
  if (!node || strcmp(this->Space, "WCS"))
    {
    return;
    }

  wcsprm *WCSNew = NULL;

  vtkMRMLAstroVolumeDisplayNode *AstroVolumeDisplayNode =
      vtkMRMLAstroVolumeDisplayNode::SafeDownCast(node);
  vtkMRMLAstroLabelMapVolumeDisplayNode *AstroLabelMapVolumeDisplayNode =
      vtkMRMLAstroLabelMapVolumeDisplayNode::SafeDownCast(node);

  if (AstroVolumeDisplayNode)
    {
    WCSNew = AstroVolumeDisplayNode->GetWCSStruct();
    }
  else if (AstroLabelMapVolumeDisplayNode)
    {
    WCSNew = AstroLabelMapVolumeDisplayNode->GetWCSStruct();
    }
  else
    {
    return;
    }

  if (!this->WCS || !WCSNew)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::WCSCopySpatial :"
                  " WCS structs not found.");
    return;
    }

  if (this->WCS->naxis < 2 || WCSNew->naxis < 2)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::WCSCopySpatial :"
                  " both WCS structs need two spatial axes (i.e., naxis > 1).");
    return;
    }

  this->WCS->flag = -1;
  this->WCS->crpix[0] = WCSNew->crpix[0];
  this->WCS->crpix[1] = WCSNew->crpix[1];

  this->WCS->cdelt[0] = WCSNew->cdelt[0];
  this->WCS->cdelt[1] = WCSNew->cdelt[1];

  this->WCS->crval[0] = WCSNew->crval[0];
  this->WCS->crval[1] = WCSNew->crval[1];

  strncpy(this->WCS->cunit[0],  WCSNew->cunit[0], 72);
  strncpy(this->WCS->cunit[1],  WCSNew->cunit[1], 72);

  strncpy(this->WCS->ctype[0],  WCSNew->ctype[0], 72);
  strncpy(this->WCS->ctype[1],  WCSNew->ctype[1], 72);

  this->WCS->lonpole = WCSNew->lonpole;
  this->WCS->latpole = WCSNew->latpole;

  this->WCS->crota[0] = WCSNew->crota[0];
  this->WCS->crota[1] = WCSNew->crota[1];

  this->WCS->colax[0] = WCSNew->colax[0];
  this->WCS->colax[1] = WCSNew->colax[1];

  strncpy(this->WCS->cname[0],  WCSNew->cname[0], 72);
  strncpy(this->WCS->cname[1],  WCSNew->cname[1], 72);

  this->WCS->crder[0] = WCSNew->crder[0];
  this->WCS->crder[1] = WCSNew->crder[1];

  this->WCS->csyer[0] = WCSNew->csyer[0];
  this->WCS->csyer[1] = WCSNew->csyer[1];

  strncpy(this->WCS->radesys,  WCSNew->radesys, 72);

  this->WCS->equinox = WCSNew->equinox;

  this->WCS->pc[0] = WCSNew->pc[0];
  this->WCS->pc[1] = WCSNew->pc[1];
  this->WCS->pc[WCS->naxis] = WCSNew->pc[WCSNew->naxis];
  this->WCS->pc[WCS->naxis + 1] = WCSNew->pc[WCSNew->naxis + 1];

  this->WCS->cd[0] = WCSNew->cd[0];
  this->WCS->cd[1] = WCSNew->cd[1];
  this->WCS->cd[WCS->naxis] = WCSNew->cd[WCSNew->naxis];
  this->WCS->cd[WCS->naxis + 1] = WCSNew->cd[WCSNew->naxis + 1];

  if ((this->WCSStatus = wcsset(this->WCS)))
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::WCSCopy : "
                  "wcsset ERROR "<<this->WCSStatus<<":\n"<<
                  "Message from "<<this->WCS->err->function<<
                  "at line "<<this->WCS->err->line_no<<
                  " of file "<<this->WCS->err->file<<
                  ": \n"<<this->WCS->err->msg<<"\n");
    this->SetSpace("IJK");
    return;
    }

  this->Modified();
}

//----------------------------------------------------------------------------
bool vtkMRMLAstroVolumeDisplayNode::SetSpaceQuantity(int ind, const char *name)
{
  if (ind >= this->SpaceQuantities->GetNumberOfValues())
    {
    this->SpaceQuantities->SetNumberOfValues(ind + 1);
    }

  vtkStdString SpaceQuantities(name);
  if (this->SpaceQuantities->GetValue(ind) != SpaceQuantities)
    {
    this->SpaceQuantities->SetValue(ind, SpaceQuantities);
    this->Modified();
    return true;
    }
  return false;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::SetContoursColor(int ind, double color)
{
  if (ind >= this->ContoursColor->GetNumberOfValues())
    {
    this->ContoursColor->SetNumberOfValues(ind + 1);
    }

  this->ContoursColor->SetValue(ind, color);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::SetFitSlices(bool fit)
{
  this->FitSlices = fit;
  this->InvokeCustomModifiedEvent(vtkMRMLAstroVolumeDisplayNode::FitSlicesModifiedEvent);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetPixelString(double *ijk)
{
  if (!this->GetVolumeNode() || !this->GetVolumeNode()->GetImageData())
    {
    return "No Image";
    }

  for (int i = 0; i < 3; i++)
    {
    if(ijk[i] < 0 or ijk[i] >=  this->GetVolumeNode()->GetImageData()->GetDimensions()[i])
      {
      return "Out of Frame";
      }
    }

  int numberOfComponents = this->GetVolumeNode()->GetImageData()->GetNumberOfScalarComponents();
  if (numberOfComponents > 3)
    {
    std::string s = IntToString(numberOfComponents) + " components";
    return s.c_str();
    }

  std::string pixel;
  std::string type = this->GetVolumeNode()->GetAttribute("SlicerAstro.DATAMODEL");
  size_t found = type.find("MOMENTMAP");
  if (found!=std::string::npos)
    {
    double component = this->GetVolumeNode()->GetImageData()->
        GetScalarComponentAsDouble(ijk[0],ijk[1],ijk[2],0);

    pixel = DoubleToString(component) + " " + this->GetVolumeNode()->GetAttribute("SlicerAstro.BUNIT") +
            " " + this->GetVolumeNode()->GetAttribute("SlicerAstro.BTYPE");
    }
  else
    {
    for(int ii = 0; ii < numberOfComponents; ii++)
      {
      double component = this->GetVolumeNode()->GetImageData()->
          GetScalarComponentAsDouble(ijk[0], ijk[1], ijk[2], ii);

      pixel += DoubleToString(component) + "  " + this->GetVolumeNode()->GetAttribute("SlicerAstro.BUNIT");
      pixel += ",";
      }

    pixel.erase(pixel.size()-1);
    }

  return pixel;
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValue(const double world,
                                                                     vtkMRMLUnitNode *node,
                                                                     int precision,
                                                                     const char* language,
                                                                     const double oldOutputValues[3],
                                                                     double outputValues[3],
                                                                     bool additionalSpace /* = false */)
{
 std::string value = "";
 if(!node)
   {
   return value.c_str();
   }

 if (!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds") ||
     !strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
   {
   std::string firstPrefix;
   std::string secondPrefix;
   std::string thirdPrefix;

   double firstFractpart, firstIntpart, secondFractpart, secondIntpart, displayValue;
   std::string displayValueString;
   std::stringstream strstream;
   strstream.setf(ios::fixed,ios::floatfield);

   float sign = 1.;
   if (world < 0.)
     {
     sign = -1.;
     }

   displayValue = fabs(world);

   if (!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds"))
     {
     if (!strcmp(language, "C++"))
       {
       firstPrefix = "\u00B0 "; //C++
       }
     else if (!strcmp(language, "Python"))
       {
       firstPrefix = "\xB0 "; //Python
       }
     else
       {
       vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValue : "
                     "no degree uft-8 code found for "<<language);
       }
     secondPrefix = "\x27 ";
     thirdPrefix = "\x22";
     }
   if (!strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
     {
     firstPrefix = "h ";
     secondPrefix = "m ";
     thirdPrefix = "s";
     displayValue /= 15.;
     }

   firstFractpart = modf(displayValue, &firstIntpart);
   if(firstFractpart * 60. > 59.99999)
     {
     firstFractpart = 0.;
     firstIntpart += 1.;
     }

   // First
   outputValues[0] = firstIntpart;
   if (firstIntpart > 0.00001 &&
       fabs(outputValues[0] - oldOutputValues[0]) > 1.E-6)
     {
     if (sign < 0.)
       {
       value = "-";
       }
     value = value + DoubleToString(firstIntpart) + firstPrefix;
     }
   else if (additionalSpace)
     {
     value = "   ";
     if (sign < 0.)
       {
       value = value + "-";
       }
     }
   else
     {
     value = "";
     if (sign < 0.)
       {
       value = value + "-";
       }
     }

   // Second
   secondFractpart = (modf(firstFractpart * 60., &secondIntpart)) * 60.;
   if(secondFractpart > 59.99999)
     {
     secondFractpart = 0.;
     secondIntpart += 1.;
     }

   outputValues[1] = secondIntpart;
   if ((secondIntpart > 0.00001 || firstIntpart > 0.00001) &&
       fabs(outputValues[1] - oldOutputValues[1]) > 1.E-6)
     {
     displayValueString = DoubleToString(fabs(secondIntpart));
     }
   else if (additionalSpace)
     {
     displayValueString = "   ";
     }
   else
     {
     displayValueString = "";
     }

   if(secondIntpart < 10.)
     {
     displayValueString = " " + displayValueString;
     }
   if ((secondIntpart > 0.00001 || firstIntpart > 0.00001) &&
       fabs(outputValues[1] - oldOutputValues[1]) > 1.E-6)
     {
     displayValueString += secondPrefix;
     }
   value = value + displayValueString;

   // Third
   displayValueString = "";
   strstream.precision(precision);
   strstream << fabs(secondFractpart);
   strstream >> displayValueString;

   outputValues[2] = StringToDouble(displayValueString.c_str());
   if(secondFractpart < 10.)
     {
     displayValueString = " " + displayValueString;
     }

   if (fabs(outputValues[2] - oldOutputValues[2]) > 1.E-6)
     {
     value = value + displayValueString + thirdPrefix;
     }
   else
     {
     value = "  " + value;
     }

   return value.c_str();
   }

  return node->GetDisplayStringFromValue(world);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValueX(const double world,
                                                                      const double oldOutputValues[3],
                                                                      double outputValues[3],
                                                                      int precision /* = 0 */,
                                                                      bool additionalSpace /* = false*/)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (!selectionNode)
    {
    return "";
    }

  double RightAscensionDegree = world;
  while (RightAscensionDegree < 0.)
    {
    RightAscensionDegree += 360.;
    }

  while (RightAscensionDegree > 360.)
    {
    RightAscensionDegree -= 360.;
    }

  vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(0));
  return this->GetDisplayStringFromValue(world, unitNode, precision, "C++", oldOutputValues, outputValues, additionalSpace);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValueY(const double world,
                                                                      const double oldOutputValues[3],
                                                                      double outputValues[3],
                                                                      int precision /* = 0 */,
                                                                      bool additionalSpace /* = false*/)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (!selectionNode)
    {
    return "";
    }

  double DeclinationDegree = world;
  if (DeclinationDegree < -90. || DeclinationDegree > 90.)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValueY :"
                  " declination coordinate out of range.");
    return "";
    }

   vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(1));
   return this->GetDisplayStringFromValue(DeclinationDegree, unitNode, precision, "C++", oldOutputValues, outputValues, additionalSpace);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValueZ(const double world,
                                                                      const double oldOutputValues[3],
                                                                      double outputValues[3],
                                                                      int precision /* = 0 */,
                                                                      bool additionalSpace /* = false*/)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (!selectionNode)
    {
    return "";
    }

  vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(2));
  return this->GetDisplayStringFromValue(world, unitNode, precision, "C++", oldOutputValues, outputValues, additionalSpace);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetPythonDisplayStringFromValueX(const double world,
                                                                            int precision /* = 0 */)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (!selectionNode)
    {
    return "";
    }

  vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(0));
  double outputValues[3], oldOutputValues[3];
  return this->GetDisplayStringFromValue(world, unitNode, precision, "Python", oldOutputValues, outputValues, false);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetPythonDisplayStringFromValueY(const double world,
                                                                            int precision /* = 0 */)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (!selectionNode)
    {
    return "";
    }

  vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(1));
  double outputValues[3], oldOutputValues[3];
  return this->GetDisplayStringFromValue(world, unitNode, precision, "Python", oldOutputValues, outputValues, false);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetPythonDisplayStringFromValueZ(const double world,
                                                                            int precision /* = 0 */)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (!selectionNode)
    {
    return "";
    }

  vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(2));
  double outputValues[3], oldOutputValues[3];
  return this->GetDisplayStringFromValue(world, unitNode, precision, "Python", oldOutputValues, outputValues, false);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::AddVelocityInfoToDisplayStringZ(std::string value)
{
  if (!strcmp(this->Space, "IJK"))
    {
    return "";
    }

  if (!this->WCS || this->WCSStatus != 0)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::AddVelocityInfoToDisplayStringZ : "
                  "WCS not found!");
    return "";
    }

  if (!this->SpaceQuantities->GetValue(2).compare("velocity"))
    {
    value = value + " (" + this->WCS->ctype[2] + ")";
    }
  return value;
}



//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);
  std::string quantities = "";
  int i,j,k;

  os << indent << "FitSlices: " << this->FitSlices << "\n";

  if (this->ContoursColor)
    {
    for(i = 0; i < this->ContoursColor->GetNumberOfValues(); i++)
      {
      quantities +=  DoubleToString(this->ContoursColor->GetValue(i));
      quantities += ";";
      }
    }

  os << indent << "ContoursColor=\"" << quantities << "\n";

  quantities.clear();
  if (this->SpaceQuantities)
    {
    for(i = 0; i < this->SpaceQuantities->GetNumberOfValues(); i++)
      {
      quantities +=  this->SpaceQuantities->GetValue(i) + ";";
      }
    }

  os << indent << "SpaceQuantities=\"" << quantities << "\n";
  os << indent << "Space: " << (this->Space ? this->Space : "(none)") << "\n";

  if (this->Space)
    {
    if (!strcmp(this->Space, "IJK"))
      {
      os << indent << "no WCS coordinates available. " << "\n";
      return;
      }
    }

  if (!this->WCS)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeDisplayNode::PrintSelf :"
                  " WCS not found.");
    return;
    }

  std::string pre=" SlicerAstro.WCS.";
  std::string und="UNDEFINED";

  os << indent << pre << "flag:   " << this->WCS->flag <<std::endl;
  os << indent << pre << "naxis:   " << this->WCS->naxis <<std::endl;
  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "crpix"<<i<<":   " << this->WCS->crpix[i] <<std::endl;
    }

  // Linear transformation.
  k = 0;
  for (i = 0; i < this->WCS->naxis; i++)
    {
    for (j = 0; j < this->WCS->naxis; j++)
      {
      os << indent << pre << "pc"<<k<<":   " << this->WCS->pc[k] <<std::endl;
      k++;
      }
    }

  // Coordinate increment at reference point.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "cdelt"<<i<<":   " << this->WCS->cdelt[i] <<std::endl;
    }

  // Coordinate value at reference point.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "crval"<<i<<":   " << this->WCS->crval[i] <<std::endl;
    }

  // Coordinate units and type.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "cunit"<<i<<":   " << this->WCS->cunit[i] <<std::endl;
    }

  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "ctype"<<i<<":   " << this->WCS->ctype[i] <<std::endl;
    }

  // Celestial and spectral transformation parameters.
  os << indent << pre << "lonpole:   " << this->WCS->lonpole <<std::endl;
  os << indent << pre << "latpole:   " << this->WCS->latpole <<std::endl;
  os << indent << pre << "restfrq:   " << this->WCS->restfrq <<std::endl;
  os << indent << pre << "restwav:   " << this->WCS->restwav <<std::endl;
  os << indent << pre << "npv:   " << this->WCS->npv <<std::endl;
  os << indent << pre << "npvmax:   " << this->WCS->npvmax <<std::endl;

  for (i = 0; i < this->WCS->npv; i++)
    {
    os << indent << pre << "pvi"<<i<<":   " << (this->WCS->pv[i]).i <<std::endl;
    os << indent << pre << "pvvalue"<<i<<":   " << (this->WCS->pv[i]).value <<std::endl;
    }

  os << indent << pre << "nps:   " << this->WCS->nps <<std::endl;
  os << indent << pre << "npsmax:   " << this->WCS->npsmax <<std::endl;

  for (i = 0; i < this->WCS->nps; i++)
    {
    os << indent << pre << "pvi"<<i<<":   " << (this->WCS->ps[i]).i <<std::endl;
    os << indent << pre << "pvvalue"<<i<<":   " << (this->WCS->ps[i]).value <<std::endl;
    }

  // Alternate linear transformations.
  k = 0;
  if (this->WCS->cd)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      for (j = 0; j < this->WCS->naxis; j++)
        {
        os << indent << pre << "cd"<<k<<":   " << this->WCS->cd[k] <<std::endl;
        k++;
        }
      }
    }

  if (this->WCS->crota)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      os << indent << pre << "crota"<<i<<":   " << this->WCS->crota[i] <<std::endl;
      }
    }

  os << indent << pre << "altlin:   " << this->WCS->altlin <<std::endl;
  os << indent << pre << "velref:   " << this->WCS->velref <<std::endl;
  os << indent << pre << "alt:   " << this->WCS->alt <<std::endl;
  os << indent << pre << "colnum:   " << this->WCS->colnum <<std::endl;

  if (this->WCS->colax)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      os << indent << pre << "colax"<<i<<":   " << this->WCS->colax[i] <<std::endl;
      }
    }

  if (this->WCS->wcsname[0] == '\0')
    {
    os << indent << pre << "wcsname:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "wcsname:   " << this->WCS->wcsname <<std::endl;
    }

  if (this->WCS->cname)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (this->WCS->cname[i][0] == '\0')
        {
        os << indent << pre << "cname"<<i<<":   " << und <<std::endl;
        }
      else
        {
        os << indent << pre << "cname"<<i<<":   " << this->WCS->cname[i] <<std::endl;
        }
      }
    }

  if (this->WCS->crder)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (undefined(this->WCS->crder[i]))
        {
        os << indent << pre << "crder"<<i<<":   " << und <<std::endl;
        }
      else
        {
        os << indent << pre << "crder"<<i<<":   " << this->WCS->crder[i] <<std::endl;
        }
      }
    }

  if (this->WCS->csyer)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (undefined(this->WCS->csyer[i]))
        {
        os << indent << pre << "csyer"<<i<<":   " << und <<std::endl;
        }
      else
        {
        os << indent << pre << "csyer"<<i<<":   " << this->WCS->csyer[i] <<std::endl;
        }
      }
    }

  if (this->WCS->radesys[0] == '\0')
    {
    os << indent << pre << "radesys:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "radesys:   " << this->WCS->radesys <<std::endl;
    }

  if (undefined(this->WCS->equinox))
    {
    os << indent << pre << "equinox:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "equinox:   " << this->WCS->equinox <<std::endl;
    }

  if (this->WCS->specsys[0] == '\0')
    {
    os << indent << pre << "specsys:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "specsys:   " << this->WCS->specsys <<std::endl;
    }

  if (this->WCS->ssysobs[0] == '\0')
    {
    os << indent << pre << "ssysobs:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "ssysobs:   " << this->WCS->ssysobs <<std::endl;
    }

  if (undefined(this->WCS->velosys))
    {
    os << indent << pre << "velosys:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "velosys:   " << this->WCS->velosys <<std::endl;
    }

  if (this->WCS->ssyssrc[0] == '\0')
    {
    os << indent << pre << "ssyssrc:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "ssyssrc:   " << this->WCS->ssyssrc <<std::endl;
    }

  if (undefined(this->WCS->zsource))
    {
    os << indent << pre << "zsource:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "zsource:   " << this->WCS->zsource <<std::endl;
    }

  for (i = 0; i < 3; i++)
    {
    if (undefined(this->WCS->obsgeo[i]))
      {
      os << indent << pre << "obsgeo"<<i<<":   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "obsgeo"<<i<<":   " << this->WCS->obsgeo[i] <<std::endl;
      }
    }

  if (this->WCS->dateobs[0] == '\0')
    {
    os << indent << pre << "dateobs:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "dateobs:   " << this->WCS->dateobs <<std::endl;
    }

  if (this->WCS->dateavg[0] == '\0')
    {
    os << indent << pre << "dateavg:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "dateavg:   " << this->WCS->dateavg <<std::endl;
    }

  if (undefined(this->WCS->mjdobs))
    {
    os << indent << pre << "mjdobs:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "mjdobs:   " << this->WCS->mjdobs <<std::endl;
    }

  if (undefined(this->WCS->mjdavg))
    {
    os << indent << pre << "mjdavg:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "mjdavg:   " << this->WCS->mjdavg <<std::endl;
    }
}
