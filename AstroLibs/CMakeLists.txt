project(SlicerAstroLibs)

# --------------------------------------------------------------------------
#                      ----] W A R N I N G [----
# --------------------------------------------------------------------------
# Order of project directory matters. Project should be topologically ordered
# --------------------------------------------------------------------------

set(dirs vtkFits
         )


#-----------------------------------------------------------------------------
# Set library properties
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Loop over list of directories
#-----------------------------------------------------------------------------

foreach(dir ${dirs})
  message(STATUS "Configuring library: ${dir}")
  string(REPLACE "/" "" dir_noslash ${dir})
  set(${dir_noslash}_INSTALL_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/../lib/Slicer-${Slicer_VERSION_MAJOR}.${Slicer_VERSION_MINOR})
  set(${dir_noslash}_INSTALL_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/../lib/Slicer-${Slicer_VERSION_MAJOR}.${Slicer_VERSION_MINOR})
  set(${dir_noslash}_INSTALL_SHARE_DIR ${CMAKE_CURRENT_BINARY_DIR}/../share/Slicer-${Slicer_VERSION_MAJOR}.${Slicer_VERSION_MINOR}/${dir_noslash})
  set(${dir_noslash}_INSTALL_NO_DEVELOPMENT ${Slicer_INSTALL_NO_DEVELOPMENT})
  set(${dir_noslash}_EXPORT_FILE ${CMAKE_CURRENT_BINARY_DIR}/../SlicerAstroTargets.cmake)
  add_subdirectory(${dir})
endforeach()

#-----------------------------------------------------------------------------
# Generate list of include directories
#-----------------------------------------------------------------------------
set(include_dirs)
foreach(dir ${dirs})
  string(REPLACE "/" "" dir_noslash ${dir})
  list(APPEND include_dirs ${${dir_noslash}_INCLUDE_DIRS})
endforeach()

# Set variable Slicer_AstroLibs_INCLUDE_DIRS
set(Slicer_AstroLibs_INCLUDE_DIRS ${include_dirs} CACHE INTERNAL "Slicer AstroLibs includes" FORCE)

#-----------------------------------------------------------------------------
# Set variable Slicer_AstroLibs_VTK_WRAPPED_LIBRARIES
#-----------------------------------------------------------------------------
set(_vtk_wrapped_libraries
  vtkFits
  )

set(Slicer_AstroLibs_VTK_WRAPPED_LIBRARIES
  ${_vtk_wrapped_libraries} CACHE INTERNAL "Slicer AstroLibs VTK wrapped" FORCE)
