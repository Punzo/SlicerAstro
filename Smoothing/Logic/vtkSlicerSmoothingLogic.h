#ifndef __vtkSlicerSmoothingLogic_h
#define __vtkSlicerSmoothingLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkSlicerCLIModuleLogic;
class vtkSlicerAstroVolumeLogic;
class vtkMRMLVolumeNode;
// vtk includes
class vtkMatrix4x4;
// Smoothings includes
#include "vtkSlicerSmoothingModuleLogicExport.h"
class vtkMRMLSmoothingParametersNode;

/// \ingroup Slicer_QtModules_Smoothing
class VTK_SLICER_SMOOTHING_MODULE_LOGIC_EXPORT vtkSlicerSmoothingLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerSmoothingLogic *New();
  vtkTypeMacro(vtkSlicerSmoothingLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent);

  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  virtual void RegisterNodes();

  int Apply(vtkMRMLSmoothingParametersNode *pnode);

protected:
  vtkSlicerSmoothingLogic();
  virtual ~vtkSlicerSmoothingLogic();

  int GaussianCPUFilter(vtkMRMLSmoothingParametersNode *pnode);
  int GradientCPUFilter(vtkMRMLSmoothingParametersNode *pnode);
  int WaveletLiftingCPUFilter(vtkMRMLSmoothingParametersNode *pnode);

private:
  vtkSlicerSmoothingLogic(const vtkSlicerSmoothingLogic&); // Not implemented
  void operator=(const vtkSlicerSmoothingLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

