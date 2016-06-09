cmake_minimum_required(VERSION 3.4)

project(SlicerAstro)

#-----------------------------------------------------------------------------
# Extension meta-information
set(EXTENSION_HOMEPAGE "https://github.com/Punzo/SlicerAstro/wiki")
set(EXTENSION_CATEGORY "Astronomy")
set(EXTENSION_CONTRIBUTORS "Davide Punzo (Kapteyn Astronomical Institute)")
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
# SuperBuild setup
option(${EXTENSION_NAME}_SUPERBUILD "Build ${EXTENSION_NAME} and the projects it depends on." ON)
mark_as_advanced(${EXTENSION_NAME}_SUPERBUILD)
if(${EXTENSION_NAME}_SUPERBUILD)
  include("${CMAKE_CURRENT_SOURCE_DIR}/SuperBuild.cmake")
  return()
endif()

#-----------------------------------------------------------------------------
# Extension modules
add_subdirectory(vtkFits)
add_subdirectory(DataProbeAstro)
add_subdirectory(AstroVolume)
add_subdirectory(Smoothing)
## NEXT_MODULE

#-----------------------------------------------------------------------------
set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${CMAKE_BINARY_DIR};${EXTENSION_NAME};ALL;/")
include(${Slicer_EXTENSION_CPACK})
