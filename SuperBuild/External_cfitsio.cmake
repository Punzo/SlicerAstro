set(proj cfitsio)

# Set dependency list
set(${proj}_DEPENDENCIES "")

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj DEPENDS_VAR ${proj}_DEPENDENCIES)

if(${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})
  unset(CFITSIO_DIR CACHE)
  find_package(cfitsio REQUIRED)
  set(CFITSIO_INCLUDE_DIR ${CFITSIO_INCLUDE_DIRS})
  set(CFITSIO_LIBRARY ${CFITSIO_LIBRARIES})
endif()

# Sanity checks
if(DEFINED CFITSIO_DIR AND NOT EXISTS ${CFITSIO_DIR})
  message(FATAL_ERROR "CFITSIO_DIR variable is defined but corresponds to nonexistent directory")
endif()

if(NOT DEFINED ${proj}_DIR AND NOT ${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})

  if(NOT DEFINED git_protocol)
    set(git_protocol "git")
  endif()

  set(EP_SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj})
  set(EP_BINARY_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  set(${proj}_INSTALL_DIR ${CMAKE_BINARY_DIR}/${proj}-install)

  ExternalProject_Add(${proj}
    ${${proj}_EP_ARGS}
    GIT_REPOSITORY "${git_protocol}://github.com/healpy/cfitsio"
    GIT_TAG "1f660bd88b464c8339a0f684a2d366b253184488"
    SOURCE_DIR ${EP_SOURCE_DIR}
    BINARY_DIR ${EP_BINARY_DIR}
    INSTALL_DIR ${${proj}_INSTALL_DIR}
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
    set(CFITSIO_INCLUDE_DIR ${EP_SOURCE_DIR})
    set(CFITSIO_LIBRARY ${EP_BINARY_DIR})
    set(CFITSIO_DIR ${${proj}_INSTALL_DIR})

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
ExternalProject_Message(${proj} "CFITSIO_DIR:${CFITSIO_DIR}")
mark_as_superbuild(CFITSIO_DIR:PATH)

