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

#ifndef __vtkSlicerAstroMomentMapsLogic_h
#define __vtkSlicerAstroMomentMapsLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;
// vtk includes
class vtkRenderWindow;
// AstroMomentMapss includes
#include "vtkSlicerAstroMomentMapsModuleLogicExport.h"
class vtkMRMLAstroMomentMapsParametersNode;

/// \ingroup Slicer_QtModules_AstroMomentMaps
class VTK_SLICER_ASTROMOMENTMAPS_MODULE_LOGIC_EXPORT vtkSlicerAstroMomentMapsLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroMomentMapsLogic *New();
  vtkTypeMacro(vtkSlicerAstroMomentMapsLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent);

  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  virtual void RegisterNodes();

  bool CalculateMomentMaps(vtkMRMLAstroMomentMapsParametersNode *pnode);

protected:
  vtkSlicerAstroMomentMapsLogic();
  virtual ~vtkSlicerAstroMomentMapsLogic();

private:
  vtkSlicerAstroMomentMapsLogic(const vtkSlicerAstroMomentMapsLogic&); // Not implemented
  void operator=(const vtkSlicerAstroMomentMapsLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

