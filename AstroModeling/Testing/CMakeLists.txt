set(KIT qSlicer${MODULE_NAME}Module)

#-----------------------------------------------------------------------------
set(KIT_TEST_SRCS
  vtkMRMLAstroModelingParametersNodeTest1.cxx
  vtkSlicerAstroModelingBBaroloErrorTest1.cxx
  )

# BBarolo headers, built with BBAROLO_THROW_ON_ERROR as libbbarolo
include_directories(${BBAROLO_INCLUDE_DIR} ${CFITSIO_INCLUDE_DIR} ${WCSLIB_INCLUDE_DIR} ${FFTW3_INCLUDE_DIR})
add_compile_definitions(BBAROLO_THROW_ON_ERROR)

#-----------------------------------------------------------------------------
slicerMacroConfigureModuleCxxTestDriver(
  NAME ${KIT}
  SOURCES ${KIT_TEST_SRCS}
  TARGET_LIBRARIES ${BBAROLO_LIBRARIES}
  WITH_VTK_DEBUG_LEAKS_CHECK
  )

add_subdirectory(Python)

#-----------------------------------------------------------------------------
simple_test(vtkMRMLAstroModelingParametersNodeTest1)
simple_test(vtkSlicerAstroModelingBBaroloErrorTest1)
