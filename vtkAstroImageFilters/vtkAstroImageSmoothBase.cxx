/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

#include "vtkAstroImageSmoothBase.h"

// VTK includes
#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkPointData.h>
#include <vtkStreamingDemandDrivenPipeline.h>

// STD includes
#include <atomic>
#include <mutex>
#include <thread>

//----------------------------------------------------------------------------
class vtkAstroImageSmoothBase::vtkInternal
{
public:
  std::thread::id OwnerThread;
  std::atomic<long long> Processed{0}; // work items (e.g. slices) processed
  double TotalWork{1.};
  double ProgressStart{0.};
  double ProgressEnd{1.};
  double LastReported{-1.};
};

//----------------------------------------------------------------------------
vtkAstroImageSmoothBase::vtkAstroImageSmoothBase()
{
  this->Internal = new vtkInternal;
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

//----------------------------------------------------------------------------
vtkAstroImageSmoothBase::~vtkAstroImageSmoothBase()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkAstroImageSmoothBase::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Aborted: " << this->Aborted << "\n";
}

//----------------------------------------------------------------------------
int vtkAstroImageSmoothBase::RequestData(vtkInformation* vtkNotUsed(request),
                                         vtkInformationVector** inputVector,
                                         vtkInformationVector* outputVector)
{
  this->Aborted = false;
  vtkImageData* input = vtkImageData::GetData(inputVector[0]);
  vtkImageData* output = vtkImageData::GetData(outputVector);
  if (!input || !output || !input->GetPointData()->GetScalars())
    {
    vtkErrorMacro("RequestData: missing input image scalars.");
    return 0;
    }
  const int scalarType = input->GetScalarType();
  if ((scalarType != VTK_FLOAT && scalarType != VTK_DOUBLE) ||
      input->GetNumberOfScalarComponents() != 1)
    {
    vtkErrorMacro("RequestData: only single-component float or double images are supported.");
    return 0;
    }

  // Whole input extent (the filters need all the neighbours)
  output->SetExtent(input->GetExtent());
  output->SetOrigin(input->GetOrigin());
  output->SetSpacing(input->GetSpacing());
  output->SetDirectionMatrix(input->GetDirectionMatrix());
  output->AllocateScalars(scalarType, 1);
  output->GetPointData()->GetScalars()->SetName(input->GetPointData()->GetScalars()->GetName());

  this->Internal->OwnerThread = std::this_thread::get_id();
  this->UpdateProgress(0.);
  const bool success = this->Smooth(input, output);
  if (this->Aborted || this->GetAbortExecute())
    {
    this->Aborted = true;
    return 1; // the output is incomplete but valid
    }
  this->UpdateProgress(1.);
  return success ? 1 : 0;
}

//----------------------------------------------------------------------------
void vtkAstroImageSmoothBase::ResetProgress(double totalWork, double progressStart, double progressEnd)
{
  this->Internal->TotalWork = totalWork > 0. ? totalWork : 1.;
  this->Internal->Processed = 0;
  this->Internal->ProgressStart = progressStart;
  this->Internal->ProgressEnd = progressEnd;
  this->Internal->LastReported = -1.;
}

//----------------------------------------------------------------------------
void vtkAstroImageSmoothBase::AddProgress(double work)
{
  const double processed = static_cast<double>(this->Internal->Processed += static_cast<long long>(work));
  if (std::this_thread::get_id() != this->Internal->OwnerThread)
    {
    return;
    }
  const double fraction = this->Internal->ProgressStart +
    (this->Internal->ProgressEnd - this->Internal->ProgressStart) * (processed / this->Internal->TotalWork);
  if (fraction - this->Internal->LastReported >= 0.01)
    {
    this->Internal->LastReported = fraction;
    // invokes ProgressEvent: observers may set AbortExecute
    this->UpdateProgress(fraction);
    }
}

//----------------------------------------------------------------------------
bool vtkAstroImageSmoothBase::IsAborting()
{
  // Called from worker threads: read only (Aborted is set by RequestData)
  return this->GetAbortExecute() != 0;
}
