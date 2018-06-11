if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroPVDiagramSelfTest
    SCRIPTS AstroPVDiagramSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroPVDiagramSelfTest.py)
endif()
