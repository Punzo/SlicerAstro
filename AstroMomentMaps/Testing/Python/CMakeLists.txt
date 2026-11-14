# (the tests are scripted module tests: QtTesting is not needed)
if(Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroMomentMapsSelfTest
    SCRIPTS AstroMomentMapsSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroMomentMapsSelfTest.py)
endif()
