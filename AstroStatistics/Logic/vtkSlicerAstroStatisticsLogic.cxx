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
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroStatisticsParametersNode.h>
#include <vtkMRMLTableNode.h>

// VTK includes
#include <vtkArrayData.h>
#include <vtkCacheManager.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkImageData.h>
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

// OpenMP includes

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
    vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                  " inputVolume not found!");
    return false;
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
    vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                  " maskVolume not found!");
    return false;
    }

  vtkMRMLTableNode* tableNode = pnode->GetTableNode();
  if(!tableNode || !tableNode->GetTable())
    {
    vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                  " tableNode not found!");
    return false;
    }

  vtkStringArray* SelectionArray = vtkStringArray::SafeDownCast
    (tableNode->GetTable()->GetColumnByName("Selection"));
  vtkIntArray* NpixelsArray = vtkIntArray::SafeDownCast
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

  double BMAJ = StringToDouble(inputVolume->GetAttribute("SlicerAstro.BMAJ"));
  double BMIN = StringToDouble(inputVolume->GetAttribute("SlicerAstro.BMIN"));
  double CDELT1 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.CDELT1"));
  double CDELT2 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.CDELT2"));

  double unitBeamConv = 1.;
  if (pnode->GetTotalFlux())
    {
    if (!strcmp(inputVolume->GetAttribute("SlicerAstro.BMAJ"), "UNDEFINED") ||
        !strcmp(inputVolume->GetAttribute("SlicerAstro.BMIN"), "UNDEFINED") ||
        !strcmp(inputVolume->GetAttribute("SlicerAstro.CDELT1"), "UNDEFINED") ||
        !strcmp(inputVolume->GetAttribute("SlicerAstro.CDELT2"), "UNDEFINED"))
      {
      vtkWarningMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                      " Beam or CDELT information are not available."
                      " The total flux can not be calculated!");
      pnode->SetTotalFlux(false);
      }
    else
      {
      unitBeamConv = fabs((CDELT1 * CDELT2) / (1.13 * BMAJ * BMIN));
      }
    }

  const int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numSlice = dims[0] * dims[1] * numComponents;
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;

  float *inFPixel = nullptr;
  double *inDPixel = nullptr;
  short *maskPixel = nullptr;
  double Max = inputVolume->GetImageData()->GetScalarTypeMin(), Min = inputVolume->GetImageData()->GetScalarTypeMax();
  double Mean = 0., Median = 0., Std = 0., Sum = 0., TotalFlux = 0.;
  int Npixels = 0;

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
  std::function<void()> compute = [&]()
  {
  progressNode->SetStatus(1);

  if(segmentationActive)
    {
    maskPixel = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer(0,0,0));

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
      vtkSMPThreadLocal<int> NpixelsLocal(0);
      vtkSMPTools::For(0, numElements, [&](vtkIdType firstIndex, vtkIdType lastIndex)
        {
        if (progress.IsCancelled())
          {
          return;
          }
        double& Max = MaxLocal.Local();
        double& Min = MinLocal.Local();
        double& Sum = SumLocal.Local();
        int& Npixels = NpixelsLocal.Local();
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
      for (int value : NpixelsLocal) { Npixels += value; }
      cancel = progress.IsCancelled();
      }

      if (cancel)
        {
        inFPixel = nullptr;
        inDPixel = nullptr;
        maskPixel = nullptr;

        delete inFPixel;
        delete inDPixel;
        delete maskPixel;

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
      TotalFlux = Sum * unitBeamConv;
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
        inFPixel = nullptr;
        inDPixel = nullptr;
        maskPixel = nullptr;

        delete inFPixel;
        delete inDPixel;
        delete maskPixel;

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
    vtkMRMLMarkupsROINode *roiNode = pnode->GetROINode();
    if(!roiNode)
      {
      vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                    " roiNode not found!");
      computeResult = false;
      return;
      }

    double roiBounds[6];
    this->GetAstroVolumeLogic()->CalculateROICropVolumeBounds(roiNode, inputVolume, roiBounds);

    int firstElement = (roiBounds[0] + roiBounds[2] * dims[0] +
                       roiBounds[4] * numSlice);

    int lastElement = (roiBounds[1] + roiBounds[3] * dims[0] +
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
      vtkSMPThreadLocal<int> NpixelsLocal(0);
      vtkSMPTools::For(firstElement, lastElement, [&](vtkIdType firstIndex, vtkIdType lastIndex)
        {
        if (progress.IsCancelled())
          {
          return;
          }
        double& Max = MaxLocal.Local();
        double& Min = MinLocal.Local();
        double& Sum = SumLocal.Local();
        int& Npixels = NpixelsLocal.Local();
        for (vtkIdType elementCnt = firstIndex; elementCnt < lastIndex; elementCnt++)
          {
          int ref  = (int) floor(elementCnt / dims[0]);
          ref *= dims[0];
          int x = elementCnt - ref;
          ref = (int) floor(elementCnt / numSlice);
          ref *= numSlice;
          ref = elementCnt - ref;
          int y = (int) floor(ref / dims[0]);
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
      for (int value : NpixelsLocal) { Npixels += value; }
      cancel = progress.IsCancelled();
      }

      if (cancel)
        {
        inFPixel = nullptr;
        inDPixel = nullptr;
        maskPixel = nullptr;

        delete inFPixel;
        delete inDPixel;
        delete maskPixel;

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
      TotalFlux = Sum * unitBeamConv;
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
          int ref  = (int) floor(elementCnt / dims[0]);
          ref *= dims[0];
          int x = elementCnt - ref;
          ref = (int) floor(elementCnt / numSlice);
          ref *= numSlice;
          ref = elementCnt - ref;
          int y = (int) floor(ref / dims[0]);
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
        inFPixel = nullptr;
        inDPixel = nullptr;
        maskPixel = nullptr;

        delete inFPixel;
        delete inDPixel;
        delete maskPixel;

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

  inFPixel = nullptr;
  inDPixel = nullptr;
  maskPixel = nullptr;

  delete inFPixel;
  delete inDPixel;
  delete maskPixel;

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
  int serial = pnode->GetOutputSerial() - 1;
  tableNode->AddEmptyRow();
  std::string CellText(inputVolume->GetName());
  CellText += "_selection_";
  CellText += IntToString(serial);
  tableNode->SetCellText(serial, 0, CellText.c_str());

  if (!pnode->GetNpixels())
    {
    NpixelsArray->SetValue(serial, NaN);
    }
  else
    {
    NpixelsArray->SetValue(serial, Npixels);
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
