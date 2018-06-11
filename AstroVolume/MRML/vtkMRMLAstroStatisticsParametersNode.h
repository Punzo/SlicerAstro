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

#ifndef __vtkMRMLAstroStatisticsParametersNode_h
#define __vtkMRMLAstroStatisticsParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLAnnotationROINode;
class vtkMRMLTableNode;
class vtkMRMLTransformNode;

/// \ingroup Slicer_QtModules_AstroStatistics
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroStatisticsParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroStatisticsParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroStatisticsParametersNode,vtkMRMLNode);
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
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroStatisticsParameters";};

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(MaskVolumeNodeID);
  vtkGetStringMacro(MaskVolumeNodeID);

  vtkMRMLAnnotationROINode* GetROINode();
  void SetROINode(vtkMRMLAnnotationROINode* node);

  vtkMRMLTableNode* GetTableNode();
  void SetTableNode(vtkMRMLTableNode* node);

  vtkSetStringMacro(Mode);
  vtkGetStringMacro(Mode);

  vtkSetMacro(Max,bool);
  vtkGetMacro(Max,bool);
  vtkBooleanMacro(Max,bool);

  vtkSetMacro(Mean,bool);
  vtkGetMacro(Mean,bool);
  vtkBooleanMacro(Mean,bool);

  vtkSetMacro(Median,bool);
  vtkGetMacro(Median,bool);
  vtkBooleanMacro(Median,bool);

  vtkSetMacro(Min,bool);
  vtkGetMacro(Min,bool);
  vtkBooleanMacro(Min,bool);

  vtkSetMacro(Npixels,bool);
  vtkGetMacro(Npixels,bool);
  vtkBooleanMacro(Npixels,bool);

  vtkSetMacro(Std,bool);
  vtkGetMacro(Std,bool);
  vtkBooleanMacro(Std,bool);

  vtkSetMacro(Sum,bool);
  vtkGetMacro(Sum,bool);
  vtkBooleanMacro(Sum,bool);

  vtkSetMacro(TotalFlux,bool);
  vtkGetMacro(TotalFlux,bool);
  vtkBooleanMacro(TotalFlux,bool);

  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

protected:
  vtkMRMLAstroStatisticsParametersNode();
  ~vtkMRMLAstroStatisticsParametersNode();

  vtkMRMLAstroStatisticsParametersNode(const vtkMRMLAstroStatisticsParametersNode&);
  void operator=(const vtkMRMLAstroStatisticsParametersNode&);

  static const char* TABLE_REFERENCE_ROLE;
  const char *GetTableNodeReferenceRole();

  static const char* ROI_REFERENCE_ROLE;
  const char *GetROINodeReferenceRole();

  static const char* ROI_ALIGNMENTTRANSFORM_REFERENCE_ROLE;
  const char *GetROIAlignmentTransformNodeReferenceRole();

  char *InputVolumeNodeID;
  char *MaskVolumeNodeID;
  char *Mode;

  int Cores;

  bool Max;
  bool Mean;
  bool Median;
  bool Min;
  bool Npixels;
  bool Std;
  bool Sum;
  bool TotalFlux;

  int OutputSerial;

  int Status;
};

#endif
