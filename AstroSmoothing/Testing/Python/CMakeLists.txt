if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroSmoothingSelfTest
    SCRIPTS AstroSmoothingSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroSmoothingSelfTest.py)
endif()
