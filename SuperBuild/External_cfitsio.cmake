
set(proj cfitsio)

# Set dependency list
set(${proj}_DEPENDS "")

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj)

if(${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})
  #unset(cfitsio_DIR CACHE)
  #find_package(CFITSIO REQUIRED)
  #set(CFITSIO_INCLUDE_DIR ${CFITSIO_INCLUDE_DIRS})
  #set(CFITSIO_LIBRARY ${CFITSIO_LIBRARIES})
endif()

# Sanity checks
if(DEFINED cfitsio_DIR AND NOT EXISTS ${cfitsio_DIR})
  message(FATAL_ERROR "cfitsio_DIR variable is defined but corresponds to nonexistent directory")
endif()

if(NOT DEFINED ${proj}_DIR AND NOT ${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})

  if(NOT DEFINED git_protocol)
    set(git_protocol "git")
  endif()

  set(EP_SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj})
  set(EP_BINARY_DIR ${CMAKE_BINARY_DIR}/${proj}-build)

  ExternalProject_Add(${proj}
    ${${proj}_EP_ARGS}
    #version 3.360 not working with cmake
    GIT_REPOSITORY "${git_protocol}://github.com/healpy/cfitsio"
    GIT_TAG "6a1ea4ec55f0f4906b6e5723236b05cb1b0cfdac"
    SOURCE_DIR ${EP_SOURCE_DIR}
    BINARY_DIR ${EP_BINARY_DIR}
    INSTALL_DIR ${EP_INSTALL_DIR}
    CMAKE_CACHE_ARGS
      -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
      -DCMAKE_CXX_FLAGS:STRING=${ep_common_cxx_flags}
      -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
      -DCMAKE_C_FLAGS:STRING=${ep_common_c_flags}
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
    DEPENDS
      ${${proj}_DEPENDS}
    )
  set(${proj}_DIR ${EP_BINARY_DIR})
  set(CFITSIO_INCLUDE_DIR ${EP_BINARY_DIR})
  set(CFITSIO_LIBRARY ${EP_SOURCE_DIR})
else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDS})
endif()

mark_as_superbuild(
  VARS
    CFITSIO_INCLUDE_DIR:${CFITSIO_INCLUDE_DIR}
    CFITSIO_LIBRARY:${CFITSIO_LIBRARY}
  LABELS "FIND_PACKAGE"
  )

ExternalProject_Message(${proj} "CFITSIO_INCLUDE_DIR:${CFITSIO_INCLUDE_DIR}")
ExternalProject_Message(${proj} "CFITSIO_LIBRARY:${CFITSIO_LIBRARY}")
