if(Slicer_USE_QtTesting AND Slicer_USE_PYTHONQT)
  slicerMacroBuildScriptedModule(
    NAME AstroPVSliceSelfTest
    SCRIPTS AstroPVSliceSelfTest.py
    )
  slicer_add_python_unittest(SCRIPT AstroPVSliceSelfTest.py)
endif()
