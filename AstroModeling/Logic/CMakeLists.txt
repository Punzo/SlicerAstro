project(vtkSlicer${MODULE_NAME}ModuleLogic)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "VTK_SLICERASTRO_${MODULE_NAME_UPPER}_MODULE_LOGIC_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  ${SlicerAstro_BINARY_DIR}
  ${BBAROLO_INCLUDE_DIR}
  ${WCSLIB_INCLUDE_DIR}
  ${CFITSIO_INCLUDE_DIR}
  ${FFTW3_INCLUDE_DIR}
  )

set(${KIT}_SRCS
  vtkSlicer${MODULE_NAME}Logic.cxx
  vtkSlicer${MODULE_NAME}Logic.h
  )

set(${KIT}_TARGET_LIBRARIES
  vtkSlicerAstroVolumeModuleMRML
  vtkSlicerAstroVolumeModuleLogic
  ${BBAROLO_LIBRARIES}
  )

#-----------------------------------------------------------------------------
# libbbarolo is built with BBAROLO_THROW_ON_ERROR: fatal errors are reported as
# bbarolo::Error exceptions instead of terminating the application.
add_compile_definitions(BBAROLO_THROW_ON_ERROR)

SlicerMacroBuildModuleLogic(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
