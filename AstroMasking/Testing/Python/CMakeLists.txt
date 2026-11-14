# (the tests are scripted module tests: QtTesting is not needed)
if(Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroMaskingSelfTest
    SCRIPTS AstroMaskingSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroMaskingSelfTest.py)
endif()
