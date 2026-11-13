project(vtkAstroReproject)

# --------------------------------------------------------------------------
# Reprojection, regridding and mosaicking of astronomical data (no MRML):
# vtkAstroReprojector (C++ only), vtkAstroBeams (Gaussian beams, wrapped) and
# the spherical pixel overlap of Montage / astropy reproject (ThirdParty).
# --------------------------------------------------------------------------
set(KIT ${PROJECT_NAME})

set(${KIT}_WRAPPED_SRCS
  vtkAstroBeams.cxx
  vtkAstroBeams.h
  )

set(${KIT}_SRCS
  ${${KIT}_WRAPPED_SRCS}
  vtkAstroReprojector.cxx
  vtkAstroReprojector.h
  ThirdParty/overlap/overlapArea.cxx
  ThirdParty/overlap/overlapArea.h
  )

# vendored code (astropy reproject / Montage) kept as upstream: no warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(ThirdParty/overlap/overlapArea.cxx PROPERTIES COMPILE_OPTIONS "-w")
elseif(MSVC)
  set_source_files_properties(ThirdParty/overlap/overlapArea.cxx PROPERTIES COMPILE_OPTIONS "/w")
endif()

set(lib_name ${PROJECT_NAME})
add_library(${lib_name} ${${KIT}_SRCS})
target_link_libraries(${lib_name}
  PUBLIC ${VTK_LIBRARIES}
  PRIVATE vtkAstroCoordinates ${WCSLIB_LIBRARIES}
  )
target_include_directories(${lib_name}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    ${WCSLIB_INCLUDE_DIR}
  )
if(MSVC)
  target_compile_definitions(${lib_name} PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
endif()

include(GenerateExportHeader)
generate_export_header(${lib_name}
  BASE_NAME vtkAstroReproject
  EXPORT_MACRO_NAME VTK_ASTRO_REPROJECT_EXPORT
  EXPORT_FILE_NAME vtkAstroReprojectExport.h
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

if(VTK_WRAP_PYTHON)
  include(vtkMacroKitPythonWrap)
  vtkMacroKitPythonWrap(
    KIT_NAME ${lib_name}
    KIT_SRCS ${${KIT}_WRAPPED_SRCS}
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

set(${PROJECT_NAME}_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
  CACHE INTERNAL "${PROJECT_NAME} include dirs" FORCE)
