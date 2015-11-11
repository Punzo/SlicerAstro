set(proj wcslib)

# Set dependency list
set(${proj}_DEPENDS cfitsio)

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj)

if(${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})
  unset(wcslib_DIR CACHE)
  find_package(wcslib REQUIRED)
  set(WCSLIB_INCLUDE_DIR ${WCSLIB_INCLUDE_DIRS})
  set(WCSLIB_LIBRARY ${WCSLIB_LIBRARIES})
endif()


# Sanity checks
if(DEFINED wcslib_DIR AND NOT EXISTS ${wcslib_DIR})
  message(FATAL_ERROR "wcslib_DIR variable is defined but corresponds to nonexistent directory")
endif()

if(NOT DEFINED ${proj}_DIR AND NOT ${CMAKE_PROJECT_NAME}_USE_SYSTEM_${proj})

  set(WCSLIB_DOWNLOAD_VERSION "5.8" CACHE STRING "Version of WCSlib source package to download")
  set_property(CACHE WCSLIB_DOWNLOAD_VERSION PROPERTY STRINGS "5.8")

  set(WCSlib_5.8_URL ftp://ftp.atnf.csiro.au/pub/software/wcslib/wcslib.tar.bz2) 

  if(NOT DEFINED WCSlib_${WCSLIB_DOWNLOAD_VERSION}_URL)
      message(FATAL_ERROR "There is no source version of WCSlib ${WCSLIB_DOWNLOAD_VERSION} available, contact the developers, please!")
  endif()

  set(EP_SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj})
  include(ExternalProjectForNonCMakeProject)

  get_filename_component(_cfitsio_library ${CFITSIO_LIBRARY} PATH)
  get_filename_component(_cfitsio_include ${CFITSIO_INCLUDE_DIR} PATH)


  #------------------------------------------------------------------------------
  ExternalProject_Add(${proj}
    ${${proj}_EP_ARGS}
    URL ${WCSlib_5.8_URL}
    #URL_MD5 ""
    SOURCE_DIR ${proj}
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure --with-cfitsiolib=_cfitsio_library --with-cfitsioinc=_cfitsio_include  --prefix=${EP_SOURCE_DIR} --exec-prefix=${EP_SOURCE_DIR} --without-pgplot
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    DEPENDS
      ${${proj}_DEPENDENCIES}
    )
    set(${proj}_DIR ${EP_SOURCE_DIR})
    set(WCSLIB_INCLUDE_DIR ${EP_SOURCE_DIR}/include/wcslib)
    set(WCSLIB_LIBRARY ${EP_SOURCE_DIR}/lib)

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
