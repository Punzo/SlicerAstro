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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

#ifndef __vtkMRMLAstroLabelMapVolumeNode_h
#define __vtkMRMLAstroLabelMapVolumeNode_h

// MRML includes
#include "vtkMRMLLabelMapVolumeNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLAstroLabelMapVolumeDisplayNode;

/// \brief MRML node for representing a label map volume.
///
/// A label map volume is typically the output of a segmentation procedure that
/// labels each voxel according to its segment
///
/// /// \ingroup Slicer_QtModules_AstroVolumeNode
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroLabelMapVolumeNode : public vtkMRMLLabelMapVolumeNode
{
  public:
  static vtkMRMLAstroLabelMapVolumeNode *New();
  vtkTypeMacro(vtkMRMLAstroLabelMapVolumeNode,vtkMRMLLabelMapVolumeNode);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual vtkMRMLNode* CreateNodeInstance() VTK_OVERRIDE;

  ///
  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) VTK_OVERRIDE;

  ///
  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) VTK_OVERRIDE;

  ///
  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) VTK_OVERRIDE;

  ///
  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroLabelMapVolume";};

  ///
  /// Create and observe default display node
  virtual void CreateDefaultDisplayNodes() VTK_OVERRIDE;

  ///
  /// Make a 'None' volume node with blank image data
  static void CreateNoneNode(vtkMRMLScene *scene);

  ///
  /// Create and observe default Storage node
  virtual vtkMRMLStorageNode* CreateDefaultStorageNode() VTK_OVERRIDE;

  ///
  /// Get AstroVolume display node
  virtual vtkMRMLAstroLabelMapVolumeDisplayNode* GetAstroLabelMapVolumeDisplayNode();

  ///
  /// Update Max and Min Attributes
  virtual void UpdateRangeAttributes();

protected:
  vtkMRMLAstroLabelMapVolumeNode();
  ~vtkMRMLAstroLabelMapVolumeNode();
  vtkMRMLAstroLabelMapVolumeNode(const vtkMRMLAstroLabelMapVolumeNode&);
  void operator=(const vtkMRMLAstroLabelMapVolumeNode&);
};

#endif
