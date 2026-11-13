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

  set(_version "2.0.1")

  set(EP_SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj})
  set(EP_BINARY_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  set(EP_INSTALL_DIR ${CMAKE_BINARY_DIR}/${proj}-install)

  ExternalProject_Add(${proj}
    ${${proj}_EP_ARGS}
    URL "https://github.com/liberfa/erfa/releases/download/v${_version}/erfa-${_version}.tar.xz"
    URL_HASH "SHA256=45111f0addf6ef69345aaf093bd054705b41ac2db7541dd3f0e105362c8e20e4"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR ${EP_SOURCE_DIR}
    BINARY_DIR ${EP_BINARY_DIR}
    INSTALL_DIR ${EP_INSTALL_DIR}
    # ERFA has no CMake build: SlicerAstro's one
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_SOURCE_DIR}/SuperBuild/erfa/CMakeLists.txt ${EP_SOURCE_DIR}/CMakeLists.txt
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
