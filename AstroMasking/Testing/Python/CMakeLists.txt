if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroMaskingSelfTest
    SCRIPTS AstroMaskingSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroMaskingSelfTest.py)
endif()
