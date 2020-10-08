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

#ifndef __vtkMRMLAstroPVSliceParametersNode_h
#define __vtkMRMLAstroPVSliceParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

/// \brief MRML parameter node for the AstroPVSlice module.
///
/// \ingroup SlicerAstro_QtModules_AstroPVSlice
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroPVSliceParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroPVSliceParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroPVSliceParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroPVSliceParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the MomentMapNodeID.
  /// \sa SetMomentMapNodeID(), GetMomentMapNodeID()
  vtkSetStringMacro(MomentMapNodeID);
  vtkGetStringMacro(MomentMapNodeID);

  /// Set/Get the LineNodeID.
  /// \sa SetLineNodeID(), GetLineNodeID()
  vtkSetStringMacro(LineNodeID);
  vtkGetStringMacro(LineNodeID);

  /// Set/Get the Line angle value respect to the center of the slice.
  /// \sa SetLineAngle(), GetLineAngle()
  vtkSetMacro(LineAngle,double);
  vtkGetMacro(LineAngle,double);

  /// Set/Get the Line old angle value respect to the center of the slice.
  /// \sa SetLineNodeID(), GetLineNodeID()
  vtkSetMacro(LineOldAngle,double);
  vtkGetMacro(LineOldAngle,double);

  /// Get Line center in MRML node is in IJK coordinates
  vtkGetVector2Macro (LineCenter, int);

  /// Set Line center. No modification event is fired
  virtual void SetLineCenterRightAscension(int value);

  /// Set Line center. No modification event is fired
  virtual void SetLineCenterDeclination(int value);

  /// Set Line center. No modification event is fired
  virtual void SetLineCenter(int arg1, int arg2);

  /// Set Line center. No modification event is fired
  virtual void SetLineCenter(int arg[2]);

  //
  enum
    {
    LineCenterModifiedEvent = 78000
    };

protected:
  vtkMRMLAstroPVSliceParametersNode();
  ~vtkMRMLAstroPVSliceParametersNode() override;

  vtkMRMLAstroPVSliceParametersNode(const vtkMRMLAstroPVSliceParametersNode&);
  void operator=(const vtkMRMLAstroPVSliceParametersNode&);

  char *InputVolumeNodeID;
  char *MomentMapNodeID;
  char *LineNodeID;

  double LineAngle;
  double LineOldAngle;

  int LineCenter[2];
};

#endif
