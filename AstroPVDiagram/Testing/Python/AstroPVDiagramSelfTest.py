import math

import numpy as np

import slicer
import vtk
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroPVDiagramSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro PVDiagram SelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroPVDiagram"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Self test of the AstroPVDiagram module: the position-velocity diagram along a
polyline is compared with an independent (numpy) bilinear interpolation of the
data cube along the same path.
"""
        self.parent.hidden = True


class AstroPVDiagramSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroPVDiagramSelfTestLogic(ScriptedLoadableModuleLogic):
    pass


def linearPath(controlPointsIJK):
    """Sampling of the line (linear curve through the control points, IJK of
    the moment map) as done by vtkSlicerAstroPVDiagramLogic: one sample every
    pixel along the line, and the end of the line."""
    points = [(float(p[0]), float(p[1])) for p in controlPointsIJK]
    path = [[points[0][0], points[0][1], 0.0]]
    toNext = 1.0
    last = points[0]
    for (x0, y0), (x1, y1) in zip(points[:-1], points[1:]):
        length = math.hypot(x1 - x0, y1 - y0)
        along = 0.0
        while length - along >= toNext:
            along += toNext
            fraction = along / length
            last = (x0 + (x1 - x0) * fraction, y0 + (y1 - y0) * fraction)
            path.append([last[0], last[1], 0.0])
            toNext = 1.0
        toNext -= length - along
    end = points[-1]
    if math.hypot(end[0] - last[0], end[1] - last[1]) > 0.5:
        path.append([end[0], end[1], 0.0])
    return path


def bilinearPV(cube, path):
    """PV diagram (channels x path samples) by bilinear interpolation of each channel."""
    nz, ny, nx = cube.shape
    pv = np.zeros((nz, len(path)), dtype=np.float64)
    for index, (x, y, _) in enumerate(path):
        left = min(max(int(math.floor(x)), 0), nx - 1)
        right = min(left + 1, nx - 1)
        wRight = 0.0 if right == left else x - left
        bottom = min(max(int(math.floor(y)), 0), ny - 1)
        top = min(bottom + 1, ny - 1)
        wTop = 0.0 if top == bottom else y - bottom
        topRow = cube[:, top, left] * (1 - wRight) + cube[:, top, right] * wRight
        bottomRow = cube[:, bottom, left] * (1 - wRight) + cube[:, bottom, right] * wRight
        pv[:, index] = topRow * wTop + bottomRow * (1 - wTop)
    return pv


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of an (astro) volume node."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


class AstroPVDiagramSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_AstroPVDiagramSelfTest()

    def test_AstroPVDiagramSelfTest(self):
        import AstroSampleData

        self.delayDisplay("Loading WEIN069")
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)

        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroPVDiagram")

        parametersNode = slicer.util.getNode("AstroPVDiagramParameters")
        parametersNode.SetInterpolation(False)
        # the line of the PV diagram is a Markups open curve (linear)
        sourceNode = slicer.util.getNode("PVDiagramLine")
        self.assertTrue(sourceNode.IsA("vtkMRMLMarkupsCurveNode"))
        self.assertEqual(sourceNode.GetCurveType(), slicer.vtkCurveGenerator.CURVE_TYPE_LINEAR)
        self.assertNotIn("vtkMRMLSliceNodeYellow", sourceNode.GetDisplayNode().GetViewNodeIDs())

        self.delayDisplay("Adding source points and calculating the PV diagram")
        sourceNode.AddControlPoint(0, 0, 0)
        sourceNode.AddControlPoint(30, 0, 0)
        sourceNode.AddControlPoint(30, 0, 40)
        slicer.app.processEvents()

        outputVolume = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        self.assertIsNotNone(outputVolume)
        momentMap = slicer.mrmlScene.GetNodeByID(parametersNode.GetMomentMapNodeID())
        self.assertIsNotNone(momentMap)

        # Control points (after the module constrained them on the moment map) in IJK
        rasToIJK = vtk.vtkMatrix4x4()
        momentMap.GetRASToIJKMatrix(rasToIJK)
        controlPointsIJK = []
        for index in range(sourceNode.GetNumberOfControlPoints()):
            ras = [0.0, 0.0, 0.0]
            sourceNode.GetNthControlPointPosition(index, ras)
            controlPointsIJK.append(rasToIJK.MultiplyPoint(ras + [1.0])[:3])

        cube = voxelArray(volumeNode)
        expected = bilinearPV(cube.astype(np.float64), linearPath(controlPointsIJK))
        # PV diagram image: (path samples, channels, 1)
        pvArray = voxelArray(outputVolume)
        self.assertEqual(pvArray.shape, (1,) + expected.shape)
        computed = pvArray[0]
        scale = np.nanmax(np.abs(cube))
        self.assertTrue(np.allclose(computed, expected, atol=1e-6 * scale, equal_nan=True),
                        "PV diagram differs from the reference interpolation, max abs diff %g"
                        % np.nanmax(np.abs(computed - expected)))

        # the Green view keeps the 2nd moment map in XY
        greenSlice = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeGreen")
        self.assertEqual(greenSlice.GetOrientation(), "XY")

        # spline interpolation: the curve type of the line
        parametersNode.SetInterpolation(True)
        slicer.app.processEvents()
        self.assertEqual(sourceNode.GetCurveType(), slicer.vtkCurveGenerator.CURVE_TYPE_CARDINAL_SPLINE)

        self.delayDisplay("Test passed")
