cmake_minimum_required(VERSION 3.28.0...3.28.0 FATAL_ERROR)

project(SlicerAstro)

#-----------------------------------------------------------------------------
set(SLICERASTRO_VERSION_MAJOR "2")
set(SLICERASTRO_VERSION_MINOR "0")
set(SLICERASTRO_VERSION_PATCH "0")
set(SLICERASTRO_VERSION ${SLICERASTRO_VERSION_MAJOR}.${SLICERASTRO_VERSION_MINOR}.${SLICERASTRO_VERSION_PATCH})

#-----------------------------------------------------------------------------
# Extension meta-information
set(EXTENSION_HOMEPAGE "https://github.com/Punzo/SlicerAstro/wiki")
set(EXTENSION_CATEGORY "Astronomy")
set(EXTENSION_CONTRIBUTORS "Davide Punzo (Kapteyn Astronomical Institute), Thijs van der Hulst (Kapteyn Astronomical institute), Jos Roerdink (Johann Bernoulli Institute), Jean-Christophe Fillion-Robin (Kitware)")
set(EXTENSION_DESCRIPTION "SlicerAstro, an Astronomy (HI) extension for 3D Slicer")
set(EXTENSION_ICONURL "https://raw.githubusercontent.com/Punzo/SlicerAstro/master/SlicerAstroIcon.png")
set(EXTENSION_SCREENSHOTURLS "https://raw.githubusercontent.com/Punzo/SlicerAstroWikiImages/master/Screenshot.png")
set(EXTENSION_STATUS "Release")
set(EXTENSION_DEPENDS "NA") # Specified as a list or "NA" if no dependencies
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
# Configuration header
# Multi-threading uses vtkSMPTools (TBB backend in Slicer): no OpenMP.
#
# GPU smoothing backends:
#  - OpenGL (vtkOpenGLFilters): available with the standard Slicer builds.
#  - WebGPU (vtkAstroWebGPUFilters, planned): only when Slicer's VTK is built
#    with VTK_ENABLE_WEBGPU (e.g. SlicerAstroApp).
set(VTK_SLICER_ASTRO_SUPPORT_OPENGL OFF)
if(TARGET VTK::RenderingOpenGL2)
  set(VTK_SLICER_ASTRO_SUPPORT_OPENGL ON)
endif()
set(SLICERASTRO_HAS_WEBGPU OFF)
if(TARGET VTK::RenderingWebGPU)
  set(SLICERASTRO_HAS_WEBGPU ON)
endif()
message(STATUS "SlicerAstro GPU smoothing: OpenGL=${VTK_SLICER_ASTRO_SUPPORT_OPENGL} WebGPU=${SLICERASTRO_HAS_WEBGPU}")
configure_file(
  ${SlicerAstro_SOURCE_DIR}/CMake/vtkSlicerAstroConfigure.h.in
  ${SlicerAstro_BINARY_DIR}/vtkSlicerAstroConfigure.h
  )

#-----------------------------------------------------------------------------
# Third-party libraries built by the superbuild
macro(slicerastro_find_thirdparty_library prefix name)
  find_library(${prefix}_LIBRARY NAMES ${name} ${ARGN}
    PATHS ${${prefix}_LIBRARY_DIR} NO_DEFAULT_PATH
    )
  if(NOT ${prefix}_LIBRARY)
    message(FATAL_ERROR "${name} library not found in ${prefix}_LIBRARY_DIR [${${prefix}_LIBRARY_DIR}]")
  endif()
  set(${prefix}_LIBRARIES ${${prefix}_LIBRARY})
  list(APPEND SLICERASTRO_THIRDPARTY_LIBRARY_NAMES ${name} ${ARGN})
endmacro()
set(SLICERASTRO_THIRDPARTY_LIBRARY_NAMES)
slicerastro_find_thirdparty_library(CFITSIO cfitsio)
slicerastro_find_thirdparty_library(WCSLIB wcslib)
slicerastro_find_thirdparty_library(FFTW3 fftw3)
slicerastro_find_thirdparty_library(ERFA erfa)
slicerastro_find_thirdparty_library(BBAROLO bbarolo)

