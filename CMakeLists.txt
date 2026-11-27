cmake_minimum_required(VERSION 3.28.0...3.28.0 FATAL_ERROR)

project(SlicerAstro)

#-----------------------------------------------------------------------------
set(SLICERASTRO_VERSION_MAJOR "2")
set(SLICERASTRO_VERSION_MINOR "0")
set(SLICERASTRO_VERSION_PATCH "0")
set(SLICERASTRO_VERSION ${SLICERASTRO_VERSION_MAJOR}.${SLICERASTRO_VERSION_MINOR}.${SLICERASTRO_VERSION_PATCH})

#-----------------------------------------------------------------------------
# Extension meta-information
set(EXTENSION_HOMEPAGE "https://github.com/Punzo/SlicerAstro")
set(EXTENSION_CATEGORY "Astronomy")
set(EXTENSION_CONTRIBUTORS "Davide Punzo (Kapteyn Astronomical Institute), Thijs van der Hulst (Kapteyn Astronomical institute), Jos Roerdink (Johann Bernoulli Institute), Jean-Christophe Fillion-Robin (Kitware)")
set(EXTENSION_DESCRIPTION "SlicerAstro, an Astronomy (HI) extension for 3D Slicer")
set(EXTENSION_ICONURL "https://raw.githubusercontent.com/Punzo/SlicerAstro/master/SlicerAstroIcon.png")
set(EXTENSION_SCREENSHOTURLS "https://raw.githubusercontent.com/Punzo/SlicerAstroWikiImages/master/Screenshot.png")
set(EXTENSION_STATUS "Release")
set(EXTENSION_DEPENDS SlicerVirtualReality) # immersive view of the 3D rendering (installed with SlicerAstro)
set(EXTENSION_BUILD_SUBDIRECTORY inner-build)

set(SUPERBUILD_TOPLEVEL_PROJECT inner)

#-----------------------------------------------------------------------------
# Extension dependencies
find_package(Slicer REQUIRED)
include(${Slicer_USE_FILE})
mark_as_superbuild(Slicer_DIR)

find_package(Git REQUIRED)
mark_as_superbuild(GIT_EXECUTABLE)

# Code coverage (gcov) of the SlicerAstro code only, not of the third-party
# libraries of the superbuild (an instrumented 3DBarolo is very slow)
option(SLICERASTRO_COVERAGE "Instrument the SlicerAstro code for code coverage (gcov)" OFF)
mark_as_advanced(SLICERASTRO_COVERAGE)
mark_as_superbuild(SLICERASTRO_COVERAGE)

#-----------------------------------------------------------------------------
# SuperBuild setup
option(${EXTENSION_NAME}_SUPERBUILD "Build ${EXTENSION_NAME} and the projects it depends on." ON)
mark_as_advanced(${EXTENSION_NAME}_SUPERBUILD)
if(${EXTENSION_NAME}_SUPERBUILD)
  include("${CMAKE_CURRENT_SOURCE_DIR}/SuperBuild.cmake")
  return()
endif()

#-----------------------------------------------------------------------------
# MSVC: the Microsoft C runtime declares a non-standard wcsset() that conflicts
# with WCSLIB's wcsset(): no non-standard C runtime names (as WCSLIB and BBarolo)
if(MSVC)
  add_compile_definitions(_CRT_DECLARE_NONSTDC_NAMES=0)
endif()

if(SLICERASTRO_COVERAGE)
  add_compile_options(--coverage)
  add_link_options(--coverage)
endif()

#-----------------------------------------------------------------------------
# Configuration header
# Multi-threading uses vtkSMPTools (TBB backend in Slicer): no OpenMP.
#
# GPU smoothing: OpenGL (vtkOpenGLFilters), available with the standard
# Slicer builds.
set(VTK_SLICER_ASTRO_SUPPORT_OPENGL OFF)
if(TARGET VTK::RenderingOpenGL2)
  set(VTK_SLICER_ASTRO_SUPPORT_OPENGL ON)
