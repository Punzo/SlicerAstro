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

/// \brief MRML parameter node for the AstroReproject module.
///
/// \ingroup SlicerAstro_QtModules_AstroReproject
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroReprojectParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroReprojectParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroReprojectParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroReprojectParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the ReferenceVolumeNodeID.
  /// \sa SetReferenceVolumeNodeID(), GetReferenceVolumeNodeID()
  vtkSetStringMacro(ReferenceVolumeNodeID);
  vtkGetStringMacro(ReferenceVolumeNodeID);

  /// Set/Get the OutputVolumeNodeID.
  /// \sa SetOutputVolumeNodeID(), GetOutputVolumeNodeID()
  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  /// Set/Get the ReprojectRotation.
  /// \sa SetReprojectRotation(), GetReprojectRotation()
  vtkSetMacro(ReprojectRotation,bool);
  vtkGetMacro(ReprojectRotation,bool);
  vtkBooleanMacro(ReprojectRotation,bool);

  /// Set/Get the ReprojectData.
  /// \sa SetReprojectData(), GetReprojectData()
  vtkSetMacro(ReprojectData,bool);
  vtkGetMacro(ReprojectData,bool);
  vtkBooleanMacro(ReprojectData,bool);

  /// Set/Get the OutputSerial.
  /// \sa SetOutputSerial(), GetOutputSerial()
  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  /// Set/Get the InterpolationOrder.
  /// Default is 1 (Bilinear)
  /// \sa SetInterpolationOrder(), GetInterpolationOrder()
  vtkSetMacro(InterpolationOrder,int);
  vtkGetMacro(InterpolationOrder,int);

  enum ORDER
  {
    NearestNeighbour = 0,
    Bilinear,
    Bicubic,
  };

  /// Set/Get the Cores.
  /// Default is 0 (all the free cores will be used)
  /// \sa SetCores(), GetCores()
  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  /// Set/Get the Status.
  /// \sa SetStatus(), GetStatus()
  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

protected:
  vtkMRMLAstroReprojectParametersNode();
  ~vtkMRMLAstroReprojectParametersNode() override;

  vtkMRMLAstroReprojectParametersNode(const vtkMRMLAstroReprojectParametersNode&);
  void operator=(const vtkMRMLAstroReprojectParametersNode&);

  char *InputVolumeNodeID;
  char *ReferenceVolumeNodeID;
  char *OutputVolumeNodeID;

  bool ReprojectRotation;
  bool ReprojectData;

  int OutputSerial;

  int InterpolationOrder;

  int Cores;

  int Status;
};

#endif
