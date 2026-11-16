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

from AstroSegmentationMaskTest import SOURCES, WCS_CARDS, syntheticMask, writeFITS


class AstroTerminologyTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Terminology Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Astronomical terminology of the segments: the terminology context and its
color table, the default entries of the astronomical segmentations (masks,
contours), the sources of FITS masks and the terminology saved in FITS masks.
"""
        self.parent.hidden = True


class AstroTerminologyTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroTerminologyTestLogic(ScriptedLoadableModuleLogic):
    pass


CONTEXT = "Segmentation category and type - SlicerAstro"
SCHEME = "99SLICERASTRO"


def entry(categoryCode, category, typeCode, typeName):
    """Serialized terminology entry (vtkSlicerTerminologiesModuleLogic format),
    as vtkMRMLAstroTerminology builds them."""
    return (f"{CONTEXT}~{SCHEME}^{categoryCode}^{category}~{SCHEME}^{typeCode}^{typeName}"
            "~^^~~^^~^^")


# the entries SlicerAstro assigns itself (vtkMRMLAstroTerminology.h)
SOURCE = entry("SA-C1", "Source detection", "SA-T101", "Source")
MASK = entry("SA-C5", "Analysis region", "SA-T501", "Mask")
CONTOUR = entry("SA-C5", "Analysis region", "SA-T505", "Contour")
MODEL = entry("SA-C5", "Analysis region", "SA-T506", "Model")
GAS_CLOUD = entry("SA-C3", "Gas structure", "SA-T306", "Gas cloud")


class AstroTerminologyTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroTerminologyTest")
        os.makedirs(self.directory, exist_ok=True)
        self.terminologies = slicer.modules.terminologies.logic()

    def runTest(self):
        for test in (self.test_ContextAndColorTable, self.test_FITSMaskSources,
                     self.test_NewSegmentsOfAstroSegmentations):
            self.setUp()
            test()

    #
    # helpers
    #

    def colorTable(self):
        tables = [node for node in slicer.util.getNodesByClass("vtkMRMLColorTableNode")
                  if node.GetSingletonTag() == "SlicerAstro.Terminology"]
        self.assertEqual(len(tables), 1)
        return tables[0]

    def typeName(self, terminologyEntry):
        """Type of an entry of the astronomical terminology (None if it is not in it)."""
        table = self.colorTable()
        index = slicer.vtkSlicerTerminologiesModuleLogic.GetColorIndexByTerminology(table, terminologyEntry)
        return table.GetColorName(index) if index > 0 else None

    def loadWEIN069(self):
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        return volumeNode

    #
    # tests
    #

    def test_ContextAndColorTable(self):
        self.assertTrue(self.terminologies.IsTerminologyContextLoaded(CONTEXT))
        table = self.colorTable()
        self.assertEqual(table.GetName(), "SlicerAstro")
        self.assertTrue(table.GetContainsTerminology())
        self.assertIn(table.GetID(), self.terminologies.GetCompatibleColorNodeIDs())
        # background + the 28 types of the terminology, each with its terminology
        self.assertEqual(table.GetNumberOfColors(), 29)
        for index in range(1, table.GetNumberOfColors()):
            self.assertIn(SCHEME, table.GetTerminologyAsString(index), index)
        # the entries used by SlicerAstro are in the terminology
        self.assertEqual(self.typeName(SOURCE), "Source")
        self.assertEqual(self.typeName(MASK), "Mask")
        self.assertEqual(self.typeName(CONTOUR), "Contour")
        self.assertEqual(self.typeName(MODEL), "Model")
        # recommended colors of the terminology
        color = [0.0] * 4
        table.GetColor(slicer.vtkSlicerTerminologiesModuleLogic.GetColorIndexByTerminology(table, SOURCE), color)
        np.testing.assert_allclose(color[:3], np.array([255, 140, 66]) / 255.0, atol=1e-6)

        # contours: the default color of the contours of the AstroVolume module
        table.GetColor(slicer.vtkSlicerTerminologiesModuleLogic.GetColorIndexByTerminology(table, CONTOUR), color)
        defaultContours = slicer.vtkMRMLAstroVolumeDisplayNode().GetContoursColor()
        np.testing.assert_allclose(color[:3], [defaultContours.GetValue(i) for i in range(3)], atol=3e-3)

        # the closing of a scene empties the singleton color tables: filled again
        slicer.mrmlScene.Clear(0)
        self.assertEqual(self.colorTable().GetNumberOfColors(), 29)
        self.assertEqual(self.typeName(MASK), "Mask")

    def test_FITSMaskSources(self):
        maskPath = os.path.join(self.directory, "survey_mask.fits")
        writeFITS(maskPath, syntheticMask(), WCS_CARDS)
        segmentationNode = slicer.util.loadSegmentation(maskPath)
        self.assertIsNotNone(segmentationNode)
        segmentation = segmentationNode.GetSegmentation()
        for sourceID in SOURCES:
            segment = segmentation.GetSegment("Source_%d" % sourceID)
            self.assertEqual(self.typeName(segment.GetTerminology()), "Source", sourceID)

        # a type chosen by the user is saved in the FITS mask (SEGTRMn)
        segmentation.GetSegment("Source_2").SetTerminology(GAS_CLOUD)
        savedPath = os.path.join(self.directory, "survey_mask_saved.fits")
        self.assertTrue(slicer.util.saveNode(segmentationNode, savedPath))
        slicer.mrmlScene.RemoveNode(segmentationNode)
        reloaded = slicer.util.loadSegmentation(savedPath).GetSegmentation()
        self.assertEqual(self.typeName(reloaded.GetSegment("Source_2").GetTerminology()), "Gas cloud")
        self.assertEqual(self.typeName(reloaded.GetSegment("Source_1").GetTerminology()), "Source")

    def test_NewSegmentsOfAstroSegmentations(self):
        volumeNode = self.loadWEIN069()
        logic = slicer.modules.astrovolume.logic()
        maskNode = volumeNode.GetNodeReference(logic.GetMaskReferenceRole())
        contoursNode = volumeNode.GetNodeReference(logic.GetContoursReferenceRole())
        self.assertIsNotNone(maskNode)
        self.assertIsNotNone(contoursNode)
        defaultEntry = slicer.vtkSlicerTerminologiesModuleLogic.GetDefaultTerminologyEntryAsString
        self.assertEqual(self.typeName(defaultEntry(maskNode)), "Mask")
        self.assertEqual(self.typeName(defaultEntry(contoursNode)), "Contour")

        # a segment added in the Segment Editor gets the astronomical entry
        editorWidget = slicer.qMRMLSegmentEditorWidget()
        editorWidget.setMRMLScene(slicer.mrmlScene)
        editorNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentEditorNode")
        editorWidget.setMRMLSegmentEditorNode(editorNode)
        editorWidget.setSegmentationNode(maskNode)
        editorWidget.setSourceVolumeNode(volumeNode)
        slicer.util.findChild(editorWidget, "AddSegmentButton").click()
        segmentation = maskNode.GetSegmentation()
        self.assertEqual(segmentation.GetNumberOfSegments(), 1)
        self.assertEqual(self.typeName(segmentation.GetNthSegment(0).GetTerminology()), "Mask")
        editorWidget.setSegmentationNode(None)
        editorWidget.setMRMLSegmentEditorNode(None)

        # the color button of the contours chooses a type of the terminology
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        colorButton = slicer.util.findChild(slicer.modules.astrovolume.widgetRepresentation(), "ColorPickerButton")
        self.assertEqual(colorButton.className(), "qSlicerTerminologySelectorButton")

        # an entry chosen by the user is kept
        slicer.vtkSlicerTerminologiesModuleLogic.SetDefaultTerminologyEntryAsString(maskNode, GAS_CLOUD)
        maskNode.Modified()
        logic.EnsureAstroTerminology(maskNode)
        self.assertEqual(self.typeName(defaultEntry(maskNode)), "Gas cloud")

        # the segmentation of a non astronomical image (e.g. medical): Slicer's default
        imageData = vtk.vtkImageData()
        imageData.SetDimensions(8, 8, 8)
        imageData.AllocateScalars(vtk.VTK_SHORT, 1)
        scalarVolume = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLScalarVolumeNode")
        scalarVolume.SetAndObserveImageData(imageData)
        medicalNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentationNode")
        medicalNode.SetReferenceImageGeometryParameterFromVolumeNode(scalarVolume)
        self.assertFalse(logic.EnsureAstroTerminology(medicalNode))
        self.assertEqual(defaultEntry(medicalNode), "")
