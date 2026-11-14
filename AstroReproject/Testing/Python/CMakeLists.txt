# (the tests are scripted module tests: QtTesting is not needed)
if(Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroReprojectSelfTest
    SCRIPTS AstroReprojectSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroReprojectSelfTest.py)

  slicerMacroBuildScriptedModule(
    NAME AstroReprojectTest
    SCRIPTS AstroReprojectTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroReprojectTest.py)

  slicerMacroBuildScriptedModule(
    NAME AstroFramesTest
    SCRIPTS AstroFramesTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroFramesTest.py)
endif()
