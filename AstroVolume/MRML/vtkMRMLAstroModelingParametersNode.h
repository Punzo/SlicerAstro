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

#ifndef __vtkMRMLAstroModelingParametersNode_h
#define __vtkMRMLAstroModelingParametersNode_h

// MRML includes
#include <vtkMRML.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// VTK includes
#include <vtkCollection.h>
#include <vtkDoubleArray.h>

// Export includes
#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkMRMLChartNode;
class vtkMRMLDoubleArrayNode;
class vtkMRMLTableNode;

/// \ingroup Slicer_QtModules_AstroModeling
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroModelingParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroModelingParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroModelingParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  virtual vtkMRMLNode* CreateNodeInstance() VTK_OVERRIDE;

  // Description:
  // Set node attributes
  virtual void ReadXMLAttributes(const char** atts) VTK_OVERRIDE;

  // Description:
  // Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) VTK_OVERRIDE;

  // Description:
  // Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) VTK_OVERRIDE;

  // Description:
  // Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroModelingParameters";};

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  vtkSetStringMacro(ResidualVolumeNodeID);
  vtkGetStringMacro(ResidualVolumeNodeID);

  vtkSetStringMacro(MaskVolumeNodeID);
  vtkGetStringMacro(MaskVolumeNodeID);

  vtkSetStringMacro(Mode);
  vtkGetStringMacro(Mode);

  vtkSetMacro(MaskActive,bool);
  vtkGetMacro(MaskActive,bool);

  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  vtkSetMacro(NumberOfRings,int);
  vtkGetMacro(NumberOfRings,int);

  vtkSetMacro(RadSep,double);
  vtkGetMacro(RadSep,double);

  vtkSetMacro(XCenter,double);
  vtkGetMacro(XCenter,double);

  vtkSetMacro(YCenter,double);
  vtkGetMacro(YCenter,double);

  vtkSetMacro(SystemicVelocity,double);
  vtkGetMacro(SystemicVelocity,double);

  vtkSetMacro(RotationVelocity,double);
  vtkGetMacro(RotationVelocity,double);

  vtkSetMacro(RadialVelocity,double);
  vtkGetMacro(RadialVelocity,double);

  vtkSetMacro(VelocityDispersion,double);
  vtkGetMacro(VelocityDispersion,double);

  vtkSetMacro(Inclination,double);
  vtkGetMacro(Inclination,double);

  vtkSetMacro(InclinationError,double);
  vtkGetMacro(InclinationError,double);

  vtkSetMacro(PositionAngle,double);
  vtkGetMacro(PositionAngle,double);

  vtkSetMacro(PositionAngleError,double);
  vtkGetMacro(PositionAngleError,double);

  vtkSetMacro(ScaleHeight,double);
  vtkGetMacro(ScaleHeight,double);

  vtkSetMacro(ColumnDensity,double);
  vtkGetMacro(ColumnDensity,double);

  vtkSetMacro(Distance,double);
  vtkGetMacro(Distance,double);

  vtkSetMacro(PositionAngleFit,bool);
  vtkGetMacro(PositionAngleFit,bool);

  vtkSetMacro(RotationVelocityFit,bool);
  vtkGetMacro(RotationVelocityFit,bool);

  vtkSetMacro(RadialVelocityFit,bool);
  vtkGetMacro(RadialVelocityFit,bool);

  vtkSetMacro(VelocityDispersionFit,bool);
  vtkGetMacro(VelocityDispersionFit,bool);

  vtkSetMacro(InclinationFit,bool);
  vtkGetMacro(InclinationFit,bool);

  vtkSetMacro(XCenterFit,bool);
  vtkGetMacro(XCenterFit,bool);

  vtkSetMacro(YCenterFit,bool);
  vtkGetMacro(YCenterFit,bool);

  vtkSetMacro(SystemicVelocityFit,bool);
  vtkGetMacro(SystemicVelocityFit,bool);

  vtkSetMacro(ScaleHeightFit,bool);
  vtkGetMacro(ScaleHeightFit,bool);

  vtkSetMacro(LayerType,int);
  vtkGetMacro(LayerType,int);

  vtkSetMacro(FittingFunction,int);
  vtkGetMacro(FittingFunction,int);

  vtkSetMacro(WeightingFunction,int);
  vtkGetMacro(WeightingFunction,int);

  vtkSetMacro(NumberOfClounds,int);
  vtkGetMacro(NumberOfClounds,int);

  vtkSetMacro(CloudsColumnDensity,double);
  vtkGetMacro(CloudsColumnDensity,double);

  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

  virtual int* GetStatusPointer() {return &Status;};

  vtkSetMacro(FitSuccess,bool);
  vtkGetMacro(FitSuccess,bool);

  vtkSetMacro(FirstPlot,bool);
  vtkGetMacro(FirstPlot,bool);

  vtkSetMacro(ContourLevel,double);
  vtkGetMacro(ContourLevel,double);

  static const char* PARAMS_TABLE_REFERENCE_ROLE;

  static const char* CHART_XPOS_REFERENCE_ROLE;
  static const char* CHART_YPOS_REFERENCE_ROLE;
  static const char* CHART_VSYS_REFERENCE_ROLE;
  static const char* CHART_VROT_REFERENCE_ROLE;
  static const char* CHART_VRAD_REFERENCE_ROLE;
  static const char* CHART_VDISP_REFERENCE_ROLE;
  static const char* CHART_DENS_REFERENCE_ROLE;
  static const char* CHART_Z0_REFERENCE_ROLE;
  static const char* CHART_INC_REFERENCE_ROLE;
  static const char* CHART_PHI_REFERENCE_ROLE;

  static const char* ARRAY_XPOS_REFERENCE_ROLE;
  static const char* ARRAY_YPOS_REFERENCE_ROLE;
  static const char* ARRAY_VSYS_REFERENCE_ROLE;
  static const char* ARRAY_VROT_REFERENCE_ROLE;
  static const char* ARRAY_VRAD_REFERENCE_ROLE;
  static const char* ARRAY_VDISP_REFERENCE_ROLE;
  static const char* ARRAY_DENS_REFERENCE_ROLE;
  static const char* ARRAY_Z0_REFERENCE_ROLE;
  static const char* ARRAY_INC_REFERENCE_ROLE;
  static const char* ARRAY_PHI_REFERENCE_ROLE;

  static const char* FIRST_ARRAY_XPOS_REFERENCE_ROLE;
  static const char* FIRST_ARRAY_YPOS_REFERENCE_ROLE;
  static const char* FIRST_ARRAY_VSYS_REFERENCE_ROLE;
  static const char* FIRST_ARRAY_VROT_REFERENCE_ROLE;
  static const char* FIRST_ARRAY_VRAD_REFERENCE_ROLE;
  static const char* FIRST_ARRAY_VDISP_REFERENCE_ROLE;
  static const char* FIRST_ARRAY_DENS_REFERENCE_ROLE;
  static const char* FIRST_ARRAY_Z0_REFERENCE_ROLE;
  static const char* FIRST_ARRAY_INC_REFERENCE_ROLE;
  static const char* FIRST_ARRAY_PHI_REFERENCE_ROLE;

  vtkMRMLTableNode* GetParamsTableNode();

  enum
  {
    ParamsColumnRadii = 0,
    ParamsColumnVRot,
    ParamsColumnVRad,
    ParamsColumnInc,
    ParamsColumnPhi,
    ParamsColumnVSys,
    ParamsColumnVDisp,
    ParamsColumnDens,
    ParamsColumnZ0,
    ParamsColumnXPos,
    ParamsColumnYPos,
  };

  /// Get chart nodes
  vtkCollection* GetChartNodes();

  /// Get array nodes for charts
  vtkCollection* GetArrayNodes();

