project(qSlicer${MODULE_NAME}ModuleWidgets)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "Q_SLICER_QTMODULES_${MODULE_NAME_UPPER}_WIDGETS_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/../MRML
    ${CMAKE_CURRENT_BINARY_DIR}/../MRML
    ${MRMLCore_INCLUDE_DIRS}
    ${WCSLIB_INCLUDE_DIR}
    ${vtkSlicerVolumeRenderingModuleMRML_INCLUDES_DIRS}
  )

set(${KIT}_SRCS
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
  Resources/UI/qSlicerLabelMapVolumeDisplayWidget.ui
  Resources/UI/qSlicerScalarVolumeDisplayWidget.ui
  #Resources/UI/qMRMLSliceAstroControllerWidget.ui
  )

set(${KIT}_RESOURCES
  Resources/qSlicer${MODULE_NAME}WidgetsModule.qrc
  )

set(${KIT}_TARGET_LIBRARIES
   MRMLCore
   vtkSlicerAstroVolumeModuleMRML
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
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()
