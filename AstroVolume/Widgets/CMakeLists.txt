project(qSlicer${MODULE_NAME}ModuleWidgets)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "Q_SLICERASTRO_QTMODULES_${MODULE_NAME_UPPER}_WIDGETS_EXPORT")

# the Segment Editor effects (options of the effects of the Masks section)
list(GET qSlicerSegmentationsModuleWidgets_INCLUDE_DIRS 0 _astro_editor_effects_dir)
list(GET qSlicerSegmentationsModuleWidgets_INCLUDE_DIRS 1 _astro_editor_effects_binary_dir)

set(${KIT}_INCLUDE_DIRECTORIES
    ${_astro_editor_effects_dir}/../EditorEffects
    ${_astro_editor_effects_binary_dir}/../EditorEffects
    ${CMAKE_CURRENT_SOURCE_DIR}/../MRML
    ${CMAKE_CURRENT_BINARY_DIR}/../MRML
    ${CMAKE_CURRENT_SOURCE_DIR}/../Logic
    ${CMAKE_CURRENT_BINARY_DIR}/../Logic
    ${MRMLCore_INCLUDE_DIRS}
    ${WCSLIB_INCLUDE_DIR}
    ${CFITSIO_INCLUDE_DIR}
    ${vtkFits_INCLUDE_DIRS}
    ${qSlicerSegmentationsModuleWidgets_INCLUDE_DIRS}
    ${vtkSlicerSegmentationsModuleLogic_INCLUDES_DIRS}
    ${vtkSlicerSegmentationsModuleMRML_INCLUDES_DIRS}
    ${qSlicerVolumeRenderingModuleWidgets_INCLUDE_DIRS}
    ${vtkSlicerVolumeRenderingModuleMRML_INCLUDE_DIRS}
  )

set(${KIT}_SRCS
  qSlicer${MODULE_NAME}IOOptionsWidget.cxx
  qSlicer${MODULE_NAME}IOOptionsWidget.h
  qSlicer${MODULE_NAME}ModuleWidget.cxx
  qSlicer${MODULE_NAME}ModuleWidget.h
  qSlicer${MODULE_NAME}ModuleWidget_p.h
  qSlicer${MODULE_NAME}ModuleWidgetViews.cxx
  qMRMLSliceAstroControllerWidget.cxx
  qMRMLSliceAstroControllerWidget.h
  qMRMLSliceAstroControllerWidget_p.h
  qMRMLSliceAstroWidget.cxx
  qMRMLSliceAstroWidget.h
  qMRMLSliceAstroWidget_p.h
  qSlicerAstroAxisScrubber.cxx
  qSlicerAstroAxisScrubber.h
  qSlicerAstroDatasetCard.cxx
  qSlicerAstroDisplaySection.cxx
  qSlicerAstroContoursSection.cxx
  qSlicerAstroMasksSection.cxx
  qSlicerAstroMasksSection.h
  qSlicerAstroColorPicker.cxx
  qSlicerAstroColorPicker.h
  qSlicerAstroSegmentDialog.cxx
  qSlicerAstroSegmentDialog.h
  qSlicerAstroSegmentsTableStyler.cxx
  qSlicerAstroSegmentsTableStyler.h
  qSlicerAstroSubCubeDialog.cxx
  qSlicerAstroSubCubeDialog.h
  qSlicerAstroRenderingSection.cxx
  qSlicerAstroRenderingSection.h
  qSlicerAstroInfoSection.cxx
  qSlicerAstroInfoSection.h
  qSlicerAstroSourcesSection.cxx
  qSlicerAstroSourcesSection.h
  qSlicerAstroWelcomeWidget.cxx
  qSlicerAstroWelcomeWidget.h
  qSlicerAstroSettingsDialog.cxx
  qSlicerAstroSettingsDialog.h
  qSlicerAstroContoursSection.h
  qSlicerAstroDisplaySection.h
  qSlicerAstroDatasetCard.h
  qSlicerAstroDatasetSelector.cxx
  qSlicerAstroDatasetSelector.h
  qSlicerAstroFoldSection.cxx
  qSlicerAstroFoldSection.h
  qSlicerAstroHistogramWidget.cxx
  qSlicerAstroHistogramWidget.h
  qSlicerAstroMapPreview.cxx
  qSlicerAstroMapPreview.h
  qSlicerAstroMessageWidget.cxx
  qSlicerAstroMessageWidget.h
  qSlicerAstroPanelHeader.cxx
  qSlicerAstroPanelHeader.h
  qSlicerAstroBackgroundRunner.cxx
  qSlicerAstroBackgroundRunner.h
  qSlicerAstroResultCard.cxx
  qSlicerAstroResultCard.h
  qSlicerAstroRunButton.cxx
  qSlicerAstroRunButton.h
  qSlicerAstroScrollAreaFitter.cxx
  qSlicerAstroScrollAreaFitter.h
  qSlicerAstroSegmentedButton.cxx
  qSlicerAstroSegmentedButton.h
  qSlicerAstroSpectrumPlot.cxx
  qSlicerAstroSpectrumPlot.h
  qSlicerAstroSpectrumRangeWidget.cxx
  qSlicerAstroSpectrumRangeWidget.h
  qSlicerAstroTheme.cxx
  qSlicerAstroTheme.h
  qSlicerAstroThumbnails.cxx
  qSlicerAstroThumbnails.h
  qSlicerAstroToggleCard.cxx
  qSlicerAstroToggleCard.h
  qSlicerAstroValueField.cxx
  qSlicerAstroValueField.h
  qSlicerAstroViewFrame.cxx
  qSlicerAstroViewFrame.h
  )

