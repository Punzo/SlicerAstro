#-----------------------------------------------------------------------------
set(MODULE_NAME AstroVolume)
set(MODULE_TITLE "Astro Volume")

string(TOUPPER ${MODULE_NAME} MODULE_NAME_UPPER)

#-----------------------------------------------------------------------------
add_subdirectory(Logic)
add_subdirectory(Widgets)
# All the MRML nodes are in AstroVolume modoule.
# AstroVolume module is required by all the others module.
# another solution can be to shift all the MRML to an AstroLibs folder
add_subdirectory(MRML)
add_subdirectory(MRMLDM)
add_subdirectory(EditorEffects)

#-----------------------------------------------------------------------------
set(MODULE_EXPORT_DIRECTIVE "Q_SLICERASTRO_QTMODULES_${MODULE_NAME_UPPER}_EXPORT")

# Additional includes - Current_{source,binary} and Slicer_{Libs,Base} already included
list(GET qSlicerSegmentationsModuleWidgets_INCLUDE_DIRS 0 qSlicerSegmentationsModuleEditorEffects_INCLUDE_DIR)
set(qSlicerSegmentationsModuleEditorEffects_INCLUDE_DIR ${qSlicerSegmentationsModuleEditorEffects_INCLUDE_DIR}/../EditorEffects)
list(GET qSlicerSegmentationsModuleWidgets_INCLUDE_DIRS 1 qSlicerSegmentationsModuleEditorEffects_INCLUDE_BINARY_DIR)
set(qSlicerSegmentationsModuleEditorEffects_INCLUDE_BINARY_DIR ${qSlicerSegmentationsModuleEditorEffects_INCLUDE_BINARY_DIR}/../EditorEffects)

set(MODULE_INCLUDE_DIRECTORIES
  ${CMAKE_CURRENT_SOURCE_DIR}/Logic
  ${CMAKE_CURRENT_BINARY_DIR}/Logic
  ${CMAKE_CURRENT_SOURCE_DIR}/Widgets
  ${CMAKE_CURRENT_BINARY_DIR}/Widgets
  ${CMAKE_CURRENT_SOURCE_DIR}/MRML
  ${CMAKE_CURRENT_BINARY_DIR}/MRML
  ${CMAKE_CURRENT_SOURCE_DIR}/MRMLDM
  ${CMAKE_CURRENT_BINARY_DIR}/MRMLDM
  ${CMAKE_CURRENT_SOURCE_DIR}/EditorEffects
  ${CMAKE_CURRENT_BINARY_DIR}/EditorEffects
  ${qSlicerSegmentationsModuleEditorEffects_INCLUDE_DIR}
  ${qSlicerSegmentationsModuleEditorEffects_INCLUDE_BINARY_DIR}
  ${qSlicerVolumeRenderingModuleWidgets_INCLUDE_DIRS}
  ${vtkSlicerVolumeRenderingModuleMRML_INCLUDES_DIRS}
  ${WCSLIB_INCLUDE_DIR}
  )

set(MODULE_SRCS
  qSlicer${MODULE_NAME}Module.cxx
  qSlicer${MODULE_NAME}Module.h
  qSlicer${MODULE_NAME}Reader.cxx
  qSlicer${MODULE_NAME}Reader.h
  qSlicer${MODULE_NAME}LayoutSliceViewFactory.h
  qSlicer${MODULE_NAME}LayoutSliceViewFactory.cxx
  )

set(MODULE_MOC_SRCS
  qSlicer${MODULE_NAME}Module.h
  qSlicer${MODULE_NAME}Reader.h
  qSlicer${MODULE_NAME}LayoutSliceViewFactory.h
  )

set(MODULE_UI_SRCS
  )

set(MODULE_TARGET_LIBRARIES
  qSlicer${MODULE_NAME}ModuleWidgets
  qSlicer${MODULE_NAME}EditorEffects
  qSlicerVolumeRenderingModuleWidgets
  vtkSlicer${MODULE_NAME}ModuleLogic
  vtkSlicer${MODULE_NAME}ModuleMRML
  vtkSlicer${MODULE_NAME}ModuleMRMLDisplayableManager
  vtkSlicerAnnotationsModuleMRML
  )

set(MODULE_RESOURCES
  Resources/qSlicer${MODULE_NAME}Module.qrc
  )

#-----------------------------------------------------------------------------
slicerMacroBuildLoadableModule(
  NAME ${MODULE_NAME}
  TITLE ${MODULE_TITLE}
  EXPORT_DIRECTIVE ${MODULE_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${MODULE_INCLUDE_DIRECTORIES}
  SRCS ${MODULE_SRCS}
  MOC_SRCS ${MODULE_MOC_SRCS}
  UI_SRCS ${MODULE_UI_SRCS}
  TARGET_LIBRARIES ${MODULE_TARGET_LIBRARIES}
  RESOURCES ${MODULE_RESOURCES}
  )

#-----------------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(Testing)
endif()
