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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

#ifndef __vtkMRMLAstroTwoDAxesDisplayableManager_h
#define __vtkMRMLAstroTwoDAxesDisplayableManager_h

// MRMLDisplayableManager includes
#include "vtkMRMLAbstractDisplayableManager.h"

#include <vtkSlicerAstroVolumeModuleMRMLExport.h>

// STL includes
#include <vector>

/// \brief Displayable manager that displays 2D WCS axes in 2D view
class VTK_MRML_ASTRO_EXPORT vtkMRMLAstroTwoDAxesDisplayableManager
  : public vtkMRMLAbstractDisplayableManager
{
  friend class vtkAstroTwoDAxesRendererUpdateObserver;

public:
  static vtkMRMLAstroTwoDAxesDisplayableManager* New();
  vtkTypeMacro(vtkMRMLAstroTwoDAxesDisplayableManager,vtkMRMLAbstractDisplayableManager);
  void PrintSelf(ostream& os, vtkIndent indent);

protected:

  vtkMRMLAstroTwoDAxesDisplayableManager();
  virtual ~vtkMRMLAstroTwoDAxesDisplayableManager();

  /// Observe the View node and initialize the renderer accordingly.
  virtual void Create() VTK_OVERRIDE;

  /// Called each time the view node is modified.
  /// Internally update the renderer from the view node.
  /// \sa UpdateFromMRMLViewNode()
  virtual void OnMRMLDisplayableNodeModifiedEvent(vtkObject* caller) VTK_OVERRIDE;

  /// Update the renderer from the view node properties.
  void UpdateFromViewNode();

  /// Update the renderer based on the master renderer (the one that the orientation marker follows)
  void UpdateFromRenderer();


private:

  vtkMRMLAstroTwoDAxesDisplayableManager(const vtkMRMLAstroTwoDAxesDisplayableManager&);// Not implemented
  void operator=(const vtkMRMLAstroTwoDAxesDisplayableManager&); // Not Implemented

  class vtkInternal;
  vtkInternal * Internal;
};

#endif
