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

#ifndef __vtkSlicerAstroPVSliceLogic_h
#define __vtkSlicerAstroPVSliceLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerAstroMomentMapsLogic;
class vtkSlicerAstroVolumeLogic;

// AstroPVSlices includes
#include "vtkSlicerAstroPVSliceModuleLogicExport.h"
class vtkMRMLAstroPVSliceParametersNode;

/// \class vtkSlicerAstroPVSliceLogic
/// \brief Calculate Position Velocity (PV) Slices.
///
/// \ingroup SlicerAstro_QtModules_AstroPVSlice
class VTK_SLICERASTRO_ASTROPVSLICE_MODULE_LOGIC_EXPORT vtkSlicerAstroPVSliceLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroPVSliceLogic *New();
  vtkTypeMacro(vtkSlicerAstroPVSliceLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

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
  virtual void RegisterNodes() VTK_OVERRIDE;

  /// Run 0th moment maps calculation algorithm
  /// \param MRML parameter node
  /// \return Success flag
  bool Calculate0thMomentMap(vtkMRMLAstroPVSliceParametersNode *pnode);

  /// Set current selected moment map on Red slice widget
  /// \param MRML parameter node
  /// \return Success flag
  bool SetMomentMapOnRedWidget(vtkMRMLAstroPVSliceParametersNode *pnode);

  /// Create the MRML Line node
  /// \param MRML parameter node
  /// \return Success flag
  bool CreateAndSetLine(vtkMRMLAstroPVSliceParametersNode *pnode);

  /// Initialize the MRML Line node
  /// \param MRML parameter node
  /// \return Success flag
  bool InitializeLine(vtkMRMLAstroPVSliceParametersNode *pnode);

  /// Update the MRML Line node
  /// \param MRML parameter node
  /// \return Success flag
  bool UpdateLine(vtkMRMLAstroPVSliceParametersNode *pnode);

  /// Update the MRML Line node when the position is given by center coordinates
  /// \param MRML parameter node
  /// \return Success flag
  bool UpdateLineFromCenter(vtkMRMLAstroPVSliceParametersNode *pnode);

  /// Initialize the PV Slice node
  /// \param MRML parameter node
  /// \return Success flag
  bool InitializePV(vtkMRMLAstroPVSliceParametersNode *pnode);

  /// Calculate the orientation and set the PV Slice node
  /// \param MRML parameter node
  /// \return Success flag
  bool UpdatePV(vtkMRMLAstroPVSliceParametersNode *pnode);

protected:
  vtkSlicerAstroPVSliceLogic();
  virtual ~vtkSlicerAstroPVSliceLogic();

private:
  vtkSlicerAstroPVSliceLogic(const vtkSlicerAstroPVSliceLogic&); // Not implemented
  void operator=(const vtkSlicerAstroPVSliceLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

