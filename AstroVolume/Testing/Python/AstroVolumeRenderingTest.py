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
keep their points far enough apart for the lookup table textures, and the
OpenGL volume mapper does not warn about the texture size.
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
