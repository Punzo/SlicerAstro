set(proj cfitsio)

# Set dependency list
set(${proj}_DEPENDS "")

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj)

if(${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})
  message(FATAL_ERROR "Enabling ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj} is not supported !")
endif()

# Sanity checks
foreach(varname IN ITEMS CFITSIO_INCLUDE_DIR CFITSIO_LIBRARY_DIR)
  if(DEFINED ${varname} AND NOT EXISTS ${${varname}})
    message(FATAL_ERROR "${varname} [${${varname}}] variable is defined but corresponds to nonexistent directory")
  endif()
endforeach()

if((NOT DEFINED CFITSIO_INCLUDE_DIR OR NOT DEFINED CFITSIO_LIBRARY_DIR)
   AND NOT ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})

  ExternalProject_SetIfNotDefined(
    ${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_REPOSITORY
    "${EP_GIT_PROTOCOL}://github.com/HEASARC/cfitsio.git"
    QUIET
    )

  ExternalProject_SetIfNotDefined(
    ${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_TAG
    "d6d27653740864a0483e74cff23c53e34e1dceee" # cfitsio-4.7.0
    QUIET
    )

  set(EP_SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj})
  set(EP_BINARY_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  set(EP_INSTALL_DIR ${CMAKE_BINARY_DIR}/${proj}-install)

  # cfitsio is built thread-safe with pthreads on Unix; there is no
  # pthreads implementation available by default on Windows.
  set(_use_pthreads ON)
  if(WIN32)
    set(_use_pthreads OFF)
  endif()

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
      -DCMAKE_INSTALL_LIBDIR:STRING=lib
      # Options
      -DBUILD_SHARED_LIBS:BOOL=ON
      -DUSE_CURL:BOOL=OFF
      -DUSE_BZIP2:BOOL=OFF
      -DUSE_PTHREADS:BOOL=${_use_pthreads}
      -DTESTS:BOOL=OFF
      -DUTILS:BOOL=OFF
      # Dependencies: zlib provided by Slicer
      -DZLIB_ROOT:PATH=${ZLIB_ROOT}
      -DZLIB_INCLUDE_DIR:PATH=${ZLIB_INCLUDE_DIR}
      -DZLIB_LIBRARY:FILEPATH=${ZLIB_LIBRARY}
    DEPENDS
      ${${proj}_DEPENDS}
    )

  set(CFITSIO_INCLUDE_DIR ${EP_INSTALL_DIR}/include)
  set(CFITSIO_LIBRARY_DIR ${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR})

else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDS})
endif()

set(${proj}_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
mark_as_superbuild(VARS ${proj}_DIR:PATH)

mark_as_superbuild(
  VARS
    CFITSIO_INCLUDE_DIR:PATH
    CFITSIO_LIBRARY_DIR:PATH
  )

ExternalProject_Message(${proj} "CFITSIO_INCLUDE_DIR:${CFITSIO_INCLUDE_DIR}")
ExternalProject_Message(${proj} "CFITSIO_LIBRARY_DIR:${CFITSIO_LIBRARY_DIR}")
