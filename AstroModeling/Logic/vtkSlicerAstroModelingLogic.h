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

#ifndef __vtkSlicerAstroModelingLogic_h
#define __vtkSlicerAstroModelingLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;
// vtk includes
class vtkRenderWindow;
// AstroModelings includes
#include "vtkSlicerAstroModelingModuleLogicExport.h"
class vtkMRMLAstroModelingParametersNode;

/// \ingroup Slicer_QtModules_AstroModeling
class VTK_SLICER_ASTROMODELING_MODULE_LOGIC_EXPORT vtkSlicerAstroModelingLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroModelingLogic *New();
  vtkTypeMacro(vtkSlicerAstroModelingLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent);

  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  virtual void RegisterNodes();

  int FitModel(vtkMRMLAstroModelingParametersNode *pnode);

  int UpdateTable(vtkMRMLAstroModelingParametersNode *pnode);

  int UpdateModelFromTable(vtkMRMLAstroModelingParametersNode *pnode);

protected:
  vtkSlicerAstroModelingLogic();
  virtual ~vtkSlicerAstroModelingLogic();

private:
  vtkSlicerAstroModelingLogic(const vtkSlicerAstroModelingLogic&); // Not implemented
  void operator=(const vtkSlicerAstroModelingLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

