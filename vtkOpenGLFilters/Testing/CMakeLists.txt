add_executable(vtkAstroOpenGLFiltersTest vtkAstroOpenGLFiltersTest.cxx)
target_link_libraries(vtkAstroOpenGLFiltersTest vtkOpenGLFilters vtkAstroImageFilters)
target_include_directories(vtkAstroOpenGLFiltersTest PRIVATE ${vtkOpenGLFilters_INCLUDE_DIRS})
set_target_properties(vtkAstroOpenGLFiltersTest PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${Slicer_BIN_DIR}")
# Requires an OpenGL context (display or xvfb-run)
add_test(NAME vtkAstroOpenGLFiltersTest COMMAND $<TARGET_FILE:vtkAstroOpenGLFiltersTest>)
set_tests_properties(vtkAstroOpenGLFiltersTest PROPERTIES LABELS "GPU")