#-----------------------------------------------------------------------------
# Extension modules
add_subdirectory(SlicerAstroWelcome)
add_subdirectory(vtkFits)
add_subdirectory(vtkAstroCoordinates)
add_subdirectory(vtkAstroImageFilters)
if(VTK_SLICER_ASTRO_SUPPORT_OPENGL)
  add_subdirectory(vtkOpenGLFilters)
endif()
if(SLICERASTRO_HAS_WEBGPU)
  add_subdirectory(vtkAstroWebGPUFilters)
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
add_subdirectory(AstroSoFiACatalog)
add_subdirectory(SlicerAstroDataProbe)
add_subdirectory(Doxygen)

#-----------------------------------------------------------------------------
# The Python tests download the sample data into the cache of Slicer: they
# are not run at the same time (concurrent downloads of a file conflict).
if(BUILD_TESTING)
  function(slicerastro_lock_python_tests directory)
    get_property(_tests DIRECTORY ${directory} PROPERTY TESTS)
    foreach(_test IN LISTS _tests)
      if(_test MATCHES "^py_")
        set_tests_properties(${_test} DIRECTORY ${directory}
          PROPERTIES RESOURCE_LOCK SlicerAstroSampleDataCache)
      endif()
    endforeach()
    get_property(_subdirectories DIRECTORY ${directory} PROPERTY SUBDIRECTORIES)
    foreach(_subdirectory IN LISTS _subdirectories)
      slicerastro_lock_python_tests(${_subdirectory})
    endforeach()
  endfunction()
  slicerastro_lock_python_tests(${CMAKE_CURRENT_SOURCE_DIR})
endif()

#-----------------------------------------------------------------------------
# Install third-party runtime libraries
include(SlicerFunctionInstallLibrary)
if(WIN32)
  # Install the DLLs (find_library returns the import libraries)
  foreach(name IN LISTS SLICERASTRO_THIRDPARTY_LIBRARY_NAMES)
    find_file(_dll NAMES ${name}.dll lib${name}.dll lib${name}-3.dll
      PATHS ${EXTENSION_SUPERBUILD_BINARY_DIR}/${Slicer_THIRDPARTY_BIN_DIR}
      NO_DEFAULT_PATH NO_CACHE)
    if(_dll)
      install(FILES ${_dll} DESTINATION ${Slicer_INSTALL_THIRDPARTY_BIN_DIR} COMPONENT RuntimeLibraries)
    endif()
    unset(_dll)
  endforeach()
else()
  foreach(library IN ITEMS ${CFITSIO_LIBRARY} ${WCSLIB_LIBRARY} ${FFTW3_LIBRARY} ${ERFA_LIBRARY} ${BBAROLO_LIBRARY})
    slicerInstallLibrary(FILE ${library}
      DESTINATION ${Slicer_INSTALL_THIRDPARTY_LIB_DIR}
      COMPONENT RuntimeLibraries
      )
  endforeach()
endif()

#-----------------------------------------------------------------------------
set(EXTENSION_CPACK_INSTALL_CMAKE_PROJECTS)
set(${EXTENSION_NAME}_CPACK_INSTALL_CMAKE_PROJECTS "${EXTENSION_CPACK_INSTALL_CMAKE_PROJECTS}" CACHE STRING "List of external projects to install" FORCE)

#-----------------------------------------------------------------------------
list(APPEND CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${EXTENSION_NAME};ALL;/")
list(APPEND CPACK_INSTALL_CMAKE_PROJECTS "${${EXTENSION_NAME}_CPACK_INSTALL_CMAKE_PROJECTS}")
include(${Slicer_EXTENSION_GENERATE_CONFIG})
include(${Slicer_EXTENSION_CPACK})
