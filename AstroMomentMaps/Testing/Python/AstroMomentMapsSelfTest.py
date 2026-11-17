import numpy as np

import slicer
import vtk
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)

DOUBLEPRECISION = 1.e-16


class AstroMomentMapsSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro MomentMaps SelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroMomentMaps"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Self test of the AstroMomentMaps module: the moment maps are compared with an
independent (numpy) computation.
"""
        self.parent.hidden = True


class AstroMomentMapsSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroMomentMapsSelfTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of an (astro) volume node."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def channelRange(volumeNode, parametersNode):
    """Velocities of the channels (km/s) and the channel range [zmin, zmax]
    selected by the velocity limits of the parameters node."""
    display = volumeNode.GetAstroVolumeDisplayNode()
    dims = volumeNode.GetImageData().GetDimensions()
    center = [dims[0] * 0.5, dims[1] * 0.5]
    world = [0.0, 0.0, 0.0]
    velocities = []
    for k in range(dims[2]):
        display.GetReferenceSpace([center[0], center[1], float(k)], world)
        velocities.append(world[2])
    velocities = np.array(velocities)
    # The spectral world coordinate is in m/s (WCS unit) or km/s; the velocity
    # limits of the parameters node are in km/s. Galaxy velocities are well
    # below 1e4 km/s, so larger values are in m/s.
    velMin, velMax = parametersNode.GetVelocityMin(), parametersNode.GetVelocityMax()
    velFactor = 0.001 if np.nanmax(np.abs(velocities)) > 1.e4 else 1.0

    def channel(velocity):
        world = [0.0, 0.0, 0.0]
        display.GetReferenceSpace([center[0], center[1], 0.0], world)
        world[2] = velocity / velFactor
        ijk = [0.0, 0.0, 0.0]
        display.GetIJKSpace(world, ijk)
        return int(min(max(ijk[2], 0), dims[2] - 1))

    zmin, zmax = channel(velMin), channel(velMax)
    if zmin > zmax:
        zmin, zmax = zmax, zmin
    return velocities * velFactor, zmin, zmax


def referenceMoments(cube, velocities, zmin, zmax, intensityMin, intensityMax, velMin, velMax):
    """Moment maps (0, 1, 2) with the definitions used by AstroMomentMaps."""
    selected = cube[zmin:zmax + 1].astype(np.float64)
    vel = velocities[zmin:zmax + 1][:, None, None]
    with np.errstate(invalid="ignore", divide="ignore"):
        inside = (selected > intensityMin) & (selected < intensityMax)
        weights = np.where(inside, selected, 0.0)
        m0raw = weights.sum(axis=0)
        m1raw = (weights * vel).sum(axis=0)
        empty0 = np.abs(m0raw) < DOUBLEPRECISION
        m1 = np.where(empty0 | (np.abs(m1raw) < DOUBLEPRECISION), np.nan, m1raw / m0raw)
        m2raw = (weights * (vel - m1) ** 2).sum(axis=0)
        m2 = np.where(empty0 | ~(np.abs(m2raw) >= DOUBLEPRECISION), np.nan, np.sqrt(m2raw / m0raw))
        # channel width (moment 0: sum of intensity times channel width)
        dV = abs((velocities[zmax] - velocities[zmin]) / (zmax - zmin)) if zmax > zmin else abs(velocities[1] - velocities[0])
        m0 = np.where(empty0, np.nan, m0raw * dV)
    return m0, m1, m2


def referenceMaskedMoments(cube, velocities, mask):
    """Moment maps (0, 1, 2) of the voxels inside the mask, all the channels
    (the mask selects the voxels: no intensity or velocity limits)."""
    data = np.where(mask, cube.astype(np.float64), 0.0)
    data = np.nan_to_num(data, nan=0.0)
    vel = velocities[:, None, None]
    dV = abs((velocities[-1] - velocities[0]) / (len(velocities) - 1))
    with np.errstate(invalid="ignore", divide="ignore"):
        m0raw = data.sum(axis=0)
        m1raw = (data * vel).sum(axis=0)
        empty0 = np.abs(m0raw) < DOUBLEPRECISION
        m1 = np.where(empty0 | (np.abs(m1raw) < DOUBLEPRECISION), np.nan, m1raw / m0raw)
        m2raw = (data * (vel - m1) ** 2).sum(axis=0)
        m2 = np.where(empty0 | ~(np.abs(m2raw) >= DOUBLEPRECISION), np.nan, np.sqrt(m2raw / m0raw))
        m0 = np.where(empty0, np.nan, m0raw * dV)
    return m0, m1, m2


class AstroMomentMapsSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        for test in (self.test_AstroMomentMapsSelfTest, self.test_DoublePrecision, self.test_Mask):
            self.setUp()
            test()

    def assertMapsClose(self, computed, expected, name, zeroTolerance=0.0):
        # moments are accumulated in the output type (float32): relative tolerance
        valid = ~np.isnan(expected) & ~np.isnan(computed)
        mismatch = np.isnan(expected) != np.isnan(computed)
        # Single-channel pixels: the dispersion is zero (blank by definition),
        # but the module computes it from the float32 first moment, leaving a
        # tiny residual. Accept those.
        mismatch &= ~(np.isnan(expected) & (np.abs(np.nan_to_num(computed)) <= zeroTolerance))
        nanMismatch = np.count_nonzero(mismatch)
        self.assertLessEqual(nanMismatch, 2, f"{name}: blank pixels differ ({nanMismatch})")
        scale = np.nanmax(np.abs(expected[valid])) if np.any(valid) else 1.0
        maxDiff = np.max(np.abs(computed[valid] - expected[valid])) if np.any(valid) else 0.0
        self.assertLess(maxDiff, 1e-4 * scale, f"{name}: max difference {maxDiff} (scale {scale})")

    def test_AstroMomentMapsSelfTest(self):
        import AstroSampleData

        self.delayDisplay("Loading WEIN069")
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        self.checkMomentMaps(volumeNode)
        self.delayDisplay("Test passed")

    def test_DoublePrecision(self):
        """64-bit cube (BITPIX -64): double moment maps, equal to the reference."""
        import AstroSampleData
        from AstroVolumeLogicTest import doublePrecisionCopy

        self.delayDisplay("Loading WEIN069 (double precision)")
        floatNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(floatNode)
        volumeNode = doublePrecisionCopy(floatNode, "WEIN069_double")
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        maps = self.checkMomentMaps(volumeNode)
        for node in maps:
            self.assertEqual(node.GetImageData().GetScalarType(), vtk.VTK_DOUBLE, node.GetName())
        self.delayDisplay("Test passed")

    def test_Mask(self):
        """Moment maps of the voxels of a mask segment (the segment read
        directly, no label map volume), for 32-bit and 64-bit cubes."""
        import AstroSampleData
        from AstroVolumeLogicTest import doublePrecisionCopy, maskSegmentation

        self.delayDisplay("Moment maps with a mask")
        floatNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(floatNode)
        doubleNode = doublePrecisionCopy(floatNode, "WEIN069_double")
        cube = voxelArray(floatNode)
        with np.errstate(invalid="ignore"):
            source = cube > 3. * floatNode.GetDisplayThreshold()
        # a second segment in the same layer: only the selected one is used
        other = np.zeros(cube.shape, dtype=bool)
        other[:, :5, :5] = True
        other &= ~source
        segmentationNode, (sourceID, otherID) = maskSegmentation(
            floatNode, {"Source": source, "Corner": other}, "WEIN069_TestMask")

        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroMomentMaps")
        widget = slicer.modules.astromomentmaps.widgetRepresentation()
        parametersNode = slicer.util.getNode("AstroMomentMapsParameters")
        for volumeNode in (floatNode, doubleNode):
            parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
            parametersNode.SetGenerateZero(True)
            parametersNode.SetGenerateFirst(True)
            parametersNode.SetGenerateSecond(True)
            slicer.util.findChild(widget, "MaskCheckBox").setChecked(True)
            self.assertTrue(parametersNode.GetMaskActive())
            slicer.util.findChild(widget, "SegmentationNodeComboBox").setCurrentNode(segmentationNode)
            slicer.app.processEvents()
            slicer.util.findChild(widget, "SegmentsTableView").setSelectedSegmentIDs([sourceID])
            slicer.app.processEvents()
            slicer.util.findChild(widget, "ApplyButton").click()
            # the segment is the mask: no label map volume
            self.assertEqual(parametersNode.GetMaskSegmentationNodeID(), segmentationNode.GetID())
            self.assertEqual(parametersNode.GetMaskSegmentID(), sourceID)
            self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLLabelMapVolumeNode"), 0)

            velocities, _, _ = channelRange(volumeNode, parametersNode)
            expected = referenceMaskedMoments(cube, velocities, source)
            for name, nodeID, reference in zip(("moment 0", "moment 1", "moment 2"),
                                               (parametersNode.GetZeroMomentVolumeNodeID(),
                                                parametersNode.GetFirstMomentVolumeNodeID(),
                                                parametersNode.GetSecondMomentVolumeNodeID()), expected):
                node = slicer.mrmlScene.GetNodeByID(nodeID)
                self.assertIsNotNone(node, name)
                self.assertEqual(node.GetImageData().GetScalarType(), volumeNode.GetImageData().GetScalarType())
                computed = voxelArray(node)[0].astype(np.float64)
                self.assertMapsClose(computed, reference, f"{volumeNode.GetName()} {name}",
                                     1e-3 * abs(velocities[1] - velocities[0]) if name == "moment 2" else 0.0)
            slicer.util.findChild(widget, "MaskCheckBox").setChecked(False)
        self.delayDisplay("Test passed")

    def checkMomentMaps(self, volumeNode):
        """Moment maps 0, 1, 2 of the volume (above 3 RMS) computed by the
        module, compared with numpy; returns the map nodes."""
        rms = volumeNode.GetDisplayThreshold()

        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroMomentMaps")

        parametersNode = slicer.util.getNode("AstroMomentMapsParameters")
        parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
        parametersNode.SetMaskActive(False)
        parametersNode.SetGenerateZero(True)
        parametersNode.SetGenerateFirst(True)
        parametersNode.SetGenerateSecond(True)
        parametersNode.SetIntensityMin(rms * 3)
        # a modification of the data does not reset the selected ranges
        volumeNode.Modified()
        slicer.app.processEvents()
        self.assertAlmostEqual(parametersNode.GetIntensityMin(), rms * 3)

        self.delayDisplay("Calculating moment maps")
        widget = slicer.modules.astromomentmaps.widgetRepresentation()
        slicer.util.findChild(widget, "ApplyButton").click()

        nodes, maps = [], []
        for nodeID in (parametersNode.GetZeroMomentVolumeNodeID(),
                       parametersNode.GetFirstMomentVolumeNodeID(),
                       parametersNode.GetSecondMomentVolumeNodeID()):
            node = slicer.mrmlScene.GetNodeByID(nodeID)
            self.assertIsNotNone(node)
            nodes.append(node)
            maps.append(voxelArray(node)[0].astype(np.float64))

        velocities, zmin, zmax = channelRange(volumeNode, parametersNode)
        expected = referenceMoments(voxelArray(volumeNode), velocities, zmin, zmax,
                                    parametersNode.GetIntensityMin(), parametersNode.GetIntensityMax(),
                                    parametersNode.GetVelocityMin(), parametersNode.GetVelocityMax())

        channelWidth = abs(velocities[1] - velocities[0])
        for name, computed, reference, zeroTolerance in zip(
                ("moment 0", "moment 1", "moment 2"), maps, expected, (0.0, 0.0, 1e-3 * channelWidth)):
            self.assertEqual(computed.shape, reference.shape)
            self.assertMapsClose(computed, reference, name, zeroTolerance)
        return nodes
