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
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual vtkMRMLNode* CreateNodeInstance() VTK_OVERRIDE;

  /// Read node attributes from XML file
  virtual void ReadXMLAttributes( const char** atts) VTK_OVERRIDE;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) VTK_OVERRIDE;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) VTK_OVERRIDE;

  /// Get node XML tag name (like Storage, Model)
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroStorage";};

  /// Set/Get the CenterImage.
  /// Default is 2.
  /// \sa CenterImage(), CenterImage()
  vtkGetMacro(CenterImage, int);
  vtkSetMacro(CenterImage, int);

  /// Return true if the node can be read in.
  virtual bool CanReadInReferenceNode(vtkMRMLNode *refNode) VTK_OVERRIDE;

  /// Configure the storage node for data exchange. This is an
  /// opportunity to optimize the storage node's settings, for
  /// instance to turn off compression.
  virtual void ConfigureForDataExchange() VTK_OVERRIDE;

protected:
  vtkMRMLAstroVolumeStorageNode();
  ~vtkMRMLAstroVolumeStorageNode();
  vtkMRMLAstroVolumeStorageNode(const vtkMRMLAstroVolumeStorageNode&);
  void operator=(const vtkMRMLAstroVolumeStorageNode&);

  /// Initialize all the supported write file types
  virtual void InitializeSupportedReadFileTypes() VTK_OVERRIDE;

  /// Initialize all the supported write file types
  virtual void InitializeSupportedWriteFileTypes() VTK_OVERRIDE;

  /// Read data and set it in the referenced node
  virtual int ReadDataInternal(vtkMRMLNode *refNode) VTK_OVERRIDE;

  /// Write data from a  referenced node
  virtual int WriteDataInternal(vtkMRMLNode *refNode) VTK_OVERRIDE;

  int CenterImage;
};

#endif
