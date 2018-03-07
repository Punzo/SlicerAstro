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

#ifndef __vtkMRMLAstroPVDiagramParametersNode_h
#define __vtkMRMLAstroPVDiagramParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

/// \ingroup Slicer_QtModules_AstroPVDiagram
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroPVDiagramParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroPVDiagramParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroPVDiagramParametersNode,vtkMRMLNode);
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
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroPVDiagramParameters";};

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(MomentMapNodeID);
  vtkGetStringMacro(MomentMapNodeID);

  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  vtkSetStringMacro(FiducialsMarkupsID);
  vtkGetStringMacro(FiducialsMarkupsID);

  vtkSetStringMacro(ModelID);
  vtkGetStringMacro(ModelID);

  void SetInterpolation(bool interpolation);
  vtkGetMacro(Interpolation,bool);

  //
  enum
    {
    InterpolationModifiedEvent = 130000
    };

  vtkSetMacro(AutoUpdate,bool);
  vtkGetMacro(AutoUpdate,bool);

protected:
  vtkMRMLAstroPVDiagramParametersNode();
  ~vtkMRMLAstroPVDiagramParametersNode();

  vtkMRMLAstroPVDiagramParametersNode(const vtkMRMLAstroPVDiagramParametersNode&);
  void operator=(const vtkMRMLAstroPVDiagramParametersNode&);

  char *InputVolumeNodeID;
  char *MomentMapNodeID;
  char *OutputVolumeNodeID;
  char *FiducialsMarkupsID;
  char *ModelID;

  bool Interpolation;
  bool AutoUpdate;
};

#endif
