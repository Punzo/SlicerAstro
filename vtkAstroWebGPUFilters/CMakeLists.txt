project(vtkAstroWebGPUFilters)

# WebGPU (WGSL compute shader) implementation of the vtkAstroImageFilters
# smoothing filters. Built only when VTK provides VTK::RenderingWebGPU
# (Slicer configured with Slicer_VTK_ENABLE_WEBGPU=ON).

# --------------------------------------------------------------------------
set(KIT ${PROJECT_NAME})

# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
set(${KIT}_SRCS
  vtkAstroWebGPUDevice.cxx
  vtkAstroWebGPUDevice.h
  vtkAstroWebGPUImageKernelSmooth.cxx
  vtkAstroWebGPUImageKernelSmooth.h
  vtkAstroWebGPUImageSeparableSmooth.cxx
  vtkAstroWebGPUImageSeparableSmooth.h
  vtkAstroWebGPUImageGradientSmooth.cxx
  vtkAstroWebGPUImageGradientSmooth.h
  )

# Internal (not wrapped)
set(${KIT}_INTERNAL_SRCS
  vtkAstroWebGPUShaders.h
  vtkAstroWebGPUSmoothingEngine.cxx
  vtkAstroWebGPUSmoothingEngine.h
  )

# --------------------------------------------------------------------------
# Build library
# --------------------------------------------------------------------------
set(lib_name ${PROJECT_NAME})
add_library(${lib_name} ${${KIT}_SRCS} ${${KIT}_INTERNAL_SRCS})
target_link_libraries(${lib_name} PUBLIC vtkAstroImageFilters ${VTK_LIBRARIES} VTK::RenderingWebGPU)
target_include_directories(${lib_name} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  )

include(GenerateExportHeader)
generate_export_header(${lib_name}
  BASE_NAME vtkAstroWebGPUFilters
  EXPORT_MACRO_NAME VTK_ASTRO_WEBGPU_FILTERS_EXPORT
  EXPORT_FILE_NAME vtkAstroWebGPUFiltersExport.h
  )

set_target_properties(${lib_name} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_BIN_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_LIB_DIR}"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_LIB_DIR}"
  POSITION_INDEPENDENT_CODE ON
  )

if(NOT DEFINED ${PROJECT_NAME}_EXPORT_FILE)
  set(${PROJECT_NAME}_EXPORT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake)
endif()
export(TARGETS ${lib_name} APPEND FILE ${${PROJECT_NAME}_EXPORT_FILE})

install(TARGETS ${lib_name}
  RUNTIME DESTINATION ${Slicer_INSTALL_QTLOADABLEMODULES_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${Slicer_INSTALL_QTLOADABLEMODULES_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${Slicer_INSTALL_QTLOADABLEMODULES_LIB_DIR} COMPONENT Development
  )

# --------------------------------------------------------------------------
# Python wrapping
# --------------------------------------------------------------------------
if(VTK_WRAP_PYTHON)
  include(vtkMacroKitPythonWrap)
  vtkMacroKitPythonWrap(
    KIT_NAME ${lib_name}
    KIT_SRCS ${${KIT}_SRCS}
    KIT_INSTALL_BIN_DIR ${Slicer_INSTALL_QTLOADABLEMODULES_BIN_DIR}
    KIT_INSTALL_LIB_DIR ${Slicer_INSTALL_QTLOADABLEMODULES_LIB_DIR}
    )
  set_target_properties(${lib_name}Python PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_BIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_LIB_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_LIB_DIR}"
    )
  export(TARGETS ${lib_name}Python APPEND FILE ${${PROJECT_NAME}_EXPORT_FILE})
endif()

# --------------------------------------------------------------------------
# Testing
# --------------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()

set(${PROJECT_NAME}_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
  CACHE INTERNAL "${PROJECT_NAME} include dirs" FORCE)
