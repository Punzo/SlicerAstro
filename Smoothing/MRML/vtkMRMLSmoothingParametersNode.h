#ifndef __vtkMRMLSmoothingParametersNode_h
#define __vtkMRMLSmoothingParametersNode_h

#include "vtkMRML.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLNode.h"
#include "vtkSlicerSmoothingModuleMRMLExport.h"
#include "vtkDoubleArray.h"

/// \ingroup Slicer_QtModules_Smoothing
class VTK_SLICER_SMOOTHING_MODULE_MRML_EXPORT vtkMRMLSmoothingParametersNode : public vtkMRMLNode
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

  vtkSetMacro(Accuracy,int);
  vtkGetMacro(Accuracy,int);

  vtkSetMacro(Status,int);
  vtkGetMacro(Status,int);

  vtkSetMacro(ParameterX,double);
  vtkGetMacro(ParameterX,double);

  vtkSetMacro(ParameterY,double);
  vtkGetMacro(ParameterY,double);

  vtkSetMacro(ParameterZ,double);
  vtkGetMacro(ParameterZ,double);


  vtkDoubleArray* GetGaussianKernelX();
  vtkDoubleArray* GetGaussianKernelY();
  vtkDoubleArray* GetGaussianKernelZ();

  void SetGaussianKernelX();
  void SetGaussianKernelY();
  void SetGaussianKernelZ();

  void SetGaussianKernels();

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
  int Accuracy;
  int Status;

  double ParameterX;
  double ParameterY;
  double ParameterZ;

  vtkSmartPointer<vtkDoubleArray> gaussianKernelX;
  vtkSmartPointer<vtkDoubleArray> gaussianKernelY;
  vtkSmartPointer<vtkDoubleArray> gaussianKernelZ;
};

#endif

