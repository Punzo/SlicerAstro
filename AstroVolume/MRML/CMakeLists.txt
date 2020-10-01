project(vtkSlicer${MODULE_NAME}ModuleMRML)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "VTK_MRML_ASTRO_EXPORT")

# --------------------------------------------------------------------------
# Include dirs
# --------------------------------------------------------------------------
set(${KIT}_INCLUDE_DIRECTORIES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/../../
  ${MRMLCore_INCLUDE_DIRS}
  ${WCSLIB_INCLUDE_DIR}
  ${CFITSIO_INCLUDE_DIR}
  ${vtkFits_INCLUDE_DIRS}
  ${Slicer_Base_INCLUDE_DIRS}
  )

# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
set(${KIT}_SRCS
    vtkMRMLAstroLabelMapVolumeDisplayNode.cxx
    vtkMRMLAstroLabelMapVolumeDisplayNode.h
    vtkMRMLAstroLabelMapVolumeNode.cxx
    vtkMRMLAstroLabelMapVolumeNode.h
    vtkMRMLAstroMaskingParametersNode.cxx
    vtkMRMLAstroMaskingParametersNode.h
    vtkMRMLAstroModelingParametersNode.cxx
    vtkMRMLAstroModelingParametersNode.h
    vtkMRMLAstroMomentMapsParametersNode.cxx
    vtkMRMLAstroMomentMapsParametersNode.h
    vtkMRMLAstroProfilesParametersNode.cxx
    vtkMRMLAstroProfilesParametersNode.h
    vtkMRMLAstroPVDiagramParametersNode.cxx
    vtkMRMLAstroPVDiagramParametersNode.h
    vtkMRMLAstroPVSliceParametersNode.cxx
    vtkMRMLAstroPVSliceParametersNode.h
    vtkMRMLAstroReprojectParametersNode.cxx
    vtkMRMLAstroReprojectParametersNode.h
    vtkMRMLAstroSmoothingParametersNode.cxx
    vtkMRMLAstroSmoothingParametersNode.h
    vtkMRMLAstroStatisticsParametersNode.cxx
    vtkMRMLAstroStatisticsParametersNode.h
    vtkMRMLAstroVolumeDisplayNode.cxx
    vtkMRMLAstroVolumeDisplayNode.h
    vtkMRMLAstroVolumeNode.cxx
    vtkMRMLAstroVolumeNode.h
    vtkMRMLAstroVolumeStorageNode.cxx
    vtkMRMLAstroVolumeStorageNode.h)

set(${KIT}_TARGET_LIBRARIES
  ${Slicer_GUI_LIBRARY}
  MRMLCore
  vtkFits
  )

# --------------------------------------------------------------------------
SlicerMacroBuildModuleMRML(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
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
