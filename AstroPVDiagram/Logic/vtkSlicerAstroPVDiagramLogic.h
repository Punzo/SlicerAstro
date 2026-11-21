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

// STD includes
#include <string>

class vtkMRMLAstroPVDiagramParametersNode;
class vtkMRMLAstroVolumeNode;
class vtkMRMLMarkupsCurveNode;
class vtkSlicerAstroMomentMapsLogic;
class vtkSlicerAstroVolumeLogic;

// AstroPVDiagrams includes
#include "vtkSlicerAstroPVDiagramModuleLogicExport.h"

/// \class vtkSlicerAstroPVDiagramLogic
/// \brief Position-velocity (PV) diagram of a data cube along a path drawn on
/// its moment-0 map.
///
/// The path is a Markups open curve (GetPathName(), one per scene) with
/// straight segments or a spline through its points, kept on the plane and in
/// the extent of the moment-0 map of the cube (the map shared by the modules).
/// Each column of the PV diagram is a sample of the path, one pixel apart
/// along it: the spectrum of the cube at that position (bilinear
/// interpolation), or the mean of Width spectra across the path. The PV
/// diagram of a cube is referenced by the cube (GetPVDiagramReferenceRole())
/// and updated in place when the path changes.
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

  /// Name of the path (Markups open curve, one per scene)
  static const char* GetPathName() { return "PVDiagramLine"; }

  /// Node reference role of a cube to its PV diagram
  static const char* GetPVDiagramReferenceRole() { return "SlicerAstroPVDiagram"; }

  /// Node reference role of the 2nd moment map in the parameter node
  static const char* GetSecondMomentMapReferenceRole();

  /// The PV diagram of a cube (nullptr if none)
  static vtkMRMLAstroVolumeNode* GetPVDiagram(vtkMRMLAstroVolumeNode* cube);

  /// Error of the last call that failed (empty if none)
  const char* GetLastError() { return this->LastError.c_str(); }

  /// Calculate a moment map of the input volume: the 0th moment map (order
  /// 0, the path is drawn on it), set as moment map of the parameter node,
  /// or the 2nd moment map (order 2, velocity dispersion, Green view of the
  /// module), referenced by the parameter node with the
  /// GetSecondMomentMapReferenceRole() role. The maps shared by the modules
  /// ("<cube>_mom0", "<cube>_mom2") are used if they exist.
  /// \param MRML parameter node
  /// \param order 0 or 2
  /// \return Success flag
  bool CalculateMomentMap(vtkMRMLAstroPVDiagramParametersNode *pnode, int order);

  /// The input of the PV diagram of the cube of the parameters node: its
  /// moment-0 map (computed if missing), its PV diagram (if any) as output
  /// and the path (created if missing) with its curve type and its points
  /// on the map.
  bool PreparePath(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// The path of the parameters node (nullptr if none)
  vtkMRMLMarkupsCurveNode* GetPath(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// The points of the path on the plane and in the extent of the moment-0
  /// map. Returns true if points were moved.
  bool ConstrainPath(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// Curve type of the path from the Interpolation of the parameters node:
  /// cardinal spline or straight segments
  bool UpdatePathCurveType(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// Remove the points of the path (the PV diagram is kept)
  bool ClearPath(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// Length of the path on the sky (arcmin) and number of columns of its PV
  /// diagram (samples one pixel apart along the path). False without a path
  /// of at least two points.
  bool GetPathInfo(vtkMRMLAstroPVDiagramParametersNode *pnode, double* lengthArcmin, int* columns);

  /// Calculate the PV diagram along the path, in the PV diagram of the cube
  /// (created if missing: "<cube>_PVDiagram", the 2D colours of the cube)
  /// set as output volume of the parameters node. Synchronous: one
  /// interpolation per column and channel (vtkSMPTools over the columns).
  /// \return Success flag (GetLastError() if false)
  bool UpdatePVDiagram(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// Moment maps in the views: the moment-0 map in the Red view (XY); with
  /// secondMomentMap, the 2nd moment map in the Green view (XY, calculated
  /// if missing). The Yellow view is prepared for the PV diagram.
  bool SetMomentMapsOnViews(vtkMRMLAstroPVDiagramParametersNode *pnode, bool secondMomentMap = true);

  /// The PV diagram in the Yellow view (fitted to it with fitView)
  bool SetPVDiagramOnViews(vtkMRMLAstroPVDiagramParametersNode *pnode, bool fitView = true);

  /// UpdatePVDiagram, then SetPVDiagramOnViews
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

  /// 2D astronomical volume for the PV diagram of a cube: offset along the
  /// path (CDELT1 in degrees) and the spectral axis of the cube
  vtkMRMLAstroVolumeNode* CreatePVDiagramVolume(vtkMRMLAstroVolumeNode* cube);

  vtkMRMLAstroVolumeNode* GetInputVolume(vtkMRMLAstroPVDiagramParametersNode *pnode);
  vtkMRMLAstroVolumeNode* GetMomentMap(vtkMRMLAstroPVDiagramParametersNode *pnode);

private:
  vtkSlicerAstroPVDiagramLogic(const vtkSlicerAstroPVDiagramLogic&); // Not implemented
  void operator=(const vtkSlicerAstroPVDiagramLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
  std::string LastError;
};

#endif
