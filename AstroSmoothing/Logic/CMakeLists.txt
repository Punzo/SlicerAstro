project(vtkSlicer${MODULE_NAME}ModuleLogic)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "VTK_SLICER_${MODULE_NAME_UPPER}_MODULE_LOGIC_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  ${SlicerAstro_BINARY_DIR}
  )

if(VTK_SLICER_ASTRO_SUPPORT_OPENGL)
  set(${KIT}_INCLUDE_DIRECTORIES
     ${${KIT}_INCLUDE_DIRECTORIES}
     ${vtkOpenGLFilters_INCLUDE_DIRS}
     )
endif()


set(${KIT}_SRCS
  vtkSlicer${MODULE_NAME}Logic.cxx
  vtkSlicer${MODULE_NAME}Logic.h
  )

set(${KIT}_TARGET_LIBRARIES
  vtkSlicerAstroVolumeModuleMRML
  vtkSlicerAstroVolumeModuleLogic
  vtkOpenGLFilters
  )

if(VTK_SLICER_ASTRO_SUPPORT_OPENGL)
  set(${KIT}_TARGET_LIBRARIES
     ${${KIT}_TARGET_LIBRARIES}
     vtkOpenGLFilters
     )
endif()

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleLogic(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
