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
  virtual void Create();

  /// Called each time the view node is modified.
  /// Internally update the renderer from the view node.
  /// \sa UpdateFromMRMLViewNode()
  virtual void OnMRMLDisplayableNodeModifiedEvent(vtkObject* caller);

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
