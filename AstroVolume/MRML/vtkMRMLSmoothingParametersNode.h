#ifndef __vtkMRMLSmoothingParametersNode_h
#define __vtkMRMLSmoothingParametersNode_h

#include "vtkMRML.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLNode.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

class vtkDoubleArray;
class vtkMatrix3x3;

/// \ingroup Slicer_QtModules_Smoothing
class VTK_MRML_ASTRO_EXPORT vtkMRMLSmoothingParametersNode : public vtkMRMLNode
{
  public:

  static vtkMRMLSmoothingParametersNode *New();
  vtkTypeMacro(vtkMRMLSmoothingParametersNode,vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent);

  virtual vtkMRMLNode* CreateNodeInstance();

  // Description:
  // Set node attributes
  virtual void ReadXMLAttributes( const char** atts);

  // Description:
  // Write this node's information to a MRML file in XML format.
  virtual void WriteXML(ostream& of, int indent);

  // Description:
  // Copy the node's attributes to this object
  virtual void Copy(vtkMRMLNode *node);

  // Description:
  // Get node XML tag name (like Volume, Model)
  virtual const char* GetNodeTagName() {return "SmoothingParameters";};

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

  vtkSetMacro(OutputSerial, int);
  vtkGetMacro(OutputSerial, int);

  vtkSetMacro(Filter,int);
  vtkGetMacro(Filter,int);

  vtkSetMacro(Hardware,int);
  vtkGetMacro(Hardware,int);

  vtkSetMacro(Link,bool);
  vtkGetMacro(Link,bool);

  vtkSetMacro(Accuracy,int);
  vtkGetMacro(Accuracy,int);

  vtkSetMacro(Rx,int);
  vtkGetMacro(Rx,int);

  vtkSetMacro(Ry,int);
  vtkGetMacro(Ry,int);

  vtkSetMacro(Rz,int);
  vtkGetMacro(Rz,int);

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
  void SetGaussianKernel3D();
  vtkDoubleArray* GetGaussianKernel1D();
  vtkDoubleArray* GetGaussianKernel3D();

protected:
  vtkMRMLSmoothingParametersNode();
  ~vtkMRMLSmoothingParametersNode();

  vtkMRMLSmoothingParametersNode(const vtkMRMLSmoothingParametersNode&);
  void operator=(const vtkMRMLSmoothingParametersNode&);

  char *InputVolumeNodeID;
  char *OutputVolumeNodeID;
  char *Mode;
  int OutputSerial;

  /// Filter method
  /// 0: Gaussian
  /// 1: Adaptive
  /// 2: Wavelet Lifting
  int Filter;

  int Hardware;

  bool Link;

  int Accuracy;
  int Status;

  int Rx;
  int Ry;
  int Rz;

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

