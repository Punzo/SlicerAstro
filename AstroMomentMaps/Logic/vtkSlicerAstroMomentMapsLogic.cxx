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
#include "vtkSlicerAstroMomentMapsLogic.h"
#include "vtkSlicerAstroConfigure.h"
#include <functional>
#include <memory>
#include <vtkSlicerAstroSMPTools.h>
#include <type_traits>
#include <vector>

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLColorTableNode.h>

// VTK includes
#include <vtkArrayData.h>
#include <vtkCacheManager.h>
#include <vtkImageData.h>
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
bool FloatIsNaN(float Value)
{
  return isNaN<float>(Value);
}

//----------------------------------------------------------------------------
// Width of a channel (velocity unit of the channels): the mean width over the
// summed channels first..last (the moment 0 is the sum of the intensities
// times the channel width)
double ChannelWidth(const std::vector<double>& channelVelocity, int first, int last)
{
  const int numberOfChannels = static_cast<int>(channelVelocity.size());
  if (numberOfChannels < 2)
    {
    return 1.;
    }
  if (last > first)
    {
    return fabs(channelVelocity[last] - channelVelocity[first]) / (last - first);
    }
  const int next = first + 1 < numberOfChannels ? first + 1 : first - 1;
  return fabs(channelVelocity[next] - channelVelocity[first]);
}

}// end namespace

//----------------------------------------------------------------------------
class vtkSlicerAstroMomentMapsLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
};

//----------------------------------------------------------------------------
vtkSlicerAstroMomentMapsLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
}

