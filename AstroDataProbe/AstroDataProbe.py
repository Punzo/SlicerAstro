from __future__ import division
import os
import unittest
from __main__ import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
import logging
from DataProbe import DataProbeInfoWidget
from math import floor
import DataProbeLib
from slicer.util import settingsValue
from slicer.util import VTKObservationMixin

"""
  sliceLogic = None
  if sliceNode:
    appLogic = slicer.app.applicationLogic()
    if appLogic:
      sliceLogic = appLogic.GetSliceLogic(sliceNode)

  world = [0., 0., 0.]
  CoordinateSystemName = "IJK"
  hasVolume = False

  if sliceLogic:

    hasBLayer = False
    hasFLayer = False
    hasLLayer = False

    layerLogicCalls = (('B', sliceLogic.GetBackgroundLayer),
                     ('F', sliceLogic.GetForegroundLayer),
                     ('L', sliceLogic.GetLabelLayer))
    for layer,logicCall in layerLogicCalls:
      layerLogic = logicCall()
      volumeNode = layerLogic.GetVolumeNode()
      ijk = [0, 0, 0]
      if volumeNode:
        hasVolume = True
        xyToIJK = layerLogic.GetXYToIJKTransform()
        ijkFloat = xyToIJK.TransformDoublePoint(xyz)
        display = volumeNode.GetDisplayNode()
        if display:
          Quantities = display.GetSpaceQuantities()
          CoordinateSystemName = display.GetSpace()
          selectionNode = slicer.mrmlScene.GetNthNodeByClass(0,'vtkMRMLSelectionNode')
          if selectionNode:
            UnitNode1 = selectionNode.GetUnitNode(Quantities.GetValue(0))
            UnitNode2 = selectionNode.GetUnitNode(Quantities.GetValue(1))
            UnitNode3 = selectionNode.GetUnitNode(Quantities.GetValue(2))
            if layer == 'B':
              hasBLayer = True
              volumeNode.GetReferenceSpace(ijkFloat, CoordinateSystemName, world)
            if layer == "F" and hasBLayer == False:
              hasFLayer = True
              volumeNode.GetReferenceSpace(ijkFloat, CoordinateSystemName, world)
            if layer == "L" and hasBLayer == False and hasFLayer == False:
              hasLLayer = True
              volumeNode.GetReferenceSpace(ijkFloat, CoordinateSystemName, world)
"""

try:
  import numpy as np
  NUMPY_AVAILABLE = True
except ImportError:
  NUMPY_AVAILABLE = False

def generateViewDescriptionAstro(self, xyz, ras, sliceNode, sliceLogic):

  if sliceLogic:

    worldX = "0"
    worldY = "0"
    worldZ = "0"

    world = [0., 0., 0.]

    CoordinateSystemName = "IJK"

    hasVolume = False
    hasBLayer = False
    hasFLayer = False
    hasLLayer = False

    layerLogicCalls = (('B', sliceLogic.GetBackgroundLayer),
                   ('F', sliceLogic.GetForegroundLayer),
                   ('L', sliceLogic.GetLabelLayer))
    for layer,logicCall in layerLogicCalls:
      layerLogic = logicCall()
      volumeNode = layerLogic.GetVolumeNode()
      ijk = [0, 0, 0]
      if volumeNode:
        hasVolume = True
        xyToIJK = layerLogic.GetXYToIJKTransform()
        ijkFloat = xyToIJK.TransformDoublePoint(xyz)
        display = volumeNode.GetDisplayNode()
        if display:
          Quantities = display.GetSpaceQuantities()
          CoordinateSystemName = display.GetSpace()
          selectionNode = slicer.mrmlScene.GetNthNodeByClass(0,'vtkMRMLSelectionNode')
          if selectionNode:
            UnitNode1 = selectionNode.GetUnitNode(Quantities.GetValue(0))
            UnitNode2 = selectionNode.GetUnitNode(Quantities.GetValue(1))
            UnitNode3 = selectionNode.GetUnitNode(Quantities.GetValue(2))
            if layer == 'B':
              hasBLayer = True
              volumeNode.GetReferenceSpace(ijkFloat, CoordinateSystemName, world)
            if layer == "F" and hasBLayer == False:
              hasFLayer = True
              volumeNode.GetReferenceSpace(ijkFloat, CoordinateSystemName, world)
            if layer == "L" and hasBLayer == False and hasFLayer == False:
              hasLLayer = True
              volumeNode.GetReferenceSpace(ijkFloat, CoordinateSystemName, world)

    if hasVolume == True:
      worldX = display.GetDisplayStringFromValue(world[0], UnitNode1)
      worldY = display.GetDisplayStringFromValue(world[1], UnitNode2)
      worldZ = display.GetDisplayStringFromValue(world[2], UnitNode3)

    if CoordinateSystemName == "WCS":
      return "  {layoutName: <8s} {sys:s}:({worldX:>16s},{worldY:>16s},{worldZ:>10s}) {orient: >8s}" \
        .format(layoutName=sliceNode.GetLayoutName(),
                sys = CoordinateSystemName,
                worldX=worldX,
                worldY=worldY,
                worldZ=worldZ,
                orient=sliceNode.GetOrientationString(),
                )
    else:
      return "  {layoutName: <8s} AstroDataProbe could not find WCS coordinates in View: {orient: >8s}" \
        .format(layoutName=sliceNode.GetLayoutName(),
                orient=sliceNode.GetOrientationString(),
                )


