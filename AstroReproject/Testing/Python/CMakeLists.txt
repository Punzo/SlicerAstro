if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroReprojectSelfTest
    SCRIPTS AstroReprojectSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroReprojectSelfTest.py)
endif()
