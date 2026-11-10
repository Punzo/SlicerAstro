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
endif()
