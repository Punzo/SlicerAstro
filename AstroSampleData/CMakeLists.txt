#-----------------------------------------------------------------------------
set(MODULE_NAME AstroSampleData)

#-----------------------------------------------------------------------------
set(MODULE_PYTHON_SCRIPTS
  ${MODULE_NAME}.py
  )

set(MODULE_PYTHON_RESOURCES
  Resources/Icons/NGC2841.png
  Resources/Icons/WEIN069.png
  Resources/Icons/WEIN069_mask.png
  Resources/Icons/NGC2403_DSS.png
  Resources/Icons/NGC2403.png
  Resources/Icons/NGC891.png
  Resources/Icons/NGC3198.png
  Resources/Icons/NGC4631.png
  Resources/Icons/Antennae_HI.png
  Resources/Icons/Antennae_CO.png
  Resources/Icons/Antennae_MUSE.png
  Resources/Icons/Antennae_R.png
  )

#-----------------------------------------------------------------------------
slicerMacroBuildScriptedModule(
  NAME ${MODULE_NAME}
  SCRIPTS ${MODULE_PYTHON_SCRIPTS}
  RESOURCES ${MODULE_PYTHON_RESOURCES}
  )

#-----------------------------------------------------------------------------
if(BUILD_TESTING)

  # Register the unittest subclass in the main script as a ctest.
  # Note that the test will also be available at runtime.
  slicer_add_python_unittest(SCRIPT ${MODULE_NAME}.py)

endif()
