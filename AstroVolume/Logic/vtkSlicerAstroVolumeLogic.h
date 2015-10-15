// .NAME vtkSlicerAstroVolumeLogic - slicer logic class for AstroVolume manipulation
// .SECTION Description
// This class manages the logic associated with reading, saving,
// and changing properties of AstroVolume


#ifndef __vtkSlicerAstroVolumeLogic_h
#define __vtkSlicerAstroVolumeLogic_h

// Slicer includes
#include <vtkSlicerModuleLogic.h>

// STD includes
#include <cstdlib>

#include "vtkSlicerAstroVolumeModuleLogicExport.h"

class vtkSlicerVolumesLogic;
class vtkMRMLVolumeNode;
class vtkSlicerUnitsLogic;
class vtkMRMLSliceCompositeNode;

class VTK_SLICER_ASTROVOLUME_MODULE_LOGIC_EXPORT vtkSlicerAstroVolumeLogic :
  public vtkSlicerModuleLogic
{
public:

  static vtkSlicerAstroVolumeLogic *New();
  vtkTypeMacro(vtkSlicerAstroVolumeLogic,vtkSlicerModuleLogic);
  void PrintSelf(ostream& os, vtkIndent indent);

  /// Register the factory that the MultiVolume needs to manage nrrd
  /// file with the specified volumes logic
  void RegisterArchetypeVolumeNodeSetFactory(vtkSlicerVolumesLogic* volumesLogic);

  /// Write volume's image data to a specified file
  int SaveArchetypeVolume (const char* filename, vtkMRMLVolumeNode *volumeNode);

  /// Return the scene containing the volume rendering presets.
  /// If there is no presets scene, a scene is created and presets are loaded
  /// into.
  /// The presets scene is loaded from a file (presets.xml) located in the
  /// module share directory
  /// \sa vtkMRMLVolumePropertyNode, GetModuleShareDirectory()
  vtkMRMLScene *GetPresetsScene();

  /// \brief synchronizePresetsToVolumeNode
  /// \param volumeNode,
  /// \return succees
  bool synchronizePresetsToVolumeNode(vtkMRMLNode *node);

protected:
  vtkSlicerAstroVolumeLogic();
  virtual ~vtkSlicerAstroVolumeLogic();

  /// Register MRML Node classes to Scene. Gets called automatically when the MRMLScene is attached to this logic class.
  virtual void RegisterNodes();

  virtual void SetMRMLSceneInternal(vtkMRMLScene * newScene);
  virtual void UpdateFromMRMLScene();
  virtual void OnMRMLSceneNodeAdded(vtkMRMLNode* node);
  virtual void OnMRMLSceneNodeRemoved(vtkMRMLNode* node);

  bool LoadPresets(vtkMRMLScene* scene);
  vtkMRMLScene* PresetsScene;

private:

  bool UnitInit;
  vtkSlicerAstroVolumeLogic(const vtkSlicerAstroVolumeLogic&); // Not implemented
  void operator=(const vtkSlicerAstroVolumeLogic&);               // Not implemented

};

#endif

