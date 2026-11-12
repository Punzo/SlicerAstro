# Builds the third-party libraries of SlicerAstro (CFITSIO, WCSLIB, FFTW,
# BBarolo) without Slicer, with the versions pinned in SuperBuild/External_*.cmake,
# and a smoke test using them. Used by .github/workflows/dependencies.yml on
# Linux, Windows and macOS.
cmake_minimum_required(VERSION 3.20)
project(SlicerAstroDependencies C CXX)

include(ExternalProject)

set(SLICERASTRO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(PREFIX "${CMAKE_BINARY_DIR}/install")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The pins of the superbuild of the extension (repository, commit; URL, hash)
function(read_pin name file)
  file(READ "${SLICERASTRO_SOURCE_DIR}/SuperBuild/${file}" content)
  if(content MATCHES "://(github\\.com/[^\"]+\\.git)\"")
    set(${name}_REPOSITORY "https://${CMAKE_MATCH_1}" PARENT_SCOPE)
  endif()
  if(content MATCHES "\"([0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]+)\" #")
    set(${name}_TAG "${CMAKE_MATCH_1}" PARENT_SCOPE)
  endif()
  if(content MATCHES "set\\(_version \"([0-9.]+)\"\\)")
    set(${name}_VERSION "${CMAKE_MATCH_1}" PARENT_SCOPE)
  endif()
  if(content MATCHES "SHA256=([0-9a-f]+)")
    set(${name}_SHA256 "${CMAKE_MATCH_1}" PARENT_SCOPE)
  endif()
endfunction()
read_pin(cfitsio External_cfitsio.cmake)
read_pin(wcslib External_wcslib.cmake)
read_pin(fftw3 External_fftw3.cmake)
read_pin(bbarolo External_bbarolo.cmake)
foreach(var cfitsio_REPOSITORY cfitsio_TAG wcslib_REPOSITORY wcslib_TAG fftw3_VERSION fftw3_SHA256
            bbarolo_REPOSITORY bbarolo_TAG)
  if(NOT ${var})
    message(FATAL_ERROR "${var} not found in SuperBuild/External_*.cmake")
  endif()
  message(STATUS "${var}: ${${var}}")
endforeach()

set(common_args
  -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
  -DCMAKE_INSTALL_PREFIX:PATH=${PREFIX}
  -DCMAKE_INSTALL_LIBDIR:STRING=lib
  -DCMAKE_PREFIX_PATH:PATH=${PREFIX}
  -DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}
  -DBUILD_SHARED_LIBS:BOOL=ON
  )

# zlib for CFITSIO: the system one, except on Windows
set(zlib_args)
set(zlib_depends)
if(WIN32)
  ExternalProject_Add(zlib
    URL "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz"
    URL_HASH "SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CMAKE_ARGS ${common_args}
    )
  set(zlib_args -DZLIB_ROOT:PATH=${PREFIX})
  set(zlib_depends zlib)
endif()

ExternalProject_Add(cfitsio
  GIT_REPOSITORY "${cfitsio_REPOSITORY}"
  GIT_TAG "${cfitsio_TAG}"
  CMAKE_ARGS ${common_args} ${zlib_args}
    -DUSE_CURL:BOOL=OFF -DUSE_PTHREADS:BOOL=OFF -DTESTS:BOOL=OFF -DUTILS:BOOL=OFF
  DEPENDS ${zlib_depends}
  )

ExternalProject_Add(wcslib
  GIT_REPOSITORY "${wcslib_REPOSITORY}"
  GIT_TAG "${wcslib_TAG}"
  CMAKE_ARGS ${common_args}
    -DWCSLIB_USE_CFITSIO:BOOL=ON
    -DCFITSIO_INCLUDE_DIR:PATH=${PREFIX}/include
    -DCFITSIO_LIBRARY_DIR:PATH=${PREFIX}/lib
  DEPENDS cfitsio
  )

set(fftw_simd OFF)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND NOT CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
  set(fftw_simd ON)
endif()
ExternalProject_Add(fftw3
  URL "https://www.fftw.org/fftw-${fftw3_VERSION}.tar.gz"
  URL_HASH "SHA256=${fftw3_SHA256}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  CMAKE_ARGS ${common_args}
    -DBUILD_TESTS:BOOL=OFF -DDISABLE_FORTRAN:BOOL=ON -DENABLE_THREADS:BOOL=OFF -DENABLE_OPENMP:BOOL=OFF
    -DENABLE_FLOAT:BOOL=OFF -DENABLE_SSE2:BOOL=${fftw_simd} -DENABLE_AVX:BOOL=${fftw_simd}
    -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5
  )

ExternalProject_Add(bbarolo
  GIT_REPOSITORY "${bbarolo_REPOSITORY}"
  GIT_TAG "${bbarolo_TAG}"
  CMAKE_ARGS ${common_args}
    -DBBAROLO_THROW_ON_ERROR:BOOL=ON
    -DCFITSIO_INCLUDE_DIR:PATH=${PREFIX}/include
    -DCFITSIO_LIBRARY_DIR:PATH=${PREFIX}/lib
    -DWCSLIB_INCLUDE_DIR:PATH=${PREFIX}/include
    -DWCSLIB_LIBRARY_DIR:PATH=${PREFIX}/lib
    -DFFTW3_INCLUDE_DIR:PATH=${PREFIX}/include
    -DFFTW3_LIBRARY_DIR:PATH=${PREFIX}/lib
  DEPENDS cfitsio wcslib fftw3
  )

ExternalProject_Add(smoke
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/smoke"
  CMAKE_ARGS ${common_args}
  INSTALL_COMMAND ""
  DEPENDS bbarolo
  )
