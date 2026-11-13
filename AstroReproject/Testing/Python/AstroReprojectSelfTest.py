import numpy as np

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroReprojectSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Reproject SelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroReproject"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Self test of the AstroReproject module on real data: the NGC 2403 DSS image
reprojected on a north-up grid (manual grid from the input), and the NGC 2403
HI cube reprojected on the (rotated) grid of the DSS image, are compared voxel
by voxel with the input data at the same world coordinates (nearest neighbour
interpolation).
"""
        self.parent.hidden = True


class AstroReprojectSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroReprojectSelfTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of an (astro) volume node."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def compareWithInput(testCase, inputNode, outputNode, samples=400, margin=0.15, seed=2403):
    """Output voxels against the input voxels at the same celestial coordinates.

    The reprojection is spatial only: channel k of the output is channel k of
    the input. Nearest neighbour: the input pixel is the rounded WCS position.
    Returns the number of compared voxels."""
    inputDisplay = inputNode.GetAstroVolumeDisplayNode()
    outputDisplay = outputNode.GetAstroVolumeDisplayNode()
    inputVoxels = voxelArray(inputNode)
    outputVoxels = voxelArray(outputNode)
    nz, ny, nx = outputVoxels.shape
    rng = np.random.default_rng(seed)
    compared = 0
    matches = 0
    for _ in range(samples):
        i = int(rng.integers(int(margin * nx), int((1. - margin) * nx)))
        j = int(rng.integers(int(margin * ny), int((1. - margin) * ny)))
        k = int(rng.integers(0, nz))
        world = [0., 0., 0.]
        testCase.assertTrue(outputDisplay.GetReferenceSpace([float(i), float(j), float(k)], world))
        ijk = [0., 0., 0.]
        testCase.assertTrue(inputDisplay.GetIJKSpace(world, ijk))
        x = int(np.floor(ijk[0] + 0.5))
        y = int(np.floor(ijk[1] + 0.5))
        if not (0 <= x < inputVoxels.shape[2] and 0 <= y < inputVoxels.shape[1]):
            continue
        expected = float(inputVoxels[k, y, x])
        value = float(outputVoxels[k, j, i])
        compared += 1
        if (np.isnan(expected) and np.isnan(value)) or np.isclose(value, expected, rtol=1.e-5, atol=0.):
            matches += 1
    testCase.assertGreater(compared, samples // 2)
    # positions within float rounding of a pixel edge can pick the neighbour
    testCase.assertGreaterEqual(matches, 0.98 * compared)
    return compared


class AstroReprojectSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_NorthAlignment()
        self.setUp()
        self.test_ReprojectionOnReference()

    def reprojectModule(self, inputNode):
        """Select the AstroReproject module with inputNode as input: the
        parameters node and the Apply button."""
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActiveVolumeID(inputNode.GetID())
        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroReproject")
        widget = slicer.modules.astroreproject.widgetRepresentation()
        parametersNode = widget.mrmlAstroReprojectParametersNode()
        self.assertIsNotNone(parametersNode)
        parametersNode.SetInputVolumeNodeIDs(inputNode.GetID())
        parametersNode.SetInputVolumeNodeID(inputNode.GetID())
        parametersNode.SetInterpolationOrder(slicer.vtkMRMLAstroReprojectParametersNode.NearestNeighbour)
        applyButton = slicer.util.findChild(widget, "ApplyButton")
        return parametersNode, applyButton

    def outputVolume(self, parametersNode):
        outputNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        self.assertIsNotNone(outputNode)
        self.assertIsNotNone(outputNode.GetImageData())
        return outputNode

    def test_NorthAlignment(self):
        """The DSS image of NGC 2403 (CROTA2 of about -2.8 degrees) on a
        north-up grid with its pixel size (manual grid from the input)."""
        import AstroSampleData

        self.delayDisplay("Aligning the NGC 2403 DSS image to the north")
        dssNode = AstroSampleData.AstroSampleDataLogic().downloadSample("NGC2403_DSS")
        self.assertIsNotNone(dssNode)
        self.assertGreater(abs(float(dssNode.GetAttribute("SlicerAstro.CROTA2") or 0.)), 1.)

        P = slicer.vtkMRMLAstroReprojectParametersNode
        parametersNode, applyButton = self.reprojectModule(dssNode)
        parametersNode.SetSpatialGridMode(P.SpatialManual)
        logic = slicer.modules.astroreproject.logic()
        self.assertTrue(logic.SetManualGridFromInput(parametersNode, True, False, True), logic.GetLastError())
        self.assertEqual(parametersNode.GetRotation(), 0.)
        applyButton.click()
        slicer.app.processEvents()

        northNode = self.outputVolume(parametersNode)
        self.assertNotEqual(northNode.GetID(), dssNode.GetID())
        self.assertEqual(float(northNode.GetAttribute("SlicerAstro.CROTA2")), 0.)
        # the north-up grid encloses the rotated image
        northDims = northNode.GetImageData().GetDimensions()
        dssDims = dssNode.GetImageData().GetDimensions()
        self.assertGreaterEqual(northDims[0], dssDims[0])
        self.assertGreaterEqual(northDims[1], dssDims[1])
        compareWithInput(self, dssNode, northNode)
        # the input data are not modified
        self.assertGreater(abs(float(dssNode.GetAttribute("SlicerAstro.CROTA2"))), 1.)
        self.delayDisplay("Test passed")

    def test_ReprojectionOnReference(self):
        """The HI cube of NGC 2403 reprojected on the (rotated) grid of the DSS
        image, with its own channels."""
        import AstroSampleData

        self.delayDisplay("Reprojecting the NGC 2403 HI cube on the DSS image")
        logic = AstroSampleData.AstroSampleDataLogic()
        hiNode = logic.downloadSample("NGC2403")
        dssNode = logic.downloadSample("NGC2403_DSS")
        self.assertIsNotNone(hiNode)
        self.assertIsNotNone(dssNode)

        P = slicer.vtkMRMLAstroReprojectParametersNode
        parametersNode, applyButton = self.reprojectModule(hiNode)
        parametersNode.SetSpatialGridMode(P.SpatialReference)
        parametersNode.SetSpectralGridMode(P.SpectralInput)
        parametersNode.SetReferenceVolumeNodeID(dssNode.GetID())
        applyButton.click()
        slicer.app.processEvents()

        outputNode = self.outputVolume(parametersNode)
        hiDims = hiNode.GetImageData().GetDimensions()
        dssDims = dssNode.GetImageData().GetDimensions()
        self.assertEqual(outputNode.GetImageData().GetDimensions(), (dssDims[0], dssDims[1], hiDims[2]))
        self.assertAlmostEqual(float(outputNode.GetAttribute("SlicerAstro.CROTA2")),
                               float(dssNode.GetAttribute("SlicerAstro.CROTA2")), places=6)
        compareWithInput(self, hiNode, outputNode)
        self.delayDisplay("Test passed")
