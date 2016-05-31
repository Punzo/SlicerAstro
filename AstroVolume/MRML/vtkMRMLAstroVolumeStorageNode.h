#ifndef __vtkMRMLAstroVolumeStorageNode_h
#define __vtkMRMLAstroVolumeStorageNode_h

#include "vtkMRMLStorageNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

/// \brief MRML node for representing a volume storage.
///
/// vtkMRMLAstroVolumeStorageNode nodes describe the archetybe based volume storage
/// node that allows to read/write volume data from/to AstroVolume file
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroVolumeStorageNode : public vtkMRMLStorageNode
{
  public:
  static vtkMRMLAstroVolumeStorageNode *New();
  vtkTypeMacro(vtkMRMLAstroVolumeStorageNode,vtkMRMLStorageNode);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();

  ///
  /// Read node attributes from XML file
  virtual void ReadXMLAttributes( const char** atts);

  ///
  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  ///
  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node);

  ///
  /// Get node XML tag name (like Storage, Model)
  virtual const char* GetNodeTagName()  {return "AstroStorage";};

  ///
  /// Center image on read
  vtkGetMacro(CenterImage, int);
  vtkSetMacro(CenterImage, int);

  ///
  /// Return a default file extension for writting
  virtual const char* GetDefaultWriteFileExtension();

  /// Return true if the node can be read in.
  virtual bool CanReadInReferenceNode(vtkMRMLNode *refNode);

  ///
  /// Configure the storage node for data exchange. This is an
  /// opportunity to optimize the storage node's settings, for
  /// instance to turn off compression.
  virtual void ConfigureForDataExchange();

protected:
  vtkMRMLAstroVolumeStorageNode();
  ~vtkMRMLAstroVolumeStorageNode();
  vtkMRMLAstroVolumeStorageNode(const vtkMRMLAstroVolumeStorageNode&);
  void operator=(const vtkMRMLAstroVolumeStorageNode&);

  /// Initialize all the supported write file types
  virtual void InitializeSupportedReadFileTypes();

  /// Initialize all the supported write file types
  virtual void InitializeSupportedWriteFileTypes();

  /// Read data and set it in the referenced node
  virtual int ReadDataInternal(vtkMRMLNode *refNode);

  /// Write data from a  referenced node
  virtual int WriteDataInternal(vtkMRMLNode *refNode);

  int CenterImage;

};

#endif
