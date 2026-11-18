import os

import slicer
import vtk
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroVolumeSegmentationsTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Volume Segmentations Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Contours and mask segmentations of the astronomical volumes: one of each per
volume (created when the data are loaded, kept when a scene is saved and
loaded, not shared by copies of the volume), and the contours of the
AstroVolume module (one layer per level, replaced when created again, show,
smooth and delete buttons; contours of other data, reprojected on the grid
of the active volume).
"""
        self.parent.hidden = True


class AstroVolumeSegmentationsTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroVolumeSegmentationsTestLogic(ScriptedLoadableModuleLogic):
    pass


class AstroVolumeSegmentationsTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.logic = slicer.modules.astrovolume.logic()

    def runTest(self):
        self.setUp()
        self.test_VolumeSegmentations()
        self.setUp()
        self.test_LoadedMaskReplacesEmptyMask()
        self.setUp()
        self.test_Contours()
        self.setUp()
        self.test_ContoursOfOtherData()
        self.setUp()
        self.test_ContoursChannelMap()
        self.setUp()
        self.test_ContoursOfOtherCube()
        self.setUp()
        self.test_CopyMoveMask()
        self.setUp()
        self.test_MasksSection()

    def loadWEIN069(self):
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        return volumeNode

    def segmentationsOfVolume(self, volumeNode):
        contoursNode = self.logic.GetContoursSegmentationForVolume(volumeNode, False)
        maskNode = self.logic.GetMaskSegmentationForVolume(volumeNode, False)
        return contoursNode, maskNode

    def assertSegmentationsOfVolume(self, volumeNode):
        contoursNode, maskNode = self.segmentationsOfVolume(volumeNode)
        self.assertIsNotNone(contoursNode, volumeNode.GetName())
        self.assertIsNotNone(maskNode, volumeNode.GetName())
        self.assertIsNot(contoursNode, maskNode)
        self.assertEqual(contoursNode.GetName(), volumeNode.GetName() + "_Contours")
        self.assertEqual(maskNode.GetName(), volumeNode.GetName() + "_mask")
        self.assertIs(volumeNode.GetNodeReference(self.logic.GetContoursReferenceRole()), contoursNode)
        self.assertIs(volumeNode.GetNodeReference(self.logic.GetMaskReferenceRole()), maskNode)
        self.assertTrue(self.logic.IsContoursSegmentation(contoursNode))
        self.assertFalse(self.logic.IsContoursSegmentation(maskNode))
        geometryRole = slicer.vtkMRMLSegmentationNode.GetReferenceImageGeometryReferenceRole()
        self.assertIs(contoursNode.GetNodeReference(geometryRole), volumeNode)
        self.assertIs(maskNode.GetNodeReference(geometryRole), volumeNode)
        # masks are saved as FITS masks, contours are not saved as masks
        self.assertEqual(maskNode.GetStorageNode().GetClassName(), "vtkMRMLAstroSegmentationStorageNode")
        contoursStorage = contoursNode.GetStorageNode()
        self.assertTrue(contoursStorage is None or not contoursStorage.IsA("vtkMRMLAstroSegmentationStorageNode"))
        return contoursNode, maskNode

    def test_VolumeSegmentations(self):
        self.delayDisplay("Contours and mask segmentations of the volumes")
        volumeNode = self.loadWEIN069()
        contoursNode, maskNode = self.assertSegmentationsOfVolume(volumeNode)
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLSegmentationNode"), 2)
        # new masks: default opacity
        self.assertAlmostEqual(maskNode.GetDisplayNode().GetOpacity(), self.logic.GetDefaultMaskOpacity())

        # a copy of the volume (e.g. the output of a module) has its own segmentations
        copyNode = self.logic.CloneAstroVolume(slicer.mrmlScene, volumeNode, None, "_copy", "WEIN069_copy")
        self.assertIsNotNone(copyNode)
        copyContours = self.logic.GetContoursSegmentationForVolume(copyNode, True)
        copyMask = self.logic.GetMaskSegmentationForVolume(copyNode, True)
        self.assertIsNotNone(copyContours)
        self.assertIsNotNone(copyMask)
        self.assertIsNot(copyContours, contoursNode)
        self.assertIsNot(copyMask, maskNode)
        self.assertSegmentationsOfVolume(copyNode)
        self.assertEqual(self.segmentationsOfVolume(volumeNode), (contoursNode, maskNode))

        # saved and loaded again: the same segmentations, no new ones
        scenePath = os.path.join(slicer.app.temporaryPath, "AstroVolumeSegmentationsTest.mrb")
        if os.path.exists(scenePath):
            os.remove(scenePath)
        self.assertTrue(slicer.util.saveScene(scenePath))
        slicer.mrmlScene.Clear(0)
        slicer.util.loadScene(scenePath)
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLSegmentationNode"), 4)
        for name in ("WEIN069", "WEIN069_copy"):
            self.assertSegmentationsOfVolume(slicer.util.getNode(name))
        os.remove(scenePath)
        self.delayDisplay("Test passed")

    def test_Contours(self):
        self.delayDisplay("Contours of the AstroVolume module")
        volumeNode = self.loadWEIN069()
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActiveVolumeID(volumeNode.GetID())
        slicer.util.selectModule("AstroVolume")
        slicer.app.processEvents()
        moduleWidget = slicer.modules.astrovolume.widgetRepresentation()
        displayWidget = moduleWidget.findChild("qSlicerAstroScalarVolumeDisplayWidget")
        self.assertIsNotNone(displayWidget)
        contoursNode, maskNode = self.segmentationsOfVolume(volumeNode)
        contours = contoursNode.GetSegmentation()

        displayWidget.onCreateContours()
        slicer.app.processEvents()
        numberOfLevels = contours.GetNumberOfSegments()
        self.assertGreater(numberOfLevels, 0)
        # one labelmap layer per level (levels overlap), tagged with the volume
        self.assertEqual(contours.GetNumberOfLayers(), numberOfLevels)
        labelmapName = slicer.vtkSegmentationConverter.GetBinaryLabelmapRepresentationName()
        surfaceName = slicer.vtkSegmentationConverter.GetClosedSurfaceRepresentationName()
        self.assertEqual(contours.GetSourceRepresentationName(), labelmapName)
        for segmentID in contours.GetSegmentIDs():
            volumeID = vtk.reference("")
            self.assertTrue(contours.GetSegment(segmentID).GetTag(self.logic.GetContoursVolumeTagName(), volumeID))
            self.assertEqual(str(volumeID), volumeNode.GetID())
        # the mask is not modified
        self.assertEqual(maskNode.GetSegmentation().GetNumberOfSegments(), 0)

        # created again: replaced, not added
        displayWidget.onCreateContours()
        slicer.app.processEvents()
        self.assertEqual(contours.GetNumberOfSegments(), numberOfLevels)
        self.assertEqual(contours.GetNumberOfLayers(), numberOfLevels)

        # show / hide
        showButton = slicer.util.findChild(displayWidget, "ShowContoursButton")
        displayNode = contoursNode.GetDisplayNode()
        self.assertTrue(showButton.checked)
        showButton.click()
        self.assertFalse(displayNode.GetVisibility())
        showButton.click()
        self.assertTrue(displayNode.GetVisibility())

        # smooth: closed surfaces in 2D, binary labelmaps again when off
        smoothButton = slicer.util.findChild(displayWidget, "SmoothContoursButton")
        smoothButton.click()
        slicer.app.processEvents()
        self.assertTrue(contours.ContainsRepresentation(surfaceName))
        self.assertEqual(displayNode.GetPreferredDisplayRepresentationName2D(), surfaceName)
        self.assertEqual(displayNode.GetPreferredDisplayRepresentationName3D(), labelmapName)
        smoothButton.click()
        self.assertEqual(displayNode.GetPreferredDisplayRepresentationName2D(), labelmapName)

        # delete
        deleteButton = slicer.util.findChild(displayWidget, "DeleteContoursButton")
        deleteButton.click()
        self.assertEqual(contours.GetNumberOfSegments(), 0)
        self.assertIs(self.logic.GetContoursSegmentationForVolume(volumeNode, False), contoursNode)
        self.delayDisplay("Test passed")

    def test_LoadedMaskReplacesEmptyMask(self):
        """The mask of a cube loaded after the cube replaces the empty automatic mask."""
        import AstroSampleData

        self.delayDisplay("Mask loaded after its data cube")
        volumeNode = self.loadWEIN069()
        emptyMask = self.logic.GetMaskSegmentationForVolume(volumeNode, False)
        self.assertEqual(emptyMask.GetSegmentation().GetNumberOfSegments(), 0)
        AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069_MASK")
        slicer.app.processEvents()
        maskNode = self.logic.GetMaskSegmentationForVolume(volumeNode, False)
        self.assertIsNotNone(maskNode)
        self.assertGreater(maskNode.GetSegmentation().GetNumberOfSegments(), 0)
        self.assertEqual(maskNode.GetName(), "WEIN069_mask")
        self.assertIsNone(slicer.mrmlScene.GetNodeByID(emptyMask.GetID()))
        masks = [node for node in slicer.util.getNodesByClass("vtkMRMLSegmentationNode")
                 if not self.logic.IsContoursSegmentation(node)]
        self.assertEqual(len(masks), 1)
        self.delayDisplay("Test passed")

    def test_ContoursOfOtherData(self):
        """Contours of an image (Gaussian source, SIN grid) over another image
        (rotated TAN grid, other pixel size): the data of the contours are
        reprojected (AstroReproject module) on the grid of the active volume,
        and the areas of the levels are those of the source."""
        import math
        import numpy as np
        from AstroFITSIOTest import celestialWCS, voxelArray, writeFITS

        self.delayDisplay("Contours of other data")
        directory = os.path.join(slicer.app.temporaryPath, "AstroVolumeSegmentationsTest")
        os.makedirs(directory, exist_ok=True)
        fwhm = 0.03
        # the source, on a SIN grid
        size = 64
        sourcePath = os.path.join(directory, "source.fits")
        writeFITS(sourcePath, np.zeros((size, size), dtype=np.float32), -32,
                  celestialWCS(crpix=(32.5, 32.5), cdelt=(-0.002, 0.002)))
        sourceNode = slicer.util.loadVolume(sourcePath)
        display = sourceNode.GetAstroVolumeDisplayNode()
        voxels = voxelArray(sourceNode)
        sigma = fwhm / (2. * math.sqrt(2. * math.log(2.)))
        for j in range(size):
            for i in range(size):
                world = [0., 0., 0.]
                display.GetReferenceSpace([float(i), float(j), 0.], world)
                dx = (world[0] - 180.) * math.cos(math.radians(30.))
                dy = world[1] - 30.
                voxels[0, j, i] = math.exp(-0.5 * (dx * dx + dy * dy) / (sigma * sigma))
        sourceNode.GetImageData().Modified()
        sourceNode.UpdateRangeAttributes()

        # the active image: rotated TAN grid, pixels 1.5 times larger
        activePath = os.path.join(directory, "active.fits")
        writeFITS(activePath, np.ones((48, 48), dtype=np.float32), -32,
                  celestialWCS(projection="TAN", crpix=(24.5, 24.5), cdelt=(-0.003, 0.003)) + [("CROTA2", 35.)])
        activeNode = slicer.util.loadVolume(activePath)
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActiveVolumeID(activeNode.GetID())
        slicer.util.selectModule("AstroVolume")
        slicer.app.processEvents()
        moduleWidget = slicer.modules.astrovolume.widgetRepresentation()
        displayWidget = moduleWidget.findChild("qSlicerAstroScalarVolumeDisplayWidget")
        self.assertIsNotNone(displayWidget)
        slicer.util.findChild(displayWidget, "ContoursVolumeNodeSelector").setCurrentNode(sourceNode)
        slicer.util.findChild(displayWidget, "ContourLevelsLineEdit").text = "0.25; 0.5; 0.75"
        numberOfVolumes = slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLAstroVolumeNode")
        displayWidget.onCreateContours()
        slicer.app.processEvents()

        # the reprojected copy was temporary
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLAstroVolumeNode"), numberOfVolumes)
        contoursNode = self.logic.GetContoursSegmentationForVolume(activeNode, False)
        self.assertIsNotNone(contoursNode)
        segmentation = contoursNode.GetSegmentation()
        self.assertEqual(segmentation.GetNumberOfSegments(), 3)
        pixelArea = 0.003 * 0.003
        for index, level in enumerate((0.25, 0.5, 0.75)):
            segmentID = segmentation.GetNthSegmentID(index)
            mask = slicer.util.arrayFromSegmentBinaryLabelmap(contoursNode, segmentID, activeNode)
            # area of the source above the level: pi r^2, exp(-r^2 / 2 sigma^2) = level
            expected = math.pi * 2. * sigma * sigma * math.log(1. / level) / pixelArea
            self.assertAlmostEqual(float(np.count_nonzero(mask)) / expected, 1., delta=0.2,
                                   msg=f"level {level}")
        self.delayDisplay("Test passed")

    def test_ContoursOfOtherCube(self):
        """A cube with the contours of another cube stays in place (only a 2D
        image moves with the channel of the contours of a cube)."""
        import numpy as np
        import vtk
        from AstroFITSIOTest import celestialWCS, radioVelocityWCS, writeFITS

        directory = os.path.join(slicer.app.temporaryPath, "AstroVolumeSegmentationsTest")
        os.makedirs(directory, exist_ok=True)
        nodes = []
        for name in ("cubeA", "cubeB"):
            path = os.path.join(directory, name + ".fits")
            writeFITS(path, np.ones((12, 20, 24), dtype=np.float32), -32,
                      celestialWCS(crpix=(12.5, 10.5)) + radioVelocityWCS())
            nodes.append(slicer.util.loadVolume(path))
        activeNode, sourceNode = nodes
        before = vtk.vtkMatrix4x4()
        activeNode.GetIJKToRASMatrix(before)
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(activeNode.GetID())
        slicer.util.selectModule("AstroVolume")
        slicer.app.processEvents()
        displayWidget = slicer.modules.astrovolume.widgetRepresentation().findChild(
            "qSlicerAstroScalarVolumeDisplayWidget")
        slicer.util.findChild(displayWidget, "ContoursVolumeNodeSelector").setCurrentNode(sourceNode)
        slicer.app.processEvents()
        after = vtk.vtkMatrix4x4()
        activeNode.GetIJKToRASMatrix(after)
        for row in range(4):
            for column in range(4):
                self.assertEqual(after.GetElement(row, column), before.GetElement(row, column))
        self.assertFalse(slicer.util.findChild(displayWidget, "Contours2DSliderWidget").isVisibleTo(displayWidget))
        self.delayDisplay("Test passed")

    def test_CopyMoveMask(self):
        """Segments of a mask copied or moved to the masks of other volumes:
        voxel by voxel on the same lattice (a copy, a cutout), projected on a
        2D image of the same sky grid, reprojected (WCS, nearest neighbour)
        on another grid; masks on another grid are refused as module inputs."""
        import numpy as np
        import vtk
        from AstroFITSIOTest import celestialWCS, radioVelocityWCS, writeFITS
        from AstroVolumeLogicTest import doublePrecisionCopy, maskSegmentation, segmentMask

        directory = os.path.join(slicer.app.temporaryPath, "AstroVolumeSegmentationsTest")
        os.makedirs(directory, exist_ok=True)
        shape = (12, 20, 24)

        def load(name, data, cards):
            path = os.path.join(directory, name + ".fits")
            writeFITS(path, data, -32, cards + [("BUNIT", "JY/BEAM")])
            return slicer.util.loadVolume(path)

        cube = load("copy_cube", np.ones(shape, dtype=np.float32),
                    celestialWCS(crpix=(12.5, 10.5)) + radioVelocityWCS(crpix=1.0))
        z, y, x = np.indices(shape)
        source = ((x - 11.) ** 2 / 25. + (y - 9.) ** 2 / 16. + (z - 6.) ** 2 / 9.) <= 1.
        corner = np.zeros(shape, dtype=bool)
        corner[1:4, 2:5, 3:7] = True
        maskNode = self.logic.GetMaskSegmentationForVolume(cube, True)
        for name, mask in (("Source", source), ("Corner", corner)):
            segmentID = maskNode.GetSegmentation().AddEmptySegment(name, name)
            slicer.util.updateSegmentBinaryLabelmapFromArray(mask.astype(np.uint8), maskNode, segmentID, cube)

        def copy(target, ids, move=False):
            array = vtk.vtkStringArray()
            for segmentID in ids:
                array.InsertNextValue(segmentID)
            targetMask = self.logic.CopyMaskSegmentsToVolume(maskNode, array, target, move)
            self.assertIsNotNone(targetMask, self.logic.GetLastMaskCopyError())
            self.assertEqual(targetMask, self.logic.GetMaskSegmentationForVolume(target, False))
            return targetMask

        # a copy of the cube (same grid): voxel by voxel
        smoothed = doublePrecisionCopy(cube, "copy_cube_smoothed")
        targetMask = copy(smoothed, ["Source"])
        np.testing.assert_array_equal(segmentMask(targetMask, "Source", smoothed), source.astype(np.uint8))
        self.assertEqual(targetMask.GetSegmentation().GetSegment("Source").GetName(), "Source")

        # a cutout of the cube (voxels [2:10, 3:15, 4:20]): offset
        cutout = load("copy_cutout", np.ones((8, 12, 16), dtype=np.float32),
                      celestialWCS(crpix=(12.5 - 4, 10.5 - 3)) + radioVelocityWCS(crpix=1.0 - 2))
        self.assertEqual(slicer.vtkSlicerAstroVolumeLogic.GetGridOffset(cube, cutout, [0, 0, 0]), True)
        targetMask = copy(cutout, ["Source", "Corner"])
        np.testing.assert_array_equal(segmentMask(targetMask, "Source", cutout),
                                      source[2:10, 3:15, 4:20].astype(np.uint8))
        np.testing.assert_array_equal(segmentMask(targetMask, "Corner", cutout),
                                      corner[2:10, 3:15, 4:20].astype(np.uint8))
        # the mask of the cube used directly on the cutout (module input)
        np.testing.assert_array_equal(segmentMask(maskNode, "Source", cutout), source[2:10, 3:15, 4:20].astype(np.uint8))

        # a 2D image of the same sky grid (e.g. a moment map): projection
        image = load("copy_image", np.ones(shape[1:], dtype=np.float32), celestialWCS(crpix=(12.5, 10.5)))
        targetMask = copy(image, ["Source"])
        np.testing.assert_array_equal(segmentMask(targetMask, "Source", image)[0],
                                      source.any(axis=0).astype(np.uint8))

        # another grid (pixels twice larger, same channels): reprojected
        rebinned = load("copy_rebinned", np.ones((12, 10, 12), dtype=np.float32),
                        # shifted by 0.1 rebinned pixel: no ties of the nearest neighbour
                        celestialWCS(crpix=(6.6, 5.6), cdelt=(-0.002, 0.002)) + radioVelocityWCS(crpix=1.0))
        self.assertFalse(slicer.vtkSlicerAstroVolumeLogic.GetGridOffset(cube, rebinned, [0, 0, 0]))
        # refused as module input
        self.assertIsNone(segmentMask(maskNode, "Source", rebinned))
        targetMask = copy(rebinned, ["Source"])
        computed = segmentMask(targetMask, "Source", rebinned)
        # nearest voxel of the cube for each voxel of the rebinned cube (WCS)
        expected = np.zeros(computed.shape, dtype=np.uint8)
        targetDisplay = rebinned.GetAstroVolumeDisplayNode()
        sourceDisplay = cube.GetAstroVolumeDisplayNode()
        for k in range(computed.shape[0]):
            for j in range(computed.shape[1]):
                for i in range(computed.shape[2]):
                    world = [0., 0., 0.]
                    targetDisplay.GetReferenceSpace([float(i), float(j), float(k)], world)
                    ijk = [0., 0., 0.]
                    sourceDisplay.GetIJKSpace(world, ijk)
                    si, sj, sk = (int(round(v)) for v in ijk)
                    if 0 <= sk < shape[0] and 0 <= sj < shape[1] and 0 <= si < shape[2]:
                        expected[k, j, i] = source[sk, sj, si]
        self.assertGreater(np.count_nonzero(expected), 0)
        np.testing.assert_array_equal(computed, expected)

        # move (Copy and Move buttons of the AstroVolume module): the segments
        # leave the mask
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(cube.GetID())
        slicer.util.selectModule("AstroVolume")
        slicer.app.processEvents()
        widget = slicer.modules.astrovolume.widgetRepresentation()
        slicer.util.findChild(widget, "SegmentationNodeComboBox").setCurrentNode(maskNode)
        slicer.app.processEvents()
        slicer.util.findChild(widget, "SegmentsTableView").setSelectedSegmentIDs(["Corner"])
        slicer.util.findChild(widget, "CopyMaskTargetSelector").setCurrentNode(smoothed)
        slicer.util.findChild(widget, "MoveMaskPushButton").click()
        slicer.app.processEvents()
        self.assertIsNone(maskNode.GetSegmentation().GetSegment("Corner"))
        smoothedMask = self.logic.GetMaskSegmentationForVolume(smoothed, False)
        np.testing.assert_array_equal(segmentMask(smoothedMask, "Corner", smoothed), corner.astype(np.uint8))
        # nothing selected: all the segments are copied
        slicer.util.findChild(widget, "SegmentsTableView").setSelectedSegmentIDs([])
        slicer.util.findChild(widget, "CopyMaskTargetSelector").setCurrentNode(image)
        slicer.util.findChild(widget, "CopyMaskPushButton").click()
        imageMask = self.logic.GetMaskSegmentationForVolume(image, False)
        self.assertEqual(imageMask.GetSegmentation().GetNumberOfSegments(), 2)
        self.assertIsNotNone(maskNode.GetSegmentation().GetSegment("Source"))
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLLabelMapVolumeNode"), 0)
        self.delayDisplay("Test passed")

    def test_MasksSection(self):
        """Masks section (reusable widget of the AstroVolume panel and the
        Workbench): the mask of the volume and its segments, copy and move of
        the selected segments, Segment Editor in the section, save as FITS
        mask (vtkSlicerAstroVolumeLogic.SaveFITSMask: round trip, a
        segmentation without FITS header takes the one of the data; contours
        and empty masks are refused)."""
        import numpy as np
        from AstroFITSIOTest import celestialWCS, radioVelocityWCS, writeFITS
        from AstroVolumeLogicTest import segmentMask

        directory = os.path.join(slicer.app.temporaryPath, "AstroVolumeSegmentationsTest")
        os.makedirs(directory, exist_ok=True)
        shape = (8, 16, 20)

        def load(name):
            path = os.path.join(directory, name + ".fits")
            writeFITS(path, np.ones(shape, dtype=np.float32), -32,
                      celestialWCS(crpix=(10.5, 8.5)) + radioVelocityWCS(crpix=1.0) + [("BUNIT", "JY/BEAM")])
            return slicer.util.loadVolume(path)

        cube = load("section_cube")
        other = load("section_other")
        maskNode = self.logic.GetMaskSegmentationForVolume(cube, True)
        masks = {}
        for name, box in (("Source", np.s_[2:6, 4:10, 5:12]), ("Corner", np.s_[0:2, 0:3, 0:4])):
            masks[name] = np.zeros(shape, dtype=np.uint8)
            masks[name][box] = 1
            segmentID = maskNode.GetSegmentation().AddEmptySegment(name, name)
            slicer.util.updateSegmentBinaryLabelmapFromArray(masks[name], maskNode, segmentID, cube)

        section = slicer.qSlicerAstroMasksSection()
        section.setVolumeNode(cube)
        slicer.app.processEvents()
        self.assertEqual(section.segmentCount(), 2)
        self.assertEqual(slicer.util.findChild(section, "MasksSelector").currentNode(), maskNode)
        self.assertTrue(slicer.util.findChild(section, "MasksSave").enabled)
        target = slicer.util.findChild(section, "MasksCopyTarget")
        self.assertEqual([target.itemText(i) for i in range(target.count)], [other.GetName()])

        # move the selected segment, then copy all (nothing selected)
        table = slicer.util.findChild(section, "MasksTable")
        table.setSelectedSegmentIDs(["Corner"])
        self.assertEqual(list(section.selectedSegmentIDs()), ["Corner"])
        self.assertTrue(section.copySegments(other.GetID(), True))
        self.assertEqual(section.status(), f"1 segment moved to {other.GetName()}")
        self.assertIsNone(maskNode.GetSegmentation().GetSegment("Corner"))
        otherMask = self.logic.GetMaskSegmentationForVolume(other, False)
        np.testing.assert_array_equal(segmentMask(otherMask, "Corner", other), masks["Corner"])
        table.setSelectedSegmentIDs([])
        self.assertTrue(section.copySegments(other.GetID(), False))
        np.testing.assert_array_equal(segmentMask(otherMask, "Source", other), masks["Source"])
        self.assertEqual(section.segmentCount(), 1)

        # the Segment Editor of the section edits the mask of the volume
        section.setEditing(True)
        editor = section.segmentEditor()
        self.assertIsNotNone(editor)
        self.assertEqual(editor.segmentationNode(), maskNode)
        self.assertEqual(editor.sourceVolumeNode(), cube)
        section.setEditing(False)
        self.assertTrue(editor.isHidden())

        # save as FITS mask and load it again
        path = os.path.join(directory, "section_mask.fits")
        if os.path.exists(path):
            os.remove(path)
        self.assertTrue(section.saveMask(path), section.status())
        self.assertTrue(os.path.exists(path))
        loaded = slicer.util.loadSegmentation(path)
        self.assertEqual(loaded.GetSegmentation().GetNumberOfSegments(), 1)
        np.testing.assert_array_equal(segmentMask(loaded, loaded.GetSegmentation().GetNthSegmentID(0), cube),
                                      masks["Source"])

        # a segmentation without FITS header takes the one of the data; the
        # FITS extension is added
        plain = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentationNode", "plain")
        plain.CreateDefaultDisplayNodes()
        segmentID = plain.GetSegmentation().AddEmptySegment("Box", "Box")
        slicer.util.updateSegmentBinaryLabelmapFromArray(masks["Source"], plain, segmentID, cube)
        plainPath = os.path.join(directory, "section_plain")
        self.assertTrue(self.logic.SaveFITSMask(plain, cube, plainPath), self.logic.GetLastMaskSaveError())
        self.assertTrue(os.path.exists(plainPath + ".fits"))
        self.assertTrue(plain.GetStorageNode().IsA("vtkMRMLAstroSegmentationStorageNode"))

        # refused: contours, empty masks, no header and no reference
        contours = self.logic.GetContoursSegmentationForVolume(cube, True)
        self.assertFalse(self.logic.SaveFITSMask(contours, cube, path))
        self.assertIn("Contours", self.logic.GetLastMaskSaveError())
        empty = self.logic.GetMaskSegmentationForVolume(other, True)
        empty.GetSegmentation().RemoveAllSegments()
        self.assertFalse(self.logic.SaveFITSMask(empty, other, path))
        self.assertIn("empty", self.logic.GetLastMaskSaveError())
        section.setVolumeNode(other)
        self.assertFalse(slicer.util.findChild(section, "MasksSave").enabled)
        self.assertFalse(slicer.util.findChild(section, "MasksEmpty").isHidden())
        self.assertTrue(slicer.util.findChild(section, "MasksTable").isHidden())
        section.setVolumeNode(None)
        self.assertFalse(section.enabled)
        self.delayDisplay("Test passed")

    def test_ContoursChannelMap(self):
        """Contours of a cube over a 2D image (channel map): the channel slider
        moves the image to the channel and the XY views to the image, without
        changing their zoom and panning or the other views."""
        import numpy as np
        from AstroFITSIOTest import celestialWCS, radioVelocityWCS, writeFITS

        self.delayDisplay("Channel map of contours")
        directory = os.path.join(slicer.app.temporaryPath, "AstroVolumeSegmentationsTest")
        os.makedirs(directory, exist_ok=True)
        cubePath = os.path.join(directory, "cube.fits")
        cube = np.zeros((20, 32, 32), dtype=np.float32)
        cube[:, 12:20, 12:20] = 1.
        writeFITS(cubePath, cube, -32, celestialWCS(crpix=(16.5, 16.5), cdelt=(-0.002, 0.002)) + radioVelocityWCS())
        imagePath = os.path.join(directory, "image.fits")
        writeFITS(imagePath, np.ones((32, 32), dtype=np.float32), -32,
                  celestialWCS(crpix=(16.5, 16.5), cdelt=(-0.002, 0.002)))
        cubeNode = slicer.util.loadVolume(cubePath)
        imageNode = slicer.util.loadVolume(imagePath)
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActiveVolumeID(imageNode.GetID())
        slicer.app.applicationLogic().PropagateVolumeSelection()
        slicer.util.selectModule("AstroVolume")
        slicer.app.processEvents()
        displayWidget = slicer.modules.astrovolume.widgetRepresentation().findChild("qSlicerAstroScalarVolumeDisplayWidget")
        slicer.util.findChild(displayWidget, "ContoursVolumeNodeSelector").setCurrentNode(cubeNode)
        slicer.util.findChild(displayWidget, "ContourLevelsLineEdit").text = "0.5"
        displayWidget.onCreateContours()
        slicer.app.processEvents()

        # the user zooms and pans the views
        layoutManager = slicer.app.layoutManager()
        sliceNodes = {name: layoutManager.sliceWidget(name).mrmlSliceNode() for name in ("Red", "Green", "Yellow")}
        for sliceNode in sliceNodes.values():
            fieldOfView = sliceNode.GetFieldOfView()
            sliceNode.SetFieldOfView(0.5 * fieldOfView[0], 0.5 * fieldOfView[1], fieldOfView[2])
            sliceNode.SetXYZOrigin(4., 2., 0.)
        slicer.app.processEvents()

        def state(sliceNode):
            matrix = sliceNode.GetSliceToRAS()
            return (sliceNode.GetOrientation(), tuple(round(v, 6) for v in sliceNode.GetFieldOfView()),
                    tuple(round(v, 6) for v in sliceNode.GetXYZOrigin()),
                    tuple(round(matrix.GetElement(i, 3), 6) for i in range(3)))
        before = {name: state(sliceNode) for name, sliceNode in sliceNodes.items()}
        slider = slicer.util.findChild(displayWidget, "Contours2DSliderWidget")
        slider.value = slider.value + 5
        slicer.app.processEvents()

        for name, sliceNode in sliceNodes.items():
            after = state(sliceNode)
            if after[0] == "XY":
                # same zoom and panning, moved to the plane of the image
                self.assertEqual(after[1:3], before[name][1:3], name)
                ras = [0., 0., 0., 1.]
                ijkToRAS = vtk.vtkMatrix4x4()
                imageNode.GetIJKToRASMatrix(ijkToRAS)
                ijkToRAS.MultiplyPoint([16., 16., 0., 1.], ras)
                rasToXY = vtk.vtkMatrix4x4()
                vtk.vtkMatrix4x4.Invert(sliceNode.GetXYToRAS(), rasToXY)
                self.assertAlmostEqual(rasToXY.MultiplyPoint(ras)[2], 0., places=3, msg=f"{name}: image on the slice")
            else:
                self.assertEqual(after, before[name], f"{name}: unchanged")
        self.delayDisplay("Test passed")
