project(vtkFits)

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
endif()

if(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
endif()


# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------

#
# Fits
#

if (NOT CFITSIO_LIBRARIES)
  find_library (CFITSIO_LIBRARIES cfitsio
    PATHS ${CFITSIO_LIBRARY}
    )
endif()

#
# WCS
#

if (NOT WCSLIB_LIBRARIES)
  find_library (WCSLIB_LIBRARIES wcs
    PATHS ${WCSLIB_LIBRARY}
    )
endif()

set(KIT ${PROJECT_NAME})

# --------------------------------------------------------------------------
# Sources
# --------------------------------------------------------------------------
set(vtkFits_SRCS
  vtkFITSReader.cxx
  vtkFITSWriter.cxx
  )

# --------------------------------------------------------------------------
# Include dirs
# --------------------------------------------------------------------------
set(include_dirs
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CFITSIO_INCLUDE_DIR}
  ${WCSLIB_INCLUDE_DIR}
  )
include_directories(BEFORE ${include_dirs})

# --------------------------------------------------------------------------
# Build library
# --------------------------------------------------------------------------
set(lib_name ${PROJECT_NAME})

set(srcs ${vtkFits_SRCS})
add_library(${lib_name} ${srcs})

if(${VTK_VERSION_MAJOR} GREATER 5)

  set(VTK_WRAP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${VTK_INCLUDE_DIRS}
      CACHE INTERNAL "VTK wrap include dirs" FORCE)
else()
  set(VTK_LIBRARIES
    vtkCommon
    vtkFiltering
    vtkGraphics
    vtkIO
    vtkImaging
  )
endif()

set(libs
  ${CFITSIO_LIBRARIES}
  ${WCSLIB_LIBRARIES}
  ${VTK_LIBRARIES}
  )
target_link_libraries(${lib_name} ${libs})

# Apply user-defined properties to the library target.
if(Slicer_LIBRARY_PROPERTIES)
  set_target_properties(${lib_name} PROPERTIES ${Slicer_LIBRARY_PROPERTIES})
endif()

# --------------------------------------------------------------------------
# Export target
# --------------------------------------------------------------------------
if(NOT DEFINED ${PROJECT_NAME}_EXPORT_FILE)
  set(${PROJECT_NAME}_EXPORT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake)
endif()
export(TARGETS ${lib_name} APPEND FILE ${${PROJECT_NAME}_EXPORT_FILE})

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
# Python Wrapping
# --------------------------------------------------------------------------
if(VTK_WRAP_PYTHON)
  include(vtkMacroKitPythonWrap)

  vtkMacroKitPythonWrap(
    KIT_NAME ${lib_name}
    KIT_SRCS ${vtkFits_SRCS}
    KIT_INSTALL_BIN_DIR ${${PROJECT_NAME}_INSTALL_BIN_DIR}
    KIT_INSTALL_LIB_DIR ${${PROJECT_NAME}_INSTALL_LIB_DIR}
    )
  # Export target
  export(TARGETS ${lib_name}Python ${lib_name}PythonD APPEND FILE ${${PROJECT_NAME}_EXPORT_FILE})
endif()

# --------------------------------------------------------------------------
# Testing (requires some of the examples)
# --------------------------------------------------------------------------
#if(BUILD_TESTING)
#  add_subdirectory(Testing)
#endif()

# --------------------------------------------------------------------------
# Set INCLUDE_DIRS variable
# --------------------------------------------------------------------------
set(${PROJECT_NAME}_INCLUDE_DIRS ${include_dirs} CACHE INTERNAL "${PROJECT_NAME} include dirs" FORCE)

