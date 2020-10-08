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

/// \brief MRML parameter node for the AstroProfiles module.
///
/// \ingroup SlicerAstro_QtModules_AstroProfiles
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroProfilesParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroProfilesParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroProfilesParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroProfilesParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the ProfileVolumeNodeID.
  /// \sa SetProfileVolumeNodeID(), GetProfileVolumeNodeID()
  vtkSetStringMacro(ProfileVolumeNodeID);
  vtkGetStringMacro(ProfileVolumeNodeID);

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
  vtkMRMLAstroProfilesParametersNode();
  ~vtkMRMLAstroProfilesParametersNode() override;

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
