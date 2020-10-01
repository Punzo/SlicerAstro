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

#ifndef __vtkSlicerAstroReprojectLogic_h
#define __vtkSlicerAstroReprojectLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;
// AstroReprojects includes
#include "vtkSlicerAstroReprojectModuleLogicExport.h"
class vtkMRMLAstroReprojectParametersNode;

/// \class vtkSlicerAstroReprojectLogic
/// \brief Implements image reprojection (resampling) methods
///  for astronomical datasets. Specifically, the methods have
///  been designed to reproject the spatial axis, (e.g., 2D images
///  or 3D datacubes over 2D images). For example in the case of
///  datacubes, the algorithm reprojects each slice
///  (spatial celestial axis) of the datacube with the reference data
///  (treating each slice as independent). However, overlaying two
///  3D astronomical datasets requires also to resample the
///  velocity axes, which is not implemented in this module.
///
/// The AstroReproject module requires that the WCS information
/// contained in the fits header are correct and that the two
/// datasets have the same celestial system of reference
/// (e.g., J2000/FK5).
///
/// \ingroup SlicerAstro_QtModules_AstroReproject
class VTK_SLICER_ASTROREPROJECT_MODULE_LOGIC_EXPORT vtkSlicerAstroReprojectLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroReprojectLogic *New();
  vtkTypeMacro(vtkSlicerAstroReprojectLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set AstroVolume module logic
  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);

  /// Get AstroVolume module logic
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  /// Register MRML Node classes to Scene.
  /// Gets called automatically when the MRMLScene is attached to this logic class
  virtual void RegisterNodes() override;

  /// Run reprojection algorithm
  /// \param MRML parameter node
  /// \return Success flag
  bool Reproject(vtkMRMLAstroReprojectParametersNode *pnode);

protected:
  vtkSlicerAstroReprojectLogic();
  virtual ~vtkSlicerAstroReprojectLogic();

private:
  vtkSlicerAstroReprojectLogic(const vtkSlicerAstroReprojectLogic&); // Not implemented
  void operator=(const vtkSlicerAstroReprojectLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

