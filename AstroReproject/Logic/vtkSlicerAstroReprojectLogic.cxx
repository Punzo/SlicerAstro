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
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroReprojectLogic.h>
#include <vtkSlicerAstroConfigure.h>

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroReprojectParametersNode.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkGeneralTransform.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkVersion.h>

// STD includes
#include <cassert>
#include <iostream>

// OpenMP includes
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
#include <omp.h>
#endif

// Qt includes
#include <QtDebug>

// WCS includes
#include "wcslib.h"

#include <iostream>
#include <sys/time.h>

#define UNUSED(expr) (void)(expr)

//----------------------------------------------------------------------------
class vtkSlicerAstroReprojectLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkImageData> tempVolumeData;
};

//----------------------------------------------------------------------------
vtkSlicerAstroReprojectLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = 0;
  this->tempVolumeData = vtkSmartPointer<vtkImageData>::New();
}

//---------------------------------------------------------------------------
vtkSlicerAstroReprojectLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroReprojectLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroReprojectLogic::vtkSlicerAstroReprojectLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroReprojectLogic::~vtkSlicerAstroReprojectLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroReprojectLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroReprojectLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroReprojectLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroReprojectLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroReprojectLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroReprojectParametersNode* pNode = vtkMRMLAstroReprojectParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
bool vtkSlicerAstroReprojectLogic::Reproject(vtkMRMLAstroReprojectParametersNode* pnode)
{
  if (!this->GetMRMLScene())
    {
    return false;
    }

  if (!pnode)
    {
    vtkErrorMacro("vtkSlicerAstroReprojectLogic::Reproject : "
                  "parameterNode not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume || !inputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroReprojectLogic::Reproject : "
                  "inputVolume not found.");
    return false;
    }

  vtkMRMLAstroVolumeDisplayNode *inputVolumeDisplay =
    inputVolume->GetAstroVolumeDisplayNode();
  if (!inputVolumeDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroReprojectLogic::Reproject : "
                  "inputVolumeDisplay not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *referenceVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetReferenceVolumeNodeID()));
  if (!referenceVolume || !referenceVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroReprojectLogic::Reproject : "
                  "referenceVolume not found.");
    return false;
    }

  vtkMRMLAstroVolumeDisplayNode *referenceVolumeDisplay =
    referenceVolume->GetAstroVolumeDisplayNode();
  if (!referenceVolumeDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroReprojectLogic::Reproject : "
                  "referenceVolumeDisplay not found.");
    return false;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume || !outputVolume->GetImageData())
    {
    vtkErrorMacro("vtkSlicerAstroReprojectLogic::Reproject : "
                  "outputVolume not found.");
    return false;
    }

  const int *inputDims = inputVolume->GetImageData()->GetDimensions();
  const int inputSliceDim = inputDims[0] * inputDims[1];
  const int *referenceDims = referenceVolume->GetImageData()->GetDimensions();
  const int referenceSliceDim = referenceDims[0] * referenceDims[1];
  const int inputNumComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int referenceNumComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (inputNumComponents > 1 || referenceNumComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroReprojectLogic::Reproject : "
                  "imageData with more than one components.");
    return false;
    }

  // Create empty data of spatial dimensions equal to the reference data
  // and of velocity dimension equal to the input volume
  // (the reprojection takes place only for the spatial axis)
  vtkNew<vtkImageData> imageDataTemp;
  imageDataTemp->SetDimensions(referenceDims[0], referenceDims[1], inputDims[2]);
  imageDataTemp->SetSpacing(1.,1.,1.);
  imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

  // copy data into the Astro Volume object
  outputVolume->SetAndObserveImageData(imageDataTemp.GetPointer());

  float *inFPixel = NULL;
  float *outFPixel = NULL;
  double *inDPixel = NULL;
  double *outDPixel = NULL;
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
      return false;
    }

  bool cancel = false;
  int status = 0;
  const double NaN = sqrt(-1);

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

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  // Calculate the 2D interpolation grid
  double ***referenceGrid=NULL;
  referenceGrid = new double**[referenceDims[0]];
  for (int ii = 0; ii < referenceDims[0]; ii++)
    {
    referenceGrid[ii] = new double*[referenceDims[1]];
    for (int jj = 0; jj < referenceDims[1]; jj++)
      {
      referenceGrid[ii][jj] = new double[2];
      }
    }

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  #pragma omp parallel for schedule(dynamic) shared(referenceGrid)
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
  for (int ii = 0; ii < referenceDims[0]; ii++)
    {
    for (int jj = 0; jj < referenceDims[1]; jj++)
      {
      double ijk[3] = {0.}, world[3] = {0.};
      ijk[0] = ii;
      ijk[1] = jj;
      referenceVolumeDisplay->GetReferenceSpace(ijk, world);
      inputVolumeDisplay->GetIJKSpace(world, ijk);
      referenceGrid[ii][jj][0] = ijk[0];
      referenceGrid[ii][jj][1] = ijk[1];
      }
    }

  pnode->SetStatus(10);

  // Interpolate
  int numElements = referenceDims[0] * referenceDims[1] * inputDims[2];

  if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::NearestNeighbour)
    {
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outFPixel, outDPixel, cancel, status)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
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
        int ref  = (int) floor(elemCnt / referenceDims[0]);
        ref *= referenceDims[0];
        int ii = elemCnt - ref;
        ref = (int) floor(elemCnt / referenceSliceDim);
        int kk = ref;
        ref *= referenceSliceDim;
        ref = elemCnt - ref;
        int jj = (int) floor(ref / referenceDims[0]);

        double x = referenceGrid[ii][jj][0];
        int x1 = round(x);
        bool x1Inside = x1 > 0 && x1 < inputDims[0];

        double y = referenceGrid[ii][jj][1];
        int y1 = round(y);
        bool y1Inside = y1 > 0 && y1 < inputDims[1];

        if (!x1Inside || !y1Inside)
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) = NaN;
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) = NaN;
              break;
            }
          continue;
          }

        switch (DataType)
          {
         case VTK_FLOAT:
           *(outFPixel + elemCnt) = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
           break;
         case VTK_DOUBLE:
           *(outDPixel + elemCnt) = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
           break;
          }

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (omp_get_thread_num() == 0)
          {
          if(elemCnt / (numElements / (numProcs * 100)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          }
        #else
        if(elemCnt / (numElements / 100) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        }
      }
    }
  else if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::Bilinear)
    {
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outFPixel, outDPixel, cancel, status)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
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
        int ref  = (int) floor(elemCnt / referenceDims[0]);
        ref *= referenceDims[0];
        int ii = elemCnt - ref;
        ref = (int) floor(elemCnt / referenceSliceDim);
        int kk = ref;
        ref *= referenceSliceDim;
        ref = elemCnt - ref;
        int jj = (int) floor(ref / referenceDims[0]);

        double x = referenceGrid[ii][jj][0];
        int x1 = floor(x);
        bool x1Inside = x1 > 0 && x1 < inputDims[0];
        int x2 = ceil(x);
        bool x2Inside = x2 > 0 && x2 < inputDims[0];

        double y = referenceGrid[ii][jj][1];
        int y1 = floor(y);
        bool y1Inside = y1 > 0 && y1 < inputDims[1];
        int y2 = ceil(y);
        bool y2Inside = y2 > 0 && y2 < inputDims[1];

        if ((!x1Inside && !x2Inside) || (!y1Inside && !y2Inside))
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) = NaN;
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) = NaN;
              break;
            }
          continue;
          }

        double F11 = 0., F12 = 0., F21 = 0., F22 = 0.;

        if (!x1Inside || !y1Inside)
          {
          F11 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F11 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
              break;
            case VTK_DOUBLE:
              F11 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
              break;
            }
          }

        if (!x2Inside || !y1Inside)
          {
          F21 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F21 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x2);
              break;
            case VTK_DOUBLE:
              F21 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x2);
              break;
            }
          }

        if (!x1Inside || !y2Inside)
          {
          F12 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F12 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x1);
              break;
            case VTK_DOUBLE:
              F12 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x1);
              break;
            }
          }

        if (!x2Inside || !y2Inside)
          {
          F22 = NaN;
          }
        else
          {
          switch (DataType)
            {
           case VTK_FLOAT:
             F22 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x2);
             break;
           case VTK_DOUBLE:
             F22 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x2);
             break;
            }
          }

        double deltax = 1. / (x2 - x1);
        double x2x = (x2 - x);
        double xx1 = (x - x1) ;
        double x2xdeltax = x2x * deltax;
        double xx1deltax = xx1 * deltax;
        double F1 = x2xdeltax * F11 + xx1deltax * F21;
        double F2 = x2xdeltax * F12 + xx1deltax * F22;
        double deltay = 1. / (y2 - y1);

        switch (DataType)
          {
         case VTK_FLOAT:
           *(outFPixel + elemCnt) =  (y2 - y) * deltay * F1 + (y - y1) * deltay * F2;
           break;
         case VTK_DOUBLE:
           *(outDPixel + elemCnt) =  (y2 - y) * deltay * F1 + (y - y1) * deltay * F2;
           break;
          }

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (omp_get_thread_num() == 0)
          {
          if(elemCnt / (numElements / (numProcs * 100)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          }
        #else
        if(elemCnt / (numElements / 100) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        }
      }
    }
  else if (pnode->GetInterpolationOrder() == vtkMRMLAstroReprojectParametersNode::Bicubic)
    {
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outFPixel, outDPixel, cancel, status)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
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
        int ref  = (int) floor(elemCnt / referenceDims[0]);
        ref *= referenceDims[0];
        int ii = elemCnt - ref;
        ref = (int) floor(elemCnt / referenceSliceDim);
        int kk = ref;
        ref *= referenceSliceDim;
        ref = elemCnt - ref;
        int jj = (int) floor(ref / referenceDims[0]);

        double x = referenceGrid[ii][jj][0];
        int x1 = floor(x) - 1;
        bool x1Inside = x1 > 0 && x1 < inputDims[0];
        int x2 = floor(x);
        bool x2Inside = x2 > 0 && x2 < inputDims[0];
        int x3 = ceil(x);
        bool x3Inside = x3 > 0 && x3 < inputDims[0];
        int x4 = ceil(x) + 1;
        bool x4Inside = x4 > 0 && x4 < inputDims[0];

        double y = referenceGrid[ii][jj][1];
        int y1 = floor(y) - 1;
        bool y1Inside = y1 > 0 && y1 < inputDims[1];
        int y2 = floor(y);
        bool y2Inside = y2 > 0 && y2 < inputDims[1];
        int y3 = ceil(y);
        bool y3Inside = y3 > 0 && y3 < inputDims[1];
        int y4 = ceil(y) + 1;
        bool y4Inside = y4 > 0 && y4 < inputDims[1];

        if ((!x1Inside && !x4Inside) || (!y1Inside && !y4Inside))
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) = NaN;
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) = NaN;
              break;
            }
          continue;
          }

        double F11 = 0., F12 = 0., F13 = 0., F14 = 0.;
        double F21 = 0., F22 = 0., F23 = 0., F24 = 0.;
        double F31 = 0., F32 = 0., F33 = 0., F34 = 0.;
        double F41 = 0., F42 = 0., F43 = 0., F44 = 0.;
        if (!x1Inside || !y1Inside)
          {
          F11 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F11 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
              break;
            case VTK_DOUBLE:
              F11 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x1);
              break;
            }
          }

        if (!x2Inside || !y1Inside)
          {
          F21 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F21 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x2);
              break;
            case VTK_DOUBLE:
              F21 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x2);
              break;
            }
          }

        if (!x3Inside || !y1Inside)
          {
          F31 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F31 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x3);
              break;
            case VTK_DOUBLE:
              F31 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x3);
              break;
            }
          }

        if (!x4Inside || !y1Inside)
          {
          F41 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F41 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y1 + x4);
              break;
            case VTK_DOUBLE:
              F41 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y1 + x4);
              break;
            }
          }

        if (!x1Inside || !y2Inside)
          {
          F12 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F12 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x1);
              break;
            case VTK_DOUBLE:
              F12 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x1);
              break;
            }
          }

        if (!x2Inside || !y2Inside)
          {
          F22 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F22 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x2);
              break;
            case VTK_DOUBLE:
              F22 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x2);
              break;
            }
          }

        if (!x3Inside || !y2Inside)
          {
          F32 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F32 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x3);
              break;
            case VTK_DOUBLE:
              F32 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x3);
              break;
            }
          }

        if (!x4Inside || !y2Inside)
          {
          F42 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F42 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y2 + x4);
              break;
            case VTK_DOUBLE:
              F42 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y2 + x4);
              break;
            }
          }

        if (!x1Inside || !y3Inside)
          {
          F13 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F13 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y3 + x1);
              break;
            case VTK_DOUBLE:
              F13 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y3 + x1);
              break;
            }
          }

        if (!x2Inside || !y3Inside)
          {
          F23 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F23 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y3 + x2);
              break;
            case VTK_DOUBLE:
              F23 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y3 + x2);
              break;
            }
          }

        if (!x3Inside || !y3Inside)
          {
          F33 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F33 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y3 + x3);
              break;
            case VTK_DOUBLE:
              F33 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y3 + x3);
              break;
            }
          }

        if (!x4Inside || !y3Inside)
          {
          F43 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F43 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y3 + x4);
              break;
            case VTK_DOUBLE:
              F43 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y3 + x4);
              break;
            }
          }

        if (!x1Inside || !y4Inside)
          {
          F14 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F14 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y4 + x1);
              break;
            case VTK_DOUBLE:
              F14 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y4 + x1);
              break;
            }
          }

        if (!x2Inside || !y4Inside)
          {
          F24 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F24 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y4 + x2);
              break;
            case VTK_DOUBLE:
              F24 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y4 + x2);
              break;
            }
          }

        if (!x3Inside || !y4Inside)
          {
          F34 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F34 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y4 + x3);
              break;
            case VTK_DOUBLE:
              F34 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y4 + x3);
              break;
            }
          }

        if (!x4Inside || !y4Inside)
          {
          F44 = NaN;
          }
        else
          {
          switch (DataType)
            {
            case VTK_FLOAT:
              F44 = *(inFPixel + inputSliceDim * kk + inputDims[1] * y4 + x4);
              break;
            case VTK_DOUBLE:
              F44 = *(inDPixel + inputSliceDim * kk + inputDims[1] * y4 + x4);
              break;
            }
          }

        double deltaY = y - y2;

        double F1 = F12 + 0.5 * deltaY * (F13 - F11 + deltaY * (2. * F11 - 5. * F12 + 4. * F13 - F14 + deltaY * (3. * (F12 - F13) + F14 - F11)));
        double F2 = F22 + 0.5 * deltaY * (F23 - F21 + deltaY * (2. * F21 - 5. * F22 + 4. * F23 - F24 + deltaY * (3. * (F22 - F23) + F24 - F21)));
        double F3 = F32 + 0.5 * deltaY * (F33 - F31 + deltaY * (2. * F31 - 5. * F32 + 4. * F33 - F34 + deltaY * (3. * (F32 - F33) + F34 - F31)));
        double F4 = F42 + 0.5 * deltaY * (F43 - F41 + deltaY * (2. * F41 - 5. * F42 + 4. * F43 - F44 + deltaY * (3. * (F42 - F43) + F44 - F41)));

        double deltaX = x - x2;
        double F = F2 + 0.5 * deltaX * (F3 - F1 + deltaX * (2. * F1 - 5. * F2 + 4. * F3 - F4 + deltaX * (3. * (F2 - F3) + F4 - F1)));

        switch (DataType)
          {
         case VTK_FLOAT:
           *(outFPixel + elemCnt) = F;
           break;
         case VTK_DOUBLE:
           *(outDPixel + elemCnt) = F;
           break;
          }

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (omp_get_thread_num() == 0)
          {
          if(elemCnt / (numElements / (numProcs * 100)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          }
        #else
        if(elemCnt / (numElements / 100) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        }
      }
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  vtkDebugMacro("Reprojection Time : "<<mtime<<" ms.");

  inFPixel = NULL;
  outFPixel = NULL;
  inDPixel = NULL;
  outDPixel = NULL;

  delete inFPixel;
  delete outFPixel;
  delete inDPixel;
  delete outDPixel;

  for(int ii = 0; ii < referenceDims[0]; ii++)
    {
    for(int jj = 0; jj < referenceDims[1]; jj++)
      {
      delete[] referenceGrid[ii][jj];
      }
    delete[] referenceGrid[ii];
    }
  delete referenceGrid;

  if (cancel)
    {
    pnode->SetStatus(100);
    return false;
    }

  gettimeofday(&start, NULL);

  int wasModifying = outputVolume->StartModify();
  this->Internal->AstroVolumeLogic->CenterVolume(outputVolume);
  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateDisplayThresholdAttributes();
  outputVolume->EndModify(wasModifying);

  this->Internal->AstroVolumeLogic->CenterVolume(referenceVolume);

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms.");

  return true;
}
