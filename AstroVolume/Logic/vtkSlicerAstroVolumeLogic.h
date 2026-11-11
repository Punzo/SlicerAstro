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
#include <vtkSlicerVolumesLogic.h>

// STD includes
#include <cstdlib>

#include "vtkSlicerAstroVolumeModuleLogicExport.h"

class vtkMRMLMarkupsROINode;
class vtkMRMLAstroLabelMapVolumeNode;
class vtkMRMLAstroReprojectParametersNode;
class vtkMRMLAstroVolumeNode;
class vtkMRMLSegmentationNode;
class vtkMRMLSliceNode;
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
  /// "<volume>_SEG", unique in the scene. Returns true if renamed.
  static bool NameMaskAfterReferenceVolume(vtkMRMLSegmentationNode* segmentationNode);

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

  /// After adding the contour levels: closed surfaces as source and displayed
  /// representation in 2D and 3D, the binary labelmaps are removed.
  static void SetClosedSurfaceContours(vtkMRMLSegmentationNode* segmentationNode);

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

  /// Reproject an astroVolumeNode over another
  bool Reproject(vtkMRMLAstroReprojectParametersNode *pnode);

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

  /// Segmentations: FITS mask storage (see EnsureFITSMaskStorageNode)
  void ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData) override;

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

