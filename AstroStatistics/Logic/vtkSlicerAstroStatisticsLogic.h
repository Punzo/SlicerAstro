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

#ifndef __vtkSlicerAstroStatisticsLogic_h
#define __vtkSlicerAstroStatisticsLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;

// AstroStatisticss includes
#include "vtkSlicerAstroStatisticsModuleLogicExport.h"
class vtkMRMLAstroStatisticsParametersNode;

/// \ingroup Slicer_QtModules_AstroStatistics
class VTK_SLICERASTRO_ASTROSTATISTICS_MODULE_LOGIC_EXPORT vtkSlicerAstroStatisticsLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroStatisticsLogic *New();
  vtkTypeMacro(vtkSlicerAstroStatisticsLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  virtual void RegisterNodes() VTK_OVERRIDE;

  bool CalculateStatistics(vtkMRMLAstroStatisticsParametersNode *pnode);

  /// Sets ROI to fit to input volume.
  /// If ROI is under a non-linear transform then the ROI transform will be reset to RAS.
  virtual bool FitROIToInputVolume(vtkMRMLAstroStatisticsParametersNode* parametersNode);

  virtual void SnapROIToVoxelGrid(vtkMRMLAstroStatisticsParametersNode* parametersNode);

  virtual bool IsROIAlignedWithInputVolume(vtkMRMLAstroStatisticsParametersNode* parametersNode);

protected:
  vtkSlicerAstroStatisticsLogic();
  virtual ~vtkSlicerAstroStatisticsLogic();

private:
  vtkSlicerAstroStatisticsLogic(const vtkSlicerAstroStatisticsLogic&); // Not implemented
  void operator=(const vtkSlicerAstroStatisticsLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

