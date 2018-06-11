if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroMomentMapsSelfTest.py
    SCRIPTS AstroMomentMapsSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroMomentMapsSelfTest.py)
endif()
