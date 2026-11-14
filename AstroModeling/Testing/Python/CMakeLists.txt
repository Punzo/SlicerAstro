# (the tests are scripted module tests: QtTesting is not needed)
if(Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroModelingSelfTest
    SCRIPTS AstroModelingSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroModelingSelfTest.py)
endif()
