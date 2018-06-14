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

#ifndef __vtkSlicerAstroSmoothingLogic_h
#define __vtkSlicerAstroSmoothingLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;
// vtk includes
class vtkRenderWindow;
// AstroSmoothings includes
#include "vtkSlicerAstroSmoothingModuleLogicExport.h"
class vtkMRMLAstroSmoothingParametersNode;

/// \class vtkSlicerAstroSmoothingLogic
/// \brief AstroSmoothing module filters a Volume using several techniques.
/// The algorithms are optmized for Astronomical Neutral Hydrogen (HI) data.
///
/// The set of filters are: anisotropic box; anisotropic Gaussian; intensity-driven gradient.
/// These algorithms are available in SlicerAstro as parallelized implementations on both
/// CPU and GPU hardware, offering interactive performance when processing data-cubes
/// of dimensions up to 10^7 voxels and very fast performance (< 3.5 sec)
/// for larger ones (up to 10^8 voxels).

/// The intensity-driven gradient filter, due to its adaptive characteristics,
/// is the optimal choice for HI data. Therefore,
/// it is the default method when the automatic mode has been chosen.
/// This algorithm preserves the detailed structure of the signal with
/// high signal-to-noise ratio (> 3) at the highest resolution, while
/// smoothing only the faint part of the signal (signal-to-noise ratio < 3).
/// For more information regarding the filters and their performance,
/// default parameters, advantages and disadvantages, we refer
/// to 10.1016/j.ascom.2016.09.002.
///
/// \ingroup SlicerAstro_QtModules_AstroSmoothing
class VTK_SLICERASTRO_ASTROSMOOTHING_MODULE_LOGIC_EXPORT vtkSlicerAstroSmoothingLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroSmoothingLogic *New();
  vtkTypeMacro(vtkSlicerAstroSmoothingLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /// Set AstroVolume module logic
  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);

  /// Get AstroVolume module logic
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  /// Register MRML Node classes to Scene.
  /// Gets called automatically when the MRMLScene is attached to this logic class
  virtual void RegisterNodes() VTK_OVERRIDE;

  /// Run smoothing algorithm
  /// \param MRML parameter node
  /// \param vtkRenderWindow to init the GPU algorithm
  /// \return Success flag
  int Apply(vtkMRMLAstroSmoothingParametersNode *pnode, vtkRenderWindow *renderWindow);

protected:
  vtkSlicerAstroSmoothingLogic();
  virtual ~vtkSlicerAstroSmoothingLogic();

  /// Run anisotropic box filter algorithm on CPU
  /// \param MRML parameter node
  /// \return Success flag
  int AnisotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

  /// Run isotropic box filter algorithm on CPU
  /// \param MRML parameter node
  /// \return Success flag
  int IsotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

  /// Run box filter algorithm on GPU
  /// \param MRML parameter node
  /// \param vtkRenderWindow to init the GPU algorithm
  /// \return Success flag
  int BoxGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode, vtkRenderWindow* renderWindow);

  /// Run anisotropic Gaussian filter algorithm on CPU
  /// \param MRML parameter node
  /// \return Success flag
  int AnisotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

  /// Run isotropic Gaussian filter algorithm on CPU
  /// \param MRML parameter node
  /// \return Success flag
  int IsotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

  /// Run Gaussian filter algorithm on GPU
  /// \param MRML parameter node
  /// \param vtkRenderWindow to init the GPU algorithm
  /// \return Success flag
  int GaussianGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode, vtkRenderWindow* renderWindow);

  /// Run intensity-driven gradient filter algorithm on CPU
  /// \param MRML parameter node
  /// \return Success flag
  int GradientCPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode);

  /// Run intensity-driven gradient filter algorithm on GPU
  /// \param MRML parameter node
  /// \param vtkRenderWindow to init the GPU algorithm
  /// \return Success flag
  int GradientGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode, vtkRenderWindow* renderWindow);

private:
  vtkSlicerAstroSmoothingLogic(const vtkSlicerAstroSmoothingLogic&); // Not implemented
  void operator=(const vtkSlicerAstroSmoothingLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

