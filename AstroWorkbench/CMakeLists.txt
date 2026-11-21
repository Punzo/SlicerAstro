#-----------------------------------------------------------------------------
set(MODULE_NAME AstroWorkbench)

#-----------------------------------------------------------------------------
set(MODULE_PYTHON_SCRIPTS
  ${MODULE_NAME}.py
  ${MODULE_NAME}Lib/__init__.py
  ${MODULE_NAME}Lib/About.py
  ${MODULE_NAME}Lib/Astro.py
  ${MODULE_NAME}Lib/Contours.py
  ${MODULE_NAME}Lib/Inspector.py
  ${MODULE_NAME}Lib/Sections.py
  ${MODULE_NAME}Lib/Views.py
  ${MODULE_NAME}Lib/Workbench.py
  ${MODULE_NAME}Lib/Workspaces.py
  )

set(MODULE_PYTHON_RESOURCES
  Resources/Icons/${MODULE_NAME}.png
  )

#-----------------------------------------------------------------------------
slicerMacroBuildScriptedModule(
  NAME ${MODULE_NAME}
  SCRIPTS ${MODULE_PYTHON_SCRIPTS}
  RESOURCES ${MODULE_PYTHON_RESOURCES}
  )

#-----------------------------------------------------------------------------
if(BUILD_TESTING)
  # one test per method (they run in parallel)
  slicerastro_add_python_unittest_methods(SCRIPT ${MODULE_NAME}.py CLASS AstroWorkbenchTest)
endif()
