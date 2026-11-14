set(proj wcslib)

# Set dependency list
set(${proj}_DEPENDS "cfitsio")

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj)

if(${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})
  message(FATAL_ERROR "Enabling ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj} is not supported !")
endif()

# Sanity checks
foreach(varname IN ITEMS WCSLIB_INCLUDE_DIR WCSLIB_LIBRARY_DIR)
  if(DEFINED ${varname} AND NOT EXISTS ${${varname}})
    message(FATAL_ERROR "${varname} [${${varname}}] variable is defined but corresponds to nonexistent directory")
  endif()
endforeach()

if((NOT DEFINED WCSLIB_INCLUDE_DIR OR NOT DEFINED WCSLIB_LIBRARY_DIR)
   AND NOT ${SUPERBUILD_TOPLEVEL_PROJECT}_USE_SYSTEM_${proj})

  ExternalProject_SetIfNotDefined(
    ${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_REPOSITORY
    "${EP_GIT_PROTOCOL}://github.com/Punzo/wcslib.git"
    QUIET
    )

  ExternalProject_SetIfNotDefined(
    ${SUPERBUILD_TOPLEVEL_PROJECT}_${proj}_GIT_TAG
    "773f4e19549127aaba06ab5bdd98bcffad8de860" # branch wcslib-8.10-cmake (WCSLIB 8.10 + SIP fix, MSVC)
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
      -DCMAKE_INSTALL_LIBDIR:STRING=lib
      # Options and dependencies
      -DBUILD_SHARED_LIBS:BOOL=ON
      -DWCSLIB_USE_CFITSIO:BOOL=ON
      -DCFITSIO_INCLUDE_DIR:PATH=${CFITSIO_INCLUDE_DIR}
      -DCFITSIO_LIBRARY_DIR:PATH=${CFITSIO_LIBRARY_DIR}
    DEPENDS
      ${${proj}_DEPENDS}
    )

  # Headers are available both as <wcslib/xxx.h> and <xxx.h>
  set(WCSLIB_INCLUDE_DIR
    ${EP_INSTALL_DIR}/include
    ${EP_INSTALL_DIR}/include/wcslib
    )
  set(WCSLIB_LIBRARY_DIR ${CMAKE_BINARY_DIR}/${Slicer_THIRDPARTY_LIB_DIR})

else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDS})
endif()

set(${proj}_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
mark_as_superbuild(VARS ${proj}_DIR:PATH)

mark_as_superbuild(
  VARS
    WCSLIB_INCLUDE_DIR:PATH
    WCSLIB_LIBRARY_DIR:PATH
  )

ExternalProject_Message(${proj} "WCSLIB_INCLUDE_DIR:${WCSLIB_INCLUDE_DIR}")
ExternalProject_Message(${proj} "WCSLIB_LIBRARY_DIR:${WCSLIB_LIBRARY_DIR}")
