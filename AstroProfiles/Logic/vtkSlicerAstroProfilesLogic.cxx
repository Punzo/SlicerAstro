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

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroProfilesParametersNode.h>
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
class vtkSlicerAstroProfilesLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkImageData> tempVolumeData;
};

//----------------------------------------------------------------------------
vtkSlicerAstroProfilesLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = vtkSmartPointer<vtkSlicerAstroVolumeLogic>::New();
  this->tempVolumeData = vtkSmartPointer<vtkImageData>::New();
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
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("vtkSlicerAstroProfilesLogic::CalculateProfile : "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results that "
                  "the AstroProfiles algorithm will show poor performance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile :"
                  " inputVolume not found!");
    return false;
    }

  vtkMRMLAstroVolumeNode *ProfileVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetProfileVolumeNodeID()));
  if(!ProfileVolume)
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile :"
                  " ProfileVolume not found!");
    return false;
    }

  vtkMRMLAstroLabelMapVolumeNode *maskVolume =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetMaskVolumeNodeID()));

  bool maskActive = pnode->GetMaskActive();

  if(!maskVolume && maskActive)
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile :"
                  " maskVolume not found!");
    return false;
    }

  double BMAJ = StringToDouble(inputVolume->GetAttribute("SlicerAstro.BMAJ"));
  double BMIN = StringToDouble(inputVolume->GetAttribute("SlicerAstro.BMIN"));
  double CDELT1 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.CDELT1"));
  double CDELT2 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.CDELT2"));

  double unitBeamConv = fabs((CDELT1 * CDELT2) / (1.13 * BMAJ * BMIN));

  const int *dims = inputVolume->GetImageData()->GetDimensions();
  const int numComponents = inputVolume->GetImageData()->GetNumberOfScalarComponents();
  const int numSlice = dims[0] * dims[1] * numComponents;

  float *inFPixel = NULL;
  float *outProfileFPixel = NULL;
  short *maskPixel = NULL;
  double *inDPixel = NULL;
  double *outProfileDPixel = NULL;

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

  const double NaN = sqrt(-1);

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = inputVolume->GetAstroVolumeDisplayNode();
  if (!astroDisplay)
    {
    vtkErrorMacro("vtkSlicerAstroProfilesLogic::CalculateProfile :"
                  " astroDisplay not found!");
    return false;
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
    return false;
    }
  if (!strcmp(WCS->cunit[2], "m/s"))
    {
    VelFactor = 0.001;
    }

  if(pnode->GetMaskActive())
    {
    maskPixel = static_cast<short*> (maskVolume->GetImageData()->GetScalarPointer(0,0,0));

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outProfileFPixel, outProfileDPixel, maskPixel, cancel, status)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < dims[2]; elemCnt++)
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
              *(outProfileFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
              *(outProfileDPixel + elemCnt) = 0.;
            break;
          }

        for (int kk = 0; kk < numSlice; kk++)
          {
          int posData = elemCnt * numSlice + kk;
          switch (DataType)
            {
            case VTK_FLOAT:
              if (*(maskPixel + posData) > 0.001)
                {
                *(outProfileFPixel + elemCnt) += *(inFPixel + posData);
                }
              break;
            case VTK_DOUBLE:
              if (*(maskPixel + posData) > 0.001)
                {
                *(outProfileDPixel + elemCnt) += *(inDPixel + posData);
                }
              break;
            }
          }

        switch (DataType)
          {
          case VTK_FLOAT:
            *(outProfileFPixel + elemCnt) *= unitBeamConv;
            if (*(outProfileFPixel + elemCnt) < FLOATPRECISION)
              {
              *(outProfileFPixel + elemCnt) = NaN;
              }
            break;
          case VTK_DOUBLE:
            *(outProfileDPixel + elemCnt) *= unitBeamConv;
            if (*(outProfileDPixel + elemCnt) < DOUBLEPRECISION)
              {
              *(outProfileDPixel + elemCnt) = NaN;
              }
            break;
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

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outProfileFPixel, outProfileDPixel, maskPixel, cancel, status, Zmin, Zmax)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < dims[2]; elemCnt++)
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
              *(outProfileFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
              *(outProfileDPixel + elemCnt) = 0.;
            break;
          }

        for (int kk = 0; kk < numSlice; kk++)
          {
          int posData = elemCnt * numSlice + kk;
          switch (DataType)
            {
            case VTK_FLOAT:
              if (*(inFPixel + posData) > pnode->GetIntensityMin() &&
                  *(inFPixel + posData) < pnode->GetIntensityMax())
                {
                *(outProfileFPixel + elemCnt) += *(inFPixel + posData);
                }
              break;
            case VTK_DOUBLE:
              if (*(inDPixel + posData) > pnode->GetIntensityMin() &&
                  *(inDPixel + posData) < pnode->GetIntensityMax())
                {
                *(outProfileDPixel + elemCnt) += *(inDPixel + posData);
                }
              break;
            }
          }

        switch (DataType)
          {
          case VTK_FLOAT:
            *(outProfileFPixel + elemCnt) *= unitBeamConv;
            if (*(outProfileFPixel + elemCnt) < FLOATPRECISION)
              {
              *(outProfileFPixel + elemCnt) = NaN;
              }
            break;
          case VTK_DOUBLE:
            *(outProfileDPixel + elemCnt) *= unitBeamConv;
            if (*(outProfileDPixel + elemCnt) < DOUBLEPRECISION)
              {
              *(outProfileDPixel + elemCnt) = NaN;
              }
            break;
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

  vtkDebugMacro("Profile Kernel Time : "<<mtime<<" ms.");

  inFPixel = NULL;
  inDPixel = NULL;

  delete inFPixel;
  delete inDPixel;

  outProfileFPixel = NULL;
  outProfileDPixel = NULL;

  delete outProfileFPixel;
  delete outProfileDPixel;

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

  ProfileVolume->UpdateRangeAttributes();
  ProfileVolume->Update3DDisplayThresholdAttributes();
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

  gettimeofday(&end, NULL);;

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms.");

  return true;
}
