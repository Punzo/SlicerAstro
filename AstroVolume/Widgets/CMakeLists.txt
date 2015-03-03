project(qSlicer${MODULE_NAME}ModuleWidgets)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "Q_SLICER_QTMODULES_${MODULE_NAME_UPPER}_WIDGETS_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
    ${MRMLCore_INCLUDE_DIRS}
  )

set(${KIT}_SRCS
  qSlicerLabelMapVolumeDisplayWidget.cxx
  qSlicerLabelMapVolumeDisplayWidget.h
  qSlicerScalarVolumeDisplayWidget.cxx
  qSlicerScalarVolumeDisplayWidget.h
  qSlicerAstroVolumeDisplayWidget.cxx
  qSlicerAstroVolumeDisplayWidget.h
  )

set(${KIT}_MOC_SRCS
  qSlicerLabelMapVolumeDisplayWidget.h
  qSlicerScalarVolumeDisplayWidget.h
  qSlicerAstroVolumeDisplayWidget.h
  )

set(${KIT}_UI_SRCS
  Resources/UI/qSlicerLabelMapVolumeDisplayWidget.ui
  Resources/UI/qSlicerScalarVolumeDisplayWidget.ui
  )

set(${KIT}_RESOURCES
  Resources/qSlicer${MODULE_NAME}WidgetsModule.qrc
  )

set(${KIT}_TARGET_LIBRARIES
   MRMLCore
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
