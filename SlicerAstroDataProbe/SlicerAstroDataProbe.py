# -*- coding: utf-8 -*-
from __future__ import division
import os
import unittest
from __main__ import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
import logging
from DataProbe import DataProbeInfoWidget
from math import floor
from math import fabs
import DataProbeLib
from slicer.util import settingsValue
from slicer.util import VTKObservationMixin


class SlicerAstroDataProbe(ScriptedLoadableModule):

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    parent.title = "SlicerAstro Data Probe"
    parent.categories = ["Quantification", "Astronomy"]
    parent.dependencies = ["DataProbe", "AstroVolume"]
    parent.contributors = ["""
    Davide Punzo (Kapteyn Astronomical Institute),
    Thijs van der Hulst (Kapteyn Astronomical Institute),
    Jos Roerdink (Johann Bernoulli Institute) and
    Jean-Christophe Fillion-Robin (Kitware)."""]
    parent.helpText = """
    Data probe factorization for WCS astronomical coordinates.
    """
    parent.acknowledgementText = """
    This module was developed by Davide Punzo. <br>
    This work was supported by ERC grant nr. 291531 and the Slicer Community.
    """

    if slicer.mrmlScene.GetTagByClassName( "vtkMRMLScriptedModuleNode" ) != 'ScriptedModule':
      slicer.mrmlScene.RegisterNodeClass(vtkMRMLScriptedModuleNode())

    # Trigger the override of DataProbe when application has started up
    if not slicer.app.commandOptions().noMainWindow :
      slicer.app.connect("startupCompleted()", self.override)


  def override(self):
    try:
      parent = slicer.util.findChildren(text='Data Probe')[0]
    except IndexError:
      print("No Data Probe frame - cannot create DataProbe")
      return
    SlicerAstroDataProbeLogic(parent)

    # disable 3DSlicer Annotations
    sliceAnnotations = slicer.modules.DataProbeInstance.infoWidget.sliceAnnotations
    sliceAnnotations.sliceViewAnnotationsEnabled = 0
    sliceAnnotations.updateSliceViewFromGUI()
    sliceAnnotations.updateEnabledButtons()
    sliceAnnotations.sliceViewAnnotationsCheckBox.checked = 0
    settingsValue('DataProbe/sliceViewAnnotations.enabled', 0, converter=int)


