import numpy as np

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)

BOX, GAUSSIAN, GRADIENT = 0, 1, 2


class AstroSmoothingSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Smoothing SelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroSmoothing"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Self test of the AstroSmoothing module (CPU): box, Gaussian and gradient
smoothing of a data cube are compared with independent numpy/scipy
computations.
"""
        self.parent.hidden = True


class AstroSmoothingSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroSmoothingSelfTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of an (astro) volume node."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def oddSize(size):
    size = max(int(size), 1)
    return size + 1 if size % 2 == 0 else size


def referenceKernel(cube, kernelZYX):
    """Correlation with zero padding; blank voxels contribute zero."""
    from scipy import ndimage
    return ndimage.correlate(np.nan_to_num(cube.astype(np.float64)), kernelZYX, mode="constant", cval=0.0)


def referenceSeparable(cube, kernel1D, axesXYZ):
    """1D correlations along x, y, z (only the enabled axes)."""
    from scipy import ndimage
    result = np.nan_to_num(cube.astype(np.float64))
    for axisXYZ in (0, 1, 2):
        if axesXYZ[axisXYZ]:
            result = ndimage.correlate1d(result, kernel1D, axis=2 - axisXYZ, mode="constant", cval=0.0)
    return result


def referenceGradient(cube, conductance, timeStep, k, noise, iterations):
    """Intensity driven diffusion with reflecting borders; voxels with a blank
    neighbour are left unchanged."""
    current = cube.astype(np.float64)
    noise2 = (noise * k) ** 2
    for _ in range(iterations):
        padded = np.pad(current, 1, mode="edge")
        center = padded[1:-1, 1:-1, 1:-1]
        neighbours = [
            (padded[1:-1, 1:-1, :-2], padded[1:-1, 1:-1, 2:], conductance[0]),  # x
            (padded[1:-1, :-2, 1:-1], padded[1:-1, 2:, 1:-1], conductance[1]),  # y
            (padded[:-2, 1:-1, 1:-1], padded[2:, 1:-1, 1:-1], conductance[2]),  # z
        ]
        flux = np.zeros_like(center)
        blank = np.isnan(center)
        for minus, plus, c in neighbours:
            flux += c * ((minus - center) + (plus - center))
            blank |= np.isnan(minus) | np.isnan(plus)
        updated = center + timeStep * flux / (1.0 + center * center / noise2)
        current = np.where(blank, center, updated)
    return current


class AstroSmoothingSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_AstroSmoothingSelfTest()

    def smooth(self, parametersNode, configure, hardware=0):
        """Configure the parameters node, run the module (0: CPU, 1: GPU) and return the output voxels."""
        parametersNode.SetHardware(hardware)
        configure(parametersNode)
        parametersNode.SetGaussianKernels()
        widget = slicer.modules.astrosmoothing.widgetRepresentation()
        slicer.util.findChild(widget, "ApplyButton").click()
        outputVolume = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        self.assertIsNotNone(outputVolume)
        return voxelArray(outputVolume).astype(np.float64)

    def assertClose(self, computed, expected, scale, name, relativeTolerance=1e-5):
        self.assertEqual(computed.shape, expected.shape, name)
        self.assertFalse(np.any(np.isnan(computed) != np.isnan(expected)), name + ": blank voxels differ")
        maxDiff = np.nanmax(np.abs(computed - expected))
        print(f"{name}: max difference {maxDiff:.3g} (scale {scale:.3g})")
        self.assertLess(maxDiff, relativeTolerance * scale, name)

    def test_AstroSmoothingSelfTest(self):
        import AstroSampleData
        from vtk.util.numpy_support import vtk_to_numpy

        self.delayDisplay("Loading WEIN069")
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)

        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroSmoothing")
        parametersNode = slicer.util.getNode("AstroSmoothingParameters")
        parametersNode.SetInputVolumeNodeID(volumeNode.GetID())

        cube = voxelArray(volumeNode).astype(np.float64)
        scale = np.nanmax(np.abs(cube))

        # Anisotropic box (3D kernel)
        self.delayDisplay("Anisotropic box")
        def anisotropicBox(p):
            p.SetFilter(BOX)
            p.SetParameterX(3)
            p.SetParameterY(5)
            p.SetParameterZ(7)
        computed = self.smooth(parametersNode, anisotropicBox)
        sizes = (oddSize(7), oddSize(5), oddSize(3))  # z, y, x
        expected = referenceKernel(cube, np.full(sizes, 1.0 / np.prod(sizes)))
        self.assertClose(computed, expected, scale, "anisotropic box")

        # Isotropic box (separable)
        self.delayDisplay("Isotropic box")
        def isotropicBox(p):
            p.SetFilter(BOX)
            p.SetParameterX(5)
            p.SetParameterY(5)
            p.SetParameterZ(5)
        computed = self.smooth(parametersNode, isotropicBox)
        expected = referenceSeparable(cube, np.full(5, 1.0 / 5), (True, True, True))
        self.assertClose(computed, expected, scale, "isotropic box")

        # Isotropic Gaussian (separable, kernel from the parameters node)
        self.delayDisplay("Isotropic Gaussian")
        def isotropicGaussian(p):
            p.SetFilter(GAUSSIAN)
            p.SetParameterX(3)
            p.SetParameterY(3)
            p.SetParameterZ(3)
            p.SetRx(0)
            p.SetRy(0)
            p.SetRz(0)
        computed = self.smooth(parametersNode, isotropicGaussian)
        kernel1D = vtk_to_numpy(parametersNode.GetGaussianKernel1D()).astype(np.float64)
        expected = referenceSeparable(cube, kernel1D, (True, True, True))
        self.assertClose(computed, expected, scale, "isotropic Gaussian")

        # Anisotropic rotated Gaussian (3D kernel from the parameters node)
        self.delayDisplay("Anisotropic Gaussian")
        def anisotropicGaussian(p):
            p.SetFilter(GAUSSIAN)
            p.SetParameterX(3)
            p.SetParameterY(2)
            p.SetParameterZ(4)
            p.SetRx(10)
            p.SetRy(20)
            p.SetRz(30)
        computed = self.smooth(parametersNode, anisotropicGaussian)
        kernel3D = vtk_to_numpy(parametersNode.GetGaussianKernel3D()).astype(np.float64).reshape(
            int(parametersNode.GetKernelLengthZ()), int(parametersNode.GetKernelLengthY()),
            int(parametersNode.GetKernelLengthX()))
        expected = referenceKernel(cube, kernel3D)
        self.assertClose(computed, expected, scale, "anisotropic Gaussian")

        # Intensity driven gradient smoothing
        self.delayDisplay("Gradient")
        def gradient(p):
            p.SetFilter(GRADIENT)
            p.SetParameterX(0.5)
            p.SetParameterY(0.5)
            p.SetParameterZ(0.3)
            p.SetAccuracy(10)
        computed = self.smooth(parametersNode, gradient)
        expected = referenceGradient(cube, (0.5, 0.5, 0.3), parametersNode.GetTimeStep(), parametersNode.GetK(),
                                     float(volumeNode.GetAttribute("SlicerAstro.DisplayThreshold")),
                                     parametersNode.GetAccuracy())
        self.assertClose(computed, expected, scale, "gradient", relativeTolerance=1e-4)

        # GPU (OpenGL) implementations, compared with the CPU results
        self.delayDisplay("GPU (OpenGL)")
        def gradientOddIterations(p):
            gradient(p)
            p.SetAccuracy(7)  # odd number of GPU ping-pong passes
        for name, configure, tolerance in (("box", anisotropicBox, 1e-5),
                                           ("Gaussian", anisotropicGaussian, 1e-5),
                                           ("gradient", gradient, 1e-5),
                                           ("gradient (odd iterations)", gradientOddIterations, 1e-5)):
            cpu = self.smooth(parametersNode, configure, hardware=0)
            gpu = self.smooth(parametersNode, configure, hardware=1)
            self.assertClose(gpu, cpu, scale, "GPU " + name, relativeTolerance=tolerance)

        # the filters must not modify the input volume
        self.assertEqual(tuple(volumeNode.GetImageData().GetSpacing()), (1.0, 1.0, 1.0))
        self.assertTrue(np.array_equal(voxelArray(volumeNode), cube, equal_nan=True))

        self.delayDisplay("Test passed")
