#ifndef __vtkMRMLAstroLabelMapVolumeNode_h
#define __vtkMRMLAstroLabelMapVolumeNode_h

// MRML includes
#include "vtkMRMLLabelMapVolumeNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

// WCS includes
#include "wcslib.h"

class vtkMRMLAstroLabelMapVolumeDisplayNode;

/// \brief MRML node for representing a label map volume.
///
/// A label map volume is typically the output of a segmentation procedure that
/// labels each voxel according to its segment (e.g., a certain type of tissue).
///
/// /// \ingroup Slicer_QtModules_AstroVolumeNode
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroLabelMapVolumeNode : public vtkMRMLLabelMapVolumeNode
{
  public:
  static vtkMRMLAstroLabelMapVolumeNode *New();
  vtkTypeMacro(vtkMRMLAstroLabelMapVolumeNode,vtkMRMLLabelMapVolumeNode);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();

  ///
  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts);

  ///
  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  ///
  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node);

  ///
  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() {return "AstroLabelMapVolume";};

  ///
  /// Create and observe default display node
  virtual void CreateDefaultDisplayNodes();

  ///
  /// Create and observe default Storage node
  virtual vtkMRMLStorageNode* CreateDefaultStorageNode();

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
