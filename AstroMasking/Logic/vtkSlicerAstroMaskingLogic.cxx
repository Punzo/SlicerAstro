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
#include <vtkAstroWCSErrors.h>
#include "vtkSlicerAstroMaskingLogic.h"
#include "vtkSlicerAstroConfigure.h"
#include <vtkSlicerAstroSMPTools.h>
#include <vtkSMPTools.h>
#include <functional>

// MRML includes
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroMaskingParametersNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLTableNode.h>

// VTK includes
#include <vtkArrayData.h>
#include <vtkCacheManager.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSegmentation.h>
#include <vtkSegment.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTable.h>
#include <vtkVersion.h>

// Std includes
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <iostream>
#include <chrono>

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
  std::stringstream strstream;
  strstream << std::setprecision(std::numeric_limits<double>::digits10) << Value;
  return strstream.str();
}

}// end namespace

//----------------------------------------------------------------------------
class vtkSlicerAstroMaskingLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkFloatArray> MedianTempArray;
};

//----------------------------------------------------------------------------
vtkSlicerAstroMaskingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
  this->MedianTempArray = vtkSmartPointer<vtkFloatArray>::New();
}

//---------------------------------------------------------------------------
vtkSlicerAstroMaskingLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroMaskingLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroMaskingLogic::vtkSlicerAstroMaskingLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroMaskingLogic::~vtkSlicerAstroMaskingLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroMaskingLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroMaskingLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroMaskingLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroMaskingLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroMaskingLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroMaskingParametersNode* pNode = vtkMRMLAstroMaskingParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroMaskingLogic::ApplyBlank(vtkMRMLAstroMaskingParametersNode *pnode)
{
  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyBlank : "
                  "parameterNode not found.");
    return false;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyBlank :"
                  " scene not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyBlank :"
                  " inputVolume not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyBlank : "
                  "outputVolume not found.");
    return 0;
    }

  vtkMRMLAstroLabelMapVolumeNode *maskVolume =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetMaskVolumeNodeID()));

  bool segmentationActive = false;
  if (!(strcmp(pnode->GetMode(), "Segmentation")))
    {
    segmentationActive = true;
    }

  if((!maskVolume || !maskVolume->GetImageData()) && segmentationActive)
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyBlank :"
                  " maskVolume not found.");
    return false;
    }

  bool regionInside = false;
  if (!(strcmp(pnode->GetBlankRegion(), "Inside")))
    {
    regionInside = true;
    }

  std::string BlankString = pnode->GetBlankValue();
  double BlankValue = 0.;
  if (BlankString.find("NaN") != std::string::npos)
    {
    BlankValue = sqrt(-1);
    }
  else
    {
    BlankValue = StringToDouble(BlankString.c_str());
    }

  const int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numSlice = dims[0] * dims[1] * numComponents;
  const int numElements = dims[0] * dims[1] * dims[2] * numComponents;

  float *inFPixel = nullptr;
  float *outFPixel = nullptr;
  double *inDPixel = nullptr;
  double *outDPixel = nullptr;
  short *maskPixel = nullptr;

  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  bool cancel = false;
  int status = 0;

  std::chrono::steady_clock::time_point start, end;

  long mtime = 0;

  start = std::chrono::steady_clock::now();

  // Region to blank (ROI bounds: from the scene, before the loop)
  double roiBounds[6] = {0., -1., 0., -1., 0., -1.};
  vtkIdType firstElement = 0, lastElement = numElements;
  if (segmentationActive)
    {
    maskPixel = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer(0,0,0));
    }
  else
    {
    vtkMRMLMarkupsROINode *roiNode = pnode->GetROINode();
    if(!roiNode)
      {
      vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyBlank :"
                    " roiNode not found!");
      return false;
      }
    this->GetAstroVolumeLogic()->CalculateROICropVolumeBounds(roiNode, inputVolume, roiBounds);
    firstElement = static_cast<vtkIdType>(roiBounds[0] + roiBounds[2] * dims[0] + roiBounds[4] * numSlice);
    lastElement = static_cast<vtkIdType>(roiBounds[1] + roiBounds[3] * dims[0] + roiBounds[5] * numSlice) + 1;
    if (firstElement == 0 && lastElement == numElements &&
        (BlankString.find("NaN") != std::string::npos ||
         !strcmp(BlankString.c_str(), "0") ||
         !strcmp(BlankString.c_str(), "0.")) &&
        regionInside)
      {
      vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyBlank :"
                    " can not fill the entire datacube with NaN!");
      return false;
      }
    }

  // Blanking: parallel loop (vtkSMPTools) in the background runner of the
  // AstroVolume logic (e.g. a worker thread, the GUI responsive with a
  // progress dialog); progress and cancel through a node without observers
  vtkNew<vtkMRMLAstroMaskingParametersNode> progressNode;
  progressNode->SetStatus(1);
  pnode->SetStatus(1);
  std::function<void()> compute = [&]()
  {
  vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroMaskingParametersNode> progress(
    progressNode.GetPointer(), numElements, 1, 99);
  vtkSMPTools::For(0, numElements, [&](vtkIdType firstIndex, vtkIdType lastIndex)
    {
    if (progress.IsCancelled())
      {
      return;
      }
    for (vtkIdType elementCnt = firstIndex; elementCnt < lastIndex; elementCnt++)
      {
      bool blank = false;
      if (segmentationActive)
        {
        const bool inMask = maskPixel[elementCnt] > 0;
        blank = regionInside ? inMask : !inMask;
        }
      else
        {
        const vtkIdType x = elementCnt % dims[0];
        const vtkIdType y = (elementCnt % numSlice) / dims[0];
        const bool eleOutside = (elementCnt < firstElement || elementCnt >= lastElement ||
                                 x < roiBounds[0] || x > roiBounds[1] ||
                                 y < roiBounds[2] || y > roiBounds[3]);
        blank = regionInside ? !eleOutside : eleOutside;
        }
      if (DataType == VTK_FLOAT)
        {
        outFPixel[elementCnt] = blank ? static_cast<float>(BlankValue) : inFPixel[elementCnt];
        }
      else
        {
        outDPixel[elementCnt] = blank ? BlankValue : inDPixel[elementCnt];
        }
      }
    progress.Done(lastIndex - firstIndex);
    });
  cancel = progress.IsCancelled();
  };
  if (this->GetAstroVolumeLogic())
    {
    this->GetAstroVolumeLogic()->RunInBackground("Blanking the data...", compute, progressNode.GetPointer());
    }
  else
    {
    compute();
    }
  cancel = cancel || progressNode->GetStatus() == -1;
  status = 100;

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Masking Kernel Time : "<<mtime<<" ms.");

  inFPixel = nullptr;
  outFPixel = nullptr;
  inDPixel = nullptr;
  outFPixel = nullptr;
  maskPixel = nullptr;

  delete inFPixel;
  delete outFPixel;
  delete inDPixel;
  delete outDPixel;
  delete maskPixel;

  if (cancel)
    {
    pnode->SetStatus(100);
    return false;
    }

  start = std::chrono::steady_clock::now();

  int wasModifying = outputVolume->StartModify();
  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();
  outputVolume->EndModify(wasModifying);

  pnode->SetStatus(100);

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Update Time : "<<mtime<<" ms.");

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroMaskingLogic::ApplyCrop(vtkMRMLAstroMaskingParametersNode *pnode,
                                           vtkMRMLSegmentationNode *segmentationNode,
                                           vtkSegment *segment)
{
  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop : "
                  "parameterNode not found.");
    return false;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop :"
                  " scene not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop :"
                  " inputVolume not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume || !outputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop : "
                  "outputVolume not found.");
    return 0;
    }

  bool segmentationActive = false;
  if (!(strcmp(pnode->GetMode(), "Segmentation")))
    {
    segmentationActive = true;
    }

  if((!segment || !segmentationNode) && segmentationActive)
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop :"
                  " segment not found.");
    return false;
    }

  const int *dims = inputVolume->GetImageData()->GetDimensions();
  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  if (DataType != VTK_FLOAT && DataType != VTK_DOUBLE)
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop : only float and double data can be cropped.");
    return false;
    }

  // Bounds of the crop (voxel indices) of the ROI or of the segment
  double cropBounds[6] = {0., -1., 0., -1., 0., -1.};
  vtkSmartPointer<vtkImageData> sourceImageData;
  bool boundsFound = false;
  if (!segmentationActive)
    {
    vtkMRMLMarkupsROINode *roiNode = pnode->GetROINode();
    if(!roiNode)
      {
      vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop :"
                    " roiNode not found!");
      return false;
      }
    boundsFound = this->GetAstroVolumeLogic()->CalculateROICropVolumeBounds(roiNode, inputVolume, cropBounds);
    sourceImageData = inputVolume->GetImageData();
    }
  else
    {
    boundsFound = this->GetAstroVolumeLogic()->CalculateSegmentCropVolumeBounds(segmentationNode,
                                                                                segment,
                                                                                inputVolume,
                                                                                cropBounds);
    if (boundsFound)
      {
      // blank outside the segment, then crop the blanked (full size) cube
      pnode->SetBlankValue("NaN");
      pnode->SetBlankRegion("Outside");
      this->ApplyBlank(pnode);
      sourceImageData = outputVolume->GetImageData();
      }
    }

  // integer voxel bounds inside the cube
  int bounds[6] = {0, -1, 0, -1, 0, -1};
  for (int axis = 0; axis < 3; axis++)
    {
    bounds[2 * axis] = std::max(0, static_cast<int>(std::ceil(cropBounds[2 * axis] - 1.E-6)));
    bounds[2 * axis + 1] = std::min(dims[axis] - 1, static_cast<int>(std::floor(cropBounds[2 * axis + 1] + 1.E-6)));
    }
  if (!boundsFound || !sourceImageData || bounds[0] > bounds[1] || bounds[2] > bounds[3] || bounds[4] > bounds[5])
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop : the "
                  << (segmentationActive ? "segment is empty or does not overlap" : "ROI does not overlap")
                  << " the data cube: nothing to crop.");
    pnode->SetStatus(100);
    return false;
    }

  const int N1 = bounds[1] - bounds[0] + 1;
  const int N2 = bounds[3] - bounds[2] + 1;
  const int N3 = bounds[5] - bounds[4] + 1;

  vtkNew<vtkImageData> imageDataTemp;
  imageDataTemp->SetDimensions(N1, N2, N3);
  imageDataTemp->SetSpacing(1.,1.,1.);
  imageDataTemp->AllocateScalars(DataType, 1);

  bool cancel = false;

  std::chrono::steady_clock::time_point start, end;

  long mtime = 0;

  start = std::chrono::steady_clock::now();

  pnode->SetStatus(1);

  // copy the voxels inside the bounds (one row at a time)
  const vtkIdType inRow = dims[0];
  const vtkIdType inSlice = static_cast<vtkIdType>(dims[0]) * dims[1];
  const size_t elementSize = (DataType == VTK_FLOAT) ? sizeof(float) : sizeof(double);
  const char* inPixel = static_cast<const char*>(sourceImageData->GetScalarPointer());
  char* outPixel = static_cast<char*>(imageDataTemp->GetScalarPointer());
  // parallel over the channels (vtkSMPTools), in the background runner of
  // the AstroVolume logic (progress and cancel through a node without
  // observers); the output volume is set afterwards, in this thread
  vtkNew<vtkMRMLAstroMaskingParametersNode> progressNode;
  progressNode->SetStatus(1);
  std::function<void()> compute = [&]()
  {
  vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroMaskingParametersNode> progress(
    progressNode.GetPointer(), N3, 1, 99);
  vtkSMPTools::For(0, N3, [&](vtkIdType firstChannel, vtkIdType lastChannel)
    {
    for (vtkIdType channel = firstChannel; channel < lastChannel; channel++)
      {
      if (progress.IsCancelled())
        {
        return;
        }
      const vtkIdType k = bounds[4] + channel;
      vtkIdType outIndex = channel * static_cast<vtkIdType>(N1) * N2;
      for (int jj = bounds[2]; jj <= bounds[3]; jj++)
        {
        const vtkIdType inIndex = k * inSlice + jj * inRow + bounds[0];
        std::memcpy(outPixel + outIndex * elementSize, inPixel + inIndex * elementSize, N1 * elementSize);
        outIndex += N1;
        }
      }
    progress.Done(lastChannel - firstChannel);
    });
  cancel = progress.IsCancelled();
  };
  if (this->GetAstroVolumeLogic())
    {
    this->GetAstroVolumeLogic()->RunInBackground("Cropping the data...", compute, progressNode.GetPointer());
    }
  else
    {
    compute();
    }
  cancel = cancel || progressNode->GetStatus() == -1;
  sourceImageData = nullptr;

  if (!cancel)
    {
    outputVolume->SetAndObserveImageData(imageDataTemp.GetPointer());
    outputVolume->SetAttribute("SlicerAstro.NAXIS1", IntToString(N1).c_str());
    outputVolume->SetAttribute("SlicerAstro.NAXIS2", IntToString(N2).c_str());
    outputVolume->SetAttribute("SlicerAstro.NAXIS3", IntToString(N3).c_str());
    }
  for (int axis = 0; axis < 6; axis++)
    {
    cropBounds[axis] = bounds[axis];
    }

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Masking Kernel Time : "<<mtime<<" ms.");

  if (cancel)
    {
    pnode->SetStatus(100);
    return false;
    }

  start = std::chrono::steady_clock::now();

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();

  // center the volume
  this->Internal->AstroVolumeLogic->CenterVolume(outputVolume);

  // reference pixels with respect to the cutout cube (CRPIX can be fractional)
  const double cPixXCut = StringToDouble(outputVolume->GetAttribute("SlicerAstro.CRPIX1")) - cropBounds[0];
  const double cPixYCut = StringToDouble(outputVolume->GetAttribute("SlicerAstro.CRPIX2")) - cropBounds[2];
  const double cPixZCut = StringToDouble(outputVolume->GetAttribute("SlicerAstro.CRPIX3")) - cropBounds[4];

  // update header keywords:
  outputVolume->SetAttribute("SlicerAstro.CRPIX1", DoubleToString(cPixXCut).c_str());
  outputVolume->SetAttribute("SlicerAstro.CRPIX2", DoubleToString(cPixYCut).c_str());
  outputVolume->SetAttribute("SlicerAstro.CRPIX3", DoubleToString(cPixZCut).c_str());

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = outputVolume->GetAstroVolumeDisplayNode();
  if (!astroDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop :"
                  " astroDisplay not found!");
    pnode->SetStatus(100);
    return false;
    }

  wcsprm* wcs = astroDisplay->GetWCSStruct();
  if (!wcs)
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop :"
                  " wcs not found!");
    pnode->SetStatus(100);
    return false;
    }

  wcs->crpix[0] = cPixXCut;
  wcs->crpix[1] = cPixYCut;
  wcs->crpix[2] = cPixZCut;

  int wcsStatus;
  if ((wcsStatus = wcsset(wcs)))
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop :"
                  << vtkAstroWCSErrorMessage("wcsset", wcsStatus, wcs));
    }

  pnode->SetStatus(100);

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Update Time : "<<mtime<<" ms.");

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroMaskingLogic::ApplyMask(vtkMRMLAstroMaskingParametersNode *pnode,
                                           vtkMRMLSegmentationNode *segmentationNode,
                                           vtkSegment *segment)
{
  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyMask : "
                  "parameterNode not found.");
    return false;
    }

  if (!(strcmp(pnode->GetOperation(), "Blank")))
    {
    this->ApplyBlank(pnode);
    }
  else if (!(strcmp(pnode->GetOperation(), "Crop")))
    {
    this->ApplyCrop(pnode, segmentationNode, segment);
    }
  else
    {
    vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyMask : "
                  "Operation Type not found.");
    return false;
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroMaskingLogic::FitROIToInputVolume(vtkMRMLAstroMaskingParametersNode *parametersNode)
{
  if (!parametersNode || !this->GetAstroVolumeLogic())
    {
    return false;
    }

  vtkMRMLMarkupsROINode* roiNode = parametersNode->GetROINode();

  vtkMRMLAstroVolumeNode *volumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(parametersNode->GetInputVolumeNodeID()));

  return this->GetAstroVolumeLogic()->FitROIToInputVolume(roiNode, volumeNode);
}

//----------------------------------------------------------------------------
void vtkSlicerAstroMaskingLogic::SnapROIToVoxelGrid(vtkMRMLAstroMaskingParametersNode *parametersNode)
{
  if (!parametersNode || !this->GetAstroVolumeLogic())
    {
    return;
    }

  vtkMRMLMarkupsROINode* roiNode = parametersNode->GetROINode();

  vtkMRMLAstroVolumeNode *volumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(parametersNode->GetInputVolumeNodeID()));

  this->GetAstroVolumeLogic()->SnapROIToVoxelGrid(roiNode, volumeNode);
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroMaskingLogic::IsROIAlignedWithInputVolume(vtkMRMLAstroMaskingParametersNode *parametersNode)
{
  if (!parametersNode || !this->GetAstroVolumeLogic())
    {
    return false;
    }

  vtkMRMLMarkupsROINode* roiNode = parametersNode->GetROINode();

  vtkMRMLAstroVolumeNode *volumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(parametersNode->GetInputVolumeNodeID()));

  return this->GetAstroVolumeLogic()->IsROIAlignedWithInputVolume(roiNode, volumeNode);
}
