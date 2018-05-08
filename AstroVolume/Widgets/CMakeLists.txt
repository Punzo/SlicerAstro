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
    ${qSlicerSegmentationsModuleWidgets_INCLUDE_DIRS}
    ${vtkSlicerSegmentationsModuleLogic_INCLUDES_DIRS}
    ${vtkSlicerSegmentationsModuleMRML_INCLUDES_DIRS}
    ${qSlicerVolumeRenderingModuleWidgets_INCLUDE_DIRS}
    ${vtkSlicerVolumeRenderingModuleMRML_INCLUDES_DIRS}
    ${vtkSlicerAstroReprojectModuleLogic_INCLUDE_DIRS}
  )

set(${KIT}_SRCS
  qSlicer${MODULE_NAME}IOOptionsWidget.cxx
  qSlicer${MODULE_NAME}IOOptionsWidget.h
  qSlicer${MODULE_NAME}ModuleWidget.cxx
  qSlicer${MODULE_NAME}ModuleWidget.h
  qSlicerAstroLabelMapVolumeDisplayWidget.cxx
  qSlicerAstroLabelMapVolumeDisplayWidget.h
  qSlicerAstroScalarVolumeDisplayWidget.cxx
  qSlicerAstroScalarVolumeDisplayWidget.h
  qSlicerAstroVolumeDisplayWidget.cxx
  qSlicerAstroVolumeDisplayWidget.h
  qMRMLAstroVolumeInfoWidget.cxx
  qMRMLAstroVolumeInfoWidget.h
  qMRMLSliceAstroControllerWidget.cxx
  qMRMLSliceAstroControllerWidget.h
  qMRMLSliceAstroControllerWidget_p.h
  qMRMLSliceAstroWidget.cxx
  qMRMLSliceAstroWidget.h
  qMRMLSliceAstroWidget_p.h
  )

set(${KIT}_MOC_SRCS
  qSlicer${MODULE_NAME}IOOptionsWidget.h
  qSlicer${MODULE_NAME}ModuleWidget.h
  qSlicerAstroLabelMapVolumeDisplayWidget.h
  qSlicerAstroScalarVolumeDisplayWidget.h
  qSlicerAstroVolumeDisplayWidget.h
  qMRMLAstroVolumeInfoWidget.h
  qMRMLSliceAstroControllerWidget.h
  qMRMLSliceAstroControllerWidget_p.h
  qMRMLSliceAstroWidget.h
  qMRMLSliceAstroWidget_p.h
  )

set(${KIT}_UI_SRCS
  Resources/UI/qSlicer${MODULE_NAME}IOOptionsWidget.ui
  Resources/UI/qSlicer${MODULE_NAME}ModuleWidget.ui
  Resources/UI/qSlicerAstroLabelMapVolumeDisplayWidget.ui
  Resources/UI/qSlicerAstroScalarVolumeDisplayWidget.ui
  )

set(${KIT}_RESOURCES
  Resources/qSlicer${MODULE_NAME}WidgetsModule.qrc
  )

set(${KIT}_TARGET_LIBRARIES
   MRMLCore
   qSlicerSegmentationsModuleWidgets
   qSlicerVolumeRenderingModuleWidgets
   vtkSlicer${MODULE_NAME}ModuleLogic
   vtkSlicer${MODULE_NAME}ModuleMRML
   vtkSlicerSegmentationsModuleLogic
   vtkSlicerSegmentationsModuleMRML
   vtkSlicerVolumeRenderingModuleMRML
   vtkSlicerVolumeRenderingModuleLogic 
   vtkSlicerAstroReprojectModuleLogic
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

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/presets.xml
  ${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_SHARE_DIR}/${MODULE_NAME}/presets.xml
  COPYONLY)

install(
  FILES ${CMAKE_BINARY_DIR}/${Slicer_QTLOADABLEMODULES_SHARE_DIR}/${MODULE_NAME}/presets.xml
  DESTINATION ${Slicer_INSTALL_QTLOADABLEMODULES_SHARE_DIR}/${MODULE_NAME}
  COMPONENT Runtime)

#-----------------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()
