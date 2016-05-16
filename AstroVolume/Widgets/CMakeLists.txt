project(qSlicer${MODULE_NAME}ModuleWidgets)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "Q_SLICER_QTMODULES_${MODULE_NAME_UPPER}_WIDGETS_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/../MRML
    ${CMAKE_CURRENT_BINARY_DIR}/../MRML
    ${CMAKE_CURRENT_SOURCE_DIR}/../Logic
    ${CMAKE_CURRENT_BINARY_DIR}/../Logic
    ${MRMLCore_INCLUDE_DIRS}
    ${WCSLIB_INCLUDE_DIR}
    ${qSlicerVolumeRenderingModuleWidgets_INCLUDE_DIRS}
    ${vtkSlicerVolumeRenderingModuleMRML_INCLUDES_DIRS}
  )

set(${KIT}_SRCS
  qSlicer${MODULE_NAME}IOOptionsWidget.cxx
  qSlicer${MODULE_NAME}IOOptionsWidget.h
  qSlicer${MODULE_NAME}ModuleWidget.cxx
  qSlicer${MODULE_NAME}ModuleWidget.h
  qSlicerLabelMapVolumeDisplayWidget.cxx
  qSlicerLabelMapVolumeDisplayWidget.h
  qSlicerScalarVolumeDisplayWidget.cxx
  qSlicerScalarVolumeDisplayWidget.h
  qSlicerAstroVolumeDisplayWidget.cxx
  qSlicerAstroVolumeDisplayWidget.h
  qMRMLAstroVolumeInfoWidget.cxx
  qMRMLAstroVolumeInfoWidget.h
  #qMRMLSliceAstroControllerWidget.cxx
  #qMRMLSliceAstroControllerWidget.h
  #qMRMLSliceAstroControllerWidget_p.h
  #qMRMLSliceAstroWidget.cxx
  #qMRMLSliceAstroWidget.h
  #qMRMLSliceAstroWidget_p.h
  )

set(${KIT}_MOC_SRCS
  qSlicer${MODULE_NAME}IOOptionsWidget.h
  qSlicer${MODULE_NAME}ModuleWidget.h
  qSlicerLabelMapVolumeDisplayWidget.h
  qSlicerScalarVolumeDisplayWidget.h
  qSlicerAstroVolumeDisplayWidget.h
  qMRMLAstroVolumeInfoWidget.h
  #qMRMLSliceAstroControllerWidget.h
  #qMRMLSliceAstroControllerWidget_p.h
  #qMRMLSliceAstroWidget.h
  #qMRMLSliceAstroWidget_p.h
  )

set(${KIT}_UI_SRCS
  Resources/UI/qSlicer${MODULE_NAME}IOOptionsWidget.ui
  Resources/UI/qSlicer${MODULE_NAME}ModuleWidget.ui
  Resources/UI/qSlicerLabelMapVolumeDisplayWidget.ui
  Resources/UI/qSlicerScalarVolumeDisplayWidget.ui
  Resources/UI/qMRMLSliceAstroControllerWidget.ui
  )

set(${KIT}_RESOURCES
  Resources/qSlicer${MODULE_NAME}WidgetsModule.qrc
  )

set(${KIT}_TARGET_LIBRARIES
   MRMLCore
   vtkSlicer${MODULE_NAME}ModuleLogic
   vtkSlicerAstroVolumeModuleMRML
   qSlicerVolumeRenderingModuleWidgets
  )

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleWidgets(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  MOC_SRCS ${${KIT}_MOC_SRCS}
  UI_SRCS ${${KIT}_UI_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  RESOURCES ${${KIT}_RESOURCES}
  WRAP_PYTHONQT
  )

#-----------------------------------------------------------------------------
# create the share forlder and copy presets.xml

set(directory ${CMAKE_CURRENT_BINARY_DIR}/../../share/Slicer-${Slicer_VERSION_MAJOR}.${Slicer_VERSION_MINOR}/qt-loadable-modules/${MODULE_NAME})
add_custom_target(build-time-make-share-directory ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${directory})

set(sourcePresets ${CMAKE_CURRENT_SOURCE_DIR}/Resources/presets.xml)
add_custom_target(build-time-copy-presets ALL
    COMMAND ${CMAKE_COMMAND} -E copy ${sourcePresets} ${directory}/presets.xml)

#-----------------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()