protected:
  vtkMRMLAstroModelingParametersNode();
  ~vtkMRMLAstroModelingParametersNode();

  vtkMRMLAstroModelingParametersNode(const vtkMRMLAstroModelingParametersNode&);
  void operator=(const vtkMRMLAstroModelingParametersNode&);

  void SetAndObserveParamsTableNode(vtkMRMLTableNode* node);

  void SetAndObserveChartNode(vtkMRMLChartNode* node, const char* chartName);
  vtkSmartPointer<vtkCollection> chartNodes;

  void SetAndObserveArrayNode(vtkMRMLDoubleArrayNode* node, const char* arrayName);
  vtkSmartPointer<vtkCollection> arrayNodes;

  char *InputVolumeNodeID;
  char *OutputVolumeNodeID;
  char *ResidualVolumeNodeID;
  char *MaskVolumeNodeID;
  char *Mode;

  bool MaskActive;

  int OutputSerial;

  int NumberOfRings;
  double RadSep;
  double XCenter;
  double YCenter;
  double SystemicVelocity;
  double RotationVelocity;
  double RadialVelocity;
  double VelocityDispersion;
  double Inclination;
  double InclinationError;
  double PositionAngle;
  double PositionAngleError;
  double ScaleHeight;
  double ColumnDensity;
  double Distance;

  bool PositionAngleFit;
  bool RotationVelocityFit;
  bool RadialVelocityFit;
  bool VelocityDispersionFit;
  bool InclinationFit;
  bool XCenterFit;
  bool YCenterFit;
  bool SystemicVelocityFit;
  bool ScaleHeightFit;

  int LayerType;
  int FittingFunction;
  int WeightingFunction;
  int NumberOfClounds;
  double CloudsColumnDensity;

  int Status;

  bool FitSuccess;
  bool FirstPlot;

  double ContourLevel;
};

#endif

