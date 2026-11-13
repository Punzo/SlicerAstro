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

// STD includes
#include <string>
#include <cstdlib>
#include <math.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>

// AstroVolume includes
#include <vtkSlicerAstroConfigure.h>
#include "vtkSlicerAstroSMPTools.h"

// MRML includes
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLVolumePropertyNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>
#include <functional>


//------------------------------------------------------------------------------
const char* vtkMRMLAstroVolumeNode::PRESET_REFERENCE_ROLE = "preset";
const char* vtkMRMLAstroVolumeNode::VOLUMEPROPERTY_REFERENCE_ROLE = "volumeProperty";
const char* vtkMRMLAstroVolumeNode::ROI_REFERENCE_ROLE = "ROI";
const char* vtkMRMLAstroVolumeNode::ROI_ALIGNMENTTRANSFORM_REFERENCE_ROLE = "roiAlignmentTransform";

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

//----------------------------------------------------------------------------
const char *vtkMRMLAstroVolumeNode::GetROINodeReferenceRole()
{
  return vtkMRMLAstroVolumeNode::ROI_REFERENCE_ROLE;
}

//----------------------------------------------------------------------------
const char *vtkMRMLAstroVolumeNode::GetROIAlignmentTransformNodeReferenceRole()
{
  return vtkMRMLAstroVolumeNode::ROI_ALIGNMENTTRANSFORM_REFERENCE_ROLE;
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
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

//----------------------------------------------------------------------------
template <typename T> bool isNaN(T value)
{
  return value != value;
}

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::ReadXMLAttributes(const char** atts)
{
  this->Superclass::ReadXMLAttributes(atts);
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
vtkMRMLVolumeRenderingDisplayNode *vtkMRMLAstroVolumeNode::GetAstroVolumeRenderingDisplayNode()
{  
  return vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(this->GetNthDisplayNode(this->GetNumberOfDisplayNodes() - 1));
}

//---------------------------------------------------------------------------
namespace
{
template <typename T>
void SampleFiniteValues(const T* values, vtkIdType count, vtkIdType stride, std::vector<double>& samples)
{
  for (vtkIdType index = 0; index < count; index += stride)
    {
    const double value = static_cast<double>(values[index]);
    if (std::isfinite(value))
      {
      samples.push_back(value);
      }
    }
}
}

//---------------------------------------------------------------------------
bool vtkMRMLAstroVolumeNode::GetDisplayRange(double range[2])
{
  vtkImageData* imageData = this->GetImageData();
  if (!imageData || !imageData->GetPointData() || !imageData->GetPointData()->GetScalars())
    {
    return false;
    }
  vtkDataArray* scalars = imageData->GetPointData()->GetScalars();
  const vtkIdType count = scalars->GetNumberOfTuples() * scalars->GetNumberOfComponents();
  const vtkIdType maximumSamples = 1000000;
  const vtkIdType stride = std::max<vtkIdType>(1, count / maximumSamples);
  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(std::min(count, maximumSamples + 1)));
  switch (scalars->GetDataType())
    {
    vtkTemplateMacro(SampleFiniteValues(static_cast<const VTK_TT*>(scalars->GetVoidPointer(0)), count, stride, samples));
    default:
      return false;
    }
  if (samples.empty())
    {
    return false;
    }
  // extremes of all the voxels (the sample may miss them)
  double dataRange[2] = { VTK_DOUBLE_MAX, VTK_DOUBLE_MIN };
  scalars->GetFiniteRange(dataRange, 0);
  if (dataRange[0] > dataRange[1])
    {
    dataRange[0] = samples.front();
    dataRange[1] = samples.front();
    }
  const size_t last = samples.size() - 1;
  const size_t highIndex = static_cast<size_t>(0.999 * last);
  const size_t lowIndex = static_cast<size_t>(0.001 * last);
  std::nth_element(samples.begin(), samples.begin() + highIndex, samples.end());
  const double high = samples[highIndex];
  std::nth_element(samples.begin(), samples.begin() + lowIndex, samples.begin() + highIndex);
  const double low = samples[lowIndex];
  range[0] = dataRange[0];
  range[1] = dataRange[1];
  // isolated outliers (bright point sources): the percentiles
  if (high > 0. && dataRange[1] > 10. * high)
    {
    range[1] = high;
    }
  if (low < 0. && dataRange[0] < 10. * low)
    {
    range[0] = low;
    }
  if (range[1] <= range[0])
    {
    range[0] = dataRange[0];
    range[1] = dataRange[1];
    }
  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLAstroVolumeNode::UpdateRangeAttributes()
{     
  if (!this->GetImageData())
   {
   return false;
   }

  this->GetImageData()->Modified();
  vtkImageData* imageData = this->GetImageData();
  const vtkIdType numElements = imageData->GetNumberOfPoints();
  const int DataType = imageData->GetPointData()->GetScalars()->GetDataType();
  // Initial values: kept if all the voxels are blank (NaN)
  double max_val = imageData->GetScalarTypeMin(), min_val = imageData->GetScalarTypeMax();
  void* scalars = imageData->GetScalarPointer();

  switch (DataType)
    {
    case VTK_SHORT:
      vtkSlicerAstroSMPTools::Range(static_cast<short*>(scalars), 0, numElements, min_val, max_val);
      break;
    case VTK_FLOAT:
      vtkSlicerAstroSMPTools::Range(static_cast<float*>(scalars), 0, numElements, min_val, max_val);
      break;
    case VTK_DOUBLE:
      vtkSlicerAstroSMPTools::Range(static_cast<double*>(scalars), 0, numElements, min_val, max_val);
      break;
    default:
      vtkErrorMacro("vtkSlicerAstroVolumeLogic::UpdateRangeAttributes : "
                    "attempt to allocate scalars of type not allowed");
      return false;
    }

  int wasModifying = this->StartModify();
  this->SetAttribute("SlicerAstro.DATAMAX", DoubleToString(max_val).c_str());
  this->SetAttribute("SlicerAstro.DATAMIN", DoubleToString(min_val).c_str());

  vtkMRMLAstroVolumeDisplayNode* displayNode = this->GetAstroVolumeDisplayNode();
  // (the display of a volume of a scene is the saved one)
  if (displayNode && !(this->GetScene() && this->GetScene()->IsImporting()))
    {
    double displayRange[2] = { min_val, max_val };
    this->GetDisplayRange(displayRange);
    double window = displayRange[1] - displayRange[0];
    double level = 0.5 * (displayRange[1] + displayRange[0]);
    int disModify = displayNode->StartModify();
    displayNode->SetWindowLevel(window, level);
    displayNode->SetThreshold(min_val, max_val);
    displayNode->EndModify(disModify);
    }

  this->EndModify(wasModifying);


  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLAstroVolumeNode::UpdateDisplayThresholdAttributes()
{
  if (!this->GetImageData())
   {
   return false;
   }

  // Calculate the noise as the std of 6 slices of the datacube.
  // The DisplayThreshold = noise
  // 3D color function starts from 3 times the value of DisplayThreshold.
  int *dims = this->GetImageData()->GetDimensions();
  const int DataType = this->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  void* scalars = this->GetImageData()->GetScalarPointer(0,0,0);
  if (DataType != VTK_SHORT && DataType != VTK_FLOAT && DataType != VTK_DOUBLE)
    {
    vtkErrorMacro("vtkMRMLAstroVolumeNode::UpdateDisplayThresholdAttributes : "
                  "attempt to allocate scalars of type not allowed");
    return false;
    }

  // RMS of the voxels in [lowBoundary, highBoundary] (NaN voxels ignored)
  std::function<double(vtkIdType, vtkIdType)> rms = [&](vtkIdType lowBoundary, vtkIdType highBoundary) -> double
    {
    const vtkIdType cont = highBoundary - lowBoundary;
    if (cont <= 0)
      {
      return 0.;
      }
    double mean = 0., sumSq = 0.;
    switch (DataType)
      {
      case VTK_SHORT:
        mean = vtkSlicerAstroSMPTools::Sum(static_cast<short*>(scalars), lowBoundary, highBoundary + 1) / cont;
        sumSq = vtkSlicerAstroSMPTools::SumSquaredDeviations(static_cast<short*>(scalars), lowBoundary, highBoundary + 1, mean);
        break;
      case VTK_FLOAT:
        mean = vtkSlicerAstroSMPTools::Sum(static_cast<float*>(scalars), lowBoundary, highBoundary + 1) / cont;
        sumSq = vtkSlicerAstroSMPTools::SumSquaredDeviations(static_cast<float*>(scalars), lowBoundary, highBoundary + 1, mean);
        break;
      case VTK_DOUBLE:
        mean = vtkSlicerAstroSMPTools::Sum(static_cast<double*>(scalars), lowBoundary, highBoundary + 1) / cont;
        sumSq = vtkSlicerAstroSMPTools::SumSquaredDeviations(static_cast<double*>(scalars), lowBoundary, highBoundary + 1, mean);
        break;
      }
    return sqrt(sumSq / cont);
    };

  const vtkIdType nx = dims[0], nxy = static_cast<vtkIdType>(dims[0]) * dims[1];
  const int naxis = StringToInt(this->GetAttribute("SlicerAstro.NAXIS"));
  double noise1 = 0., noise2 = 0., noise = 0.;
  // noise from the first and the last channels (rows, pixels for 2D/1D data)
  if (naxis == 3)
    {
    noise1 = rms(nxy * 2, nxy * 4);
    noise2 = rms(nxy * (dims[2] - 4), nxy * (dims[2] - 2));
    }
  else if (naxis == 2)
    {
    noise1 = rms(nx * 2, nx * 4);
    noise2 = rms(nx * (dims[1] - 4), nx * (dims[1] - 2));
    }
  else
    {
    noise1 = rms(2, 4);
    noise2 = rms(dims[0] - 4, dims[0] - 2);
    }

  noise = (noise1 + noise2) * 0.5;

  if (noise < 1.E-6)
    {
    double MAX = StringToDouble(this->GetAttribute("SlicerAstro.DATAMAX"));
    double MIN = StringToDouble(this->GetAttribute("SlicerAstro.DATAMIN"));
    noise = (MAX - MIN) * 0.01;
    }

  this->SetDisplayThreshold(noise);

  return true;
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetDisplayThreshold(double DisplayThreshold)
{
  this->SetAttribute("SlicerAstro.DisplayThreshold", DoubleToString(DisplayThreshold).c_str());
  this->InvokeCustomModifiedEvent(vtkMRMLAstroVolumeNode::DisplayThresholdModifiedEvent);
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeNode::GetDisplayThreshold()
{
  return StringToDouble(this->GetAttribute("SlicerAstro.DisplayThreshold"));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetROINode(vtkMRMLMarkupsROINode* node)
{
  this->SetNodeReferenceID(this->GetROINodeReferenceRole(), (node ? node->GetID() : nullptr));
}

//----------------------------------------------------------------------------
vtkMRMLMarkupsROINode *vtkMRMLAstroVolumeNode::GetROINode()
{
  return vtkMRMLMarkupsROINode::SafeDownCast(this->GetNodeReference(this->GetROINodeReferenceRole()));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetROIAlignmentTransformNode(vtkMRMLTransformNode* node)
{
  this->SetNodeReferenceID(this->GetROIAlignmentTransformNodeReferenceRole(), (node ? node->GetID() : nullptr));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::DeleteROIAlignmentTransformNode()
{
  vtkMRMLTransformNode* transformNode = this->GetROIAlignmentTransformNode();
  if (transformNode)
    {
    this->SetROIAlignmentTransformNodeID(nullptr);
    if (this->GetScene())
      {
      this->GetScene()->RemoveNode(transformNode);
      }
    }
}

//----------------------------------------------------------------------------
vtkMRMLTransformNode *vtkMRMLAstroVolumeNode::GetROIAlignmentTransformNode()
{
  return vtkMRMLTransformNode::SafeDownCast(this->GetNodeReference
                                           (this->GetROIAlignmentTransformNodeReferenceRole()));
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetROIAlignmentTransformNodeID(const char *nodeID)
{
  this->SetNodeReferenceID(this->GetROIAlignmentTransformNodeReferenceRole(), nodeID);
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetPresetNode(vtkMRMLVolumePropertyNode *node)
{
  this->SetNodeReferenceID(this->GetPresetNodeReferenceRole(), (node ? node->GetID() : nullptr));
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetPresetNode(vtkMRMLNode *node)
{
  this->SetPresetNode(vtkMRMLVolumePropertyNode::SafeDownCast(node));
}

//-----------------------------------------------------------
vtkMRMLNode *vtkMRMLAstroVolumeNode::GetPresetNode()
{
  return this->GetNodeReference(this->GetPresetNodeReferenceRole());
}

//-----------------------------------------------------------
int vtkMRMLAstroVolumeNode::GetPresetIndex()
{
  vtkMRMLNode* presetNode = this->GetPresetNode();
  if (!presetNode)
    {
    return vtkMRMLAstroVolumeNode::ThreeSurfacesPreset;
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
  else if (presetName.find("OneSurfaceGreen") != std::string::npos)
    {
    return vtkMRMLAstroVolumeNode::OneSurfaceGreenPreset;
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
  else if (presetName.find("BrightSurface") != std::string::npos)
    {
    return vtkMRMLAstroVolumeNode::BrightSurface;
    }
  else
    {
    return vtkMRMLAstroVolumeNode::ThreeSurfacesPreset;
    }
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetVolumePropertyNode(vtkMRMLVolumePropertyNode *node)
{
  this->SetNodeReferenceID(this->GetVolumePropertyNodeReferenceRole(), (node ? node->GetID() : nullptr));
}

//-----------------------------------------------------------
void vtkMRMLAstroVolumeNode::SetVolumePropertyNode(vtkMRMLNode *node)
{
  this->SetVolumePropertyNode(vtkMRMLVolumePropertyNode::SafeDownCast(node));
}

//-----------------------------------------------------------
vtkMRMLNode *vtkMRMLAstroVolumeNode::GetVolumePropertyNode()
{
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
  if(displayNode == nullptr)
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
