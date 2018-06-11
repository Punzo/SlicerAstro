if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroProfilesSelfTest
    SCRIPTS AstroProfilesSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroProfilesSelfTest.py)
endif()
