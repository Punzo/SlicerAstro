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

  set(WCSLIB_DOWNLOAD_VERSION "5.15" CACHE STRING "Version of WCSlib source package to download")
  set_property(CACHE WCSLIB_DOWNLOAD_VERSION PROPERTY STRINGS "5.15")

  set(WCSlib_5.15_URL ftp://ftp.atnf.csiro.au/pub/software/wcslib/wcslib.tar.bz2) 

  if(NOT DEFINED WCSlib_${WCSLIB_DOWNLOAD_VERSION}_URL)
      message(FATAL_ERROR "There is no source version of WCSlib ${WCSLIB_DOWNLOAD_VERSION} available, contact the developers, please!")
  endif()

  set(EP_INSTALL_DIR ${CMAKE_BINARY_DIR}/${proj}-install)

  set(WCSLIB_INCLUDE_DIR "${EP_INSTALL_DIR}/include")
  set(WCSLIB_LIBRARY_DIR "${EP_INSTALL_DIR}/lib")

  #------------------------------------------------------------------------------
  ExternalProject_Add(${proj}
    ${${proj}_EP_ARGS}
    URL ${WCSlib_5.15_URL}
    URL_MD5 "84d688bbb2a949b172b444b37c0011e3"
    SOURCE_DIR ${proj}
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure
      --with-cfitsiolib=${CFITSIO_LIBRARY_DIR}
      --with-cfitsioinc=${CFITSIO_INCLUDE_DIR}
      --prefix=${EP_INSTALL_DIR}
      --without-pgplot
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    DEPENDS
      ${${proj}_DEPENDENCIES}
    )

else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDS})
endif()

mark_as_superbuild(
  VARS
    WCSLIB_INCLUDE_DIR:PATH
    WCSLIB_LIBRARY_DIR:PATH
  )

ExternalProject_Message(${proj} "WCSLIB_INCLUDE_DIR:${WCSLIB_INCLUDE_DIR}")
ExternalProject_Message(${proj} "WCSLIB_LIBRARY_DIR:${WCSLIB_LIBRARY_DIR}")
