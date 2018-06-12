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

#ifndef __vtkSlicerAstroPVDiagramLogic_h
#define __vtkSlicerAstroPVDiagramLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerAstroMomentMapsLogic;
class vtkSlicerAstroVolumeLogic;

// AstroPVDiagrams includes
#include "vtkSlicerAstroPVDiagramModuleLogicExport.h"
class vtkMRMLAstroPVDiagramParametersNode;

/// \ingroup Slicer_QtModules_AstroPVDiagram
class VTK_SLICERASTRO_ASTROPVDIAGRAM_MODULE_LOGIC_EXPORT vtkSlicerAstroPVDiagramLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroPVDiagramLogic *New();
  vtkTypeMacro(vtkSlicerAstroPVDiagramLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  void SetAstroMomentMapsLogic(vtkSlicerAstroMomentMapsLogic* logic);
  vtkSlicerAstroMomentMapsLogic *GetAstroMomentMapsLogic();

  virtual void RegisterNodes() VTK_OVERRIDE;

  bool Calculate0thMomentMap(vtkMRMLAstroPVDiagramParametersNode *pnode);
  bool SetMomentMapOnRedWidget(vtkMRMLAstroPVDiagramParametersNode *pnode);

  bool UpdateSliceSelection(vtkMRMLAstroPVDiagramParametersNode *pnode);

  bool GenerateAndSetPVDiagram(vtkMRMLAstroPVDiagramParametersNode *pnode);

protected:
  vtkSlicerAstroPVDiagramLogic();
  virtual ~vtkSlicerAstroPVDiagramLogic();

private:
  vtkSlicerAstroPVDiagramLogic(const vtkSlicerAstroPVDiagramLogic&); // Not implemented
  void operator=(const vtkSlicerAstroPVDiagramLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

