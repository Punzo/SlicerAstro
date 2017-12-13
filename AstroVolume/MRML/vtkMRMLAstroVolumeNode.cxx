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

#include <string>
#include <cstdlib>
#include <math.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumePropertyNode.h>

//------------------------------------------------------------------------------
const char* vtkMRMLAstroVolumeNode::PRESET_REFERENCE_ROLE = "preset";
const char* vtkMRMLAstroVolumeNode::VOLUMEPROPERTY_REFERENCE_ROLE = "volumeProperty";

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode::vtkMRMLAstroVolumeNode()
{
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode::~vtkMRMLAstroVolumeNode()
{
}

//----------------------------------------------------------------------------
const char *vtkMRMLAstroVolumeNode::GetPresetNodeReferenceRole()
{
  return vtkMRMLAstroVolumeNode::PRESET_REFERENCE_ROLE;
}

//----------------------------------------------------------------------------
const char *vtkMRMLAstroVolumeNode::GetVolumePropertyNodeReferenceRole()
{
  return vtkMRMLAstroVolumeNode::VOLUMEPROPERTY_REFERENCE_ROLE;
}

namespace
{
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
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

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
template <typename T> bool isNaN(T value)
{
  return value != value;
}

//----------------------------------------------------------------------------
bool DoubleIsNaN(double Value)
{
  return isNaN<double>(Value);
}

//----------------------------------------------------------------------------
bool FloatIsNaN(double Value)
{
  return isNaN<float>(Value);
}

//----------------------------------------------------------------------------
bool ShortIsNaN(double Value)
{
  return isNaN<short>(Value);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::ReadXMLAttributes(const char** atts)
{
  this->Superclass::ReadXMLAttributes(atts);

  this->WriteXML(std::cout,0);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::WriteXML(ostream& of, int nIndent)
{
  this->Superclass::WriteXML(of, nIndent);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::Copy(vtkMRMLNode *anode)
{
  vtkMRMLAstroVolumeNode *astroVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(anode);
  if (!astroVolumeNode)
    {
    return;
    }

  this->Superclass::Copy(astroVolumeNode);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}

//---------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode* vtkMRMLAstroVolumeNode::GetAstroVolumeDisplayNode()
{
  return vtkMRMLAstroVolumeDisplayNode::SafeDownCast(this->GetDisplayNode());
}

//---------------------------------------------------------------------------
bool vtkMRMLAstroVolumeNode::UpdateRangeAttributes()
{
  if (this->GetImageData() == NULL)
   {
   vtkErrorMacro("vtkMRMLAstroVolumeNode::UpdateRangeAttributes : "
                 "imageData not allocated.");
   return false;
   }
  this->GetImageData()->Modified();
  int *dims = this->GetImageData()->GetDimensions();
  int numElements = dims[0] * dims[1] * dims[2];
  const int DataType = this->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  double max = this->GetImageData()->GetScalarTypeMin(), min = this->GetImageData()->GetScalarTypeMax();
  short *outSPixel = NULL;
  float *outFPixel = NULL;
  double *outDPixel = NULL;

  switch (DataType)
    {
  case VTK_SHORT:
    outSPixel = static_cast<short*> (this->GetImageData()->GetScalarPointer());
    for (int elementCnt = 0; elementCnt < numElements; elementCnt++)
      {
      if (ShortIsNaN(*(outSPixel + elementCnt)))
        {
        continue;
        }
      if (*(outSPixel + elementCnt) > max)
        {
        max =  *(outSPixel + elementCnt);
        }
      if (*(outSPixel + elementCnt) < min)
        {
        min =  *(outSPixel + elementCnt);
        }
      }
    break;
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (this->GetImageData()->GetScalarPointer());
      for (int elementCnt = 0; elementCnt < numElements; elementCnt++)
        {
       if (FloatIsNaN(*(outFPixel + elementCnt)))
          {
          continue;
          }
        if (*(outFPixel + elementCnt) > max)
          {
          max =  *(outFPixel + elementCnt);
          }
        if (*(outFPixel + elementCnt) < min)
          {
          min =  *(outFPixel + elementCnt);
          }
        }
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (this->GetImageData()->GetScalarPointer());
      for (int elementCnt = 0; elementCnt < numElements; elementCnt++)
        {
        if (DoubleIsNaN(*(outDPixel + elementCnt)))
          {
          continue;
          }
        if (*(outDPixel + elementCnt) > max)
          {
          max =  *(outDPixel + elementCnt);
          }
        if (*(outDPixel + elementCnt) < min)
          {
          min =  *(outDPixel + elementCnt);
          }
        }
      break;
    default:
      vtkErrorMacro("vtkMRMLAstroVolumeNode::UpdateRangeAttributes() : "
                    "attempt to allocate scalars of type not allowed");
      return false;
    }

  this->SetAttribute("SlicerAstro.DATAMAX", DoubleToString(max).c_str());
  this->SetAttribute("SlicerAstro.DATAMIN", DoubleToString(min).c_str());

  outSPixel = NULL;
  outFPixel = NULL;
  outDPixel = NULL;
  delete outSPixel;
  delete outFPixel;
  delete outDPixel;

  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLAstroVolumeNode::Update3DDisplayThresholdAttributes()
{
  if (this->GetImageData() == NULL)
   {
   vtkErrorMacro("vtkMRMLAstroVolumeNode::Update3DDisplayThresholdAttributes : "
                 "imageData not allocated.");
   return false;
   }

  //We calculate the noise as the std of 6 slices of the datacube.
  int *dims = this->GetImageData()->GetDimensions();
  const int DataType = this->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  float *outFPixel = NULL;
  double *outDPixel = NULL;
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (this->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (this->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("vtkMRMLAstroVolumeNode::Update3DDisplayThresholdAttributes : "
                    "attempt to allocate scalars of type not allowed");
      return false;
    }
  double sum = 0., noise1 = 0., noise2 = 0, noise = 0., mean1 = 0., mean2 = 0., mean = 0.;
  int lowBoundary;
  int highBoundary;

  if (StringToInt(this->GetAttribute("SlicerAstro.NAXIS")) == 3)
    {
    lowBoundary = dims[0] * dims[1] * 2;
    highBoundary = dims[0] * dims[1] * 4;
    }
  else if (StringToInt(this->GetAttribute("SlicerAstro.NAXIS")) == 2)
    {
    lowBoundary = dims[0] * 2;
    highBoundary = dims[0] * 4;
    }
  else
    {
    lowBoundary = 2;
    highBoundary = 4;
    }

  int cont = highBoundary - lowBoundary;
  switch (DataType)
    {
    case VTK_FLOAT:
      for( int elemCnt = lowBoundary; elemCnt <= highBoundary; elemCnt++)
        {
        if (FloatIsNaN(*(outFPixel + elemCnt)))
           {
           continue;
           }
        sum += *(outFPixel + elemCnt);
        }
      sum /= cont;
      for( int elemCnt = lowBoundary; elemCnt <= highBoundary; elemCnt++)
        {
        if (FloatIsNaN(*(outFPixel + elemCnt)))
           {
           continue;
           }
        noise1 += (*(outFPixel + elemCnt) - sum) * (*(outFPixel+elemCnt) - sum);
        }
      noise1 = sqrt(noise1 / cont);
      break;
    case VTK_DOUBLE:
      for( int elemCnt = lowBoundary; elemCnt <= highBoundary; elemCnt++)
        {
        if (DoubleIsNaN(*(outDPixel + elemCnt)))
           {
           continue;
           }
        sum += *(outDPixel + elemCnt);
        }
      sum /= cont;
      for( int elemCnt = lowBoundary; elemCnt <= highBoundary; elemCnt++)
        {
        if (DoubleIsNaN(*(outDPixel + elemCnt)))
           {
           continue;
           }
        noise1 += (*(outDPixel + elemCnt) - sum) * (*(outDPixel+elemCnt) - sum);
        }
      noise1 = sqrt(noise1 / cont);
      break;
    }

  mean1 = sum;

  if (StringToInt(this->GetAttribute("SlicerAstro.NAXIS")) == 3)
    {
    lowBoundary = dims[0] * dims[1] * (dims[2] - 4);
    highBoundary = dims[0] * dims[1] * (dims[2] - 2);
    }
  else if (StringToInt(this->GetAttribute("SlicerAstro.NAXIS")) == 2)
    {
    lowBoundary = dims[0] * (dims[1] - 4);
    highBoundary = dims[0] * (dims[1] - 2);
    }
  else
    {
    lowBoundary = dims[0] - 4;
    highBoundary = dims[0] - 2;
    }

  sum = 0.;

  switch (DataType)
    {
    case VTK_FLOAT:
      for( int elemCnt = lowBoundary; elemCnt <= highBoundary; elemCnt++)
        {
        if (FloatIsNaN(*(outFPixel + elemCnt)))
           {
           continue;
           }
        sum += *(outFPixel + elemCnt);
        }
      sum /= cont;
      for( int elemCnt = lowBoundary; elemCnt <= highBoundary; elemCnt++)
        {
        if (FloatIsNaN(*(outFPixel + elemCnt)))
           {
           continue;
           }
        noise2 += (*(outFPixel + elemCnt) - sum) * (*(outFPixel+elemCnt) - sum);
        }
      noise2 = sqrt(noise2 / cont);
      break;
    case VTK_DOUBLE:
      for( int elemCnt = lowBoundary; elemCnt <= highBoundary; elemCnt++)
        {
        if (DoubleIsNaN(*(outDPixel + elemCnt)))
           {
           continue;
           }
        sum += *(outDPixel + elemCnt);
        }
      sum /= cont;
      for( int elemCnt = lowBoundary; elemCnt <= highBoundary; elemCnt++)
        {
        if (DoubleIsNaN(*(outDPixel + elemCnt)))
           {
           continue;
           }
        noise2 += (*(outDPixel + elemCnt) - sum) * (*(outDPixel+elemCnt) - sum);
        }
      noise2 = sqrt(noise2 / cont);
      break;
    }

  mean2 = sum;

  noise = (noise1 + noise2) * 0.5;
  mean = (mean1 + mean2) * 0.5;

  outFPixel = NULL;
  outDPixel = NULL;
  delete outFPixel;
  delete outDPixel;

  this->Set3DDisplayThreshold(noise);
  this->SetAttribute("SlicerAstro.3DDisplayThresholdMean", DoubleToString(mean).c_str());

  return true;
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::Set3DDisplayThreshold(double DisplayThreshold)
{
  this->SetAttribute("SlicerAstro.3DDisplayThreshold", DoubleToString(DisplayThreshold).c_str());
  this->InvokeCustomModifiedEvent(vtkMRMLAstroVolumeNode::DisplayThresholdModifiedEvent);
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetPresetNode(vtkMRMLVolumePropertyNode *node)
{
  this->SetNodeReferenceID(this->GetPresetNodeReferenceRole(), (node ? node->GetID() : NULL));
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetPresetNode(vtkMRMLNode *node)
{
  this->SetPresetNode(vtkMRMLVolumePropertyNode::SafeDownCast(node));
}

//-----------------------------------------------------------
vtkMRMLNode *vtkMRMLAstroVolumeNode::GetPresetNode()
{
  if (!this->Scene)
    {
    return NULL;
    }

  return this->GetNodeReference(this->GetPresetNodeReferenceRole());
}

//-----------------------------------------------------------
int vtkMRMLAstroVolumeNode::GetPresetIndex()
{
  vtkMRMLNode* presetNode = this->GetPresetNode();
  if (!presetNode)
    {
    return vtkMRMLAstroVolumeNode::LowConstantOpacityPreset;
    }

  std::string presetName = presetNode->GetName();

  if (presetName.find("LowConstantOpacity") != std::string::npos)
    {
    return vtkMRMLAstroVolumeNode::LowConstantOpacityPreset;
    }
  else if (presetName.find("MediumConstantOpacity") != std::string::npos)
    {
    return vtkMRMLAstroVolumeNode::MediumConstantOpacityPreset;
    }
  else if (presetName.find("HighConstantOpacity") != std::string::npos)
    {
    return vtkMRMLAstroVolumeNode::HighConstantOpacityPreset;
    }
  else if (presetName.find("OneSurface") != std::string::npos)
    {
    return vtkMRMLAstroVolumeNode::OneSurfacePreset;
    }
  else if (presetName.find("OneSurfaceWhite") != std::string::npos)
    {
    return vtkMRMLAstroVolumeNode::OneSurfaceWhitePreset;
    }
  else if (presetName.find("TwoSurfaces") != std::string::npos)
    {
    return vtkMRMLAstroVolumeNode::TwoSurfacesPreset;
    }
  else if (presetName.find("ThreeSurfaces") != std::string::npos)
    {
    return vtkMRMLAstroVolumeNode::ThreeSurfacesPreset;
    }
  else
    {
    return vtkMRMLAstroVolumeNode::LowConstantOpacityPreset;
    }
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetVolumePropertyNode(vtkMRMLVolumePropertyNode *node)
{
  this->SetNodeReferenceID(this->GetVolumePropertyNodeReferenceRole(), (node ? node->GetID() : NULL));
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetVolumePropertyNode(vtkMRMLNode *node)
{
  this->SetVolumePropertyNode(vtkMRMLVolumePropertyNode::SafeDownCast(node));
}

//-----------------------------------------------------------
vtkMRMLNode *vtkMRMLAstroVolumeNode::GetVolumePropertyNode()
{
  if (!this->Scene)
    {
    return NULL;
    }

  return this->GetNodeReference(this->GetVolumePropertyNodeReferenceRole());
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::CreateNoneNode(vtkMRMLScene *scene)
{
  // Create a None volume RGBA of 0, 0, 0 so the filters won't complain
  // about missing input
  vtkNew<vtkImageData> id;
  id->SetDimensions(1, 1, 1);
  id->AllocateScalars(VTK_DOUBLE, 4);
  id->GetPointData()->GetScalars()->FillComponent(0, 0.0);
  id->GetPointData()->GetScalars()->FillComponent(1, 125.0);
  id->GetPointData()->GetScalars()->FillComponent(2, 0.0);
  id->GetPointData()->GetScalars()->FillComponent(3, 0.0);

  vtkNew<vtkMRMLAstroVolumeNode> n;
  n->SetName("None");
  // the scene will set the id
  n->SetAndObserveImageData(id.GetPointer());
  scene->AddNode(n.GetPointer());
}

//---------------------------------------------------------------------------
vtkMRMLStorageNode* vtkMRMLAstroVolumeNode::CreateDefaultStorageNode()
{
  return vtkMRMLAstroVolumeStorageNode::New();
}

//---------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::CreateDefaultDisplayNodes()
{
  vtkMRMLAstroVolumeDisplayNode *displayNode = 
    vtkMRMLAstroVolumeDisplayNode::SafeDownCast(this->GetDisplayNode());
  if(displayNode == NULL)
    {
    displayNode = vtkMRMLAstroVolumeDisplayNode::New();
    if(this->GetScene())
      {
      displayNode->SetScene(this->GetScene());
      this->GetScene()->AddNode(displayNode);
      displayNode->SetDefaultColorMap();
      displayNode->Delete();

      this->SetAndObserveDisplayNodeID(displayNode->GetID());
      vtkWarningMacro("Display node set and observed" << endl);
      }
    }
}
