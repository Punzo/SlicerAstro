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
#include "vtkSlicerAstroStatisticsLogic.h"
#include "vtkSlicerAstroConfigure.h"
#include <functional>
#include <vtkSlicerAstroSMPTools.h>
#include <type_traits>

// MRML includes
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLAstroStatisticsParametersNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkSegment.h>
#include <vtkSegmentation.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkArrayData.h>
#include <vtkCacheManager.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTable.h>
#include <vtkVersion.h>

// Std includes
#include <algorithm>
#include <cassert>
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
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
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

}// end namespace

//----------------------------------------------------------------------------
class vtkSlicerAstroStatisticsLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
};

//----------------------------------------------------------------------------
vtkSlicerAstroStatisticsLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
}

//---------------------------------------------------------------------------
vtkSlicerAstroStatisticsLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroStatisticsLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroStatisticsLogic::vtkSlicerAstroStatisticsLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroStatisticsLogic::~vtkSlicerAstroStatisticsLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroStatisticsLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroStatisticsLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroStatisticsLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroStatisticsLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroStatisticsLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroStatisticsParametersNode* pNode = vtkMRMLAstroStatisticsParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroStatisticsLogic::CalculateStatistics(vtkMRMLAstroStatisticsParametersNode *pnode)
{
  this->LastError.clear();

  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics : "
                  "parameterNode not found.");
    return false;
    }

  if (!this->GetMRMLScene())
    {
    vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                  " scene not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    this->LastError = "Select the data.";
    vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                  " inputVolume not found!");
    return false;
    }

  const bool segmentationActive = pnode->GetMode() && !strcmp(pnode->GetMode(), "Segmentation");
  const bool wholeVolume = pnode->GetMode() && !strcmp(pnode->GetMode(), "Volume");
  if (!segmentationActive && !wholeVolume && !pnode->GetROINode())
    {
    this->LastError = "No box (ROI).";
    vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics : roiNode not found!");
    return false;
    }

  // input mask: a segment, on the voxel grid of the data (1 inside)
  vtkNew<vtkImageData> maskImage;
  unsigned char* maskVoxels = nullptr;
  if (segmentationActive)
    {
    std::string maskError;
    const char* maskSegmentationID = pnode->GetMaskSegmentationNodeID();
    vtkMRMLSegmentationNode* maskSegmentationNode = vtkMRMLSegmentationNode::SafeDownCast(
      maskSegmentationID ? this->GetMRMLScene()->GetNodeByID(maskSegmentationID) : nullptr);
    if (!vtkSlicerAstroVolumeLogic::CreateSegmentMask(maskSegmentationNode, pnode->GetMaskSegmentID(), inputVolume,
                                                      maskImage, maskError))
      {
      this->LastError = maskError;
      vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics : " << maskError);
      return false;
      }
    maskVoxels = static_cast<unsigned char*>(maskImage->GetScalarPointer());
    }

  vtkMRMLTableNode* tableNode = this->EnsureStatisticsTable(pnode);
  if(!tableNode || !tableNode->GetTable())
    {
    vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                  " tableNode not found!");
    return false;
    }

  vtkStringArray* SelectionArray = vtkStringArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("Selection"));
  // double in new tables, int in tables of older scenes
  vtkDataArray* NpixelsArray = vtkDataArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("Npixels"));
  vtkDoubleArray* MinArray = vtkDoubleArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("Min"));
  vtkDoubleArray* MaxArray = vtkDoubleArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("Max"));
  vtkDoubleArray* MeanArray = vtkDoubleArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("Mean"));
  vtkDoubleArray* StdArray = vtkDoubleArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("Std"));
  vtkDoubleArray* MedianArray = vtkDoubleArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("Median"));
  vtkDoubleArray* SumArray = vtkDoubleArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("Sum"));
  vtkDoubleArray* TotalFluxArray = vtkDoubleArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("TotalFlux"));

  if (!SelectionArray || !NpixelsArray || !MinArray ||
      !MaxArray || !MeanArray || !StdArray ||
      !MedianArray || !SumArray || !TotalFluxArray)
    {
    vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                  " arrays not found!");
    return false;
    }

  // Jy/beam -> Jy (pixel area / beam area); 0: no total flux (no beam, or
  // data not in Jy/beam), the total flux is blank (the setting is kept)
  const double unitBeamConv = vtkSlicerAstroVolumeLogic::GetJyBeamToJyFactor(inputVolume);
  if (pnode->GetTotalFlux() && unitBeamConv <= 0.)
    {
    this->LastError = "The data have no beam or are not in Jy/beam: no total flux.";
    vtkWarningMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics : the data have no beam (BMAJ, BMIN) "
                    "or are not in Jy/beam: the total flux can not be calculated.");
    }

  const int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  // 64-bit counts: cubes may exceed 2^31 voxels
  const vtkIdType numSlice = static_cast<vtkIdType>(dims[0]) * dims[1] * numComponents;
  vtkIdType numElements = numSlice * dims[2];

  float *inFPixel = nullptr;
  double *inDPixel = nullptr;
  unsigned char *maskPixel = nullptr;
  double Max = inputVolume->GetImageData()->GetScalarTypeMin(), Min = inputVolume->GetImageData()->GetScalarTypeMax();
  double Mean = 0., Median = 0., Std = 0., Sum = 0., TotalFlux = 0.;
  vtkIdType Npixels = 0;

  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return false;
    }

  bool cancel = false;

  std::chrono::steady_clock::time_point start, end;

  long mtime = 0;

  start = std::chrono::steady_clock::now();

  // The loops run in the background runner of the AstroVolume logic (e.g. a
  // worker thread, the GUI responsive with a progress dialog): the progress
  // and the cancel request go to a node without observers (no MRML events
  // from a worker thread); the scene is modified before and after them only.
  vtkNew<vtkMRMLAstroStatisticsParametersNode> progressNode;
  bool computeResult = true;
  // what the computation uses in the worker thread, prepared here in the
  // GUI thread (vtkSlicerAstroVolumeLogic::RunInBackground): the voxels held
  // (shared, not copied), the bounds of the box (from its node and the
  // geometry of the volume) and a copy of the parameters
  vtkSmartPointer<vtkImageData> heldInput = inputVolume->GetImageData();
  double boxBounds[6] = {0., 0., 0., 0., 0., 0.};
  bool boxBoundsValid = false;
  if (!segmentationActive && !wholeVolume && pnode->GetROINode())
    {
    const int* boxDims = inputVolume->GetImageData()->GetDimensions();
    boxBounds[1] = boxDims[0] - 1.;
    boxBounds[3] = boxDims[1] - 1.;
    boxBounds[5] = boxDims[2] - 1.;
    this->GetAstroVolumeLogic()->CalculateROICropVolumeBounds(pnode->GetROINode(), inputVolume, boxBounds);
    boxBoundsValid = true;
    }
  vtkNew<vtkMRMLAstroStatisticsParametersNode> workerParameters;
  workerParameters->Copy(pnode);
  std::function<void()> compute = [&, pnode = workerParameters.GetPointer()]()
  {
  progressNode->SetStatus(1);

  if(segmentationActive)
    {
    maskPixel = maskVoxels;

    // Calculate Max, Min, NPixels, Sum
    if (pnode->GetMax() || pnode->GetMin() ||
        pnode->GetNpixels() || pnode->GetSum() ||
        pnode->GetMean() || pnode->GetStd() ||
        pnode->GetTotalFlux() || pnode->GetMedian())
      {
      {
      vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroStatisticsParametersNode> progress(progressNode.GetPointer(), (numElements) - (0), 10, 100);
      vtkSMPThreadLocal<double> MaxLocal(Max);
      vtkSMPThreadLocal<double> MinLocal(Min);
      vtkSMPThreadLocal<double> SumLocal(0);
      vtkSMPThreadLocal<vtkIdType> NpixelsLocal(0);
      vtkSMPTools::For(0, numElements, [&](vtkIdType firstIndex, vtkIdType lastIndex)
        {
        if (progress.IsCancelled())
          {
          return;
          }
        double& Max = MaxLocal.Local();
        double& Min = MinLocal.Local();
        double& Sum = SumLocal.Local();
        vtkIdType& Npixels = NpixelsLocal.Local();
        for (vtkIdType elementCnt = firstIndex; elementCnt < lastIndex; elementCnt++)
          {
          if (*(maskPixel + elementCnt) < 1)
            {
            continue;
            }

          switch (DataType)
            {
            case VTK_FLOAT:
              if (FloatIsNaN(*(inFPixel + elementCnt)))
                {
                continue;
                }
              if (*(inFPixel + elementCnt) > Max)
                {
                Max = *(inFPixel + elementCnt);
                }
              if (*(inFPixel + elementCnt) < Min)
                {
                Min = *(inFPixel + elementCnt);
                }
              Sum += *(inFPixel + elementCnt);
              break;
            case VTK_DOUBLE:
              if (DoubleIsNaN(*(inDPixel + elementCnt)))
                {
                continue;
                }
              if (*(inDPixel + elementCnt) > Max)
                {
                Max = *(inDPixel + elementCnt);
                }
              if (*(inDPixel + elementCnt) < Min)
                {
                Min = *(inDPixel + elementCnt);
                }
              Sum += *(inDPixel + elementCnt);
              break;
            }

          Npixels += 1;
          }
        progress.Done(lastIndex - firstIndex);
        });
      for (double value : MaxLocal) { Max = std::max(Max, value); }
      for (double value : MinLocal) { Min = std::min(Min, value); }
      for (double value : SumLocal) { Sum += value; }
      for (vtkIdType value : NpixelsLocal) { Npixels += value; }
      cancel = progress.IsCancelled();
      }

      if (cancel)
        {

        computeResult = false;

        return;
        }
      }

    // Calculate Mean
    if (pnode->GetMean())
      {
      Mean = Sum / Npixels;
      }

    // Calculate TotalFlux
    if (pnode->GetTotalFlux())
      {
      TotalFlux = unitBeamConv > 0. ? Sum * unitBeamConv : sqrt(-1.);
      }

    // Calculate Std
    if (pnode->GetStd())
      {
      {
      vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroStatisticsParametersNode> progress(progressNode.GetPointer(), (numElements) - (0), 10, 100);
      vtkSMPThreadLocal<double> StdLocal(0);
      vtkSMPTools::For(0, numElements, [&](vtkIdType firstIndex, vtkIdType lastIndex)
        {
        if (progress.IsCancelled())
          {
          return;
          }
        double& Std = StdLocal.Local();
        for (vtkIdType elementCnt = firstIndex; elementCnt < lastIndex; elementCnt++)
          {
          if (*(maskPixel + elementCnt) < 1)
            {
            continue;
            }

          switch (DataType)
            {
            case VTK_FLOAT:
              if (FloatIsNaN(*(inFPixel + elementCnt)))
                {
                continue;
                }
              Std += (*(inFPixel + elementCnt) - Mean) * (*(inFPixel + elementCnt) - Mean);
              break;
            case VTK_DOUBLE:
              if (DoubleIsNaN(*(inDPixel + elementCnt)))
                {
                continue;
                }
              Std += (*(inDPixel + elementCnt) - Mean) * (*(inDPixel + elementCnt) - Mean);
              break;
            }
          }
        progress.Done(lastIndex - firstIndex);
        });
      for (double value : StdLocal) { Std += value; }
      cancel = progress.IsCancelled();
      }

      if (cancel)
        {

        computeResult = false;

        return;
        }

      Std = sqrt(Std / Npixels);
      }

    // Calculate Median
    if (pnode->GetMedian())
      {
      // Copy of the selected voxels (required by the median), freed at the end
      std::vector<float> selectedValues;
      selectedValues.reserve(static_cast<size_t>(Npixels));
      for (vtkIdType elementCnt = 0; elementCnt < numElements; elementCnt++)
        {
        if ((elementCnt & 0xFFFFF) == 0 && progressNode->GetStatus() < 0)
          {
          cancel = true;
          break;
          }
        if (*(maskPixel + elementCnt) < 1)
          {
          continue;
          }
        const double value = (DataType == VTK_FLOAT) ? static_cast<double>(*(inFPixel + elementCnt))
                                                     : *(inDPixel + elementCnt);
        if (vtkSlicerAstroSMPTools::IsNaN(value))
          {
          continue;
          }
        selectedValues.push_back(static_cast<float>(value));
        }

      progressNode->SetStatus(90);
      Median = vtkSlicerAstroSMPTools::Median(selectedValues);
      progressNode->SetStatus(95);
      }
    }
  else
    {
    // the voxels of the box, or all the voxels
    double roiBounds[6] = {0., dims[0] - 1., 0., dims[1] - 1., 0., dims[2] - 1.};
    if (!wholeVolume)
      {
      if (!boxBoundsValid)
        {
        vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                      " roiNode not found!");
        computeResult = false;
        return;
        }
      std::copy(boxBounds, boxBounds + 6, roiBounds);
      }

    const vtkIdType firstElement = static_cast<vtkIdType>(roiBounds[0] + roiBounds[2] * dims[0] +
                                                          roiBounds[4] * numSlice);

    const vtkIdType lastElement = static_cast<vtkIdType>(roiBounds[1] + roiBounds[3] * dims[0] +
                                                         roiBounds[5] * numSlice) + 1;

    numElements = lastElement - firstElement;

    // Calculate Max, Min, NPixels, Sum
    if (pnode->GetMax() || pnode->GetMin() ||
        pnode->GetNpixels() || pnode->GetSum() ||
        pnode->GetMean() || pnode->GetStd() ||
        pnode->GetTotalFlux() || pnode->GetMedian())
      {
      {
      vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroStatisticsParametersNode> progress(progressNode.GetPointer(), (lastElement) - (firstElement), 10, 100);
      vtkSMPThreadLocal<double> MaxLocal(Max);
      vtkSMPThreadLocal<double> MinLocal(Min);
      vtkSMPThreadLocal<double> SumLocal(0);
      vtkSMPThreadLocal<vtkIdType> NpixelsLocal(0);
      vtkSMPTools::For(firstElement, lastElement, [&](vtkIdType firstIndex, vtkIdType lastIndex)
        {
        if (progress.IsCancelled())
          {
          return;
          }
        double& Max = MaxLocal.Local();
        double& Min = MinLocal.Local();
        double& Sum = SumLocal.Local();
        vtkIdType& Npixels = NpixelsLocal.Local();
        for (vtkIdType elementCnt = firstIndex; elementCnt < lastIndex; elementCnt++)
          {
          // position in the plane (64-bit indices)
          const vtkIdType x = elementCnt % dims[0];
          const vtkIdType y = (elementCnt % numSlice) / dims[0];
          if (x < roiBounds[0] || x > roiBounds[1] ||
              y < roiBounds[2] || y > roiBounds[3])
            {
            continue;
            }

          switch (DataType)
            {
            case VTK_FLOAT:
              if (FloatIsNaN(*(inFPixel + elementCnt)))
                {
                continue;
                }
              if (*(inFPixel + elementCnt) > Max)
                {
                Max = *(inFPixel + elementCnt);
                }
              if (*(inFPixel + elementCnt) < Min)
                {
                Min = *(inFPixel + elementCnt);
                }
              Sum += *(inFPixel + elementCnt);
              break;
            case VTK_DOUBLE:
              if (DoubleIsNaN(*(inDPixel + elementCnt)))
                {
                continue;
                }
              if (*(inDPixel + elementCnt) > Max)
                {
                Max = *(inDPixel + elementCnt);
                }
              if (*(inDPixel + elementCnt) < Min)
                {
                Min = *(inDPixel + elementCnt);
                }
              Sum += *(inDPixel + elementCnt);
              break;
            }

          Npixels += 1;
          }
        progress.Done(lastIndex - firstIndex);
        });
      for (double value : MaxLocal) { Max = std::max(Max, value); }
      for (double value : MinLocal) { Min = std::min(Min, value); }
      for (double value : SumLocal) { Sum += value; }
      for (vtkIdType value : NpixelsLocal) { Npixels += value; }
      cancel = progress.IsCancelled();
      }

      if (cancel)
        {

        computeResult = false;

        return;
        }
      }

    // Calculate Mean
    if (pnode->GetMean())
      {
      Mean = Sum / Npixels;
      }

    // Calculate TotalFlux
    if (pnode->GetTotalFlux())
      {
      TotalFlux = unitBeamConv > 0. ? Sum * unitBeamConv : sqrt(-1.);
      }

    // Calculate Std
    if (pnode->GetStd())
      {
      {
      vtkSlicerAstroSMPTools::ProgressReporter<vtkMRMLAstroStatisticsParametersNode> progress(progressNode.GetPointer(), (lastElement) - (firstElement), 10, 100);
      vtkSMPThreadLocal<double> StdLocal(0);
      vtkSMPTools::For(firstElement, lastElement, [&](vtkIdType firstIndex, vtkIdType lastIndex)
        {
        if (progress.IsCancelled())
          {
          return;
          }
        double& Std = StdLocal.Local();
        for (vtkIdType elementCnt = firstIndex; elementCnt < lastIndex; elementCnt++)
          {
          // position in the plane (64-bit indices)
          const vtkIdType x = elementCnt % dims[0];
          const vtkIdType y = (elementCnt % numSlice) / dims[0];
          if (x < roiBounds[0] || x > roiBounds[1] ||
              y < roiBounds[2] || y > roiBounds[3])
            {
            continue;
            }

          switch (DataType)
            {
            case VTK_FLOAT:
              if (FloatIsNaN(*(inFPixel + elementCnt)))
                {
                continue;
                }
              Std += (*(inFPixel + elementCnt) - Mean) * (*(inFPixel + elementCnt) - Mean);
              break;
            case VTK_DOUBLE:
              if (DoubleIsNaN(*(inDPixel + elementCnt)))
                {
                continue;
                }
              Std += (*(inDPixel + elementCnt) - Mean) * (*(inDPixel + elementCnt) - Mean);
              break;
            }
          }
        progress.Done(lastIndex - firstIndex);
        });
      for (double value : StdLocal) { Std += value; }
      cancel = progress.IsCancelled();
      }

      if (cancel)
        {

        computeResult = false;

        return;
        }

      Std = sqrt(Std / Npixels);
      }

    // Calculate Median
    if (pnode->GetMedian())
      {
      // Copy of the selected voxels (required by the median), freed at the end
      std::vector<float> selectedValues;
      selectedValues.reserve(static_cast<size_t>(Npixels));
      for (vtkIdType elementCnt = firstElement; elementCnt < lastElement; elementCnt++)
        {
        if ((elementCnt & 0xFFFFF) == 0 && progressNode->GetStatus() < 0)
          {
          cancel = true;
          break;
          }
        const vtkIdType x = elementCnt % dims[0];
        const vtkIdType y = (elementCnt % numSlice) / dims[0];
        if (x < roiBounds[0] || x > roiBounds[1] ||
            y < roiBounds[2] || y > roiBounds[3])
          {
          continue;
          }
        const double value = (DataType == VTK_FLOAT) ? static_cast<double>(*(inFPixel + elementCnt))
                                                     : *(inDPixel + elementCnt);
        if (vtkSlicerAstroSMPTools::IsNaN(value))
          {
          continue;
          }
        selectedValues.push_back(static_cast<float>(value));
        }

      progressNode->SetStatus(90);
      Median = vtkSlicerAstroSMPTools::Median(selectedValues);
      progressNode->SetStatus(95);
      }
    }

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Statistics Kernel Time : "<<mtime<<" ms.");

  };
  progressNode->SetStatus(1);
  if (this->Internal->AstroVolumeLogic)
    {
    this->Internal->AstroVolumeLogic->RunInBackground("Calculating the statistics...", compute, progressNode.GetPointer());
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

  pnode->SetStatus(100);

  if (cancel)
    {
    return false;
    }

  start = std::chrono::steady_clock::now();

  double NaN = sqrt(-1);
  // a row per calculation: the data and the region
  tableNode->AddEmptyRow();
  int serial = tableNode->GetNumberOfRows() - 1;
  std::string CellText(inputVolume->GetName() ? inputVolume->GetName() : "");
  CellText += " \xC2\xB7 ";
  if (segmentationActive)
    {
    vtkMRMLSegmentationNode* maskNode = vtkMRMLSegmentationNode::SafeDownCast(
      this->GetMRMLScene()->GetNodeByID(pnode->GetMaskSegmentationNodeID()));
    vtkSegment* segment = (maskNode && maskNode->GetSegmentation() && pnode->GetMaskSegmentID()) ?
      maskNode->GetSegmentation()->GetSegment(pnode->GetMaskSegmentID()) : nullptr;
    CellText += segment && segment->GetName() ? segment->GetName() : "mask";
    }
  else if (wholeVolume)
    {
    CellText += "whole data";
    }
  else
    {
    CellText += pnode->GetROINode() && pnode->GetROINode()->GetName() ? pnode->GetROINode()->GetName() : "box";
    }
  tableNode->SetCellText(serial, 0, CellText.c_str());
  // units of the data
  const char* unit = inputVolume->GetAttribute("SlicerAstro.BUNIT");
  for (const char* column : {"Min", "Max", "Mean", "Std", "Median", "Sum"})
    {
    tableNode->SetColumnUnitLabel(column, unit ? unit : "");
    }

  if (!pnode->GetNpixels())
    {
    NpixelsArray->SetTuple1(serial, vtkDoubleArray::SafeDownCast(NpixelsArray) ? NaN : 0.);
    }
  else
    {
    NpixelsArray->SetTuple1(serial, static_cast<double>(Npixels));
    }

  if (!pnode->GetMin())
    {
    Min = NaN;
    }
  MinArray->SetValue(serial, Min);

  if (!pnode->GetMax())
    {
    Max = NaN;
    }
  MaxArray->SetValue(serial, Max);

  if (!pnode->GetMean())
    {
    Mean = NaN;
    }
  MeanArray->SetValue(serial, Mean);

  if (!pnode->GetStd())
    {
    Std = NaN;
    }
  StdArray->SetValue(serial, Std);

  if (!pnode->GetMedian())
    {
    Median = NaN;
    }
  MedianArray->SetValue(serial, Median);

  if (!pnode->GetSum())
    {
    Sum = NaN;
    }
  SumArray->SetValue(serial, Sum);

  if (!pnode->GetTotalFlux())
    {
    TotalFlux = NaN;
    }
  TotalFluxArray->SetValue(serial, TotalFlux);

  serial++;
  pnode->SetOutputSerial(serial + 1);

  end = std::chrono::steady_clock::now();

    mtime = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  vtkDebugMacro("Update Time : "<<mtime<<" ms.");

  return true;
}

