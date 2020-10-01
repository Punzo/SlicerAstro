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
class vtkSlicerAstroMomentMapsLogic;
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

  /// Run 0th moment maps calculation algorithm
  /// \param MRML parameter node
  /// \return Success flag
  bool Calculate0thMomentMap(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// Set current selected moment map on Red slice widget
  /// \param MRML parameter node
  /// \return Success flag
  bool SetMomentMapOnRedWidget(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// Update the slice selection from the MRML fiducial markups node
  /// \param MRML parameter node
  /// \return Success flag
  bool UpdateSliceSelection(vtkMRMLAstroPVDiagramParametersNode *pnode);

  /// Run the calculation to generate the PV diagram and set in the layout
  /// \param MRML parameter node
  /// \return Success flag
  bool GenerateAndSetPVDiagram(vtkMRMLAstroPVDiagramParametersNode *pnode);

protected:
  vtkSlicerAstroPVDiagramLogic();
  virtual ~vtkSlicerAstroPVDiagramLogic();

private:
  vtkSlicerAstroPVDiagramLogic(const vtkSlicerAstroPVDiagramLogic&); // Not implemented
  void operator=(const vtkSlicerAstroPVDiagramLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

