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
#include <functional>

#include "vtkSlicerAstroStatisticsModuleLogicExport.h"

#include <string>
class vtkMRMLAstroStatisticsParametersNode;
class vtkMRMLMarkupsROINode;
class vtkMRMLTableNode;

/// \class vtkSlicerAstroStatisticsLogic
/// \brief Calculate statistics given selection (ROI or segmentation).
///
/// \ingroup SlicerAstro_QtModules_AstroStatistics
class VTK_SLICERASTRO_ASTROSTATISTICS_MODULE_LOGIC_EXPORT vtkSlicerAstroStatisticsLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroStatisticsLogic *New();
  vtkTypeMacro(vtkSlicerAstroStatisticsLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set AstroVolume module logic
  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);

  /// Get AstroVolume module logic
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  /// Register MRML Node classes to Scene.
  /// Gets called automatically when the MRMLScene is attached to this logic class
  virtual void RegisterNodes() override;

  /// Run statistics calculation algorithm
  /// \param MRML parameter node
  /// \return Success flag
  /// The region is set by the Mode of the parameters node: "Volume" (all
  /// the voxels), "ROI" (the box) or "Segmentation" (a segment of a mask).
  /// The results are added as a row of the table of the parameters node
  /// (EnsureStatisticsTable). On failure the reason is in GetLastError.
  /// Synchronous: returns when the statistics are computed (scripts).
  bool CalculateStatistics(vtkMRMLAstroStatisticsParametersNode *pnode);

#ifndef __VTK_WRAP__
  /// Start the computation without blocking the user interface; done(success)
  /// is called in the GUI thread at its end. Returns false if it could not
  /// start (done is not called). Synchronous if synchronous is true.
  bool StartCalculateStatistics(vtkMRMLAstroStatisticsParametersNode *pnode,
                                const std::function<void(bool)>& done, bool synchronous = false);
#endif
  std::string GetLastError() { return this->LastError; }

  /// The table of the results of the parameters node: created if needed
  /// (columns Selection, Npixels, Min, Max, Mean, Std, Median, Sum,
  /// TotalFlux; one row per calculation), units of the input data
  vtkMRMLTableNode* EnsureStatisticsTable(vtkMRMLAstroStatisticsParametersNode* pnode);
  /// The box (ROI) of the parameters node: created if needed (fitted to the
  /// input data)
  vtkMRMLMarkupsROINode* EnsureStatisticsROI(vtkMRMLAstroStatisticsParametersNode* pnode);

  /// Sets ROI to fit to input volume.
  /// If ROI is under a non-linear transform then the ROI transform will be reset to RAS.
  /// \param MRML parameter node
  /// \return Success flag
  virtual bool FitROIToInputVolume(vtkMRMLAstroStatisticsParametersNode* parametersNode);

  /// Snap the ROI to the voxel grid of the input volume
  /// \param MRML parameter node
  virtual void SnapROIToVoxelGrid(vtkMRMLAstroStatisticsParametersNode* parametersNode);

  /// Check if the ROI is aligned with the input volume
  /// \param MRML parameter node
  /// \return Success flag
  virtual bool IsROIAlignedWithInputVolume(vtkMRMLAstroStatisticsParametersNode* parametersNode);

protected:
  std::string LastError;
  vtkSlicerAstroStatisticsLogic();
  virtual ~vtkSlicerAstroStatisticsLogic();

private:
  vtkSlicerAstroStatisticsLogic(const vtkSlicerAstroStatisticsLogic&); // Not implemented
  void operator=(const vtkSlicerAstroStatisticsLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

