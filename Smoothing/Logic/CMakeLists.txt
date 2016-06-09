project(vtkSlicer${MODULE_NAME}ModuleLogic)

set(KIT ${PROJECT_NAME})

# XXX Move this to top-level CMakeLists (before including SuperBuild.cmake)
#     That way it provides feedback early in the configure process
find_package(OpenMP)
set(VTK_SLICER_ASTRO_SUPPORT_OPENMP ${OpenMP_FOUND})
set(status "disabled")
if(VTK_SLICER_ASTRO_SUPPORT_OPENMP)
  set(status "enabled")
endif()
message(STATUS "OpenMP support ${status}")
configure_file(
  ${SlicerAstro_SOURCE_DIR}/CMake/vtkSlicerAstroConfigure.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/vtkSlicerAstroConfigure.h
  )

# XXX Move this after including SuperBuild.cmake in top-level CMakeLists.txt
if(VTK_SLICER_ASTRO_SUPPORT_OPENMP)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()


set(${KIT}_EXPORT_DIRECTIVE "VTK_SLICER_${MODULE_NAME_UPPER}_MODULE_LOGIC_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  )

set(${KIT}_SRCS
  vtkSlicer${MODULE_NAME}Logic.cxx
  vtkSlicer${MODULE_NAME}Logic.h
  vtkOpenGLAstroTextureImage.h
  vtkOpenGLAstroTextureImage.cxx
  vtkOpenGLAstroShaderComputation.h
  vtkOpenGLAstroShaderComputation.cxx
  )

set(${KIT}_TARGET_LIBRARIES
  vtkSlicerAstroVolumeModuleMRML
  vtkSlicerAstroVolumeModuleLogic
  )

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleLogic(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
