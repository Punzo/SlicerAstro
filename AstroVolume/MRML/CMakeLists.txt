project(vtkSlicer${MODULE_NAME}ModuleMRML)

set(module_mrml_name "vtkSlicer${MODULE_NAME}ModuleMRML")

# --------------------------------------------------------------------------
# Include dirs
# --------------------------------------------------------------------------
set(module_mrml_include_directory
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${MRMLCore_INCLUDE_DIRS}
  ${WCSLIB_INCLUDE_DIR}
  ${CFITSIO_INCLUDE_DIR}
  ${vtkFits_INCLUDE_DIRS}
  )
include_directories(BEFORE ${module_mrml_include_directory})


# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
set(module_mrml_SRCS
    vtkMRMLAstroVolumeNode.cxx
    vtkMRMLAstroVolumeNode.h
    vtkMRMLAstroVolumeDisplayNode.cxx
    vtkMRMLAstroVolumeDisplayNode.h
    vtkMRMLAstroVolumeStorageNode.cxx
    vtkMRMLAstroVolumeStorageNode.h
    vtkMRMLAstroLabelMapVolumeNode.cxx
    vtkMRMLAstroLabelMapVolumeNode.h
    vtkMRMLAstroLabelMapVolumeDisplayNode.cxx
    vtkMRMLAstroLabelMapVolumeDisplayNode.h)

# The header '${module_mrml_name}Export.h' will be automatically configured.
set(module_mrml_export_directive "VTK_MRMLASTRO_EXPORT")

set(module_mrml_target_libraries
  ${MRML_LIBRARIES}
  vtkFits
  )

SlicerMacroBuildModuleMRML(
  NAME ${module_mrml_name}
  EXPORT_DIRECTIVE ${module_mrml_export_directive}
  INCLUDE_DIRECTORIES ${module_mrml_include_directories}
  SRCS ${module_mrml_SRCS}
  TARGET_LIBRARIES ${module_mrml_target_libraries}
  )
