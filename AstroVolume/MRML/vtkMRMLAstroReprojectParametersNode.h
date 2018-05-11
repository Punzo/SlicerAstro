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

#ifndef __vtkMRMLAstroReprojectParametersNode_h
#define __vtkMRMLAstroReprojectParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLAnnotationROINode;
class vtkMRMLTableNode;
class vtkMRMLTransformNode;

/// \ingroup Slicer_QtModules_AstroReproject
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroReprojectParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroReprojectParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroReprojectParametersNode,vtkMRMLNode);
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
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroReprojectParameters";};

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(ReferenceVolumeNodeID);
  vtkGetStringMacro(ReferenceVolumeNodeID);

  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  vtkSetMacro(ReprojectRotation,bool);
  vtkGetMacro(ReprojectRotation,bool);

  vtkSetMacro(ReprojectTime,bool);
  vtkGetMacro(ReprojectTime,bool);

  vtkSetMacro(ReprojectData,bool);
  vtkGetMacro(ReprojectData,bool);

  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  vtkSetMacro(InterpolationOrder,int);
  vtkGetMacro(InterpolationOrder,int);

  enum ORDER
  {
    NearestNeighbour = 0,
    Bilinear,
    Bicubic,
  };

  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);


protected:
  vtkMRMLAstroReprojectParametersNode();
  ~vtkMRMLAstroReprojectParametersNode();

  vtkMRMLAstroReprojectParametersNode(const vtkMRMLAstroReprojectParametersNode&);
  void operator=(const vtkMRMLAstroReprojectParametersNode&);

  char *InputVolumeNodeID;
  char *ReferenceVolumeNodeID;
  char *OutputVolumeNodeID;

  bool ReprojectRotation;
  bool ReprojectTime;
  bool ReprojectData;

  int OutputSerial;

  int InterpolationOrder;

  int Cores;

  int Status;
};

#endif
