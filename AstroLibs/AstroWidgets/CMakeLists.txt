project(qMRMLAstroWidgets)


#-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 2.8.4)
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# See http://cmake.org/cmake/help/cmake-2-8-docs.html#section_Policies for details
#-----------------------------------------------------------------------------
if(POLICY CMP0017)
  cmake_policy(SET CMP0017 OLD)
endif()

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
if(NOT DEFINED BUILD_SHARED_LIBS)
  option(BUILD_SHARED_LIBS "Build with shared libraries." ON)
endif(NOT DEFINED BUILD_SHARED_LIBS)

include(CheckIncludeFiles)
check_include_files("stdint.h" HAVE_STDINT_H)

# CMake module
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${MRMLWidgets_SOURCE_DIR}/CMake")

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------


#
# CTK
#
set(CTK_LIBRARIES CTKVisualizationVTKWidgets)
if(Slicer_USE_PYTHONQT)
  list(APPEND CTK_LIBRARIES
    CTKScriptingPythonWidgets
    )
endif()
if(Slicer_USE_QtTesting)
  list(APPEND CTK_LIBRARIES
    CTKQtTesting
    )
endif()

# --------------------------------------------------------------------------
# Configure headers
# --------------------------------------------------------------------------
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/qMRMLAstroWidgetsConfigure.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/qMRMLAstroWidgetsConfigure.h
  )

#-----------------------------------------------------------------------------
# Sources
#
set(MRMLAstroWidgets_SRCS
  qMRMLAstroVolumeInfoWidget.cxx
  qMRMLAstroVolumeInfoWidget.h
  #qMRMLSliceAstroControllerWidget.cxx
  #qMRMLSliceAstroControllerWidget.h
  #qMRMLSliceAstroControllerWidget_p.h
  #qMRMLSliceAstroWidget.cxx
  #qMRMLSliceAstroWidget.h
  #qMRMLSliceAstroWidget_p.h
  )

# Headers that should run through moc
set(MRMLAstroWidgets_MOC_SRCS
  qMRMLAstroVolumeInfoWidget.h
  #qMRMLSliceAstroControllerWidget.h
  #qMRMLSliceAstroControllerWidget_p.h
  #qMRMLSliceAstroWidget.h
  #qMRMLSliceAstroWidget_p.h
  )

# UI files
set(MRMLAstroWidgets_UI_SRCS
    #Resources/UI/qMRMLSliceAstroControllerWidget.ui
  )

set(MRMLAstroWidgets_QRC_SRCS
  Resources/qMRMLAstroWidgets.qrc
  )

QT4_WRAP_CPP(MRMLAstroWidgets_MOC_CXX ${MRMLAstroWidgets_MOC_SRCS})
QT4_WRAP_UI(MRMLAstroWidgets_UI_CXX ${MRMLAstroWidgets_UI_SRCS})
QT4_ADD_RESOURCES(MRMLAstroWidgets_QRC_CXX ${MRMLAstroWidgets_QRC_SRCS})

source_group("Resources" FILES
  ${MRMLWidgets_UI_SRCS}
  ${MRMLWidgets_QRC_SRCS}
  )

source_group("Generated" FILES
  ${MRMLWidgets_MOC_CXX}
  ${MRMLWidgets_UI_CXX}
  ${MRMLWidgets_QRC_CXX}
  )

# --------------------------------------------------------------------------
# Include dirs
# --------------------------------------------------------------------------
set(include_dirs
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${qMRMLWidgets_INCLUDE_DIRS}
  ${MRMLCore_INCLUDE_DIRS}
  ${MRMLAstro_INCLUDE_DIRS}
  )
include_directories(${include_dirs})

# --------------------------------------------------------------------------
# Install headers
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_INSTALL_NO_DEVELOPMENT)
  set(${PROJECT_NAME}_INSTALL_NO_DEVELOPMENT ON)
