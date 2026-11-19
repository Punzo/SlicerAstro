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

  /// The line on the sky: centre (RA, Dec in degrees), position angle of the
  /// line (degrees from North to East, 0-180) and length (arcmin), from its
  /// ends with the WCS of the moment map. Returns false without line, moment
  /// map or WCS.
  bool GetLineSky(vtkMRMLAstroPVSliceParametersNode* pnode, double center[2], double* positionAngle,
                  double* length);
  /// Place the line on the sky (centre in degrees, position angle in degrees
  /// from North to East, length in arcmin); the line centre of the
  /// parameters node follows
  bool SetLineSky(vtkMRMLAstroPVSliceParametersNode* pnode, const double center[2], double positionAngle,
                  double length);
  /// The line centred on the peak of the moment map (same angle and length)
  bool CenterLineOnPeak(vtkMRMLAstroPVSliceParametersNode* pnode);
  /// The line along the major axis of the emission of the moment map: the
  /// pixels above 20% of the peak, weighted by their value (second moments),
  /// through their centroid (same length)
  bool AlignLineWithMajorAxis(vtkMRMLAstroPVSliceParametersNode* pnode);
  /// The line kept on the plane of the moment map (e.g. after a drag in a
  /// view) and the line centre of the parameters node updated. Returns false
  /// without a complete line.
  bool ConstrainLine(vtkMRMLAstroPVSliceParametersNode* pnode);

  enum PVCameraView { SkyView = 0, AlongLineView, FaceOnView };
  /// The camera of the first 3D view: on the sky (along the velocity axis,
  /// north up), along the line, or facing the PV plane (velocity up)
  bool SetPVCamera(vtkMRMLAstroPVSliceParametersNode* pnode, int view);

protected:
  vtkSlicerAstroPVSliceLogic();
  virtual ~vtkSlicerAstroPVSliceLogic();

private:
  vtkSlicerAstroPVSliceLogic(const vtkSlicerAstroPVSliceLogic&); // Not implemented
  void operator=(const vtkSlicerAstroPVSliceLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
  bool ConstrainingLine{false};
};

#endif

