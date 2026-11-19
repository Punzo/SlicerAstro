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

#ifndef __vtkMRMLAstroVolumeNode_h
#define __vtkMRMLAstroVolumeNode_h


// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScalarVolumeNode.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkSmartPointer.h>

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLMarkupsROINode;
class vtkMRMLAstroVolumeDisplayNode;
class vtkMRMLTransformNode;
class vtkMRMLVolumePropertyNode;
class vtkMRMLVolumeRenderingDisplayNode;

/// \brief MRML node for representing an astro volume.
///
/// An astro volume is loaded from fits file.
///
/// \ingroup SlicerAstro_QtModules_AstroVolumeNode
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroVolumeNode : public vtkMRMLScalarVolumeNode
{
  public:

  static vtkMRMLAstroVolumeNode *New();
  vtkTypeMacro(vtkMRMLAstroVolumeNode,vtkMRMLScalarVolumeNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroVolume";};

  /// Scene format of SlicerAstro, written in the scene with the volume:
  /// 2 since SlicerAstro 2.0 (masks are segmentations). Scenes of older
  /// versions (masks as label map volumes, other parameters nodes) have no
  /// format: they are not compatible.
  static int GetCurrentSceneFormat() { return 2; }

  /// Scene format of the scene the volume was read from (0: an older
  /// SlicerAstro); the current format for volumes not read from a scene.
  vtkGetMacro(SceneFormat, int);

  /// Make a 'None' volume node with blank image data
  static void CreateNoneNode(vtkMRMLScene *scene);

  /// Create and return default Storage node
  virtual vtkMRMLStorageNode* CreateDefaultStorageNode() override;

  /// Create and observe default display node
  virtual void CreateDefaultDisplayNodes() override;
  
  /// Get AstroVolume display node
  virtual vtkMRMLAstroVolumeDisplayNode* GetAstroVolumeDisplayNode();

  /// Get rendering display node
  virtual vtkMRMLVolumeRenderingDisplayNode* GetAstroVolumeRenderingDisplayNode();

  /// Get MRML ROI node
  vtkMRMLMarkupsROINode* GetROINode();

  /// Set MRML ROI node
  void SetROINode(vtkMRMLMarkupsROINode* node);

  /// Get MRML ROI alignment transform node
  vtkMRMLTransformNode* GetROIAlignmentTransformNode();

  /// Set MRML ROI alignment transform node ID
  void SetROIAlignmentTransformNodeID(const char *nodeID);

  /// Set MRML ROI alignment transform node
  void SetROIAlignmentTransformNode(vtkMRMLTransformNode* node);

  /// Delete MRML ROI alignment transform node
  void DeleteROIAlignmentTransformNode();

  /// Update Max and Min Attributes
  virtual bool UpdateRangeAttributes();

  /// Range of the 2D display (window/level) of the data: the minimum and
  /// maximum, except when they are isolated outliers (e.g. stars or bright
  /// HII regions in optical data: the maximum more than 10 times the 99.9th
  /// percentile), which are replaced by the percentile (0.1th for the
  /// minimum). Percentiles from a sample of at most 10^6 finite voxels.
  /// Returns false if the volume has no finite voxels.
  bool GetDisplayRange(double range[2]);

  /// Update DisplayThreshold Attribute
   virtual bool UpdateDisplayThresholdAttributes();

  enum
     {
     DisplayThresholdModifiedEvent = 71000,
     };

  /// Set the SlicerAstro.DisplayThreshold keyword and fire the signal
  void SetDisplayThreshold(double DisplayThreshold);

  /// Get the SlicerAstro.DisplayThreshold keyword
  double GetDisplayThreshold();

  /// Set reference to a Preset Node
  void SetPresetNode(vtkMRMLVolumePropertyNode* node);

  /// Set reference to a Preset Node
  void SetPresetNode(vtkMRMLNode* node);

  /// Get reference to a Preset Node
  vtkMRMLNode *GetPresetNode();

  /// Utility method to get the PresetComboBoxIndex from the PresetNode
  int GetPresetIndex();

  /// Identifier of a 3D colour function (preset): the attribute
  /// SlicerAstro.Preset (e.g. "ThreeSurfaces"), independent of the name shown
  /// to the user (e.g. "Surfaces at 3σ, 7σ, 15σ"); the name for nodes
  /// without the attribute
  static std::string GetPresetIdentifier(vtkMRMLNode* presetNode);
  static const char* GetPresetIdentifierAttributeName() { return "SlicerAstro.Preset"; }

  /// The 3D colour functions, in the order of presets.xml (the index in the
  /// preset selector)
  enum
     {
      LowConstantOpacityPreset,
      MediumConstantOpacityPreset,
      HighConstantOpacityPreset,
      OneSurfaceGreenPreset,
      OneSurfaceWhitePreset,
      TwoSurfacesPreset,
      ThreeSurfacesPreset,
      BrightSurface,
      ObservatorySurfacesPreset,
      ObservatoryShellsPreset,
      ObservatoryEmissionPreset,
      MagmaEmissionPreset,
      ViridisEmissionPreset,
      /// surfaces of emission and of absorption (negative values)
      EmissionAbsorptionPreset,
      /// default of the data: the surfaces at 3, 7 and 15 sigma in colours
      /// safe for colour-blind people
      DefaultPreset = ObservatorySurfacesPreset
     };

  /// Set reference to the current VolumeProperty Node
  void SetVolumePropertyNode(vtkMRMLVolumePropertyNode* node);

  /// Set reference to the current VolumeProperty Node
  void SetVolumePropertyNode(vtkMRMLNode* node);

  /// Get reference to the current VolumeProperty Node
  vtkMRMLNode *GetVolumePropertyNode();

protected:
  vtkMRMLAstroVolumeNode();
  ~vtkMRMLAstroVolumeNode() override;

  static const char* PRESET_REFERENCE_ROLE;
  const char *GetPresetNodeReferenceRole();

  static const char* VOLUMEPROPERTY_REFERENCE_ROLE;
  const char *GetVolumePropertyNodeReferenceRole();

  static const char* ROI_REFERENCE_ROLE;
  const char *GetROINodeReferenceRole();

  static const char* ROI_ALIGNMENTTRANSFORM_REFERENCE_ROLE;
  const char *GetROIAlignmentTransformNodeReferenceRole();

  int SceneFormat;

  vtkMRMLAstroVolumeNode(const vtkMRMLAstroVolumeNode&);
  void operator=(const vtkMRMLAstroVolumeNode&);
};

#endif
