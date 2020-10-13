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

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
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
#include <sys/time.h>

// OpenMP includes
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
#include <omp.h>
#endif

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
  vtkSmartPointer<vtkFloatArray> MedianTempArray;
};

//----------------------------------------------------------------------------
vtkSlicerAstroStatisticsLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
  this->MedianTempArray = vtkSmartPointer<vtkFloatArray>::New();
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
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics : "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results that "
                  "the AstroStatistics algorithm may show poor performance.");
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

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
  int status = 0;

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, nullptr);

  pnode->SetStatus(1);

  if(segmentationActive)
    {
    maskPixel = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer(0,0,0));

    // Calculate Max, Min, NPixels, Sum
    if (pnode->GetMax() || pnode->GetMin() ||
        pnode->GetNpixels() || pnode->GetSum() ||
        pnode->GetMean() || pnode->GetStd() ||
        pnode->GetTotalFlux() || pnode->GetMedian())
      {
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp parallel for schedule(static) shared(pnode, cancel, status) reduction(max : Max), reduction(min : Min), reduction(+:Sum), reduction(+:Npixels)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      for (int elementCnt = 0; elementCnt < numElements; elementCnt++)
        {
        int stat = pnode->GetStatus();

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (stat == -1 && omp_get_thread_num() == 0)
        #else
        if (stat == -1)
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
          {
          cancel = true;
          }
        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        #pragma omp flush (cancel)
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

        if (!cancel)
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

          #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
          if (omp_get_thread_num() == 0)
            {
            if(elementCnt / (numElements / (numProcs * 33.)) > status)
              {
              status += 10;
              pnode->SetStatus(status);
              }
            }
          #else
          if(elementCnt / (numElements / 33.) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
          }
        }

      if (cancel)
        {
        inFPixel = nullptr;
        inDPixel = nullptr;
        maskPixel = nullptr;

        delete inFPixel;
        delete inDPixel;
        delete maskPixel;

        return false;
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
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp parallel for schedule(static) shared(pnode, cancel, status) reduction(+:Std)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      for (int elementCnt = 0; elementCnt < numElements; elementCnt++)
        {
        int stat = pnode->GetStatus();

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (stat == -1 && omp_get_thread_num() == 0)
        #else
        if (stat == -1)
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
          {
          cancel = true;
          }
        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        #pragma omp flush (cancel)
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

        if (!cancel)
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

          #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
          if (omp_get_thread_num() == 0)
            {
            if(33. + (elementCnt / (numElements / (numProcs * 33.))) > status)
              {
              status += 10;
              pnode->SetStatus(status);
              }
            }
          #else
          if(33. + (elementCnt / (numElements / 33.)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
          }
        }

      if (cancel)
        {
        inFPixel = nullptr;
        inDPixel = nullptr;
        maskPixel = nullptr;

        delete inFPixel;
        delete inDPixel;
        delete maskPixel;

        return false;
        }

      Std = sqrt(Std / Npixels);
      }

    // Calculate Median
    if (pnode->GetMedian())
      {
      this->Internal->MedianTempArray->Initialize();
      this->Internal->MedianTempArray->SetNumberOfValues(Npixels);

      float *TempPixel = static_cast<float*> (this->Internal->MedianTempArray->GetPointer(0));
      int TempCnt = 0;
      for (int elementCnt = 0; elementCnt < numElements; elementCnt++)
        {

        int status = pnode->GetStatus();
        if (status < 0)
          {
          cancel = true;
          break;
          }

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
            *(TempPixel + TempCnt) = *(inFPixel + elementCnt);
            break;
          case VTK_DOUBLE:
            if (DoubleIsNaN(*(inDPixel + elementCnt)))
              {
              continue;
              }
            *(TempPixel + TempCnt) = *(inDPixel + elementCnt);
            break;
          }

        TempCnt++;

        if(66. + (elementCnt / (numElements / 16.)) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        }

      std::sort(TempPixel, TempPixel + Npixels);

      pnode->SetStatus(95);

      if((Npixels) % 2 == 0)
        {
        Median = (*(TempPixel + (int) (Npixels * 0.5)) + *(TempPixel + (int) ((Npixels * 0.5) - 1))) * 0.5;
        }
      else
        {
        Median = *(TempPixel + (int) ((Npixels - 1) * 0.5));
        }

      TempPixel = nullptr;

      delete TempPixel;

      this->Internal->MedianTempArray->Initialize();
      }
    }
  else
    {
    vtkMRMLAnnotationROINode *roiNode = pnode->GetROINode();
    if(!roiNode)
      {
      vtkErrorMacro("vtkSlicerAstroStatisticsLogic::CalculateStatistics :"
                    " roiNode not found!");
      return false;
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
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp parallel for schedule(static) shared(pnode, cancel, status) reduction(max : Max), reduction(min : Min), reduction(+:Sum), reduction(+:Npixels)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      for (int elementCnt = firstElement; elementCnt < lastElement; elementCnt++)
        {
        int stat = pnode->GetStatus();

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (stat == -1 && omp_get_thread_num() == 0)
        #else
        if (stat == -1)
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
          {
          cancel = true;
          }
        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        #pragma omp flush (cancel)
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

        if (!cancel)
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

          #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
          if (omp_get_thread_num() == 0)
            {
            if((elementCnt - firstElement) / (numElements / (numProcs * 33.)) > status)
              {
              status += 10;
              pnode->SetStatus(status);
              }
            }
          #else
          if((elementCnt - firstElement) / (numElements / 33.) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
          }
        }

      if (cancel)
        {
        inFPixel = nullptr;
        inDPixel = nullptr;
        maskPixel = nullptr;

        delete inFPixel;
        delete inDPixel;
        delete maskPixel;

        return false;
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
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp parallel for schedule(static) shared(pnode, cancel, status) reduction(+:Std)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      for (int elementCnt = firstElement; elementCnt < lastElement; elementCnt++)
        {
        int stat = pnode->GetStatus();

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (stat == -1 && omp_get_thread_num() == 0)
        #else
        if (stat == -1)
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
          {
          cancel = true;
          }
        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        #pragma omp flush (cancel)
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

        if (!cancel)
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

          #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
          if (omp_get_thread_num() == 0)
            {
            if(33. + ((elementCnt - firstElement) / (numElements / (numProcs * 33.))) > status)
              {
              status += 10;
              pnode->SetStatus(status);
              }
            }
          #else
          if(33. + ((elementCnt - firstElement) / (numElements / 33.)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
          }
        }

      if (cancel)
        {
        inFPixel = nullptr;
        inDPixel = nullptr;
        maskPixel = nullptr;

        delete inFPixel;
        delete inDPixel;
        delete maskPixel;

        return false;
        }

      Std = sqrt(Std / Npixels);
      }

    // Calculate Median
    if (pnode->GetMedian())
      {
      this->Internal->MedianTempArray->Initialize();
      this->Internal->MedianTempArray->SetNumberOfValues(Npixels);

      float *TempPixel = static_cast<float*> (this->Internal->MedianTempArray->GetPointer(0));
      int TempCnt = 0;
      for (int elementCnt = firstElement; elementCnt < lastElement; elementCnt++)
        {
        int status = pnode->GetStatus();
        if (status < 0)
          {
          cancel = true;
          break;
          }

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
            *(TempPixel + TempCnt) = *(inFPixel + elementCnt);
            break;
          case VTK_DOUBLE:
            if (DoubleIsNaN(*(inDPixel + elementCnt)))
              {
              continue;
              }
            *(TempPixel + TempCnt) = *(inDPixel + elementCnt);
            break;
          }

        TempCnt++;

        if(66. + ((elementCnt - firstElement) / (numElements / 16.)) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        }

      std::sort(TempPixel, TempPixel + Npixels);

      pnode->SetStatus(95);

      if((Npixels) % 2 == 0)
        {
        Median = (*(TempPixel + (int) (Npixels * 0.5)) + *(TempPixel + (int) ((Npixels * 0.5) - 1))) * 0.5;
        }
      else
        {
        Median = *(TempPixel + (int) ((Npixels - 1) * 0.5));
        }

      TempPixel = nullptr;

      delete TempPixel;

      this->Internal->MedianTempArray->Initialize();
      }
    }

  gettimeofday(&end, nullptr);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Statistics Kernel Time : "<<mtime<<" ms.");

  inFPixel = nullptr;
  inDPixel = nullptr;
  maskPixel = nullptr;

  delete inFPixel;
  delete inDPixel;
  delete maskPixel;

  pnode->SetStatus(100);

  if (cancel)
    {
    return false;
    }

  gettimeofday(&start, nullptr);

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

  gettimeofday(&end, nullptr);;

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

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

  vtkMRMLAnnotationROINode* roiNode = parametersNode->GetROINode();

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

  vtkMRMLAnnotationROINode* roiNode = parametersNode->GetROINode();

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

  vtkMRMLAnnotationROINode* roiNode = parametersNode->GetROINode();

  vtkMRMLAstroVolumeNode *volumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(parametersNode->GetInputVolumeNodeID()));

  return this->GetAstroVolumeLogic()->IsROIAlignedWithInputVolume(roiNode, volumeNode);
}
