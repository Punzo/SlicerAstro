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

// STD includes
#include <cstdlib>

#include "vtkSlicerAstroVolumeModuleLogicExport.h"

class vtkMRMLCameraNode;
class vtkMRMLMarkupsNode;
class vtkMRMLMarkupsROINode;
class vtkMRMLAstroLabelMapVolumeNode;
class vtkMRMLAstroReprojectParametersNode;
class vtkMRMLAstroVolumeNode;
class vtkMRMLSegmentationNode;
class vtkMRMLSliceNode;
class vtkMRMLVolumePropertyNode;
class vtkMRMLVolumeNode;
class vtkSegment;
class vtkVolumeProperty;
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

  /// Create a label map volume to match the given \a volumeNode and add it to the \a scene
  vtkMRMLAstroLabelMapVolumeNode *CreateAndAddLabelVolume(vtkMRMLScene *scene,
                                                          vtkMRMLAstroVolumeNode *volumeNode,
                                                          const char *name);

  /// Widen the gaps between the points of the scalar opacity and color
  /// transfer functions of \a volumeProperty to at least 1/10000 of the
  /// scalar range. The default transfer functions of Slicer keep duplicate
  /// points apart with a gap of the order of the float precision: with the
  /// small intensity ranges of astronomical data (e.g. 0.01 Jy/beam) the
  /// lookup tables of the volume rendering would need textures wider than
  /// the GPU allows. Returns true if a transfer function was modified.
  static bool EnsureMinimumTransferFunctionGap(vtkVolumeProperty* volumeProperty, const double scalarRange[2]);

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
  /// Opacity of the masks created by SlicerAstro (the opacity of a mask read
  /// from a FITS file is the one saved in the file, keyword SEGOPAC)
  static double GetDefaultMaskOpacity() { return 0.3; }

  /// A mask with segments (or read from a file) whose reference geometry is
  /// an astronomical volume replaces the empty mask created automatically for
  /// that volume (e.g. the mask of a data cube loaded after the cube).
  /// Returns true if replaced.
  bool ReplaceEmptyMaskOfReferenceVolume(vtkMRMLSegmentationNode* segmentationNode);

  /// True if the segmentation has the geometry (voxel grid) of the volume.
  static bool HasGeometryOfVolume(vtkMRMLSegmentationNode* segmentationNode, vtkMRMLVolumeNode* volumeNode);

  /// The astronomical volume with the geometry of a mask without reference
  /// volume (e.g. a FITS mask loaded after its data cube), nullptr if none.
  static vtkMRMLAstroVolumeNode* FindVolumeOfMask(vtkMRMLScene* scene, vtkMRMLSegmentationNode* segmentationNode);

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
  ReprojectFunctionType ReprojectFunction;
#endif
  std::string LastReprojectError;

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

private:

  vtkSlicerAstroVolumeLogic(const vtkSlicerAstroVolumeLogic&); // Not implemented
  void operator=(const vtkSlicerAstroVolumeLogic&);               // Not implemented
};

#endif

