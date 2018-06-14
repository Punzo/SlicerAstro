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

#ifndef __vtkSlicerAstroModelingLogic_h
#define __vtkSlicerAstroModelingLogic_h

// Slicer includes
#include "vtkSlicerModuleLogic.h"
class vtkMRMLTableNode;
class vtkMRMLVolumeNode;
class vtkSlicerAstroVolumeLogic;
class vtkSlicerMarkupsLogic;

// AstroModelings includes
#include "vtkSlicerAstroModelingModuleLogicExport.h"
class vtkMRMLAstroModelingParametersNode;
class vtkMRMLAstroVolumeDisplayNode;

/// \class vtkSlicerAstroModelingLogic
/// \brief This is a C++ SlicerAstro wrapping of 3DBAROLO,
/// a 3D algorithm to derive rotation curves of galaxies,
/// developed by Enrico di Teodoro.
///
/// \ingroup SlicerAstro_QtModules_AstroModeling
class VTK_SLICERASTRO_ASTROMODELING_MODULE_LOGIC_EXPORT vtkSlicerAstroModelingLogic
  : public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroModelingLogic *New();
  vtkTypeMacro(vtkSlicerAstroModelingLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /// Set AstroVolume module logic
  void SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic);

  /// Get AstroVolume module logic
  vtkSlicerAstroVolumeLogic* GetAstroVolumeLogic();

  /// Set Markups module logic
  void SetMarkupsLogic(vtkSlicerMarkupsLogic* logic);

  /// Get Markups module logic
  vtkSlicerMarkupsLogic* GetMarkupsLogic();

  /// Register MRML Node classes to Scene.
  /// Gets called automatically when the MRMLScene is attached to this logic class
  virtual void RegisterNodes() VTK_OVERRIDE;

  /// Run Estimation, Crate Model or Fitting Model
  /// \param MRML parameter node
  /// \param MRML table node
  /// \return Success flag
  int OperateModel(vtkMRMLAstroModelingParametersNode *pnode,
                   vtkMRMLTableNode *tnode);

  /// Recalculate the model from new parameters in the mrml table
  /// \param MRML parameter node
  /// \param MRML table node
  /// \return Success flag
  int UpdateModelFromTable(vtkMRMLAstroModelingParametersNode *pnode);

  /// Clean all the pointers of the instantiated calsses of 3DBarolo
  void cleanPointers();

protected:
  vtkSlicerAstroModelingLogic();
  virtual ~vtkSlicerAstroModelingLogic();

  /// Calculate the wcs velocity in the center of the data-cube
  /// \param MRML astro volume display node
  /// \return value
  double CalculateCentralVelocity(vtkMRMLAstroVolumeDisplayNode* volumeDisplayNode);

private:
  vtkSlicerAstroModelingLogic(const vtkSlicerAstroModelingLogic&); // Not implemented
  void operator=(const vtkSlicerAstroModelingLogic&);           // Not implemented

  class vtkInternal;
  vtkInternal* Internal;
};

#endif

