set(proj python-slicerastro-requirements)

# Set dependency list
set(${proj}_DEPENDENCIES)

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj DEPENDS_VAR ${proj}_DEPENDENCIES)

ExternalProject_Add(${proj}
  ${${proj}_EP_ARGS}
  DOWNLOAD_COMMAND ""
  SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj}
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ${PYTHON_EXECUTABLE} -m pip install -r ${SlicerAstro_SOURCE_DIR}/SuperBuild/slicerastro-requirements.txt
  LOG_INSTALL 1
  DEPENDS
    ${${proj}_DEPENDENCIES}
  )

ExternalProject_GenerateProjectDescription_Step(${proj}
  VERSION ${_version}
  )

