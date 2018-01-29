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

/// \ingroup Slicer_QtModules_AstroMasking
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroMaskingParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroMaskingParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroMaskingParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual vtkMRMLNode* CreateNodeInstance() VTK_OVERRIDE;

  // Description:
  // Set node attributes
  virtual void ReadXMLAttributes( const char** atts) VTK_OVERRIDE;

  // Description:
  // Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) VTK_OVERRIDE;

  // Description:
  // Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) VTK_OVERRIDE;

  // Description:
  // Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroMaskingParameters";};

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(MaskVolumeNodeID);
  vtkGetStringMacro(MaskVolumeNodeID);

  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  vtkMRMLAnnotationROINode* GetROINode();
  void SetROINode(vtkMRMLAnnotationROINode* node);

  vtkSetStringMacro(Mode);
  vtkGetStringMacro(Mode);

  vtkSetStringMacro(Operation);
  vtkGetStringMacro(Operation);

  vtkSetStringMacro(BlankRegion);
  vtkGetStringMacro(BlankRegion);

  vtkSetStringMacro(BlankValue);
  vtkGetStringMacro(BlankValue);

  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

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

  int Cores;
  int OutputSerial;
  int Status;
};

#endif