endif()
message(STATUS "SlicerAstro GPU smoothing: OpenGL=${VTK_SLICER_ASTRO_SUPPORT_OPENGL}")
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
add_subdirectory(vtkFits)
add_subdirectory(vtkAstroCoordinates)
add_subdirectory(vtkAstroReproject)
add_subdirectory(vtkAstroImageFilters)
if(VTK_SLICER_ASTRO_SUPPORT_OPENGL)
  add_subdirectory(vtkOpenGLFilters)
endif()
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/SlicerAstroTesting.cmake)
add_subdirectory(AstroVolume)
# after AstroVolume: uses its widgets (qSlicerAstroVolumeModuleWidgets_INCLUDE_DIRS)
add_subdirectory(SlicerAstroWelcome)
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
add_subdirectory(AstroWorkbench)
add_subdirectory(SlicerAstroDataProbe)

#-----------------------------------------------------------------------------
# Test displays (Linux): the GUI tests run on their own virtual X displays
# (Xvfb), SLICERASTRO_TEST_DISPLAYS of them, so that they run in parallel
# (ctest -j<N>) and never on the desktop of the user; 0: the display of the
# environment, one GUI test at a time.
if(BUILD_TESTING)
  set(_default_test_displays 0)
  if(UNIX AND NOT APPLE)
    find_program(SLICERASTRO_XVFB_EXECUTABLE Xvfb)
    if(SLICERASTRO_XVFB_EXECUTABLE)
      set(_default_test_displays 8)
    endif()
  endif()
  set(SLICERASTRO_TEST_DISPLAYS ${_default_test_displays} CACHE STRING
    "Number of virtual X displays (Xvfb) of the GUI tests (0: the display of the environment)")
  set(SLICERASTRO_TEST_FIRST_DISPLAY 120 CACHE STRING "Number of the first virtual X display of the tests")
  if(SLICERASTRO_TEST_DISPLAYS GREATER 0)
    set(_displays_state ${CMAKE_BINARY_DIR}/Testing/Displays)
    add_test(NAME SlicerAstroTestDisplaysStart
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/CMake/SlicerAstroTestDisplays.sh start
        ${SLICERASTRO_XVFB_EXECUTABLE} ${SLICERASTRO_TEST_FIRST_DISPLAY} ${SLICERASTRO_TEST_DISPLAYS}
        ${_displays_state})
    set_property(TEST SlicerAstroTestDisplaysStart PROPERTY FIXTURES_SETUP SlicerAstroDisplays)
    add_test(NAME SlicerAstroTestDisplaysStop
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/CMake/SlicerAstroTestDisplays.sh stop ${_displays_state})
    set_property(TEST SlicerAstroTestDisplaysStop PROPERTY FIXTURES_CLEANUP SlicerAstroDisplays)
  endif()
  set_property(GLOBAL PROPERTY SLICERASTRO_NEXT_TEST_DISPLAY 0)
endif()

