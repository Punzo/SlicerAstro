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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

#ifndef __vtkMRMLAstroMomentMapsParametersNode_h
#define __vtkMRMLAstroMomentMapsParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

/// \ingroup Slicer_QtModules_AstroMomentMaps
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroMomentMapsParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroMomentMapsParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroMomentMapsParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();

  // Description:
  // Set node attributes
  virtual void ReadXMLAttributes( const char** atts);

  // Description:
  // Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  // Description:
  // Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node);

  // Description:
  // Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() {return "AstroMomentMapsParameters";};

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(ZeroMomentVolumeNodeID);
  vtkGetStringMacro(ZeroMomentVolumeNodeID);

  vtkSetStringMacro(FirstMomentVolumeNodeID);
  vtkGetStringMacro(FirstMomentVolumeNodeID);

  vtkSetStringMacro(SecondMomentVolumeNodeID);
  vtkGetStringMacro(SecondMomentVolumeNodeID);

  vtkSetStringMacro(MaskVolumeNodeID);
  vtkGetStringMacro(MaskVolumeNodeID);

  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  vtkSetMacro(MaskActive,bool);
  vtkGetMacro(MaskActive,bool);

  vtkSetMacro(GenerateZero,bool);
  vtkGetMacro(GenerateZero,bool);

  vtkSetMacro(GenerateFirst,bool);
  vtkGetMacro(GenerateFirst,bool);

  vtkSetMacro(GenerateSecond,bool);
  vtkGetMacro(GenerateSecond,bool);

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
  vtkMRMLAstroMomentMapsParametersNode();
  ~vtkMRMLAstroMomentMapsParametersNode();

  vtkMRMLAstroMomentMapsParametersNode(const vtkMRMLAstroMomentMapsParametersNode&);
  void operator=(const vtkMRMLAstroMomentMapsParametersNode&);

  char *InputVolumeNodeID;
  char *ZeroMomentVolumeNodeID;
  char *FirstMomentVolumeNodeID;
  char *SecondMomentVolumeNodeID;
  char *MaskVolumeNodeID;

  int Cores;

  bool MaskActive;
  bool GenerateZero;
  bool GenerateFirst;
  bool GenerateSecond;

  double IntensityMin;
  double IntensityMax;
  double VelocityMin;
  double VelocityMax;

  int OutputSerial;

  int Status;

};

#endif
