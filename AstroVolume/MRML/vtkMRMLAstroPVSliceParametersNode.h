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

/// \ingroup SlicerAstro_QtModules_AstroPVSlice
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroPVSliceParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroPVSliceParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroPVSliceParametersNode,vtkMRMLNode);
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
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroPVSliceParameters";};

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(MomentMapNodeID);
  vtkGetStringMacro(MomentMapNodeID);

  vtkSetStringMacro(RulerNodeID);
  vtkGetStringMacro(RulerNodeID);

  vtkSetMacro(RulerAngle,double);
  vtkGetMacro(RulerAngle,double);

  vtkSetMacro(RulerOldAngle,double);
  vtkGetMacro(RulerOldAngle,double);

  vtkSetMacro(RulerShiftX,double);
  vtkGetMacro(RulerShiftX,double);

  vtkSetMacro(RulerOldShiftX,double);
  vtkGetMacro(RulerOldShiftX,double);

  vtkSetMacro(RulerShiftY,double);
  vtkGetMacro(RulerShiftY,double);

  vtkSetMacro(RulerOldShiftY,double);
  vtkGetMacro(RulerOldShiftY,double);

  // Ruler center in MRML node is in IJK coordinates
  vtkGetVector2Macro (RulerCenter, int);
  // These set methods do not fire any event
  virtual void SetRulerCenterRightAscension(int value);
  virtual void SetRulerCenterDeclination(int value);
  virtual void SetRulerCenter(int arg1, int arg2);
  virtual void SetRulerCenter(int arg[2]);

  //
  enum
    {
    RulerCenterModifiedEvent = 78000
    };

protected:
  vtkMRMLAstroPVSliceParametersNode();
  ~vtkMRMLAstroPVSliceParametersNode();

  vtkMRMLAstroPVSliceParametersNode(const vtkMRMLAstroPVSliceParametersNode&);
  void operator=(const vtkMRMLAstroPVSliceParametersNode&);

  char *InputVolumeNodeID;
  char *MomentMapNodeID;
  char *RulerNodeID;

  double RulerAngle;
  double RulerOldAngle;

  double RulerShiftX;
  double RulerOldShiftX;

  double RulerShiftY;
  double RulerOldShiftY;

  int RulerCenter[2];
};

#endif
