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
    parent.dependencies = ["DataProbe"]
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
      #qt.QTimer.singleShot(2000, self.override);


  def override(self):
    try:
      parent = slicer.util.findChildren(text='Data Probe')[0]
    except IndexError:
      print("No Data Probe frame - cannot create DataProbe")
      return
    SlicerAstroDataProbeLogic(parent)


class SlicerAstroDataProbeWidget(ScriptedLoadableModuleWidget):

  def setup(self):
    self.developerMode = False
    ScriptedLoadableModuleWidget.setup(self)

class SlicerAstroDataProbeLogic(ScriptedLoadableModuleLogic):

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

    worldX = "0"
    worldY = "0"
    worldZ = "0"
    world = [0., 0., 0.]

    CoordinateSystemName = "IJK"
    dimensionality = 3

    layerLogicCalls = (('B', sliceLogic.GetBackgroundLayer),
                      ('F', sliceLogic.GetForegroundLayer),
                      ('L', sliceLogic.GetLabelLayer))
    for layer,logicCall in layerLogicCalls:
      layerLogic = logicCall()
      volumeNode = layerLogic.GetVolumeNode()
      if volumeNode and (volumeNode.IsA("vtkMRMLAstroVolumeNode") or volumeNode.IsA("vtkMRMLAstroLabelMapVolumeNode")):
        dimensionality = int(volumeNode.GetAttribute("SlicerAstro.NAXIS"))
        xyToIJK = layerLogic.GetXYToIJKTransform()
        ijkFloat = xyToIJK.TransformDoublePoint(xyz)
        displayNode = volumeNode.GetDisplayNode()
        if displayNode:
          CoordinateSystemName = displayNode.GetSpace()
          displayNode.GetReferenceSpace(ijkFloat, world)
          worldX = displayNode.GetPythonDisplayStringFromValueX(world[0], 3)
          worldY = displayNode.GetPythonDisplayStringFromValueY(world[1], 3)
          worldZ = displayNode.GetPythonDisplayStringFromValueZ(world[2], 3)
          worldZ = displayNode.AddVelocityInfoToDisplayStringZ(worldZ)
          break

    if CoordinateSystemName == "WCS":
      if dimensionality > 2:
        return " {layoutName: <8s} {sys:s}[{orient:s}]:{worldX:>16s},{worldY:>16s},{worldZ:>10s}" \
                .format(layoutName=sliceNode.GetLayoutName(),
                sys = CoordinateSystemName,
                orient=sliceNode.GetOrientation(),
                worldX=worldX,
                worldY=worldY,
                worldZ=worldZ,
                )
      elif dimensionality > 1:
        return " {layoutName: <8s} {sys:s}[{orient:s}]:{worldX:>16s},{worldY:>16s}" \
                .format(layoutName=sliceNode.GetLayoutName(),
                sys = CoordinateSystemName,
                orient=sliceNode.GetOrientation(),
                worldX=worldX,
                worldY=worldY,
                )
      else:
        return " {layoutName: <8s} {sys:s}[{orient:s}]:{worldX:>16s}" \
                .format(layoutName=sliceNode.GetLayoutName(),
                sys = CoordinateSystemName,
                orient=sliceNode.GetOrientation(),
                worldX=worldX,
                )
    else:
      return " {layoutName: <8s} WCS in View {orient: >2s} not found" \
        .format(layoutName=sliceNode.GetLayoutName(),
                orient=sliceNode.GetOrientation(),
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
    displayNode = volumeNode.GetDisplayNode()
    return "<b>%s</b>" % displayNode.GetPixelString(ijk) if displayNode else ""
