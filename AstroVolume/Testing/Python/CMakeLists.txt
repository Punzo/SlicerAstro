# (the tests are scripted module tests: QtTesting is not needed)
if(Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroVolumeSelfTest
    SCRIPTS AstroVolumeSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroVolumeSelfTest.py)

  slicerMacroBuildScriptedModule(
    NAME AstroSliceViewsTest
    SCRIPTS AstroSliceViewsTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroSliceViewsTest.py)

  slicerMacroBuildScriptedModule(
    NAME AstroVolumeRenderingTest
    SCRIPTS AstroVolumeRenderingTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroVolumeRenderingTest.py)

  slicerMacroBuildScriptedModule(
    NAME AstroSegmentationMaskTest
    SCRIPTS AstroSegmentationMaskTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroSegmentationMaskTest.py)

  slicerMacroBuildScriptedModule(
    NAME AstroCloudLassoTest
    SCRIPTS AstroCloudLassoTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroCloudLassoTest.py)

  slicerMacroBuildScriptedModule(
    NAME AstroFITSIOTest
    SCRIPTS AstroFITSIOTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroFITSIOTest.py)

  # real data (TestingData-v1, SampleData-v2) against the values of astropy
  file(GLOB _real_data_values RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} RealData/*.json)
  slicerMacroBuildScriptedModule(
    NAME AstroFITSRealDataTest
    SCRIPTS AstroFITSRealDataTest.py
    RESOURCES ${_real_data_values}
    )
  slicer_add_python_unittest(SCRIPT AstroFITSRealDataTest.py)

  slicerMacroBuildScriptedModule(
    NAME AstroVolumeSegmentationsTest
    SCRIPTS AstroVolumeSegmentationsTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroVolumeSegmentationsTest.py)
endif()
