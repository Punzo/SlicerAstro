cmake_minimum_required(VERSION 3.4)

project(SlicerAstro)

#-----------------------------------------------------------------------------
set(EXTENSION_NAME SlicerAstro)

# ------------------------------------------------------------------------
set(SLICERRT_VERSION_MAJOR "1")
set(SLICERRT_VERSION_MINOR "0")
set(SLICERRT_VERSION_PATCH "0")
set(SLICERRT_VERSION ${SLICERRT_VERSION_MAJOR}.${SLICERRT_VERSION_MINOR}.${SLICERRT_VERSION_PATCH})

#-----------------------------------------------------------------------------
# Extension meta-information
set(EXTENSION_HOMEPAGE "https://github.com/Punzo/SlicerAstro/wiki")
set(EXTENSION_CATEGORY "Astronomy")
set(EXTENSION_CONTRIBUTORS "Davide Punzo (Kapteyn Astronomical Institute), Thijs van der Hulst (Kapteyn Astronomical institute), Jos Roerdink (Johann Bernoulli Institute), Jean-Christophe Fillion-Robin (Kitware)")
set(EXTENSION_DESCRIPTION "Extension for enabling Astronomical (HI) visualization in Slicer")
set(EXTENSION_ICONURL "https://raw.githubusercontent.com/Punzo/SlicerAstro/master/SlicerAstroIcon.png")
set(EXTENSION_SCREENSHOTURLS "https://raw.githubusercontent.com/Punzo/SlicerAstroWikiImages/master/Screenshoot.png")
set(EXTENSION_STATUS "Beta")
set(EXTENSION_DEPENDS "NA") # Specified as a space separated list or 'NA' if any
set(EXTENSION_BUILD_SUBDIRECTORY inner-build)

set(SUPERBUILD_TOPLEVEL_PROJECT inner)

#-----------------------------------------------------------------------------
# Extension dependencies
#find_package(Slicer COMPONENTS ConfigurePrerequisites REQUIRED)
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
# configurating additional Flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -ftree-vectorize -fPIC -Wuninitialized")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -ftree-vectorize -fPIC -Wuninitialized")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O2 -ftree-vectorize -fPIC -Wuninitialized -flto")

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
# configurating OpenMP Flags
if(VTK_SLICER_ASTRO_SUPPORT_OPENMP)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

#-----------------------------------------------------------------------------
# configurating OpenGL2
find_package(OpenGL)
set(VTK_SLICER_ASTRO_SUPPORT_OPENGL ${OPENGL_FOUND})
set(status "disabled")
if(VTK_SLICER_ASTRO_SUPPORT_OPENGL AND ${VTK_RENDERING_BACKEND} STREQUAL "OpenGL2")
  set(status "enabled")
else(VTK_SLICER_ASTRO_SUPPORT_OPENGL AND ${VTK_RENDERING_BACKEND} STREQUAL "OpenGL2")
  message(STATUS "VTK_RENDERING_BACKEND OpenGL2 not supported")
  set(VTK_SLICER_ASTRO_SUPPORT_OPENGL 0)
endif(VTK_SLICER_ASTRO_SUPPORT_OPENGL AND ${VTK_RENDERING_BACKEND} STREQUAL "OpenGL2")
message(STATUS "OpenGL filtering support ${status}")
configure_file(
  ${SlicerAstro_SOURCE_DIR}/CMake/vtkSlicerAstroConfigure.h.in
  ${SlicerAstro_BINARY_DIR}/vtkSlicerAstroConfigure.h
  )

#-----------------------------------------------------------------------------
# SuperBuild setup
option(${EXTENSION_NAME}_SUPERBUILD "Build ${EXTENSION_NAME} and the projects it depends on." ON)
mark_as_advanced(${EXTENSION_NAME}_SUPERBUILD)
if(${EXTENSION_NAME}_SUPERBUILD)
  include("${CMAKE_CURRENT_SOURCE_DIR}/SuperBuild.cmake")
  return()
endif()

#-----------------------------------------------------------------------------
# Install extension C/C++ packages

#
# CFITSIO
#

find_library(CFITSIO_LIBRARY cfitsio
  PATHS ${CFITSIO_LIBRARY_DIR} NO_DEFAULT_PATH
  )
set(CFITSIO_LIBRARIES ${CFITSIO_LIBRARY})

#
# WCS
#

find_library (WCSLIB_LIBRARY wcslib
  PATHS ${WCSLIB_LIBRARY_DIR} NO_DEFAULT_PATH
  )
set(WCSLIB_LIBRARIES ${WCSLIB_LIBRARY})

#
# 3DBarolo
#

find_library (BBAROLO_LIBRARY bbarolo
  PATHS ${BBAROLO_LIBRARY_DIR} NO_DEFAULT_PATH
  )
set(BBAROLO_LIBRARIES ${BBAROLO_LIBRARY})

# Install libraries
if(APPLE)
  include(SlicerFunctionInstallLibrary)
  foreach(library IN ITEMS ${CFITSIO_LIBRARY} ${WCSLIB_LIBRARY} ${BBAROLO_LIBRARY})
    slicerInstallLibrary(FILE ${library}
      DESTINATION ${Slicer_INSTALL_LIB_DIR}
      COMPONENT Runtime
      )
  endforeach()
endif()
if(NOT APPLE)
  include(SlicerFunctionInstallLibrary)
  foreach(library IN ITEMS ${CFITSIO_LIBRARY} ${WCSLIB_LIBRARY} ${BBAROLO_LIBRARY})
    slicerInstallLibrary(FILE ${library}
      DESTINATION ${Slicer_THIRDPARTY_LIB_DIR}/qt-loadable-modules
      COMPONENT Runtime
      )
  endforeach()
endif()

#-----------------------------------------------------------------------------
# Install extension python packages

install(CODE "message(\"CPack: - Install directory: ${python_reproject_DIR}\")")
install(
  DIRECTORY "${python_reproject_DIR}/"
  DESTINATION ${Slicer_INSTALL_ROOT}${Slicer_BUNDLE_EXTENSIONS_LOCATION}
  COMPONENT RuntimeLibraries
  )

#-----------------------------------------------------------------------------
# Extension modules

add_subdirectory(SlicerAstroWelcome)
if(UNIX)
  add_subdirectory(vtkFits)
  if(VTK_SLICER_ASTRO_SUPPORT_OPENGL)
    add_subdirectory(vtkOpenGLFilters)
  endif()
  add_subdirectory(AstroVolume)
  add_subdirectory(AstroReproject)
  add_subdirectory(AstroStatistics)
  add_subdirectory(AstroSmoothing)
  add_subdirectory(AstroMasking)
  add_subdirectory(AstroProfiles)
  add_subdirectory(AstroMomentMaps)
  add_subdirectory(AstroPVSlice)
  add_subdirectory(AstroPVDiagram)
  add_subdirectory(AstroModeling)
  add_subdirectory(AstroSampleData)
  add_subdirectory(SlicerAstroDataProbe)
endif(UNIX)

#-----------------------------------------------------------------------------
# Install extension deps packages
set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${CMAKE_BINARY_DIR};${EXTENSION_NAME};ALL;/")
if(UNIX)
  set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${cfitsio_DIR};cfitsio;RuntimeLibraries;/")
  set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${wcslib_DIR};wcslib;RuntimeLibraries;/")
  set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${bbarolo_DIR};bbarolo;RuntimeLibraries;/")
endif(UNIX)

include(${Slicer_EXTENSION_CPACK})
