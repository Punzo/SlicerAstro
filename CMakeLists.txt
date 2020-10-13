cmake_minimum_required(VERSION 3.4)

project(SlicerAstro)

#-----------------------------------------------------------------------------
set(EXTENSION_NAME SlicerAstro)

# ------------------------------------------------------------------------
set(SLICERASTRO_VERSION_MAJOR "1")
set(SLICERASTRO_VERSION_MINOR "0")
set(SLICERASTRO_VERSION_PATCH "0")
set(SLICERASTRO_VERSION ${SLICERASTRO_VERSION_MAJOR}.${SLICERASTRO_VERSION_MINOR}.${SLICERASTRO_VERSION_PATCH})

#-----------------------------------------------------------------------------
# Extension meta-information
set(EXTENSION_HOMEPAGE "https://github.com/Punzo/SlicerAstro/wiki")
set(EXTENSION_CATEGORY "Astronomy")
set(EXTENSION_CONTRIBUTORS "Davide Punzo (Kapteyn Astronomical Institute), Thijs van der Hulst (Kapteyn Astronomical institute), Jos Roerdink (Johann Bernoulli Institute), Jean-Christophe Fillion-Robin (Kitware)")
set(EXTENSION_DESCRIPTION "SlicerAstro, an Astronomy (HI) extension for 3DSlicer")
set(EXTENSION_ICONURL "https://raw.githubusercontent.com/Punzo/SlicerAstro/master/SlicerAstroIcon.png")
set(EXTENSION_SCREENSHOTURLS "https://raw.githubusercontent.com/Punzo/SlicerAstroWikiImages/master/Screenshot.png")
set(EXTENSION_STATUS "Release")
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
if(UNIX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -ftree-vectorize -fPIC -Wuninitialized -std=c++11")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -ftree-vectorize -fPIC -Wuninitialized -std=c++11")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O2 -ftree-vectorize -fPIC -Wuninitialized -flto -std=c++11")
endif()

#-----------------------------------------------------------------------------
# configurating OpenMP
if(UNIX)
  include(FindOpenMP)
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
endif()

#-----------------------------------------------------------------------------
# configurating OpenMP Flags
if(UNIX)
  if(VTK_SLICER_ASTRO_SUPPORT_OPENMP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
  endif()
endif()

#-----------------------------------------------------------------------------
# configurating OpenGL2
if(UNIX)
  find_package(OpenGL)
  set(VTK_SLICER_ASTRO_SUPPORT_OPENGL ${OPENGL_FOUND})
  set(status "disabled")
  if(VTK_SLICER_ASTRO_SUPPORT_OPENGL)
    set(status "enabled")
    message(STATUS "OpenGL filtering support ${status}")
  endif()
  configure_file(
    ${SlicerAstro_SOURCE_DIR}/CMake/vtkSlicerAstroConfigure.h.in
    ${SlicerAstro_BINARY_DIR}/vtkSlicerAstroConfigure.h
    )
endif()

#-----------------------------------------------------------------------------
# SuperBuild setup
if(UNIX)
  option(${EXTENSION_NAME}_SUPERBUILD "Build ${EXTENSION_NAME} and the projects it depends on." ON)
  mark_as_advanced(${EXTENSION_NAME}_SUPERBUILD)
  if(${EXTENSION_NAME}_SUPERBUILD)
    include("${CMAKE_CURRENT_SOURCE_DIR}/SuperBuild.cmake")
    return()
  endif()
endif()

#-----------------------------------------------------------------------------
# set libraries variables
if(UNIX)
  # CFITSIO
  find_library(CFITSIO_LIBRARY cfitsio
    PATHS ${CFITSIO_LIBRARY_DIR} NO_DEFAULT_PATH
    )
  set(CFITSIO_LIBRARIES ${CFITSIO_LIBRARY})

  # WCS
  find_library (WCSLIB_LIBRARY wcslib
    PATHS ${WCSLIB_LIBRARY_DIR} NO_DEFAULT_PATH
    )
  set(WCSLIB_LIBRARIES ${WCSLIB_LIBRARY})

  # 3DBarolo
  find_library (BBAROLO_LIBRARY bbarolo
    PATHS ${BBAROLO_LIBRARY_DIR} NO_DEFAULT_PATH
    )
  set(BBAROLO_LIBRARIES ${BBAROLO_LIBRARY})
endif()

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
  add_subdirectory(Doxygen)
endif(UNIX)

#-----------------------------------------------------------------------------
# Install extension C/C++ packages
if(UNIX)
  # Install libraries
  include(SlicerFunctionInstallLibrary)
  foreach(library IN ITEMS ${CFITSIO_LIBRARY} ${WCSLIB_LIBRARY} ${BBAROLO_LIBRARY})
    message(STATUS "installing library:  ${library}" )
    slicerInstallLibrary(FILE ${library}
      DESTINATION ${Slicer_THIRDPARTY_LIB_DIR}
      COMPONENT RuntimeLibraries
      )
  endforeach()
endif()

#-----------------------------------------------------------------------------
# Install extension python packages
install(CODE "message(\"CPack: - Install directory: ${python_packages_DIR}\")")
install(
    DIRECTORY "${python_packages_DIR}/"
    DESTINATION ${Slicer_INSTALL_ROOT}${Slicer_BUNDLE_EXTENSIONS_LOCATION}
    COMPONENT RuntimeLibraries
    )

#-----------------------------------------------------------------------------
# Install extension deps packages
set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${CMAKE_BINARY_DIR};${EXTENSION_NAME};ALL;/")
if(UNIX)
  set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${cfitsio_DIR};cfitsio;RuntimeLibraries;/")
  set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${wcslib_DIR};wcslib;RuntimeLibraries;/")
  set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_INSTALL_CMAKE_PROJECTS};${bbarolo_DIR};bbarolo;RuntimeLibraries;/")
endif(UNIX)

include(${Slicer_EXTENSION_CPACK})
