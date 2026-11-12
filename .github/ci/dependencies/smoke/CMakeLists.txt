# Smoke test of the third-party libraries of SlicerAstro (see ../CMakeLists.txt)
cmake_minimum_required(VERSION 3.20)
project(SlicerAstroDependenciesSmoke CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_path(CFITSIO_INCLUDE_DIR fitsio.h REQUIRED)
find_library(CFITSIO_LIBRARY cfitsio REQUIRED)
find_path(WCSLIB_INCLUDE_DIR wcslib/wcs.h REQUIRED)
find_library(WCSLIB_LIBRARY NAMES wcslib wcs REQUIRED)
find_path(FFTW3_INCLUDE_DIR fftw3.h REQUIRED)
find_library(FFTW3_LIBRARY NAMES fftw3 libfftw3-3 REQUIRED)
find_path(BBAROLO_INCLUDE_DIR bbarolo/Arrays/header.hh REQUIRED)
find_library(BBAROLO_LIBRARY bbarolo REQUIRED)

add_executable(smoke smoke.cxx)
target_include_directories(smoke PRIVATE
  ${CFITSIO_INCLUDE_DIR} ${WCSLIB_INCLUDE_DIR} ${FFTW3_INCLUDE_DIR} ${BBAROLO_INCLUDE_DIR}/bbarolo)
target_link_libraries(smoke PRIVATE ${BBAROLO_LIBRARY} ${WCSLIB_LIBRARY} ${CFITSIO_LIBRARY} ${FFTW3_LIBRARY})
if(MSVC)
  target_compile_definitions(smoke PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
endif()
