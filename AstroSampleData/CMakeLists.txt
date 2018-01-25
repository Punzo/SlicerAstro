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
  Resources/Icons/NGC2403.png
  Resources/Icons/NGC4111.png
  Resources/Icons/NGC3379.png
  )

#-----------------------------------------------------------------------------
slicerMacroBuildScriptedModule(
  NAME ${MODULE_NAME}
  SCRIPTS ${MODULE_PYTHON_SCRIPTS}
  RESOURCES ${MODULE_PYTHON_RESOURCES}
  WITH_GENERIC_TESTS
  )
