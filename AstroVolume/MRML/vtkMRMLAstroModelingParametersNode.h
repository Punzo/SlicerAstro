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

class vtkMRMLTableNode;

/// \brief MRML parameter node for the AstroModeling module.
///
/// \ingroup SlicerAstro_QtModules_AstroModeling
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroModelingParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroModelingParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroModelingParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes(const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroModelingParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the OutputVolumeNodeID.
  /// \sa SetOutputVolumeNodeID(), GetOutputVolumeNodeID()
  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  /// Set/Get the ResidualVolumeNodeID.
  /// \sa SetResidualVolumeNodeID(), GetResidualVolumeNodeID()
  vtkSetStringMacro(ResidualVolumeNodeID);
  vtkGetStringMacro(ResidualVolumeNodeID);

  /// Set/Get the MaskVolumeNodeID.
  /// \sa SetMaskVolumeNodeID(), GetMaskVolumeNodeID()
  vtkSetStringMacro(MaskVolumeNodeID);
  vtkGetStringMacro(MaskVolumeNodeID);

  /// Set/Get the Mode.
  /// Default is "Automatic"
  /// \sa SetMode(), GetMode()
  vtkSetStringMacro(Mode);
  vtkGetStringMacro(Mode);

  /// Set/Get the MaskActive.
  /// Default is false
  /// \sa SetMaskActive(), GetMaskActive()
  vtkSetMacro(MaskActive,bool);
  vtkGetMacro(MaskActive,bool);
  vtkBooleanMacro(MaskActive,bool);

  /// Set/Get the OutputSerial.
  /// \sa SetOutputSerial(), GetOutputSerial()
  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  /// Set/Get the NumberOfRings.
  /// \sa SetNumberOfRings(), GetNumberOfRings()
  vtkSetMacro(NumberOfRings,int);
  vtkGetMacro(NumberOfRings,int);

  /// Set/Get the RadSep.
  /// \sa SetRadSep(), GetRadSep()
  vtkSetMacro(RadSep,double);
  vtkGetMacro(RadSep,double);

  /// Set/Get the XCenter.
  /// \sa Set()XCenter, GetXCenter()
  vtkSetMacro(XCenter,double);
  vtkGetMacro(XCenter,double);

  /// Set/Get the YCenter.
  /// \sa SetYCenter(), GetYCenter()
  vtkSetMacro(YCenter,double);
  vtkGetMacro(YCenter,double);

  /// Set/Get the SystemicVelocity.
  /// \sa SetSystemicVelocity(), GetSystemicVelocity()
  vtkSetMacro(SystemicVelocity,double);
  vtkGetMacro(SystemicVelocity,double);

  /// Set/Get the RotationVelocity.
  /// \sa SetRotationVelocity(), GetRotationVelocity()
  vtkSetMacro(RotationVelocity,double);
  vtkGetMacro(RotationVelocity,double);

  /// Set/Get the RadialVelocity.
  /// \sa SetRadialVelocity(), GetRadialVelocity()
  vtkSetMacro(RadialVelocity,double);
  vtkGetMacro(RadialVelocity,double);

  /// Set/Get the VerticalVelocity.
  /// \sa SetVerticalVelocity(), GetVerticalVelocity()
  vtkSetMacro(VerticalVelocity,double);
  vtkGetMacro(VerticalVelocity,double);

  /// Set/Get the VerticalRotationalGradient.
  /// \sa SetVerticalRotationalGradient(), GetVerticalRotationalGradient()
  vtkSetMacro(VerticalRotationalGradient,double);
  vtkGetMacro(VerticalRotationalGradient,double);

  /// Set/Get the VerticalRotationalGradientHeight.
  /// \sa SetVerticalRotationalGradientHeight(), GetVerticalRotationalGradientHeight()
  vtkSetMacro(VerticalRotationalGradientHeight,double);
  vtkGetMacro(VerticalRotationalGradientHeight,double);

  /// Set/Get the VelocityDispersion.
  /// \sa SetVelocityDispersion(), GetVelocityDispersion()
  vtkSetMacro(VelocityDispersion,double);
  vtkGetMacro(VelocityDispersion,double);

  /// Set/Get the Inclination.
  /// \sa SetInclination(), GetInclination()
  vtkSetMacro(Inclination,double);
  vtkGetMacro(Inclination,double);

  /// Set/Get the InclinationError.
  /// \sa SetInclinationError(), GetInclinationError()
  vtkSetMacro(InclinationError,double);
  vtkGetMacro(InclinationError,double);

  /// Set/Get the PositionAngle.
  /// \sa SetPositionAngle(), GetPositionAngle()
  vtkSetMacro(PositionAngle,double);
  vtkGetMacro(PositionAngle,double);

  /// Set/Get the PositionAngleError.
  /// \sa SetPositionAngleError(), GetPositionAngleError()
  vtkSetMacro(PositionAngleError,double);
  vtkGetMacro(PositionAngleError,double);

  /// Set/Get the ScaleHeight.
  /// \sa SetScaleHeight(), GetScaleHeight()
  vtkSetMacro(ScaleHeight,double);
  vtkGetMacro(ScaleHeight,double);

  /// Set/Get the ColumnDensity.
  /// \sa SetColumnDensity(), GetColumnDensity()
  vtkSetMacro(ColumnDensity,double);
  vtkGetMacro(ColumnDensity,double);

  /// Set/Get the Distance.
  /// \sa SetDistance(), GetDistance()
  vtkSetMacro(Distance,double);
  vtkGetMacro(Distance,double);

  /// Set/Get the PositionAngleFit.
  /// \sa SetPositionAngleFit(), GetPositionAngleFit()
  vtkSetMacro(PositionAngleFit,bool);
  vtkGetMacro(PositionAngleFit,bool);
  vtkBooleanMacro(PositionAngleFit,bool);

  /// Set/Get the RotationVelocityFit.
  /// \sa SetRotationVelocityFit(), GetRotationVelocityFit()
  vtkSetMacro(RotationVelocityFit,bool);
  vtkGetMacro(RotationVelocityFit,bool);
  vtkBooleanMacro(RotationVelocityFit,bool);

  /// Set/Get the RadialVelocityFit.
  /// \sa SetRadialVelocityFit(), GetRadialVelocityFit()
  vtkSetMacro(RadialVelocityFit,bool);
  vtkGetMacro(RadialVelocityFit,bool);
  vtkBooleanMacro(RadialVelocityFit,bool);

  /// Set/Get the VelocityDispersionFit.
  /// \sa SetVelocityDispersionFit(), GetVelocityDispersionFit()
  vtkSetMacro(VelocityDispersionFit,bool);
  vtkGetMacro(VelocityDispersionFit,bool);
  vtkBooleanMacro(VelocityDispersionFit,bool);

  /// Set/Get the InclinationFit.
  /// \sa SetInclinationFit(), GetInclinationFit()
  vtkSetMacro(InclinationFit,bool);
  vtkGetMacro(InclinationFit,bool);
  vtkBooleanMacro(InclinationFit,bool);

  /// Set/Get the XCenterFit.
  /// \sa SetXCenterFit(), GetXCenterFit()
  vtkSetMacro(XCenterFit,bool);
  vtkGetMacro(XCenterFit,bool);
  vtkBooleanMacro(XCenterFit,bool);

  /// Set/Get the YCenterFit.
  /// \sa SetYCenterFit(), GetYCenterFit()
  vtkSetMacro(YCenterFit,bool);
  vtkGetMacro(YCenterFit,bool);
  vtkBooleanMacro(YCenterFit,bool);

  /// Set/Get the SystemicVelocityFit.
  /// \sa SetSystemicVelocityFit(), GetSystemicVelocityFit()
  vtkSetMacro(SystemicVelocityFit,bool);
  vtkGetMacro(SystemicVelocityFit,bool);
  vtkBooleanMacro(SystemicVelocityFit,bool);

  /// Set/Get the ScaleHeightFit.
  /// \sa SetScaleHeightFit(), GetScaleHeightFit()
  vtkSetMacro(ScaleHeightFit,bool);
  vtkGetMacro(ScaleHeightFit,bool);
  vtkBooleanMacro(ScaleHeightFit,bool);

  /// Set/Get the ADRIFTCorrection.
  /// \sa SetADRIFTCorrection(), GetADRIFTCorrection()
  vtkSetMacro(ADRIFTCorrection,bool);
  vtkGetMacro(ADRIFTCorrection,bool);
  vtkBooleanMacro(ADRIFTCorrection,bool);

  /// Set/Get the LayerType.
  /// \sa SetLayerType(), GetLayerType()
  vtkSetMacro(LayerType,int);
  vtkGetMacro(LayerType,int);

  /// Set/Get the FittingFunction.
  /// \sa SetFittingFunction(), GetFittingFunction()
  vtkSetMacro(FittingFunction,int);
  vtkGetMacro(FittingFunction,int);

  /// Set/Get the WeightingFunction.
  /// \sa SetWeightingFunction(), GetWeightingFunction()
  vtkSetMacro(WeightingFunction,int);
  vtkGetMacro(WeightingFunction,int);

  /// Set/Get the NumberOfClounds.
  /// \sa SetNumberOfClounds(), GetNumberOfClounds()
  vtkSetMacro(NumberOfClounds,int);
  vtkGetMacro(NumberOfClounds,int);

  /// Set/Get the CloudsColumnDensity.
  /// \sa SetCloudsColumnDensity(), GetCloudsColumnDensity()
  vtkSetMacro(CloudsColumnDensity,double);
  vtkGetMacro(CloudsColumnDensity,double);

  /// Set/Get the Tollerance for the fitting.
  /// \sa SetTollerance(), GetTollerance()
  vtkSetMacro(Tollerance,double);
  vtkGetMacro(Tollerance,double);

  /// IJK coordinates of the center
  /// for the PV on the semi-major axis.
  /// Set/Get the XPosCenterIJK
  /// \sa SetXPosCenterIJK(), GetXPosCenterIJK()
  vtkSetMacro(XPosCenterIJK,double);
  vtkGetMacro(XPosCenterIJK,double);

  /// IJK coordinates of the center
  /// for the PV on the semi-major axis.
  /// Set/Get the YPosCenterIJK.
  /// \sa SetYPosCenterIJK(), GetYPosCenterIJK()
  vtkSetMacro(YPosCenterIJK,double);
  vtkGetMacro(YPosCenterIJK,double);

  /// RAS coordinates of the center
  /// for the PV on the semi-major axis.
  /// Set/Get the XPosCenterRAS
  /// \sa SetXPosCenterRAS(), GetXPosCenterRAS()
  vtkSetMacro(XPosCenterRAS,double);
  vtkGetMacro(XPosCenterRAS,double);

  /// RAS coordinates of the center
  /// for the PV on the semi-major axis
  /// Set/Get the YPosCenterRAS
  /// \sa SetYPosCenterRAS(), GetYPosCenterRAS()
  vtkSetMacro(YPosCenterRAS,double);
  vtkGetMacro(YPosCenterRAS,double);

  /// RAS coordinates of the center
  /// for the PV on the semi-major axis.
  /// Set/Get the ZPosCenterRAS.
  /// \sa SetZPosCenterRAS(), GetZPosCenterRAS()
  vtkSetMacro(ZPosCenterRAS,double);
  vtkGetMacro(ZPosCenterRAS,double);

  /// Angle reference for the PV
  /// on the semi-major axis from W->E position.
  /// Set/Get the PVPhi
  /// \sa SetPVPhi(), GetPVPhi()
  vtkSetMacro(PVPhi,double);
  vtkGetMacro(PVPhi,double);

  enum
    {
      YellowRotationModifiedEvent = 70000,
      GreenRotationModifiedEvent = 71000,
      YellowRotationUpdatedEvent = 72000,
      GreenRotationUpdatedEvent = 73000
    };

  /// Angles for the semi-major axis PV rotation.
  /// Set/Get the YellowRotOldValue
  /// \sa SetYellowRotOldValue(), GetYellowRotOldValue()
  vtkSetMacro(YellowRotOldValue,double);
  vtkGetMacro(YellowRotOldValue,double);

  /// Set Yellow slice rotation value
  void SetYellowRotValue(double rot);

  /// Get Yellow slice rotation value
  vtkGetMacro(YellowRotValue,double);

  /// Angles for the semi-minor axis PV rotation.
  /// Set/Get the GreenRotOldValue
  /// \sa SetGreenRotOldValue(), GetGreenRotOldValue()
  vtkSetMacro(GreenRotOldValue,double);
  vtkGetMacro(GreenRotOldValue,double);

  /// Set Green slice rotation value
  void SetGreenRotValue(double rot);

  /// Get Green slice rotation value
  vtkGetMacro(GreenRotValue,double);

  /// Set/Get the Status.
  /// \sa SetStatus(), GetStatus()
  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

  virtual int* GetStatusPointer() {return &Status;};

  /// Set/Get the Operation (Estimate parameters, Create Model, Fit Model).
  /// Default is ESTIMATE
  /// \sa SetOperatione(), GetOperation()
  vtkSetMacro(Operation,int);
  vtkGetMacro(Operation,int);

  enum OPERATION
  {
    ESTIMATE = 0,
    CREATE,
    FIT,
  };

  /// Convert between Operation ID and name
  virtual const char *GetOperationAsString(int id);

  /// Convert between Operation ID and name
  virtual int GetOperationFromString(const char *name);

  /// Set/Get the FitSuccess.
  /// \sa SetFitSuccess(), GetFitSuccess()
  vtkSetMacro(FitSuccess,bool);
  vtkGetMacro(FitSuccess,bool);
  vtkBooleanMacro(FitSuccess,bool);

  /// Set/Get the Normalize.
  /// Default is "Local".
  /// \sa SetNormalize(), GetNormalize()
  vtkSetStringMacro(Normalize);
  vtkGetStringMacro(Normalize);

  /// Set/Get the ForceSliceUpdate.
  /// \sa SetForceSliceUpdate(), GetForceSliceUpdate()
  vtkSetMacro(ForceSliceUpdate,bool);
  vtkGetMacro(ForceSliceUpdate,bool);
  vtkBooleanMacro(ForceSliceUpdate,bool);

  /// Set/Get the ContourLevel.
  /// Default is 3
  /// \sa SetContourLevel(), GetContourLevel()
  vtkSetMacro(ContourLevel,double);
  vtkGetMacro(ContourLevel,double);

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

  /// Get MRML table node with model parameters
  vtkMRMLTableNode* GetParamsTableNode();

  /// Set MRML table node with model parameters
  void SetParamsTableNode(vtkMRMLTableNode* node);

protected:
  vtkMRMLAstroModelingParametersNode();
  ~vtkMRMLAstroModelingParametersNode() override;

  vtkMRMLAstroModelingParametersNode(const vtkMRMLAstroModelingParametersNode&);
  void operator=(const vtkMRMLAstroModelingParametersNode&);

  static const char* PARAMS_TABLE_REFERENCE_ROLE;
  const char *GetTableNodeReferenceRole();

  char *InputVolumeNodeID;
  char *OutputVolumeNodeID;
  char *ResidualVolumeNodeID;
  char *MaskVolumeNodeID;
  char *Mode;
  char *Normalize;

  bool MaskActive;

  int OutputSerial;

  int NumberOfRings;
  double RadSep;
  double XCenter;
  double YCenter;
  double SystemicVelocity;
  double RotationVelocity;
  double RadialVelocity;
  double VerticalVelocity;
  double VerticalRotationalGradient;
  double VerticalRotationalGradientHeight;
  double VelocityDispersion;
  double Inclination;
  double InclinationError;
  double PositionAngle;
  double PositionAngleError;
  double ScaleHeight;
  double ColumnDensity;
  double Distance;

  double XPosCenterIJK;
  double YPosCenterIJK;
  double XPosCenterRAS;
  double YPosCenterRAS;
  double ZPosCenterRAS;
  double PVPhi;
  double YellowRotValue;
  double YellowRotOldValue;
  double GreenRotValue;
  double GreenRotOldValue;

  bool PositionAngleFit;
  bool RotationVelocityFit;
  bool RadialVelocityFit;
  bool VelocityDispersionFit;
  bool InclinationFit;
  bool XCenterFit;
  bool YCenterFit;
  bool SystemicVelocityFit;
  bool ScaleHeightFit;

  bool ADRIFTCorrection;

  int LayerType;
  int FittingFunction;
  int WeightingFunction;
  int NumberOfClounds;
  double CloudsColumnDensity;

  double Tollerance;

  int Status;
  int Operation;

  bool FitSuccess;
  bool ForceSliceUpdate;

  double ContourLevel;
};

#endif
