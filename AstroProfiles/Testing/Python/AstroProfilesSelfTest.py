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


def referenceProfile(volumeNode, intensityMin, intensityMax, mask=None, aperture=False, channels=None):
    """Flux per channel: the sum of the voxels within (intensityMin,
    intensityMax), of the voxels of the mask, or (aperture) of all the voxels
    in the sky area of the mask, converted from Jy/beam to Jy when the beam
    is defined; 0 for channels without selected voxels; blank (NaN) for the
    channels outside channels (first, last) and the channels without data."""
    cube = voxelArray(volumeNode).astype(np.float64)
    def attribute(key):
        try:
            return float(volumeNode.GetAttribute("SlicerAstro." + key))
        except (TypeError, ValueError):  # e.g. "UNDEFINED"
            return None
    bmaj, bmin = attribute("BMAJ"), attribute("BMIN")
    jyBeam = (volumeNode.GetAttribute("SlicerAstro.BUNIT") or "").replace(" ", "").upper() == "JY/BEAM"
    if jyBeam and bmaj and bmin and bmaj > 0 and bmin > 0:
        unitBeamConv = abs(attribute("CDELT1") * attribute("CDELT2") / (np.pi / (4. * np.log(2.)) * bmaj * bmin))
    else:
        unitBeamConv = 1.0  # beam not defined: the sum of the intensities
    with np.errstate(invalid="ignore"):
        if mask is None:
            inside = (cube > intensityMin) & (cube < intensityMax)
        elif aperture:
            inside = np.broadcast_to(mask.any(axis=0), cube.shape) & ~np.isnan(cube)
        else:
            inside = mask & ~np.isnan(cube)
    profile = np.where(inside, cube, 0.0).sum(axis=(1, 2)) * unitBeamConv
    profile[np.all(np.isnan(cube), axis=(1, 2))] = np.nan
    if channels is not None:
        outside = np.ones(profile.shape, dtype=bool)
        outside[channels[0]:channels[1] + 1] = False
        profile[outside] = np.nan
    return profile


class AstroProfilesSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        for test in (self.test_AstroProfilesSelfTest, self.test_Mask, self.test_VelocityRangeAndUnits):
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
        """Profile with a mask segment (read directly, no label map volume),
        aperture (sky area of the mask, all channels) and voxels of the mask,
        for 32-bit and 64-bit cubes; another segment of the same labelmap
        layer is not used; channels without selected voxels are 0."""
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
        for volumeNode, aperture in ((floatNode, True), (floatNode, False), (doubleNode, True), (doubleNode, False)):
            parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
            slicer.util.findChild(widget, "MaskCheckBox").setChecked(True)
            apertureCheckBox = slicer.util.findChild(widget, "MaskApertureCheckBox")
            self.assertTrue(apertureCheckBox.enabled)
            apertureCheckBox.setChecked(aperture)
            self.assertEqual(parametersNode.GetMaskAperture(), aperture)
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
            expected = referenceProfile(floatNode, 0., 0., mask=source, aperture=aperture)
            self.assertTrue(np.array_equal(np.isnan(computed), np.isnan(expected)), "blank channels differ")
            valid = ~np.isnan(expected)
            scale = np.max(np.abs(expected[valid]))
            self.assertLess(np.max(np.abs(computed[valid] - expected[valid])), 1e-5 * scale,
                            f"{volumeNode.GetName()} aperture {aperture}")
            # every channel has data: no gaps in the plot (0 without selected voxels)
            self.assertFalse(np.any(np.isnan(computed)))
            if not aperture:
                self.assertGreater(np.count_nonzero(computed == 0.), 0)
            # WEIN069 has no beam: the sum of the intensities
            chart = slicer.mrmlScene.GetFirstNodeByName("ProfileChart")
            self.assertEqual(chart.GetYAxisTitle(), "Sum of the intensities per channel (JY/BEAM)")
            slicer.util.findChild(widget, "MaskCheckBox").setChecked(False)
        self.delayDisplay("Test passed")

    def test_VelocityRangeAndUnits(self):
        """Channels outside the velocity range are blank; with a beam the
        profile is in Jy (pixel area / beam area), otherwise the sum of the
        intensities."""
        import os
        from AstroFITSIOTest import beamCards, celestialWCS, radioVelocityWCS, writeFITS

        directory = os.path.join(slicer.app.temporaryPath, "AstroProfilesSelfTest")
        os.makedirs(directory, exist_ok=True)
        rng = np.random.default_rng(5)
        data = rng.normal(0., 1., (10, 12, 14)).astype(np.float32)
        data[3:7, 4:8, 5:9] += 10.
        mainWindow = slicer.util.mainWindow()
        widget = None
        for name, cards in (("beam", beamCards()), ("nobeam", [("BUNIT", "JY/BEAM")]), ("kelvin", beamCards("K"))):
            path = os.path.join(directory, f"profile_{name}.fits")
            writeFITS(path, data, -32, celestialWCS(crpix=(7.5, 6.5)) + radioVelocityWCS() + cards)
            volumeNode = slicer.util.loadVolume(path)
            slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
            mainWindow.moduleSelector().selectModule("AstroVolume")
            mainWindow.moduleSelector().selectModule("AstroProfiles")
            widget = slicer.modules.astroprofiles.widgetRepresentation()
            parametersNode = slicer.util.getNode("AstroProfilesParameters")
            parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
            slicer.util.findChild(widget, "MaskCheckBox").setChecked(False)
            parametersNode.SetIntensityMin(3.)
            # channels 2..7: velocities 1000 + 5000 k m/s
            parametersNode.SetVelocityMin(11.)
            parametersNode.SetVelocityMax(36.)
            # a modification of the data does not reset the selected ranges
            volumeNode.Modified()
            slicer.app.processEvents()
            self.assertEqual((parametersNode.GetVelocityMin(), parametersNode.GetVelocityMax(),
                              parametersNode.GetIntensityMin()), (11., 36., 3.))
            slicer.util.findChild(widget, "ApplyButton").click()
            profileNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetProfileVolumeNodeID())
            computed = voxelArray(profileNode).ravel().astype(np.float64)
            expected = referenceProfile(volumeNode, parametersNode.GetIntensityMin(), parametersNode.GetIntensityMax(),
                                        channels=(2, 7))
            self.assertTrue(np.array_equal(np.isnan(computed), np.isnan(expected)), name)
            valid = ~np.isnan(expected)
            np.testing.assert_allclose(computed[valid], expected[valid], rtol=1e-5, atol=1e-7, err_msg=name)
            title = slicer.mrmlScene.GetFirstNodeByName("ProfileChart").GetYAxisTitle()
            if name == "beam":
                self.assertEqual(title, "Total flux per channel (Jy)")
                # Jy: the sum of the intensities times pixel area / beam area
                # (CDELT 0.001 deg, beam 0.004 x 0.003 deg)
                cube = voxelArray(volumeNode).astype(np.float64)
                sums = np.where((cube > 3.) & (cube < parametersNode.GetIntensityMax()), cube, 0.).sum(axis=(1, 2))
                factor = 0.001 * 0.001 / (np.pi / (4. * np.log(2.)) * 0.004 * 0.003)
                np.testing.assert_allclose(computed[2:8], sums[2:8] * factor, rtol=1e-5)
            else:
                self.assertEqual(title, "Sum of the intensities per channel (%s)" %
                                 ("JY/BEAM" if name == "nobeam" else "K"))
        self.delayDisplay("Test passed")
