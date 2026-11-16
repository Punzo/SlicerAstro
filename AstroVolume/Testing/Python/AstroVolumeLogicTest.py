import os

import numpy as np

import slicer
import vtk
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroVolumeLogicTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Volume Logic Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Functions of the AstroVolume logic, against analytic or numpy results: the beam
estimated from correlated noise, intensity histograms (single and double
precision cubes), the alignment of ROIs to the voxel grid of a volume and the
input masks of the modules (segments on the voxel grid of a volume).
"""
        self.parent.hidden = True


class AstroVolumeLogicTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroVolumeLogicTestLogic(ScriptedLoadableModuleLogic):
    pass


def maskSegmentation(volumeNode, masks, name="TestMask", sharedLayer=True):
    """Segmentation on the grid of volumeNode with one segment per boolean
    (z, y, x) mask of masks (dict name -> array); the segments share one
    labelmap layer (label values 1, 2, ...) if sharedLayer. Returns the
    segmentation node and the segment IDs (in the order of masks)."""
    segmentationNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentationNode", name)
    segmentationNode.CreateDefaultDisplayNodes()
    segmentationNode.SetReferenceImageGeometryParameterFromVolumeNode(volumeNode)
    segmentIDs = []
    for segmentName, mask in masks.items():
        segmentID = segmentationNode.GetSegmentation().AddEmptySegment(segmentName, segmentName)
        slicer.util.updateSegmentBinaryLabelmapFromArray(mask.astype(np.uint8), segmentationNode,
                                                         segmentID, volumeNode)
        segmentIDs.append(segmentID)
    if sharedLayer:
        segmentationNode.GetSegmentation().CollapseBinaryLabelmaps(False)
    return segmentationNode, segmentIDs


def segmentMask(segmentationNode, segmentID, volumeNode):
    """Mask of the segment on the voxel grid of the volume (z, y, x), by the logic."""
    from vtk.util.numpy_support import vtk_to_numpy
    mask = vtk.vtkImageData()
    if not slicer.vtkSlicerAstroVolumeLogic.CreateSegmentMask(segmentationNode, segmentID, volumeNode, mask):
        return None
    dims = mask.GetDimensions()
    return vtk_to_numpy(mask.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0]).copy()


def doublePrecisionCopy(volumeNode, name):
    """Copy of an astronomical volume with 64-bit floating point voxels (BITPIX -64)."""
    logic = slicer.modules.astrovolume.logic()
    copyNode = logic.CloneAstroVolume(slicer.mrmlScene, volumeNode, None, "_double", name)
    cast = vtk.vtkImageCast()
    cast.SetInputData(volumeNode.GetImageData())
    cast.SetOutputScalarTypeToDouble()
    cast.Update()
    imageData = vtk.vtkImageData()
    imageData.DeepCopy(cast.GetOutput())
    copyNode.SetAndObserveImageData(imageData)
    copyNode.SetAttribute("SlicerAstro.BITPIX", "-64")
    return copyNode


class AstroVolumeLogicTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.logic = slicer.modules.astrovolume.logic()
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroVolumeLogicTest")
        os.makedirs(self.directory, exist_ok=True)

    def runTest(self):
        for test in (self.test_EstimateBeamFromNoise, self.test_Histogram, self.test_SnapROIToVoxelGrid,
                     self.test_SegmentMask):
            self.setUp()
            test()

    #
    # helpers
    #

    def loadCube(self, name, data, cdelt=(-0.001, 0.001), extraCards=()):
        from AstroFITSIOTest import celestialWCS, radioVelocityWCS, writeFITS
        path = os.path.join(self.directory, name + ".fits")
        writeFITS(path, data, -32, celestialWCS(crpix=(data.shape[2] / 2, data.shape[1] / 2), cdelt=cdelt) +
                  radioVelocityWCS() + [("BUNIT", "JY/BEAM")] + list(extraCards))
        node = slicer.util.loadVolume(path)
        self.assertIsNotNone(node)
        return node

    @staticmethod
    def correlatedNoise(shape, fwhmMajor, fwhmMinor, positionAngle, seed):
        """White noise convolved (FFT, periodic) with a Gaussian beam; FWHM in
        pixels, position angle in degrees from North (+y) to East (-x, as
        CDELT1 < 0)."""
        rng = np.random.default_rng(seed)
        noise = rng.standard_normal(shape)
        ny, nx = shape[1], shape[2]
        y = np.fft.fftfreq(ny) * ny
        x = np.fft.fftfreq(nx) * nx
        xx, yy = np.meshgrid(x, y)
        angle = np.radians(positionAngle)
        major = -xx * np.sin(angle) + yy * np.cos(angle)
        minor = xx * np.cos(angle) + yy * np.sin(angle)
        toSigma = 1.0 / (2.0 * np.sqrt(2.0 * np.log(2.0)))
        kernel = np.exp(-0.5 * (major / (fwhmMajor * toSigma)) ** 2 - 0.5 * (minor / (fwhmMinor * toSigma)) ** 2)
        kernelFFT = np.fft.fft2(kernel)
        return np.real(np.fft.ifft2(np.fft.fft2(noise, axes=(1, 2)) * kernelFFT, axes=(1, 2))).astype(np.float32)

    #
    # tests
    #

    def test_EstimateBeamFromNoise(self):
        """The beam of interferometric noise (noise correlated on the scale of
        the beam); no beam for uncorrelated noise and too small images."""
        self.delayDisplay("Beam from the noise")
        cdelt = 0.001
        for fwhmMajor, fwhmMinor, positionAngle in ((8.0, 5.0, 30.0), (6.0, 6.0, 0.0), (9.0, 4.0, 125.0)):
            data = self.correlatedNoise((3, 160, 160), fwhmMajor, fwhmMinor, positionAngle, seed=11)
            node = self.loadCube(f"beam_{positionAngle:.0f}", data)
            beam = [0.0, 0.0, 0.0]
            self.assertTrue(slicer.vtkSlicerAstroVolumeLogic.EstimateBeamFromNoise(node, beam))
            message = f"beam {fwhmMajor}x{fwhmMinor} px at {positionAngle} deg: estimated {beam}"
            self.assertAlmostEqual(beam[0] / (fwhmMajor * cdelt), 1.0, delta=0.1, msg=message)
            self.assertAlmostEqual(beam[1] / (fwhmMinor * cdelt), 1.0, delta=0.1, msg=message)
            if fwhmMajor > 1.2 * fwhmMinor:
                difference = (beam[2] - positionAngle + 90.0) % 180.0 - 90.0
                self.assertLess(abs(difference), 8.0, message)

        # uncorrelated noise (e.g. single dish channels, optical images)
        rng = np.random.default_rng(3)
        node = self.loadCube("white", rng.standard_normal((3, 96, 96)).astype(np.float32))
        beam = [0.0, 0.0, 0.0]
        self.assertFalse(slicer.vtkSlicerAstroVolumeLogic.EstimateBeamFromNoise(node, beam))
        # too small for the autocorrelation
        node = self.loadCube("small", self.correlatedNoise((2, 10, 10), 3.0, 3.0, 0.0, seed=5))
        self.assertFalse(slicer.vtkSlicerAstroVolumeLogic.EstimateBeamFromNoise(node, beam))
        # mostly blank channels are skipped: blank cube, no estimate
        node = self.loadCube("blank", np.full((2, 64, 64), np.nan, dtype=np.float32))
        self.assertFalse(slicer.vtkSlicerAstroVolumeLogic.EstimateBeamFromNoise(node, beam))
        self.delayDisplay("Test passed")

    def test_Histogram(self):
        """Histograms of float and double cubes (blank voxels left out, the
        maximum in the last bin) equal numpy's."""
        self.delayDisplay("Histograms")
        rng = np.random.default_rng(7)
        data = rng.normal(0.0, 1.0, (6, 30, 40)).astype(np.float32)
        data[0, 0, :5] = np.nan
        floatNode = self.loadCube("histogram", data)
        doubleNode = doublePrecisionCopy(floatNode, "histogram_double")
        self.assertEqual(doubleNode.GetImageData().GetScalarType(), vtk.VTK_DOUBLE)

        dataMin = float(floatNode.GetAttribute("SlicerAstro.DATAMIN"))
        dataMax = float(floatNode.GetAttribute("SlicerAstro.DATAMAX"))
        self.assertAlmostEqual(dataMin, float(np.nanmin(data)), places=5)
        numberOfBins = 50
        binSpacing = (dataMax - dataMin) / (numberOfBins - 1)
        values = data[~np.isnan(data)].astype(np.float64)
        indices = np.clip(np.floor((values - dataMin) / binSpacing).astype(int), 0, numberOfBins - 1)
        expected = np.bincount(indices, minlength=numberOfBins)
        for node in (floatNode, doubleNode):
            histogram = vtk.vtkIntArray()
            self.logic.CalculateHistogram(node, histogram, binSpacing, numberOfBins)
            computed = np.array([histogram.GetValue(i) for i in range(histogram.GetNumberOfValues())])
            np.testing.assert_array_equal(computed, expected, err_msg=node.GetName())
            self.assertEqual(computed.sum(), values.size)

        # invalid bins: no histogram
        histogram = vtk.vtkIntArray()
        self.logic.CalculateHistogram(floatNode, histogram, 0.0, numberOfBins)
        self.assertEqual(histogram.GetNumberOfValues(), 0)
        self.delayDisplay("Test passed")

    def test_SegmentMask(self):
        """Input masks of the modules: a segment on the voxel grid of the data,
        read from its labelmap (segments sharing a layer told apart by their
        label value), resampled when the segmentation is transformed,
        converted from closed surfaces; empty and missing segments."""
        self.delayDisplay("Masks from segments")
        shape = (10, 16, 20)
        volumeNode = self.loadCube("segmentmask", np.zeros(shape, dtype=np.float32))
        z, y, x = np.indices(shape)
        ellipsoid = ((x - 9.) ** 2 / 25. + (y - 8.) ** 2 / 16. + (z - 5.) ** 2 / 9.) <= 1.
        box = np.zeros(shape, dtype=bool)
        box[1:4, 2:6, 13:18] = True
        segmentationNode, (ellipsoidID, boxID) = maskSegmentation(
            volumeNode, {"Ellipsoid": ellipsoid, "Box": box})
        segmentation = segmentationNode.GetSegmentation()
        self.assertEqual(segmentation.GetNumberOfLayers(), 1)
        self.assertNotEqual(segmentation.GetSegment(ellipsoidID).GetLabelValue(),
                            segmentation.GetSegment(boxID).GetLabelValue())

        # each segment of the shared layer, on the grid of the data (1 inside)
        for segmentID, expected in ((ellipsoidID, ellipsoid), (boxID, box)):
            mask = segmentMask(segmentationNode, segmentID, volumeNode)
            self.assertIsNotNone(mask)
            self.assertEqual(mask.shape, shape)
            self.assertEqual(mask.dtype, np.uint8)
            np.testing.assert_array_equal(mask, expected.astype(np.uint8), err_msg=segmentID)

        # segmentation moved by 3 voxels along i (transform): resampled
        ijkToRAS = vtk.vtkMatrix4x4()
        volumeNode.GetIJKToRASMatrix(ijkToRAS)
        shift = [3. * ijkToRAS.GetElement(row, 0) for row in range(3)]
        transformNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLLinearTransformNode", "Shift")
        matrix = vtk.vtkMatrix4x4()
        for row in range(3):
            matrix.SetElement(row, 3, shift[row])
        transformNode.SetMatrixTransformToParent(matrix)
        segmentationNode.SetAndObserveTransformNodeID(transformNode.GetID())
        shifted = np.zeros(shape, dtype=np.uint8)
        shifted[:, :, 3:] = box[:, :, :-3]
        np.testing.assert_array_equal(segmentMask(segmentationNode, boxID, volumeNode), shifted)
        segmentationNode.SetAndObserveTransformNodeID(None)

        # closed surfaces as source representation: converted for the segment
        surfaceNode, (surfaceID,) = maskSegmentation(volumeNode, {"Ellipsoid": ellipsoid}, "Surfaces",
                                                     sharedLayer=False)
        surfaceSegmentation = surfaceNode.GetSegmentation()
        surfaceName = slicer.vtkSegmentationConverter.GetSegmentationClosedSurfaceRepresentationName()
        labelmapName = slicer.vtkSegmentationConverter.GetSegmentationBinaryLabelmapRepresentationName()
        self.assertTrue(surfaceSegmentation.CreateRepresentation(surfaceName))
        surfaceSegmentation.SetSourceRepresentationName(surfaceName)
        surfaceSegmentation.RemoveRepresentation(labelmapName)
        self.assertIsNone(surfaceSegmentation.GetSegment(surfaceID).GetRepresentation(labelmapName))
        mask = segmentMask(surfaceNode, surfaceID, volumeNode)
        self.assertIsNotNone(mask)
        overlap = np.count_nonzero(mask.astype(bool) & ellipsoid)
        dice = 2. * overlap / (np.count_nonzero(mask) + np.count_nonzero(ellipsoid))
        self.assertGreater(dice, 0.85)
        # the segmentation is not changed (no labelmap added to it)
        self.assertIsNone(surfaceSegmentation.GetSegment(surfaceID).GetRepresentation(labelmapName))

        # empty segment: empty mask; missing segment or volume: no mask
        emptyID = segmentation.AddEmptySegment("Empty", "Empty")
        np.testing.assert_array_equal(segmentMask(segmentationNode, emptyID, volumeNode),
                                      np.zeros(shape, dtype=np.uint8))
        self.assertIsNone(segmentMask(segmentationNode, "NoSuchSegment", volumeNode))
        self.assertIsNone(segmentMask(None, boxID, volumeNode))
        self.assertIsNone(segmentMask(segmentationNode, boxID, None))
        self.delayDisplay("Test passed")

    def test_SnapROIToVoxelGrid(self):
        """A ROI is aligned to the voxel grid of a rotated volume (alignment
        transform, the ROI covers the original region), and a rotated ROI to
        the grid of a volume that is not rotated (transform removed)."""
        self.delayDisplay("ROI aligned to the voxel grid")
        data = np.zeros((10, 12, 14), dtype=np.float32)
        volumeNode = self.loadCube("roi", data)
        bounds = [0.0] * 6
        volumeNode.GetRASBounds(bounds)
        center = [(bounds[2 * axis] + bounds[2 * axis + 1]) / 2.0 for axis in range(3)]

        def rotation(degrees, name):
            transform = vtk.vtkTransform()
            transform.Translate(center)
            transform.RotateWXYZ(degrees, 0.3, 0.2, 1.0)
            transform.Translate([-value for value in center])
            node = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLLinearTransformNode", name)
            node.SetMatrixTransformToParent(transform.GetMatrix())
            return node

        # rotated volume, ROI in RAS
        volumeTransform = rotation(30.0, "VolumeRotation")
        volumeNode.SetAndObserveTransformNodeID(volumeTransform.GetID())
        roiNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsROINode", "ROI")
        roiNode.SetXYZ(center)
        roiNode.SetRadiusXYZ(2.0, 3.0, 1.5)
        original = [0.0] * 6
        roiNode.GetRASBounds(original)
        self.assertFalse(self.logic.IsROIAlignedWithInputVolume(roiNode, volumeNode))
        self.logic.SnapROIToVoxelGrid(roiNode, volumeNode)
        self.assertTrue(self.logic.IsROIAlignedWithInputVolume(roiNode, volumeNode))
        alignmentNode = volumeNode.GetROIAlignmentTransformNode()
        self.assertIsNotNone(alignmentNode)
        self.assertEqual(roiNode.GetParentTransformNode(), alignmentNode)
        snapped = [0.0] * 6
        roiNode.GetRASBounds(snapped)
        for axis in range(3):
            self.assertLessEqual(snapped[2 * axis], original[2 * axis] + 1.e-6)
            self.assertGreaterEqual(snapped[2 * axis + 1], original[2 * axis + 1] - 1.e-6)
        # aligned: nothing changes
        roiNode.GetXYZ(center)
        self.logic.SnapROIToVoxelGrid(roiNode, volumeNode)
        after = [0.0] * 3
        roiNode.GetXYZ(after)
        for axis in range(3):
            self.assertAlmostEqual(after[axis], center[axis], places=9)

        # volume not rotated, ROI under a rotation: the transform is removed
        volumeNode.SetAndObserveTransformNodeID(None)
        volumeNode.GetRASBounds(bounds)
        center = [(bounds[2 * axis] + bounds[2 * axis + 1]) / 2.0 for axis in range(3)]
        roiNode.SetAndObserveTransformNodeID(rotation(20.0, "ROIRotation").GetID())
        roiNode.SetXYZ(center)
        roiNode.GetRASBounds(original)
        self.assertFalse(self.logic.IsROIAlignedWithInputVolume(roiNode, volumeNode))
        self.logic.SnapROIToVoxelGrid(roiNode, volumeNode)
        self.assertIsNone(roiNode.GetParentTransformNode())
        self.assertIsNone(volumeNode.GetROIAlignmentTransformNode())
        self.assertTrue(self.logic.IsROIAlignedWithInputVolume(roiNode, volumeNode))
        snapped = [0.0] * 6
        roiNode.GetRASBounds(snapped)
        for index in range(6):
            self.assertAlmostEqual(snapped[index], original[index], places=6)
        self.delayDisplay("Test passed")
