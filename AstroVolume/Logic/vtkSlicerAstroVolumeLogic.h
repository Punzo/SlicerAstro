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

// .NAME vtkSlicerAstroVolumeLogic - slicer logic class for AstroVolume manipulation
// .SECTION Description
// This class manages the logic associated with reading, saving,
// and changing properties of AstroVolume


#ifndef __vtkSlicerAstroVolumeLogic_h
#define __vtkSlicerAstroVolumeLogic_h

// Slicer includes
#include <atomic>
#include <functional>
#include <string>
#include <vtkSlicerVolumesLogic.h>
#include <vtkCommand.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

// STD includes
#include <cstdlib>

#include "vtkSlicerAstroVolumeModuleLogicExport.h"

class vtkMRMLCameraNode;
class vtkMRMLColorNode;
class vtkMRMLColorTableNode;
class vtkMRMLMarkupsNode;
class vtkMRMLMarkupsROINode;
class vtkMRMLAstroReprojectParametersNode;
class vtkMRMLAstroVolumeNode;
class vtkMRMLSegmentationNode;
class vtkMRMLSliceNode;
class vtkMRMLMarkupsFiducialNode;
class vtkMRMLTableNode;
class vtkCollection;
class vtkMRMLVolumePropertyNode;
class vtkMRMLVolumeNode;
class vtkSegment;
class vtkSlicerTerminologiesModuleLogic;
class vtkSlicerVolumeRenderingLogic;
class vtkMRMLVolumeRenderingDisplayNode;
class vtkVolumeProperty;
class vtkImageData;
class vtkStringArray;
struct wcsprm;
class vtkDoubleArray;
class vtkIntArray;

