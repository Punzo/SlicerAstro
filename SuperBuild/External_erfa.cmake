set(proj erfa)

# ERFA (Essential Routines for Fundamental Astronomy, BSD-3, derived from the
# IAU SOFA library): celestial frame conversions (FK4, FK5, ICRS, Galactic)
# and the velocity of the Earth, for the spectral reference frames.

# Set dependency list
set(${proj}_DEPENDS "")

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj)

if(${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})
  message(FATAL_ERROR "Enabling ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj} is not supported !")
endif()

# Sanity checks
foreach(varname IN ITEMS ERFA_INCLUDE_DIR ERFA_LIBRARY_DIR)
  if(DEFINED ${varname} AND NOT EXISTS ${${varname}})
    message(FATAL_ERROR "${varname} [${${varname}}] variable is defined but corresponds to nonexistent directory")
  endif()
endforeach()

if((NOT DEFINED ERFA_INCLUDE_DIR OR NOT DEFINED ERFA_LIBRARY_DIR)
   AND NOT ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})

  ExternalProject_SetIfNotDefined(
    ${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_REPOSITORY
    "${EP_GIT_PROTOCOL}://github.com/Punzo/erfa.git"
    QUIET
    )

  ExternalProject_SetIfNotDefined(
    ${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_TAG
    "52fb5df87ebb0bf683e26af126fed38a2856251e" # slicerastro-cmake: upstream master 1a8044c (2.0.1+) with a CMake build
    QUIET
    )

  set(EP_SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj})
  set(EP_BINARY_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  set(EP_INSTALL_DIR ${CMAKE_BINARY_DIR}/${proj}-install)

  ExternalProject_Add(${proj}
    ${${proj}_EP_ARGS}
    GIT_REPOSITORY "${${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_REPOSITORY}"
    GIT_TAG "${${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_TAG}"
    SOURCE_DIR ${EP_SOURCE_DIR}
    BINARY_DIR ${EP_BINARY_DIR}
    INSTALL_DIR ${EP_INSTALL_DIR}
    CMAKE_CACHE_ARGS
      # Compiler settings
      -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
      -DCMAKE_C_FLAGS:STRING=${ep_common_c_flags}
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
      -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
      # Output directories
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_BIN_DIR}
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH=${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR}
      -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR}
      -DCMAKE_INSTALL_PREFIX:PATH=${EP_INSTALL_DIR}
      # Options
      -DBUILD_SHARED_LIBS:BOOL=ON
      -DBUILD_TESTING:BOOL=OFF
    DEPENDS
      ${${proj}_DEPENDS}
    )

  set(ERFA_INCLUDE_DIR ${EP_INSTALL_DIR}/include)
  set(ERFA_LIBRARY_DIR ${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR})

else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDS})
endif()

set(${proj}_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
mark_as_superbuild(VARS ${proj}_DIR:PATH)

mark_as_superbuild(
  VARS
    ERFA_INCLUDE_DIR:PATH
    ERFA_LIBRARY_DIR:PATH
  )

ExternalProject_Message(${proj} "ERFA_INCLUDE_DIR:${ERFA_INCLUDE_DIR}")
ExternalProject_Message(${proj} "ERFA_LIBRARY_DIR:${ERFA_LIBRARY_DIR}")

#-----------------------------------------------------------------------------
# Launcher settings of the build tree: the library in the library paths of the
# launcher of 3D Slicer when SlicerAstro is bundled in an application
# (Slicer_EXTENSION_SOURCE_DIRS, e.g. SlicerAstroApp); a standalone extension
# build lists the folder in its AdditionalLauncherSettings.ini
if(WIN32)
  set(${proj}_PATHS_LAUNCHER_BUILD ${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_BIN_DIR})
  mark_as_superbuild(
    VARS ${proj}_PATHS_LAUNCHER_BUILD
    LABELS "PATHS_LAUNCHER_BUILD"
    )
else()
  set(${proj}_LIBRARY_PATHS_LAUNCHER_BUILD ${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR})
  mark_as_superbuild(
    VARS ${proj}_LIBRARY_PATHS_LAUNCHER_BUILD
    LABELS "LIBRARY_PATHS_LAUNCHER_BUILD"
    )
endif()
