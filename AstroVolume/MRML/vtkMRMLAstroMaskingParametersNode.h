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

#ifndef __vtkMRMLAstroMaskingParametersNode_h
#define __vtkMRMLAstroMaskingParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLAnnotationROINode;
class vtkMRMLTableNode;
class vtkMRMLTransformNode;

/// \ingroup SlicerAstro_QtModules_AstroMasking
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroMaskingParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroMaskingParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroMaskingParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual vtkMRMLNode* CreateNodeInstance() VTK_OVERRIDE;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) VTK_OVERRIDE;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) VTK_OVERRIDE;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) VTK_OVERRIDE;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroMaskingParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the MaskVolumeNodeID.
  /// \sa SetMaskVolumeNodeID(), GetMaskVolumeNodeID()
  vtkSetStringMacro(MaskVolumeNodeID);
  vtkGetStringMacro(MaskVolumeNodeID);

  /// Set/Get the OutputVolumeNodeID.
  /// \sa SetOutputVolumeNodeID(), GetOutputVolumeNodeID()
  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  /// Get MRML ROI node
  vtkMRMLAnnotationROINode* GetROINode();

  /// Set MRML ROI node
  void SetROINode(vtkMRMLAnnotationROINode* node);

  /// Set/Get the Mode.
  /// Default is "ROI"
  /// \sa SetMode(), GetMode()
  vtkSetStringMacro(Mode);
  vtkGetStringMacro(Mode);

  /// Set/Get the Operation.
  /// Default is "Blank"
  /// \sa SetOperation(), GetOperation()
  vtkSetStringMacro(Operation);
  vtkGetStringMacro(Operation);

  /// Set/Get the BlankRegion.
  /// Default is "Outside"
  /// \sa SetBlankRegion(), GetBlankRegion()
  vtkSetStringMacro(BlankRegion);
  vtkGetStringMacro(BlankRegion);

  /// Set/Get the BlankValue.
  /// Default is "NaN"
  /// \sa SetBlankValue(), GetBlankValue()
  vtkSetStringMacro(BlankValue);
  vtkGetStringMacro(BlankValue);

  /// Set/Get the OutputSerial.
  /// \sa SetOutputSerial(), GetOutputSerial()
  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  /// Set/Get the Status.
  /// \sa SetStatus(), GetStatus()
  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

protected:
  vtkMRMLAstroMaskingParametersNode();
  ~vtkMRMLAstroMaskingParametersNode();

  vtkMRMLAstroMaskingParametersNode(const vtkMRMLAstroMaskingParametersNode&);
  void operator=(const vtkMRMLAstroMaskingParametersNode&);

  static const char* ROI_REFERENCE_ROLE;
  const char *GetROINodeReferenceRole();

  static const char* ROI_ALIGNMENTTRANSFORM_REFERENCE_ROLE;
  const char *GetROIAlignmentTransformNodeReferenceRole();

  char *InputVolumeNodeID;
  char *MaskVolumeNodeID;
  char *OutputVolumeNodeID;
  char *Mode;
  char *Operation;
  char *BlankRegion;
  char *BlankValue;

  int OutputSerial;
  int Status;
};

#endif
