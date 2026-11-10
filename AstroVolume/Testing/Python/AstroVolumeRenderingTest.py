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
