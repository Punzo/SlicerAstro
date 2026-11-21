import os

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
                     self.test_SegmentMask, self.test_BeamInInfo, self.test_AllBlankCube,
                     self.test_DisplayHistogramAndWindow, self.test_ContoursSection, self.test_InfoSection):
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

    def test_BeamInInfo(self):
        """Beam in the Info section of the AstroVolume module: the beam of the
        header is shown; without beam nothing is estimated until the user
        asks; the estimate fills the fields (not applied); Apply sets the beam
        of the data (fluxes in Jy)."""
        self.delayDisplay("Beam in the AstroVolume module")
        data = self.correlatedNoise((3, 160, 160), 8.0, 5.0, 30.0, seed=11)
        volumeNode = self.loadCube("info_nobeam", data)
        self.assertEqual(volumeNode.GetAttribute("SlicerAstro.BMAJ"), "UNDEFINED")
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        slicer.util.selectModule("AstroVolume")
        slicer.app.processEvents()
        widget = slicer.modules.astrovolume.widgetRepresentation().infoSection()
        major = slicer.util.findChild(widget, "InfoBeam_BMAJ")
        minor = slicer.util.findChild(widget, "InfoBeam_BMIN")
        angle = slicer.util.findChild(widget, "InfoBeam_BPA")
        # not estimated automatically
        self.assertEqual(major.value, 0.)
        self.assertIn("no beam", widget.beamStatus().lower())
        self.assertEqual(volumeNode.GetAttribute("SlicerAstro.BMAJ"), "UNDEFINED")
        self.assertEqual(slicer.vtkSlicerAstroVolumeLogic.GetJyBeamToJyFactor(volumeNode), 0.)

        # estimate: the fields, not the data
        slicer.util.findChild(widget, "InfoEstimateBeam").click()
        self.assertAlmostEqual(major.value / (8.0 * 3.6), 1.0, delta=0.1)
        self.assertAlmostEqual(minor.value / (5.0 * 3.6), 1.0, delta=0.1)
        self.assertLess(abs(angle.value - 30.), 8.)
        self.assertIn("ESTIMATED", widget.beamStatus())
        self.assertEqual(volumeNode.GetAttribute("SlicerAstro.BMAJ"), "UNDEFINED")

        # apply: the beam of the data
        slicer.util.findChild(widget, "InfoApplyBeam").click()
        self.assertAlmostEqual(float(volumeNode.GetAttribute("SlicerAstro.BMAJ")) * 3600., major.value, places=6)
        self.assertAlmostEqual(float(volumeNode.GetAttribute("SlicerAstro.BMIN")) * 3600., minor.value, places=6)
        self.assertAlmostEqual(float(volumeNode.GetAttribute("SlicerAstro.BPA")), angle.value, places=6)
        self.assertGreater(slicer.vtkSlicerAstroVolumeLogic.GetJyBeamToJyFactor(volumeNode), 0.)
        self.assertIn("applied", widget.beamStatus())

        # a beam entered by the user
        major.value = 40.
        minor.value = 30.
        angle.value = -10.
        self.assertIn("not applied", widget.beamStatus())
        slicer.util.findChild(widget, "InfoApplyBeam").click()
        self.assertAlmostEqual(float(volumeNode.GetAttribute("SlicerAstro.BMAJ")), 40. / 3600., places=12)
        self.assertAlmostEqual(float(volumeNode.GetAttribute("SlicerAstro.BPA")), -10., places=12)

        # the beam of a header keeps its precision (fields with 2 decimals)
        withBeam = self.loadCube("info_beam", data, extraCards=[("BMAJ", 0.0082401093), ("BMIN", 0.0081396336),
                                                                ("BPA", -79.44637)])
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(withBeam.GetID())
        slicer.app.processEvents()
        self.assertAlmostEqual(major.value, 0.0082401093 * 3600., places=2)
        self.assertNotIn("no beam", widget.beamStatus().lower())
        header = withBeam.GetAttribute("SlicerAstro.BMAJ")
        slicer.util.findChild(widget, "InfoApplyBeam").click()
        self.assertEqual(withBeam.GetAttribute("SlicerAstro.BMAJ"), header)
        self.delayDisplay("Test passed")

    def test_AllBlankCube(self):
        """A cube without finite voxels loaded with the AstroVolume module
        shown: an empty data range (it crashed with the limits of the float
        type as range)."""
        slicer.util.selectModule("AstroVolume")
        slicer.app.processEvents()
        volumeNode = self.loadCube("allblank", np.full((2, 32, 32), np.nan, dtype=np.float32))
        slicer.app.processEvents()
        self.assertEqual(float(volumeNode.GetAttribute("SlicerAstro.DATAMIN")), 0.)
        self.assertEqual(float(volumeNode.GetAttribute("SlicerAstro.DATAMAX")), 0.)

    def test_DisplayHistogramAndWindow(self):
        """Histogram of the finite voxels and window of the 2D colours with a
        percentage of the negative and of the positive values (numpy
        quantiles); the Display section applies them to the display node."""
        rng = np.random.default_rng(11)
        data = rng.normal(0.0, 1.0, (6, 30, 40)).astype(np.float32)
        data[2, 10:14, 10:16] += 25.0
        data[0, 0, :5] = np.nan
        volume = self.loadCube("display", data)
        logic = slicer.vtkSlicerAstroVolumeLogic
        # loaded data: the window of the 2D colours is the 99.5% clipping (the
        # bright source does not hide the rest)
        values = data[np.isfinite(data)].astype(np.float64)
        loadedWindow = (np.quantile(values[values < 0], 0.005), np.quantile(values[values > 0], 0.995))
        displayNode = volume.GetDisplayNode()
        self.assertAlmostEqual(displayNode.GetWindow(), loadedWindow[1] - loadedWindow[0], places=5)
        self.assertAlmostEqual(displayNode.GetLevel(), (loadedWindow[1] + loadedWindow[0]) / 2, places=5)
        centres, counts = vtk.vtkDoubleArray(), vtk.vtkDoubleArray()
        self.assertTrue(logic.ComputeHistogram(volume, 50, centres, counts))
        values = data[np.isfinite(data)].astype(np.float64)
        expectedCounts, edges = np.histogram(values, bins=50, range=(values.min(), values.max()))
        np.testing.assert_allclose([counts.GetValue(i) for i in range(50)], expectedCounts)
        np.testing.assert_allclose([centres.GetValue(i) for i in range(50)], (edges[:-1] + edges[1:]) / 2, rtol=1e-12)
        for percentage in (95.0, 99.5, 100.0):
            window = [0.0, 0.0]
            self.assertTrue(logic.ComputeClippedWindow(volume, percentage, window))
            if percentage == 100.0:
                expected = (values.min(), values.max())
            else:
                expected = (np.quantile(values[values < 0], 1 - percentage / 100),
                            np.quantile(values[values > 0], percentage / 100))
            np.testing.assert_allclose(window, expected, rtol=1e-9)

        # the Display section (reusable widget of the AstroVolume panel and
        # the Workbench)
        section = slicer.qSlicerAstroDisplaySection()
        section.setVolumeNode(volume)
        section.applyPercentage(section.percentages().index(99.5))
        displayNode = volume.GetDisplayNode()
        window = [0.0, 0.0]
        logic.ComputeClippedWindow(volume, 99.5, window)
        self.assertAlmostEqual(displayNode.GetWindow(), window[1] - window[0], places=6)
        self.assertAlmostEqual(displayNode.GetLevel(), (window[1] + window[0]) / 2, places=6)
        section.setNoise(2.0)
        self.assertAlmostEqual(volume.GetDisplayThreshold(), 2.0)
        histogram = slicer.util.findChild(section, "DisplayHistogram")
        self.assertEqual(int(sum(histogram.binCounts())), values.size)
        self.assertAlmostEqual(histogram.noise, 2.0)

        # zoom on the noise: the bins of the visible values (logic
        # ComputeHistogramInRange), all the values again with a double click
        histogram.resize(380, 150)
        histogram.setViewRange(-3.0, 9.0)
        self.assertTrue(histogram.isZoomed())
        centres = list(histogram.binCentres())
        self.assertAlmostEqual(centres[0] - (centres[1] - centres[0]) / 2, -3.0, places=9)
        inRange = values[(values >= -3.0) & (values <= 9.0)]
        self.assertEqual(int(sum(histogram.binCounts())), inRange.size)
        rangeCentres, rangeCounts = vtk.vtkDoubleArray(), vtk.vtkDoubleArray()
        self.assertTrue(logic.ComputeHistogramInRange(volume, 40, -3.0, 9.0, rangeCentres, rangeCounts))
        expectedCounts, edges = np.histogram(inRange, bins=40, range=(-3.0, 9.0))
        np.testing.assert_allclose([rangeCounts.GetValue(i) for i in range(40)], expectedCounts)

        # the σ handle is dragged: the noise of the data follows
        handle = histogram.noiseHandlePosition()
        self.assertGreater(handle.x(), 0)

        def mouse(eventType, x, y, buttons):
            event = qt.QMouseEvent(eventType, qt.QPointF(x, y), qt.QPointF(x, y), qt.Qt.LeftButton, buttons,
                                   qt.Qt.NoModifier)
            slicer.app.sendEvent(histogram, event)

        target = handle.x() + 40.0
        mouse(qt.QEvent.MouseButtonPress, handle.x(), handle.y(), qt.Qt.LeftButton)
        mouse(qt.QEvent.MouseMove, target, handle.y(), qt.Qt.LeftButton)
        # applied while dragging (the 3D colour functions follow live)
        import time
        end = time.time() + 0.2
        while time.time() < end:
            slicer.app.processEvents()
        self.assertGreater(volume.GetDisplayThreshold(), 2.0)
        mouse(qt.QEvent.MouseButtonRelease, target, handle.y(), qt.Qt.NoButton)
        self.assertGreater(histogram.noise, 2.0)
        self.assertAlmostEqual(volume.GetDisplayThreshold(), histogram.noise, places=9)
        self.assertGreater(histogram.noiseHandlePosition().x(), handle.x() + 30.0)
        # the window lines dragged: the 2D colours follow at once
        section.onHistogramWindowDragged(-1.0, 5.0)
        self.assertAlmostEqual(displayNode.GetWindow(), 6.0, places=9)
        self.assertAlmostEqual(displayNode.GetLevel(), 2.0, places=9)
        mouse(qt.QEvent.MouseButtonDblClick, 100.0, 100.0, qt.Qt.LeftButton)
        self.assertFalse(histogram.isZoomed())
        self.assertEqual(int(sum(histogram.binCounts())), values.size)
        self.delayDisplay("Test passed")

    def test_ContoursSection(self):
        """Contour colour families (interpolated between their stops), the
        contours of each source with their visibility, and the Contours
        section (reusable widget of the AstroVolume panel and the Workbench):
        levels in units of the noise, one colour per level, source rows,
        delete."""
        logic = slicer.vtkSlicerAstroVolumeLogic
        names = vtk.vtkStringArray()
        logic.GetContourColorFamilies(names)
        families = [names.GetValue(i) for i in range(names.GetNumberOfValues())]
        self.assertEqual(families, ["Amber", "Cyan", "Deep blue", "White", "Observatory ramp"])
        colours = vtk.vtkDoubleArray()
        self.assertTrue(logic.GetContourFamilyColors("Amber", 3, colours))
        self.assertEqual((colours.GetNumberOfTuples(), colours.GetNumberOfComponents()), (3, 3))
        np.testing.assert_allclose([colours.GetTuple3(i) for i in range(3)],
                                   np.array([(255, 236, 196), (242, 178, 76), (184, 104, 16)]) / 255.0, atol=1e-12)
        self.assertTrue(logic.GetContourFamilyColors("Amber", 9, colours))
        np.testing.assert_allclose(colours.GetTuple3(1), (np.array((255, 236, 196)) + (246, 194, 108)) / 2 / 255.0,
                                   atol=1e-12)
        # an unknown family gives the first one
        unknown = vtk.vtkDoubleArray()
        self.assertTrue(logic.GetContourFamilyColors("No such family", 3, unknown))
        self.assertEqual(unknown.GetTuple3(2), (184 / 255.0, 104 / 255.0, 16 / 255.0))
        self.assertFalse(logic.GetContourFamilyColors("Amber", 0, unknown))

        rng = np.random.default_rng(5)
        data = rng.normal(0.0, 1.0, (4, 40, 40)).astype(np.float32)
        yy, xx = np.mgrid[0:40, 0:40]
        data += (20.0 * np.exp(-((xx - 20) ** 2 + (yy - 20) ** 2) / (2 * 4.0 ** 2)))[None].astype(np.float32)
        volume = self.loadCube("contours", data)
        volume.SetDisplayThreshold(1.0)
        section = slicer.qSlicerAstroContoursSection()
        section.setVolumeNode(volume)
        section.setLevels("3, 6, 12", slicer.qSlicerAstroContoursSection.Sigma)
        self.assertEqual(section.drawContours(), 3)
        self.assertIn("3 levels drawn", section.status())

        contours = slicer.modules.astrovolume.logic().GetContoursSegmentationForVolume(volume, False)
        sourceIDs, levels = vtk.vtkStringArray(), vtk.vtkIntArray()
        logic.GetContourSources(contours, sourceIDs, levels)
        self.assertEqual((sourceIDs.GetNumberOfValues(), sourceIDs.GetValue(0), levels.GetValue(0)),
                         (1, volume.GetID(), 3))
        # one colour of the default family (Amber) per level, lowest first
        segmentation = contours.GetSegmentation()
        np.testing.assert_allclose([segmentation.GetNthSegment(i).GetColor() for i in range(3)],
                                   np.array([(255, 236, 196), (242, 178, 76), (184, 104, 16)]) / 255.0, atol=1e-6)
        self.assertTrue(logic.GetContourSourceVisibility(contours, volume.GetID()))

        # the source row toggles its contours, the delete button removes them
        eye = slicer.util.findChild(section, f"ContoursSourceShow_{volume.GetID()}")
        eye.click()
        self.assertFalse(logic.GetContourSourceVisibility(contours, volume.GetID()))
        logic.SetContourSourceVisibility(contours, volume.GetID(), True)
        self.assertTrue(logic.GetContourSourceVisibility(contours, volume.GetID()))
        slicer.util.findChild(section, "ContoursDelete").click()
        self.assertEqual(segmentation.GetNumberOfSegments(), 0)
        self.assertEqual(section.status(), "")

        # without a noise, levels in sigma are refused
        volume.SetDisplayThreshold(0.0)
        section.setLevels("3", slicer.qSlicerAstroContoursSection.Sigma)
        self.assertEqual(section.drawContours(), 0)
        self.assertIn("noise", section.status())
        section.setLevels("5", slicer.qSlicerAstroContoursSection.DataUnit)
        self.assertEqual(section.drawContours(), 1)
        self.delayDisplay("Test passed")

    def test_InfoSection(self):
        """Info section (reusable widget of the AstroVolume panel and the
        Workbench) and its logic: facts, beam (GetBeam, SetBeam: estimate,
        edit, apply; the header precision is kept when the beam is not
        changed), coordinate conventions (right ascension, velocity
        definition), channel width, FITS keywords with a filter."""
        logic = slicer.vtkSlicerAstroVolumeLogic
        data = self.correlatedNoise((3, 160, 160), 8.0, 5.0, 30.0, seed=11)
        volume = self.loadCube("info_section", data)
        beam = [0.0, 0.0, 0.0]
        self.assertFalse(logic.GetBeam(volume, beam))
        self.assertEqual(beam, [0.0, 0.0, 0.0])

        section = slicer.qSlicerAstroInfoSection()
        section.setVolumeNode(volume)
        self.assertEqual(section.fact("Object"), volume.GetName())
        self.assertEqual(section.fact("Dimensions"), "160×160×3")
        self.assertEqual(section.fact("Unit"), "Jy/beam")
        self.assertEqual(section.fact("Beam"), "—")
        self.assertIn("No beam", section.beamStatus())
        # channel width at the centre of the field (radioVelocityWCS)
        width = logic.GetChannelWidth(volume)
        self.assertGreater(width, 0.0)
        self.assertTrue(section.fact("Channel width").startswith(f"{width:.3g} "))

        # estimate (fields only), then apply
        self.assertTrue(section.estimateBeam())
        self.assertIn("ESTIMATED", section.beamStatus())
        self.assertFalse(logic.GetBeam(volume, beam))
        major = slicer.util.findChild(section, "InfoBeam_BMAJ")
        self.assertAlmostEqual(major.value / (8.0 * 3.6), 1.0, delta=0.1)
        self.assertTrue(section.applyBeam())
        self.assertTrue(logic.GetBeam(volume, beam))
        self.assertAlmostEqual(beam[0] * 3600.0, major.value, places=6)
        self.assertIn("applied", section.beamStatus())
        self.assertNotEqual(section.fact("Beam"), "—")

        # SetBeam: the larger axis is the major one; refused without axes
        self.assertTrue(logic.SetBeam(volume, [10.0 / 3600, 20.0 / 3600, -15.0]))
        self.assertTrue(logic.GetBeam(volume, beam))
        self.assertAlmostEqual(beam[0], 20.0 / 3600, places=15)
        self.assertAlmostEqual(beam[1], 10.0 / 3600, places=15)
        self.assertEqual(beam[2], -15.0)
        self.assertFalse(logic.SetBeam(volume, [0.0, 20.0 / 3600, 0.0]))
        # the fields follow the data; applying unchanged fields keeps the
        # full precision of the header
        precise = "0.0055555555555555558"
        volume.SetAttribute("SlicerAstro.BMAJ", precise)
        self.assertAlmostEqual(major.value, 20.0, places=2)
        self.assertTrue(section.applyBeam())
        self.assertEqual(volume.GetAttribute("SlicerAstro.BMAJ"), precise)

        # coordinate conventions
        raFormat = slicer.util.findChild(section, "InfoRightAscension")
        raFormat.setCurrentIndex(1)
        self.assertFalse(logic.IsRightAscensionSexagesimal(volume))
        raFormat.setCurrentIndex(0)
        self.assertTrue(logic.IsRightAscensionSexagesimal(volume))
        velocity = slicer.util.findChild(section, "InfoVelocity")
        velocity.setCurrentIndex(1)
        self.assertTrue(logic.IsOpticalVelocityDefinition(volume))
        velocity.setCurrentIndex(0)
        self.assertFalse(logic.IsOpticalVelocityDefinition(volume))

        # FITS keywords, filtered
        total = section.keywordRowCount()
        self.assertGreater(total, 10)
        section.setKeywordFilter("BMAJ")
        self.assertEqual(section.keywordRowCount(), 1)
        section.setKeywordFilter("")
        self.assertEqual(section.keywordRowCount(), total)
        section.setVolumeNode(None)
        self.assertFalse(section.enabled)
        self.assertEqual(section.fact("Object"), "")
        self.delayDisplay("Test passed")
