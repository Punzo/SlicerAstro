project(vtkSlicer${MODULE_NAME}ModuleLogic)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "VTK_SLICER_${MODULE_NAME_UPPER}_MODULE_LOGIC_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
   ${CMAKE_CURRENT_SOURCE_DIR}/../MRML
   ${CMAKE_CURRENT_BINARY_DIR}/../MRML
   ${WCSLIB_INCLUDE_DIR}
   ${vtkFits_INCLUDE_DIRS}
   ${vtkSlicerVolumeRenderingModuleMRML_INCLUDES_DIRS}
  )

set(${KIT}_SRCS
  vtkSlicer${MODULE_NAME}Logic.cxx
  vtkSlicer${MODULE_NAME}Logic.h
  )

set(${KIT}_TARGET_LIBRARIES
  ${ITK_LIBRARIES}
  vtkSlicerAstroVolumeModuleMRML
  vtkSlicerVolumesModuleLogic
  vtkSlicerUnitsModuleLogic
  vtkSlicerVolumeRenderingModuleMRML
  )


#-----------------------------------------------------------------------------
SlicerMacroBuildModuleLogic(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
