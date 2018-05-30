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

#ifndef __vtkSlicerAstroMaskingLogic_h
#define __vtkSlicerAstroMaskingLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLSegmentationNode;
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;

// vtk includes
class vtkSegment;

// AstroMaskings includes
#include "vtkSlicerAstroMaskingModuleLogicExport.h"
class vtkMRMLAstroMaskingParametersNode;

/// \ingroup Slicer_QtModules_AstroMasking
class VTK_SLICER_ASTROMASKING_MODULE_LOGIC_EXPORT vtkSlicerAstroMaskingLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroMaskingLogic *New();
  vtkTypeMacro(vtkSlicerAstroMaskingLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  virtual void RegisterNodes() VTK_OVERRIDE;

  bool ApplyMask(vtkMRMLAstroMaskingParametersNode *pnode,
                 vtkMRMLSegmentationNode *segmentationNode,
                 vtkSegment *segment);

  bool ApplyBlank(vtkMRMLAstroMaskingParametersNode *pnode);

  bool ApplyCrop(vtkMRMLAstroMaskingParametersNode *pnode,
                 vtkMRMLSegmentationNode *segmentationNode,
                 vtkSegment *segment);

  /// Sets ROI to fit to input volume.
  /// If ROI is under a non-linear transform then the ROI transform will be reset to RAS.
  virtual bool FitROIToInputVolume(vtkMRMLAstroMaskingParametersNode* parametersNode);

  virtual void SnapROIToVoxelGrid(vtkMRMLAstroMaskingParametersNode* parametersNode);

  virtual bool IsROIAlignedWithInputVolume(vtkMRMLAstroMaskingParametersNode* parametersNode);

protected:
  vtkSlicerAstroMaskingLogic();
  virtual ~vtkSlicerAstroMaskingLogic();

private:
  vtkSlicerAstroMaskingLogic(const vtkSlicerAstroMaskingLogic&); // Not implemented
  void operator=(const vtkSlicerAstroMaskingLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

