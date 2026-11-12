if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
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
endif()