//---------------------------------------------------------------------------
vtkSlicerAstroMomentMapsLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroMomentMapsLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroMomentMapsLogic::vtkSlicerAstroMomentMapsLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroMomentMapsLogic::~vtkSlicerAstroMomentMapsLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroMomentMapsLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroMomentMapsLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroMomentMapsLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroMomentMapsLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroMomentMapsLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroMomentMapsParametersNode* pNode = vtkMRMLAstroMomentMapsParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps(vtkMRMLAstroMomentMapsParametersNode *pnode)
{
  bool result = false;
  const bool started = this->StartCalculateMomentMaps(pnode, [&result](bool success) { result = success; }, true);
  return started && result;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroMomentMapsLogic::StartCalculateMomentMaps(vtkMRMLAstroMomentMapsParametersNode *pnode,
                                                             const std::function<void(bool)>& done,
                                                             bool synchronous)
{

   if (!pnode)
     {
     vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps : "
                   "parameterNode not found.");
     return 0;
     }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " scene not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " inputVolume not found!");
    return false;
    }

  vtkMRMLAstroVolumeNode *ZeroMomentVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetZeroMomentVolumeNodeID()));
  if((!ZeroMomentVolume || !ZeroMomentVolume->GetImageData()) && pnode->GetGenerateZero())
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " ZeroMomentVolume not found!");
    return false;
    }

  vtkMRMLAstroVolumeNode *FirstMomentVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetFirstMomentVolumeNodeID()));
  if((!FirstMomentVolume || !FirstMomentVolume->GetImageData()) && pnode->GetGenerateFirst())
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " FirstMomentVolume not found!");
    return false;
    }

  vtkMRMLAstroVolumeNode *SecondMomentVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetSecondMomentVolumeNodeID()));
  if((!SecondMomentVolume || !SecondMomentVolume->GetImageData()) && pnode->GetGenerateSecond())
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " SecondMomentVolume not found!");
    return false;
    }

  bool maskActive = pnode->GetMaskActive();

  // input mask: a segment, on the voxel grid of the data (1 inside)
  vtkSmartPointer<vtkImageData> maskImage = vtkSmartPointer<vtkImageData>::New();
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
      vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps : " << maskError);
      return false;
      }
    maskVoxels = static_cast<unsigned char*>(maskImage->GetScalarPointer());
    }

  const int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numSlice = dims[0] * dims[1] * numComponents;

  float *inFPixel = nullptr;
  float *outZeroFPixel = nullptr;
  float *outFirstFPixel = nullptr;
  float *outSecondFPixel = nullptr;
  unsigned char *maskPixel = nullptr;
  double *inDPixel = nullptr;
  double *outZeroDPixel = nullptr;
  double *outFirstDPixel = nullptr;
  double *outSecondDPixel = nullptr;

  bool forceGenerateFirst = false;

  if (pnode->GetGenerateFirst() || pnode->GetGenerateSecond())
    {
    forceGenerateFirst = true ;
    }

  // The 1st moment needs the 0th, the 2nd the 0th and the 1st: maps that
  // are not requested as output are calculated in internal buffers
  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  vtkSmartPointer<vtkImageData> zeroMomentImage = (pnode->GetGenerateZero() && ZeroMomentVolume) ?
    ZeroMomentVolume->GetImageData() : nullptr;
  vtkSmartPointer<vtkImageData> firstMomentImage = (pnode->GetGenerateFirst() && FirstMomentVolume) ?
    FirstMomentVolume->GetImageData() : nullptr;
  for (vtkSmartPointer<vtkImageData>* momentImage : { &zeroMomentImage, &firstMomentImage })
    {
    if (!*momentImage)
      {
      *momentImage = vtkSmartPointer<vtkImageData>::New();
      (*momentImage)->SetDimensions(dims[0], dims[1], 1);
      (*momentImage)->AllocateScalars(DataType, 1);
      }
    }
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outZeroFPixel = static_cast<float*> (zeroMomentImage->GetScalarPointer(0,0,0));
      if (forceGenerateFirst)
        {
        outFirstFPixel = static_cast<float*> (firstMomentImage->GetScalarPointer(0,0,0));
        }
      if (pnode->GetGenerateSecond())
        {
        outSecondFPixel = static_cast<float*> (SecondMomentVolume->GetImageData()->GetScalarPointer(0,0,0));
        }
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outZeroDPixel = static_cast<double*> (zeroMomentImage->GetScalarPointer(0,0,0));
      if (forceGenerateFirst)
        {
        outFirstDPixel = static_cast<double*> (firstMomentImage->GetScalarPointer(0,0,0));
        }
      if (pnode->GetGenerateSecond())
        {
        outSecondDPixel = static_cast<double*> (SecondMomentVolume->GetImageData()->GetScalarPointer(0,0,0));
        };
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  // written by the worker, read afterwards
  std::shared_ptr<bool> cancel = std::make_shared<bool>(false);

  std::chrono::steady_clock::time_point start, end;

  long mtime = 0;

  start = std::chrono::steady_clock::now();

  // The loops run in the background runner of the AstroVolume logic (e.g. a
  // worker thread, the GUI responsive with a progress dialog): the progress
  // and the cancel request go to a node without observers (no MRML events
  // from a worker thread); the scene is modified before and after them only.
  vtkSmartPointer<vtkMRMLAstroMomentMapsParametersNode> progressNode =
    vtkSmartPointer<vtkMRMLAstroMomentMapsParametersNode>::New();
  std::shared_ptr<bool> computeResult = std::make_shared<bool>(true);
  // what the computation uses in the worker thread, prepared here in the
  // GUI thread (vtkSlicerAstroVolumeLogic::RunInBackground): the voxel
  // buffers held (shared, not copied), a copy of the WCS, the attributes and
  // a copy of the parameters
  vtkSmartPointer<vtkImageData> heldInput = inputVolume->GetImageData();
  vtkSmartPointer<vtkImageData> heldSecondMoment = SecondMomentVolume ? SecondMomentVolume->GetImageData() : nullptr;
  vtkSmartPointer<vtkMRMLAstroVolumeDisplayNode> workerDisplay =
    vtkSlicerAstroVolumeLogic::CopyDisplayNodeForWorker(inputVolume);
  const double inputNAXIS1 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.NAXIS1"));
  const double inputNAXIS2 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.NAXIS2"));
  vtkSmartPointer<vtkMRMLAstroMomentMapsParametersNode> workerParameters =
    vtkSmartPointer<vtkMRMLAstroMomentMapsParametersNode>::New();
  workerParameters->Copy(pnode);
  // everything captured by value: the job outlives this function; the
  // buffers used through raw pointers are held explicitly
  std::vector<vtkSmartPointer<vtkObject> > held = {maskImage, heldInput, heldSecondMoment, zeroMomentImage, firstMomentImage};
  std::function<void()> compute = [=, pnode = workerParameters, held = held]() mutable
  {
  progressNode->SetStatus(1);

  const double NaN = sqrt(-1);

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = workerDisplay;
  if (!astroDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " astroDisplay not found!");
    *computeResult = false;
    return;
    }
  double ijk[3], world[3];
  ijk[0] = inputNAXIS1 * 0.5;
  ijk[1] = inputNAXIS2 * 0.5;
  double VelFactor = 1.;

  struct wcsprm* WCS = astroDisplay->GetWCSStruct();
  if (!WCS)
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " WCS not found!");
    *computeResult = false;
    return;
    }
  if (!strcmp(WCS->cunit[2], "m/s"))
    {
    VelFactor = 0.001;
    }

  if(pnode->GetMaskActive())
    {
    maskPixel = maskVoxels;

    // Velocity of each channel at the reference spatial position (the same
    // for all the pixels): computed once, outside of the parallel loop.
    std::vector<double> channelVelocity(dims[2], 0.);
    {
    double ijkChannel[3] = {ijk[0], ijk[1], 0.}, worldChannel[3] = {0., 0., 0.};
    for (int kk = 0; kk < dims[2]; kk++)
      {
      ijkChannel[2] = kk;
      astroDisplay->GetReferenceSpace(ijkChannel, worldChannel);
      channelVelocity[kk] = worldChannel[2] * VelFactor;
      }
    }
    // all the channels are summed (the mask selects the voxels)
    const double dV = ChannelWidth(channelVelocity, 0, dims[2] - 1);

    {
    vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroMomentMapsParametersNode> progress(progressNode.GetPointer(), (numSlice) - (0), 10, 100);
    vtkSMPTools::For(0, numSlice, [&](vtkIdType firstIndex, vtkIdType lastIndex)
      {
      if (progress.IsCancelled())
        {
        return;
        }
      for (vtkIdType elemCnt = firstIndex; elemCnt < lastIndex; elemCnt++)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
              *(outZeroFPixel + elemCnt) = 0.;
            if (forceGenerateFirst)
              {
              *(outFirstFPixel + elemCnt) = 0.;
              }
            if (pnode->GetGenerateSecond())
              {
              *(outSecondFPixel + elemCnt) = 0.;
              }
            break;
          case VTK_DOUBLE:
              *(outZeroDPixel + elemCnt) = 0.;
            if (forceGenerateFirst)
              {
              *(outFirstDPixel + elemCnt) = 0.;
              }
            if (pnode->GetGenerateSecond())
              {
              *(outSecondDPixel + elemCnt) = 0.;
              }
            break;
          }

        double SpaceCoordinates[3];
        for (int kk = 0; kk < dims[2]; kk++)
          {
          vtkIdType posData = elemCnt + static_cast<vtkIdType>(kk) * numSlice;
          if (forceGenerateFirst)
            {
            SpaceCoordinates[2] = channelVelocity[kk];
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              if (*(maskPixel + posData) > 0.001)
                {
                if (FloatIsNaN(*(inFPixel + posData)))
                  {
                  continue;
                  }
                *(outZeroFPixel + elemCnt) += *(inFPixel + posData);
                if (forceGenerateFirst)
                  {
                  *(outFirstFPixel + elemCnt) += *(inFPixel + posData) * SpaceCoordinates[2];
                  }
                }
              break;
            case VTK_DOUBLE:
              if (*(maskPixel + posData) > 0.001)
                {
                if (DoubleIsNaN(*(inDPixel + posData)))
                  {
                  continue;
                  }
                *(outZeroDPixel + elemCnt) += *(inDPixel + posData);
                if (forceGenerateFirst)
                  {
                  *(outFirstDPixel + elemCnt) += *(inDPixel + posData) * SpaceCoordinates[2];
                  }
                }
              break;
            }
          }

        if (forceGenerateFirst)
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              if (fabs(*(outZeroFPixel + elemCnt)) < DOUBLEPRECISION ||
                  fabs(*(outFirstFPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outFirstFPixel + elemCnt) = NaN;
                }
              else
                {
                *(outFirstFPixel + elemCnt) /= *(outZeroFPixel + elemCnt);
                }
              break;
            case VTK_DOUBLE:
              if (fabs(*(outZeroDPixel + elemCnt)) < DOUBLEPRECISION ||
                  fabs(*(outFirstDPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outFirstDPixel + elemCnt) = NaN;
                }
              else
                {
                *(outFirstDPixel + elemCnt) /= *(outZeroDPixel + elemCnt);
                }
              break;
            }
          }

        if (pnode->GetGenerateSecond())
          {
          for (int kk = 0; kk < dims[2]; kk++)
            {
            vtkIdType posData = elemCnt + static_cast<vtkIdType>(kk) * numSlice;
            SpaceCoordinates[2] = channelVelocity[kk];
            switch (DataType)
              {
              case VTK_FLOAT:
                if (*(maskPixel + posData) > 0.001)
                  {
                  if (FloatIsNaN(*(inFPixel + posData)))
                    {
                    continue;
                    }
                  *(outSecondFPixel + elemCnt) += *(inFPixel + posData) * (SpaceCoordinates[2] - *(outFirstFPixel + elemCnt))
                                                                        * (SpaceCoordinates[2] - *(outFirstFPixel + elemCnt));
                  }
                break;
              case VTK_DOUBLE:
                if (*(maskPixel + posData) > 0.001)
                  {
                  if (DoubleIsNaN(*(inDPixel + posData)))
                    {
                    continue;
                    }
                  *(outSecondDPixel + elemCnt) += *(inDPixel + posData) * (SpaceCoordinates[2] - *(outFirstDPixel + elemCnt))
                                                                        * (SpaceCoordinates[2] - *(outFirstDPixel + elemCnt));
                  }
                break;
              }
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              if (fabs(*(outZeroFPixel + elemCnt)) < DOUBLEPRECISION ||
                  fabs(*(outSecondFPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outSecondFPixel + elemCnt) = NaN;
                }
              else
                {
                *(outSecondFPixel + elemCnt) = sqrt(*(outSecondFPixel + elemCnt) / *(outZeroFPixel + elemCnt));
                }
              break;
            case VTK_DOUBLE:
              if (fabs(*(outZeroDPixel + elemCnt)) < DOUBLEPRECISION ||
                  fabs(*(outSecondDPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outSecondDPixel + elemCnt) = NaN;
                }
              else
                {
                *(outSecondDPixel + elemCnt) = sqrt(*(outSecondDPixel + elemCnt) / *(outZeroDPixel + elemCnt));
                }
              break;
            }
          }

        if (pnode->GetGenerateZero())
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              if (fabs(*(outZeroFPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outZeroFPixel + elemCnt) = NaN;
                }
              else
                {
                *(outZeroFPixel + elemCnt) *= dV;
                }
              break;
            case VTK_DOUBLE:
              if (fabs(*(outZeroDPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outZeroDPixel + elemCnt) = NaN;
                }
              else
                {
                *(outZeroDPixel + elemCnt) *= dV;
                }
              break;
            }
          }
        }
      progress.Done(lastIndex - firstIndex);
      });
    *cancel = progress.IsCancelled();
    }
    }
  else
    {
    ijk[2] = 0.;
    astroDisplay->GetReferenceSpace(ijk, world);
    double VelMin = pnode->GetVelocityMin();
    VelMin /= VelFactor;
    world[2] = VelMin;
    astroDisplay->GetIJKSpace(world, ijk);
    int Zmin;
    if (ijk[2] < 0)
      {
      Zmin = 0;
      }
    else if (ijk[2] >= dims[2])
      {
      Zmin = dims[2] - 1;
      }
    else
      {
      Zmin = ijk[2];
      }

    double VelMax = pnode->GetVelocityMax();
    VelMax /= VelFactor;
    world[2] = VelMax;
    astroDisplay->GetIJKSpace(world, ijk);
    int Zmax;
    if (ijk[2] < 0)
      {
      Zmax = 0;
      }
    else if (ijk[2] >= dims[2])
      {
      Zmax = dims[2] - 1;
      }
    else
      {
      Zmax = ijk[2];
      }

    if (Zmin > Zmax)
      {
      double temp = Zmin;
      Zmin = Zmax;
      Zmax = temp;
      }

    // Velocity of each channel at the reference spatial position (the same
    // for all the pixels): computed once, outside of the parallel loop.
    std::vector<double> channelVelocity(dims[2], 0.);
    {
    double ijkChannel[3] = {ijk[0], ijk[1], 0.}, worldChannel[3] = {0., 0., 0.};
    for (int kk = 0; kk < dims[2]; kk++)
      {
      ijkChannel[2] = kk;
      astroDisplay->GetReferenceSpace(ijkChannel, worldChannel);
      channelVelocity[kk] = worldChannel[2] * VelFactor;
      }
    }
    const double dV = ChannelWidth(channelVelocity, Zmin, Zmax);

    {
    vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroMomentMapsParametersNode> progress(progressNode.GetPointer(), (numSlice) - (0), 10, 100);
    vtkSMPTools::For(0, numSlice, [&](vtkIdType firstIndex, vtkIdType lastIndex)
      {
      if (progress.IsCancelled())
        {
        return;
        }
      for (vtkIdType elemCnt = firstIndex; elemCnt < lastIndex; elemCnt++)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
              *(outZeroFPixel + elemCnt) = 0.;
            if (forceGenerateFirst)
              {
              *(outFirstFPixel + elemCnt) = 0.;
              }
            if (pnode->GetGenerateSecond())
              {
              *(outSecondFPixel + elemCnt) = 0.;
              }
            break;
          case VTK_DOUBLE:
              *(outZeroDPixel + elemCnt) = 0.;
            if (forceGenerateFirst)
              {
              *(outFirstDPixel + elemCnt) = 0.;
              }
            if (pnode->GetGenerateSecond())
              {
              *(outSecondDPixel + elemCnt) = 0.;
              }
            break;
          }

        double SpaceCoordinates[3];
        for (int kk = Zmin; kk <= Zmax; kk++)
          {
          vtkIdType posData = elemCnt + static_cast<vtkIdType>(kk) * numSlice;
          if (forceGenerateFirst)
            {
            SpaceCoordinates[2] = channelVelocity[kk];
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              if (*(inFPixel + posData) > pnode->GetIntensityMin() &&
                  *(inFPixel + posData) <= pnode->GetIntensityMax())
                {
                if (FloatIsNaN(*(inFPixel + posData)))
                  {
                  continue;
                  }
                *(outZeroFPixel + elemCnt) += *(inFPixel + posData);
                if (forceGenerateFirst)
                  {
                  *(outFirstFPixel + elemCnt) += *(inFPixel + posData) * SpaceCoordinates[2];
                  }
                }
              break;
            case VTK_DOUBLE:
              if (*(inDPixel + posData) > pnode->GetIntensityMin() &&
                  *(inDPixel + posData) <= pnode->GetIntensityMax())
                {
                if (DoubleIsNaN(*(inDPixel + posData)))
                  {
                  continue;
                  }
                *(outZeroDPixel + elemCnt) += *(inDPixel + posData);
                if (forceGenerateFirst)
                  {
                  *(outFirstDPixel + elemCnt) += *(inDPixel + posData) * SpaceCoordinates[2];
                  }
                }
              break;
            }
          }

        if (forceGenerateFirst)
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              if (fabs(*(outZeroFPixel + elemCnt)) < DOUBLEPRECISION ||
                  fabs(*(outFirstFPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outFirstFPixel + elemCnt) = NaN;
                }
              else
                {
                *(outFirstFPixel + elemCnt) /= *(outZeroFPixel + elemCnt);
                }
              break;
            case VTK_DOUBLE:
              if (fabs(*(outZeroDPixel + elemCnt)) < DOUBLEPRECISION ||
                  fabs(*(outFirstDPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outFirstDPixel + elemCnt) = NaN;
                }
              else
                {
                *(outFirstDPixel + elemCnt) /= *(outZeroDPixel + elemCnt);
                }
              break;
            }
          }

        if (pnode->GetGenerateSecond())
          {
          for (int kk = Zmin; kk <= Zmax; kk++)
            {
            vtkIdType posData = elemCnt + static_cast<vtkIdType>(kk) * numSlice;
            SpaceCoordinates[2] = channelVelocity[kk];

            switch (DataType)
              {
              case VTK_FLOAT:
                if (*(inFPixel + posData) > pnode->GetIntensityMin() &&
                    *(inFPixel + posData) <= pnode->GetIntensityMax())
                  {
                  if (FloatIsNaN(*(inFPixel + posData)))
                    {
                    continue;
                    }
                  *(outSecondFPixel + elemCnt) += *(inFPixel + posData) * (SpaceCoordinates[2] - *(outFirstFPixel + elemCnt))
                                                                        * (SpaceCoordinates[2] - *(outFirstFPixel + elemCnt));
                  }
                break;
              case VTK_DOUBLE:
                if (*(inDPixel + posData) > pnode->GetIntensityMin() &&
                    *(inDPixel + posData) <= pnode->GetIntensityMax())
                  {
                  if (DoubleIsNaN(*(inDPixel + posData)))
                    {
                    continue;
                    }
                  *(outSecondDPixel + elemCnt) += *(inDPixel + posData) * (SpaceCoordinates[2] - *(outFirstDPixel + elemCnt))
                                                                        * (SpaceCoordinates[2] - *(outFirstDPixel + elemCnt));
                  }
                break;
              }
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              if (fabs(*(outZeroFPixel + elemCnt)) < DOUBLEPRECISION ||
                  fabs(*(outSecondFPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outSecondFPixel + elemCnt) = NaN;
                }
              else
                {
                *(outSecondFPixel + elemCnt) = sqrt(*(outSecondFPixel + elemCnt) / *(outZeroFPixel + elemCnt));
                }
              break;
            case VTK_DOUBLE:
              if (fabs(*(outZeroDPixel + elemCnt)) < DOUBLEPRECISION ||
                  fabs(*(outSecondDPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outSecondDPixel + elemCnt) = NaN;
                }
              else
                {
                *(outSecondDPixel + elemCnt) = sqrt(*(outSecondDPixel + elemCnt) / *(outZeroDPixel + elemCnt));
                }
              break;
            }
          }

        if (pnode->GetGenerateZero())
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              if (fabs(*(outZeroFPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outZeroFPixel + elemCnt) = NaN;
                }
              else
                {
                *(outZeroFPixel + elemCnt) *= dV;
                }
              break;
            case VTK_DOUBLE:
              if (fabs(*(outZeroDPixel + elemCnt)) < DOUBLEPRECISION)
                {
                *(outZeroDPixel + elemCnt) = NaN;
                }
              else
                {
                *(outZeroDPixel + elemCnt) *= dV;
                }
              break;
            }
          }
        }
      progress.Done(lastIndex - firstIndex);
      });
    *cancel = progress.IsCancelled();
    }
    }

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Moment Maps Kernel Time : "<<mtime<<" ms.");


  };
  // the results go to the scene after the work, in the GUI thread (the
  // nodes are held: they may be removed meanwhile)
  vtkSmartPointer<vtkMRMLAstroMomentMapsParametersNode> parameters = pnode;
  vtkSmartPointer<vtkMRMLAstroVolumeNode> zeroHold = ZeroMomentVolume;
  vtkSmartPointer<vtkMRMLAstroVolumeNode> firstHold = FirstMomentVolume;
  vtkSmartPointer<vtkMRMLAstroVolumeNode> secondHold = SecondMomentVolume;
  std::function<void()> finish = [this, parameters, zeroHold, firstHold, secondHold, progressNode, computeResult,
                                  cancel, done]()
    {
    vtkMRMLAstroMomentMapsParametersNode* pnode = parameters;
    vtkMRMLAstroVolumeNode* ZeroMomentVolume = zeroHold;
    vtkMRMLAstroVolumeNode* FirstMomentVolume = firstHold;
    vtkMRMLAstroVolumeNode* SecondMomentVolume = secondHold;
      if (!*computeResult)
        {
        pnode->SetStatus(100);
        if (done)
          {
          done(false);
          }
        return;
        }
      if (progressNode->GetStatus() == -1)
        {
        pnode->SetStatus(-1);
        }
      *cancel = *cancel || progressNode->GetStatus() == -1;

      if (*cancel)
        {
        pnode->SetStatus(100);
        if (done)
          {
          done(false);
          }
        return;
        }

      std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

      if (pnode->GetGenerateZero())
        {
        int wasModifying = ZeroMomentVolume->StartModify();
        ZeroMomentVolume->UpdateRangeAttributes();
        ZeroMomentVolume->UpdateDisplayThresholdAttributes();
        int disabledModify = ZeroMomentVolume->GetAstroVolumeDisplayNode()->StartModify();
        ZeroMomentVolume->GetAstroVolumeDisplayNode()->ResetWindowLevelPresets();
        ZeroMomentVolume->GetAstroVolumeDisplayNode()->SetAutoWindowLevel(0);
        double min = StringToDouble(ZeroMomentVolume->GetAttribute("SlicerAstro.DATAMIN"));
        double max = StringToDouble(ZeroMomentVolume->GetAttribute("SlicerAstro.DATAMAX"));
        double window = max-min;
        double level = 0.5*(max+min);
        ZeroMomentVolume->GetAstroVolumeDisplayNode()->SetWindowLevel(window, level);
        ZeroMomentVolume->GetAstroVolumeDisplayNode()->SetThreshold(min, max);
        ZeroMomentVolume->GetAstroVolumeDisplayNode()->EndModify(disabledModify);
        ZeroMomentVolume->EndModify(wasModifying);
        }
      if (pnode->GetGenerateFirst())
        {
        int wasModifying = FirstMomentVolume->StartModify();
        FirstMomentVolume->UpdateRangeAttributes();
        FirstMomentVolume->UpdateDisplayThresholdAttributes();
        int disabledModify = FirstMomentVolume->GetAstroVolumeDisplayNode()->StartModify();
        FirstMomentVolume->GetAstroVolumeDisplayNode()->ResetWindowLevelPresets();
        FirstMomentVolume->GetAstroVolumeDisplayNode()->SetAutoWindowLevel(0);
        double min = StringToDouble(FirstMomentVolume->GetAttribute("SlicerAstro.DATAMIN"));
        double max = StringToDouble(FirstMomentVolume->GetAttribute("SlicerAstro.DATAMAX"));
        double window = max-min;
        double level = 0.5*(max+min);
        FirstMomentVolume->GetAstroVolumeDisplayNode()->SetWindowLevel(window, level);
        FirstMomentVolume->GetAstroVolumeDisplayNode()->SetThreshold(min, max);
        FirstMomentVolume->GetAstroVolumeDisplayNode()->EndModify(disabledModify);
        FirstMomentVolume->EndModify(wasModifying);
        }
      if (pnode->GetGenerateSecond())
        {
        int wasModifying = SecondMomentVolume->StartModify();
        SecondMomentVolume->UpdateRangeAttributes();
        SecondMomentVolume->UpdateDisplayThresholdAttributes();
        int disabledModify = SecondMomentVolume->GetAstroVolumeDisplayNode()->StartModify();
        SecondMomentVolume->GetAstroVolumeDisplayNode()->ResetWindowLevelPresets();
        SecondMomentVolume->GetAstroVolumeDisplayNode()->SetAutoWindowLevel(0);
        double min = StringToDouble(SecondMomentVolume->GetAttribute("SlicerAstro.DATAMIN"));
        double max = StringToDouble(SecondMomentVolume->GetAttribute("SlicerAstro.DATAMAX"));
        double window = max-min;
        double level = 0.5*(max+min);
        SecondMomentVolume->GetAstroVolumeDisplayNode()->SetWindowLevel(window, level);
        SecondMomentVolume->GetAstroVolumeDisplayNode()->SetThreshold(min, max);
        SecondMomentVolume->GetAstroVolumeDisplayNode()->EndModify(disabledModify);
        SecondMomentVolume->EndModify(wasModifying);
        }

      pnode->SetStatus(100);

      std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

        const long mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

      vtkDebugMacro("Update Time : "<<mtime<<" ms.");

    if (done)
      {
      done(true);
      }
    };
  progressNode->SetStatus(1);
  if (this->Internal->AstroVolumeLogic)
    {
    this->Internal->AstroVolumeLogic->StartInBackground("Calculating the moment maps...", compute,
      [progressNode]() { return progressNode->GetStatus(); },
      [progressNode]() { progressNode->SetStatus(-1); }, finish, synchronous);
    }
  else
    {
    compute();
    finish();
    }
  return true;
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* vtkSlicerAstroMomentMapsLogic::CreateMomentMapVolume(
  vtkMRMLAstroVolumeNode* input, const char* name, const char* unit, const char* colorTableName,
  const char* datamodel)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = this->GetAstroVolumeLogic();
  if (!scene || !astroVolumeLogic || !input || !input->GetImageData() || !input->GetAstroVolumeDisplayNode() ||
      !name)
    {
    vtkErrorMacro("CreateMomentMapVolume: input data, scene or AstroVolume logic not found");
    return nullptr;
    }
  int dims[3];
  input->GetImageData()->GetDimensions(dims);
  vtkNew<vtkImageData> imageData;
  imageData->SetDimensions(dims[0], dims[1], 1);
  imageData->SetSpacing(1., 1., 1.);
  imageData->AllocateScalars(input->GetImageData()->GetScalarType(), 1);

  vtkMRMLAstroVolumeNode* map = vtkMRMLAstroVolumeNode::SafeDownCast(
    astroVolumeLogic->CloneVolumeWithoutImageData(scene, input, name));
  if (!map || !map->GetAstroVolumeDisplayNode())
    {
    vtkErrorMacro("CreateMomentMapVolume: the map could not be created");
    return nullptr;
    }
  map->SetAttribute("SlicerAstro.NAXIS", "2");
  map->GetAstroVolumeDisplayNode()->SetAttribute("SlicerAstro.NAXIS", "2");
  map->GetAstroVolumeDisplayNode()->CopyWCS(input->GetAstroVolumeDisplayNode());
  map->SetAttribute("SlicerAstro.BUNIT", unit ? unit : "");
  const std::string btype = input->GetAstroVolumeDisplayNode()->AddVelocityInfoToDisplayStringZ(std::string());
  map->SetAttribute("SlicerAstro.BTYPE", btype.c_str());
  const char* spectralKeys[] = {"NAXIS3", "CDELT3", "CROTA3", "CRPIX3", "CRVAL3", "CTYPE3", "CUNIT3", "DTYPE3",
                                "DRVAL3", "DUNIT3", "PC1_3", "PC2_3", "PC3_1", "PC3_2", "PC3_3"};
  for (const char* key : spectralKeys)
    {
    map->RemoveAttribute((std::string("SlicerAstro.") + key).c_str());
    }
  map->SetAndObserveImageData(imageData);
  if (colorTableName && *colorTableName)
    {
    vtkMRMLColorTableNode* colorTable = vtkMRMLColorTableNode::SafeDownCast(scene->GetFirstNodeByName(colorTableName));
    if (colorTable)
      {
      map->GetAstroVolumeDisplayNode()->SetAndObserveColorNodeID(colorTable->GetID());
      }
    else
      {
      vtkErrorMacro("CreateMomentMapVolume: the color table " << colorTableName << " was not found");
      }
    }
  map->SetName(name);
  vtkMRMLNode* noPreset = nullptr;
  map->SetPresetNode(noPreset);
  // maps have no volume rendering
  for (int ii = map->GetNumberOfDisplayNodes() - 1; ii >= 0; ii--)
    {
    if (vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(map->GetNthDisplayNode(ii)))
      {
      map->RemoveNthDisplayNodeID(ii);
      }
    }
  map->SetAttribute("SlicerAstro.DATAMODEL", datamodel ? datamodel : "");
  return map;
}
