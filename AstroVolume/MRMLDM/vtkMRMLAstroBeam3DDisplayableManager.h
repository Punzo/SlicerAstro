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

==============================================================================*/

#ifndef __vtkMRMLAstroBeam3DDisplayableManager_h
#define __vtkMRMLAstroBeam3DDisplayableManager_h

// MRMLDisplayableManager includes
#include "vtkMRMLAbstractDisplayableManager.h"
#include <vtkSlicerAstroVolumeModuleMRMLDisplayableManagerExport.h>

class vtkActor;
class vtkProp;
class vtkPolyData;

/// \brief Displayable manager of the 3D views: the resolution element of the
/// cube rendered in the view, an ellipsoid with the FWHM of the beam on the
/// sky (BMAJ x BMIN at BPA) and the FWHM of the spectral response along the
/// spectral axis (vtkMRMLAstroVolumeNode::GetSpectralFWHM), in the corner of
/// the data at the first pixel and channel.
///
/// Shown when the view node has the attribute SlicerAstro.Beam3D = "on"
/// (qSlicerAstroBeam3DButton) and a cube with a beam is rendered in the view
/// (vtkMRMLAstroVolumeNode::GetBeam3DVolume).
class VTK_MRMLDISPLAYABLEMANAGER_ASTRO_EXPORT vtkMRMLAstroBeam3DDisplayableManager
  : public vtkMRMLAbstractDisplayableManager
{
  friend class vtkAstroBeam3DRendererUpdateObserver;

public:
  static vtkMRMLAstroBeam3DDisplayableManager* New();
  vtkTypeMacro(vtkMRMLAstroBeam3DDisplayableManager, vtkMRMLAbstractDisplayableManager);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Name of the attribute of the view node that shows the resolution element
  static const char* GetVisibilityAttributeName() { return "SlicerAstro.Beam3D"; }

  /// The ellipsoid (RAS coordinates) and its actor
  vtkPolyData* GetEllipsoidPolyData();
  vtkActor* GetEllipsoidActor();
  /// The text "beam" next to the ellipsoid
  vtkProp* GetLabelActor();

protected:
  vtkMRMLAstroBeam3DDisplayableManager();
  ~vtkMRMLAstroBeam3DDisplayableManager() override;

  void Create() override;

  /// The view node is changed: rendered again (the ellipsoid is updated
  /// before the render)
  void OnMRMLDisplayableNodeModifiedEvent(vtkObject* caller) override;

  /// Before each render of the view: the ellipsoid is updated if its inputs
  /// changed (cube, beam, spectral FWHM, geometry)
  void UpdateFromRenderer();

private:
  vtkMRMLAstroBeam3DDisplayableManager(const vtkMRMLAstroBeam3DDisplayableManager&) = delete;
  void operator=(const vtkMRMLAstroBeam3DDisplayableManager&) = delete;

  class vtkInternal;
  vtkInternal* Internal;
};

#endif
