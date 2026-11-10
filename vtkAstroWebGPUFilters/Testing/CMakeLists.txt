add_executable(vtkAstroWebGPUFiltersTest vtkAstroWebGPUFiltersTest.cxx)
target_link_libraries(vtkAstroWebGPUFiltersTest vtkAstroWebGPUFilters)
set_target_properties(vtkAstroWebGPUFiltersTest PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${Slicer_BIN_DIR}")
# Skipped (return code 77) when no WebGPU adapter is available
add_test(NAME vtkAstroWebGPUFiltersTest COMMAND $<TARGET_FILE:vtkAstroWebGPUFiltersTest>)
set_tests_properties(vtkAstroWebGPUFiltersTest PROPERTIES LABELS "GPU" SKIP_RETURN_CODE 77)
