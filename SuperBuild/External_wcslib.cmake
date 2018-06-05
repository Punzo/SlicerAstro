set(proj wcslib)

# Set dependency list
set(${proj}_DEPENDENCIES "cfitsio")

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj DEPENDS_VAR ${proj}_DEPENDENCIES)

if(${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})
  message(FATAL_ERROR "Enabling ${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj} is not supported !")
endif()

# Sanity checks
foreach(varname IN ITEMS WCSLIB_LIBRARY_DIR WCSLIB_INCLUDE_DIR)
  if(DEFINED ${varname} AND NOT EXISTS ${${varname}})
    message(FATAL_ERROR "${varname} variable is defined but corresponds to nonexistent directory")
  endif()
endforeach()

if((NOT DEFINED WCSLIB_LIBRARY_DIR OR NOT DEFINED WCSLIB_INCLUDE_DIR) AND NOT ${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})

  set(WCSLIB_DOWNLOAD_VERSION "5.18" CACHE STRING "Version of WCSlib source package to download")
  set_property(CACHE WCSLIB_DOWNLOAD_VERSION PROPERTY STRINGS "5.18")

  if(NOT DEFINED git_protocol)
    set(git_protocol "git")
  endif()

  set(${proj}_SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj})
  set(${proj}_BINARY_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  set(${proj}_INSTALL_DIR ${CMAKE_BINARY_DIR}/${proj}-install)

  ExternalProject_Add(${proj}
    ${${proj}_EP_ARGS}
    GIT_REPOSITORY "${git_protocol}://github.com/Punzo/wcslib"
    GIT_TAG "e6e4570a42197a5ca1b494b0cd1f3175b114ac3e"
    SOURCE_DIR ${${proj}_SOURCE_DIR}
    BINARY_DIR ${${proj}_BINARY_DIR}
    CMAKE_CACHE_ARGS
      -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
      -DCMAKE_CXX_FLAGS:STRING=${ep_common_cxx_flags}
      -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
      -DCMAKE_C_FLAGS:STRING=${ep_common_c_flags}
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DBUILD_TESTING:BOOL=OFF
      -DCFITSIO_LIBRARY_DIR:PATH=${CFITSIO_LIBRARY_DIR}
      -DCFITSIO_INCLUDE_DIR:PATH=${CFITSIO_INCLUDE_DIR}
      -DCMAKE_INSTALL_PREFIX:PATH=${${proj}_INSTALL_DIR}
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_BIN_DIR}
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY:PATH=${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR}
      -DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
      -DWCSLIB_INSTALL_RUNTIME_DIR:STRING=${Slicer_INSTALL_THIRDPARTY_LIB_DIR}
      -DWCSLIB_INSTALL_LIBRARY_DIR:STRING=${Slicer_INSTALL_THIRDPARTY_LIB_DIR}
      -DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}
      -DCMAKE_OSX_SYSROOT:PATH=${CMAKE_OSX_SYSROOT}
      -DCMAKE_MACOSX_RPATH:BOOL=0
    DEPENDS
      ${${proj}_DEPENDENCIES}
    )
  set(WCSLIB_INCLUDE_DIR
      ${${proj}_SOURCE_DIR}
      ${${proj}_SOURCE_DIR}/C
  )
  set(WCSLIB_LIBRARY_DIR ${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR})

else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDS})
endif()

set(${proj}_DIR ${${proj}_BINARY_DIR})
mark_as_superbuild(VARS ${proj}_DIR:PATH)

ExternalProject_Message(${proj} "${proj}_DIR:${${proj}_DIR}")

mark_as_superbuild(
  VARS
    WCSLIB_INCLUDE_DIR:PATH
    WCSLIB_LIBRARY_DIR:PATH
  )

ExternalProject_Message(${proj} "WCSLIB_INCLUDE_DIR:${WCSLIB_INCLUDE_DIR}")
ExternalProject_Message(${proj} "WCSLIB_LIBRARY_DIR:${WCSLIB_LIBRARY_DIR}")
