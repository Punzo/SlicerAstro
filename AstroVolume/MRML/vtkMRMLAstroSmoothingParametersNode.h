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

#ifndef __vtkMRMLAstroSmoothingParametersNode_h
#define __vtkMRMLAstroSmoothingParametersNode_h

#include "vtkMRML.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkDoubleArray;

/// \brief MRML parameter node for the AstroMSmoothing module.
///
/// \ingroup SlicerAstro_QtModules_AstroSmoothing
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroSmoothingParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroSmoothingParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroSmoothingParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes
  virtual void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node) override;

  /// Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() override {return "AstroSmoothingParameters";};

  /// Set/Get the InputVolumeNodeID.
  /// \sa SetInputVolumeNodeID(), GetInputVolumeNodeID()
  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  /// Set/Get the OutputVolumeNodeID.
  /// \sa SetOutputVolumeNodeID(), GetOutputVolumeNodeID()
  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  /// Set/Get the Mode.
  /// Default is "Automatic"
  /// \sa SetMode(), GetMode()
  vtkSetStringMacro(Mode);
  vtkGetStringMacro(Mode);

  /// Set/Get the MasksCommand.
  /// Default is "Skip"
  /// \sa SetMasksCommand(), GetMasksCommand()
  vtkSetStringMacro(MasksCommand);
  vtkGetStringMacro(MasksCommand);

  /// Set/Get the OutputSerial
  /// \sa SetOutputSerial(), GetOutputSerial()
  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  /// Set/Get the Filter.
  /// Default is 2 (intensity-driven gradient)
  /// \sa SetFilter(), GetFilter()
  vtkSetMacro(Filter,int);
  vtkGetMacro(Filter,int);

  /// Set/Get the Hardware.
  /// Default is 0 (CPU)
  /// \sa SetHardware(), GetHardware()
  vtkSetMacro(Hardware,int);
  vtkGetMacro(Hardware,int);

  /// Set/Get the Cores.
  /// Default is 0 (all the free cores will be used)
  /// \sa SetCores(), GetCores()
  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  /// Set/Get the Link of the three main parameters.
  /// Default is false
  /// \sa SetLink(), GetLink()
  vtkSetMacro(Link,bool);
  vtkGetMacro(Link,bool);
  vtkBooleanMacro(Link,bool);

  /// Set/Get the AutoRun.
  /// Default is false
  /// \sa SetAutoRun(), GetAutoRun()
  vtkSetMacro(AutoRun,bool);
  vtkGetMacro(AutoRun,bool);
  vtkBooleanMacro(AutoRun,bool);

  /// Set/Get the ParameterX.
  /// Default is 5
  /// \sa SetParameterX(), GetParameterX()
  vtkSetMacro(ParameterX,double);
  vtkGetMacro(ParameterX,double);

  /// Set/Get the ParameterY.
  /// Default is 5
  /// \sa SetParameterY(), GetParameterY()
  vtkSetMacro(ParameterY,double);
  vtkGetMacro(ParameterY,double);

  /// Set/Get the ParameterZ.
  /// Default is 5
  /// \sa SetParameterZ(), GetParameterZ()
  vtkSetMacro(ParameterZ,double);
  vtkGetMacro(ParameterZ,double);

  /// Set/Get the Accuracy.
  /// Default is 20
  /// \sa SetAccuracy(), GetAccuracy()
  vtkSetMacro(Accuracy,int);
  vtkGetMacro(Accuracy,int);

  /// Set/Get the Rx (Eulerian angle in degree).
  /// Default is 0
  /// \sa SetRx(), GetRx()
  vtkSetMacro(Rx,double);
  vtkGetMacro(Rx,double);

  /// Set/Get the Ry (Eulerian angle in degree).
  /// Default is 0
  /// \sa SetRy(), GetRy()
  vtkSetMacro(Ry,double);
  vtkGetMacro(Ry,double);

  /// Set/Get the Rz (Eulerian angle in degree).
  /// Default is 0
  /// \sa SetRz(), GetRz()
  vtkSetMacro(Rz,double);
  vtkGetMacro(Rz,double);

  /// Set/Get the Status.
  /// \sa SetStatus(), GetStatus()
  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

  /// Set/Get the K (intensity-driven gradient parameter).
  /// Default is 2
  /// \sa SetK(), GetK()
  vtkSetMacro(K,double);
  vtkGetMacro(K,double);

  /// Set/Get the TimeStep (intensity-driven gradient parameter).
  /// Default is 0.0325
  /// \sa SetTimeStep(), GetTimeStep()
  vtkSetMacro(TimeStep,double);
  vtkGetMacro(TimeStep,double);

  /// Set/Get the KernelLengthX (Gaussian parameter).
  /// \sa SetKernelLengthX(), GetKernelLengthX()
  vtkSetMacro(KernelLengthX,int);
  vtkGetMacro(KernelLengthX,int);

  /// Set/Get the KernelLengthY (Gaussian parameter).
  /// \sa SetKernelLengthY(), GetKernelLengthY()
  vtkSetMacro(KernelLengthY,int);
  vtkGetMacro(KernelLengthY,int);

  /// Set/Get the KernelLengthZ (Gaussian parameter).
  /// \sa SetKernelLengthZ(), GetKernelLengthZ()
  vtkSetMacro(KernelLengthZ,int);
  vtkGetMacro(KernelLengthZ,int);

  /// Initialize Gaussian kernels
  void SetGaussianKernels();

  /// Initialize 1D Gaussian kernels
  void SetGaussianKernel1D();

  /// Get 1D Gaussian kernels
  vtkDoubleArray* GetGaussianKernel1D();

  /// Initialize 3D Gaussian kernels
  void SetGaussianKernel3D();

  /// Get 3D Gaussian kernels
  vtkDoubleArray* GetGaussianKernel3D();

protected:
  vtkMRMLAstroSmoothingParametersNode();
  ~vtkMRMLAstroSmoothingParametersNode() override;

  vtkMRMLAstroSmoothingParametersNode(const vtkMRMLAstroSmoothingParametersNode&);
  void operator=(const vtkMRMLAstroSmoothingParametersNode&);

  char *InputVolumeNodeID;
  char *OutputVolumeNodeID;
  char *Mode;
  char *MasksCommand;
  int OutputSerial;

  /// Filter method
  /// 0: Box
  /// 1: Gaussian
  /// 2: Intensity-driven gradient
  int Filter;

  int Hardware;

  int Cores;

  bool Link;
  bool AutoRun;

  int Accuracy;
  int Status;

  double Rx;
  double Ry;
  double Rz;

  double K;
  double TimeStep;

  double ParameterX;
  double ParameterY;
  double ParameterZ;

  int KernelLengthX;
  int KernelLengthY;
  int KernelLengthZ;

  vtkSmartPointer<vtkDoubleArray> gaussianKernel3D;
  vtkSmartPointer<vtkDoubleArray> gaussianKernel1D;

  double DegToRad;
};

#endif
