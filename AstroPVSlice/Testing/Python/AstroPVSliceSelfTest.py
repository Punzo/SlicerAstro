import math

import numpy as np
import slicer
import vtk
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


#
# AstroPVSliceSelfTest
#

class AstroPVSliceSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro PVSlice SelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroPVSlice"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = "Self test of the AstroPVSlice module (logic and panel)."
        self.parent.acknowledgementText = ""


class AstroPVSliceSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


def voxelArray(volume):
    from vtk.util.numpy_support import vtk_to_numpy
    dims = volume.GetImageData().GetDimensions()
    return vtk_to_numpy(volume.GetImageData().GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def lineEndsIJK(line, volume):
    """Ends of the line in the IJK coordinates of a volume."""
    rasToIJK = vtk.vtkMatrix4x4()
    volume.GetRASToIJKMatrix(rasToIJK)
    ends = []
    for index in range(2):
        ras = [0.0, 0.0, 0.0]
        line.GetNthControlPointPosition(index, ras)
        ends.append(np.array(rasToIJK.MultiplyPoint(ras + [1.0])[:3]))
    return ends


class AstroPVSliceSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_AstroPVSliceSelfTest()

    def process(self, count=20):
        for _ in range(count):
            slicer.app.processEvents()

    def test_AstroPVSliceSelfTest(self):
        import AstroSampleData

        self.delayDisplay("Loading WEIN069")
        cube = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(cube)
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(cube.GetID())
        slicer.util.selectModule("AstroPVSlice")
        self.process(40)
        widget = slicer.modules.astropvslice.widgetRepresentation()
        logic = slicer.modules.astropvslice.logic()
        parameters = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroPVSliceParametersNode")
        self.assertEqual(parameters.GetInputVolumeNodeID(), cube.GetID())

        # the moment-0 map of the cube shared by the modules
        self.delayDisplay("Moment-0 map")
        moment = slicer.mrmlScene.GetNodeByID(parameters.GetMomentMapNodeID())
        self.assertIsNotNone(moment)
        self.assertEqual(moment.GetName(), cube.GetName() + "_mom0")
        self.assertIs(slicer.vtkSlicerAstroVolumeLogic.GetMomentMap(cube, 0), moment)
        line = slicer.mrmlScene.GetNodeByID(parameters.GetLineNodeID())
        self.assertIsNotNone(line)
        self.assertEqual(line.GetNumberOfDefinedControlPoints(), 2)

        # the line on the sky: set and read back
        self.delayDisplay("Line on the sky")
        sky = list(widget.lineSky())
        self.assertEqual(len(sky), 4)
        target = [sky[0] + 0.002, sky[1] - 0.001, 35.0, 8.0]
        widget.setLineSky(*target)
        self.process()
        result = list(widget.lineSky())
        self.assertAlmostEqual(result[0], target[0], delta=1e-4)
        self.assertAlmostEqual(result[1], target[1], delta=1e-4)
        self.assertAlmostEqual(result[2], target[2], delta=0.05)
        self.assertAlmostEqual(result[3], target[3], delta=1e-3 * target[3])
        # the panel fields follow the line
        self.assertAlmostEqual(slicer.util.findChild(widget, "PVSlicePositionAngle").value, result[2], places=1)
        self.assertAlmostEqual(slicer.util.findChild(widget, "PVSliceLength").value, result[3], places=2)

        # centre on the peak of the map
        self.delayDisplay("Centre on peak")
        values = voxelArray(moment)[0].astype(np.float64)
        finite = np.where(np.isfinite(values), values, -np.inf)
        peakJ, peakI = np.unravel_index(np.argmax(finite), finite.shape)
        widget.centerOnPeak()
        self.process()
        end1, end2 = lineEndsIJK(line, moment)
        centre = 0.5 * (end1 + end2)
        self.assertLess(abs(centre[0] - peakI), 0.6, (centre, peakI, peakJ))
        self.assertLess(abs(centre[1] - peakJ), 0.6, (centre, peakI, peakJ))
        self.assertAlmostEqual(list(widget.lineSky())[2], target[2], delta=0.05)

        # major axis: second moments of the pixels above 20% of the peak
        self.delayDisplay("Major axis")
        widget.alignWithMajorAxis()
        self.process()
        peak = np.nanmax(values)
        jj, ii = np.nonzero(np.isfinite(values) & (values > 0.2 * peak))
        weights = values[jj, ii]
        mi = np.sum(weights * ii) / weights.sum()
        mj = np.sum(weights * jj) / weights.sum()
        covariance = np.cov(np.vstack((ii - mi, jj - mj)), aweights=weights, bias=True)
        eigenvalues, eigenvectors = np.linalg.eigh(covariance)
        major = eigenvectors[:, np.argmax(eigenvalues)]
        end1, end2 = lineEndsIJK(line, moment)
        direction = (end2 - end1)[:2]
        direction /= np.linalg.norm(direction)
        self.assertGreater(abs(float(np.dot(direction, major))), 0.999, (direction, major))
        centre = 0.5 * (end1 + end2)
        self.assertLess(abs(centre[0] - mi), 0.6)
        self.assertLess(abs(centre[1] - mj), 0.6)

        # a line moved off the plane of the map (e.g. dragged in 3D) is put
        # back on it
        self.delayDisplay("Line kept on the map")
        origin = moment.GetOrigin()
        position = [0.0, 0.0, 0.0]
        line.GetNthControlPointPosition(0, position)
        line.SetNthControlPointPosition(0, position[0], position[1] + 7.0, position[2])
        self.process()
        line.GetNthControlPointPosition(0, position)
        planeY = vtk.vtkMatrix4x4()
        moment.GetIJKToRASMatrix(planeY)
        self.assertAlmostEqual(position[1], planeY.MultiplyPoint([0, 0, 0, 1])[1], delta=0.01)

        # the PV slice (yellow view): the plane of the line and of the
        # velocity axis
        self.delayDisplay("PV slice")
        yellow = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow")
        sliceToRAS = yellow.GetSliceToRAS()
        normal = np.array([sliceToRAS.GetElement(row, 2) for row in range(3)])
        normal /= np.linalg.norm(normal)
        start, end = [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]
        line.GetNthControlPointPosition(0, start)
        line.GetNthControlPointPosition(1, end)
        along = np.array(end) - np.array(start)
        along /= np.linalg.norm(along)
        self.assertLess(abs(float(np.dot(normal, along))), 1e-3, (normal, along))
        self.assertLess(abs(normal[1]), 1e-3, normal)

        # cameras of the 3D view: sky, along the line, facing the PV plane
        self.delayDisplay("3D view")
        camera = slicer.vtkSlicerAstroVolumeLogic.GetThreeDViewCameraNode(slicer.mrmlScene, "vtkMRMLViewNode1")
        self.assertIsNotNone(camera)
        alongSky = np.array([along[0], 0.0, along[2]])
        alongSky /= np.linalg.norm(alongSky)
        for view, check in ((0, lambda d: abs(d[1]) > 0.999),
                            (1, lambda d: abs(float(np.dot(d, alongSky))) > 0.999),
                            (2, lambda d: abs(float(np.dot(d, alongSky))) < 1e-3 and abs(d[1]) < 1e-3)):
            widget.setCameraView(view)
            self.process()
            projection = np.array(camera.GetCamera().GetDirectionOfProjection())
            self.assertTrue(check(projection), (view, projection))
            self.assertEqual(slicer.util.findChild(widget, "PVSliceCamera").currentIndex, view)

        # the Moment maps module uses the same map
        self.assertEqual(len([node for node in slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode")
                              if node.GetName() == cube.GetName() + "_mom0"]), 1)
        self.delayDisplay("Test passed")
