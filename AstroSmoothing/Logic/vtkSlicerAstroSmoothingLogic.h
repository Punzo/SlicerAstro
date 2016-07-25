#ifndef __vtkSlicerAstroSmoothingLogic_h
#define __vtkSlicerAstroSmoothingLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerCLIModuleLogic;
class vtkSlicerAstroVolumeLogic;
// vtk includes
class vtkMatrix4x4;
class vtkRenderWindow;
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

  int Apply(vtkMRMLAstroSmoothingParametersNode *pnode, vtkRenderWindow *renderWindow);

protected:
  vtkSlicerAstroSmoothingLogic();
  virtual ~vtkSlicerAstroSmoothingLogic();

  // note: CPU filters imeplemented here have more functionality respect to
  // the vtkImageFilters. However, these CPU filtering methods
  // should be rewritten as classes in order to be reusable and to cleanup vtkSlicerAstroSmoothing.cxx.
  int AnisotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int IsotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int BoxGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode, vtkRenderWindow* renderWindow);

  int AnisotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int IsotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int GaussianGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode, vtkRenderWindow* renderWindow);

  int GradientCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);
  int GradientGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode, vtkRenderWindow* renderWindow);

private:
  vtkSlicerAstroSmoothingLogic(const vtkSlicerAstroSmoothingLogic&); // Not implemented
  void operator=(const vtkSlicerAstroSmoothingLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

