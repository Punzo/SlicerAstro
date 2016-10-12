project(qSlicer${MODULE_NAME}EditorEffects)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "Q_SLICER_${MODULE_NAME_UPPER}_EFFECTS_EXPORT")

list(GET qSlicerSegmentationsModuleWidgets_INCLUDE_DIRS 0 qSlicerSegmentationsModuleEditorEffects_INCLUDE_DIR)
set(qSlicerSegmentationsModuleEditorEffects_INCLUDE_DIR ${qSlicerSegmentationsModuleEditorEffects_INCLUDE_DIR}/../EditorEffects)
list(GET qSlicerSegmentationsModuleWidgets_INCLUDE_DIRS 1 qSlicerSegmentationsModuleEditorEffects_INCLUDE_BINARY_DIR)
set(qSlicerSegmentationsModuleEditorEffects_INCLUDE_BINARY_DIR ${qSlicerSegmentationsModuleEditorEffects_INCLUDE_BINARY_DIR}/../EditorEffects)

set(${KIT}_INCLUDE_DIRECTORIES
  ${vtkSlicerSegmentationsModuleMRML_INCLUDE_DIRS}
  ${vtkSlicerSegmentationsModuleLogic_INCLUDE_DIRS}
  ${MRMLCore_INCLUDE_DIRS}
  ${qMRMLWidgets_INCLUDE_DIRS}
  ${MRMLLogic_INCLUDE_DIRS}
  ${Slicer_Base_INCLUDE_DIRS}
  ${vtkSegmentationCore_INCLUDE_DIRS}
  ${qSlicerSegmentationsModuleEditorEffects_INCLUDE_DIR}
  ${qSlicerSegmentationsModuleEditorEffects_INCLUDE_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../MRML
  ${CMAKE_CURRENT_BINARY_DIR}/../MRML
  )

set(${KIT}_SRCS
  qSlicerSegmentEditorAstroCloudLassoEffect.h
  qSlicerSegmentEditorAstroCloudLassoEffect_p.h
  qSlicerSegmentEditorAstroCloudLassoEffect.cxx
  )

set(${KIT}_MOC_SRCS
  qSlicerSegmentEditorAstroCloudLassoEffect.h
  qSlicerSegmentEditorAstroCloudLassoEffect_p.h
  )

set(${KIT}_UI_SRCS
  )

set(${KIT}_RESOURCES
  Resources/${KIT}.qrc
  )

#-----------------------------------------------------------------------------
set(${KIT}_TARGET_LIBRARIES
  vtkSlicerSegmentationsModuleMRML
  vtkSlicerSegmentationsModuleLogic
  MRMLCore
  MRMLLogic
  qMRMLWidgets
  qSlicerSegmentationsEditorEffects
  vtkSlicerAstroVolumeModuleMRML
  )

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleQtLibrary(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  FOLDER "Module-${MODULE_NAME}"
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  MOC_SRCS ${${KIT}_MOC_SRCS}
  UI_SRCS ${${KIT}_UI_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  RESOURCES ${${KIT}_RESOURCES}
  WRAP_PYTHONQT
  )
