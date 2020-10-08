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

/// \brief MRML parameter node for the AstroPVDiagram module.
///
/// \ingroup SlicerAstro_QtModules_AstroPVDiagram
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroPVDiagramParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroPVDiagramParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroPVDiagramParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroPVDiagramParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the MomentMapNodeID.
  /// \sa SetMomentMapNodeID(), GetMomentMapNodeID()
  vtkSetStringMacro(MomentMapNodeID);
  vtkGetStringMacro(MomentMapNodeID);

  /// Set/Get the OutputVolumeNodeID.
  /// \sa SetOutputVolumeNodeID(), GetOutputVolumeNodeID()
  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  /// Set/Get the FiducialsMarkupsID.
  /// \sa SetFiducialsMarkupsID(), GetFiducialsMarkupsID()
  vtkSetStringMacro(FiducialsMarkupsID);
  vtkGetStringMacro(FiducialsMarkupsID);

  /// Set/Get the ModelID.
  /// \sa SetModelID(), GetModelID()
  vtkSetStringMacro(ModelID);
  vtkGetStringMacro(ModelID);

  /// Set Interpolation
  void SetInterpolation(bool interpolation);

  /// Get Interpolation
  vtkGetMacro(Interpolation,bool);
  vtkBooleanMacro(Interpolation,bool);

  //
  enum
    {
    InterpolationModifiedEvent = 130000
    };

  /// Set/Get the AutoUpdate.
  /// \sa SetAutoUpdate(), GetAutoUpdate()
  vtkSetMacro(AutoUpdate,bool);
  vtkGetMacro(AutoUpdate,bool);
  vtkBooleanMacro(AutoUpdate,bool);

protected:
  vtkMRMLAstroPVDiagramParametersNode();
  ~vtkMRMLAstroPVDiagramParametersNode() override;

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
