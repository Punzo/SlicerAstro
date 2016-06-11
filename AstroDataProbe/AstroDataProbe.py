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
    This work was supported by ERC grant nr. 291531 and the Slicer Community.
    """
    # Trigger the override od DataProbe when application has started up
    if not slicer.app.commandOptions().noMainWindow :
      qt.QTimer.singleShot(0, self.override);


  def override(self):
    try:
      parent = slicer.util.findChildren(text='Data Probe')[0]
    except IndexError:
      print("No Data Probe frame - cannot create DataProbe")
      return
    AstroDataProbeLogic(parent)


class AstroDataProbeWidget(ScriptedLoadableModuleWidget):

  def setup(self):
    self.developerMode = False
    ScriptedLoadableModuleWidget.setup(self)

class AstroDataProbeLogic(ScriptedLoadableModuleLogic):

  def __init__(self, parent):
    ScriptedLoadableModuleLogic.__init__(self, parent)
    dataProbeInstance = slicer.modules.DataProbeInstance
    funcType = type(dataProbeInstance.infoWidget.generateViewDescription)
    dataProbeInstance.infoWidget.generateViewDescription = funcType(generateViewDescriptionAstro, dataProbeInstance.infoWidget, DataProbeInfoWidget)

    funcType = type(dataProbeInstance.infoWidget.generateLayerName)
    dataProbeInstance.infoWidget.generateLayerName = funcType(generateLayerNameAstro, dataProbeInstance.infoWidget, DataProbeInfoWidget)

    funcType = type(dataProbeInstance.infoWidget.generateIJKPixelDescription)
    dataProbeInstance.infoWidget.generateIJKPixelDescription = funcType(generateIJKPixelDescriptionAstro, dataProbeInstance.infoWidget, DataProbeInfoWidget)

    funcType = type(dataProbeInstance.infoWidget.generateIJKPixelValueDescription)
    dataProbeInstance.infoWidget.generateIJKPixelValueDescription = funcType(generateIJKPixelValueDescriptionAstro, dataProbeInstance.infoWidget, DataProbeInfoWidget)


class AstroDataProbeTest(ScriptedLoadableModuleTest):

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

    worldX = "0"
    worldY = "0"
    worldZ = "0"
    world = [0., 0., 0.]

    CoordinateSystemName = "IJK"

    layerLogicCalls = (('B', sliceLogic.GetBackgroundLayer),
                   ('F', sliceLogic.GetForegroundLayer),
                   ('L', sliceLogic.GetLabelLayer))
    for layer,logicCall in layerLogicCalls:
      layerLogic = logicCall()
      volumeNode = layerLogic.GetVolumeNode()
      if volumeNode:
        xyToIJK = layerLogic.GetXYToIJKTransform()
        ijkFloat = xyToIJK.TransformDoublePoint(xyz)
        displayNode = volumeNode.GetDisplayNode()
        if displayNode:
          CoordinateSystemName = displayNode.GetSpace()
          ijkFloat = ijkFloat
          displayNode.GetReferenceSpace(ijkFloat, world)
          worldX = displayNode.GetDisplayStringFromValueX(world[0])
          worldY = displayNode.GetDisplayStringFromValueY(world[1])
          worldZ = displayNode.GetDisplayStringFromValueZ(world[2])
          printVel = 0
          stringArray = displayNode.GetSpaceQuantities()
          if stringArray.GetValue(2) == "velocity":
            printVel = 1
            velKey = volumeNode.GetAttribute("SlicerAstro.CTYPE3")
          break

    if CoordinateSystemName == "WCS":
      if printVel:
        return "  {layoutName: <8s} {sys:s}:{worldX:>16s},{worldY:>16s},{worldZ:>10s} ({velKey}) {orient: >4s}" \
               .format(layoutName=sliceNode.GetLayoutName(),
               sys = CoordinateSystemName,
               worldX=worldX,
               worldY=worldY,
               worldZ=worldZ,
               velKey = velKey,
               orient=sliceNode.GetOrientationString(),
               )
      else:
        return "  {layoutName: <8s} {sys:s}:{worldX:>16s},{worldY:>16s},{worldZ:>10s} {orient: >4s}" \
                .format(layoutName=sliceNode.GetLayoutName(),
                sys = CoordinateSystemName,
                worldX=worldX,
                worldY=worldY,
                worldZ=worldZ,
                orient=sliceNode.GetOrientationString(),
                )
    else:
      return "  {layoutName: <8s} AstroDataProbe could not find WCS coordinates in View: {orient: >4s}" \
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
      displayNode = volumeNode.GetDisplayNode()
      return "<b>%s</b>" % displayNode.GetPixelString(ijk) if displayNode else ""
    else:
      return "<b>%s</b>" % self.getPixelString(volumeNode,ijk) if volumeNode else ""
