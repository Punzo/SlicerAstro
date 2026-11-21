#-----------------------------------------------------------------------------
# slicerastro_add_python_unittest_methods(SCRIPT <file.py> CLASS <TestCase class>
#                                         [METHODS <test_a> <test_b> ...])
#
# One CTest test per test method of a Python unit test (py_<Script>_<method>),
# instead of one test running all of them (slicer_add_python_unittest): the
# methods of a long test run in parallel, each in its own Slicer process (and
# display). unittest calls setUp before each method. Without METHODS: all
# the methods "def test_..." of the script.
function(slicerastro_add_python_unittest_methods)
  set(oneValueArgs SCRIPT CLASS)
  set(multiValueArgs METHODS SLICER_ARGS)
  cmake_parse_arguments(MY "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  get_filename_component(_module ${MY_SCRIPT} NAME_WE)
  if(NOT MY_METHODS)
    file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/${MY_SCRIPT} _definitions REGEX "^    def test_[A-Za-z0-9_]+\\(self\\)")
    foreach(_definition IN LISTS _definitions)
      string(REGEX REPLACE "^    def (test_[A-Za-z0-9_]+)\\(self\\).*" "\\1" _method "${_definition}")
      list(APPEND MY_METHODS ${_method})
    endforeach()
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${MY_SCRIPT})
  _slicer_python_test_append_module_paths(MY_SLICER_ARGS)
  foreach(_method IN LISTS MY_METHODS)
    ExternalData_add_test(${Slicer_ExternalData_DATA_MANAGEMENT_TARGET}
      NAME py_${_module}_${_method}
      COMMAND ${Slicer_LAUNCHER_EXECUTABLE}
      --no-splash
      --testing
      ${Slicer_ADDITIONAL_LAUNCHER_SETTINGS}
      ${MY_SLICER_ARGS}
      --python-code "import slicer.testing\; slicer.testing.runUnitTest(['${CMAKE_CURRENT_BINARY_DIR}', '${CMAKE_CURRENT_SOURCE_DIR}'], '${_module}.${MY_CLASS}.${_method}')"
      )
  endforeach()
endfunction()
