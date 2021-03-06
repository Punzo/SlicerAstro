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
#include "vtkSlicerAstroMaskingLogic.h"
#include "vtkSlicerAstroConfigure.h"

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
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
#include <cassert>
#include <iostream>
#include <sys/time.h>

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

}// end namespace

//----------------------------------------------------------------------------
class vtkSlicerAstroMaskingLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkFloatArray> MedianTempArray;
  vtkSmartPointer<vtkImageData> blankImageDataTemp;
};

//----------------------------------------------------------------------------
vtkSlicerAstroMaskingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = nullptr;
  this->MedianTempArray = vtkSmartPointer<vtkFloatArray>::New();
  this->blankImageDataTemp = vtkSmartPointer<vtkImageData>::New();
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

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, nullptr);

  pnode->SetStatus(1);

  if(segmentationActive)
    {
    maskPixel = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer(0,0,0));
    for (int elementCnt = 0; elementCnt < numElements; elementCnt++)
      {
      if (pnode->GetStatus() == -1)
        {
        cancel = true;
        break;
        }

      if ((regionInside && *(maskPixel + elementCnt) < 1) ||
          (!regionInside && *(maskPixel + elementCnt) > 0))
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elementCnt) = *(inFPixel + elementCnt);
            break;
          case VTK_DOUBLE:
            *(outDPixel + elementCnt) = *(inDPixel + elementCnt);
            break;
          }
        }
      else
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elementCnt) = BlankValue;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elementCnt) = BlankValue;
            break;
          }
        }

      if(elementCnt / numElements > status)
        {
        status += 10;
        pnode->SetStatus(status);
        }
      }
    }
  else
    {
    vtkMRMLAnnotationROINode *roiNode = pnode->GetROINode();
    if(!roiNode)
      {
      vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyBlank :"
                    " roiNode not found!");
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

      return false;
      }

    double roiBounds[6];
    this->GetAstroVolumeLogic()->CalculateROICropVolumeBounds(roiNode, inputVolume, roiBounds);

    int firstElement = (roiBounds[0] + roiBounds[2] * dims[0] +
                       roiBounds[4] * numSlice);

    int lastElement = (roiBounds[1] + roiBounds[3] * dims[0] +
                      roiBounds[5] * numSlice) + 1;

    if (firstElement == 0 && lastElement == numElements &&
        (BlankString.find("NaN") != std::string::npos ||
         !strcmp(BlankString.c_str(), "0") ||
         !strcmp(BlankString.c_str(), "0.")) &&
        regionInside)
      {
      vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyBlank :"
                    " can not fill the entire datacube with NaN!");
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

      return false;
      }

    for (int elementCnt = 0; elementCnt < numElements; elementCnt++)
      {
      if (pnode->GetStatus() == -1)
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
      bool eleOutside = (elementCnt < firstElement || elementCnt >= lastElement ||
                        x < roiBounds[0] ||  x > roiBounds[1] ||
                        y < roiBounds[2] ||  y > roiBounds[3]);
      if ((regionInside && !eleOutside) ||
          (!regionInside && eleOutside))
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elementCnt) = BlankValue;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elementCnt) = BlankValue;
            break;
          }
        }
      else
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elementCnt) = *(inFPixel + elementCnt);
            break;
          case VTK_DOUBLE:
            *(outDPixel + elementCnt) = *(inDPixel + elementCnt);
            break;
          }
        }

      if(elementCnt / numElements > status)
        {
        status += 10;
        pnode->SetStatus(status);
        }
      }
    }

  gettimeofday(&end, nullptr);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

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

  gettimeofday(&start, nullptr);

  int wasModifying = outputVolume->StartModify();
  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();
  outputVolume->EndModify(wasModifying);

  pnode->SetStatus(100);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

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
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numSlice = dims[0] * dims[1] * numComponents;
  int numElements = dims[0] * dims[1] * dims[2] * numComponents;

  // Crop outputVolume by voxels bounds
  int firstElement = 0, lastElement = 0;
  double cropBounds[6] = {0.};

  float *inFPixel = nullptr;
  float *outFPixel = nullptr;
  double *inDPixel = nullptr;
  double *outDPixel = nullptr;

  const int DataType = inputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();

  vtkNew<vtkImageData> imageDataTemp;

  if (!segmentationActive)
    {
    vtkMRMLAnnotationROINode *roiNode = pnode->GetROINode();
    if(!roiNode)
      {
      vtkErrorMacro("vtkSlicerAstroMaskingLogic::ApplyCrop :"
                    " roiNode not found!");
      return false;
      }

    this->GetAstroVolumeLogic()->CalculateROICropVolumeBounds(roiNode, inputVolume, cropBounds);

    firstElement = (cropBounds[0] + cropBounds[2] * dims[0] +
                   cropBounds[4] * numSlice) + 1;

    lastElement = (cropBounds[1] + cropBounds[3] * dims[0] +
                  cropBounds[5] * numSlice) + 1;

    numElements = lastElement - firstElement;

    int N1 = (int) (cropBounds[1] - cropBounds[0]) + 1;
    int N2 = (int) (cropBounds[3] - cropBounds[2]) + 1;
    int N3 = (int) (cropBounds[5] - cropBounds[4]) + 1;

    imageDataTemp->SetDimensions(N1, N2, N3);
    imageDataTemp->SetSpacing(1.,1.,1.);
    imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);
    outputVolume->SetAndObserveImageData(imageDataTemp.GetPointer());
    outputVolume->SetAttribute("SlicerAstro.NAXIS1", IntToString(N1).c_str());
    outputVolume->SetAttribute("SlicerAstro.NAXIS2", IntToString(N2).c_str());
    outputVolume->SetAttribute("SlicerAstro.NAXIS3", IntToString(N3).c_str());

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
    }
  else
    {
    pnode->SetBlankValue("NaN");
    pnode->SetBlankRegion("Outside");
    this->ApplyBlank(pnode);
    this->Internal->blankImageDataTemp->DeepCopy(outputVolume->GetImageData());

    this->GetAstroVolumeLogic()->CalculateSegmentCropVolumeBounds(segmentationNode,
                                                                  segment,
                                                                  inputVolume,
                                                                  cropBounds);

    firstElement = (cropBounds[0] + cropBounds[2] * dims[0] +
                   cropBounds[4] * numSlice) + 1;

    lastElement = (cropBounds[1] + cropBounds[3] * dims[0] +
                  cropBounds[5] * numSlice) + 1;

    numElements = lastElement - firstElement;

    int N1 = (int) (cropBounds[1] - cropBounds[0]) + 1;
    int N2 = (int) (cropBounds[3] - cropBounds[2]) + 1;
    int N3 = (int) (cropBounds[5] - cropBounds[4]) + 1;

    imageDataTemp->SetDimensions(N1, N2, N3);
    imageDataTemp->SetSpacing(1.,1.,1.);
    imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);
    outputVolume->SetAndObserveImageData(imageDataTemp.GetPointer());
    outputVolume->SetAttribute("SlicerAstro.NAXIS1", IntToString(N1).c_str());
    outputVolume->SetAttribute("SlicerAstro.NAXIS2", IntToString(N2).c_str());
    outputVolume->SetAttribute("SlicerAstro.NAXIS3", IntToString(N3).c_str());

    switch (DataType)
      {
      case VTK_FLOAT:
        inFPixel = static_cast<float*> (this->Internal->blankImageDataTemp->GetScalarPointer(0,0,0));
        outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        inDPixel = static_cast<double*> (this->Internal->blankImageDataTemp->GetScalarPointer(0,0,0));
        outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      default:
        vtkErrorMacro("Attempt to allocate scalars of type not allowed");
        return 0;
      }
    }

  bool cancel = false;
  int status = 0;

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, nullptr);

  pnode->SetStatus(1);

  int outElementCnt = 0;

  for (int elementCnt = firstElement; elementCnt < lastElement; elementCnt++)
    {
    if (pnode->GetStatus() == -1)
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

    if (x < cropBounds[0] || x > cropBounds[1] ||
        y < cropBounds[2] || y > cropBounds[3])
      {
      continue;
      }

    outElementCnt++;
    switch (DataType)
      {
      case VTK_FLOAT:
        *(outFPixel + outElementCnt) = *(inFPixel + elementCnt);
        break;
      case VTK_DOUBLE:
        *(outDPixel + outElementCnt) = *(inDPixel + elementCnt);
        break;
      }

    if((elementCnt - firstElement) / numElements > status)
      {
      status += 10;
      pnode->SetStatus(status);
      }
    }

  gettimeofday(&end, nullptr);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Masking Kernel Time : "<<mtime<<" ms.");

  inFPixel = nullptr;
  inDPixel = nullptr;

  delete inFPixel;
  delete inDPixel;

  this->Internal->blankImageDataTemp->Initialize();

  if (cancel)
    {
    outFPixel = nullptr;
    outDPixel = nullptr;
    delete outFPixel;
    delete outDPixel;

    pnode->SetStatus(100);
    return false;
    }

  gettimeofday(&start, nullptr);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();

  // center the volume
  this->Internal->AstroVolumeLogic->CenterVolume(outputVolume);

  // Calculate the new crpix values
  int cPixX = StringToInt(outputVolume->GetAttribute("SlicerAstro.CRPIX1"));
  int cPixY = StringToInt(outputVolume->GetAttribute("SlicerAstro.CRPIX2"));
  int cPixZ = StringToInt(outputVolume->GetAttribute("SlicerAstro.CRPIX3"));

  // calculate the center with respect to the cutout cube
  int cPixXCut = cPixX - (int) cropBounds[0];
  int cPixYCut = cPixY - (int) cropBounds[2];
  int cPixZCut = cPixZ - (int) cropBounds[4];

  // update header keywords:
  outputVolume->SetAttribute("SlicerAstro.CRPIX1", IntToString(cPixXCut).c_str());
  outputVolume->SetAttribute("SlicerAstro.CRPIX2", IntToString(cPixYCut).c_str());
  outputVolume->SetAttribute("SlicerAstro.CRPIX3", IntToString(cPixZCut).c_str());

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
                  "wcsset ERROR "<<wcsStatus<<":\n"<<
                  "Message from "<<wcs->err->function<<
                  "at line "<<wcs->err->line_no<<
                  " of file "<<wcs->err->file<<
                  ": \n"<<wcs->err->msg<<"\n");
    }

  pnode->SetStatus(100);

  gettimeofday(&end, nullptr);;

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

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

  vtkMRMLAnnotationROINode* roiNode = parametersNode->GetROINode();

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

  vtkMRMLAnnotationROINode* roiNode = parametersNode->GetROINode();

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

  vtkMRMLAnnotationROINode* roiNode = parametersNode->GetROINode();

  vtkMRMLAstroVolumeNode *volumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(parametersNode->GetInputVolumeNodeID()));

  return this->GetAstroVolumeLogic()->IsROIAlignedWithInputVolume(roiNode, volumeNode);
}
