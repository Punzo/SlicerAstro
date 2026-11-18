import os
import shutil

import numpy as np
import qt
import vtk
import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroMaskingSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Masking SelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroMasking"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Self test of the AstroMasking module: blanking and cropping with a box (ROI)
and with a mask (segment) on a synthetic cube, compared voxel by voxel with
numpy.
"""
        self.parent.hidden = True


class AstroMaskingSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroMaskingSelfTestLogic(ScriptedLoadableModuleLogic):
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


CUBE_CARDS = [
    ("CTYPE1", "RA---SIN"), ("CRVAL1", 180.), ("CDELT1", -0.002), ("CRPIX1", 12.5), ("CUNIT1", "deg"),
    ("CTYPE2", "DEC--SIN"), ("CRVAL2", 30.), ("CDELT2", 0.002), ("CRPIX2", 10.5), ("CUNIT2", "deg"),
    ("CTYPE3", "VRAD"), ("CRVAL3", 100000.), ("CDELT3", 2000.), ("CRPIX3", 1.), ("CUNIT3", "m/s"),
    ("RESTFREQ", 1.420405752E9), ("SPECSYS", "BARYCENT"), ("RADESYS", "FK5"), ("EQUINOX", 2000.),
    ("BUNIT", "JY/BEAM"), ("BMAJ", 0.006), ("BMIN", 0.006), ("BPA", 0.)]


class AstroMaskingSelfTestTest(ScriptedLoadableModuleTest):
    SHAPE = (12, 20, 24)  # (z, y, x)

    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroMaskingSelfTest")
        if os.path.isdir(self.directory):
            shutil.rmtree(self.directory)
        os.makedirs(self.directory)

    def runTest(self):
        for test in (self.test_BlankInsideBox, self.test_BlankOutsideBoxNaN, self.test_CropBox,
                     self.test_BlankWithMask, self.test_CropWithMask, self.test_BoxOutsideCube,
                     self.test_MaskDoubleSharedLayer):
            self.setUp()
            test()

    #
    # helpers
    #

    def cube(self):
        """Synthetic cube: every voxel has a different value (1, 2, 3, ...)."""
        data = (np.arange(np.prod(self.SHAPE), dtype=np.float32) + 1.).reshape(self.SHAPE)
        path = os.path.join(self.directory, "cube.fits")
        writeFITS(path, data, CUBE_CARDS)
        node = slicer.util.loadVolume(path)
        self.assertIsNotNone(node)
        np.testing.assert_array_equal(voxelArray(node), data)
        return node, data

    def openModule(self, volumeNode):
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActiveVolumeID(volumeNode.GetID())
        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroMasking")
        widget = slicer.modules.astromasking.widgetRepresentation()
        parametersNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroMaskingParametersNode")
        self.assertIsNotNone(parametersNode)
        parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
        return widget, parametersNode

    def setBox(self, parametersNode, volumeNode, first, last):
        """Box (ROI) covering the voxels first..last (i, j, k, inclusive)."""
        ijkToRAS = vtk.vtkMatrix4x4()
        volumeNode.GetIJKToRASMatrix(ijkToRAS)
        corners = []
        for i in (first[0] - 0.5, last[0] + 0.5):
            for j in (first[1] - 0.5, last[1] + 0.5):
                for k in (first[2] - 0.5, last[2] + 0.5):
                    corners.append(ijkToRAS.MultiplyPoint((i, j, k, 1.))[:3])
        corners = np.array(corners)
        roiNode = parametersNode.GetROINode()
        self.assertIsNotNone(roiNode)
        roiNode.SetXYZ(*((corners.min(axis=0) + corners.max(axis=0)) / 2.))
        roiNode.SetRadiusXYZ(*((corners.max(axis=0) - corners.min(axis=0)) / 2.))
        bounds = [0.] * 6
        self.assertTrue(slicer.modules.astrovolume.logic().CalculateROICropVolumeBounds(roiNode, volumeNode, bounds))
        self.assertEqual([round(b) for b in bounds], [first[0], last[0], first[1], last[1], first[2], last[2]])

    def apply(self, widget, parametersNode):
        slicer.util.findChild(widget, "ApplyButton").click()
        outputNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        self.assertIsNotNone(outputNode)
        return outputNode

    def maskSegmentation(self, volumeNode, mask):
        """Segmentation with one segment equal to the boolean (z, y, x) mask."""
        segmentationNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentationNode", "TestMask")
        segmentationNode.CreateDefaultDisplayNodes()
        segmentationNode.SetReferenceImageGeometryParameterFromVolumeNode(volumeNode)
        segmentID = segmentationNode.GetSegmentation().AddEmptySegment("Source", "Source")
        slicer.util.updateSegmentBinaryLabelmapFromArray(mask.astype(np.uint8), segmentationNode,
                                                         segmentID, volumeNode)
        return segmentationNode, segmentID

    def selectSegment(self, widget, segmentationNode, segmentID):
        slicer.util.findChild(widget, "RegionButton").setCurrentIndex(1)
        slicer.util.findChild(widget, "SegmentationNodeComboBox").setCurrentNode(segmentationNode)
        slicer.app.processEvents()
        slicer.util.findChild(widget, "SegmentsTableView").setSelectedSegmentIDs([segmentID])
        slicer.app.processEvents()

    def world(self, volumeNode, ijk):
        world = [0., 0., 0.]
        self.assertTrue(volumeNode.GetAstroVolumeDisplayNode().GetReferenceSpace([float(v) for v in ijk], world))
        return np.array(world)

    #
    # tests
    #

    def test_BlankInsideBox(self):
        volumeNode, data = self.cube()
        widget, parametersNode = self.openModule(volumeNode)
        parametersNode.SetMode("ROI")
        parametersNode.SetOperation("Blank")
        parametersNode.SetBlankRegion("Inside")
        parametersNode.SetBlankValue("-5.5")
        self.setBox(parametersNode, volumeNode, (3, 4, 2), (15, 11, 8))
        outputNode = self.apply(widget, parametersNode)

        expected = data.copy()
        expected[2:9, 4:12, 3:16] = -5.5
        np.testing.assert_array_equal(voxelArray(outputNode), expected)
        # the input is not modified
        np.testing.assert_array_equal(voxelArray(volumeNode), data)

    def test_BlankOutsideBoxNaN(self):
        volumeNode, data = self.cube()
        widget, parametersNode = self.openModule(volumeNode)
        parametersNode.SetMode("ROI")
        parametersNode.SetOperation("Blank")
        parametersNode.SetBlankRegion("Outside")
        parametersNode.SetBlankValue("NaN")
        self.setBox(parametersNode, volumeNode, (0, 5, 1), (9, 19, 10))
        outputNode = self.apply(widget, parametersNode)

        expected = np.full(self.SHAPE, np.nan, dtype=np.float32)
        expected[1:11, 5:20, 0:10] = data[1:11, 5:20, 0:10]
        np.testing.assert_array_equal(voxelArray(outputNode), expected)

    def test_CropBox(self):
        volumeNode, data = self.cube()
        widget, parametersNode = self.openModule(volumeNode)
        parametersNode.SetMode("ROI")
        parametersNode.SetOperation("Crop")
        first, last = (4, 3, 2), (17, 14, 9)
        self.setBox(parametersNode, volumeNode, first, last)
        outputNode = self.apply(widget, parametersNode)

        cropped = voxelArray(outputNode)
        self.assertEqual(cropped.shape, (last[2] - first[2] + 1, last[1] - first[1] + 1, last[0] - first[0] + 1))
        np.testing.assert_array_equal(cropped, data[first[2]:last[2] + 1, first[1]:last[1] + 1, first[0]:last[0] + 1])
        # the cropped cube keeps the world coordinates of its voxels
        for ijk in ((0, 0, 0), (5, 7, 3), (13, 11, 7)):
            inputIJK = [ijk[0] + first[0], ijk[1] + first[1], ijk[2] + first[2]]
            np.testing.assert_allclose(self.world(outputNode, ijk), self.world(volumeNode, inputIJK),
                                       rtol=0., atol=1e-6)

    def test_BlankWithMask(self):
        volumeNode, data = self.cube()
        z, y, x = np.indices(self.SHAPE)
        mask = ((x - 11.) ** 2 / 36. + (y - 9.) ** 2 / 25. + (z - 6.) ** 2 / 9.) <= 1.
        segmentationNode, segmentID = self.maskSegmentation(volumeNode, mask)
        widget, parametersNode = self.openModule(volumeNode)
        parametersNode.SetOperation("Blank")
        parametersNode.SetBlankRegion("Outside")
        parametersNode.SetBlankValue("0")
        self.selectSegment(widget, segmentationNode, segmentID)
        self.assertEqual(parametersNode.GetMode(), "Segmentation")
        outputNode = self.apply(widget, parametersNode)

        np.testing.assert_array_equal(voxelArray(outputNode), np.where(mask, data, 0.).astype(np.float32))

    def test_CropWithMask(self):
        volumeNode, data = self.cube()
        mask = np.zeros(self.SHAPE, dtype=bool)
        mask[3:7, 5:9, 6:12] = True
        mask[4, 9, 12] = True  # one voxel extends the bounding box
        segmentationNode, segmentID = self.maskSegmentation(volumeNode, mask)
        widget, parametersNode = self.openModule(volumeNode)
        parametersNode.SetOperation("Crop")
        self.selectSegment(widget, segmentationNode, segmentID)
        outputNode = self.apply(widget, parametersNode)

        # bounding box of the mask; voxels outside the mask are blank (NaN)
        box = (slice(3, 7), slice(5, 10), slice(6, 13))
        expected = np.where(mask[box], data[box], np.nan).astype(np.float32)
        np.testing.assert_array_equal(voxelArray(outputNode), expected)

    def test_MaskDoubleSharedLayer(self):
        """Blank and crop of a 64-bit cube with a segment that shares its
        labelmap layer with another segment: only the selected segment is the
        mask, read directly (no label map volume)."""
        from AstroVolumeLogicTest import doublePrecisionCopy, maskSegmentation
        floatNode, data = self.cube()
        volumeNode = doublePrecisionCopy(floatNode, "cube_double")
        z, y, x = np.indices(self.SHAPE)
        ellipsoid = ((x - 11.) ** 2 / 36. + (y - 9.) ** 2 / 25. + (z - 6.) ** 2 / 9.) <= 1.
        corner = np.zeros(self.SHAPE, dtype=bool)
        corner[:3, :3, :3] = True
        segmentationNode, (ellipsoidID, cornerID) = maskSegmentation(
            volumeNode, {"Ellipsoid": ellipsoid, "Corner": corner})
        self.assertEqual(segmentationNode.GetSegmentation().GetNumberOfLayers(), 1)

        widget, parametersNode = self.openModule(volumeNode)
        parametersNode.SetOperation("Blank")
        parametersNode.SetBlankRegion("Outside")
        parametersNode.SetBlankValue("0")
        self.selectSegment(widget, segmentationNode, ellipsoidID)
        outputNode = self.apply(widget, parametersNode)
        self.assertEqual(parametersNode.GetMaskSegmentationNodeID(), segmentationNode.GetID())
        self.assertEqual(parametersNode.GetMaskSegmentID(), ellipsoidID)
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLLabelMapVolumeNode"), 0)
        self.assertEqual(outputNode.GetImageData().GetScalarType(), vtk.VTK_DOUBLE)
        np.testing.assert_array_equal(voxelArray(outputNode), np.where(ellipsoid, data.astype(np.float64), 0.))
        # the input is unchanged
        np.testing.assert_array_equal(voxelArray(volumeNode), data.astype(np.float64))

        parametersNode.SetOperation("Crop")
        self.selectSegment(widget, segmentationNode, cornerID)
        outputNode = self.apply(widget, parametersNode)
        self.assertEqual(parametersNode.GetMaskSegmentID(), cornerID)
        np.testing.assert_array_equal(voxelArray(outputNode), data[:3, :3, :3].astype(np.float64))

    def test_BoxOutsideCube(self):
        """A box that does not overlap the cube crops nothing (no crash, no empty output)."""
        volumeNode, data = self.cube()
        widget, parametersNode = self.openModule(volumeNode)
        parametersNode.SetMode("ROI")
        parametersNode.SetOperation("Crop")
        roiNode = parametersNode.GetROINode()
        center = [0., 0., 0.]
        roiNode.GetXYZ(center)
        roiNode.SetXYZ(center[0] + 1.e4, center[1] + 1.e4, center[2] + 1.e4)
        roiNode.SetRadiusXYZ(1., 1., 1.)
        slicer.util.findChild(widget, "ApplyButton").click()
        outputNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID() or "")
        if outputNode is not None and outputNode.GetImageData() is not None:
            # nothing was cropped: the output is not a 0-voxel cube
            self.assertGreater(outputNode.GetImageData().GetNumberOfPoints(), 0)
        np.testing.assert_array_equal(voxelArray(volumeNode), data)
