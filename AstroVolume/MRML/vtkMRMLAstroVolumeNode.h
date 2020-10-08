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

class vtkMRMLAnnotationROINode;
class vtkMRMLAstroVolumeDisplayNode;
class vtkMRMLAstroLabelMapVolumeNode;
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
  vtkMRMLAnnotationROINode* GetROINode();

  /// Set MRML ROI node
  void SetROINode(vtkMRMLAnnotationROINode* node);

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

  enum
     {
      LowConstantOpacityPreset,
      MediumConstantOpacityPreset,
      HighConstantOpacityPreset,
      OneSurfaceGreenPreset,
      OneSurfaceWhitePreset,
      TwoSurfacesPreset,
      ThreeSurfacesPreset,
      BrightSurface
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

  vtkMRMLAstroVolumeNode(const vtkMRMLAstroVolumeNode&);
  void operator=(const vtkMRMLAstroVolumeNode&);
};

#endif