endif()
if(NOT ${PROJECT_NAME}_INSTALL_NO_DEVELOPMENT)
  file(GLOB headers "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
  install(
    FILES ${headers}
    DESTINATION include/${PROJECT_NAME} COMPONENT Development)
endif()

# --------------------------------------------------------------------------
# Build library
# --------------------------------------------------------------------------
set(lib_name ${PROJECT_NAME})
set(targets_to_export ${lib_name})

add_library(${lib_name}
  ${MRMLAstroWidgets_SRCS}
  ${MRMLAstroWidgets_MOC_CXX}
  ${MRMLAstroWidgets_UI_CXX}
  )

set(MRMLAstroWidgets_LIBRARIES
  ${QT_LIBRARIES}
  ${CTK_LIBRARIES}
  qMRMLWidgets
  MRMLLogic
  MRMLAstro
  MRMLCore
  )

if(${VTK_VERSION_MAJOR} GREATER 5)
  list(APPEND MRMLAstroWidgets_LIBRARIES
    vtkRenderingQt
    )
endif()

target_link_libraries(${lib_name} ${MRMLAstroWidgets_LIBRARIES})

# Apply user-defined properties to the library target.
if(Slicer_LIBRARY_PROPERTIES)
  set_target_properties(${lib_name} PROPERTIES ${Slicer_LIBRARY_PROPERTIES})
endif()

# --------------------------------------------------------------------------
# Folder
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_FOLDER)
  set(${PROJECT_NAME}_FOLDER ${PROJECT_NAME})
endif()
if(NOT "${${PROJECT_NAME}_FOLDER}" STREQUAL "")
  set_target_properties(${lib_name} PROPERTIES FOLDER ${${PROJECT_NAME}_FOLDER})
endif()

# --------------------------------------------------------------------------
# PythonQt wrapping
# --------------------------------------------------------------------------
if(MRMLWidgets_WRAP_PYTHON)
  ctkMacroBuildLibWrapper(
    NAMESPACE "org.slicer.libs"
    TARGET ${lib_name}
    SRCS "${MRMLAstroWidgets_SRCS}"
    INSTALL_BIN_DIR ${Slicer_INSTALL_BIN_DIR}
    INSTALL_LIB_DIR ${Slicer_INSTALL_LIB_DIR}
    )
  # Folder
  if(NOT "${${PROJECT_NAME}_FOLDER}" STREQUAL "")
    set_target_properties(${lib_name}PythonQt PROPERTIES FOLDER ${${PROJECT_NAME}_FOLDER})
  endif()
endif()

# --------------------------------------------------------------------------
# Export target
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_EXPORT_FILE)
  set(${PROJECT_NAME}_EXPORT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake)
endif()
export(TARGETS ${targets_to_export} APPEND FILE ${${PROJECT_NAME}_EXPORT_FILE})

# --------------------------------------------------------------------------
# Install library
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_INSTALL_BIN_DIR)
  set(${PROJECT_NAME}_INSTALL_BIN_DIR bin)
endif()
if(NOT DEFINED ${PROJECT_NAME}_INSTALL_LIB_DIR)
  set(${PROJECT_NAME}_INSTALL_LIB_DIR lib/${PROJECT_NAME})
endif()

install(TARGETS ${lib_name}
  RUNTIME DESTINATION ${${PROJECT_NAME}_INSTALL_BIN_DIR} COMPONENT RuntimeLibraries
  LIBRARY DESTINATION ${${PROJECT_NAME}_INSTALL_LIB_DIR} COMPONENT RuntimeLibraries
  ARCHIVE DESTINATION ${${PROJECT_NAME}_INSTALL_LIB_DIR} COMPONENT Development
  )

# --------------------------------------------------------------------------
# Testing
# --------------------------------------------------------------------------
#if(BUILD_TESTING)
#  add_subdirectory(Testing)
#endif()

# --------------------------------------------------------------------------
# Set INCLUDE_DIRS variable
# --------------------------------------------------------------------------
set(${PROJECT_NAME}_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
  CACHE INTERNAL "${PROJECT_NAME} include dirs" FORCE)

# --------------------------------------------------------------------------
# Configure support file
# --------------------------------------------------------------------------
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/qMRMLAstroWidgetsConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/qMRMLAstroWidgetsConfig.cmake
  )
