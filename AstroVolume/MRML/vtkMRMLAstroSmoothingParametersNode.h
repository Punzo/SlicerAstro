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

/// \ingroup SlicerAstro_QtModules_AstroSmoothing
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroSmoothingParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLAstroSmoothingParametersNode *New();
  vtkTypeMacro(vtkMRMLAstroSmoothingParametersNode,vtkMRMLNode);
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
  virtual const char* GetNodeTagName() VTK_OVERRIDE {return "AstroSmoothingParameters";};

  vtkSetMacro(ParameterX,double);
  vtkGetMacro(ParameterX,double);

  vtkSetMacro(ParameterY,double);
  vtkGetMacro(ParameterY,double);

  vtkSetMacro(ParameterZ,double);
  vtkGetMacro(ParameterZ,double);

  vtkSetStringMacro(InputVolumeNodeID);
  vtkGetStringMacro(InputVolumeNodeID);

  vtkSetStringMacro(OutputVolumeNodeID);
  vtkGetStringMacro(OutputVolumeNodeID);

  vtkSetStringMacro(Mode);
  vtkGetStringMacro(Mode);

  vtkSetStringMacro(MasksCommand);
  vtkGetStringMacro(MasksCommand);

  vtkSetMacro(OutputSerial,int);
  vtkGetMacro(OutputSerial,int);

  vtkSetMacro(Filter,int);
  vtkGetMacro(Filter,int);

  vtkSetMacro(Hardware,int);
  vtkGetMacro(Hardware,int);

  vtkSetMacro(Cores,int);
  vtkGetMacro(Cores,int);

  vtkSetMacro(Link,bool);
  vtkGetMacro(Link,bool);
  vtkBooleanMacro(Link,bool);

  vtkSetMacro(AutoRun,bool);
  vtkGetMacro(AutoRun,bool);
  vtkBooleanMacro(AutoRun,bool);

  vtkSetMacro(Accuracy,int);
  vtkGetMacro(Accuracy,int);

  vtkSetMacro(Rx,double);
  vtkGetMacro(Rx,double);

  vtkSetMacro(Ry,double);
  vtkGetMacro(Ry,double);

  vtkSetMacro(Rz,double);
  vtkGetMacro(Rz,double);

  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

  vtkSetMacro(K,double);
  vtkGetMacro(K,double);

  vtkSetMacro(TimeStep,double);
  vtkGetMacro(TimeStep,double);

  vtkSetMacro(KernelLengthX,int);
  vtkGetMacro(KernelLengthX,int);

  vtkSetMacro(KernelLengthY,int);
  vtkGetMacro(KernelLengthY,int);

  vtkSetMacro(KernelLengthZ,int);
  vtkGetMacro(KernelLengthZ,int);

  void SetGaussianKernels();

  void SetGaussianKernel1D();
  vtkDoubleArray* GetGaussianKernel1D();

  void SetGaussianKernel3D();
  vtkDoubleArray* GetGaussianKernel3D();

protected:
  vtkMRMLAstroSmoothingParametersNode();
  ~vtkMRMLAstroSmoothingParametersNode();

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
