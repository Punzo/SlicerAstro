import vtk

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroVolumeRenderingTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Volume Rendering Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Test of the volume rendering of astronomical data: the transfer functions
keep their points far enough apart for the lookup table textures, the
OpenGL volume mapper does not warn about the texture size, and the ROI of
the 3-D display (Display ROI, crop, RMS in ROI) works.
"""
        self.parent.hidden = True


class AstroVolumeRenderingTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroVolumeRenderingTestLogic(ScriptedLoadableModuleLogic):
    pass


def opacityPositions(volumeProperty):
    function = volumeProperty.GetScalarOpacity()
    positions = []
    for index in range(function.GetSize()):
        node = [0.0] * 4
        function.GetNodeValue(index, node)
        positions.append(node[0])
    return positions


def colorPositions(volumeProperty):
    function = volumeProperty.GetRGBTransferFunction()
    positions = []
    for index in range(function.GetSize()):
        node = [0.0] * 6
        function.GetNodeValue(index, node)
        positions.append(node[0])
    return positions


def minimumGap(positions):
    return min(b - a for a, b in zip(positions, positions[1:]))


class AstroVolumeRenderingTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_EnsureMinimumTransferFunctionGap()
        self.setUp()
        self.test_NoTextureSizeWarning()
        self.setUp()
        self.test_DisplayROIAndRMS()
        self.setUp()
        self.test_DefaultPreset()
        self.setUp()
        self.test_SceneRestoresColorFunctions()

    def test_EnsureMinimumTransferFunctionGap(self):
        # Points 1e-9 apart, as created by Slicer for a threshold equal to the
        # scalar range, on an astronomical intensity range of 0.01
        scalarRange = [-0.002, 0.008]
        volumeProperty = vtk.vtkVolumeProperty()
        # (the default functions of vtkVolumeProperty have points at 0 and 1024)
        opacity = vtk.vtkPiecewiseFunction()
        volumeProperty.SetScalarOpacity(0, opacity)
        for x, y in ((-0.002, 0.0), (-0.002 + 1e-9, 0.0), (0.008 - 2e-9, 1.0), (0.008 - 1e-9, 1.0), (0.008, 0.0)):
            opacity.AddPoint(x, y)
        color = vtk.vtkColorTransferFunction()
        for x in (-0.002, -0.002 + 1e-9, 0.008):
            color.AddRGBPoint(x, 0.0, 1.0, 0.0)
        volumeProperty.SetColor(0, color)

        logic = slicer.vtkSlicerAstroVolumeLogic
        self.assertTrue(logic.EnsureMinimumTransferFunctionGap(volumeProperty, scalarRange))
        gap = (scalarRange[1] - scalarRange[0]) * 1e-4
        for positions in (opacityPositions(volumeProperty), colorPositions(volumeProperty)):
            self.assertGreaterEqual(minimumGap(positions), gap * 0.999)
            # The ends of the range are kept
            self.assertAlmostEqual(positions[0], scalarRange[0], places=12)
            self.assertAlmostEqual(positions[-1], scalarRange[1], places=12)
        self.assertEqual(volumeProperty.GetScalarOpacity().GetSize(), 5)

        # Functions with wide enough gaps are not modified
        self.assertFalse(logic.EnsureMinimumTransferFunctionGap(volumeProperty, scalarRange))

    def test_NoTextureSizeWarning(self):
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        slicer.app.layoutManager().setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)
        slicer.app.processEvents()

        displayNode = volumeNode.GetAstroVolumeRenderingDisplayNode()
        self.assertIsNotNone(displayNode)
        displayNode.SetVisibility(True)

        slicer.util.forceRenderAllViews()
        slicer.app.processEvents()

        # The volume rendering lookup tables warn in the application log
        model = slicer.app.errorLogModel()
        DescriptionTextRole = 257
        texts = []
        for row in range(model.rowCount()):
            index = model.index(row, model.columnCount() - 1)
            texts.append(str(model.data(index, DescriptionTextRole) or model.data(index)))
        self.assertFalse([t for t in texts if "texture size" in t])
        scalarRange = volumeNode.GetImageData().GetPointData().GetScalars().GetFiniteRange()
        gap = (scalarRange[1] - scalarRange[0]) * 1e-4
        volumeProperty = displayNode.GetVolumePropertyNode().GetVolumeProperty()
        self.assertGreaterEqual(minimumGap(opacityPositions(volumeProperty)), gap * 0.999)
        self.delayDisplay("Test passed")

    def roiExtentIJK(self, roiNode, volumeNode):
        """Voxel extent (inclusive) of the voxel centers inside the ROI."""
        import numpy as np

        rasToIJK = vtk.vtkMatrix4x4()
        volumeNode.GetRASToIJKMatrix(rasToIJK)
        objectToWorld = roiNode.GetObjectToWorldMatrix()
        radius = [0.0] * 3
        roiNode.GetRadiusXYZ(radius)
        corners = []
        for corner in range(8):
            point = [radius[0] if corner & 1 else -radius[0],
                     radius[1] if corner & 2 else -radius[1],
                     radius[2] if corner & 4 else -radius[2], 1.0]
            world = objectToWorld.MultiplyPoint(point)
            corners.append(rasToIJK.MultiplyPoint(world)[:3])
        corners = np.array(corners)
        dims = volumeNode.GetImageData().GetDimensions()
        lower = np.maximum(np.ceil(corners.min(axis=0) + 0.5 - 0.001), 0).astype(int)
        upper = np.minimum(np.floor(corners.max(axis=0) - 0.5 + 0.001), np.array(dims) - 1).astype(int)
        return lower, upper

    def expectedRMS(self, roiNode, volumeNode):
        import numpy as np

        lower, upper = self.roiExtentIJK(roiNode, volumeNode)
        from vtk.util.numpy_support import vtk_to_numpy

        imageData = volumeNode.GetImageData()
        dims = imageData.GetDimensions()
        array = vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])
        region = array[lower[2]:upper[2] + 1, lower[1]:upper[1] + 1, lower[0]:upper[0] + 1]
        values = region[np.isfinite(region)].astype(np.float64)
        return float(np.sqrt(np.mean((values - values.mean()) ** 2)))

    def test_DisplayROIAndRMS(self):
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        slicer.app.processEvents()
        widget = slicer.modules.astrovolume.widgetRepresentation()
        displayNode = volumeNode.GetAstroVolumeRenderingDisplayNode()
        self.assertIsNotNone(displayNode)

        # Display ROI creates the cropping ROI, fitted to the volume, and shows it
        self.assertIsNone(displayNode.GetMarkupsROINode())
        displayROICheckBox = slicer.util.findChild(widget, "ROICropDisplayCheckBox")
        displayROICheckBox.checked = True
        slicer.app.processEvents()
        roiNode = displayNode.GetMarkupsROINode()
        self.assertIsNotNone(roiNode)
        self.assertTrue(roiNode.IsA("vtkMRMLMarkupsROINode"))
        self.assertTrue(roiNode.GetDisplayVisibility())
        self.assertTrue(displayNode.GetCroppingEnabled())
        lower, upper = self.roiExtentIJK(roiNode, volumeNode)
        self.assertEqual(list(lower), [0, 0, 0])
        self.assertEqual(list(upper), [d - 1 for d in volumeNode.GetImageData().GetDimensions()])

        # Hiding the ROI keeps it
        displayROICheckBox.checked = False
        slicer.app.processEvents()
        self.assertFalse(roiNode.GetDisplayVisibility())

        # RMS in ROI: whole cube, then a corner of the cube
        rmsButton = slicer.util.findChild(widget, "CalculateRMSPushButton")
        rmsButton.click()
        slicer.app.processEvents()
        self.assertAlmostEqual(volumeNode.GetDisplayThreshold(), self.expectedRMS(roiNode, volumeNode),
                               delta=1e-5 * self.expectedRMS(roiNode, volumeNode))  # stored with 6 digits

        center = [0.0] * 3
        roiNode.GetXYZ(center)
        radius = [0.0] * 3
        roiNode.GetRadiusXYZ(radius)
        roiNode.SetXYZ(center[0] - radius[0] / 2, center[1] - radius[1] / 2, center[2] + radius[2] / 2)
        roiNode.SetRadiusXYZ(radius[0] / 3, radius[1] / 3, radius[2] / 3)
        rmsButton.click()
        slicer.app.processEvents()
        expected = self.expectedRMS(roiNode, volumeNode)
        self.assertGreater(expected, 0.0)
        self.assertAlmostEqual(volumeNode.GetDisplayThreshold(), expected, delta=1e-5 * expected)
        self.delayDisplay("Test passed")

    def test_DefaultPreset(self):
        """The 3D color function of a loaded cube is ThreeSurfaces."""
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        slicer.app.processEvents()
        presetNode = volumeNode.GetPresetNode()
        self.assertIsNotNone(presetNode)
        self.assertEqual(presetNode.GetName(), "ThreeSurfaces")
        self.assertEqual(volumeNode.GetPresetIndex(), slicer.vtkMRMLAstroVolumeNode.ThreeSurfacesPreset)
        self.delayDisplay("Test passed")

    def displayState(self, volumeNode):
        displayNode = volumeNode.GetAstroVolumeDisplayNode()
        volumeProperty = volumeNode.GetVolumePropertyNode().GetVolumeProperty()
        return {
            "preset": volumeNode.GetPresetNode().GetName() if volumeNode.GetPresetNode() else None,
            "offset": round(displayNode.GetPresetOffset(), 9),
            "stretch": round(displayNode.GetPresetStretch(), 9),
            "opacityShift": round(displayNode.GetOpacityShift(), 9),
            "color": displayNode.GetColorNode().GetName(),
            "autoWindowLevel": displayNode.GetAutoWindowLevel(),
            "window": round(displayNode.GetWindow(), 9),
            "level": round(displayNode.GetLevel(), 9),
            "opacityPoints": [round(x, 9) for x in opacityPositions(volumeProperty)],
            "colorPoints": [round(x, 9) for x in colorPositions(volumeProperty)],
        }

    def test_SceneRestoresColorFunctions(self):
        """A saved scene restores the 2D (color table, window/level) and 3D
        (preset, offset, stretch, opacity shift) color functions of a cube,
        with the AstroVolume module open while the scene is loaded."""
        import os
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        slicer.app.processEvents()
        widget = slicer.modules.astrovolume.widgetRepresentation()
        slicer.util.findChild(widget, "PresetsNodeComboBox").setCurrentNodeIndex(
            slicer.vtkMRMLAstroVolumeNode.OneSurfaceWhitePreset)
        slicer.app.processEvents()
        for name, fraction in (("PresetOffsetSlider", 0.7), ("PresetStretchSlider", 0.6), ("OpacitySliderWidget", 0.3)):
            slider = slicer.util.findChild(widget, name)
            slider.value = slider.minimum + fraction * (slider.maximum - slider.minimum)
            slicer.app.processEvents()
        displayNode = volumeNode.GetAstroVolumeDisplayNode()
        # 2D color table chosen in the selector, as by the user
        import qt
        displayWidget = slicer.util.findChildren(widget, className="qSlicerAstroScalarVolumeDisplayWidget")[0]
        colorSelector = slicer.util.findChild(displayWidget, "ColorTableComboBox").findChild(qt.QComboBox)
        heatIndex = [colorSelector.itemText(i) for i in range(colorSelector.count)].index("Heat")
        colorSelector.setCurrentIndex(heatIndex)
        colorSelector.activated(heatIndex)
        slicer.app.processEvents()
        self.assertEqual(displayNode.GetColorNode().GetName(), "Heat")
        displayNode.SetAutoWindowLevel(0)
        displayNode.SetWindowLevel(0.01, 0.004)
        slicer.app.processEvents()
        saved = self.displayState(volumeNode)
        self.assertEqual(saved["preset"], "OneSurfaceWhite")
        self.assertNotEqual(saved["offset"], 0.)
        self.assertNotEqual(saved["opacityShift"], 0.)

        sceneFile = os.path.join(slicer.app.temporaryPath, "AstroVolumeRenderingTestScene.mrb")
        if os.path.exists(sceneFile):
            os.remove(sceneFile)
        self.assertTrue(slicer.util.saveScene(sceneFile))
        try:
            slicer.mrmlScene.Clear(0)
            slicer.util.loadScene(sceneFile)
            slicer.app.processEvents()
            volumeNode = slicer.util.getFirstNodeByClassByName("vtkMRMLAstroVolumeNode", "WEIN069")
            self.assertIsNotNone(volumeNode)
            slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
            slicer.app.processEvents()
            self.assertEqual(self.displayState(volumeNode), saved)
            # the 3D view shows the restored color function
            renderingDisplay = slicer.modules.volumerendering.logic().GetFirstVolumeRenderingDisplayNode(volumeNode)
            self.assertIsNotNone(renderingDisplay)
            self.assertIs(renderingDisplay.GetVolumePropertyNode(), volumeNode.GetVolumePropertyNode())
            # one color table per name (singletons, not duplicated by the import)
            self.assertEqual(len([node for node in slicer.util.getNodesByClass("vtkMRMLColorTableNode")
                                  if node.GetName() == "Heat"]), 1)
        finally:
            os.remove(sceneFile)
        self.delayDisplay("Test passed")
