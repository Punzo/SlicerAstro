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
  ${Slicer_Base_INCLUDE_DIRS}
  )
include_directories(BEFORE ${module_mrml_include_directory})

# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
set(module_mrml_SRCS
    vtkMRMLAstroLabelMapVolumeDisplayNode.cxx
    vtkMRMLAstroLabelMapVolumeDisplayNode.h
    vtkMRMLAstroLabelMapVolumeNode.cxx
    vtkMRMLAstroLabelMapVolumeNode.h
    vtkMRMLAstroModelingParametersNode.cxx
    vtkMRMLAstroModelingParametersNode.h
    vtkMRMLAstroMomentMapsParametersNode.cxx
    vtkMRMLAstroMomentMapsParametersNode.h
    vtkMRMLAstroSmoothingParametersNode.cxx
    vtkMRMLAstroSmoothingParametersNode.h
    vtkMRMLAstroVolumeDisplayNode.cxx
    vtkMRMLAstroVolumeDisplayNode.h
    vtkMRMLAstroVolumeNode.cxx
    vtkMRMLAstroVolumeNode.h
    vtkMRMLAstroVolumeStorageNode.cxx
    vtkMRMLAstroVolumeStorageNode.h)

# The header '${module_mrml_name}Export.h' will be automatically configured.
set(module_mrml_export_directive "VTK_MRML_ASTRO_EXPORT")

set(module_mrml_target_libraries
  ${Slicer_GUI_LIBRARY}
  MRMLCore
  vtkFits
  )

SlicerMacroBuildModuleMRML(
  NAME ${module_mrml_name}
  EXPORT_DIRECTIVE ${module_mrml_export_directive}
  INCLUDE_DIRECTORIES ${module_mrml_include_directories}
  SRCS ${module_mrml_SRCS}
  TARGET_LIBRARIES ${module_mrml_target_libraries}
  )

# --------------------------------------------------------------------------
# Set INCLUDE_DIRS variable
# --------------------------------------------------------------------------
set(${PROJECT_NAME}_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
  CACHE INTERNAL "${PROJECT_NAME} include dirs" FORCE)

# --------------------------------------------------------------------------
# Configure support file
# --------------------------------------------------------------------------
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/MRMLAstroConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/MRMLAstroConfig.cmake
  )
