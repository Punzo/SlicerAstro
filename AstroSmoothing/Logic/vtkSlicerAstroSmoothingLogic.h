#ifndef __vtkSlicerAstroSmoothingLogic_h
#define __vtkSlicerAstroSmoothingLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerCLIModuleLogic;
class vtkSlicerAstroVolumeLogic;
// vtk includes
class vtkMatrix4x4;
// AstroSmoothings includes
#include "vtkSlicerAstroSmoothingModuleLogicExport.h"
class vtkMRMLAstroSmoothingParametersNode;

/// \ingroup Slicer_QtModules_AstroSmoothing
class VTK_SLICER_ASTROSMOOTHING_MODULE_LOGIC_EXPORT vtkSlicerAstroSmoothingLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroSmoothingLogic *New();
  vtkTypeMacro(vtkSlicerAstroSmoothingLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent);

  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  virtual void RegisterNodes();

  int Apply(vtkMRMLAstroSmoothingParametersNode *pnode);

protected:
  vtkSlicerAstroSmoothingLogic();
  virtual ~vtkSlicerAstroSmoothingLogic();

  int AnisotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int IsotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

  int AnisotropicBoxGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int IsotropicBoxGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

  int AnisotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int IsotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

  int AnisotropicGaussianGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int IsotropicGaussianGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

  int GradientCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int GradientGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

private:
  vtkSlicerAstroSmoothingLogic(const vtkSlicerAstroSmoothingLogic&); // Not implemented
  void operator=(const vtkSlicerAstroSmoothingLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

