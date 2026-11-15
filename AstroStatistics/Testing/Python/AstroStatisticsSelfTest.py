import os
import shutil

import numpy as np
import slicer
import vtk
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroStatisticsSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Statistics SelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroStatistics"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Self test of the AstroStatistics module: the statistics of the whole data
cube are compared with an independent (numpy) computation.
"""
        self.parent.hidden = True


class AstroStatisticsSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroStatisticsSelfTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of a volume node."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def fitsCard(key, value=None):
    """80-character header card."""
    if isinstance(value, bool):
        text = ("T" if value else "F").rjust(20)
    elif isinstance(value, (int, np.integer)):
        text = str(int(value)).rjust(20)
    elif isinstance(value, (float, np.floating)):
        text = repr(float(value)).upper().rjust(20)
    else:
        text = ("'" + str(value).replace("'", "''").ljust(8) + "'").ljust(20)
    return f"{key:8s}= {text}"[:80].ljust(80)


def writeFITS(path, data, cards):
    """Single float image HDU with the header cards (list of (key, value))."""
    header = [("SIMPLE", True), ("BITPIX", -32), ("NAXIS", data.ndim)]
    header += [(f"NAXIS{axis}", size) for axis, size in enumerate(reversed(data.shape), start=1)]
    text = "".join(fitsCard(*card) for card in header + list(cards)) + "END".ljust(80)
    text += " " * (-len(text) % 2880)
    payload = np.ascontiguousarray(data, dtype=">f4").tobytes()
    with open(path, "wb") as f:
        f.write(text.encode("ascii") + payload + b"\0" * (-len(payload) % 2880))


CDELT = 0.002
BEAM = 0.006
CUBE_CARDS = [
    ("CTYPE1", "RA---SIN"), ("CRVAL1", 180.), ("CDELT1", -CDELT), ("CRPIX1", 12.5), ("CUNIT1", "deg"),
    ("CTYPE2", "DEC--SIN"), ("CRVAL2", 30.), ("CDELT2", CDELT), ("CRPIX2", 10.5), ("CUNIT2", "deg"),
    ("CTYPE3", "VRAD"), ("CRVAL3", 100000.), ("CDELT3", 2000.), ("CRPIX3", 1.), ("CUNIT3", "m/s"),
    ("RESTFREQ", 1.420405752E9), ("SPECSYS", "BARYCENT"), ("RADESYS", "FK5"), ("EQUINOX", 2000.),
    ("BUNIT", "JY/BEAM"), ("BMAJ", BEAM), ("BMIN", BEAM), ("BPA", 0.)]


def referenceStatistics(volumeNode):
    """Statistics of all the non-blank voxels, computed with numpy (float64)."""
    # zero-copy view on the voxel buffer
    from vtk.util.numpy_support import vtk_to_numpy
    values = vtk_to_numpy(volumeNode.GetImageData().GetPointData().GetScalars())
    values = values[~np.isnan(values)].astype(np.float64)
    n = values.size
    mean = values.sum() / n
    return {
        "Npixels": n,
        "Min": values.min(),
        "Max": values.max(),
        "Mean": mean,
        "Std": np.sqrt(((values - mean) ** 2).sum() / n),
        "Median": np.median(values.astype(np.float32)),
        "Sum": values.sum(),
    }


class AstroStatisticsSelfTestTest(ScriptedLoadableModuleTest):
    SHAPE = (10, 20, 24)  # (z, y, x)

    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroStatisticsSelfTest")
        if os.path.isdir(self.directory):
            shutil.rmtree(self.directory)
        os.makedirs(self.directory)

    def runTest(self):
        for test in (self.test_AstroStatisticsSelfTest, self.test_Box, self.test_Mask,
                     self.test_SelectedQuantities):
            self.setUp()
            test()

    #
    # helpers of the tests on a synthetic cube
    #

    def cube(self):
        """Synthetic cube: Gaussian noise plus a source, a few blank (NaN) voxels."""
        random = np.random.default_rng(12345)
        data = random.normal(0., 1., self.SHAPE).astype(np.float32)
        z, y, x = np.indices(self.SHAPE)
        data += 20. * np.exp(-((x - 12.) ** 2 + (y - 10.) ** 2) / 18. - (z - 5.) ** 2 / 4.)
        data[random.random(self.SHAPE) < 0.02] = np.nan
        path = os.path.join(self.directory, "cube.fits")
        writeFITS(path, data, CUBE_CARDS)
        node = slicer.util.loadVolume(path)
        self.assertIsNotNone(node)
        np.testing.assert_array_equal(voxelArray(node), data)
        return node, data

    def openModule(self, volumeNode):
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroStatistics")
        widget = slicer.modules.astrostatistics.widgetRepresentation()
        parametersNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroStatisticsParametersNode")
        parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
        return widget, parametersNode

    def setBox(self, parametersNode, volumeNode, first, last):
        """Box (ROI) covering the voxels first..last (i, j, k, inclusive)."""
        ijkToRAS = vtk.vtkMatrix4x4()
        volumeNode.GetIJKToRASMatrix(ijkToRAS)
        corners = np.array([ijkToRAS.MultiplyPoint((i, j, k, 1.))[:3]
                            for i in (first[0] - 0.5, last[0] + 0.5)
                            for j in (first[1] - 0.5, last[1] + 0.5)
                            for k in (first[2] - 0.5, last[2] + 0.5)])
        roiNode = parametersNode.GetROINode()
        roiNode.SetXYZ(*((corners.min(axis=0) + corners.max(axis=0)) / 2.))
        roiNode.SetRadiusXYZ(*((corners.max(axis=0) - corners.min(axis=0)) / 2.))

    def compute(self, widget, parametersNode):
        """Statistics of the last row of the output table."""
        slicer.util.findChild(widget, "ApplyButton").click()
        table = parametersNode.GetTableNode().GetTable()
        row = table.GetNumberOfRows() - 1
        self.assertGreaterEqual(row, 0)
        return {name: table.GetValueByName(row, name).ToDouble()
                for name in ("Npixels", "Min", "Max", "Mean", "Std", "Median", "Sum", "TotalFlux")}

    def assertStatistics(self, computed, values, names=None):
        values = values[~np.isnan(values)].astype(np.float64)
        sum = values.sum()
        expected = {
            "Npixels": values.size, "Min": values.min(), "Max": values.max(), "Mean": values.mean(),
            "Std": values.std(), "Median": np.median(values.astype(np.float32)), "Sum": sum,
            # total flux (Jy): sum of the Jy/beam values divided by the beam area in pixels
            "TotalFlux": sum * CDELT * CDELT / (1.13 * BEAM * BEAM)}
        for name in names or expected:
            self.assertAlmostEqual(computed[name], expected[name],
                                   delta=1e-5 * max(1., abs(expected[name])), msg=name)

    #
    # tests on a synthetic cube
    #

    def test_Box(self):
        volumeNode, data = self.cube()
        widget, parametersNode = self.openModule(volumeNode)
        slicer.util.findChild(widget, "ROIModeRadioButton").click()
        parametersNode.SetTotalFlux(True)
        self.setBox(parametersNode, volumeNode, (5, 3, 2), (18, 15, 8))
        computed = self.compute(widget, parametersNode)
        self.assertStatistics(computed, data[2:9, 3:16, 5:19])

    def test_Mask(self):
        volumeNode, data = self.cube()
        z, y, x = np.indices(self.SHAPE)
        mask = ((x - 12.) ** 2 + (y - 10.) ** 2) / 36. + (z - 5.) ** 2 / 9. <= 1.
        segmentationNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentationNode", "TestMask")
        segmentationNode.CreateDefaultDisplayNodes()
        segmentationNode.SetReferenceImageGeometryParameterFromVolumeNode(volumeNode)
        segmentID = segmentationNode.GetSegmentation().AddEmptySegment("Source", "Source")
        slicer.util.updateSegmentBinaryLabelmapFromArray(mask.astype(np.uint8), segmentationNode,
                                                         segmentID, volumeNode)
        widget, parametersNode = self.openModule(volumeNode)
        parametersNode.SetTotalFlux(True)
        slicer.util.findChild(widget, "SegmentationModeRadioButton").click()
        slicer.util.findChild(widget, "SegmentationNodeComboBox").setCurrentNode(segmentationNode)
        slicer.app.processEvents()
        slicer.util.findChild(widget, "SegmentsTableView").setSelectedSegmentIDs([segmentID])
        slicer.app.processEvents()
        self.assertEqual(parametersNode.GetMode(), "Segmentation")
        computed = self.compute(widget, parametersNode)
        self.assertStatistics(computed, data[mask])

    def test_SelectedQuantities(self):
        """Quantities switched off in the widget are not computed (NaN in the table)."""
        volumeNode, data = self.cube()
        widget, parametersNode = self.openModule(volumeNode)
        slicer.util.findChild(widget, "ROIModeRadioButton").click()
        self.assertTrue(slicer.modules.astrostatistics.logic().FitROIToInputVolume(parametersNode))
        enabled = {"Npixels": False, "Min": True, "Max": False, "Mean": True, "Std": False,
                   "Median": False, "Sum": True, "TotalFlux": False}
        for name, on in enabled.items():
            checkBox = slicer.util.findChild(widget, name + "CheckBox")
            checkBox.setChecked(not on)
            checkBox.setChecked(on)
        computed = self.compute(widget, parametersNode)
        for name, on in enabled.items():
            self.assertEqual(not np.isnan(computed[name]), on, msg=name)
        self.assertStatistics(computed, data, [name for name, on in enabled.items() if on])

    def test_AstroStatisticsSelfTest(self):
        import AstroSampleData

        self.delayDisplay("Loading WEIN069")
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)

        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroStatistics")

        parametersNode = slicer.util.getNode("AstroStatisticsParameters")
        parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
        parametersNode.SetMode("ROI")
        logic = slicer.modules.astrostatistics.logic()
        self.assertTrue(logic.FitROIToInputVolume(parametersNode))

        self.delayDisplay("Calculating statistics")
        widget = slicer.modules.astrostatistics.widgetRepresentation()
        applyButton = slicer.util.findChild(widget, "ApplyButton")
        applyButton.click()

        table = parametersNode.GetTableNode().GetTable()

        def value(column):
            return table.GetValue(0, column).ToDouble()

        computed = {
            "Npixels": value(1),
            "Min": value(2),
            "Max": value(3),
            "Mean": value(4),
            "Std": value(5),
            "Median": value(6),
            "Sum": value(7),
        }
        expected = referenceStatistics(volumeNode)
        print("computed:", computed)
        print("expected:", expected)

        self.assertEqual(int(round(computed["Npixels"])), expected["Npixels"])
        scale = max(abs(expected["Min"]), abs(expected["Max"]))
        for key in ("Min", "Max", "Median"):
            self.assertAlmostEqual(computed[key], expected[key], delta=1e-6 * scale, msg=key)
        for key in ("Mean", "Std"):
            self.assertAlmostEqual(computed[key], expected[key], delta=1e-6 * scale, msg=key)
        # the sum is accumulated per thread: relative tolerance
        self.assertAlmostEqual(computed["Sum"], expected["Sum"],
                               delta=1e-6 * abs(expected["Sum"]) + 1e-6 * scale, msg="Sum")

        self.delayDisplay("Test passed")
