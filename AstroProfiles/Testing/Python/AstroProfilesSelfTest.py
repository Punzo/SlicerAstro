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


def referenceProfile(volumeNode, intensityMin, intensityMax, mask=None):
    """Flux per channel of the voxels within (intensityMin, intensityMax),
    converted from Jy/beam to Jy when the beam is defined; blank (NaN) for
    empty channels."""
    cube = voxelArray(volumeNode).astype(np.float64)
    def attribute(key):
        try:
            return float(volumeNode.GetAttribute("SlicerAstro." + key))
        except (TypeError, ValueError):  # e.g. "UNDEFINED"
            return None
    bmaj, bmin = attribute("BMAJ"), attribute("BMIN")
    if bmaj and bmin and bmaj > 0 and bmin > 0:
        unitBeamConv = abs(attribute("CDELT1") * attribute("CDELT2") / (np.pi / (4. * np.log(2.)) * bmaj * bmin))
    else:
        unitBeamConv = 1.0  # beam not defined: the profile stays in Jy/beam
    with np.errstate(invalid="ignore"):
        if mask is None:
            inside = (cube > intensityMin) & (cube < intensityMax)
        else:
            # the mask selects the voxels (no intensity limits); blank voxels skipped
            inside = mask & ~np.isnan(cube)
    profile = np.where(inside, cube, 0.0).sum(axis=(1, 2)) * unitBeamConv
    profile[np.abs(profile) < DOUBLEPRECISION] = np.nan
    return profile


class AstroProfilesSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        for test in (self.test_AstroProfilesSelfTest, self.test_Mask):
            self.setUp()
            test()

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

    def test_Mask(self):
        """Profile of the voxels of a mask segment (read directly, no label map
        volume) for 32-bit and 64-bit cubes; another segment of the same
        labelmap layer is not used."""
        import AstroSampleData
        from AstroVolumeLogicTest import doublePrecisionCopy, maskSegmentation

        self.delayDisplay("Profile with a mask")
        floatNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(floatNode)
        doubleNode = doublePrecisionCopy(floatNode, "WEIN069_double")
        cube = voxelArray(floatNode)
        with np.errstate(invalid="ignore"):
            source = cube > 3. * floatNode.GetDisplayThreshold()
        other = np.zeros(cube.shape, dtype=bool)
        other[:, -6:, -6:] = True
        other &= ~source
        segmentationNode, (sourceID, otherID) = maskSegmentation(
            floatNode, {"Source": source, "Corner": other}, "WEIN069_TestMask")

        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroProfiles")
        widget = slicer.modules.astroprofiles.widgetRepresentation()
        parametersNode = slicer.util.getNode("AstroProfilesParameters")
        for volumeNode in (floatNode, doubleNode):
            parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
            slicer.util.findChild(widget, "MaskCheckBox").setChecked(True)
            self.assertTrue(parametersNode.GetMaskActive())
            slicer.util.findChild(widget, "SegmentationNodeComboBox").setCurrentNode(segmentationNode)
            slicer.app.processEvents()
            slicer.util.findChild(widget, "SegmentsTableView").setSelectedSegmentIDs([sourceID])
            slicer.app.processEvents()
            slicer.util.findChild(widget, "ApplyButton").click()
            self.assertEqual(parametersNode.GetMaskSegmentationNodeID(), segmentationNode.GetID())
            self.assertEqual(parametersNode.GetMaskSegmentID(), sourceID)
            self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLLabelMapVolumeNode"), 0)

            profileNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetProfileVolumeNodeID())
            self.assertIsNotNone(profileNode)
            self.assertEqual(profileNode.GetImageData().GetScalarType(), volumeNode.GetImageData().GetScalarType())
            computed = voxelArray(profileNode).ravel().astype(np.float64)
            expected = referenceProfile(floatNode, 0., 0., mask=source)
            self.assertTrue(np.array_equal(np.isnan(computed), np.isnan(expected)), "blank channels differ")
            valid = ~np.isnan(expected)
            scale = np.max(np.abs(expected[valid]))
            self.assertLess(np.max(np.abs(computed[valid] - expected[valid])), 1e-5 * scale, volumeNode.GetName())
            slicer.util.findChild(widget, "MaskCheckBox").setChecked(False)
        self.delayDisplay("Test passed")
