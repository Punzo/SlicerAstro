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

/// \class vtkSlicerAstroMaskingLogic
/// \brief Blank or Crop a volume given a selection (ROI or segmentation).
///
/// This class implements blanking and cropping.
/// Two main use cases:
///
/// 1. Remove artifacts from the data;
///
/// 2. Reduce size (both extent and resolution) of a large volume.
/// Size reduction is useful, as it reduces memory need and makes
/// visualization and processing faster.
///
/// Limitations:
/// * Region of interes (ROI) node cannot be under non-linear transform
///   and it must be aligned with the input data.
/// * Cropped output volume node cannot be under non-linear transform
///
/// \ingroup SlicerAstro_QtModules_AstroMasking
class VTK_SLICERASTRO_ASTROMASKING_MODULE_LOGIC_EXPORT vtkSlicerAstroMaskingLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroMaskingLogic *New();
  vtkTypeMacro(vtkSlicerAstroMaskingLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set AstroVolume module logic
  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);

  /// Get AstroVolume module logic
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  /// Register MRML Node classes to Scene.
  /// Gets called automatically when the MRMLScene is attached to this logic class
  virtual void RegisterNodes() override;

  /// Convenience function which calls ApplyBlank or ApplyCrop.
  /// \param MRML parameter node
  /// \param MRML segmentation node (if not using a ROI)
  /// \param the selected segment from the MRML segmentation node (if not using a ROI)
  /// \return Success flag
  bool ApplyMask(vtkMRMLAstroMaskingParametersNode *pnode,
                 vtkMRMLSegmentationNode *segmentationNode,
                 vtkSegment *segment);

  /// Apply Blank algorithm
  /// \param MRML parameter node
  /// \return Success flag
  bool ApplyBlank(vtkMRMLAstroMaskingParametersNode *pnode);

  /// Apply Crop algorithm
  /// \param MRML parameter node
  /// \param MRML segmentation node (if not using a ROI)
  /// \param the selected segment from the MRML segmentation node (if not using a ROI)
  /// \return Success flag
  bool ApplyCrop(vtkMRMLAstroMaskingParametersNode *pnode,
                 vtkMRMLSegmentationNode *segmentationNode,
                 vtkSegment *segment);

  /// Sets ROI to fit to input volume.
  /// If ROI is under a non-linear transform then the ROI transform will be reset to RAS.
  /// \param MRML parameter node
  /// \return Success flag
  virtual bool FitROIToInputVolume(vtkMRMLAstroMaskingParametersNode* parametersNode);

  /// Snap the ROI to the voxel grid of the input volume
  /// \param MRML parameter node
  virtual void SnapROIToVoxelGrid(vtkMRMLAstroMaskingParametersNode* parametersNode);

  /// Check if the ROI is aligned with the input volume
  /// \param MRML parameter node
  /// \return Success flag
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

