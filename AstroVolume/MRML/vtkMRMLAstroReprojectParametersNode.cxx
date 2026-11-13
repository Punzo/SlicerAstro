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
#include <vtkMRMLVolumeNode.h>

// CropModuleMRML includes
#include <vtkMRMLAstroReprojectParametersNode.h>

// STD includes
#include <iomanip>
#include <sstream>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroReprojectParametersNode);

//----------------------------------------------------------------------------
vtkMRMLAstroReprojectParametersNode::vtkMRMLAstroReprojectParametersNode()
{
  this->HideFromEditors = 1;

  this->InputVolumeNodeID = nullptr;
  this->ReferenceVolumeNodeID = nullptr;
  this->OutputVolumeNodeID = nullptr;
  this->ReprojectRotation = false;
  this->ReprojectData = false;
  this->OutputSerial = 1;
  this->InterpolationOrder = 1;
  this->Cores = 0;
  this->Status = 0;
  this->InputVolumeNodeIDs = "";
  this->PrimaryBeamVolumeNodeIDs = "";
  this->InputNoises = "";
  this->WeightVolumeNodeID = "";
  this->SensitivityVolumeNodeID = "";
  this->SaultVolumeNodeID = "";
  this->CelestialFrame = "ICRS";
  this->Projection = "SIN";
  this->SpectralType = "VRAD";
  this->SpectralFrame = "BARYCENT";
  this->SpatialGridMode = 0;
  this->SpectralGridMode = 0;
  this->SizeX = 100;
  this->SizeY = 100;
  this->SpectralCount = 1;
  this->SpectralMethod = 4;
  this->SpectralMatching = 0;
  this->Combination = 1;
  this->InputCorrection = 0;
  this->PrimaryBeamModel = 0;
  this->ResolutionMode = 0;
  this->Supersampling = 0;
  this->NoiseWeighting = 0;
  this->ComputeWeight = 0;
  this->ComputeSensitivity = 0;
  this->SaultImage = 0;
  this->Equinox = 2000.;
  this->CenterLongitude = 0.;
  this->CenterLatitude = 0.;
  this->PixelSizeX = 1.;
  this->PixelSizeY = 1.;
  this->Rotation = 0.;
  this->SpatialResampleFactor = 1.;
  this->RestFrequency = 1.420405751768e9;
  this->SpectralStart = 0.;
  this->SpectralStep = 1000.;
  this->SpectralResampleFactor = 1.;
  this->DishDiameter = 25.;
  this->PrimaryBeamFactor = 1.13;
  this->PrimaryBeamCutoff = 0.2;
  this->MinimumCoverage = 0.5;
  this->TargetBeamMajor = 0.;
  this->TargetBeamMinor = 0.;
  this->TargetBeamPA = 0.;
  this->SaultFraction = 0.1;
}