//----------------------------------------------------------------------------
vtkMRMLTableNode* vtkSlicerAstroStatisticsLogic::EnsureStatisticsTable(vtkMRMLAstroStatisticsParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!pnode || !scene)
    {
    return nullptr;
    }
  vtkMRMLTableNode* tableNode = pnode->GetTableNode();
  if (tableNode && tableNode->GetTable() && tableNode->GetTable()->GetColumnByName("TotalFlux"))
    {
    return tableNode;
    }
  tableNode = vtkMRMLTableNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLTableNode",
    scene->GenerateUniqueName("StatisticsTable")));
  if (!tableNode)
    {
    return nullptr;
    }
  const int wasModifying = tableNode->StartModify();
  tableNode->RemoveAllColumns();
  tableNode->SetUseColumnTitleAsColumnHeader(true);
  vtkNew<vtkStringArray> selection;
  selection->SetName("Selection");
  tableNode->AddColumn(selection);
  tableNode->SetColumnTitle("Selection", "Selection");
  // double: exact counts up to 2^53 voxels (cubes larger than 2^31 voxels)
  // and NaN when a quantity is not computed
  const char* columns[8][3] = {{"Npixels", "#", "Number of pixels"}, {"Min", "", "Minimum"},
                               {"Max", "", "Maximum"}, {"Mean", "", "Mean"}, {"Std", "", "Standard deviation"},
                               {"Median", "", "Median"}, {"Sum", "", "Sum"}, {"TotalFlux", "Jy", "Total Flux"}};
  for (int i = 0; i < 8; i++)
    {
    vtkNew<vtkDoubleArray> column;
    column->SetName(columns[i][0]);
    tableNode->AddColumn(column);
    tableNode->SetColumnUnitLabel(columns[i][0], columns[i][1]);
    tableNode->SetColumnTitle(columns[i][0], columns[i][2]);
    }
  tableNode->EndModify(wasModifying);
  pnode->SetTableNode(tableNode);
  pnode->SetOutputSerial(1);
  return tableNode;
}

//----------------------------------------------------------------------------
vtkMRMLMarkupsROINode* vtkSlicerAstroStatisticsLogic::EnsureStatisticsROI(vtkMRMLAstroStatisticsParametersNode* pnode)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!pnode || !scene)
    {
    return nullptr;
    }
  vtkMRMLMarkupsROINode* roiNode = pnode->GetROINode();
  if (roiNode)
    {
    return roiNode;
    }
  roiNode = vtkMRMLMarkupsROINode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLMarkupsROINode",
    scene->GenerateUniqueName("StatisticsROI")));
  if (!roiNode)
    {
    return nullptr;
    }
  roiNode->CreateDefaultDisplayNodes();
  pnode->SetROINode(roiNode);
  this->FitROIToInputVolume(pnode);
  return roiNode;
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroStatisticsLogic::FitROIToInputVolume(vtkMRMLAstroStatisticsParametersNode *parametersNode)
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
void vtkSlicerAstroStatisticsLogic::SnapROIToVoxelGrid(vtkMRMLAstroStatisticsParametersNode *parametersNode)
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
bool vtkSlicerAstroStatisticsLogic::IsROIAlignedWithInputVolume(vtkMRMLAstroStatisticsParametersNode *parametersNode)
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
