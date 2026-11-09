import numpy as np

import slicer
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
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_AstroStatisticsSelfTest()

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
