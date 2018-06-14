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

#ifndef __vtkMRMLAstroBeamDisplayableManager_h
#define __vtkMRMLAstroBeamDisplayableManager_h

// MRMLDisplayableManager includes
#include "vtkMRMLAbstractDisplayableManager.h"

#include <vtkSlicerAstroVolumeModuleMRMLDisplayableManagerExport.h>

// STL includes
#include <vector>

/// \brief Displayable manager that displays the beam in 2D view
class VTK_MRMLDISPLAYABLEMANAGER_ASTRO_EXPORT vtkMRMLAstroBeamDisplayableManager
  : public vtkMRMLAbstractDisplayableManager
{
  friend class vtkAstroBeamRendererUpdateObserver;

public:
  static vtkMRMLAstroBeamDisplayableManager* New();
  vtkTypeMacro(vtkMRMLAstroBeamDisplayableManager,vtkMRMLAbstractDisplayableManager);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /// Get vtkMarkerRenderer
  vtkRenderer* vtkMarkerRenderer();

  /// Set annotation color
  void SetAnnotationsColor(double red,
                           double green,
                           double blue);

protected:

  vtkMRMLAstroBeamDisplayableManager();
  virtual ~vtkMRMLAstroBeamDisplayableManager();

  /// Observe the View node and initialize the renderer accordingly.
  virtual void Create() VTK_OVERRIDE;

  /// Called each time the view node is modified.
  /// Internally update the renderer from the view node
  /// \sa UpdateFromMRMLViewNode()
  virtual void OnMRMLDisplayableNodeModifiedEvent(vtkObject* caller) VTK_OVERRIDE;

  /// Update the renderer from the view node properties.
  void UpdateFromViewNode();

  /// Update the renderer based on the master renderer (the one that the orientation marker follows)
  void UpdateFromRenderer();


private:

  vtkMRMLAstroBeamDisplayableManager(const vtkMRMLAstroBeamDisplayableManager&);// Not implemented
  void operator=(const vtkMRMLAstroBeamDisplayableManager&); // Not Implemented

  class vtkInternal;
  vtkInternal * Internal;
};

#endif
