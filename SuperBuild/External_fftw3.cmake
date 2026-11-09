set(proj fftw3)

# Set dependency list
set(${proj}_DEPENDS "")

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj)

if(${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})
  message(FATAL_ERROR "Enabling ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj} is not supported !")
endif()

# Sanity checks
foreach(varname IN ITEMS FFTW3_INCLUDE_DIR FFTW3_LIBRARY_DIR)
  if(DEFINED ${varname} AND NOT EXISTS ${${varname}})
    message(FATAL_ERROR "${varname} [${${varname}}] variable is defined but corresponds to nonexistent directory")
  endif()
endforeach()

if((NOT DEFINED FFTW3_INCLUDE_DIR OR NOT DEFINED FFTW3_LIBRARY_DIR)
   AND NOT ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})

  set(_version "3.3.11")

  # SSE2/AVX codelets (with runtime CPU detection) only on x86_64
  set(_fftw_x86_simd OFF)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND NOT CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    set(_fftw_x86_simd ON)
  endif()

  set(EP_SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj})
  set(EP_BINARY_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  set(EP_INSTALL_DIR ${CMAKE_BINARY_DIR}/${proj}-install)

  ExternalProject_Add(${proj}
    ${${proj}_EP_ARGS}
    URL "https://www.fftw.org/fftw-${_version}.tar.gz"
    URL_HASH "SHA256=5630c24cdeb33b131612f7eb4b1a9934234754f9f388ff8617458d0be6f239a1"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
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
      # Options and dependencies
      -DBUILD_SHARED_LIBS:BOOL=ON
      -DBUILD_TESTS:BOOL=OFF
      -DDISABLE_FORTRAN:BOOL=ON
      -DENABLE_THREADS:BOOL=OFF
      -DENABLE_OPENMP:BOOL=OFF
      -DENABLE_FLOAT:BOOL=OFF
      -DENABLE_SSE2:BOOL=${_fftw_x86_simd}
      -DENABLE_AVX:BOOL=${_fftw_x86_simd}
      -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5
    DEPENDS
      ${${proj}_DEPENDS}
    )

  set(FFTW3_INCLUDE_DIR ${EP_INSTALL_DIR}/include)
  set(FFTW3_LIBRARY_DIR ${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR})

else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDS})
endif()

set(${proj}_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
mark_as_superbuild(VARS ${proj}_DIR:PATH)

mark_as_superbuild(
  VARS
    FFTW3_INCLUDE_DIR:PATH
    FFTW3_LIBRARY_DIR:PATH
  )

ExternalProject_Message(${proj} "FFTW3_INCLUDE_DIR:${FFTW3_INCLUDE_DIR}")
ExternalProject_Message(${proj} "FFTW3_LIBRARY_DIR:${FFTW3_LIBRARY_DIR}")
