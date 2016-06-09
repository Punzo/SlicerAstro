cmake_minimum_required(VERSION 3.4)

project(SlicerAstro)

#-----------------------------------------------------------------------------
# Extension meta-information
set(EXTENSION_HOMEPAGE "https://github.com/Punzo/SlicerAstro/wiki")
set(EXTENSION_CATEGORY "Astronomy")
set(EXTENSION_CONTRIBUTORS "Davide Punzo (Kapteyn Astronomical Institute), Jean-Christophe Fillion-Robin (Kitware)")
set(EXTENSION_DESCRIPTION "Extension for enabling Astronomical (HI) visualization in Slicer")
set(EXTENSION_ICONURL "https://raw.githubusercontent.com/Punzo/SlicerAstro/master/SlicerAstroIcon.png")
set(EXTENSION_SCREENSHOTURLS "https://raw.githubusercontent.com/Punzo/SlicerAstroWikiImages/master/Screenshoot.png")
set(EXTENSION_DEPENDS "NA") # Specified as a space separated list or 'NA' if any
set(EXTENSION_BUILD_SUBDIRECTORY inner-build)

set(SUPERBUILD_TOPLEVEL_PROJECT inner)

#-----------------------------------------------------------------------------
# Extension dependencies
find_package(Slicer REQUIRED)
include(${Slicer_USE_FILE})
mark_as_superbuild(Slicer_DIR)

find_package(Git REQUIRED)
mark_as_superbuild(GIT_EXECUTABLE)

#-----------------------------------------------------------------------------
# check OS
if(WIN32)
  message(STATUS "SlicerAstro is not available on windows platforms.
                  Only the SlicerAstro Welcoming module will be compiled.")
endif(WIN32)

#-----------------------------------------------------------------------------
# configurating OpenMP
find_package(OpenMP)
set(VTK_SLICER_ASTRO_SUPPORT_OPENMP ${OPENMP_FOUND})
set(status "disabled")
if(VTK_SLICER_ASTRO_SUPPORT_OPENMP)
  set(status "enabled")
endif()
message(STATUS "OpenMP support ${status}")
configure_file(
  ${SlicerAstro_SOURCE_DIR}/CMake/vtkSlicerAstroConfigure.h.in
  ${SlicerAstro_BINARY_DIR}/vtkSlicerAstroConfigure.h
  )

#-----------------------------------------------------------------------------
# SuperBuild setup
if(UNIX)
  option(${EXTENSION_NAME}_SUPERBUILD "Build ${EXTENSION_NAME} and the projects it depends on." ON)
  mark_as_advanced(${EXTENSION_NAME}_SUPERBUILD)
  if(${EXTENSION_NAME}_SUPERBUILD)
    include("${CMAKE_CURRENT_SOURCE_DIR}/SuperBuild.cmake")
    return()
  endif()
endif(UNIX)

#-----------------------------------------------------------------------------
# configurating OpenMP Flags
if(VTK_SLICER_ASTRO_SUPPORT_OPENMP)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

#-----------------------------------------------------------------------------
# Extension modules

add_subdirectory(SlicerAstroWelcome)
if(UNIX)
  add_subdirectory(vtkFits)
  add_subdirectory(DataProbeAstro)
  add_subdirectory(AstroVolume)
  add_subdirectory(Smoothing)
endif(UNIX)

## NEXT_MODULE

#-----------------------------------------------------------------------------
set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${CMAKE_BINARY_DIR};${EXTENSION_NAME};ALL;/")
include(${Slicer_EXTENSION_CPACK})
