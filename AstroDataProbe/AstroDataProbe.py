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
from math import fabs
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
        displayNode = volumeNode.GetDisplayNode()
        if displayNode:
          CoordinateSystemName = displayNode.GetSpace()
          displayNode.GetReferenceSpace(ijkFloat, world)
          worldX = displayNode.GetDisplayStringFromValueX(world[0])
          worldY = displayNode.GetDisplayStringFromValueY(world[1])
          worldZ = displayNode.GetDisplayStringFromValueZ(world[2])
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
      displayNode = volumeNode.GetDisplayNode()
      return "<b>%s</b>" % displayNode.GetPixelString(ijk) if displayNode else ""
    else:
      return "<b>%s</b>" % self.getPixelString(volumeNode,ijk) if volumeNode else ""


def updateAstroRuler(self, sliceLogic):
  sliceCompositeNode = sliceLogic.GetSliceCompositeNode()
  # Get the layers
  sliceNode = sliceLogic.GetBackgroundLayer().GetSliceNode()
  if not sliceNode:
    return
  sliceViewName = sliceNode.GetLayoutName()
  renderer = self.renderers[sliceViewName]
  if self.rulerEnabled:
    self.makeRuler(sliceNode)
  else:
    renderer.RemoveActor2D(self.rulerActors[sliceViewName])
    Actors = renderer.GetActors2D()
    Actors.InitTraversal()
    for i in range(0, Actors.GetNumberOfItems()):
      Actor2D = Actors.GetNextActor2D()
      if(Actor2D.IsA("vtkTextActor")):
        renderer.RemoveActor2D(Actor2D)


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

    CoordinateSystemName = "IJK"
    hasVolume = False

    if sliceLogic:

      layerLogicCalls = (('B', sliceLogic.GetBackgroundLayer),
                       ('F', sliceLogic.GetForegroundLayer),
                       ('L', sliceLogic.GetLabelLayer))

      for layer,logicCall in layerLogicCalls:
        layerLogic = logicCall()

        volumeNode = layerLogic.GetVolumeNode()
        if volumeNode:
          hasVolume = True
          xyToIJK = layerLogic.GetXYToIJKTransform()
          displayNode = volumeNode.GetDisplayNode()
          if displayNode:
            if(displayNode.GetSpace() == "WCS"):

              viewWidth = self.sliceViews[sliceViewName].width
              numberOfPointsHorizontal = [0]
              numberOfPointsHorizontal[0] = (int) (viewWidth / 120.) + 1

              viewHeight = self.sliceViews[sliceViewName].height
              numberOfPointsVertical = [0]
              numberOfPointsVertical[0] = (int) (viewHeight / 100.) + 1

              worldA = [0., 0., 0.]
              worldB = [0., 0., 0.]
              worldC = [0., 0., 0.]
              world = []
              xyzDisplay = []
              axisCoord = [0., 0.]
              wcsStep = [0., 0.,]

              xyz = [0, 0, 0]
              ijkFloat = xyToIJK.TransformDoublePoint(xyz)
              displayNode.GetReferenceSpace(ijkFloat, worldA)
              xyz = [viewWidth, 0, 0]
              ijkFloat = xyToIJK.TransformDoublePoint(xyz)
              displayNode.GetReferenceSpace(ijkFloat, worldB)
              xyz = [0, viewHeight, 0]
              ijkFloat = xyToIJK.TransformDoublePoint(xyz)
              displayNode.GetReferenceSpace(ijkFloat, worldC)

              # calculate the wcsSteps for the two axes
              if(sliceNode.GetOrientationString() == "Sagittal"):
                wcsStep[0] = displayNode.GetWcsTickStepAxisZ(fabs(worldA[2] - worldB[2]), numberOfPointsHorizontal)
                axisCoord[0] = displayNode.GetFirstWcsTickAxisZ(worldA[2], worldB[2], wcsStep[0])
                wcsStep[1] = displayNode.GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), numberOfPointsVertical)
                axisCoord[1] = displayNode.GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1])

              if(sliceNode.GetOrientationString() == "Coronal"):
                wcsStep[0] = displayNode.GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), numberOfPointsHorizontal)
                axisCoord[0] = displayNode.GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0])
                wcsStep[1] = displayNode.GetWcsTickStepAxisY(fabs(worldA[1] - worldC[1]), numberOfPointsVertical)
                axisCoord[1] = displayNode.GetFirstWcsTickAxisY(worldA[1], worldC[1], wcsStep[1])

              if(sliceNode.GetOrientationString() == "Axial"):
                wcsStep[0] = displayNode.GetWcsTickStepAxisX(fabs(worldA[0] - worldB[0]), numberOfPointsHorizontal)
                axisCoord[0] = displayNode.GetFirstWcsTickAxisX(worldA[0], worldB[0], wcsStep[0])
                wcsStep[1] = displayNode.GetWcsTickStepAxisZ(fabs(worldA[2] - worldC[2]), numberOfPointsVertical)
                axisCoord[1] = displayNode.GetFirstWcsTickAxisZ(worldA[2], worldC[2], wcsStep[1])

              #allocate point along the horizontal axes
              pts = vtk.vtkPoints()
              for i in range(numberOfPointsHorizontal[0]):

                i8 = i * 8
                if(sliceNode.GetOrientationString() == "Sagittal"):
                  world.append([worldA[0], worldA[1], axisCoord[0] + wcsStep[0] * i])
                if(sliceNode.GetOrientationString() == "Coronal" or sliceNode.GetOrientationString() == "Axial"):
                  world.append([axisCoord[0] + wcsStep[0] * i, worldA[1], worldA[2]])
                ijk = [0., 0., 0.]
                displayNode.GetIJKSpace(world[i], ijk)
                xyToIJK.Inverse()
                xyz = xyToIJK.TransformDoublePoint(ijk)
                xyzDisplay.append(xyz)
                pts.InsertPoint(i8, [xyz[0], 2, 0])
                pts.InsertPoint(i8 + 1, [xyz[0], 12, 0])
                if(sliceNode.GetOrientationString() == "Sagittal"):
                  displayNode.GetIJKSpace([worldA[0], worldA[1], axisCoord[0] \
                            + (wcsStep[0] * i + wcsStep[0] / 4.)], ijk)
                if(sliceNode.GetOrientationString() == "Coronal" or sliceNode.GetOrientationString() == "Axial"):
                    displayNode.GetIJKSpace([axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 4.), \
                                            worldA[1], worldA[2]], ijk)
                xyz = xyToIJK.TransformDoublePoint(ijk)
                pts.InsertPoint(i8 + 2, [xyz[0], 2, 0])
                pts.InsertPoint(i8 + 3, [xyz[0], 7, 0])
                if(sliceNode.GetOrientationString() == "Sagittal"):
                  displayNode.GetIJKSpace([worldA[0], worldA[1], axisCoord[0] \
                            + (wcsStep[0] * i + wcsStep[0] / 2.)], ijk)
                if(sliceNode.GetOrientationString() == "Coronal" or sliceNode.GetOrientationString() == "Axial"):
                  displayNode.GetIJKSpace([axisCoord[0] + (wcsStep[0] * i + wcsStep[0] / 2.), \
                                           worldA[1], worldA[2]], ijk)

                xyz = xyToIJK.TransformDoublePoint(ijk)
                pts.InsertPoint(i8 + 4, [xyz[0], 2, 0])
                pts.InsertPoint(i8 + 5, [xyz[0], 7, 0])
                if(sliceNode.GetOrientationString() == "Sagittal"):
                  displayNode.GetIJKSpace([worldA[0], worldA[1], axisCoord[0] \
                            + (wcsStep[0] * i + wcsStep[0] * 3. / 4.)], ijk)
                if(sliceNode.GetOrientationString() == "Coronal" or sliceNode.GetOrientationString() == "Axial"):
                  displayNode.GetIJKSpace([axisCoord[0] + (wcsStep[0] * i + wcsStep[0] *3. / 4.), \
                                           worldA[1], worldA[2]], ijk)
                xyz = xyToIJK.TransformDoublePoint(ijk)
                pts.InsertPoint(i8 + 6, [xyz[0], 2, 0])
                pts.InsertPoint(i8 + 7, [xyz[0], 7, 0])
                xyToIJK.Inverse()

              nTot = numberOfPointsVertical[0] + numberOfPointsHorizontal[0]


              #allocate point along the vertical axes
              for i in range(numberOfPointsHorizontal[0], nTot):
                i8 = i * 8
                ii = i - numberOfPointsHorizontal[0]
                if(sliceNode.GetOrientationString() == "Sagittal" or sliceNode.GetOrientationString() == "Coronal"):
                  world.append([worldA[0], axisCoord[1] + wcsStep[1] * ii, worldA[2]])
                if(sliceNode.GetOrientationString() == "Axial"):
                  world.append([worldA[0], worldA[1], axisCoord[1] + wcsStep[1] * ii])
                ijk = [0., 0., 0.]
                displayNode.GetIJKSpace(world[i], ijk)
                xyToIJK.Inverse()
                xyz = xyToIJK.TransformDoublePoint(ijk)
                xyzDisplay.append(xyz)
                pts.InsertPoint(i8, [2, xyz[1], 0])
                pts.InsertPoint(i8 + 1, [12, xyz[1], 0])
                if(sliceNode.GetOrientationString() == "Sagittal" or sliceNode.GetOrientationString() == "Coronal"):
                  displayNode.GetIJKSpace([worldA[0], axisCoord[1] \
                            + (wcsStep[1] * ii + wcsStep[1] / 4.), worldA[2]], ijk)
                if(sliceNode.GetOrientationString() == "Axial"):
                  displayNode.GetIJKSpace([worldA[0], worldA[1], axisCoord[1] \
                            + (wcsStep[1] * ii + wcsStep[1] / 4.)], ijk)
                xyz = xyToIJK.TransformDoublePoint(ijk)
                pts.InsertPoint(i8 + 2, [2, xyz[1], 0])
                pts.InsertPoint(i8 + 3, [7, xyz[1], 0])
                if(sliceNode.GetOrientationString() == "Sagittal" or sliceNode.GetOrientationString() == "Coronal"):
                  displayNode.GetIJKSpace([worldA[0], axisCoord[1] \
                            + (wcsStep[1] * ii + wcsStep[1] / 2.), worldA[2]], ijk)
                if(sliceNode.GetOrientationString() == "Axial"):
                  displayNode.GetIJKSpace([worldA[0], worldA[1], axisCoord[1] \
                            + (wcsStep[1] * ii + wcsStep[1] / 2.)], ijk)
                xyz = xyToIJK.TransformDoublePoint(ijk)
                pts.InsertPoint(i8 + 4, [2, xyz[1], 0])
                pts.InsertPoint(i8 + 5, [7, xyz[1], 0])
                if(sliceNode.GetOrientationString() == "Sagittal" or sliceNode.GetOrientationString() == "Coronal"):
                  displayNode.GetIJKSpace([worldA[0], axisCoord[1] \
                            + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.), worldA[2]], ijk)
                if(sliceNode.GetOrientationString() == "Axial"):
                  displayNode.GetIJKSpace([worldA[0], worldA[1], axisCoord[1] \
                            + (wcsStep[1] * ii + wcsStep[1] * 3. / 4.)], ijk)
                xyz = xyToIJK.TransformDoublePoint(ijk)
                pts.InsertPoint(i8 + 6, [2, xyz[1], 0])
                pts.InsertPoint(i8 + 7, [7, xyz[1], 0])
                xyToIJK.Inverse()

              n = pts.GetNumberOfPoints()

              # unify the points with lines
              lines = []
              for i in xrange(0, n-1):
                line = vtk.vtkLine()
                lines.append(line)

              nHori8 = numberOfPointsHorizontal[0] * 8
              for i in xrange(0, nHori8 -1):
                if (i%2 == 0):
                  lines[i].GetPointIds().SetId(0,i)
                  lines[i].GetPointIds().SetId(1,i+1)
                else:
                  lines[i].GetPointIds().SetId(0,i-1)
                  lines[i].GetPointIds().SetId(1,i+1)

              nVert8 = numberOfPointsVertical[0] * 8
              nTot8 = nHori8 + nVert8
              for i in xrange(nHori8, nTot8 - 1):
                if (i%2 == 0):
                  lines[i].GetPointIds().SetId(0,i)
                  lines[i].GetPointIds().SetId(1,i+1)
                else:
                  lines[i].GetPointIds().SetId(0,i-1)
                  lines[i].GetPointIds().SetId(1,i+1)

              # create the cellArray
              linesArray = vtk.vtkCellArray()
              for i in xrange(0, n-1):
                linesArray.InsertNextCell(lines[i])
              linesPolyData = vtk.vtkPolyData()
              linesPolyData.SetPoints(pts)
              linesPolyData.SetLines(linesArray)

              # setup the mapper
              mapper = actor.GetMapper()

              if vtk.VTK_MAJOR_VERSION <= 5:
                mapper.SetInput(linesPolyData)
              else:
                mapper.SetInputData(linesPolyData)

              actor.SetMapper(mapper)
              actor.GetProperty().SetLineWidth(2)

              renderer.AddActor2D(self.rulerActors[sliceViewName])

              #setup the 2DActros
              Actors = renderer.GetActors2D()
              Actors.InitTraversal()
              for i in range(0, Actors.GetNumberOfItems()):
                Actor2D = Actors.GetNextActor2D()
                if(Actor2D.IsA("vtkTextActor")):
                  renderer.RemoveActor2D(Actor2D)

              textActor = self.rulerTextActors[sliceViewName]
              textProperty = textActor.GetTextProperty()
              textProperty.SetFontSize(self.fontSize)
              if self.fontFamily == 'Times':
                textProperty.SetFontFamilyToTimes()
              else:
                textProperty.SetFontFamilyToArial()

              textActor.SetInput("")

              # allocate 2DTextActors for the horizontal axes
              for i in range(1, numberOfPointsHorizontal[0]):
                if(xyzDisplay[i][0] < 50 or xyzDisplay[i][0] > viewWidth - 50):
                  continue
                if(sliceNode.GetOrientationString() == "Sagittal"):
                  coord = displayNode.GetAxisDisplayStringFromValueZ(world[i][2])
                if(sliceNode.GetOrientationString() == "Coronal" or sliceNode.GetOrientationString() == "Axial"):
                  coord = displayNode.GetAxisDisplayStringFromValueX(world[i][0])
                textActorHorizontal = vtk.vtkTextActor()
                textActorHorizontal.SetTextProperty(textActor.GetTextProperty())
                textActorHorizontal.SetInput(coord)
                textActorHorizontal.SetDisplayPosition((int) (xyzDisplay[i][0]-40), 15)
                renderer.AddActor2D(textActorHorizontal)

              # allocate 2DTextActors for the vertical axes
              for i in range(numberOfPointsHorizontal[0], nTot):
                if (xyzDisplay[i][1] < 50 or xyzDisplay[i][1] > viewHeight):
                  continue
                if(sliceNode.GetOrientationString() == "Sagittal" or \
                  sliceNode.GetOrientationString() == "Coronal"):
                  coord = displayNode.GetAxisDisplayStringFromValueY(world[i][1])
                if(sliceNode.GetOrientationString() == "Axial"):
                    coord = displayNode.GetAxisDisplayStringFromValueZ(world[i][2])
                textActorVertical = vtk.vtkTextActor()
                textActorVertical.SetTextProperty(textActor.GetTextProperty())
                textActorVertical.SetInput(coord)
                textActorVertical.SetDisplayPosition(20, (int) (xyzDisplay[i][1]-5))
                renderer.AddActor2D(textActorVertical)

              break

        if(not hasVolume):
          renderer.RemoveActor2D(self.rulerActors[sliceViewName])
          Actors = renderer.GetActors2D()
          Actors.InitTraversal()
          for i in range(0, Actors.GetNumberOfItems()):
            Actor2D = Actors.GetNextActor2D()
            if(Actor2D.IsA("vtkTextActor")):
              renderer.RemoveActor2D(Actor2D)
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

      uncType = type(dataProbeInstance.infoWidget.sliceAnnotations.updateRuler)
      dataProbeInstance.infoWidget.sliceAnnotations.updateRuler = funcType(updateAstroRuler, dataProbeInstance.infoWidget.sliceAnnotations, DataProbeInfoWidget)

class AstroDataProbeTest(ScriptedLoadableModuleTest):

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    self.setUp()
