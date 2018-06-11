if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroStatisticsSelfTest
    SCRIPTS AstroStatisticsSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroStatisticsSelfTest.py)
endif()
