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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

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