class SlicerAstroDataProbeWidget(ScriptedLoadableModuleWidget):

  def setup(self):
    self.developerMode = False
    ScriptedLoadableModuleWidget.setup(self)

    annotationsCollapsibleButton = ctk.ctkCollapsibleButton()
    annotationsCollapsibleButton.text = "Astro Annotations"
    self.layout.addWidget(annotationsCollapsibleButton)
    # Layout within the dummy collapsible button
    annotationsFormLayout = qt.QVBoxLayout(annotationsCollapsibleButton)

    # Color
    horizontalLayout_1 = qt.QHBoxLayout()
    horizontalLayout_1.setObjectName("horizontalLayout_1");

    self.colorLabel = qt.QLabel()
    self.colorLabel.setText("Annotations color:")
    self.colorLabel.setFixedSize(qt.QSize(120, 30))
    horizontalLayout_1.addWidget(self.colorLabel)

    self.colorSelector = ctk.ctkColorPickerButton()
    self.colorSelector.setObjectName("ColorPickerButton")
    sizePolicy = qt.QSizePolicy()
    sizePolicy.setHorizontalPolicy(qt.QSizePolicy.Expanding)
    sizePolicy.setVerticalPolicy(qt.QSizePolicy.Fixed)
    sizePolicy.setHorizontalStretch(0)
    sizePolicy.setVerticalStretch(0)
    sizePolicy.setHeightForWidth(self.colorSelector.sizePolicy.hasHeightForWidth())
    self.colorSelector.setSizePolicy(sizePolicy)
    self.colorSelector.setMinimumSize(qt.QSize(0, 30))
    self.colorSelector.setIconSize(qt.QSize(32, 32))
    self.colorSelector.setColor(qt.QColor(255, 187, 20))
    self.colorSelector.setDisplayColorName(0)
    horizontalLayout_1.addWidget(self.colorSelector)

    annotationsFormLayout.addLayout(horizontalLayout_1)

    # Font Style
    horizontalLayout_2 = qt.QHBoxLayout()
    horizontalLayout_2.setObjectName("horizontalLayout_2");

    self.fontStyleLabel = qt.QLabel()
    self.fontStyleLabel.setText("Font style:")
    self.fontStyleLabel.setFixedSize(qt.QSize(120, 30))
    horizontalLayout_2.addWidget(self.fontStyleLabel)

    self.styleComboBox = qt.QComboBox()
    self.styleComboBox.addItem("Arial")
    self.styleComboBox.addItem("Courier")
    self.styleComboBox.addItem("Times")
    sizePolicy.setHeightForWidth(self.styleComboBox.sizePolicy.hasHeightForWidth())
    self.styleComboBox.setMinimumSize(qt.QSize(0, 30))
    horizontalLayout_2.addWidget(self.styleComboBox)

    annotationsFormLayout.addLayout(horizontalLayout_2)

    # Font size
    horizontalLayout_3 = qt.QHBoxLayout()
    horizontalLayout_3.setObjectName("horizontalLayout_3");

    self.fontSizeLabel = qt.QLabel()
    self.fontSizeLabel.setText("Font size:")
    self.fontSizeLabel.setFixedSize(qt.QSize(120, 30))
    horizontalLayout_3.addWidget(self.fontSizeLabel)

    self.sizeSpinBox = qt.QSpinBox()
    sizePolicy.setHeightForWidth(self.sizeSpinBox.sizePolicy.hasHeightForWidth())
    self.sizeSpinBox.setMinimumSize(qt.QSize(0, 30))
    self.sizeSpinBox.minimum = 1
    self.sizeSpinBox.maximum = 30
    self.sizeSpinBox.setValue(12)
    self.sizeSpinBox.setToolTip("This value is multiplied for 1.5 if the ruler is set to thick.")

    horizontalLayout_3.addWidget(self.sizeSpinBox)

    annotationsFormLayout.addLayout(horizontalLayout_3)

    verticalSpacer = qt.QSpacerItem(200, 200, qt.QSizePolicy.Minimum, qt.QSizePolicy.Expanding)
    self.layout.addItem(verticalSpacer)

    # Connections
    self.colorSelector.connect('colorChanged(QColor)', self.onAnnotationsColorChanged)

    self.styleComboBox.connect('currentTextChanged(QString)', self.onAnnotationsFontStyleChanged)

    self.sizeSpinBox.connect('valueChanged(int)', self.onAnnotationsFontSizeChanged)


  def onAnnotationsColorChanged(self, color):
    lm = slicer.app.layoutManager()
    for sliceName in lm.sliceViewNames():
      sWidget = lm.sliceWidget(sliceName)
      sView = sWidget.sliceView()
      DisplayableManagersCollection = vtk.vtkCollection()
      sView.getDisplayableManagers(DisplayableManagersCollection)
      for DisplayableManagersIndex in range(DisplayableManagersCollection.GetNumberOfItems()):
        AstroDisplayableManager = DisplayableManagersCollection.GetItemAsObject(DisplayableManagersIndex)
        if AstroDisplayableManager.GetClassName() == "vtkMRMLAstroTwoDAxesDisplayableManager" or \
           AstroDisplayableManager.GetClassName() == "vtkMRMLAstroBeamDisplayableManager" :
          AstroDisplayableManager.SetAnnotationsColor(color.red() / 255., color.green() / 255., color.blue() / 255.)

  def onAnnotationsFontStyleChanged(self, font):
    lm = slicer.app.layoutManager()
    for sliceName in lm.sliceViewNames():
      sWidget = lm.sliceWidget(sliceName)
      sView = sWidget.sliceView()
      DisplayableManagersCollection = vtk.vtkCollection()
      sView.getDisplayableManagers(DisplayableManagersCollection)
      for DisplayableManagersIndex in range(DisplayableManagersCollection.GetNumberOfItems()):
        AstroDisplayableManager = DisplayableManagersCollection.GetItemAsObject(DisplayableManagersIndex)
        if AstroDisplayableManager.GetClassName() == "vtkMRMLAstroTwoDAxesDisplayableManager":
          AstroDisplayableManager.SetAnnotationsFontStyle(font)

  def onAnnotationsFontSizeChanged(self, size):
    lm = slicer.app.layoutManager()
    for sliceName in lm.sliceViewNames():
      sWidget = lm.sliceWidget(sliceName)
      sView = sWidget.sliceView()
      DisplayableManagersCollection = vtk.vtkCollection()
      sView.getDisplayableManagers(DisplayableManagersCollection)
      for DisplayableManagersIndex in range(DisplayableManagersCollection.GetNumberOfItems()):
        AstroDisplayableManager = DisplayableManagersCollection.GetItemAsObject(DisplayableManagersIndex)
        if AstroDisplayableManager.GetClassName() == "vtkMRMLAstroTwoDAxesDisplayableManager":
          AstroDisplayableManager.SetAnnotationsFontSize(size)

class SlicerAstroDataProbeLogic(ScriptedLoadableModuleLogic):

  def __init__(self, parent):
    ScriptedLoadableModuleLogic.__init__(self, parent)

    dataProbeInstance = slicer.modules.DataProbeInstance
    funcType = type(dataProbeInstance.infoWidget.generateViewDescription)
    dataProbeInstance.infoWidget.generateViewDescription = funcType(generateViewDescriptionAstro, dataProbeInstance.infoWidget)

    funcType = type(dataProbeInstance.infoWidget.generateLayerName)
    dataProbeInstance.infoWidget.generateLayerName = funcType(generateLayerNameAstro, dataProbeInstance.infoWidget)

    funcType = type(dataProbeInstance.infoWidget.generateIJKPixelDescription)
    dataProbeInstance.infoWidget.generateIJKPixelDescription = funcType(generateIJKPixelDescriptionAstro, dataProbeInstance.infoWidget)

    funcType = type(dataProbeInstance.infoWidget.generateIJKPixelValueDescription)
    dataProbeInstance.infoWidget.generateIJKPixelValueDescription = funcType(generateIJKPixelValueDescriptionAstro, dataProbeInstance.infoWidget)