set(${KIT}_MOC_SRCS
  qSlicer${MODULE_NAME}IOOptionsWidget.h
  qSlicer${MODULE_NAME}ModuleWidget.h
  qMRMLSliceAstroControllerWidget.h
  qMRMLSliceAstroControllerWidget_p.h
  qMRMLSliceAstroWidget.h
  qMRMLSliceAstroWidget_p.h
  qSlicerAstroAxisScrubber.h
  qSlicerAstroDatasetCard.h
  qSlicerAstroDatasetSelector.h
  qSlicerAstroDisplaySection.h
  qSlicerAstroContoursSection.h
  qSlicerAstroMasksSection.h
  qSlicerAstroColorPicker.h
  qSlicerAstroSegmentDialog.h
  qSlicerAstroSegmentsTableStyler.h
  qSlicerAstroSubCubeDialog.h
  qSlicerAstroRenderingSection.h
  qSlicerAstroInfoSection.h
  qSlicerAstroSourcesSection.h
  qSlicerAstroWelcomeWidget.h
  qSlicerAstroSettingsDialog.h
  qSlicerAstroFoldSection.h
  qSlicerAstroHistogramWidget.h
  qSlicerAstroMapPreview.h
  qSlicerAstroMessageWidget.h
  qSlicerAstroPanelHeader.h
  qSlicerAstroBackgroundRunner.h
  qSlicerAstroResultCard.h
  qSlicerAstroRunButton.h
  qSlicerAstroScrollAreaFitter.h
  qSlicerAstroSegmentedButton.h
  qSlicerAstroSpectrumPlot.h
  qSlicerAstroSpectrumRangeWidget.h
  qSlicerAstroTheme.h
  qSlicerAstroThumbnails.h
  qSlicerAstroToggleCard.h
  qSlicerAstroValueField.h
  qSlicerAstroViewFrame.h
  )

set(${KIT}_UI_SRCS
  Resources/UI/qSlicer${MODULE_NAME}IOOptionsWidget.ui
  )

set(${KIT}_RESOURCES
  Resources/qSlicer${MODULE_NAME}WidgetsModule.qrc
  Resources/qSlicerAstroTheme.qrc
  )

# Qt Svg: icons of the theme (tinted SVG files)
if(Slicer_REQUIRED_QT_VERSION VERSION_GREATER_EQUAL "6")
  set(_astro_qt_svg Qt6::Svg)
else()
  set(_astro_qt_svg Qt5::Svg)
endif()

set(${KIT}_TARGET_LIBRARIES
   ${_astro_qt_svg}
   MRMLCore
   qSlicerSegmentationsModuleWidgets
   qSlicerSegmentationsEditorEffects
   qSlicerTerminologiesModuleWidgets
   qSlicerVolumeRenderingModuleWidgets
   vtkSlicer${MODULE_NAME}ModuleLogic
   vtkSlicer${MODULE_NAME}ModuleMRML
   vtkSlicerSegmentationsModuleLogic
   vtkSlicerSegmentationsModuleMRML
   vtkSlicerVolumeRenderingModuleMRML
   vtkSlicerVolumeRenderingModuleLogic 
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
