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

#ifndef __vtkMRMLAstroLabelMapVolumeNode_h
#define __vtkMRMLAstroLabelMapVolumeNode_h

// MRML includes
#include "vtkMRMLLabelMapVolumeNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLAstroLabelMapVolumeDisplayNode;

/// \brief MRML node for representing an astro label map volume.
///
/// A label map volume is typically the output of a segmentation procedure that
/// labels each voxel according to its segment
///
/// \ingroup SlicerAstro_QtModules_AstroVolumeNode
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroLabelMapVolumeNode : public vtkMRMLLabelMapVolumeNode
{
  public:
  static vtkMRMLAstroLabelMapVolumeNode *New();
  vtkTypeMacro(vtkMRMLAstroLabelMapVolumeNode,vtkMRMLLabelMapVolumeNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroLabelMapVolume";};

  /// Create and observe default display node
  virtual void CreateDefaultDisplayNodes() override;

  /// Make a 'None' volume node with blank image data
  static void CreateNoneNode(vtkMRMLScene *scene);

  /// Create and observe default Storage node
  virtual vtkMRMLStorageNode* CreateDefaultStorageNode() override;

  /// Get AstroVolume display node
  virtual vtkMRMLAstroLabelMapVolumeDisplayNode* GetAstroLabelMapVolumeDisplayNode();

  /// Update Max and Min Attributes
  virtual bool UpdateRangeAttributes();

protected:
  vtkMRMLAstroLabelMapVolumeNode();
  ~vtkMRMLAstroLabelMapVolumeNode() override;
  vtkMRMLAstroLabelMapVolumeNode(const vtkMRMLAstroLabelMapVolumeNode&);
  void operator=(const vtkMRMLAstroLabelMapVolumeNode&);
};

#endif
