add_executable(vtkAstroImageFiltersTest vtkAstroImageFiltersTest.cxx)
target_link_libraries(vtkAstroImageFiltersTest vtkAstroImageFilters)
set_target_properties(vtkAstroImageFiltersTest PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${Slicer_BIN_DIR}")
add_test(NAME vtkAstroImageFiltersTest COMMAND $<TARGET_FILE:vtkAstroImageFiltersTest>)
