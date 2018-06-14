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

#ifndef __vtkMRMLAstroMomentMapsParametersNode_h
#define __vtkMRMLAstroMomentMapsParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

/// \ingroup SlicerAstro_QtModules_AstroMomentMaps
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroMomentMapsParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroMomentMapsParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroMomentMapsParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual vtkMRMLNode* CreateNodeInstance() VTK_OVERRIDE;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) VTK_OVERRIDE;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) VTK_OVERRIDE;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) VTK_OVERRIDE;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroMomentMapsParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the ZeroMomentVolumeNodeID.
  /// \sa SetZeroMomentVolumeNodeID(), GetZeroMomentVolumeNodeID()
  vtkSetStringMacro(ZeroMomentVolumeNodeID);
  vtkGetStringMacro(ZeroMomentVolumeNodeID);

  /// Set/Get the FirstMomentVolumeNodeID.
  /// \sa SetFirstMomentVolumeNodeID(), GetFirstMomentVolumeNodeID()
  vtkSetStringMacro(FirstMomentVolumeNodeID);
  vtkGetStringMacro(FirstMomentVolumeNodeID);

  /// Set/Get the SecondMomentVolumeNodeID.
  /// \sa SetSecondMomentVolumeNodeID(), GetSecondMomentVolumeNodeID()
  vtkSetStringMacro(SecondMomentVolumeNodeID);
  vtkGetStringMacro(SecondMomentVolumeNodeID);

  /// Set/Get the MaskVolumeNodeID.
  /// \sa SetMaskVolumeNodeID(), GetMaskVolumeNodeID()
  vtkSetStringMacro(MaskVolumeNodeID);
  vtkGetStringMacro(MaskVolumeNodeID);

  /// Set/Get the Cores.
  /// Default is 0 (all the free cores will be used)
  /// \sa SetCores(), GetCores()
  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  /// Set/Get the MaskActive.
  /// Default is false
  /// \sa SetMaskActive(), GetMaskActive()
  vtkSetMacro(MaskActive,bool);
  vtkGetMacro(MaskActive,bool);
  vtkBooleanMacro(MaskActive,bool);

  /// Set/Get the GenerateZero.
  /// Default is true
  /// \sa SetGenerateZero(), GetGenerateZero()
  vtkSetMacro(GenerateZero,bool);
  vtkGetMacro(GenerateZero,bool);
  vtkBooleanMacro(GenerateZero,bool);

  /// Set/Get the GenerateFirst.
  /// Default is true
  /// \sa SetGenerateFirst(), GetGenerateFirst()
  vtkSetMacro(GenerateFirst,bool);
  vtkGetMacro(GenerateFirst,bool);
  vtkBooleanMacro(GenerateFirst,bool);

  /// Set/Get the GenerateSecond.
  /// Default is true
  /// \sa SetGenerateSecond(), GetGenerateSecond()
  vtkSetMacro(GenerateSecond,bool);
  vtkGetMacro(GenerateSecond,bool);
  vtkBooleanMacro(GenerateSecond,bool);

  /// Set/Get the IntensityMin.
  /// \sa SetIntensityMin(), GetIntensityMin()
  vtkSetMacro(IntensityMin,double);
  vtkGetMacro(IntensityMin,double);

  /// Set/Get the IntensityMax.
  /// \sa SetIntensityMax(), GetIntensityMax()
  vtkSetMacro(IntensityMax,double);
  vtkGetMacro(IntensityMax,double);

  /// Set/Get the VelocityMin.
  /// \sa SetVelocityMin(), GetVelocityMin()
  vtkSetMacro(VelocityMin,double);
  vtkGetMacro(VelocityMin,double);

  /// Set/Get the VelocityMax.
  /// \sa SetVelocityMax(), GetVelocityMax()
  vtkSetMacro(VelocityMax,double);
  vtkGetMacro(VelocityMax,double);

  /// Set/Get the OutputSerial.
  /// \sa SetOutputSerial(), GetOutputSerial()
  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  /// Set/Get the Status.
  /// \sa SetStatus(), GetStatus()
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
