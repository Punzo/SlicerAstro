import os
import unittest
from __main__ import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
import logging
from DataProbe import DataProbeInfoWidget


def GetInfoAstro(self, xyz, ras, sliceNode, sliceLogic):

  selectionNode = slicer.mrmlScene.GetNthNodeByClass(0,'vtkMRMLSelectionNode')

  spacing = "%.1f" % sliceLogic.GetLowestVolumeSliceSpacing()[2]
  if sliceNode.GetSliceSpacingMode() == slicer.vtkMRMLSliceNode.PrescribedSliceSpacingMode:
    spacing = "(%s)" % spacing

  def _roundInt(value):
    try:
      return int(round(value))
    except ValueError:
      return 0

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
      ijk = [_roundInt(value) for value in ijkFloat]
      display = volumeNode.GetDisplayNode()
      if display:
        if layer == 'B':
          hasBLayer = True
          CoordinateSystemName = display.GetSpace()
          volumeNode.GetReferenceSpace(ijk, CoordinateSystemName, ras)
          Quantities = display.GetSpaceQuantities()
          UnitNode1 = selectionNode.GetUnitNode(Quantities.GetValue(0))
          UnitNode2 = selectionNode.GetUnitNode(Quantities.GetValue(1))
          UnitNode3 = selectionNode.GetUnitNode(Quantities.GetValue(2))

        if layer == "F" and hasBLayer == False:
          hasFLayer = True
          CoordinateSystemName = display.GetSpace()
          volumeNode.GetReferenceSpace(ijk, CoordinateSystemName, ras)
          Quantities = display.GetSpaceQuantities()
          UnitNode1 = selectionNode.GetUnitNode(Quantities.GetValue(0))
          UnitNode2 = selectionNode.GetUnitNode(Quantities.GetValue(1))
          UnitNode3 = selectionNode.GetUnitNode(Quantities.GetValue(2))

        if layer == "L" and hasBLayer == False and hasFLayer == False:
          hasLLayer = True
          CoordinateSystemName = display.GetSpace()
          volumeNode.GetReferenceSpace(ijk, CoordinateSystemName, ras)
          Quantities = display.GetSpaceQuantities()
          UnitNode1 = selectionNode.GetUnitNode(Quantities.GetValue(0))
          UnitNode2 = selectionNode.GetUnitNode(Quantities.GetValue(1))
          UnitNode3 = selectionNode.GetUnitNode(Quantities.GetValue(2))

      self.layerNames[layer].setText(
          "<b>%s</b>" % (self.fitName(volumeNode.GetName()) if volumeNode else "None"))
      self.layerIJKs[layer].setText(
          "({i:4d}, {j:4d}, {k:4d})".format(i=ijk[0], j=ijk[1], k=ijk[2]) if volumeNode else "")
      self.layerValues[layer].setText(
          "<b>%s</b>" % display.GetPixelString(ijk) if display else "")

      if not (hasBLayer or hasFLayer or hasLLayer):
        UnitNode1 = selectionNode.GetUnitNode("length")
        UnitNode2 = selectionNode.GetUnitNode("length")
        UnitNode3 = selectionNode.GetUnitNode("velocity")
        CoordinateSystemName = "WCS"

      world_x = UnitNode1.GetDisplayStringFromValue(ras[0])
      world_y = UnitNode2.GetDisplayStringFromValue(ras[1])
      world_z = UnitNode3.GetDisplayStringFromValue(ras[2])

      self.viewInfo.text = \
        "  {layoutName: <8s} {sys:s}:({world_x:>10s},{world_y:>10s},{world_z:>10s}) {orient: >8s} Sp:{spacing:s}" \
        .format(layoutName=sliceNode.GetLayoutName(),
                  sys = CoordinateSystemName,
                  world_x=world_x,
                  world_y=world_y,
                  world_z=world_z,
                  orient=sliceNode.GetOrientationString(),
                  spacing=spacing
                  )

  return hasVolume


class AstroDataProbe(ScriptedLoadableModule):

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    parent.title = "AstroDataProbe"
    parent.categories = ["Quantification"]
    parent.dependencies = ["DataProbe"]
    parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute."]
    parent.helpText = """
    Data probe factorization for WCS astronomical coordinates.
    """
    parent.acknowledgementText = """
    This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute.
    """
    if slicer.mrmlScene.GetTagByClassName( "vtkMRMLScriptedModuleNode" ) != 'ScriptedModule':
      slicer.mrmlScene.RegisterNodeClass(vtkMRMLScriptedModuleNode())

    logic = AstroDataProbeLogic(parent)

#class AstroDataProbeWidget(ScriptedLoadableModuleWidget):

 # def setup(self):
  #  ScriptedLoadableModuleWidget.setup(self)

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
    if callData.GetName() == "AstroVolumeDisplay" and self.factorizing:
      self.factorizing = False
      dataProbeInstance = slicer.modules.DataProbeInstance
      funcType = type(dataProbeInstance.infoWidget.GetInfo)
      dataProbeInstance.infoWidget.GetInfo = funcType(GetInfoAstro, dataProbeInstance.infoWidget, DataProbeInfoWidget)

class AstroDataProbeTest(ScriptedLoadableModuleTest):

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    self.setUp()
