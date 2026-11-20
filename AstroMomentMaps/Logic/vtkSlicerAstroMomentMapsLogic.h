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

#ifndef __vtkSlicerAstroMomentMapsLogic_h
#define __vtkSlicerAstroMomentMapsLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLAstroVolumeNode;
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;

// AstroMomentMapss includes
#include <functional>

#include "vtkSlicerAstroMomentMapsModuleLogicExport.h"
class vtkMRMLAstroMomentMapsParametersNode;

/// \class vtkSlicerAstroMomentMapsLogic
/// \brief Create 0th, 1st and 2nd moment maps given a selection or segmentation.
///
/// \ingroup SlicerAstro_QtModules_AstroMomentMaps
class VTK_SLICERASTRO_ASTROMOMENTMAPS_MODULE_LOGIC_EXPORT vtkSlicerAstroMomentMapsLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroMomentMapsLogic *New();
  vtkTypeMacro(vtkSlicerAstroMomentMapsLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set AstroVolume module logic
  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);

  /// Get AstroVolume module logic
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  /// Register MRML Node classes to Scene.
  /// Gets called automatically when the MRMLScene is attached to this logic class
  virtual void RegisterNodes() override;

  /// Run moment maps calculation algorithm
  /// \param MRML parameter node
  /// \return Success flag
  /// Synchronous: returns when the maps are computed (scripts, other logics).
  bool CalculateMomentMaps(vtkMRMLAstroMomentMapsParametersNode *pnode);

#ifndef __VTK_WRAP__
  /// Start the computation without blocking the user interface (worker
  /// thread, AstroVolume background starter); done(success) is called in the
  /// GUI thread when the maps are computed (or cancelled, or failed). Returns
  /// false if the computation could not start (done is not called).
  /// Synchronous if synchronous is true.
  bool StartCalculateMomentMaps(vtkMRMLAstroMomentMapsParametersNode *pnode,
                                const std::function<void(bool)>& done, bool synchronous = false);
#endif

  /// Output volume of a moment map of a data cube (2D, the celestial WCS of
  /// the cube, no volume rendering), to be filled by CalculateMomentMaps.
  /// \param input data cube
  /// \param name name of the map
  /// \param unit BUNIT of the map (e.g. "JY/BEAM km/s", "km/s")
  /// \param colorTableName color table of the map (none: the default one)
  /// \param datamodel ZEROMOMENTMAP, FIRSTMOMENTMAP or SECONDMOMENTMAP
  vtkMRMLAstroVolumeNode* CreateMomentMapVolume(vtkMRMLAstroVolumeNode* input, const char* name, const char* unit,
                                                const char* colorTableName, const char* datamodel);

protected:
  vtkSlicerAstroMomentMapsLogic();
  virtual ~vtkSlicerAstroMomentMapsLogic();

private:
  vtkSlicerAstroMomentMapsLogic(const vtkSlicerAstroMomentMapsLogic&); // Not implemented
  void operator=(const vtkSlicerAstroMomentMapsLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

