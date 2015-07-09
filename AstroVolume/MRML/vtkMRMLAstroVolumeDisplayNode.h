#ifndef __vtkMRMLAstroVolumeDisplayNode_h
#define __vtkMRMLAstroVolumeDisplayNode_h

// MRML includes
#include "vtkMRMLScalarVolumeDisplayNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

// VTK includes
class vtkAlgorithmOutput;
class vtkImageData;

class VTK_MRMLASTRO_EXPORT vtkMRMLAstroVolumeDisplayNode : public vtkMRMLScalarVolumeDisplayNode
{
  public:
  static vtkMRMLAstroVolumeDisplayNode *New();
  vtkTypeMacro(vtkMRMLAstroVolumeDisplayNode,vtkMRMLScalarVolumeDisplayNode);
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
  virtual const char* GetNodeTagName() {return "AstroVolumeDisplay";};


protected:
  vtkMRMLAstroVolumeDisplayNode();
  ~vtkMRMLAstroVolumeDisplayNode();
  vtkMRMLAstroVolumeDisplayNode(const vtkMRMLAstroVolumeDisplayNode&);
  void operator=(const vtkMRMLAstroVolumeDisplayNode&);




};

#endif

