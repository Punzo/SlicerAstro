set(proj python-slicerastro-requirements)

# Alternative python prefix for installing extension python packages
set(python_packages_DIR "${CMAKE_BINARY_DIR}/python-packages-install")
file(TO_NATIVE_PATH ${python_packages_DIR} python_packages_DIR_NATIVE_DIR)

ExternalProject_Add(${proj}
  ${${proj}_EP_ARGS}
  DOWNLOAD_COMMAND ""
  SOURCE_DIR ${python_packages_DIR}/${proj}
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ${PYTHON_EXECUTABLE} -m pip install -r ${SlicerAstro_SOURCE_DIR}/SuperBuild/slicerastro-requirements.txt --prefix ${python_packages_DIR_NATIVE_DIR}
  LOG_INSTALL 1
  DEPENDS
  )

ExternalProject_GenerateProjectDescription_Step(${proj}
  VERSION ${_version}
  )

# Launcher setting specific to build tree
set(${proj}_PYTHONPATH_LAUNCHER_BUILD
  ${python_packages_DIR}/${PYTHON_STDLIB_SUBDIR}
  ${python_packages_DIR}/${PYTHON_STDLIB_SUBDIR}/lib-dynload
  ${python_packages_DIR}/${PYTHON_SITE_PACKAGES_SUBDIR}
  )
mark_as_superbuild(
  VARS ${proj}_PYTHONPATH_LAUNCHER_BUILD
  LABELS "PYTHONPATH_LAUNCHER_BUILD"
  )

mark_as_superbuild(python_packages_DIR:PATH)

