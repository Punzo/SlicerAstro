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
smooth and delete buttons).
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
        self.test_Contours()

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
        self.assertEqual(maskNode.GetName(), volumeNode.GetName() + "_SEG")
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
