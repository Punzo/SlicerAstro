set(KIT qSlicer${MODULE_NAME}Module)

#-----------------------------------------------------------------------------
set(KIT_TEST_SRCS
  vtkMRMLAstroMomentMapsParametersNodeTest1.cxx
  )

#-----------------------------------------------------------------------------
slicerMacroConfigureModuleCxxTestDriver(
  NAME ${KIT}
  SOURCES ${KIT_TEST_SRCS}
  WITH_VTK_DEBUG_LEAKS_CHECK
  )

add_subdirectory(Python)

#-----------------------------------------------------------------------------
simple_test(vtkMRMLAstroMomentMapsParametersNodeTest1)
