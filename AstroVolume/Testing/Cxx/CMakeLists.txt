set(KIT qSlicer${MODULE_NAME}Module)

#-----------------------------------------------------------------------------
set(TEMP ${Slicer_BINARY_DIR}/Testing/Temporary)
set(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/..)

#-----------------------------------------------------------------------------
set(KIT_TEST_SRCS
  qSlicer${MODULE_NAME}IOOptionsWidgetTest1.cxx
  qSlicer${MODULE_NAME}ModuleWidgetTest1.cxx
  )

#-----------------------------------------------------------------------------
set(KIT_LIBRARIES
  vtkSlicerAstroVolumeModuleLogic
  vtkSlicerVolumesModuleLogic
  )

#-----------------------------------------------------------------------------
slicerMacroConfigureModuleCxxTestDriver(
  NAME ${KIT}
  SOURCES ${KIT_TEST_SRCS}
  TARGET_LIBRARIES ${KIT_LIBRARIES}
  WITH_VTK_DEBUG_LEAKS_CHECK
  WITH_VTK_ERROR_OUTPUT_CHECK
  )

#-----------------------------------------------------------------------------
simple_test(qSlicerAstroVolumeIOOptionsWidgetTest1)
simple_test(qSlicerAstroVolumeModuleWidgetTest1 ${INPUT}/WEIN069.fits)
