# (the tests are scripted module tests: QtTesting is not needed)
if(Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroModelingSelfTest
    SCRIPTS AstroModelingSelfTest.py
    )
  # one test per method (they run in parallel)
  slicerastro_add_python_unittest_methods(SCRIPT AstroModelingSelfTest.py CLASS AstroModelingSelfTestTest)
endif()
