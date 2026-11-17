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
#include "vtkSlicerAstroProfilesLogic.h"
#include "vtkSlicerAstroConfigure.h"
#include <functional>
#include <vtkSlicerAstroSMPTools.h>
#include <type_traits>

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLAstroProfilesParametersNode.h>
#include <vtkMRMLTableNode.h>

// VTK includes
#include <vtkArrayData.h>
#include <vtkCacheManager.h>
#include <vtkImageData.h>
#include <cmath>
#include <vector>
#include <limits>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkVersion.h>

// STD includes
#include <cassert>
#include <iostream>
#include <chrono>

#define DOUBLEPRECISION 1.E-16

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
// voxels of each channel summed in a profile
struct ProfileSelection
{
  const unsigned char* Mask = nullptr;      // voxels of the mask
  const unsigned char* Footprint = nullptr; // sky area of the mask (aperture), all channels
  bool Threshold = false;                   // voxels within (IntensityMin, IntensityMax)
  double IntensityMin = 0.;
  double IntensityMax = 0.;
  int Zmin = 0;                             // channels of the profile
  int Zmax = 0;
  double UnitConversion = 1.;               // Jy/beam -> Jy (1: sum of the intensities)
};

//----------------------------------------------------------------------------
// Profile: in each channel the sum of the selected voxels (double
// accumulation, blank voxels skipped) times the unit conversion; 0 when no
// voxel is selected; blank (NaN) for the channels outside the range and the
// channels without data (all voxels blank)
template <typename T, typename ProgressType>
void ComputeProfile(const T* in, T* out, vtkIdType numSlice, int numChannels, const ProfileSelection& selection,
                    ProgressType& progress)
{
  const T NaN = std::numeric_limits<T>::quiet_NaN();
  vtkSMPTools::For(0, numChannels, [&](vtkIdType first, vtkIdType last)
    {
    if (progress.IsCancelled())
      {
      return;
      }
    for (vtkIdType k = first; k < last; k++)
      {
      if (k < selection.Zmin || k > selection.Zmax)
        {
        out[k] = NaN;
        continue;
        }
      const T* channel = in + k * numSlice;
      const unsigned char* mask = selection.Mask ? selection.Mask + k * numSlice : nullptr;
      double sum = 0.;
      vtkIdType finite = 0;
      for (vtkIdType voxel = 0; voxel < numSlice; voxel++)
        {
        const double value = channel[voxel];
        if (std::isnan(value))
          {
          continue;
          }
        finite++;
        if ((mask && !mask[voxel]) || (selection.Footprint && !selection.Footprint[voxel]) ||
            (selection.Threshold && !(value > selection.IntensityMin && value < selection.IntensityMax)))
          {
          continue;
          }
        sum += value;
        }
      out[k] = finite > 0 ? static_cast<T>(sum * selection.UnitConversion) : NaN;
      }
    progress.Done(last - first);
    });
}


}// end namespace

//----------------------------------------------------------------------------
class vtkSlicerAstroProfilesLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
};

//----------------------------------------------------------------------------
vtkSlicerAstroProfilesLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
}

