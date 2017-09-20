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

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLTableNode.h>

// VTK includes
#include <vtkArrayData.h>
#include <vtkCacheManager.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
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

#include <iostream>
#include <sys/time.h>

#define FLOATPRECISION 0.000001
#define DOUBLEPRECISION 0.000000000000001

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

}// end namespace

//----------------------------------------------------------------------------
class vtkSlicerAstroMomentMapsLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkImageData> tempVolumeData;
};

//----------------------------------------------------------------------------
vtkSlicerAstroMomentMapsLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = vtkSmartPointer<vtkSlicerAstroVolumeLogic>::New();
  this->tempVolumeData = vtkSmartPointer<vtkImageData>::New();
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
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps : "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results that "
                  "the AstroMomentMaps algorithm will show poor performance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " inputVolume not found!");
    return false;
    }

  vtkMRMLAstroVolumeNode *ZeroMomentVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetZeroMomentVolumeNodeID()));
  if(!ZeroMomentVolume && pnode->GetGenerateZero())
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " ZeroMomentVolume not found!");
    return false;
    }

  vtkMRMLAstroVolumeNode *FirstMomentVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetFirstMomentVolumeNodeID()));
  if(!FirstMomentVolume && pnode->GetGenerateFirst())
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " FirstMomentVolume not found!");
    return false;
    }

  vtkMRMLAstroVolumeNode *SecondMomentVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetSecondMomentVolumeNodeID()));
  if(!SecondMomentVolume && pnode->GetGenerateSecond())
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " SecondMomentVolume not found!");
    return false;
    }

  vtkMRMLAstroLabelMapVolumeNode *maskVolume =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetMaskVolumeNodeID()));

  bool maskActive = pnode->GetMaskActive();

  if(!maskVolume && maskActive)
    {
    vtkErrorMacro("vtkSlicerAstroMomentMapsLogic::CalculateMomentMaps :"
                  " maskVolume not found!");
    return false;
    }

  const int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numSlice = dims[0] * dims[1] * numComponents;

  float *inFPixel = NULL;
  float *outZeroFPixel = NULL;
  float *outFirstFPixel = NULL;
  float *outSecondFPixel = NULL;
  short *maskPixel = NULL;
  double *inDPixel = NULL;
  double *outZeroDPixel = NULL;
  double *outFirstDPixel = NULL;
  double *outSecondDPixel = NULL;

  bool forceGenerateFirst = false;

  if (pnode->GetGenerateFirst() || pnode->GetGenerateSecond())
    {
    forceGenerateFirst = true ;
    }

  const int DataType = ZeroMomentVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
        outZeroFPixel = static_cast<float*> (ZeroMomentVolume->GetImageData()->GetScalarPointer(0,0,0));
      if (forceGenerateFirst)
        {
        outFirstFPixel = static_cast<float*> (FirstMomentVolume->GetImageData()->GetScalarPointer(0,0,0));
        }
      if (pnode->GetGenerateSecond())
        {
        outSecondFPixel = static_cast<float*> (SecondMomentVolume->GetImageData()->GetScalarPointer(0,0,0));
        }
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
        outZeroDPixel = static_cast<double*> (ZeroMomentVolume->GetImageData()->GetScalarPointer(0,0,0));
      if (forceGenerateFirst)
        {
        outFirstDPixel = static_cast<double*> (FirstMomentVolume->GetImageData()->GetScalarPointer(0,0,0));
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

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = inputVolume->GetAstroVolumeDisplayNode();
  double ijk[3], world[3];
  ijk[0] = StringToDouble(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) * 0.5;
  ijk[1] = StringToDouble(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) * 0.5;
  double VelFactor = 1.;
  if (!strcmp(astroDisplay->GetWCSStruct()->cunit[2], "m/s"))
    {
    VelFactor = 0.001;
    }

  if(pnode->GetMaskActive())
    {
    double dV = fabs((pnode->GetVelocityMax() - pnode->GetVelocityMin()) / dims[2]);
    maskPixel = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer(0,0,0));

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outZeroFPixel, outZeroDPixel, outFirstFPixel, outFirstDPixel, outSecondFPixel, outSecondDPixel, ijk, world, maskPixel, cancel, status, forceGenerateFirst, VelFactor, dV)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numSlice; elemCnt++)
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
        double ijkCoordinates[3];
        ijkCoordinates[0] = ijk[0];
        ijkCoordinates[1] = ijk[1];
        for (int kk = 0; kk < dims[2]; kk++)
          {
          int posData = elemCnt + kk * numSlice;
          if (forceGenerateFirst)
            {
            ijkCoordinates[2] = kk;
            astroDisplay->GetReferenceSpace(ijkCoordinates, SpaceCoordinates);
            SpaceCoordinates[2] *= VelFactor;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              if (*(maskPixel + posData) > 0.001)
                {
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
              if (*(outZeroFPixel + elemCnt) < FLOATPRECISION || *(outFirstFPixel + elemCnt) < FLOATPRECISION)
                {
                *(outFirstFPixel + elemCnt) = 0.;
                }
              else
                {
                *(outFirstFPixel + elemCnt) /= *(outZeroFPixel + elemCnt);
                }
              break;
            case VTK_DOUBLE:
              if (*(outZeroDPixel + elemCnt) < DOUBLEPRECISION || *(outFirstFPixel + elemCnt) < DOUBLEPRECISION)
                {
                *(outFirstDPixel + elemCnt) = 0.;
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
            int posData = elemCnt + kk * numSlice;
            ijkCoordinates[2] = kk;
            astroDisplay->GetReferenceSpace(ijkCoordinates, SpaceCoordinates);
            SpaceCoordinates[2] *= VelFactor;
            switch (DataType)
              {
              case VTK_FLOAT:
                if (*(maskPixel + posData) > 0.001)
                  {
                  *(outSecondFPixel + elemCnt) += *(inFPixel + posData) * (SpaceCoordinates[2] - *(outFirstFPixel + elemCnt))
                                                                        * (SpaceCoordinates[2] - *(outFirstFPixel + elemCnt));
                  }
                break;
              case VTK_DOUBLE:
                if (*(maskPixel + posData) > 0.001)
                  {
                  *(outSecondDPixel + elemCnt) += *(inDPixel + posData) * (SpaceCoordinates[2] - *(outFirstDPixel + elemCnt))
                                                                        * (SpaceCoordinates[2] - *(outFirstDPixel + elemCnt));
                  }
                break;
              }
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              if (*(outZeroFPixel + elemCnt) < FLOATPRECISION || *(outSecondFPixel + elemCnt) < FLOATPRECISION)
                {
                *(outSecondFPixel + elemCnt) = 0.;
                }
              else
                {
                *(outSecondFPixel + elemCnt) = sqrt(*(outSecondFPixel + elemCnt) / *(outZeroFPixel + elemCnt));
                }
              break;
            case VTK_DOUBLE:
              if (*(outZeroDPixel + elemCnt) < DOUBLEPRECISION || *(outSecondFPixel + elemCnt) < DOUBLEPRECISION)
                {
                *(outSecondDPixel + elemCnt) = 0.;
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
              *(outZeroFPixel + elemCnt) *= dV;
              break;
            case VTK_DOUBLE:
              *(outZeroDPixel + elemCnt) *= dV;
              break;
            }
          }

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (omp_get_thread_num() == 0)
          {
          if(elemCnt / (numSlice / (numProcs * 100.)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          }
        #else
        if(elemCnt / (numSlice / 100.) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        }
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
    else
      {
      Zmin = ijk[2];
      }

    double VelMax = pnode->GetVelocityMax();
    VelMax /= VelFactor;
    world[2] = VelMax;
    astroDisplay->GetIJKSpace(world, ijk);
    int Zmax;
    if (ijk[2] > dims[2])
      {
      Zmax = dims[2];
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

    double dV = fabs((pnode->GetVelocityMax() - pnode->GetVelocityMin()) / (Zmax - Zmin));
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outZeroFPixel, outZeroDPixel, outFirstFPixel, outFirstDPixel, outSecondFPixel, outSecondDPixel, ijk, world, maskPixel, cancel, status, forceGenerateFirst, VelFactor, Zmin, Zmax, dV)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numSlice; elemCnt++)
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
        double ijkCoordinates[3];
        ijkCoordinates[0] = ijk[0];
        ijkCoordinates[1] = ijk[1];
        for (int kk = Zmin; kk <= Zmax; kk++)
          {
          int posData = elemCnt + kk * numSlice;
          if (forceGenerateFirst)
            {
            ijkCoordinates[2] = kk;
            astroDisplay->GetReferenceSpace(ijkCoordinates, SpaceCoordinates);
            SpaceCoordinates[2] *= VelFactor;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              if (*(inFPixel + posData) > pnode->GetIntensityMin() &&
                  *(inFPixel + posData) < pnode->GetIntensityMax())
                {
                *(outZeroFPixel + elemCnt) += *(inFPixel + posData);
                if (forceGenerateFirst)
                  {
                  *(outFirstFPixel + elemCnt) += *(inFPixel + posData) * SpaceCoordinates[2];
                  }
                }
              break;
            case VTK_DOUBLE:
              if (*(inDPixel + posData) > pnode->GetIntensityMin() &&
                  *(inDPixel + posData) < pnode->GetIntensityMax())
                {
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
              if (*(outZeroFPixel + elemCnt) < FLOATPRECISION || *(outFirstFPixel + elemCnt) < FLOATPRECISION)
                {
                *(outFirstFPixel + elemCnt) = 0.;
                }
              else
                {
                *(outFirstFPixel + elemCnt) /= *(outZeroFPixel + elemCnt);
                }
              break;
            case VTK_DOUBLE:
              if (*(outZeroDPixel + elemCnt) < DOUBLEPRECISION || *(outFirstFPixel + elemCnt) < DOUBLEPRECISION)
                {
                *(outFirstDPixel + elemCnt) = 0.;
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
            int posData = elemCnt + kk * numSlice;
            ijkCoordinates[2] = kk;
            astroDisplay->GetReferenceSpace(ijkCoordinates, SpaceCoordinates);
            SpaceCoordinates[2] *= VelFactor;

            switch (DataType)
              {
              case VTK_FLOAT:
                if (*(inFPixel + posData) > pnode->GetIntensityMin() &&
                    *(inFPixel + posData) < pnode->GetIntensityMax())
                  {
                  *(outSecondFPixel + elemCnt) += *(inFPixel + posData) * (SpaceCoordinates[2] - *(outFirstFPixel + elemCnt))
                                                                        * (SpaceCoordinates[2] - *(outFirstFPixel + elemCnt));
                  }
                break;
              case VTK_DOUBLE:
                if (*(inDPixel + posData) > pnode->GetIntensityMin() &&
                    *(inDPixel + posData) < pnode->GetIntensityMax())
                  {
                  *(outSecondDPixel + elemCnt) += *(inDPixel + posData) * (SpaceCoordinates[2] - *(outFirstDPixel + elemCnt))
                                                                        * (SpaceCoordinates[2] - *(outFirstDPixel + elemCnt));
                  }
                break;
              }
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              if (*(outZeroFPixel + elemCnt) < FLOATPRECISION || *(outSecondFPixel + elemCnt) < FLOATPRECISION)
                {
                *(outSecondFPixel + elemCnt) = 0.;
                }
              else
                {
                *(outSecondFPixel + elemCnt) = sqrt(*(outSecondFPixel + elemCnt) / *(outZeroFPixel + elemCnt));
                }
              break;
            case VTK_DOUBLE:
              if (*(outZeroDPixel + elemCnt) < DOUBLEPRECISION || *(outSecondDPixel + elemCnt) < DOUBLEPRECISION)
                {
                *(outSecondDPixel + elemCnt) = 0.;
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
              *(outZeroFPixel + elemCnt) *= dV;
              break;
            case VTK_DOUBLE:
              *(outZeroDPixel + elemCnt) *= dV;
              break;
            }
          }

        #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
        if (omp_get_thread_num() == 0)
          {
          if(elemCnt / (numSlice / (numProcs * 100.)) > status)
            {
            status += 10;
            pnode->SetStatus(status);
            }
          }
        #else
        if(elemCnt / (numSlice / 100.) > status)
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

  vtkDebugMacro("Moment Maps Kernel Time : "<<mtime<<" ms /n");

  inFPixel = NULL;
  inDPixel = NULL;

  delete inFPixel;
  delete inDPixel;

  outZeroFPixel = NULL;
  outFirstFPixel = NULL;
  outSecondFPixel = NULL;
  outZeroDPixel = NULL;
  outFirstDPixel = NULL;
  outSecondDPixel = NULL;

  delete outZeroFPixel;
  delete outFirstFPixel;
  delete outSecondFPixel;
  delete outZeroDPixel;
  delete outFirstDPixel;
  delete outSecondDPixel;

  if (pnode->GetMaskActive())
    {
    maskPixel = NULL;
    delete maskPixel;
    }

  pnode->SetStatus(0);

  if (cancel)
    {
    return false;
    }

  gettimeofday(&start, NULL);

  if (pnode->GetGenerateZero())
    {
    ZeroMomentVolume->UpdateRangeAttributes();
    ZeroMomentVolume->UpdateNoiseAttributes();
    ZeroMomentVolume->GetAstroVolumeDisplayNode()->SetAutoWindowLevel(0);
    double min = StringToDouble(ZeroMomentVolume->GetAttribute("SlicerAstro.DATAMIN"));
    double max = StringToDouble(ZeroMomentVolume->GetAttribute("SlicerAstro.DATAMAX"));
    double window = max-min;
    double level = 0.5*(max+min);
    double lower = level;
    double upper = max;
    int disabledModify = ZeroMomentVolume->GetAstroVolumeDisplayNode()->StartModify();
    ZeroMomentVolume->GetAstroVolumeDisplayNode()->SetWindowLevel(window, level);
    ZeroMomentVolume->GetAstroVolumeDisplayNode()->SetThreshold(lower, upper);
    ZeroMomentVolume->GetAstroVolumeDisplayNode()->EndModify(disabledModify);
    }
  if (pnode->GetGenerateFirst())
    {
    FirstMomentVolume->UpdateRangeAttributes();
    FirstMomentVolume->UpdateNoiseAttributes();
    FirstMomentVolume->GetAstroVolumeDisplayNode()->SetAutoWindowLevel(0);
    double min = StringToDouble(FirstMomentVolume->GetAttribute("SlicerAstro.DATAMIN"));
    double max = StringToDouble(FirstMomentVolume->GetAttribute("SlicerAstro.DATAMAX"));
    double window = max-min;
    double level = 0.5*(max+min);
    double lower = level;
    double upper = max;
    int disabledModify = FirstMomentVolume->GetAstroVolumeDisplayNode()->StartModify();
    FirstMomentVolume->GetAstroVolumeDisplayNode()->SetWindowLevel(window, level);
    FirstMomentVolume->GetAstroVolumeDisplayNode()->SetThreshold(lower, upper);
    FirstMomentVolume->GetAstroVolumeDisplayNode()->EndModify(disabledModify);
    }
  if (pnode->GetGenerateSecond())
    {
    SecondMomentVolume->UpdateRangeAttributes();
    SecondMomentVolume->UpdateNoiseAttributes();
    SecondMomentVolume->GetAstroVolumeDisplayNode()->SetAutoWindowLevel(0);
    double min = StringToDouble(SecondMomentVolume->GetAttribute("SlicerAstro.DATAMIN"));
    double max = StringToDouble(SecondMomentVolume->GetAttribute("SlicerAstro.DATAMAX"));
    double window = max-min;
    double level = 0.5*(max+min);
    double lower = level;
    double upper = max;
    int disabledModify = SecondMomentVolume->GetAstroVolumeDisplayNode()->StartModify();
    SecondMomentVolume->GetAstroVolumeDisplayNode()->SetWindowLevel(window, level);
    SecondMomentVolume->GetAstroVolumeDisplayNode()->SetThreshold(lower, upper);
    SecondMomentVolume->GetAstroVolumeDisplayNode()->EndModify(disabledModify);
    }

  gettimeofday(&end, NULL);;

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms /n");

  return true;
}
