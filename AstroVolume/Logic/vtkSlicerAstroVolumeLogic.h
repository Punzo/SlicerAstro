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

class vtkMRMLAnnotationROINode;
class vtkMRMLAstroLabelMapVolumeNode;
class vtkMRMLAstroReprojectParametersNode;
class vtkMRMLAstroVolumeNode;
class vtkMRMLSegmentationNode;
class vtkMRMLVolumeNode;
class vtkSegment;
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

  /// Sets ROI to fit to input volume.
  /// If ROI is under a non-linear transform then the ROI transform will be reset to RAS.
  /// \param MRML parameter node
  /// \return Success flag
  virtual bool FitROIToInputVolume(vtkMRMLAnnotationROINode* roiNode,
                                   vtkMRMLAstroVolumeNode *inputVolume);

  /// Snap the ROI to the voxel grid of the input volume
  /// \param MRML parameter node
  virtual void SnapROIToVoxelGrid(vtkMRMLAnnotationROINode* roiNode,
                                  vtkMRMLAstroVolumeNode *inputVolume);

  /// Check if the ROI is aligned with the input volume
  /// \param MRML parameter node
  /// \return Success flag
  virtual bool IsROIAlignedWithInputVolume(vtkMRMLAnnotationROINode* roiNode,
                                           vtkMRMLAstroVolumeNode *inputVolume);

  /// Calculate the Bounds of a ROI within a Volume in IJK coordinates
  /// \param MRML ROI node
  /// \param MRML input volume node
  /// \param extent (i.e., dimensions)
  /// \return Success flag
  virtual bool CalculateROICropVolumeBounds(vtkMRMLAnnotationROINode* roiNode,
                                            vtkMRMLAstroVolumeNode *inputVolume,
                                            double outputExtent[6]);

  /// Calculate the Bounds of a Segment within a Volume in IJK coordinates
  virtual bool CalculateSegmentCropVolumeBounds(vtkMRMLSegmentationNode *segmentationNode,
                                                vtkSegment* segment,
                                                vtkMRMLAstroVolumeNode *inputVolume,
                                                double outputExtent[6]);

  /// Calculate STD given a ROI node
  virtual double CalculateDisplayThresholdInROI(vtkMRMLAnnotationROINode* roiNode,
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

  /// Handle MRML node scene ended events
  virtual void OnMRMLSceneEndImport() override;

  /// Load presets for the 3D color function from xml file
  bool LoadPresets(vtkMRMLScene* scene);

  vtkMRMLScene* PresetsScene;
  bool Init;

private:

  vtkSlicerAstroVolumeLogic(const vtkSlicerAstroVolumeLogic&); // Not implemented
  void operator=(const vtkSlicerAstroVolumeLogic&);               // Not implemented
};

#endif

