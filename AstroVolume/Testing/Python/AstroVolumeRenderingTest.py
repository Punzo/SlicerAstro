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
the 3D display (Display ROI, crop, RMS in ROI) works.
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
        self.test_PresetNamesAndFunctions()
        self.setUp()
        self.test_PresetKeptWhenPresetsAreListed()
        self.setUp()
        self.test_ColorFunctionOptions()
        self.setUp()
        self.test_SceneRestoresColorFunctions()
        self.setUp()
        self.test_RenderingSection()

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
        widget = slicer.modules.astrovolume.widgetRepresentation().renderingSection()
        displayNode = volumeNode.GetAstroVolumeRenderingDisplayNode()
        self.assertIsNotNone(displayNode)

        # Crop creates the cropping ROI, fitted to the volume, and shows it
        self.assertIsNone(displayNode.GetMarkupsROINode())
        cropButton = slicer.util.findChild(widget, "RenderingCrop")
        cropButton.checked = True
        slicer.app.processEvents()
        roiNode = displayNode.GetMarkupsROINode()
        self.assertIsNotNone(roiNode)
        self.assertTrue(roiNode.IsA("vtkMRMLMarkupsROINode"))
        self.assertTrue(roiNode.GetDisplayVisibility())
        self.assertTrue(displayNode.GetCroppingEnabled())
        lower, upper = self.roiExtentIJK(roiNode, volumeNode)
        self.assertEqual(list(lower), [0, 0, 0])
        self.assertEqual(list(upper), [d - 1 for d in volumeNode.GetImageData().GetDimensions()])

        # No cropping: the ROI is hidden, not deleted
        cropButton.checked = False
        slicer.app.processEvents()
        self.assertFalse(roiNode.GetDisplayVisibility())
        self.assertIs(displayNode.GetMarkupsROINode(), roiNode)

        # Noise in the box: whole cube, then a corner of the cube
        rmsButton = slicer.util.findChild(widget, "RenderingNoiseInBox")
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
        """The 3D color function of a loaded cube: the surfaces at 3σ, 7σ,
        15σ in colours safe for colour-blind people (identifier
        ObservatorySurfaces)."""
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        slicer.app.processEvents()
        presetNode = volumeNode.GetPresetNode()
        self.assertIsNotNone(presetNode)
        self.assertEqual(slicer.vtkMRMLAstroVolumeNode.GetPresetIdentifier(presetNode), "ObservatorySurfaces")
        self.assertEqual(presetNode.GetName(), "Surfaces at 3σ, 7σ, 15σ · Observatory")
        self.assertEqual(volumeNode.GetPresetIndex(), slicer.vtkMRMLAstroVolumeNode.DefaultPreset)
        self.assertEqual(slicer.vtkMRMLAstroVolumeNode.DefaultPreset,
                         slicer.vtkMRMLAstroVolumeNode.ObservatorySurfacesPreset)
        self.delayDisplay("Test passed")

    def test_PresetKeptWhenPresetsAreListed(self):
        """Listing the presets (e.g. the 3D section of the Workbench) loads
        them once: the preset selector of the AstroVolume panel does not
        reapply the first preset to the active cube."""
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        slicer.app.processEvents()
        identifier = slicer.vtkMRMLAstroVolumeNode.GetPresetIdentifier(volumeNode.GetPresetNode())
        logic = slicer.modules.astrovolume.logic()
        first = [node.GetID() for node in slicer.util.getNodesByClass("vtkMRMLVolumePropertyNode",
                                                                      logic.GetPresetsScene())]
        second = [node.GetID() for node in slicer.util.getNodesByClass("vtkMRMLVolumePropertyNode",
                                                                       logic.GetPresetsScene())]
        slicer.app.processEvents()
        self.assertEqual(first, second)
        self.assertEqual(len(first), 13)
        self.assertEqual(slicer.vtkMRMLAstroVolumeNode.GetPresetIdentifier(volumeNode.GetPresetNode()), identifier)
        self.delayDisplay("Test passed")

    def test_ColorFunctionOptions(self):
        """Reverse, inverse and logarithmic 2D colour functions: the attribute
        is the state of the lookup table (setting the same state twice does
        not change the table again)."""
        import numpy as np

        logic = slicer.vtkSlicerAstroVolumeLogic
        colorNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLColorTableNode", "OptionsTest")
        colorNode.SetTypeToUser()
        colorNode.SetNumberOfColors(256)
        for index in range(256):
            colorNode.SetColor(index, index / 255.0, 0.5 * index / 255.0, 1.0 - index / 255.0, 1.0)
        colorNode.SetAttribute("SlicerAstro.AddFunctions", "on")

        def table():
            rgba = [0.0] * 4
            values = []
            for index in range(256):
                colorNode.GetLookupTable().GetTableValue(index, rgba)
                values.append(list(rgba))
            return np.array(values)

        original = table()
        self.assertTrue(logic.HasColorFunctionOptions(colorNode))
        self.assertFalse(logic.GetColorFunctionOption(colorNode, "Reverse"))
        for _ in range(2):
            self.assertTrue(logic.SetColorFunctionOption(colorNode, "Reverse", True))
            np.testing.assert_allclose(table(), original[::-1])
        self.assertTrue(logic.GetColorFunctionOption(colorNode, "Reverse"))
        self.assertTrue(logic.SetColorFunctionOption(colorNode, "Inverse", True))
        expected = original[::-1].copy()
        expected[:, :3] = 1.0 - expected[:, :3]
        np.testing.assert_allclose(table(), expected)
        logic.SetColorFunctionOption(colorNode, "Reverse", False)
        logic.SetColorFunctionOption(colorNode, "Inverse", False)
        np.testing.assert_allclose(table(), original, atol=1e-12)
        self.assertTrue(logic.SetColorFunctionOption(colorNode, "Log", True))
        self.assertEqual(colorNode.GetLookupTable().GetScale(), 1)  # VTK_SCALE_LOG10
        logic.SetColorFunctionOption(colorNode, "Log", False)
        self.assertEqual(colorNode.GetLookupTable().GetScale(), 0)
        self.assertFalse(logic.SetColorFunctionOption(colorNode, "Unknown", True))
        self.delayDisplay("Test passed")

    def test_PresetNamesAndFunctions(self):
        """The 3D colour functions: readable names, stable identifiers in the
        order of the selector, the new colour-blind safe surfaces and the
        emission ramps scaled to the noise of the cube."""
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        slicer.app.processEvents()
        logic = slicer.modules.astrovolume.logic()
        presetsScene = logic.GetPresetsScene()
        identifiers = ["LowConstantOpacity", "MediumConstantOpacity", "HighConstantOpacity", "OneSurfaceGreen",
                       "OneSurfaceWhite", "TwoSurfaces", "ThreeSurfaces", "BrightSurfaces", "ObservatorySurfaces",
                       "ObservatoryShells", "ObservatoryEmission", "MagmaEmission", "ViridisEmission"]
        presets = [presetsScene.GetNthNodeByClass(i, "vtkMRMLVolumePropertyNode")
                   for i in range(presetsScene.GetNumberOfNodesByClass("vtkMRMLVolumePropertyNode"))]
        self.assertEqual([slicer.vtkMRMLAstroVolumeNode.GetPresetIdentifier(p) for p in presets], identifiers)
        for preset in presets:
            self.assertIn("σ", preset.GetName())
        self.assertEqual(slicer.vtkMRMLAstroVolumeNode.ViridisEmissionPreset, len(identifiers) - 1)

        # functions scaled to the noise
        logic.synchronizePresetsToVolumeNode(volumeNode)
        noise = volumeNode.GetDisplayThreshold()
        dataMax = float(volumeNode.GetAttribute("SlicerAstro.DATAMAX"))
        surfaces = presets[identifiers.index("ObservatorySurfaces")].GetVolumeProperty().GetScalarOpacity()
        for sigma, opacity in ((3, 0.12), (7, 0.3), (15, 0.9)):
            if sigma * noise * 1.2 < dataMax:
                self.assertAlmostEqual(surfaces.GetValue(sigma * noise), opacity, places=3)
                self.assertLess(surfaces.GetValue(sigma * noise * 1.25), 0.05)
        emission = presets[identifiers.index("ObservatoryEmission")].GetVolumeProperty().GetScalarOpacity()
        self.assertAlmostEqual(emission.GetValue(3 * noise), 0.04, places=3)
        self.assertLess(emission.GetValue(2 * noise), 0.01)
        values = [emission.GetValue(s * noise) for s in (3, 5, 8, 12) if s * noise < dataMax]
        self.assertEqual(values, sorted(values))

        # a new function chosen in the selector: the copy of the volume keeps
        # the identifier, named "<preset> · <volume>"
        widget = slicer.modules.astrovolume.widgetRepresentation().renderingSection()
        widget.applyPreset(slicer.vtkMRMLAstroVolumeNode.ObservatorySurfacesPreset)
        slicer.app.processEvents()
        self.assertEqual(volumeNode.GetPresetIndex(), slicer.vtkMRMLAstroVolumeNode.ObservatorySurfacesPreset)
        copy = volumeNode.GetVolumePropertyNode()
        self.assertEqual(slicer.vtkMRMLAstroVolumeNode.GetPresetIdentifier(copy), "ObservatorySurfaces")
        self.assertEqual(copy.GetName(), "Surfaces at 3σ, 7σ, 15σ · Observatory · WEIN069")
        self.delayDisplay("Test passed")

    def displayState(self, volumeNode):
        displayNode = volumeNode.GetAstroVolumeDisplayNode()
        volumeProperty = volumeNode.GetVolumePropertyNode().GetVolumeProperty()
        return {
            "preset": slicer.vtkMRMLAstroVolumeNode.GetPresetIdentifier(volumeNode.GetPresetNode())
            if volumeNode.GetPresetNode() else None,
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
        moduleWidget = slicer.modules.astrovolume.widgetRepresentation()
        widget = moduleWidget.renderingSection()
        widget.applyPreset(slicer.vtkMRMLAstroVolumeNode.OneSurfaceWhitePreset)
        slicer.app.processEvents()
        for name, fraction in (("RenderingShift", 0.7), ("RenderingStretch", 0.6), ("RenderingOpacity", 0.3)):
            slider = slicer.util.findChild(widget, name)
            slider.value = slider.minimum + fraction * (slider.maximum - slider.minimum)
            slicer.app.processEvents()
        displayNode = volumeNode.GetAstroVolumeDisplayNode()
        # 2D color table chosen in the selector, as by the user
        colorSelector = slicer.util.findChild(moduleWidget.displaySection(), "DisplayColourTable")
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

    @staticmethod
    def functionNodes(volumeProperty):
        """(x, value...) of the opacity and color transfer functions"""
        nodes = []
        for function, size in ((volumeProperty.GetScalarOpacity(), 4), (volumeProperty.GetRGBTransferFunction(), 6)):
            values = []
            for index in range(function.GetSize()):
                node = [0.0] * size
                function.GetNodeValue(index, node)
                values.append(node)
            nodes.append(values)
        return nodes

    @staticmethod
    def controlPointsReference(volumeProperty, operation, value):
        """The transfer functions moved (or spread) by the control points
        items of VTK (the transfer function editors), first and last points
        fixed"""
        copy = vtk.vtkVolumeProperty()
        copy.DeepCopy(volumeProperty)
        opacity = vtk.vtkPiecewiseFunction()
        opacity.DeepCopy(volumeProperty.GetScalarOpacity())
        color = vtk.vtkColorTransferFunction()
        color.DeepCopy(volumeProperty.GetRGBTransferFunction())
        copy.SetScalarOpacity(opacity)
        copy.SetColor(color)
        opacityItem = vtk.vtkPiecewiseControlPointsItem()
        opacityItem.SetPiecewiseFunction(opacity)
        colorItem = vtk.vtkColorTransferControlPointsItem()
        colorItem.SetColorTransferFunction(color)
        for item in (opacityItem, colorItem):
            if operation == "move":
                item.MovePoints(vtk.vtkVector2f(value, 0.0), True)
            else:
                item.SpreadPoints(value, True)
        return AstroVolumeRenderingTestTest.functionNodes(copy)

    def assertFunctionsEqual(self, computed, expected, tolerance):
        for computedFunction, expectedFunction in zip(computed, expected):
            self.assertEqual(len(computedFunction), len(expectedFunction))
            for a, b in zip(computedFunction, expectedFunction):
                for x, y in zip(a, b):
                    self.assertAlmostEqual(x, y, delta=tolerance)

    def test_RenderingSection(self):
        """3D section (reusable widget of the AstroVolume panel and the
        Workbench) and its logic: show in 3D with the default color function,
        presets scaled to the noise, shift and stretch as the transfer
        function editors of VTK move the points, opacity shift, the color
        function follows the noise, crop box, noise in the box, quality;
        disabled for images."""
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        logic = slicer.modules.astrovolume.logic()
        section = slicer.qSlicerAstroRenderingSection()
        section.setVolumeNode(volumeNode)
        self.assertTrue(section.enabled)
        names = list(section.presetNames())
        self.assertGreater(len(names), 5)

        # shown in 3D: the default color function
        section.setRenderingVisible(True)
        displayNode = logic.GetVolumeRenderingDisplayNode(volumeNode, False)
        self.assertIsNotNone(displayNode)
        self.assertTrue(displayNode.GetVisibility())
        self.assertEqual(section.currentPreset(), slicer.vtkMRMLAstroVolumeNode.DefaultPreset)
        self.assertEqual(slicer.vtkMRMLAstroVolumeNode.GetPresetIdentifier(volumeNode.GetPresetNode()),
                         "ObservatorySurfaces")
        propertyNode = volumeNode.GetVolumePropertyNode()
        self.assertEqual(propertyNode, displayNode.GetVolumePropertyNode())
        self.assertEqual(propertyNode.GetName(), f"{names[section.currentPreset()]} · {volumeNode.GetName()}")

        # another preset: a copy of the preset of the presets scene, scaled
        # to the noise of the data
        index = names.index("Surfaces at 3σ, 7σ")
        section.applyPreset(index)
        self.assertEqual(section.currentPreset(), index)
        presets = slicer.util.getNodesByClass("vtkMRMLVolumePropertyNode", logic.GetPresetsScene())
        volumeProperty = propertyNode.GetVolumeProperty()
        self.assertFunctionsEqual(self.functionNodes(volumeProperty),
                                  self.functionNodes(presets[index].GetVolumeProperty()), 0.0)
        noise = volumeNode.GetDisplayThreshold()
        opacityX = [node[0] for node in self.functionNodes(volumeProperty)[0]]
        self.assertTrue(any(abs(x - 3 * noise) < noise for x in opacityX), (opacityX, noise))

        # shift and stretch: the points move as with the transfer function
        # editors of VTK (single precision positions there)
        dataRange = float(volumeNode.GetAttribute("SlicerAstro.DATAMAX")) - \
            float(volumeNode.GetAttribute("SlicerAstro.DATAMIN"))
        tolerance = 1e-6 * dataRange
        for operation, value, setter in (("move", 2.0 * noise, section.setShift),
                                         ("spread", 1.5 * noise, section.setStretch)):
            section.applyPreset(index)
            original = self.functionNodes(volumeProperty)
            expected = self.controlPointsReference(volumeProperty, operation, value)
            setter(value)
            self.assertFunctionsEqual(self.functionNodes(volumeProperty), expected, tolerance)
            # the inner points moved, the first and last did not
            moved = [abs(a[0] - b[0]) for a, b in zip(self.functionNodes(volumeProperty)[0], original[0])]
            self.assertGreater(max(moved[1:-1]), 0.5 * value, operation)
            self.assertEqual((moved[0], moved[-1]), (0.0, 0.0))
        # the change from the applied value: a second call does nothing
        before = self.functionNodes(volumeProperty)
        section.setStretch(1.5 * noise)
        self.assertFunctionsEqual(self.functionNodes(volumeProperty), before, 0.0)
        self.assertAlmostEqual(volumeNode.GetAstroVolumeDisplayNode().GetPresetStretch(), 1.5 * noise)
        # a negative stretch brings the points closer to the centre
        section.applyPreset(index)
        expected = self.controlPointsReference(volumeProperty, "spread", -0.5 * noise)
        section.setStretch(-0.5 * noise)
        self.assertFunctionsEqual(self.functionNodes(volumeProperty), expected, tolerance)

        # opacity shift: non-zero opacities, at most 1
        section.applyPreset(index)
        opacities = [node[1] for node in self.functionNodes(volumeProperty)[0]]
        section.setOpacityShift(0.3)
        shifted = [node[1] for node in self.functionNodes(volumeProperty)[0]]
        for old, new in zip(opacities, shifted):
            self.assertAlmostEqual(new, old if abs(old) < 1e-4 else min(old + 0.3, 1.0), places=12)

        # the color function follows the noise: applied again, shifts at 0
        section.setShift(noise)
        volumeNode.SetDisplayThreshold(2.0 * noise)
        slicer.app.processEvents()
        display = volumeNode.GetAstroVolumeDisplayNode()
        self.assertEqual((display.GetPresetOffset(), display.GetPresetStretch(), display.GetOpacityShift()),
                         (0.0, 0.0, 0.0))
        opacityX = [node[0] for node in self.functionNodes(volumeProperty)[0]]
        self.assertTrue(any(abs(x - 6 * noise) < noise for x in opacityX), (opacityX, noise))
        volumeNode.SetDisplayThreshold(noise)

        # crop box and noise of the voxels in the box
        section.setCropping(True)
        roiNode = logic.GetVolumeRenderingROINode(volumeNode, False)
        self.assertIsNotNone(roiNode)
        self.assertTrue(displayNode.GetCroppingEnabled())
        self.assertTrue(roiNode.GetDisplayVisibility())
        section.setCropping(False)
        self.assertFalse(displayNode.GetCroppingEnabled())
        center, radius = [0.0] * 3, [0.0] * 3
        roiNode.GetXYZ(center)
        roiNode.GetRadiusXYZ(radius)
        roiNode.SetRadiusXYZ([r / 4 for r in radius])
        roiNode.SetXYZ([c - r / 2 for c, r in zip(center, radius)])
        expectedRMS = self.expectedRMS(roiNode, volumeNode)
        measured = section.computeNoiseInBox()
        self.assertAlmostEqual(measured / expectedRMS, 1.0, places=5)
        self.assertAlmostEqual(volumeNode.GetDisplayThreshold() / expectedRMS, 1.0, places=5)
        self.assertIn("from the box", section.status())

        # quality of the 3D views showing the cube
        section.setQuality(2)
        self.assertEqual(logic.GetVolumeRenderingQuality(volumeNode), 2)
        section.setRenderingVisible(False)
        self.assertFalse(displayNode.GetVisibility())

        # an image: no 3D rendering
        image = AstroSampleData.AstroSampleDataLogic().downloadSample("NGC2403_DSS")
        section.setVolumeNode(image)
        self.assertFalse(section.enabled)
        self.assertFalse(slicer.util.findChild(section, "RenderingNote").isHidden())
        self.delayDisplay("Test passed")
