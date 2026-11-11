set(proj bbarolo)

# Set dependency list
set(${proj}_DEPENDS "cfitsio" "wcslib" "fftw3")

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj)

if(${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})
  message(FATAL_ERROR "Enabling ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj} is not supported !")
endif()

# Sanity checks
foreach(varname IN ITEMS BBAROLO_INCLUDE_DIR BBAROLO_LIBRARY_DIR)
  if(DEFINED ${varname} AND NOT EXISTS ${${varname}})
    message(FATAL_ERROR "${varname} [${${varname}}] variable is defined but corresponds to nonexistent directory")
  endif()
endforeach()

if((NOT DEFINED BBAROLO_INCLUDE_DIR OR NOT DEFINED BBAROLO_LIBRARY_DIR)
   AND NOT ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})

  ExternalProject_SetIfNotDefined(
    ${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_REPOSITORY
    "${EP_GIT_PROTOCOL}://github.com/Punzo/Bbarolo.git"
    QUIET
    )

  ExternalProject_SetIfNotDefined(
    ${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_TAG
    "3d5b55046f92de089c7d7d9fadc54bd97af69067" # branch slicerastro-1.8 (BBarolo 1.8 + master fixes)
    QUIET
    )

  # BBarolo includes <wcslib/wcs.h>: pass the parent include directory
  list(GET WCSLIB_INCLUDE_DIR 0 wcslib_INSTALL_INCLUDE_DIR)

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
      -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
      -DCMAKE_CXX_FLAGS:STRING=${ep_common_cxx_flags}
      -DCMAKE_CXX_STANDARD:STRING=${CMAKE_CXX_STANDARD}
      -DCMAKE_CXX_STANDARD_REQUIRED:BOOL=${CMAKE_CXX_STANDARD_REQUIRED}
      -DCMAKE_CXX_EXTENSIONS:BOOL=${CMAKE_CXX_EXTENSIONS}
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
      -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
      # Output directories
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_BIN_DIR}
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH=${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR}
      -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR}
      -DCMAKE_INSTALL_PREFIX:PATH=${EP_INSTALL_DIR}
      -DCMAKE_INSTALL_LIBDIR:STRING=lib
      # Options and dependencies
      -DBUILD_SHARED_LIBS:BOOL=ON
      -DBBAROLO_THROW_ON_ERROR:BOOL=ON
      -DCFITSIO_INCLUDE_DIR:PATH=${CFITSIO_INCLUDE_DIR}
      -DCFITSIO_LIBRARY_DIR:PATH=${CFITSIO_LIBRARY_DIR}
      -DWCSLIB_INCLUDE_DIR:PATH=${wcslib_INSTALL_INCLUDE_DIR}
      -DWCSLIB_LIBRARY_DIR:PATH=${WCSLIB_LIBRARY_DIR}
      -DFFTW3_INCLUDE_DIR:PATH=${FFTW3_INCLUDE_DIR}
      -DFFTW3_LIBRARY_DIR:PATH=${FFTW3_LIBRARY_DIR}
    DEPENDS
      ${${proj}_DEPENDS}
    )

  set(BBAROLO_INCLUDE_DIR ${EP_SOURCE_DIR}/src)
  set(BBAROLO_LIBRARY_DIR ${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR})

else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDS})
endif()

set(${proj}_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
mark_as_superbuild(VARS ${proj}_DIR:PATH)

mark_as_superbuild(
  VARS
    BBAROLO_INCLUDE_DIR:PATH
    BBAROLO_LIBRARY_DIR:PATH
  )

ExternalProject_Message(${proj} "BBAROLO_INCLUDE_DIR:${BBAROLO_INCLUDE_DIR}")
ExternalProject_Message(${proj} "BBAROLO_LIBRARY_DIR:${BBAROLO_LIBRARY_DIR}")