def generateLayerNameAstro(self, slicerLayerLogic):
  volumeNode = slicerLayerLogic.GetVolumeNode()
  return "<b>%s</b>" % (self.fitName(volumeNode.GetName()) if volumeNode else "None")

def generateIJKPixelDescriptionAstro(self, ijk, slicerLayerLogic):
  volumeNode = slicerLayerLogic.GetVolumeNode()
  return "({i:4d}, {j:4d}, {k:4d})".format(i=ijk[0], j=ijk[1], k=ijk[2]) if volumeNode else ""

def generateIJKPixelValueDescriptionAstro(self, ijk, slicerLayerLogic):
  volumeNode = slicerLayerLogic.GetVolumeNode()
  if volumeNode:
    if volumeNode.IsA("vtkMRMLAstroVolumeNode"):
      display = volumeNode.GetDisplayNode()
      return "<b>%s</b>" % display.GetPixelString(ijk) if display else ""
    else:
      return "<b>%s</b>" % self.getPixelString(volumeNode,ijk) if volumeNode else ""


def  makeAstroRuler(self, sliceNode):
  sliceViewName = sliceNode.GetLayoutName()
  renderer = self.renderers[sliceViewName]
  if self.sliceViews[sliceViewName]:
    #
    # update scaling ruler
    #
    viewWidth = self.sliceViews[sliceViewName].width
    pts = self.points[sliceViewName]
    pts.SetPoint(0,[0,5, 0])
    pts.SetPoint(1,[10,15, 0])
    pts.SetPoint(2,[20,5, 0])
    pts.SetPoint(3,[30,10, 0])
    pts.SetPoint(4,[40,5, 0])
    pts.SetPoint(5,[50,15, 0])
    pts.SetPoint(6,[60,5, 0])
    pts.SetPoint(7,[70,10, 0])
    pts.SetPoint(8,[80,5, 0])
    pts.SetPoint(9,[90,15, 0])
    pts.SetPoint(10,[100, 0, 0])
    pts.SetPoint(11,[110,10, 0])
    pts.SetPoint(12,[120,5, 0])
    pts.SetPoint(13,[130,15, 0])
    pts.SetPoint(14,[140,5, 0])
    pts.SetPoint(15,[150,10, 0])
    pts.SetPoint(16,[160,5, 0])
    pts.SetPoint(17,[170,15, 0])
    pts.SetPoint(18,[180,5, 0])
    pts.SetPoint(19,[190,10, 0])
    pts.SetPoint(20,[2000,5, 0])
    pts.SetPoint(21,[210,15, 0])

    textActor = self.rulerTextActors[sliceViewName]
    textActor.SetInput("bella")
    textProperty = textActor.GetTextProperty()
    # set font size
    textProperty.SetFontSize(self.fontSize)
    # set font family
    if self.fontFamily == 'Times':
      textProperty.SetFontFamilyToTimes()
    else:
      textProperty.SetFontFamilyToArial()
    # set ruler text actor position
    textActor.SetDisplayPosition(int(viewWidth/2.), int(viewWidth/2.))

    renderer.AddActor2D(self.rulerActors[sliceViewName])
    renderer.RemoveActor2D(textActor)
    renderer.AddActor2D(textActor)

  return

