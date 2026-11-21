import math

import numpy as np

import qt
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
path (straight segments, spline, with a width) is compared with an independent
(numpy) interpolation of the data cube along the same path; the panel (path
drawn on the map preview, shape, width, result) and the views.
"""
        self.parent.hidden = True


class AstroPVDiagramSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroPVDiagramSelfTestLogic(ScriptedLoadableModuleLogic):
    pass


def resample(pointsIJ):
    """Samples one pixel apart along a polyline (IJ of the moment map) and its
    end, as done by vtkSlicerAstroPVDiagramLogic."""
    points = [(float(p[0]), float(p[1])) for p in pointsIJ]
    path = [points[0]]
    toNext = 1.0
    last = points[0]
    for (x0, y0), (x1, y1) in zip(points[:-1], points[1:]):
        length = math.hypot(x1 - x0, y1 - y0)
        along = 0.0
        while length - along >= toNext:
            along += toNext
            fraction = along / length
            last = (x0 + (x1 - x0) * fraction, y0 + (y1 - y0) * fraction)
            path.append(last)
            toNext = 1.0
        toNext -= length - along
    end = points[-1]
    if math.hypot(end[0] - last[0], end[1] - last[1]) > 0.5:
        path.append(end)
    return path


def bilinear(cube, x, y):
    """Spectrum at (x, y) by bilinear interpolation; neighbours with a zero
    weight do not count (a blank one does not blank the value); None outside."""
    nz, ny, nx = cube.shape
    if x < -1e-6 or y < -1e-6 or x > nx - 1 + 1e-6 or y > ny - 1 + 1e-6:
        return None
    x = min(max(x, 0.0), nx - 1.0)
    y = min(max(y, 0.0), ny - 1.0)
    left, bottom = min(int(math.floor(x)), nx - 1), min(int(math.floor(y)), ny - 1)
    right, top = min(left + 1, nx - 1), min(bottom + 1, ny - 1)
    wx = 0.0 if right == left else x - left
    wy = 0.0 if top == bottom else y - bottom
    value = np.zeros(nz)
    for (i, j, w) in ((left, bottom, (1 - wx) * (1 - wy)), (right, bottom, wx * (1 - wy)),
                      (left, top, (1 - wx) * wy), (right, top, wx * wy)):
        if w > 0:
            value = value + cube[:, j, i] * w
    return value


def referencePV(cube, samples, width=1):
    """PV diagram (channels x columns): mean of the finite spectra at width
    positions across the path, one pixel apart along its normal."""
    n = len(samples)
    pv = np.full((cube.shape[0], n), np.nan)
    for c in range(n):
        before, after = samples[max(c - 1, 0)], samples[min(c + 1, n - 1)]
        tx, ty = after[0] - before[0], after[1] - before[1]
        norm = math.hypot(tx, ty)
        if norm > 0:
            tx, ty = tx / norm, ty / norm
        nxv, nyv = -ty, tx
        values = []
        for a in range(width):
            offset = a - 0.5 * (width - 1)
            spectrum = bilinear(cube, samples[c][0] + offset * nxv, samples[c][1] + offset * nyv)
            if spectrum is not None:
                values.append(spectrum)
        if values:
            stack = np.array(values)
            finite = np.isfinite(stack)
            count = finite.sum(axis=0)
            total = np.where(finite, stack, 0.0).sum(axis=0)
            pv[:, c] = np.where(count > 0, total / np.maximum(count, 1), np.nan)
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

    def controlPointsIJ(self, path, momentMap):
        rasToIJK = vtk.vtkMatrix4x4()
        momentMap.GetRASToIJKMatrix(rasToIJK)
        points = []
        for index in range(path.GetNumberOfControlPoints()):
            ras = [0.0, 0.0, 0.0]
            path.GetNthControlPointPosition(index, ras)
            points.append(rasToIJK.MultiplyPoint(ras + [1.0])[:2])
        return points

    def curveSamplesIJ(self, path, momentMap):
        rasToIJK = vtk.vtkMatrix4x4()
        momentMap.GetRASToIJKMatrix(rasToIJK)
        curvePoints = path.GetCurvePointsWorld()
        return [rasToIJK.MultiplyPoint(list(curvePoints.GetPoint(i)) + [1.0])[:2]
                for i in range(curvePoints.GetNumberOfPoints())]

    def assertPV(self, cube, outputVolume, expected, message):
        pvArray = voxelArray(outputVolume)
        self.assertEqual(pvArray.shape, (1,) + expected.shape, message)
        computed = pvArray[0].astype(np.float64)
        scale = np.nanmax(np.abs(cube))
        same = np.allclose(computed, expected, atol=1e-6 * scale, equal_nan=True)
        self.assertTrue(same, "%s: max abs diff %g" % (message, np.nanmax(np.abs(computed - expected))))

    def test_AstroPVDiagramSelfTest(self):
        import AstroSampleData

        self.delayDisplay("Loading WEIN069")
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())

        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroPVDiagram")
        widget = slicer.modules.astropvdiagram.widgetRepresentation()
        logic = slicer.modules.astropvdiagram.logic()

        parametersNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroPVDiagramParametersNode")
        self.assertEqual(parametersNode.GetInputVolumeNodeID(), volumeNode.GetID())
        # the moment-0 map shared by the modules, the path (an open curve)
        momentMap = slicer.mrmlScene.GetNodeByID(parametersNode.GetMomentMapNodeID())
        self.assertEqual(momentMap.GetName(), volumeNode.GetName() + "_mom0")
        path = slicer.util.getNode("PVDiagramLine")
        self.assertTrue(path.IsA("vtkMRMLMarkupsCurveNode"))
        self.assertNotIn("vtkMRMLSliceNodeYellow", path.GetDisplayNode().GetViewNodeIDs())
        self.assertTrue(path.GetDisplayVisibility())
        # no output selectors, no Generate or Auto update buttons
        for name in ("OutputNodeSelector", "GeneratePVPushButton", "AutoUpdateCheckBox", "ParametersNodeComboBox"):
            self.assertEqual(slicer.util.findChildren(widget, name), [])
        info = slicer.util.findChild(widget, "PVDiagramPathInfo")
        self.assertIn("Click", info.text)

        cube = voxelArray(volumeNode).astype(np.float64)

        self.delayDisplay("Straight path: the PV diagram follows the points")
        widget.setInterpolation(False)
        slicer.app.processEvents()
        self.assertEqual(path.GetCurveType(), slicer.vtkCurveGenerator.CURVE_TYPE_LINEAR_SPLINE)
        path.AddControlPoint(0, 0, 0)
        path.AddControlPoint(30, 0, 0)
        path.AddControlPoint(30, 0, 40)
        slicer.app.processEvents()
        outputVolume = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        self.assertIsNotNone(outputVolume)
        self.assertEqual(outputVolume.GetAttribute("SlicerAstro.DATAMODEL"), "PVDIAGRAM")
        self.assertEqual(outputVolume.GetName(), volumeNode.GetName() + "_PVDiagram")
        self.assertEqual(logic.GetPVDiagram(volumeNode), outputVolume)
        samples = resample(self.controlPointsIJ(path, momentMap))
        self.assertPV(cube, outputVolume, referencePV(cube, samples), "straight path")
        # the points stay on the plane and in the extent of the map
        dims = momentMap.GetImageData().GetDimensions()
        for i, j in self.controlPointsIJ(path, momentMap):
            self.assertTrue(-1e-6 <= i <= dims[0] - 1 + 1e-6 and -1e-6 <= j <= dims[1] - 1 + 1e-6)

        # offset step (degrees) = length of the path on the sky / (columns - 1)
        length = 0.0
        display = volumeNode.GetAstroVolumeDisplayNode()
        previous = None
        for x, y in samples:
            world = [0.0, 0.0, 0.0]
            display.GetReferenceSpace([x, y, 0.0], world)
            if previous is not None:
                ra1, dec1, ra2, dec2 = (math.radians(v) for v in (previous[0], previous[1], world[0], world[1]))
                h = math.sin((dec2 - dec1) / 2) ** 2 + math.cos(dec1) * math.cos(dec2) * math.sin((ra2 - ra1) / 2) ** 2
                length += math.degrees(2 * math.asin(min(1.0, math.sqrt(h))))
            previous = world
        self.assertAlmostEqual(float(outputVolume.GetAttribute("SlicerAstro.CDELT1")),
                               length / (len(samples) - 1), delta=1e-9)
        self.assertIn("3 points", info.text)
        self.assertIn("%d columns" % len(samples), info.text)
        self.assertIn("%.2f" % (length * 60.0), info.text)
        result = slicer.util.findChild(widget, "PVDiagramResult")
        self.assertTrue(result.isVisible())

        # the Yellow view shows it; moving a point updates it without fitting
        # the view again (the zoom of the user kept)
        yellowComposite = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceCompositeNodeYellow")
        self.assertEqual(yellowComposite.GetBackgroundVolumeID(), outputVolume.GetID())
        yellow = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow")
        self.assertEqual(yellow.GetOrientation(), "PVDiagram")
        greenSlice = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeGreen")
        self.assertEqual(greenSlice.GetOrientation(), "XY")
        fov = list(yellow.GetFieldOfView())
        yellow.SetFieldOfView(fov[0] * 0.5, fov[1] * 0.5, fov[2])
        zoomed = list(yellow.GetFieldOfView())
        historyBefore = [n for n in outputVolume.GetAttributeNames() if n.startswith("SlicerAstro._HISTORY")]
        path.SetNthControlPointPosition(2, 20, 0, 30)
        slicer.app.processEvents()
        self.assertEqual(list(yellow.GetFieldOfView()), zoomed)
        samples = resample(self.controlPointsIJ(path, momentMap))
        self.assertPV(cube, outputVolume, referencePV(cube, samples), "moved point")
        # the path in the header, replaced (not accumulated) at each update
        history = [n for n in outputVolume.GetAttributeNames() if n.startswith("SlicerAstro._HISTORY")]
        self.assertEqual(len(history), len(historyBefore))
        pvCards = [outputVolume.GetAttribute(n) for n in history
                   if outputVolume.GetAttribute(n).startswith("PV diagram")]
        self.assertEqual(len(pvCards), 4)

        self.delayDisplay("Width: mean of the spectra across the path")
        widthField = slicer.util.findChild(widget, "PVDiagramWidth")
        widthField.value = 5
        slicer.app.processEvents()
        self.assertEqual(parametersNode.GetWidth(), 5)
        self.assertPV(cube, outputVolume, referencePV(cube, samples, 5), "width 5")
        widget.setWidth(1)
        slicer.app.processEvents()

        self.delayDisplay("Spline path")
        shape = slicer.util.findChild(widget, "PVDiagramShape")
        shape.setCurrentIndex(1)
        slicer.app.processEvents()
        self.assertTrue(parametersNode.GetInterpolation())
        self.assertEqual(path.GetCurveType(), slicer.vtkCurveGenerator.CURVE_TYPE_CARDINAL_SPLINE)
        samples = resample(self.curveSamplesIJ(path, momentMap))
        self.assertPV(cube, outputVolume, referencePV(cube, samples), "spline path")

        self.delayDisplay("Path drawn on the map preview")
        widget.clearPath()
        slicer.app.processEvents()
        self.assertEqual(path.GetNumberOfControlPoints(), 0)
        preview = slicer.util.findChild(widget, "PVDiagramPreview")

        def click(position, button):
            for eventType in (qt.QEvent.MouseButtonPress, qt.QEvent.MouseButtonRelease):
                buttons = button if eventType == qt.QEvent.MouseButtonPress else qt.Qt.NoButton
                slicer.app.sendEvent(preview, qt.QMouseEvent(eventType, position, position, button, buttons,
                                                             qt.Qt.NoModifier))

        for i, j in ((10.0, 20.0), (60.0, 25.0)):
            position = preview.mapToWidget(i, j)
            click(position, qt.Qt.LeftButton)
        slicer.app.processEvents()
        self.assertEqual(path.GetNumberOfDefinedControlPoints(), 2)
        points = self.controlPointsIJ(path, momentMap)
        self.assertAlmostEqual(points[1][0], 60.0, delta=1.0)
        self.assertAlmostEqual(points[1][1], 25.0, delta=1.0)
        position = preview.mapToWidget(points[1][0], points[1][1])
        click(position, qt.Qt.RightButton)
        slicer.app.processEvents()
        self.assertEqual(path.GetNumberOfDefinedControlPoints(), 1)

        self.delayDisplay("Draw in views: place mode on the path")
        draw = slicer.util.findChild(widget, "PVDiagramDrawButton")
        draw.click()
        interaction = slicer.app.applicationLogic().GetInteractionNode()
        self.assertEqual(interaction.GetCurrentInteractionMode(), interaction.Place)
        self.assertEqual(slicer.app.applicationLogic().GetSelectionNode().GetActivePlaceNodeID(), path.GetID())
        interaction.SetCurrentInteractionMode(interaction.ViewTransform)
        self.assertFalse(draw.checked)

        # leaving the module hides the path
        mainWindow.moduleSelector().selectModule("AstroVolume")
        self.assertFalse(path.GetDisplayVisibility())
        self.delayDisplay("Test passed")
