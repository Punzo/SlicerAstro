#!/usr/bin/env python
# -*- coding: utf-8 -*-

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

    layerLogicCalls = (('B', sliceLogic.GetBackgroundLayer),
                   ('F', sliceLogic.GetForegroundLayer),
                   ('L', sliceLogic.GetLabelLayer))
    for layer,logicCall in layerLogicCalls:
      layerLogic = logicCall()
      volumeNode = layerLogic.GetVolumeNode()
      if volumeNode:
        xyToIJK = layerLogic.GetXYToIJKTransform()
        ijkFloat = xyToIJK.TransformDoublePoint(xyz)
        display = volumeNode.GetDisplayNode()
        if display:
          CoordinateSystemName = display.GetSpace()
          volumeNode.GetReferenceSpace(ijkFloat, CoordinateSystemName, world)
          worldX = display.GetDisplayStringFromValueX(world[0])
          worldY = display.GetDisplayStringFromValueY(world[1])
          worldZ = display.GetDisplayStringFromValueZ(world[2])
      break


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
  actor = self.rulerActors[sliceViewName]

  if self.sliceViews[sliceViewName]:
    #
    # update scaling ruler
    #

    sliceLogic = None
    if sliceNode:
      appLogic = slicer.app.applicationLogic()
      if appLogic:
        sliceLogic = appLogic.GetSliceLogic(sliceNode)

    world = [0., 0., 0.]
    CoordinateSystemName = "IJK"

    if sliceLogic:

      layerLogicCalls = (('B', sliceLogic.GetBackgroundLayer),
                       ('F', sliceLogic.GetForegroundLayer),
                       ('L', sliceLogic.GetLabelLayer))
      for layer,logicCall in layerLogicCalls:
        layerLogic = logicCall()
        volumeNode = layerLogic.GetVolumeNode()
        if volumeNode:
          xyToIJK = layerLogic.GetXYToIJKTransform()
          display = volumeNode.GetDisplayNode()
          if display:
            CoordinateSystemName = display.GetSpace()
            viewWidth = self.sliceViews[sliceViewName].width
            pts = self.points[sliceViewName]
            pts.Resize(0)
            pts.Squeeze()
            pts.InsertPoint(0,[10,5, 0])
            pts.InsertPoint(1,[10,15, 0])
            pts.InsertPoint(2,[30,5, 0])
            pts.InsertPoint(3,[30,10, 0])

            n = pts.GetNumberOfPoints()

            lines = []
            for i in xrange(0, n-1):
              line = vtk.vtkLine()
              lines.append(line)

            for i in xrange(0, n-1):
              if (i%2 == 0):
                lines[i].GetPointIds().SetId(0,i)
                lines[i].GetPointIds().SetId(1,i+1)
              else:
                lines[i].GetPointIds().SetId(0,i-1)
                lines[i].GetPointIds().SetId(1,i+1)

            linesArray = vtk.vtkCellArray()
            for i in xrange(0, n-1):
              linesArray.InsertNextCell(lines[i])
            linesPolyData = vtk.vtkPolyData()
            linesPolyData.SetPoints(pts)
            linesPolyData.SetLines(linesArray)

            mapper = actor.GetMapper()
            if vtk.VTK_MAJOR_VERSION <= 5:
              mapper.SetInput(linesPolyData)
            else:
              mapper.SetInputData(linesPolyData)

            actor.SetMapper(mapper)
            actor.GetProperty().SetLineWidth(2)

            textActor = self.rulerTextActors[sliceViewName]
            xyz = [10, 0, 0]
            ijkFloat = xyToIJK.TransformDoublePoint(xyz)
            volumeNode.GetReferenceSpace(ijkFloat, CoordinateSystemName, world)
            worldX = "0"
            if(sliceNode.GetOrientationString() == "Coronal"):
           #   worldX = display.GetDisplayStringFromValueX(world[0])
           #   print "({worldX:>16s}) ".format(worldX=worldX)
           #   print "bella"
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
            textActor.SetDisplayPosition(10, 5)

            renderer.AddActor2D(self.rulerActors[sliceViewName])
            renderer.RemoveActor2D(textActor)
            renderer.AddActor2D(textActor)
        break

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
