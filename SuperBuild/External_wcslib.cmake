set(proj wcslib)

# Set dependency list
set(${proj}_DEPENDENCIES "cfitsio")

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj DEPENDS_VAR ${proj}_DEPENDENCIES)

if(${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})
  unset(WCSLIB_DIR CACHE)
  find_package(wcslib REQUIRED)
  set(WCSLIB_INCLUDE_DIR ${WCSLIB_INCLUDE_DIRS})
  set(WCSLIB_LIBRARY ${WCSLIB_LIBRARIES})
endif()

# Sanity checks
if(DEFINED WCSLIB_DIR AND NOT EXISTS ${WCSLIB_DIR})
  message(FATAL_ERROR "WCSLIB_DIR variable is defined but corresponds to nonexistent directory")
endif()

if(NOT DEFINED ${proj}_DIR AND NOT ${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})

  set(WCSLIB_DOWNLOAD_VERSION "5.15" CACHE STRING "Version of WCSlib source package to download")
  set_property(CACHE WCSLIB_DOWNLOAD_VERSION PROPERTY STRINGS "5.15")

  set(WCSlib_5.15_URL ftp://ftp.atnf.csiro.au/pub/software/wcslib/wcslib.tar.bz2) 

  if(NOT DEFINED WCSlib_${WCSLIB_DOWNLOAD_VERSION}_URL)
      message(FATAL_ERROR "There is no source version of WCSlib ${WCSLIB_DOWNLOAD_VERSION} available, contact the developers, please!")
  endif()

  set(EP_SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj})
  set(EP_BINARY_DIR ${CMAKE_BINARY_DIR}/${proj}-build)
  include(ExternalProjectForNonCMakeProject)

  get_filename_component(_cfitsio_library ${CFITSIO_LIBRARY} PATH)
  get_filename_component(_cfitsio_include ${CFITSIO_INCLUDE_DIR} PATH)


  #------------------------------------------------------------------------------
  ExternalProject_Add(${proj}
    ${${proj}_EP_ARGS}
    URL ${WCSlib_5.15_URL}
    URL_MD5 "84d688bbb2a949b172b444b37c0011e3"
    SOURCE_DIR ${proj}
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure --with-cfitsiolib=_cfitsio_library --with-cfitsioinc=_cfitsio_include  --prefix=${EP_BINARY_DIR} --exec-prefix=${EP_BINARY_DIR} --without-pgplot
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    DEPENDS
      ${${proj}_DEPENDENCIES}
    )
    set(WCSLIB_INCLUDE_DIR ${EP_BINARY_DIR}/include/wcslib)
    set(WCSLIB_LIBRARY ${EP_BINARY_DIR}/lib)
    set(WCSLIB_DIR ${EP_BINARY_DIR}/lib)

else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDS})
endif()

mark_as_superbuild(
  VARS
    WCSLIB_INCLUDE_DIR:${WCSLIB_INCLUDE_DIR}
    WCSLIB_LIBRARY:${WCSLIB_LIBRARY}
  LABELS "FIND_PACKAGE"
  )

ExternalProject_Message(${proj} "WCSLIB_INCLUDE_DIR:${WCSLIB_INCLUDE_DIR}")
ExternalProject_Message(${proj} "WCSLIB_LIBRARY:${WCSLIB_LIBRARY}")
ExternalProject_Message(${proj} "WCSLIB_DIR:${WCSLIB_DIR}")
mark_as_superbuild(WCSLIB_DIR:PATH)
