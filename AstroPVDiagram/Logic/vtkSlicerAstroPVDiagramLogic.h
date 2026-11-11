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

#ifndef __vtkSlicerAstroPVDiagramLogic_h
#define __vtkSlicerAstroPVDiagramLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
#include <string>

class vtkSlicerAstroMomentMapsLogic;
class vtkMRMLAstroVolumeNode;
class vtkSlicerAstroVolumeLogic;

// AstroPVDiagrams includes
#include "vtkSlicerAstroPVDiagramModuleLogicExport.h"
class vtkMRMLAstroPVDiagramParametersNode;

/// \class vtkSlicerAstroPVDiagramLogic
/// \brief Generates a Position Velocity (PV) diagram from fiducial markups.
///
/// \ingroup SlicerAstro_QtModules_AstroPVDiagram
class VTK_SLICERASTRO_ASTROPVDIAGRAM_MODULE_LOGIC_EXPORT vtkSlicerAstroPVDiagramLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroPVDiagramLogic *New();
  vtkTypeMacro(vtkSlicerAstroPVDiagramLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set AstroVolume module logic
  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);

  /// Get AstroVolume module logic
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  /// Set AstroMomentMaps module logic
  void SetAstroMomentMapsLogic(vtkSlicerAstroMomentMapsLogic* logic);

  /// Get AstroMomentMaps module logic
  vtkSlicerAstroMomentMapsLogic* GetAstroMomentMapsLogic();

  /// Register MRML Node classes to Scene.
  /// Gets called automatically when the MRMLScene is attached to this logic class
  virtual void RegisterNodes() override;

  /// Calculate a moment map of the input volume: the 0th moment map (order
  /// 0, Red view: the line of the PV diagram is drawn on it), set as moment
  /// map of the parameter node, or the 2nd moment map (order 2, velocity
  /// dispersion, Green view), referenced by the parameter node with the
  /// GetSecondMomentMapReferenceRole() role.
  /// \param MRML parameter node
  /// \param order 0 or 2
  /// \return Success flag
  bool CalculateMomentMap(vtkMRMLAstroPVDiagramParametersNode *pnode, int order);

  /// Set the moment maps in the views: 0th moment map in the Red view, 2nd
  /// moment map in the Green view (XY orientation); the Yellow view is prepared for the PV
  /// diagram.
  /// \param MRML parameter node
  /// \return Success flag
  bool SetMomentMapsOnViews(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// Node reference role of the 2nd moment map in the parameter node
  static const char* GetSecondMomentMapReferenceRole();

  /// Run the calculation to generate the PV diagram along the line (Markups
  /// open curve, linear or spline) and set it in the layout
  /// \param MRML parameter node
  /// \return Success flag
  bool GenerateAndSetPVDiagram(vtkMRMLAstroPVDiagramParametersNode *pnode);

protected:
  vtkSlicerAstroPVDiagramLogic();
  virtual ~vtkSlicerAstroPVDiagramLogic();

  /// 2D astronomical volume (no spectral axis, no volume rendering) for a
  /// moment map of the input volume
  vtkMRMLAstroVolumeNode* CreateMomentMapVolume(vtkMRMLAstroVolumeNode* inputVolume,
                                                const std::string& nameSuffix,
                                                const char* bunit,
                                                const char* dataModel);

private:
  vtkSlicerAstroPVDiagramLogic(const vtkSlicerAstroPVDiagramLogic&); // Not implemented
  void operator=(const vtkSlicerAstroPVDiagramLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

