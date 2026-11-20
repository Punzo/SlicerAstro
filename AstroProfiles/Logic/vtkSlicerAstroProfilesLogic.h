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

#ifndef __vtkSlicerAstroProfilesLogic_h
#define __vtkSlicerAstroProfilesLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkDoubleArray;
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;

// AstroProfiless includes
#include <functional>

#include "vtkSlicerAstroProfilesModuleLogicExport.h"
class vtkMRMLAstroProfilesParametersNode;

/// \class vtkSlicerAstroProfilesLogic
/// \brief Create profiles given a a selection or segmentation.
///
/// \ingroup SlicerAstro_QtModules_AstroProfiles
class VTK_SLICERASTRO_ASTROPROFILES_MODULE_LOGIC_EXPORT vtkSlicerAstroProfilesLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroProfilesLogic *New();
  vtkTypeMacro(vtkSlicerAstroProfilesLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set AstroVolume module logic
  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);

  /// Get AstroVolume module logic
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  /// Register MRML Node classes to Scene.
  /// Gets called automatically when the MRMLScene is attached to this logic class
  virtual void RegisterNodes() override;

  /// Run profile calculation algorithm
  /// \param MRML parameter node
  /// \return Success flag
  /// Synchronous: returns when the profile is computed (scripts).
  bool CalculateProfile(vtkMRMLAstroProfilesParametersNode *pnode);

#ifndef __VTK_WRAP__
  /// Start the computation without blocking the user interface; done(success)
  /// is called in the GUI thread at its end. Returns false if it could not
  /// start (done is not called). Synchronous if synchronous is true.
  bool StartCalculateProfile(vtkMRMLAstroProfilesParametersNode *pnode,
                             const std::function<void(bool)>& done, bool synchronous = false);
#endif

  /// Measurements of a line profile (e.g. the global HI profile of a galaxy).
  /// \param velocity velocity of each channel (any order, e.g. km/s)
  /// \param flux flux of each channel (NaN for blank channels)
  /// \param measurements output:
  ///   0 integrated flux: sum of flux times channel width (e.g. Jy km/s),
  ///   1 peak flux,
  ///   2 W50 and 3 W20: widths at 50% and 20% of the peak, between the
  ///     outermost crossings (from the ends of the spectrum inwards, linear
  ///     interpolation between channels; the convention for double-horned
  ///     profiles),
  ///   4 systemic velocity: middle of the W50 edges,
  ///   5, 6 lower and upper W50 edges, 7, 8 lower and upper W20 edges.
  /// \return false without a positive peak (measurements are NaN)
  static bool MeasureProfile(vtkDoubleArray* velocity, vtkDoubleArray* flux, double measurements[9]);

protected:
  vtkSlicerAstroProfilesLogic();
  virtual ~vtkSlicerAstroProfilesLogic();

private:
  vtkSlicerAstroProfilesLogic(const vtkSlicerAstroProfilesLogic&); // Not implemented
  void operator=(const vtkSlicerAstroProfilesLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