class AstroDataProbe(ScriptedLoadableModule):

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    parent.title = "AstroDataProbe"
    parent.categories = ["Quantification", "Astronomy"]
    parent.dependencies = ["DataProbe"]
    parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)."]
    parent.helpText = """
    Data probe factorization for WCS astronomical coordinates.
    """
    parent.acknowledgementText = """
    This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute.
    """
    if slicer.mrmlScene.GetTagByClassName( "vtkMRMLScriptedModuleNode" ) != 'ScriptedModule':
      slicer.mrmlScene.RegisterNodeClass(vtkMRMLScriptedModuleNode())

    logic = AstroDataProbeLogic(parent)

class AstroDataProbeWidget(ScriptedLoadableModuleWidget):

  def __init__(self, parent):
    ScriptedLoadableModuleLogic.__init__(self, parent)

  def setup(self):
    ScriptedLoadableModuleWidget.setup(self)


class AstroDataProbeLogic(ScriptedLoadableModuleLogic):

  def __init__(self, parent):
    ScriptedLoadableModuleLogic.__init__(self, parent)
    self.factorizing = True
    # Observe Astrovolume has been loaded
    self.nodeAddedModifiedObserverTag = slicer.mrmlScene.AddObserver(slicer.vtkMRMLScene.NodeAddedEvent, self.nodeAddedCallback)

  def __del__(self):
    self.removeObservers()

  def removeObservers(self):
    # remove observers and reset
    if self.nodeAddedModifiedObserverTag:
      slicer.mrmlScene.AddObserver.RemoveObserver(self.nodeAddedModifiedObserverTag)
    self.nodeAddedModifiedObserverTag = None


  @vtk.calldata_type(vtk.VTK_OBJECT)
  def nodeAddedCallback(self, caller, eventId, callData):
    if (callData.GetName() == "AstroVolumeDisplay" or callData.GetName() == "AstroLabelMapVolumeDisplay") and self.factorizing:
      self.factorizing = False
      dataProbeInstance = slicer.modules.DataProbeInstance
      funcType = type(dataProbeInstance.infoWidget.generateViewDescription)
      dataProbeInstance.infoWidget.generateViewDescription = funcType(generateViewDescriptionAstro, dataProbeInstance.infoWidget, DataProbeInfoWidget)

      funcType = type(dataProbeInstance.infoWidget.generateLayerName)
      dataProbeInstance.infoWidget.generateLayerName = funcType(generateLayerNameAstro, dataProbeInstance.infoWidget, DataProbeInfoWidget)

      funcType = type(dataProbeInstance.infoWidget.generateIJKPixelDescription)
      dataProbeInstance.infoWidget.generateIJKPixelDescription = funcType(generateIJKPixelDescriptionAstro, dataProbeInstance.infoWidget, DataProbeInfoWidget)

      funcType = type(dataProbeInstance.infoWidget.generateIJKPixelValueDescription)
      dataProbeInstance.infoWidget.generateIJKPixelValueDescription = funcType(generateIJKPixelValueDescriptionAstro, dataProbeInstance.infoWidget, DataProbeInfoWidget)

      funcType = type(dataProbeInstance.infoWidget.sliceAnnotations.makeRuler)
      dataProbeInstance.infoWidget.sliceAnnotations.makeRuler = funcType(makeAstroRuler, dataProbeInstance.infoWidget.sliceAnnotations, DataProbeInfoWidget)

class AstroDataProbeTest(ScriptedLoadableModuleTest):

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    self.setUp()
