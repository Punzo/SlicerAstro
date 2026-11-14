# (the tests are scripted module tests: QtTesting is not needed)
if(Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroPVSliceSelfTest
    SCRIPTS AstroPVSliceSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroPVSliceSelfTest.py)
endif()
