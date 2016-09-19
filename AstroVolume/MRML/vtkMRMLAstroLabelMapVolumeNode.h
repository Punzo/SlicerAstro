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
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();

  ///
  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() {return "AstroLabelMapVolume";};

  ///
  /// Create and observe default display node
  virtual void CreateDefaultDisplayNodes();

  ///
  /// Make a 'None' volume node with blank image data
  static void CreateNoneNode(vtkMRMLScene *scene);

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
