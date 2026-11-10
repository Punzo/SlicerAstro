import numpy as np

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)

DOUBLEPRECISION = 1.e-16


class AstroProfilesSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Profiles SelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroProfiles"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Self test of the AstroProfiles module: the flux profile is compared with an
independent (numpy) computation.
"""
        self.parent.hidden = True


class AstroProfilesSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroProfilesSelfTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of an (astro) volume node."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def referenceProfile(volumeNode, intensityMin, intensityMax):
    """Flux per channel of the voxels within (intensityMin, intensityMax),
    converted from Jy/beam to Jy; blank (NaN) for empty channels."""
    cube = voxelArray(volumeNode).astype(np.float64)
    attribute = lambda key: float(volumeNode.GetAttribute("SlicerAstro." + key))
    unitBeamConv = abs(attribute("CDELT1") * attribute("CDELT2") / (1.13 * attribute("BMAJ") * attribute("BMIN")))
    with np.errstate(invalid="ignore"):
        inside = (cube > intensityMin) & (cube < intensityMax)
    profile = np.where(inside, cube, 0.0).sum(axis=(1, 2)) * unitBeamConv
    profile[np.abs(profile) < DOUBLEPRECISION] = np.nan
    return profile


class AstroProfilesSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_AstroProfilesSelfTest()

    def test_AstroProfilesSelfTest(self):
        import AstroSampleData

        self.delayDisplay("Loading WEIN069")
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        rms = volumeNode.GetDisplayThreshold()

        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroProfiles")

        parametersNode = slicer.util.getNode("AstroProfilesParameters")
        parametersNode.SetMaskActive(False)
        parametersNode.SetIntensityMin(rms * 3)

        self.delayDisplay("Calculating profile")
        widget = slicer.modules.astroprofiles.widgetRepresentation()
        slicer.util.findChild(widget, "ApplyButton").click()

        profileNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetProfileVolumeNodeID())
        self.assertIsNotNone(profileNode)
        computed = voxelArray(profileNode).ravel().astype(np.float64)
        expected = referenceProfile(volumeNode, parametersNode.GetIntensityMin(), parametersNode.GetIntensityMax())

        self.assertEqual(computed.shape, expected.shape)
        self.assertTrue(np.array_equal(np.isnan(computed), np.isnan(expected)), "blank channels differ")
        valid = ~np.isnan(expected)
        scale = np.max(np.abs(expected[valid]))
        maxDiff = np.max(np.abs(computed[valid] - expected[valid]))
        # the module accumulates in float32: relative tolerance
        self.assertLess(maxDiff, 1e-5 * scale, f"max difference {maxDiff} (scale {scale})")

        self.delayDisplay("Test passed")