//----------------------------------------------------------------------------
vtkMRMLAstroReprojectParametersNode::~vtkMRMLAstroReprojectParametersNode()
{
  if (this->InputVolumeNodeID)
    {
    delete [] this->InputVolumeNodeID;
    this->InputVolumeNodeID = nullptr;
    }

  if (this->ReferenceVolumeNodeID)
    {
    delete [] this->ReferenceVolumeNodeID;
    this->ReferenceVolumeNodeID = nullptr;
    }

  if (this->OutputVolumeNodeID)
    {
    delete [] this->OutputVolumeNodeID;
    this->OutputVolumeNodeID = nullptr;
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
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroReprojectParametersNode::ReadXMLAttributes(const char** atts)
{
  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;
  while (*atts != nullptr)
    {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, "inputVolumeNodeID"))
      {
      this->SetInputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "referenceVolumeNodeID"))
      {
      this->SetReferenceVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "outputVolumeNodeID"))
      {
      this->SetOutputVolumeNodeID(attValue);
      continue;
      }

    if (!strcmp(attName, "ReprojectRotation"))
      {
      this->ReprojectRotation = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "ReprojectData"))
      {
      this->ReprojectData = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "InterpolationOrder"))
      {
      this->InterpolationOrder = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "OutputSerial"))
      {
      this->OutputSerial = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Cores"))
      {
      this->Cores = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Status"))
      {
      this->Status = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "InputVolumeNodeIDs"))
      {
      this->InputVolumeNodeIDs = attValue;
      continue;
      }

    if (!strcmp(attName, "PrimaryBeamVolumeNodeIDs"))
      {
      this->PrimaryBeamVolumeNodeIDs = attValue;
      continue;
      }

    if (!strcmp(attName, "InputNoises"))
      {
      this->InputNoises = attValue;
      continue;
      }

    if (!strcmp(attName, "WeightVolumeNodeID"))
      {
      this->WeightVolumeNodeID = attValue;
      continue;
      }

    if (!strcmp(attName, "SensitivityVolumeNodeID"))
      {
      this->SensitivityVolumeNodeID = attValue;
      continue;
      }

    if (!strcmp(attName, "SaultVolumeNodeID"))
      {
      this->SaultVolumeNodeID = attValue;
      continue;
      }

    if (!strcmp(attName, "CelestialFrame"))
      {
      this->CelestialFrame = attValue;
      continue;
      }

    if (!strcmp(attName, "Projection"))
      {
      this->Projection = attValue;
      continue;
      }

    if (!strcmp(attName, "SpectralType"))
      {
      this->SpectralType = attValue;
      continue;
      }

    if (!strcmp(attName, "SpectralFrame"))
      {
      this->SpectralFrame = attValue;
      continue;
      }

    if (!strcmp(attName, "SpatialGridMode"))
      {
      this->SpatialGridMode = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "SpectralGridMode"))
      {
      this->SpectralGridMode = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "SizeX"))
      {
      this->SizeX = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "SizeY"))
      {
      this->SizeY = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "SpectralCount"))
      {
      this->SpectralCount = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "SpectralMethod"))
      {
      this->SpectralMethod = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "SpectralMatching"))
      {
      this->SpectralMatching = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Combination"))
      {
      this->Combination = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "InputCorrection"))
      {
      this->InputCorrection = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "PrimaryBeamModel"))
      {
      this->PrimaryBeamModel = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "ResolutionMode"))
      {
      this->ResolutionMode = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Supersampling"))
      {
      this->Supersampling = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "NoiseWeighting"))
      {
      this->NoiseWeighting = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "ComputeWeight"))
      {
      this->ComputeWeight = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "ComputeSensitivity"))
      {
      this->ComputeSensitivity = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "SaultImage"))
      {
      this->SaultImage = StringToInt(attValue);
      continue;
      }

    if (!strcmp(attName, "Equinox"))
      {
      this->Equinox = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "CenterLongitude"))
      {
      this->CenterLongitude = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "CenterLatitude"))
      {
      this->CenterLatitude = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "PixelSizeX"))
      {
      this->PixelSizeX = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "PixelSizeY"))
      {
      this->PixelSizeY = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "Rotation"))
      {
      this->Rotation = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "SpatialResampleFactor"))
      {
      this->SpatialResampleFactor = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "RestFrequency"))
      {
      this->RestFrequency = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "SpectralStart"))
      {
      this->SpectralStart = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "SpectralStep"))
      {
      this->SpectralStep = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "SpectralResampleFactor"))
      {
      this->SpectralResampleFactor = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "DishDiameter"))
      {
      this->DishDiameter = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "PrimaryBeamFactor"))
      {
      this->PrimaryBeamFactor = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "PrimaryBeamCutoff"))
      {
      this->PrimaryBeamCutoff = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "MinimumCoverage"))
      {
      this->MinimumCoverage = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "TargetBeamMajor"))
      {
      this->TargetBeamMajor = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "TargetBeamMinor"))
      {
      this->TargetBeamMinor = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "TargetBeamPA"))
      {
      this->TargetBeamPA = StringToNumber<double>(attValue);
      continue;
      }

    if (!strcmp(attName, "SaultFraction"))
      {
      this->SaultFraction = StringToNumber<double>(attValue);
      continue;
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroReprojectParametersNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);
  const std::streamsize precision = of.precision();

  if (this->InputVolumeNodeID != nullptr)
    {
    of << indent << " inputVolumeNodeID=\"" << this->InputVolumeNodeID << "\"";
    }

  if (this->ReferenceVolumeNodeID != nullptr)
    {
    of << indent << " referenceVolumeNodeID=\"" << this->ReferenceVolumeNodeID << "\"";
    }

  if (this->OutputVolumeNodeID != nullptr)
    {
    of << indent << " outputVolumeNodeID=\"" << this->OutputVolumeNodeID << "\"";
    }

  of << indent << " ReprojectRotation=\"" << this->ReprojectRotation << "\"";
  of << indent << " ReprojectData=\"" << this->ReprojectData << "\"";
  of << indent << " OutputSerial=\"" << this->OutputSerial << "\"";
  of << indent << " InterpolationOrder=\"" << this->InterpolationOrder << "\"";
  of << indent << " Cores=\"" << this->Cores << "\"";
  of << indent << " Status=\"" << this->Status << "\"";
  of << indent << " InputVolumeNodeIDs=\"" << this->InputVolumeNodeIDs << "\"";
  of << indent << " PrimaryBeamVolumeNodeIDs=\"" << this->PrimaryBeamVolumeNodeIDs << "\"";
  of << indent << " InputNoises=\"" << this->InputNoises << "\"";
  of << indent << " WeightVolumeNodeID=\"" << this->WeightVolumeNodeID << "\"";
  of << indent << " SensitivityVolumeNodeID=\"" << this->SensitivityVolumeNodeID << "\"";
  of << indent << " SaultVolumeNodeID=\"" << this->SaultVolumeNodeID << "\"";
  of << indent << " CelestialFrame=\"" << this->CelestialFrame << "\"";
  of << indent << " Projection=\"" << this->Projection << "\"";
  of << indent << " SpectralType=\"" << this->SpectralType << "\"";
  of << indent << " SpectralFrame=\"" << this->SpectralFrame << "\"";
  of << indent << " SpatialGridMode=\"" << this->SpatialGridMode << "\"";
  of << indent << " SpectralGridMode=\"" << this->SpectralGridMode << "\"";
  of << indent << " SizeX=\"" << this->SizeX << "\"";
  of << indent << " SizeY=\"" << this->SizeY << "\"";
  of << indent << " SpectralCount=\"" << this->SpectralCount << "\"";
  of << indent << " SpectralMethod=\"" << this->SpectralMethod << "\"";
  of << indent << " SpectralMatching=\"" << this->SpectralMatching << "\"";
  of << indent << " Combination=\"" << this->Combination << "\"";
  of << indent << " InputCorrection=\"" << this->InputCorrection << "\"";
  of << indent << " PrimaryBeamModel=\"" << this->PrimaryBeamModel << "\"";
  of << indent << " ResolutionMode=\"" << this->ResolutionMode << "\"";
  of << indent << " Supersampling=\"" << this->Supersampling << "\"";
  of << indent << " NoiseWeighting=\"" << this->NoiseWeighting << "\"";
  of << indent << " ComputeWeight=\"" << this->ComputeWeight << "\"";
  of << indent << " ComputeSensitivity=\"" << this->ComputeSensitivity << "\"";
  of << indent << " SaultImage=\"" << this->SaultImage << "\"";
  of << indent << " Equinox=\"" << std::setprecision(17) << this->Equinox << "\"";
  of << indent << " CenterLongitude=\"" << std::setprecision(17) << this->CenterLongitude << "\"";
  of << indent << " CenterLatitude=\"" << std::setprecision(17) << this->CenterLatitude << "\"";
  of << indent << " PixelSizeX=\"" << std::setprecision(17) << this->PixelSizeX << "\"";
  of << indent << " PixelSizeY=\"" << std::setprecision(17) << this->PixelSizeY << "\"";
  of << indent << " Rotation=\"" << std::setprecision(17) << this->Rotation << "\"";
  of << indent << " SpatialResampleFactor=\"" << std::setprecision(17) << this->SpatialResampleFactor << "\"";
  of << indent << " RestFrequency=\"" << std::setprecision(17) << this->RestFrequency << "\"";
  of << indent << " SpectralStart=\"" << std::setprecision(17) << this->SpectralStart << "\"";
  of << indent << " SpectralStep=\"" << std::setprecision(17) << this->SpectralStep << "\"";
  of << indent << " SpectralResampleFactor=\"" << std::setprecision(17) << this->SpectralResampleFactor << "\"";
  of << indent << " DishDiameter=\"" << std::setprecision(17) << this->DishDiameter << "\"";
  of << indent << " PrimaryBeamFactor=\"" << std::setprecision(17) << this->PrimaryBeamFactor << "\"";
  of << indent << " PrimaryBeamCutoff=\"" << std::setprecision(17) << this->PrimaryBeamCutoff << "\"";
  of << indent << " MinimumCoverage=\"" << std::setprecision(17) << this->MinimumCoverage << "\"";
  of << indent << " TargetBeamMajor=\"" << std::setprecision(17) << this->TargetBeamMajor << "\"";
  of << indent << " TargetBeamMinor=\"" << std::setprecision(17) << this->TargetBeamMinor << "\"";
  of << indent << " TargetBeamPA=\"" << std::setprecision(17) << this->TargetBeamPA << "\"";
  of << indent << " SaultFraction=\"" << std::setprecision(17) << this->SaultFraction << "\"";
  of.precision(precision);
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, SliceID
void vtkMRMLAstroReprojectParametersNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();
  
  Superclass::Copy(anode);
  vtkMRMLAstroReprojectParametersNode *node = vtkMRMLAstroReprojectParametersNode::SafeDownCast(anode);

  this->SetInputVolumeNodeID(node->GetInputVolumeNodeID());
  this->SetReferenceVolumeNodeID(node->GetReferenceVolumeNodeID());
  this->SetOutputVolumeNodeID(node->GetOutputVolumeNodeID());
  this->SetReprojectRotation(node->GetReprojectRotation());
  this->SetReprojectData(node->GetReprojectData());
  this->SetInterpolationOrder(node->GetInterpolationOrder());
  this->SetOutputSerial(node->GetOutputSerial());
  this->SetCores(node->GetCores());
  this->SetStatus(node->GetStatus());
  this->InputVolumeNodeIDs = node->InputVolumeNodeIDs;
  this->PrimaryBeamVolumeNodeIDs = node->PrimaryBeamVolumeNodeIDs;
  this->InputNoises = node->InputNoises;
  this->WeightVolumeNodeID = node->WeightVolumeNodeID;
  this->SensitivityVolumeNodeID = node->SensitivityVolumeNodeID;
  this->SaultVolumeNodeID = node->SaultVolumeNodeID;
  this->CelestialFrame = node->CelestialFrame;
  this->Projection = node->Projection;
  this->SpectralType = node->SpectralType;
  this->SpectralFrame = node->SpectralFrame;
  this->SpatialGridMode = node->SpatialGridMode;
  this->SpectralGridMode = node->SpectralGridMode;
  this->SizeX = node->SizeX;
  this->SizeY = node->SizeY;
  this->SpectralCount = node->SpectralCount;
  this->SpectralMethod = node->SpectralMethod;
  this->SpectralMatching = node->SpectralMatching;
  this->Combination = node->Combination;
  this->InputCorrection = node->InputCorrection;
  this->PrimaryBeamModel = node->PrimaryBeamModel;
  this->ResolutionMode = node->ResolutionMode;
  this->Supersampling = node->Supersampling;
  this->NoiseWeighting = node->NoiseWeighting;
  this->ComputeWeight = node->ComputeWeight;
  this->ComputeSensitivity = node->ComputeSensitivity;
  this->SaultImage = node->SaultImage;
  this->Equinox = node->Equinox;
  this->CenterLongitude = node->CenterLongitude;
  this->CenterLatitude = node->CenterLatitude;
  this->PixelSizeX = node->PixelSizeX;
  this->PixelSizeY = node->PixelSizeY;
  this->Rotation = node->Rotation;
  this->SpatialResampleFactor = node->SpatialResampleFactor;
  this->RestFrequency = node->RestFrequency;
  this->SpectralStart = node->SpectralStart;
  this->SpectralStep = node->SpectralStep;
  this->SpectralResampleFactor = node->SpectralResampleFactor;
  this->DishDiameter = node->DishDiameter;
  this->PrimaryBeamFactor = node->PrimaryBeamFactor;
  this->PrimaryBeamCutoff = node->PrimaryBeamCutoff;
  this->MinimumCoverage = node->MinimumCoverage;
  this->TargetBeamMajor = node->TargetBeamMajor;
  this->TargetBeamMinor = node->TargetBeamMinor;
  this->TargetBeamPA = node->TargetBeamPA;
  this->SaultFraction = node->SaultFraction;
  this->Modified();

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLAstroReprojectParametersNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << indent << "InputVolumeNodeID: " << ( (this->InputVolumeNodeID) ? this->InputVolumeNodeID : "None" ) << "\n";
  os << indent << "ReferenceVolumeNodeID: " << ( (this->ReferenceVolumeNodeID) ? this->ReferenceVolumeNodeID : "None" ) << "\n";
  os << indent << "OutputVolumeNodeID: " << ( (this->OutputVolumeNodeID) ? this->OutputVolumeNodeID : "None" ) << "\n";
  os << indent << "ReprojectRotation: " << this->ReprojectRotation << "\n";
  os << indent << "ReprojectData: " << this->ReprojectData << "\n";
  os << indent << "InterpolationOrder: " << this->InterpolationOrder << "\n";
  os << indent << "OutputSerial: " << this->OutputSerial << "\n";
  os << indent << "Cores: " << this->Cores << "\n";
  os << indent << "Status: " << this->Status << "\n";
  os << indent << "InputVolumeNodeIDs: " << this->InputVolumeNodeIDs << "\n";
  os << indent << "PrimaryBeamVolumeNodeIDs: " << this->PrimaryBeamVolumeNodeIDs << "\n";
  os << indent << "InputNoises: " << this->InputNoises << "\n";
  os << indent << "WeightVolumeNodeID: " << this->WeightVolumeNodeID << "\n";
  os << indent << "SensitivityVolumeNodeID: " << this->SensitivityVolumeNodeID << "\n";
  os << indent << "SaultVolumeNodeID: " << this->SaultVolumeNodeID << "\n";
  os << indent << "CelestialFrame: " << this->CelestialFrame << "\n";
  os << indent << "Projection: " << this->Projection << "\n";
  os << indent << "SpectralType: " << this->SpectralType << "\n";
  os << indent << "SpectralFrame: " << this->SpectralFrame << "\n";
  os << indent << "SpatialGridMode: " << this->SpatialGridMode << "\n";
  os << indent << "SpectralGridMode: " << this->SpectralGridMode << "\n";
  os << indent << "SizeX: " << this->SizeX << "\n";
  os << indent << "SizeY: " << this->SizeY << "\n";
  os << indent << "SpectralCount: " << this->SpectralCount << "\n";
  os << indent << "SpectralMethod: " << this->SpectralMethod << "\n";
  os << indent << "SpectralMatching: " << this->SpectralMatching << "\n";
  os << indent << "Combination: " << this->Combination << "\n";
  os << indent << "InputCorrection: " << this->InputCorrection << "\n";
  os << indent << "PrimaryBeamModel: " << this->PrimaryBeamModel << "\n";
  os << indent << "ResolutionMode: " << this->ResolutionMode << "\n";
  os << indent << "Supersampling: " << this->Supersampling << "\n";
  os << indent << "NoiseWeighting: " << this->NoiseWeighting << "\n";
  os << indent << "ComputeWeight: " << this->ComputeWeight << "\n";
  os << indent << "ComputeSensitivity: " << this->ComputeSensitivity << "\n";
  os << indent << "SaultImage: " << this->SaultImage << "\n";
  os << indent << "Equinox: " << this->Equinox << "\n";
  os << indent << "CenterLongitude: " << this->CenterLongitude << "\n";
  os << indent << "CenterLatitude: " << this->CenterLatitude << "\n";
  os << indent << "PixelSizeX: " << this->PixelSizeX << "\n";
  os << indent << "PixelSizeY: " << this->PixelSizeY << "\n";
  os << indent << "Rotation: " << this->Rotation << "\n";
  os << indent << "SpatialResampleFactor: " << this->SpatialResampleFactor << "\n";
  os << indent << "RestFrequency: " << this->RestFrequency << "\n";
  os << indent << "SpectralStart: " << this->SpectralStart << "\n";
  os << indent << "SpectralStep: " << this->SpectralStep << "\n";
  os << indent << "SpectralResampleFactor: " << this->SpectralResampleFactor << "\n";
  os << indent << "DishDiameter: " << this->DishDiameter << "\n";
  os << indent << "PrimaryBeamFactor: " << this->PrimaryBeamFactor << "\n";
  os << indent << "PrimaryBeamCutoff: " << this->PrimaryBeamCutoff << "\n";
  os << indent << "MinimumCoverage: " << this->MinimumCoverage << "\n";
  os << indent << "TargetBeamMajor: " << this->TargetBeamMajor << "\n";
  os << indent << "TargetBeamMinor: " << this->TargetBeamMinor << "\n";
  os << indent << "TargetBeamPA: " << this->TargetBeamPA << "\n";
  os << indent << "SaultFraction: " << this->SaultFraction << "\n";
}
