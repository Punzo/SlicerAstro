#ifndef __vtkMRMLAstroVolumeNode_h
#define __vtkMRMLAstroVolumeNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLScalarVolumeNode.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkSmartPointer.h>

// WCS includes
#include "wcslib.h"

//#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLAstroVolumeDisplayNode;

/// \ingroup Slicer_QtModules_AstroVolumeNode
class VTK_MRML_EXPORT vtkMRMLAstroVolumeNode : public vtkMRMLScalarVolumeNode
{
  public:

  static vtkMRMLAstroVolumeNode *New();
  vtkTypeMacro(vtkMRMLAstroVolumeNode,vtkMRMLScalarVolumeNode);
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
  virtual const char* GetNodeTagName() {return "AstroVolume";};

  ///
  ///Set WCSStruct
  virtual void SetWCSStruct(struct wcsprm*);

  ///
  ///WcsStatus
  vtkSetMacro(WCSStatus,int);
  vtkGetMacro(WCSStatus,int);

  ///
  /// Create and observe default Storage node
  virtual vtkMRMLStorageNode* CreateDefaultStorageNode();

  ///
  /// Create and observe default display node
  virtual void CreateDefaultDisplayNodes();
  
  ///
  /// Get AstroVolume display node
  virtual vtkMRMLAstroVolumeDisplayNode* GetAstroVolumeDisplayNode();

  ///
  ///Get WCSCoordinates
  virtual void GetReferenceSpace(const double ijk[3],
                                 const char* Space,
                                 double SpaceCoordinates[3]);

protected:
  vtkMRMLAstroVolumeNode();
  virtual ~vtkMRMLAstroVolumeNode();

  vtkMRMLAstroVolumeNode(const vtkMRMLAstroVolumeNode&);
  void operator=(const vtkMRMLAstroVolumeNode&);

  struct wcsprm* WCS;
  int WCSStatus;

};

#endif
