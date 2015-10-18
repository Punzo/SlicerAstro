#ifndef __vtkMRMLAstroSliceNode_h
#define __vtkMRMLAstroSliceNode_h

// MRML includes
#include "vtkMRMLSliceNode.h"
//#include <vtkSlicerAstroVolumeModuleMRMLExport.h>
class vtkMRMLVolumeNode;

// VTK includes
class vtkMatrix4x4;

class VTK_MRML_EXPORT vtkMRMLAstroSliceNode : public vtkMRMLSliceNode
{
  public:
  static vtkMRMLAstroSliceNode *New();
  vtkTypeMacro(vtkMRMLAstroSliceNode,vtkMRMLSliceNode);
  //void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();
/*
  ///
  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts);

  ///
  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  ///
  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node);*/

  ///
  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() {return "AstroSlice";};
/*
  ///
  /// 'standard' radiological convention views of patient space
  /// these calls adjust the SliceToRAS matrix to position the slice
  /// cutting plane
  void SetOrientationToXZ();
  void SetOrientationToZY();
  void SetOrientationToXY();

  ///
  /// General 'reformat' view that allows for multiplanar reformat
  void SetOrientationToReformat();

  /// Convenient function that calls SetOrientationToXZ(),
  /// SetOrientationToZY(), SetOrientationToXY() or
  /// SetOrientationToReformat() depending on the value of the string
  void SetOrientation(const char* orientation);

  ///
  /// Recalculate XYToSlice and XYToRAS in terms or fov, dim, SliceToRAS
  /// - called when any of the inputs change
  void UpdateMatrices();

  ///
  /// adjusts the slice node to align with the
  /// native space of the image data so that no oblique resampling
  /// occurs when rendering (helps to see original acquisition data
  /// and for obluique volumes with few slices).
  void RotateToVolumePlane(vtkMRMLVolumeNode *volumeNode);*/

protected:

  vtkMRMLAstroSliceNode();
  ~vtkMRMLAstroSliceNode();
  vtkMRMLAstroSliceNode(const vtkMRMLAstroSliceNode&);
  void operator=(const vtkMRMLAstroSliceNode&);

};

#endif
