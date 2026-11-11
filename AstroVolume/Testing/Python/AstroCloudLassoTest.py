import math

import numpy as np
import vtk

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroCloudLassoTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Cloud Lasso Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Test of the Cloud Lasso segment editor effect: lassos drawn (simulated mouse
events) in a slice view and in the 3D view fill the voxels in the threshold
range, the automatic threshold updating applies the lasso again, erasing
removes the voxels inside the lasso.
"""
        self.parent.hidden = True


class AstroCloudLassoTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroCloudLassoTestLogic(ScriptedLoadableModuleLogic):
    pass


class AstroCloudLassoTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_CloudLasso()

    def drawLasso(self, view, center, radius, steps=24):
        """Left-drag along a circle in a view (display coordinates)."""
        interactor = view.interactor()
        points = [(int(center[0] + radius * math.cos(2 * math.pi * i / steps)),
                   int(center[1] + radius * math.sin(2 * math.pi * i / steps))) for i in range(steps + 1)]
        interactor.SetEventInformation(points[0][0], points[0][1], 0, 0, chr(0), 0, None)
        interactor.MouseMoveEvent()
        interactor.LeftButtonPressEvent()
        for x, y in points[1:]:
            interactor.SetEventInformation(x, y, 0, 0, chr(0), 0, None)
            interactor.MouseMoveEvent()
        interactor.LeftButtonReleaseEvent()
        slicer.app.processEvents()

    def segmentVoxels(self, segmentationNode, segmentID, volumeNode):
        return slicer.util.arrayFromSegmentBinaryLabelmap(segmentationNode, segmentID, volumeNode)

    def volumeVoxels(self, volumeNode):
        from vtk.util.numpy_support import vtk_to_numpy

        dims = volumeNode.GetImageData().GetDimensions()
        return vtk_to_numpy(volumeNode.GetImageData().GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])

    def test_CloudLasso(self):
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)
        slicer.app.processEvents()
        data = self.volumeVoxels(volumeNode)

        segmentationNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentationNode")
        segmentationNode.CreateDefaultDisplayNodes()
        segmentationNode.SetReferenceImageGeometryParameterFromVolumeNode(volumeNode)
        segmentID = segmentationNode.GetSegmentation().AddEmptySegment("", "Galaxy")

        editorWidget = slicer.qMRMLSegmentEditorWidget()
        editorWidget.setMRMLScene(slicer.mrmlScene)
        editorNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentEditorNode")
        editorWidget.setMRMLSegmentEditorNode(editorNode)
        editorWidget.setSegmentationNode(segmentationNode)
        editorWidget.setSourceVolumeNode(volumeNode)
        editorWidget.setCurrentSegmentID(segmentID)
        editorWidget.setupViewObservations()
        editorWidget.setActiveEffectByName("AstroCloudLasso")
        effect = editorWidget.activeEffect()
        self.assertIsNotNone(effect)
        self.assertEqual(effect.parameter("Operation"), "FillInside")

        # threshold range: from 3 times the noise to the maximum
        noise3 = 3.0 * float(volumeNode.GetAttribute("SlicerAstro.DisplayThreshold"))
        self.assertAlmostEqual(effect.doubleParameter("ThresholdMinimum"), noise3, places=6)
        self.assertTrue(editorNode.GetSourceVolumeIntensityMask())

        # Lasso in the red slice view: only voxels in the threshold range
        redView = layoutManager.sliceWidget("Red").sliceView()
        self.drawLasso(redView, (redView.width // 2, redView.height // 2), min(redView.width, redView.height) // 3)
        voxels = self.segmentVoxels(segmentationNode, segmentID, volumeNode)
        selected = voxels != 0
        self.assertGreater(np.count_nonzero(selected), 0)
        self.assertGreaterEqual(float(np.nanmin(data[selected])), noise3 - 1e-6)
        countSlice = np.count_nonzero(selected)

        # Automatic threshold updating: the last lasso with a higher minimum
        effect.setParameter("AutomaticThresholdUpdating", "1")
        higherMinimum = noise3 * 2.0
        effect.onThresholdValuesChanged(higherMinimum, effect.doubleParameter("ThresholdMaximum"))
        voxels = self.segmentVoxels(segmentationNode, segmentID, volumeNode)
        selected = voxels != 0
        self.assertGreater(np.count_nonzero(selected), 0)
        self.assertLess(np.count_nonzero(selected), countSlice)
        self.assertGreaterEqual(float(np.nanmin(data[selected])), higherMinimum - 1e-6)
        effect.setParameter("AutomaticThresholdUpdating", "0")

        # Lasso in the 3D view (through the whole cube)
        threeDView = layoutManager.threeDWidget(0).threeDView()
        layoutManager.threeDWidget(0).threeDController().resetFocalPoint()
        slicer.app.processEvents()
        before = np.count_nonzero(self.segmentVoxels(segmentationNode, segmentID, volumeNode))
        self.drawLasso(threeDView, (threeDView.width // 2, threeDView.height // 2),
                       min(threeDView.width, threeDView.height) // 3)
        after = np.count_nonzero(self.segmentVoxels(segmentationNode, segmentID, volumeNode))
        self.assertGreater(after, before)

        # Erase: all the voxels inside the lasso
        effect.setParameter("Operation", "EraseInside")
        self.drawLasso(redView, (redView.width // 2, redView.height // 2), min(redView.width, redView.height) // 3)
        erased = np.count_nonzero(self.segmentVoxels(segmentationNode, segmentID, volumeNode))
        self.assertLess(erased, after)
        # the intensity mask is restored after erasing
        self.assertTrue(editorNode.GetSourceVolumeIntensityMask())

        print(f"Cloud Lasso voxels: slice lasso {countSlice}, higher threshold {np.count_nonzero(selected)}, "
              f"3D lasso {before} -> {after}, after erasing {erased}")
        editorWidget.setActiveEffect(None)
        self.assertFalse(editorNode.GetSourceVolumeIntensityMask())
        self.delayDisplay("Test passed")
