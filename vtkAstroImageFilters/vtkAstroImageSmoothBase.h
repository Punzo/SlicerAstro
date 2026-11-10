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

==============================================================================*/

#ifndef __vtkAstroImageSmoothBase_h
#define __vtkAstroImageSmoothBase_h

#include "vtkAstroImageFiltersExport.h"

// VTK includes
#include <vtkImageAlgorithm.h>

class vtkImageData;

/// \ingroup SlicerAstro_vtkAstroImageFilters
/// \brief Base class of the SlicerAstro CPU smoothing filters.
///
/// The filters process single-component float or double images (data cubes).
/// Blank voxels (NaN) do not contribute to the smoothed values. The work is
/// split across threads with vtkSMPTools (TBB in 3D Slicer).
///
/// Progress events are invoked only from the thread that called Update() (VTK
/// observers are not thread safe); setting AbortExecute (e.g. from a progress
/// observer) stops the computation as soon as possible: the output is then
/// incomplete and GetAborted() returns true.
class VTK_ASTRO_IMAGE_FILTERS_EXPORT vtkAstroImageSmoothBase : public vtkImageAlgorithm
{
public:
  vtkTypeMacro(vtkAstroImageSmoothBase, vtkImageAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// True if the last execution was aborted.
  vtkGetMacro(Aborted, bool);

protected:
  vtkAstroImageSmoothBase();
  ~vtkAstroImageSmoothBase() override;

  int RequestData(vtkInformation* request,
                  vtkInformationVector** inputVector,
                  vtkInformationVector* outputVector) override;

  /// Filter implementation: input and output have the same geometry and a
  /// single float or double component; output scalars are allocated.
  /// Return false on error or abort.
  virtual bool Smooth(vtkImageData* input, vtkImageData* output) = 0;

  /// Thread-safe progress reporting helpers for the subclasses:
  /// ResetProgress(total) before a parallel loop, AddProgress(n) from any
  /// thread after processing n items, IsAborting() to stop early.
  void ResetProgress(double totalWork, double progressStart = 0., double progressEnd = 1.);
  void AddProgress(double work);
  bool IsAborting();

  bool Aborted{false};

private:
  vtkAstroImageSmoothBase(const vtkAstroImageSmoothBase&) = delete;
  void operator=(const vtkAstroImageSmoothBase&) = delete;

  class vtkInternal;
  vtkInternal* Internal;
};

#endif
