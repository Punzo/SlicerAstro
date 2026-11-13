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

// AstroReproject includes
#include "vtkSlicerAstroReprojectModuleLogicExport.h"

// STD includes
#include <string>
#include <vector>

class vtkDoubleArray;
class vtkMRMLAstroReprojectParametersNode;
class vtkMRMLAstroVolumeNode;
class vtkSlicerAstroVolumeLogic;
struct wcsprm;

/// \class vtkSlicerAstroReprojectLogic
/// \brief Reprojection, regridding, resampling and mosaicking of astronomical
/// data cubes and images (vtkAstroReprojector).
///
/// The output grid is defined by the parameters node, separately for the
/// celestial axes (grid of a reference volume or of the input, the input
/// with larger or smaller pixels, the union or the intersection of the
/// inputs, or a manual grid: frame, projection, centre, pixel size,
/// rotation, size) and for the spectral axis (channels of the input or of a
/// reference, wider or narrower channels, the union or the intersection of
/// the inputs, or manual channels: type, frame, rest frequency, start,
/// width, number). The inputs can be convolved to a common or target beam,
/// regridded in frequency or velocity with frame conversions, and combined
/// in a linear mosaic with primary beam and noise weights. The outputs are
/// new volumes (the result and optionally the weight, the sensitivity and
/// the Sault-weighted result).
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

  /// Run the reprojection (in the background runner of the AstroVolume
  /// logic if set) and create or update the output volumes. Returns false on
  /// error or cancellation (GetLastError).
  bool Reproject(vtkMRMLAstroReprojectParametersNode *pnode);

  /// Description of the output grid of the parameters (size, pixel and
  /// channel widths, frames, memory), or the reason why it can not be made
  std::string DescribeOutputGrid(vtkMRMLAstroReprojectParametersNode* pnode);

  /// Set the manual grid parameters (celestial and/or spectral) from the
  /// grid of the first input. alignNorth: no rotation.
  bool SetManualGridFromInput(vtkMRMLAstroReprojectParametersNode* pnode, bool celestial, bool spectral,
                              bool alignNorth = false);

  /// Common beam of the inputs (major and minor FWHM in arcsec, position
  /// angle in degrees); false if an input has no beam
  bool GetCommonBeam(vtkMRMLAstroReprojectParametersNode* pnode, double beam[3]);

  /// Input volumes of the parameters node
  std::vector<vtkMRMLAstroVolumeNode*> GetInputVolumes(vtkMRMLAstroReprojectParametersNode* pnode);

  /// Error of the last Reproject or DescribeOutputGrid, and the warnings of
  /// the last Reproject (one per line)
  std::string GetLastError() { return this->LastError; }
  std::string GetLastWarnings() { return this->LastWarnings; }

#ifndef __VTK_WRAP__
  /// Output grid of the parameters: WCS (free with
  /// vtkAstroReprojector::FreeWCS) and dimensions; nullptr on error.
  wcsprm* ComputeOutputGrid(vtkMRMLAstroReprojectParametersNode* pnode, int dimensions[3], std::string& error);
#endif

protected:
  vtkSlicerAstroReprojectLogic();
  virtual ~vtkSlicerAstroReprojectLogic();

  std::string LastError;
  std::string LastWarnings;

private:
  vtkSlicerAstroReprojectLogic(const vtkSlicerAstroReprojectLogic&); // Not implemented
  void operator=(const vtkSlicerAstroReprojectLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif
