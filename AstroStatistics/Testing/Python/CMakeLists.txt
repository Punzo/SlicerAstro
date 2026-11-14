# (the tests are scripted module tests: QtTesting is not needed)
if(Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroStatisticsSelfTest
    SCRIPTS AstroStatisticsSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroStatisticsSelfTest.py)
endif()
