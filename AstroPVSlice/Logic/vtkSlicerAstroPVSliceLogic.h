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

#ifndef __vtkSlicerAstroPVSliceLogic_h
#define __vtkSlicerAstroPVSliceLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerAstroMomentMapsLogic;
class vtkSlicerAstroVolumeLogic;

// AstroPVSlices includes
#include "vtkSlicerAstroPVSliceModuleLogicExport.h"
class vtkMRMLAstroPVSliceParametersNode;

/// \ingroup Slicer_QtModules_AstroPVSlice
class VTK_SLICER_ASTROPVSLICE_MODULE_LOGIC_EXPORT vtkSlicerAstroPVSliceLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroPVSliceLogic *New();
  vtkTypeMacro(vtkSlicerAstroPVSliceLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent);

  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  void SetAstroMomentMapsLogic(vtkSlicerAstroMomentMapsLogic* logic);
  vtkSlicerAstroMomentMapsLogic *GetAstroMomentMapsLogic();

  virtual void RegisterNodes();

  bool Calculate0thMomentMap(vtkMRMLAstroPVSliceParametersNode *pnode);
  bool SetMomentMapOnRedWidget(vtkMRMLAstroPVSliceParametersNode *pnode);

  bool CreateAndSetRuler(vtkMRMLAstroPVSliceParametersNode *pnode);
  bool InitializeRuler(vtkMRMLAstroPVSliceParametersNode *pnode);
  bool UpdateRuler(vtkMRMLAstroPVSliceParametersNode *pnode);
  bool UpdateRulerFromCenter(vtkMRMLAstroPVSliceParametersNode *pnode);

  bool InitializePV(vtkMRMLAstroPVSliceParametersNode *pnode);
  bool UpdatePV(vtkMRMLAstroPVSliceParametersNode *pnode);

protected:
  vtkSlicerAstroPVSliceLogic();
  virtual ~vtkSlicerAstroPVSliceLogic();

private:
  vtkSlicerAstroPVSliceLogic(const vtkSlicerAstroPVSliceLogic&); // Not implemented
  void operator=(const vtkSlicerAstroPVSliceLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