/// \class vtkSlicerAstroVolumeLogic
/// \brief I/O and color funtions handling for AstroVolumes (FITS files).
///
/// \ingroup SlicerAstro_QtModules_AstroVolume
class VTK_SLICERASTRO_ASTROVOLUME_MODULE_LOGIC_EXPORT vtkSlicerAstroVolumeLogic :
  public vtkSlicerVolumesLogic
{
public:

  static vtkSlicerAstroVolumeLogic *New();
  vtkTypeMacro(vtkSlicerAstroVolumeLogic,vtkSlicerVolumesLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  typedef vtkSlicerAstroVolumeLogic Self;

  enum Events
    {
    /// A scene saved with a SlicerAstro older than 2.0 was imported (see
    /// GetIncompatibleSceneMessage): invoked at the end of the import.
    IncompatibleSceneEvent = vtkCommand::UserEvent + 1301
    };

  /// Message for the scenes of SlicerAstro older than 2.0.
  static const char* GetIncompatibleSceneMessage();

  /// True if the last imported scene was saved with a SlicerAstro older
  /// than 2.0 (its astronomical volumes have no scene format).
  vtkGetMacro(IncompatibleSceneImported, bool);

  /// Register the factory that the AstroVolume needs to manage fits
  /// file with the specified volumes logic
  void RegisterArchetypeVolumeNodeSetFactory(vtkSlicerVolumesLogic* volumesLogic);

  /// Return the scene containing the volume rendering presets.
  /// If there is no presets scene, a scene is created and presets are loaded into.
  /// The presets scene is loaded from a file (presets.xml) located in the
  /// module share directory
  /// \sa vtkMRMLVolumePropertyNode, GetModuleShareDirectory()
  vtkMRMLScene *GetPresetsScene();

  /// Return the node of the MRML scene that stands for a preset of the
  /// presets scene (same name): the preset of an astro volume
  /// (vtkMRMLAstroVolumeNode::SetPresetNode) refers to it, so that it is
  /// saved and restored with the scene. These nodes are singletons
  /// (tag "SlicerAstro.Preset.<name>"), hidden from the editors: the presets
  /// of a loaded scene replace them instead of being duplicated.
  /// Return nullptr if presetNode is not a volume property node.
  vtkMRMLVolumePropertyNode* GetScenePresetNode(vtkMRMLNode* presetNode);

  /// \brief synchronizePresetsToVolumeNode
  /// \param astroVolumeNode,
  /// \return succees
  bool synchronizePresetsToVolumeNode(vtkMRMLNode *node);

  /// Update the units nodes to the metadata stored in the active volume
  /// \param astroVolumeNode,
  void updateIntensityUnitsNode(vtkMRMLNode *astroVolumeNode);

  /// Create a deep copy of a \a astroVolumeNode and add it to the \a scene
  /// Only works for vtkMRMLAstroVolumeNode.
  /// \sa CloneVolumeGeneric, \sa CloneVolume
  vtkMRMLAstroVolumeNode *CloneAstroVolume(vtkMRMLScene *scene,
                                           vtkMRMLAstroVolumeNode *inputVolumeNode,
                                           vtkMRMLAstroVolumeNode *outputVolumeNode,
                                           const char *outputNameReference,
                                           const char *name,
                                           bool cloneImageData = true);

  /// Widen the gaps between the points of the scalar opacity and color
  /// transfer functions of \a volumeProperty to at least 1/10000 of the
  /// scalar range. The default transfer functions of Slicer keep duplicate
  /// points apart with a gap of the order of the float precision: with the
  /// small intensity ranges of astronomical data (e.g. 0.01 Jy/beam) the
  /// lookup tables of the volume rendering would need textures wider than
  /// the GPU allows. Returns true if a transfer function was modified.
  static bool EnsureMinimumTransferFunctionGap(vtkVolumeProperty* volumeProperty, const double scalarRange[2]);

  /// Volume Rendering module logic (set by the AstroVolume module): used to
  /// create the volume rendering nodes and the cropping box of the volumes
  void SetVolumeRenderingLogic(vtkSlicerVolumeRenderingLogic* volumeRenderingLogic);
  vtkSlicerVolumeRenderingLogic* GetVolumeRenderingLogic();

  /// Volume rendering display node of the volume (created if create is true
  /// and the Volume Rendering logic is set)
  vtkMRMLVolumeRenderingDisplayNode* GetVolumeRenderingDisplayNode(vtkMRMLAstroVolumeNode* volumeNode,
                                                                   bool create);

  /// 3D color function of a volume from a preset of the presets scene
  /// (GetPresetsScene): the presets are adapted to the noise and range of
  /// the data (synchronizePresetsToVolumeNode), the preset is copied in the
  /// volume property of the volume rendering ("<preset> · <volume>"), it
  /// becomes the preset of the volume (saved with the scene) and, with
  /// resetShifts, its shift, stretch and opacity shift are set to 0. The
  /// volume rendering nodes are created if needed. Returns false without
  /// volume, preset or volume rendering.
  bool ApplyPreset(vtkMRMLAstroVolumeNode* volumeNode, vtkMRMLVolumePropertyNode* presetNode,
                   bool resetShifts = true);
  /// Only the copy of a preset in the volume property of the existing volume
  /// rendering of the volume ("<preset> · <volume>"), which becomes the
  /// volume property of the volume: no nodes are created, the presets are
  /// not adapted and the preset of the volume is not changed. Returns false
  /// without volume rendering.
  bool CopyPresetToVolume(vtkMRMLAstroVolumeNode* volumeNode, vtkMRMLVolumePropertyNode* presetNode);

  /// Shift (data units), stretch (data units added to the distance of the
  /// outer points from the centre) and opacity shift of the 3D color
  /// function of the volume, from the preset: the change from the values
  /// stored in its display node (PresetOffset, PresetStretch, OpacityShift)
  /// is applied to the transfer functions and the values are stored. The
  /// first and last points of the functions do not move. Returns false
  /// without volume property.
  bool SetColorFunctionShift(vtkMRMLAstroVolumeNode* volumeNode, double shift);
  bool SetColorFunctionStretch(vtkMRMLAstroVolumeNode* volumeNode, double stretch);
  bool SetColorFunctionOpacityShift(vtkMRMLAstroVolumeNode* volumeNode, double shift);

  /// Transfer functions of a volume property (color and scalar opacity),
  /// except their first and last points: moved by offset (a point does not
  /// pass its neighbours), spread by factor away from the centre of the
  /// points (negative: closer), opacities shifted by shift (in 0.001 - 1;
  /// zero opacities stay zero). Same rules as vtkControlPointsItem
  /// (MovePoints, SpreadPoints), used by the transfer function editors.
  static void MoveTransferFunctionPoints(vtkVolumeProperty* volumeProperty, double offset);
  static void SpreadTransferFunctionPoints(vtkVolumeProperty* volumeProperty, double factor);
  static void ShiftOpacities(vtkVolumeProperty* volumeProperty, double shift);

  /// Volume rendering quality (vtkMRMLViewNode::VolumeRenderingQuality) of
  /// the 3D views showing the volume
  bool SetVolumeRenderingQuality(vtkMRMLAstroVolumeNode* volumeNode, int quality);
  int GetVolumeRenderingQuality(vtkMRMLAstroVolumeNode* volumeNode);

  /// Cropping box (Markups ROI) of the volume rendering, created (fitted to
  /// the volume) if create is true
  vtkMRMLMarkupsROINode* GetVolumeRenderingROINode(vtkMRMLAstroVolumeNode* volumeNode, bool create);
  /// Crop the volume rendering to its box (shown while cropping)
  bool SetVolumeRenderingCropping(vtkMRMLAstroVolumeNode* volumeNode, bool crop);
  /// Fit the box to the whole volume
  bool FitVolumeRenderingROI(vtkMRMLAstroVolumeNode* volumeNode);

  /// Replace the anatomical slice orientation presets of a slice node
  /// (Axial, Sagittal, Coronal) with the astronomical ones (XZ, ZY, XY).
  /// The orientation and the default orientation of the node are converted.
  static void SetAstroSliceOrientationPresets(vtkMRMLSliceNode* sliceNode);

  /// Astronomical measurements of markups (lines, open and closed curves,
  /// angles) with the WCS of the active volume (vtkMRMLAstroMeasurement:
  /// separation, velocity difference, sky length, sky area, sky angle): added
  /// if missing (also in place of the static copies of a loaded scene); the
  /// geometric measurements (millimetres, angles in the slice plane) are used
  /// only when the active volume is not astronomical. The measurements are
  /// computed again.
  static void UpdateAstroMeasurements(vtkMRMLMarkupsNode* markupsNode);

  /// Return true if the segmentation is an astronomical mask (binary labelmap
  /// source, not contours): read from a FITS mask (it has the FITS header),
  /// with an astronomical volume as reference geometry, or without reference
  /// volume in a scene with astronomical volumes.
  static bool IsAstroSegmentation(vtkMRMLSegmentationNode* segmentationNode);

  /// Give an astronomical segmentation without storage node the FITS mask
  /// storage node (vtkMRMLAstroSegmentationStorageNode), so that it is saved
  /// as a FITS mask by default (the default FITS storage is removed again,
  /// before the first save, from a segmentation that is no longer
  /// astronomical). Returns true if the segmentation is stored as a FITS mask.
  bool EnsureFITSMaskStorageNode(vtkMRMLSegmentationNode* segmentationNode);

  /// Return true if the segmentation has a FITS header for a FITS mask: it
  /// was read from a FITS mask, or its reference geometry is an astronomical
  /// volume
  static bool HasFITSMaskHeader(vtkMRMLSegmentationNode* segmentationNode);

  /// Save a mask as a FITS mask (integer cube, one label value per segment):
  /// with the FITS header of the mask read from file, or of referenceVolume
  /// (its geometry becomes the reference of the mask). A storage node of
  /// another format is replaced by the FITS mask storage; ".fits" is added
  /// to fileName when it has no FITS extension. On failure the reason is in
  /// GetLastMaskSaveError (also for contours and empty masks).
  bool SaveFITSMask(vtkMRMLSegmentationNode* segmentationNode, vtkMRMLAstroVolumeNode* referenceVolume,
                    const char* fileName);
  std::string GetLastMaskSaveError() { return this->LastMaskSaveError; }

  /// Astronomical terminology (vtkMRMLAstroTerminology) for the new segments
  /// of an astronomical segmentation: default terminology entry of the node
  /// (Mask for masks, Contour for contours), used by the Segment Editor. An
  /// entry chosen by the user is kept; the default set by SlicerAstro is
  /// removed from a segmentation that is no longer astronomical. Returns true
  /// if the segmentation is astronomical.
  static bool EnsureAstroTerminology(vtkMRMLSegmentationNode* segmentationNode);

  /// Color table of the astronomical terminology (singleton, name
  /// "SlicerAstro"): one color per type of the terminology loaded in the
  /// Terminologies module, with its recommended color and its terminology, so
  /// that it is listed in the terminology selector. Returns the table (the
  /// existing one if it is already in the scene, filled again if it was
  /// emptied by the closing of a scene), nullptr if the terminology is not
  /// loaded.
  vtkMRMLColorTableNode* AddAstroTerminologyColorTable();
  /// Terminologies module logic, with the astronomical terminology loaded
  /// (set by the AstroVolume module): the color table is added again to the
  /// scene after it is closed
  void SetTerminologiesLogic(vtkSlicerTerminologiesModuleLogic* terminologiesLogic);
  static const char* GetAstroTerminologyColorTableSingletonTag() { return "SlicerAstro.Terminology"; }

  /// Use the astronomical volume as reference geometry of a new mask (no
  /// segments and no reference volume yet; not the contours). Then the mask
  /// is saved as a FITS mask with the header of the volume. Returns true if
  /// the reference was set.
  static bool SetMaskReferenceVolume(vtkMRMLSegmentationNode* segmentationNode,
                                     vtkMRMLAstroVolumeNode* volumeNode);

  /// Name a mask with the default name of Slicer ("Segmentation",
  /// "Segmentation_1", ...) after its reference astronomical volume:
  /// "<volume>_mask", unique in the scene. Returns true if renamed.
  static bool NameMaskAfterReferenceVolume(vtkMRMLSegmentationNode* segmentationNode);

  /// Estimate the beam of a radio data cube from its noise. The noise of
  /// interferometric data is correlated on the scale of the beam: its
  /// spatial autocorrelation is the beam convolved with itself (for a
  /// Gaussian beam, sqrt(2) times wider). The autocorrelation is measured
  /// on the first and last channels (usually without emission).
  /// beam: BMAJ, BMIN (FWHM, degrees) and BPA (degrees, from North to East).
  /// Returns false if the noise is not correlated (e.g. single dish or
  /// optical data) or the cube is too small.
  static bool EstimateBeamFromNoise(vtkMRMLAstroVolumeNode* volumeNode, double beam[3]);

  /// Source catalogues (tables of AstroSoFiACatalog, attribute
  /// SlicerAstro.SoFiASources) placed on the volume, directly or through
  /// its mask
  static void GetSourceCatalogues(vtkMRMLAstroVolumeNode* volumeNode, vtkCollection* tables);
  /// The volume a catalogue is placed on (its reference cube, or the cube of
  /// its reference mask)
  static vtkMRMLAstroVolumeNode* GetSourceCatalogueVolume(vtkMRMLTableNode* table);
  /// The points of the sources of a catalogue
  static vtkMRMLMarkupsFiducialNode* GetSourceCataloguePoints(vtkMRMLTableNode* table);
  /// Select the source of a row of a catalogue: its point becomes the only
  /// selected one and the slice views are centred on it. Returns the SoFiA
  /// id of the source ("" if it has none, or for an invalid row).
  static std::string SelectSource(vtkMRMLTableNode* table, int row);
  /// Segment of a source (SoFiA id, segment tag SlicerAstro.SourceID) in a
  /// mask; "" if none
  static std::string GetSourceSegmentID(vtkMRMLSegmentationNode* mask, const char* sourceID);

  /// Beam of the data in degrees: BMAJ, BMIN (FWHM) and BPA (position angle
  /// of the major axis, from North to East; 0 if not given). Returns false
  /// (beam 0) if the data have no beam.
  static bool GetBeam(vtkMRMLAstroVolumeNode* volumeNode, double beam[3]);
  /// Set the beam of the data (degrees, saved in the FITS header): the
  /// larger axis is the major one. Returns false for axes that are not
  /// positive.
  static bool SetBeam(vtkMRMLAstroVolumeNode* volumeNode, const double beam[3]);

  /// Velocity definition of the coordinates of a cube (readouts, axes):
  /// optical or radio. The slice views are updated. Returns false if the
  /// definition can not be changed (e.g. no velocity axis).
  static bool SetVelocityDefinition(vtkMRMLAstroVolumeNode* volumeNode, bool optical);
  static bool IsOpticalVelocityDefinition(vtkMRMLAstroVolumeNode* volumeNode);

  /// Right ascension in hours, minutes and seconds (true) or in degrees
  /// (readouts, axes). The slice views are updated.
  static bool SetRightAscensionSexagesimal(vtkMRMLAstroVolumeNode* volumeNode, bool sexagesimal);
  static bool IsRightAscensionSexagesimal(vtkMRMLAstroVolumeNode* volumeNode);

  /// Channel width of a cube at the centre of the field, in the unit of the
  /// spectral axis (km/s for velocities given in m/s); 0 without spectral
  /// axis
  static double GetChannelWidth(vtkMRMLAstroVolumeNode* volumeNode);

  /// Mask of a segment on the voxel grid of a volume (input masks of the
  /// modules): unsigned char voxels with the dimensions of the volume, 1
  /// inside the segment and 0 outside. The binary labelmap of the segment is
  /// read directly when it is on the voxel grid of the volume (e.g. the masks
  /// of the volume, whose reference geometry is the volume), otherwise it is
  /// resampled (nearest neighbour; transforms of the segmentation and of the
  /// volume applied). The segments sharing a labelmap layer are told apart
  /// by their label value. The mask of another astronomical volume (its
  /// reference geometry) is placed with the WCS when the two volumes are on
  /// the same voxel lattice (e.g. a cube and its cutout; projected along the
  /// channels on a 2D image of the same sky grid), and refused otherwise
  /// (CopyMaskSegmentsToVolume reprojects it). Returns false if the segment
  /// or the voxels of the volume are not found, or for a mask on another grid.
  static bool CreateSegmentMask(vtkMRMLSegmentationNode* segmentationNode, const char* segmentID,
                                vtkMRMLVolumeNode* volumeNode, vtkImageData* mask);
#ifndef __VTK_WRAP__
  /// As CreateSegmentMask, with the reason of a failure in error.
  static bool CreateSegmentMask(vtkMRMLSegmentationNode* segmentationNode, const char* segmentID,
                                vtkMRMLVolumeNode* volumeNode, vtkImageData* mask, std::string& error);
#endif

  /// Return true if the segmentation is a mask: binary labelmap source and
  /// not contours.
  static bool IsMaskSegmentation(vtkMRMLSegmentationNode* segmentationNode);

  /// Mask of a volume: a mask segmentation with the volume as reference
  /// geometry. The preferred segmentation is returned if it is a mask of the
  /// volume, otherwise the last mask of the volume added to the scene;
  /// nullptr if the volume has no mask.
  static vtkMRMLSegmentationNode* GetMaskForVolume(vtkMRMLScene* scene, vtkMRMLVolumeNode* volumeNode,
                                                   vtkMRMLSegmentationNode* preferredSegmentationNode = nullptr);

  /// Segmentations associated with an astronomical volume: its contours
  /// ("<volume>_Contours": contours drawn over the volume, of the volume or of
  /// other data reprojected on it; not a mask) and its mask ("<volume>_mask",
  /// binary labelmap, FITS mask). Both have the volume as reference geometry
  /// and are referenced by the volume (roles GetContoursReferenceRole and
  /// GetMaskReferenceRole). An existing segmentation of the scene is used
  /// (and its references fixed) before creating a new one (if create).
  vtkMRMLSegmentationNode* GetContoursSegmentationForVolume(vtkMRMLAstroVolumeNode* volumeNode, bool create);
  vtkMRMLSegmentationNode* GetMaskSegmentationForVolume(vtkMRMLAstroVolumeNode* volumeNode, bool create);
  /// Contours and mask segmentations of the volume (created if needed): when
  /// a FITS file is loaded, and for the volumes of an imported scene
  void EnsureVolumeSegmentations(vtkMRMLAstroVolumeNode* volumeNode);
  /// EnsureVolumeSegmentations for a volume loaded from a file (data and a
  /// storage node with a file name), outside of scene imports
  void EnsureSegmentationsOfLoadedVolume(vtkMRMLAstroVolumeNode* volumeNode);
  /// Opacity of the masks created by SlicerAstro: Slicer's default (the
  /// opacities of the segments, shown and edited in the Segment Editor, are
  /// the ones displayed). The opacity of a mask read from a FITS file is the
  /// one saved in the file (keyword SEGOPAC).
  static double GetDefaultMaskOpacity() { return 1.; }

  /// A mask with segments (or read from a file) whose reference geometry is
  /// an astronomical volume replaces the empty mask created automatically for
  /// that volume (e.g. the mask of a data cube loaded after the cube).
  /// Returns true if replaced.
  bool ReplaceEmptyMaskOfReferenceVolume(vtkMRMLSegmentationNode* segmentationNode);

  /// True if the segmentation has the geometry (voxel grid) of the volume.
  static bool HasGeometryOfVolume(vtkMRMLSegmentationNode* segmentationNode, vtkMRMLVolumeNode* volumeNode);

  /// Factor converting intensities in Jy/beam to fluxes in Jy per pixel:
  /// pixel area / area of the Gaussian beam (pi / (4 ln 2) BMAJ BMIN). 0 if
  /// the data are not in Jy/beam (BUNIT) or the beam or the pixel size are
  /// not defined (missing, UNDEFINED or not positive, e.g. BMAJ = -1).
  /// Version of SlicerAstro (e.g. "2.0.0")
  static std::string GetSlicerAstroVersion();
  /// Versions of the libraries of SlicerAstro used by this logic, e.g.
  /// "CFITSIO 4.7;WCSLIB 8.10" (name and version separated by a space,
  /// libraries by ";")
  static std::string GetLibraryVersions();

  static double GetJyBeamToJyFactor(vtkMRMLVolumeNode* volumeNode);

  /// Voxel offset between the grids of two astronomical volumes (WCS): voxel
  /// (i, j, k) of volumeA is voxel (i, j, k) + offset of volumeB. True if the
  /// two grids are the same voxel lattice (same pixel size, projection,
  /// orientation, celestial frame and channels; whole voxel offset), e.g. a
  /// cube and a cutout of it. A 3D volumeA and a 2D volumeB (e.g. a moment
  /// map) are compared on their celestial axes (offset[2] = 0).
  static bool GetGridOffset(vtkMRMLAstroVolumeNode* volumeA, vtkMRMLAstroVolumeNode* volumeB, int offset[3]);
#ifndef __VTK_WRAP__
  /// GetGridOffset for two WCS (the grid of A has dimensionsA voxels).
  static bool GetGridOffset(struct wcsprm* wcsA, const int dimensionsA[3], struct wcsprm* wcsB, int offset[3]);
#endif

  /// The astronomical volume of the scene whose voxel lattice is the one of
  /// a FITS mask without reference volume (e.g. a FITS mask loaded after its
  /// data cube; the active volume first, then the most recent one), nullptr
  /// if none. offset: the voxel offset of the mask in the grid of the volume
  /// (non zero for the mask of a part of the cube).
  static vtkMRMLAstroVolumeNode* FindVolumeOfMask(vtkMRMLScene* scene, vtkMRMLSegmentationNode* segmentationNode,
                                                  int offset[3]);

  /// The mask on the voxel grid of the volume: its labelmaps are placed in
  /// the grid of the volume (offset of the mask in that grid: geometry only,
  /// the voxels are not resampled) and the volume becomes its reference
  /// geometry. The FITS mask is still saved on its own grid (its header).
  static bool AttachMaskToVolume(vtkMRMLSegmentationNode* segmentationNode, vtkMRMLAstroVolumeNode* volumeNode,
                                 const int offset[3]);

  /// The FITS masks of the scene without reference volume that are on the
  /// voxel lattice of the volume (e.g. masks loaded before their cube) are
  /// attached to it (AttachMaskToVolume). Returns the number of masks.
  int AttachMasksToVolume(vtkMRMLAstroVolumeNode* volumeNode);

  /// Copy (move: copy, then remove from the mask) segments of a mask (all if
  /// segmentIDs is empty) to the mask of another volume. The mask must have
  /// an astronomical reference volume. The segments are placed voxel by
  /// voxel when the volumes are on the same voxel lattice (e.g. a smoothed
  /// copy, a cutout), projected along the channels onto a 2D image on the
  /// same sky grid (e.g. a moment map), otherwise reprojected with the WCS
  /// (AstroReproject, nearest neighbour). On failure the reason is in
  /// GetLastMaskCopyError. Returns the mask of the target volume.
  vtkMRMLSegmentationNode* CopyMaskSegmentsToVolume(vtkMRMLSegmentationNode* segmentationNode,
                                                    vtkStringArray* segmentIDs,
                                                    vtkMRMLAstroVolumeNode* targetVolume, bool move);
  std::string GetLastMaskCopyError() { return this->LastMaskCopyError; }

  static const char* GetContoursReferenceRole() { return "SlicerAstroContours"; }
  static const char* GetMaskReferenceRole() { return "SlicerAstroMask"; }

  /// Point the camera at the volume: focal point at the center of the RAS
  /// bounds of the volume (or at focalPoint, if given), camera along
  /// directionOfProjection (from the camera to the focal point) at the
  /// distance where the whole volume fits in the view angle.
  /// Returns false if the volume has no valid bounds.
  static bool CenterCameraOnVolume(vtkMRMLCameraNode* cameraNode, vtkMRMLVolumeNode* volumeNode,
                                   const double directionOfProjection[3], const double viewUp[3],
                                   const double* focalPoint = nullptr);

  /// Camera of a 3D view (e.g. "vtkMRMLViewNode1")
  static vtkMRMLCameraNode* GetThreeDViewCameraNode(vtkMRMLScene* scene, const char* viewNodeID);

  /// Display the binary labelmap of the segmentation in the 2D and 3D views
  /// (no closed surface: faster, and the voxels of the data are shown); the
  /// closed surface representation is removed if it is not the source. The
  /// 3D display is on ("Show 3D" of the Segment Editor).
  static void SetBinaryLabelmapDisplay(vtkMRMLSegmentationNode* segmentationNode);

  /// Return true if the segmentation has at least one segment that is not
  /// empty (voxels with the label value of the segment in its binary
  /// labelmap, or a closed surface with points). The search stops at the
  /// first voxel found.
  static bool HasNonEmptySegments(vtkMRMLSegmentationNode* segmentationNode);

  /// Contours (AstroVolume contours, comparisons of Smoothing and Modeling) are
  /// segmentations of closed surfaces marked with the AstroContours attribute:
  /// they are not masks (not in the Masks section, not saved as FITS masks).
  static const char* GetContoursAttributeName() { return "AstroContours"; }
  static bool IsContoursSegmentation(vtkMRMLSegmentationNode* segmentationNode);

  /// Contour levels of data drawn over a volume (its contours segmentation,
  /// see GetContoursSegmentationForVolume): one segment per level, with its
  /// own binary labelmap, tagged with the ID of the source data.
  /// \param target volume the contours are drawn over
  /// \param source data of the contours: the target itself, or other data,
  ///   reprojected on the celestial grid of the target (bilinear) unless the
  ///   grids are the same
  /// \param levels levels in the units of the source; a positive level
  ///   selects the voxels above it, a negative one the voxels below it;
  ///   with bands, pairs (lower, upper) of bands
  /// \param overlay keep the contours of other data (otherwise all the
  ///   previous contours of the target are replaced)
  /// \param colors optional RGB (0-1, 3 components) of each level (of each
  ///   band); by default the contours colour of the source display node
  /// \param visible3D contours also shown in 3D
  /// \return number of contour segments created; GetLastContoursMessage
  ///   explains a failure (0) or the levels left out (outside the data)
  int CreateContours(vtkMRMLAstroVolumeNode* target, vtkMRMLAstroVolumeNode* source, vtkDoubleArray* levels,
                     bool bands, bool overlay, vtkDoubleArray* colors = nullptr, bool visible3D = false);
  std::string GetLastContoursMessage() { return this->LastContoursMessage; }

  /// Colour families of contour levels ("Amber", "Cyan", "Deep blue",
  /// "White", "Observatory ramp"): names, and count colours (RGB 0-1, 3
  /// components) of a family from the lowest level to the highest (an
  /// unknown family gives the first one; false if count < 1 or no array)
  static void GetContourColorFamilies(vtkStringArray* names);
  static bool GetContourFamilyColors(const char* family, int count, vtkDoubleArray* colors);

  /// Contours drawn over a volume per source data: the IDs of the source
  /// volumes and, for each, the number of its levels (segments)
  static void GetContourSources(vtkMRMLSegmentationNode* contours, vtkStringArray* sourceIDs, vtkIntArray* levels);
  /// Shows or hides the contours of a source data
  static void SetContourSourceVisibility(vtkMRMLSegmentationNode* contours, const char* sourceID, bool visible);
  static bool GetContourSourceVisibility(vtkMRMLSegmentationNode* contours, const char* sourceID);

  /// HI column density per unit of a map of integrated intensity (cm^-2 per
  /// Jy/beam km/s, or per K km/s): N_HI = 1.8224e18 cm^-2 int(T_B dv) with
  /// T_B from the beam (Rayleigh-Jeans, at the HI rest frequency). 0 if the
  /// map is not an integrated intensity (unit Jy/beam or K times km/s or
  /// m/s) or a Jy/beam map has no beam.
  static double GetHIColumnDensityFactor(vtkMRMLVolumeNode* mapNode);

  /// Mask of the voxels of data above a threshold (e.g. 3 sigma of smoothed
  /// data): a new mask segmentation (binary labelmap, astronomical Mask
  /// terminology) on the grid of the reference volume, with one segment.
  /// \param reference volume of the mask (e.g. the input of the smoothing)
  /// \param data the thresholded data (the same grid)
  /// \param threshold lower limit; the voxels up to the maximum of the data
  /// \param segmentName name of the segment
  /// \param color colour of the segment (RGB 0-1)
  vtkMRMLSegmentationNode* CreateThresholdMask(vtkMRMLAstroVolumeNode* reference, vtkMRMLAstroVolumeNode* data,
                                               double threshold, const char* segmentName, const double color[3]);

  /// Options of the SlicerAstro 2D colour functions (colour tables with the
  /// attribute SlicerAstro.AddFunctions "on"): "Reverse" (the colours in
  /// reverse order), "Inverse" (complementary colours), "Log" (logarithmic
  /// scale). The attribute SlicerAstro.<option> ("on"/"off") is the state of
  /// the lookup table: the table is changed only when the state changes.
  /// \return false for an unknown option or a colour node without lookup table
  static bool SetColorFunctionOption(vtkMRMLColorNode* colorNode, const char* option, bool on);
  static bool GetColorFunctionOption(vtkMRMLColorNode* colorNode, const char* option);
  /// The colour node has the SlicerAstro options (reverse, inverse, log)
  static bool HasColorFunctionOptions(vtkMRMLColorNode* colorNode);

  /// Before adding contour levels as binary labelmaps: binary labelmap as
  /// source representation (the segmentation is marked as contours).
  static void PrepareContoursUpdate(vtkMRMLSegmentationNode* segmentationNode);

  /// After adding the contour levels: binary labelmaps (source
  /// representation) shown in the 2D and 3D views; the closed surfaces are
  /// removed (slow for large data, see SetContoursSmooth2D). The segmentation
  /// is marked as contours (not listed as a mask).
  static void SetContoursDisplay(vtkMRMLSegmentationNode* segmentationNode);

  /// 2D display of contours: closed surfaces (smooth lines, if the segments
  /// have them) or binary labelmaps (outlines of the voxels).
  static void SetContoursSmooth2D(vtkMRMLSegmentationNode* segmentationNode, bool smooth);

  /// Tag of the contour segments with the ID of the volume of their data
  static const char* GetContoursVolumeTagName() { return "SlicerAstro.ContoursVolumeID"; }

  /// Apply SetAstroSliceOrientationPresets to the default slice node and to
  /// all the slice nodes of the scene.
  void UpdateSliceOrientationPresets();

  /// Sets ROI to fit to input volume.
  /// If ROI is under a non-linear transform then the ROI transform will be reset to RAS.
  /// \param MRML parameter node
  /// \return Success flag
  virtual bool FitROIToInputVolume(vtkMRMLMarkupsROINode* roiNode,
                                   vtkMRMLAstroVolumeNode *inputVolume);

  /// Snap the ROI to the voxel grid of the input volume
  /// \param MRML parameter node
  virtual void SnapROIToVoxelGrid(vtkMRMLMarkupsROINode* roiNode,
                                  vtkMRMLAstroVolumeNode *inputVolume);

  /// Check if the ROI is aligned with the input volume
  /// \param MRML parameter node
  /// \return Success flag
  virtual bool IsROIAlignedWithInputVolume(vtkMRMLMarkupsROINode* roiNode,
                                           vtkMRMLAstroVolumeNode *inputVolume);

  /// Calculate the Bounds of a ROI within a Volume in IJK coordinates
  /// \param MRML ROI node
  /// \param MRML input volume node
  /// \param extent (i.e., dimensions)
  /// \return Success flag
  virtual bool CalculateROICropVolumeBounds(vtkMRMLMarkupsROINode* roiNode,
                                            vtkMRMLAstroVolumeNode *inputVolume,
                                            double outputExtent[6]);

  /// Calculate the Bounds of a Segment within a Volume in IJK coordinates
  virtual bool CalculateSegmentCropVolumeBounds(vtkMRMLSegmentationNode *segmentationNode,
                                                vtkSegment* segment,
                                                vtkMRMLAstroVolumeNode *inputVolume,
                                                double outputExtent[6]);

  /// Calculate STD given a ROI node
  virtual double CalculateDisplayThresholdInROI(vtkMRMLMarkupsROINode* roiNode,
                                                  vtkMRMLAstroVolumeNode *inputVolume);

  /// Calculate an histogram of a astroVolumeNode
  virtual void CalculateHistogram(vtkMRMLAstroVolumeNode *Volume,
                                  vtkIntArray *histoArray,
                                  double binSpacing,
                                  int numberOfBins);

  /// Histogram of the finite voxels of a volume (at most maxSamples voxels,
  /// sampled regularly): bins between the minimum and the maximum, centres
  /// and counts. Returns false without finite voxels.
  static bool ComputeHistogram(vtkMRMLVolumeNode* volume, int bins, vtkDoubleArray* centres,
                               vtkDoubleArray* counts, vtkIdType maxSamples = 2000000);
  /// Histogram of the finite voxels between minimum and maximum (e.g. a
  /// zoomed value axis): bins equal width over [minimum, maximum], voxels
  /// outside are not counted
  static bool ComputeHistogramInRange(vtkMRMLVolumeNode* volume, int bins, double minimum, double maximum,
                                      vtkDoubleArray* centres, vtkDoubleArray* counts,
                                      vtkIdType maxSamples = 2000000);

  /// Window of the 2D colours with a percentage of the negative and of the
  /// positive values of a volume (the extreme values outside; the SlicerAstro
  /// clipping); 100: the minimum and maximum. window: lower, upper.
  static bool ComputeClippedWindow(vtkMRMLVolumeNode* volume, double percentage, double window[2],
                                   vtkIdType maxSamples = 2000000);

  /// The 2D views showing the volume (or a volume with FitSlices) fit their
  /// field of view to the grid of the data (channel maps, position-velocity
  /// slices), centred on it; the other views fit to all.
  void FitSlicesToVolume(vtkMRMLAstroVolumeNode* volume);

  /// Reprojection with the parameters (service of the AstroReproject module,
  /// which sets the function: the AstroVolume module does not depend on it).
  /// The output volume ID is set in the parameters node. On failure the
  /// reason is in GetLastReprojectError ("cancelled" if cancelled).
  bool Reproject(vtkMRMLAstroReprojectParametersNode *pnode);
  std::string GetLastReprojectError() { return this->LastReprojectError; }


#ifndef __VTK_WRAP__
  typedef std::function<bool(vtkMRMLAstroReprojectParametersNode* pnode, std::string& error)> ReprojectFunctionType;
  void SetReprojectFunction(const ReprojectFunctionType& function);
#endif

#ifndef __VTK_WRAP__
  /// Runner of long computations (e.g. a worker thread, the GUI responsive
  /// with a progress dialog): runs work and returns when it is done. label
  /// describes the work, progress returns its progress (0-100) and cancel
  /// requests to stop it. The work must not modify the scene (no MRML events
  /// from a worker thread).
  typedef std::function<void(const std::string& label, const std::function<void()>& work,
                             const std::function<int()>& progress,
                             const std::function<void()>& cancel)> BackgroundRunnerType;
  void SetBackgroundRunner(BackgroundRunnerType runner)
    {
    this->BackgroundRunner = runner;
    }

  /// Run work with the background runner if set, otherwise directly (used
  /// by the logics of all the SlicerAstro modules).
  void RunInBackground(const std::string& label, const std::function<void()>& work,
                       const std::function<int()>& progress, const std::function<void()>& cancel)
    {
    if (this->BackgroundRunner)
      {
      this->BackgroundRunner(label, work, progress, cancel);
      }
    else
      {
      work();
      }
    }

  /// RunInBackground with the progress and the cancel request in the Status
  /// of progressNode (any node with GetStatus/SetStatus, -1: cancel), e.g. an
  /// unobserved copy of a parameters node used by ProgressReporter.
  template <class ProgressNodeType>
  void RunInBackground(const std::string& label, const std::function<void()>& work,
                       ProgressNodeType* progressNode)
    {
    this->RunInBackground(label, work,
      [progressNode]() { return progressNode->GetStatus(); },
      [progressNode]() { progressNode->SetStatus(-1); });
    }
#endif

protected:
  vtkSlicerAstroVolumeLogic();
  virtual ~vtkSlicerAstroVolumeLogic();

  virtual void SetMRMLSceneInternal(vtkMRMLScene * newScene) override;
  virtual void UpdateFromMRMLScene() override;

  /// Register MRML Node classes to Scene. Gets called automatically when the MRMLScene is attached to this logic class.
  virtual void RegisterNodes() override;

  /// Handle MRML node added events
  virtual void OnMRMLSceneNodeAdded(vtkMRMLNode* node) override;

  /// Handle MRML node removed events
  virtual void OnMRMLSceneNodeRemoved(vtkMRMLNode* node) override;

  /// Segmentations: FITS mask storage (see EnsureFITSMaskStorageNode).
  /// Volume properties: minimum gap of the transfer functions (see
  /// EnsureVolumePropertyTransferFunctionGaps).
  void ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData) override;

  /// Transfer functions of a volume property (any module, preset or user
  /// edit) without points closer than 1/10000 of the range of the data of
  /// the volumes rendered with it (and of the functions): the lookup table
  /// textures of the GPU ray cast mapper stay small.
  void EnsureVolumePropertyTransferFunctionGaps(vtkMRMLVolumePropertyNode* volumePropertyNode);
  bool UpdatingTransferFunctionGaps{false};
  bool EnsuringVolumeSegmentations{false};
#ifndef __VTK_WRAP__
  BackgroundRunnerType BackgroundRunner;
  bool IncompatibleSceneImported{false};
  std::string LastMaskCopyError;
  std::string LastMaskSaveError;
  ReprojectFunctionType ReprojectFunction;
#endif
  std::string LastReprojectError;
  std::string LastContoursMessage;
  bool PresetsLoaded{false};

  /// Measurements of the markups computed again when the active volume
  /// changes (its WCS is used)
  void UpdateAllAstroMeasurements();
  void ObserveSelectionNode();
  std::string MeasurementsActiveVolumeID;

  /// Handle MRML node scene ended events
  virtual void OnMRMLSceneEndImport() override;
  virtual void OnMRMLSceneEndClose() override;

  /// Load presets for the 3D color function from xml file
  bool LoadPresets(vtkMRMLScene* scene);

  vtkMRMLScene* PresetsScene;
  bool Init;
  vtkWeakPointer<vtkSlicerTerminologiesModuleLogic> TerminologiesLogic;
  vtkWeakPointer<vtkSlicerVolumeRenderingLogic> VolumeRenderingLogic;

private:

  vtkSlicerAstroVolumeLogic(const vtkSlicerAstroVolumeLogic&); // Not implemented
  void operator=(const vtkSlicerAstroVolumeLogic&);               // Not implemented
};

#endif

