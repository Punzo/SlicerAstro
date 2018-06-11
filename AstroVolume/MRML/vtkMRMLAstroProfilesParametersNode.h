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

#ifndef __vtkMRMLAstroProfilesParametersNode_h
#define __vtkMRMLAstroProfilesParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

/// \ingroup Slicer_QtModules_AstroProfiles
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroProfilesParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroProfilesParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroProfilesParametersNode,vtkMRMLNode);
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
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroProfilesParameters";};

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(ProfileVolumeNodeID);
  vtkGetStringMacro(ProfileVolumeNodeID);

  vtkSetStringMacro(MaskVolumeNodeID);
  vtkGetStringMacro(MaskVolumeNodeID);

  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  vtkSetMacro(MaskActive,bool);
  vtkGetMacro(MaskActive,bool);
  vtkBooleanMacro(MaskActive,bool);

  vtkSetMacro(IntensityMin,double);
  vtkGetMacro(IntensityMin,double);

  vtkSetMacro(IntensityMax,double);
  vtkGetMacro(IntensityMax,double);

  vtkSetMacro(VelocityMin,double);
  vtkGetMacro(VelocityMin,double);

  vtkSetMacro(VelocityMax,double);
  vtkGetMacro(VelocityMax,double);

  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

protected:
  vtkMRMLAstroProfilesParametersNode();
  ~vtkMRMLAstroProfilesParametersNode();

  vtkMRMLAstroProfilesParametersNode(const vtkMRMLAstroProfilesParametersNode&);
  void operator=(const vtkMRMLAstroProfilesParametersNode&);

  char *InputVolumeNodeID;
  char *ProfileVolumeNodeID;
  char *MaskVolumeNodeID;

  int Cores;

  bool MaskActive;

  double IntensityMin;
  double IntensityMax;
  double VelocityMin;
  double VelocityMax;

  int OutputSerial;

  int Status;
};

#endif
