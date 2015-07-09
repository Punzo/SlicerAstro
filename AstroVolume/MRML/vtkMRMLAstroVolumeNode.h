#ifndef __vtkMRMLAstroVolumeNode_h
#define __vtkMRMLAstroVolumeNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLScalarVolumeNode.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkSmartPointer.h>

#include "wcslib.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLAstroVolumeDisplayNode;

/// \ingroup Slicer_QtModules_AstroVolumeNode
class VTK_MRMLASTRO_EXPORT vtkMRMLAstroVolumeNode : public vtkMRMLScalarVolumeNode
{
  public:

  static vtkMRMLAstroVolumeNode *New();
  vtkTypeMacro(vtkMRMLAstroVolumeNode,vtkMRMLScalarVolumeNode);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts);

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node);

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() {return "AstroVolume";};

  ///Set WCSStruct
  void SetWcsStruct(struct wcsprm*);

  /// Update the stored reference to another node in the scene
  //virtual void UpdateReferenceID(const char *oldID, const char *newID);

  /// Updates this node if it depends on other nodes
  /// when the node is deleted in the scene
  //virtual void UpdateReferences();

  // Description:
  //virtual void UpdateScene(vtkMRMLScene *scene);

  //virtual void ProcessMRMLEvents ( vtkObject *caller, unsigned long event, void *callData);
  

  virtual vtkMRMLStorageNode* CreateDefaultStorageNode();
  virtual void CreateDefaultDisplayNodes();
  
  vtkMRMLAstroVolumeDisplayNode* GetAstroVolumeDisplayNode();

protected:
  vtkMRMLAstroVolumeNode();
  virtual ~vtkMRMLAstroVolumeNode();

  vtkMRMLAstroVolumeNode(const vtkMRMLAstroVolumeNode&);
  void operator=(const vtkMRMLAstroVolumeNode&);

  struct wcsprm* wcs;
  int WcsStatus;

};

#endif