class SlicerAstroDataProbeTest(ScriptedLoadableModuleTest):

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    self.setUp()


def generateViewDescriptionAstro(self, xyz, ras, sliceNode, sliceLogic):
  if sliceLogic:

    worldX = ""
    worldY = ""
    worldZ = ""
    equinoxString = ""
    world = [0., 0., 0.]

    CoordinateSystemName = "IJK"
    dimensionality = 3

    layerLogicCalls = (('B', sliceLogic.GetBackgroundLayer),
                      ('F', sliceLogic.GetForegroundLayer),
                      ('L', sliceLogic.GetLabelLayer))

    for layer,logicCall in layerLogicCalls:
      layerLogic = logicCall()
      volumeNode = layerLogic.GetVolumeNode()
      if volumeNode is not None and (volumeNode.IsA("vtkMRMLAstroVolumeNode") or volumeNode.IsA("vtkMRMLAstroLabelMapVolumeNode")):
        dimensionality = int(volumeNode.GetAttribute("SlicerAstro.NAXIS"))
        xyToIJK = layerLogic.GetXYToIJKTransform()
        ijkFloat = xyToIJK.TransformDoublePoint(xyz)
        displayNode = volumeNode.GetDisplayNode()
        if displayNode is not None and (displayNode.IsA("vtkMRMLAstroVolumeDisplayNode") or displayNode.IsA("vtkMRMLAstroLabelMapVolumeDisplayNode")):
          CoordinateSystemName = displayNode.GetSpace()
          displayNode.GetReferenceSpace(ijkFloat, world)
          worldTemp = [0., 0., 0.]
          worldX = displayNode.GetDisplayStringFromValueX(world[0], worldTemp, worldTemp, 3)
          equinox = float(volumeNode.GetAttribute("SlicerAstro.EQUINOX"))
          if equinox > 1975:
            equinoxString = "J" + str(equinox)[:4]
          elif equinox < 1975:
            equinoxString = "B" + str(equinox)[:4]
          if dimensionality > 1:
            worldY = displayNode.GetDisplayStringFromValueY(world[1], worldTemp, worldTemp, 3)
          if dimensionality > 2:
            worldZ = displayNode.GetDisplayStringFromValueZ(world[2], worldTemp, worldTemp, 3)
            worldZ = displayNode.AddVelocityInfoToDisplayStringZ(worldZ)
          break

    if CoordinateSystemName == "WCS":
      if dimensionality > 2:
        return " {sys}[{orient}]: {worldX}, {worldY} ({equinoxString}), {worldZ}" \
                .format(
                sys = CoordinateSystemName,
                orient = sliceNode.GetOrientation(),
                worldX = worldX,
                worldY = worldY,
                equinoxString = equinoxString,
                worldZ = worldZ
                )
      elif dimensionality > 1:
        return " {sys}[{orient}]: {worldX}, {worldY} ({equinoxString})" \
                .format(
                sys = CoordinateSystemName,
                orient = sliceNode.GetOrientation(),
                worldX = worldX,
                worldY = worldY,
                equinoxString = equinoxString
                )
      else:
        return " {sys}[{orient}]: {worldX} ({equinoxString})" \
                .format(
                sys = CoordinateSystemName,
                orient = sliceNode.GetOrientation(),
                worldX = worldX,
                equinoxString = equinoxString
                )
    else:
      return " {layoutName} WCS in View {orient} not found" \
        .format(layoutName=sliceNode.GetLayoutName(),
                orient=sliceNode.GetOrientation()
                )


def generateLayerNameAstro(self, slicerLayerLogic):
  volumeNode = slicerLayerLogic.GetVolumeNode()
  return "<b>%s</b>" % (self.fitName(volumeNode.GetName()) if volumeNode else "None")

def generateIJKPixelDescriptionAstro(self, ijk, slicerLayerLogic):
  volumeNode = slicerLayerLogic.GetVolumeNode()
  return "({i:4d}, {j:4d}, {k:4d})".format(i=ijk[0], j=ijk[1], k=ijk[2]) if volumeNode else ""

def generateIJKPixelValueDescriptionAstro(self, ijk, slicerLayerLogic):
  volumeNode = slicerLayerLogic.GetVolumeNode()
  if volumeNode is not None:
    displayNode = volumeNode.GetDisplayNode()
    if displayNode is not None and (displayNode.IsA("vtkMRMLAstroVolumeDisplayNode") or displayNode.IsA("vtkMRMLAstroLabelMapVolumeDisplayNode")):
      return "<b>%s</b>" % displayNode.GetPixelString(ijk) if displayNode else ""
