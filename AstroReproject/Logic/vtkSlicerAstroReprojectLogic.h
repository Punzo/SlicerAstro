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

#ifndef __vtkSlicerAstroReprojectLogic_h
#define __vtkSlicerAstroReprojectLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;
// AstroReprojects includes
#include "vtkSlicerAstroReprojectModuleLogicExport.h"
class vtkMRMLAstroReprojectParametersNode;

/// \ingroup Slicer_QtModules_AstroReproject
class VTK_SLICER_ASTROREPROJECT_MODULE_LOGIC_EXPORT vtkSlicerAstroReprojectLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroReprojectLogic *New();
  vtkTypeMacro(vtkSlicerAstroReprojectLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent);

  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  virtual void RegisterNodes();

  bool Reproject(vtkMRMLAstroReprojectParametersNode *pnode);

protected:
  vtkSlicerAstroReprojectLogic();
  virtual ~vtkSlicerAstroReprojectLogic();

private:
  vtkSlicerAstroReprojectLogic(const vtkSlicerAstroReprojectLogic&); // Not implemented
  void operator=(const vtkSlicerAstroReprojectLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

