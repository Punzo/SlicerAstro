set(KIT ${PROJECT_NAME})

#-----------------------------------------------------------------------------
set(INPUT "${MRMLCore_SOURCE_DIR}/Testing/TestData")

#-----------------------------------------------------------------------------
set(KIT_TEST_SRCS
  )

#-----------------------------------------------------------------------------
slicerMacroConfigureModuleCxxTestDriver(
  NAME ${KIT}
  SOURCES ${KIT_TEST_SRCS}
  TARGET_LIBRARIES vtkSlicerAstroVolumeModuleLogic
  INCLUDE_DIRECTORIES
    ${vtkSlicerAstroVolumeModuleLogic_SOURCE_DIR}
    ${vtkSlicerAstroVolumeModuleLogic_BINARY_DIR}
  WITH_VTK_DEBUG_LEAKS_CHECK
  )

#-----------------------------------------------------------------------------