//---------------------------------------------------------------------------
vtkSlicerAstroProfilesLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroProfilesLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroProfilesLogic::vtkSlicerAstroProfilesLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroProfilesLogic::~vtkSlicerAstroProfilesLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroProfilesLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroProfilesLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroProfilesLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroProfilesLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroProfilesLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroProfilesParametersNode* pNode = vtkMRMLAstroProfilesParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroProfilesLogic::CalculateProfile(vtkMRMLAstroProfilesParametersNode *pnode)
{

  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile : "
                  "parameterNode not found.");
    return 0;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile :"
                  " scene not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile :"
                  " inputVolume not found!");
    return false;
    }

  vtkMRMLAstroVolumeNode *ProfileVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetProfileVolumeNodeID()));
  if(!ProfileVolume || !ProfileVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile :"
                  " ProfileVolume not found!");
    return false;
    }

  bool maskActive = pnode->GetMaskActive();

  // input mask: a segment, on the voxel grid of the data (1 inside)
  vtkNew<vtkImageData> maskImage;
  unsigned char* maskVoxels = nullptr;
  if (maskActive)
    {
    std::string maskError;
    const char* maskSegmentationID = pnode->GetMaskSegmentationNodeID();
    vtkMRMLSegmentationNode* maskSegmentationNode = vtkMRMLSegmentationNode::SafeDownCast(
      maskSegmentationID ? this->GetMRMLScene()->GetNodeByID(maskSegmentationID) : nullptr);
    if (!vtkSlicerAstroVolumeLogic::CreateSegmentMask(maskSegmentationNode, pnode->GetMaskSegmentID(), inputVolume,
                                                      maskImage, maskError))
      {
      vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile : " << maskError);
      return false;
      }
    maskVoxels = static_cast<unsigned char*>(maskImage->GetScalarPointer());
    }

  // Jy/beam -> Jy (pixel area / beam area); without a beam (or data not in
  // Jy/beam) the profile is the sum of the intensities
  double unitBeamConv = vtkSlicerAstroVolumeLogic::GetJyBeamToJyFactor(inputVolume);
  if (unitBeamConv <= 0.)
    {
    vtkWarningMacro("vtkSlicerAstroProfilesLogic::CalculateProfile : the data have no beam (BMAJ, BMIN) or are "
                    "not in Jy/beam: the profile is the sum of the intensities per channel.");
    unitBeamConv = 1.;
    }

  const int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numSlice = dims[0] * dims[1] * numComponents;

  float *inFPixel = nullptr;
  float *outProfileFPixel = nullptr;
  unsigned char *maskPixel = nullptr;
  double *inDPixel = nullptr;
  double *outProfileDPixel = nullptr;

  const int DataType = ProfileVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outProfileFPixel = static_cast<float*> (ProfileVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outProfileDPixel = static_cast<double*> (ProfileVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  bool cancel = false;

  std::chrono::steady_clock::time_point start, end;

  long mtime = 0;

  start = std::chrono::steady_clock::now();

  // The loops run in the background runner of the AstroVolume logic (e.g. a
  // worker thread, the GUI responsive with a progress dialog): the progress
  // and the cancel request go to a node without observers (no MRML events
  // from a worker thread); the scene is modified before and after them only.
  vtkNew<vtkMRMLAstroProfilesParametersNode> progressNode;
  bool computeResult = true;
  std::function<void()> compute = [&]()
  {
  progressNode->SetStatus(1);

  const double NaN = sqrt(-1);

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = inputVolume->GetAstroVolumeDisplayNode();
  if (!astroDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile :"
                  " astroDisplay not found!");
    computeResult = false;
    return;
    }
  double ijk[3], world[3];
  ijk[0] = StringToDouble(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) * 0.5;
  ijk[1] = StringToDouble(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) * 0.5;
  double VelFactor = 1.;

  struct wcsprm* WCS = astroDisplay->GetWCSStruct();
  if (!WCS)
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile :"
                  " WCS not found!");
    computeResult = false;
    return;
    }
  if (!strcmp(WCS->cunit[2], "m/s"))
    {
    VelFactor = 0.001;
    }

  // channels: the velocity range (threshold), all the channels with a mask
  int Zmin = 0, Zmax = dims[2] - 1;
  if (!pnode->GetMaskActive())
    {
    ijk[2] = 0.;
    astroDisplay->GetReferenceSpace(ijk, world);
    int limits[2] = {0, dims[2] - 1};
    const double velocities[2] = {pnode->GetVelocityMin() / VelFactor, pnode->GetVelocityMax() / VelFactor};
    for (int index = 0; index < 2; index++)
      {
      world[2] = velocities[index];
      double channel[3] = {0., 0., 0.};
      astroDisplay->GetIJKSpace(world, channel);
      limits[index] = std::min(std::max(static_cast<int>(std::lround(channel[2])), 0), dims[2] - 1);
      }
    Zmin = std::min(limits[0], limits[1]);
    Zmax = std::max(limits[0], limits[1]);
    }

  // aperture: the sky area of the mask (its voxels in any channel), all the
  // channels; otherwise the voxels of the mask, or the threshold
  std::vector<unsigned char> footprint;
  if (pnode->GetMaskActive() && pnode->GetMaskAperture())
    {
    maskPixel = maskVoxels;
    footprint.assign(static_cast<size_t>(numSlice), 0);
    for (vtkIdType voxel = 0; voxel < static_cast<vtkIdType>(numSlice) * dims[2]; voxel++)
      {
      if (maskPixel[voxel])
        {
        footprint[static_cast<size_t>(voxel % numSlice)] = 1;
        }
      }
    }
  ProfileSelection selection;
  selection.Mask = pnode->GetMaskActive() && footprint.empty() ? maskVoxels : nullptr;
  selection.Footprint = footprint.empty() ? nullptr : footprint.data();
  selection.Threshold = !pnode->GetMaskActive();
  selection.IntensityMin = pnode->GetIntensityMin();
  selection.IntensityMax = pnode->GetIntensityMax();
  selection.Zmin = Zmin;
  selection.Zmax = Zmax;
  selection.UnitConversion = unitBeamConv;

  {
  vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroProfilesParametersNode> progress(progressNode.GetPointer(), dims[2], 10, 100);
  switch (DataType)
    {
    case VTK_FLOAT:
      ComputeProfile(inFPixel, outProfileFPixel, numSlice, dims[2], selection, progress);
      break;
    case VTK_DOUBLE:
      ComputeProfile(inDPixel, outProfileDPixel, numSlice, dims[2], selection, progress);
      break;
    }
  cancel = progress.IsCancelled();
  }
  (void)NaN;

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Profile Kernel Time : "<<mtime<<" ms.");


  };
  progressNode->SetStatus(1);
  if (this->Internal->AstroVolumeLogic)
    {
    this->Internal->AstroVolumeLogic->RunInBackground("Calculating the profile...", compute, progressNode.GetPointer());
    }
  else
    {
    compute();
    }
  if (!computeResult)
    {
    pnode->SetStatus(100);
    return false;
    }
  if (progressNode->GetStatus() == -1)
    {
    pnode->SetStatus(-1);
    }
  cancel = cancel || progressNode->GetStatus() == -1;

  if (cancel)
    {
    pnode->SetStatus(100);
    return false;
    }

  start = std::chrono::steady_clock::now();

  int wasModifying = ProfileVolume->StartModify();
  ProfileVolume->UpdateRangeAttributes();
  ProfileVolume->UpdateDisplayThresholdAttributes();
  int disabledModify = ProfileVolume->GetAstroVolumeDisplayNode()->StartModify();
  ProfileVolume->GetAstroVolumeDisplayNode()->ResetWindowLevelPresets();
  ProfileVolume->GetAstroVolumeDisplayNode()->SetAutoWindowLevel(0);
  double min = StringToDouble(ProfileVolume->GetAttribute("SlicerAstro.DATAMIN"));
  double max = StringToDouble(ProfileVolume->GetAttribute("SlicerAstro.DATAMAX"));
  double window = max-min;
  double level = 0.5*(max+min);
  ProfileVolume->GetAstroVolumeDisplayNode()->SetWindowLevel(window, level);
  ProfileVolume->GetAstroVolumeDisplayNode()->SetThreshold(min, max);
  ProfileVolume->GetAstroVolumeDisplayNode()->EndModify(disabledModify);
  ProfileVolume->EndModify(wasModifying);

  pnode->SetStatus(100);

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Update Time : "<<mtime<<" ms.");

  return true;
}