#-----------------------------------------------------------------------------
# Properties of all the tests:
# - the Python tests download the sample data into the cache of Slicer: they
#   are not run at the same time (concurrent downloads of a file conflict);
# - every test fails when VTK reports leaked objects at exit (3D Slicer builds
#   with VTK_DEBUG_LEAKS, the default for Debug builds); the exit code of the
#   application does not change when objects leak.
if(BUILD_TESTING)
  function(slicerastro_configure_tests directory)
    get_property(_tests DIRECTORY ${directory} PROPERTY TESTS)
    foreach(_test IN LISTS _tests)
      # the sample data are downloaded once before the Python tests (fixture
      # SlicerAstroSampleData); the tests only read the cache
      if(_test STREQUAL "py_AstroTestSampleData")
        set_property(TEST ${_test} DIRECTORY ${directory} PROPERTY FIXTURES_SETUP SlicerAstroSampleData)
      elseif(_test MATCHES "^py_")
        set_property(TEST ${_test} DIRECTORY ${directory} APPEND
          PROPERTY FIXTURES_REQUIRED SlicerAstroSampleData)
      endif()
      # tests with windows (the Slicer application, Qt widgets, OpenGL): on
      # their own virtual display, or one at a time on a shared display, so
      # that another application does not take the active window, the focus
      # or the input events of a test
      if(_test MATCHES "^(py_|qSlicer|qMRML|vtkAstroOpenGL)")
        if(SLICERASTRO_TEST_DISPLAYS GREATER 0)
          get_property(_slot GLOBAL PROPERTY SLICERASTRO_NEXT_TEST_DISPLAY)
          math(EXPR _display "${SLICERASTRO_TEST_FIRST_DISPLAY} + ${_slot}")
          math(EXPR _next "(${_slot} + 1) % ${SLICERASTRO_TEST_DISPLAYS}")
          set_property(GLOBAL PROPERTY SLICERASTRO_NEXT_TEST_DISPLAY ${_next})
          set_property(TEST ${_test} DIRECTORY ${directory} APPEND PROPERTY
            ENVIRONMENT "DISPLAY=:${_display}" "QT_QPA_PLATFORM=xcb" "XDG_CURRENT_DESKTOP=KDE")
          set_property(TEST ${_test} DIRECTORY ${directory} APPEND PROPERTY
            ENVIRONMENT_MODIFICATION "WAYLAND_DISPLAY=unset:")
          set_property(TEST ${_test} DIRECTORY ${directory} APPEND PROPERTY
            RESOURCE_LOCK SlicerAstroDisplay${_slot})
          set_property(TEST ${_test} DIRECTORY ${directory} APPEND PROPERTY
            FIXTURES_REQUIRED SlicerAstroDisplays)
        else()
          set_property(TEST ${_test} DIRECTORY ${directory} APPEND
            PROPERTY RESOURCE_LOCK SlicerAstroDisplay)
        endif()
      endif()
      set_property(TEST ${_test} DIRECTORY ${directory} APPEND
        PROPERTY FAIL_REGULAR_EXPRESSION "vtkDebugLeaks has detected LEAKS")
      # a test waiting forever (e.g. on a modal dialog) fails instead of
      # blocking the suite
      get_property(_timeout TEST ${_test} DIRECTORY ${directory} PROPERTY TIMEOUT)
      if(NOT _timeout)
        set_property(TEST ${_test} DIRECTORY ${directory} PROPERTY TIMEOUT 900)
      endif()
    endforeach()
    get_property(_subdirectories DIRECTORY ${directory} PROPERTY SUBDIRECTORIES)
    foreach(_subdirectory IN LISTS _subdirectories)
      slicerastro_configure_tests(${_subdirectory})
    endforeach()
  endfunction()
  slicerastro_configure_tests(${CMAKE_CURRENT_SOURCE_DIR})
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
  if(NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # The OpenMP runtime of 3DBarolo (GCC): not installed on all the Linux
    # systems (AstroModeling did not load without it). On Windows 3D Slicer
    # installs the one of MSVC (CMAKE_INSTALL_OPENMP_LIBRARIES).
    execute_process(
      COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libgomp.so.1
      OUTPUT_VARIABLE _gomp
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    if(IS_ABSOLUTE "${_gomp}" AND EXISTS "${_gomp}")
      get_filename_component(_gomp_file "${_gomp}" REALPATH)
      install(FILES ${_gomp_file}
        DESTINATION ${Slicer_INSTALL_THIRDPARTY_LIB_DIR}
        COMPONENT RuntimeLibraries
        RENAME libgomp.so.1
        )
    endif()
  endif()
endif()

#-----------------------------------------------------------------------------
set(EXTENSION_CPACK_INSTALL_CMAKE_PROJECTS)
set(${EXTENSION_NAME}_CPACK_INSTALL_CMAKE_PROJECTS "${EXTENSION_CPACK_INSTALL_CMAKE_PROJECTS}" CACHE STRING "List of external projects to install" FORCE)

#-----------------------------------------------------------------------------
list(APPEND CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${EXTENSION_NAME};ALL;/")
list(APPEND CPACK_INSTALL_CMAKE_PROJECTS "${${EXTENSION_NAME}_CPACK_INSTALL_CMAKE_PROJECTS}")
include(${Slicer_EXTENSION_GENERATE_CONFIG})
include(${Slicer_EXTENSION_CPACK})
