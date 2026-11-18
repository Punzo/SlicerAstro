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
        self.setUp()
        self.test_AstroSmoothingGPU()
        self.setUp()
        self.test_ImagesAndSpectra()
        self.setUp()
        self.test_SmoothedBeam()

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
        def spatialGaussian(p):
            # no smoothing along the spectral axis (FWHM 0: weights were NaN)
            p.SetFilter(GAUSSIAN)
            p.SetParameterX(3)
            p.SetParameterY(3)
            p.SetParameterZ(0)
            p.SetRx(0)
            p.SetRy(0)
            p.SetRz(0)
        for name, configure, tolerance in (("box", anisotropicBox, 1e-5),
                                           ("Gaussian", anisotropicGaussian, 1e-5),
                                           ("Gaussian on the sky only", spatialGaussian, 1e-5),
                                           ("gradient", gradient, 1e-5),
                                           ("gradient (odd iterations)", gradientOddIterations, 1e-5)):
            cpu = self.smooth(parametersNode, configure, hardware=0)
            gpu = self.smooth(parametersNode, configure, hardware=1)
            self.assertClose(gpu, cpu, scale, "GPU " + name, relativeTolerance=tolerance)

        # the filters must not modify the input volume
        self.assertEqual(tuple(volumeNode.GetImageData().GetSpacing()), (1.0, 1.0, 1.0))
        self.assertTrue(np.array_equal(voxelArray(volumeNode), cube, equal_nan=True))

        self.delayDisplay("Test passed")

    def test_AstroSmoothingGPU(self):
        """The GPU filters (OpenGL, or WebGPU if available) through the module
        (the Apply button: as the users run them), against the references."""
        import AstroSampleData
        from vtk.util.numpy_support import vtk_to_numpy

        backend = slicer.modules.astrosmoothing.logic().GetGPUBackend()
        if not backend:
            # (runTest calls the tests directly: no unittest skip)
            self.delayDisplay("GPU smoothing: skipped (no GPU backend)")
            return
        self.delayDisplay("GPU smoothing (%s)" % backend)
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroSmoothing")
        parametersNode = slicer.util.getNode("AstroSmoothingParameters")
        parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
        cube = voxelArray(volumeNode).astype(np.float64)
        scale = np.nanmax(np.abs(cube))

        def isotropicBox(p):
            p.SetFilter(BOX)
            p.SetParameterX(5)
            p.SetParameterY(5)
            p.SetParameterZ(5)
        computed = self.smooth(parametersNode, isotropicBox, hardware=1)
        expected = referenceSeparable(cube, np.full(5, 1.0 / 5), (True, True, True))
        self.assertClose(computed, expected, scale, "GPU isotropic box", relativeTolerance=1e-4)

        def isotropicGaussian(p):
            p.SetFilter(GAUSSIAN)
            p.SetParameterX(3)
            p.SetParameterY(3)
            p.SetParameterZ(3)
            p.SetRx(0)
            p.SetRy(0)
            p.SetRz(0)
        computed = self.smooth(parametersNode, isotropicGaussian, hardware=1)
        kernel1D = vtk_to_numpy(parametersNode.GetGaussianKernel1D()).astype(np.float64)
        expected = referenceSeparable(cube, kernel1D, (True, True, True))
        self.assertClose(computed, expected, scale, "GPU isotropic Gaussian", relativeTolerance=1e-4)

        def gradient(p):
            p.SetFilter(GRADIENT)
            p.SetParameterX(0.5)
            p.SetParameterY(0.5)
            p.SetParameterZ(0.3)
            p.SetAccuracy(10)
        computed = self.smooth(parametersNode, gradient, hardware=1)
        expected = referenceGradient(cube, (0.5, 0.5, 0.3), parametersNode.GetTimeStep(), parametersNode.GetK(),
                                     float(volumeNode.GetAttribute("SlicerAstro.DisplayThreshold")),
                                     parametersNode.GetAccuracy())
        self.assertClose(computed, expected, scale, "GPU gradient", relativeTolerance=1e-3)
        self.delayDisplay("Test passed")

    def test_ImagesAndSpectra(self):
        """Smoothing of 2D images and 1D spectra: only along their axes (the
        parameters of the missing axes are ignored, and restored after the
        filter), with blank voxels."""
        import os
        from AstroFITSIOTest import beamCards, celestialWCS, writeFITS
        from vtk.util.numpy_support import vtk_to_numpy

        directory = os.path.join(slicer.app.temporaryPath, "AstroSmoothingSelfTest")
        os.makedirs(directory, exist_ok=True)
        rng = np.random.default_rng(3)
        image = rng.normal(0.0, 1.0, (40, 50)).astype(np.float32)
        image[18:24, 20:30] += 8.0
        image[5, 7] = np.nan
        imagePath = os.path.join(directory, "image.fits")
        # in K: the values are not rescaled with the beam (Jy/beam data are, see test_SmoothedBeam)
        writeFITS(imagePath, image, -32, celestialWCS(crpix=(25.0, 20.0)) + beamCards("K"))
        spectrum = rng.normal(0.0, 1.0, (1, 64)).astype(np.float32)
        spectrum[0, 30:36] += 6.0
        spectrum[0, 10] = np.nan
        spectrumPath = os.path.join(directory, "spectrum.fits")
        writeFITS(spectrumPath, spectrum, -32, [("CTYPE1", "VRAD"), ("CRVAL1", 1000.0), ("CDELT1", 5000.0),
                                                ("CRPIX1", 1.0), ("CUNIT1", "m/s"), ("CTYPE2", "DEC--SIN"),
                                                ("CRVAL2", 30.0), ("CDELT2", 0.001), ("CRPIX2", 1.0),
                                                ("CUNIT2", "deg"), ("BUNIT", "JY/BEAM")])

        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroSmoothing")
        parametersNode = slicer.util.getNode("AstroSmoothingParameters")

        for path, axes in ((imagePath, (True, True, False)), (spectrumPath, (True, False, False))):
            volumeNode = slicer.util.loadVolume(path)
            self.assertIsNotNone(volumeNode)
            parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
            data = voxelArray(volumeNode).astype(np.float64)
            scale = np.nanmax(np.abs(data))
            name = "image" if axes[1] else "spectrum"
            self.assertEqual(data.shape[0], 1)

            def box(p):
                p.SetFilter(BOX)
                p.SetParameterX(3)
                p.SetParameterY(5)
                p.SetParameterZ(7)
            computed = self.smooth(parametersNode, box)
            sizes = (1, oddSize(5) if axes[1] else 1, oddSize(3))
            expected = referenceKernel(data, np.full(sizes, 1.0 / np.prod(sizes)))
            self.assertClose(computed, expected, scale, f"{name} box")
            # the parameters of the user are kept
            self.assertEqual((parametersNode.GetParameterY(), parametersNode.GetParameterZ()), (5, 7))

            def gaussian(p):
                p.SetFilter(GAUSSIAN)
                p.SetParameterX(3)
                p.SetParameterY(3)
                p.SetParameterZ(3)
                p.SetRx(0)
                p.SetRy(0)
                p.SetRz(0)
            computed = self.smooth(parametersNode, gaussian)
            parametersNode.SetParameterZ(0)
            if not axes[1]:
                parametersNode.SetParameterY(0)
            parametersNode.SetGaussianKernels()
            kernel3D = vtk_to_numpy(parametersNode.GetGaussianKernel3D()).astype(np.float64).reshape(
                int(parametersNode.GetKernelLengthZ()), int(parametersNode.GetKernelLengthY()),
                int(parametersNode.GetKernelLengthX()))
            expected = referenceKernel(data, kernel3D)
            self.assertClose(computed, expected, scale, f"{name} Gaussian")

            def gradient(p):
                p.SetFilter(GRADIENT)
                p.SetParameterX(0.5)
                p.SetParameterY(0.5)
                p.SetParameterZ(0.3)
                p.SetAccuracy(10)
            computed = self.smooth(parametersNode, gradient)
            expected = referenceGradient(data, (0.5, 0.5 if axes[1] else 0.0, 0.0), parametersNode.GetTimeStep(),
                                         parametersNode.GetK(),
                                         float(volumeNode.GetAttribute("SlicerAstro.DisplayThreshold")),
                                         parametersNode.GetAccuracy())
            self.assertClose(computed, expected, scale, f"{name} gradient", relativeTolerance=1e-4)

            # GPU implementations (one-slice textures), compared with the CPU
            for filterName, configure in (("box", box), ("Gaussian", gaussian), ("gradient", gradient)):
                cpu = self.smooth(parametersNode, configure, hardware=0)
                gpu = self.smooth(parametersNode, configure, hardware=1)
                self.assertClose(gpu, cpu, scale, f"GPU {name} {filterName}")
        self.delayDisplay("Test passed")

    def test_SmoothedBeam(self):
        """Gaussian smoothing of Jy/beam data: the beam of the output is the
        beam convolved with the kernel, the data are scaled by the ratio of
        the beam areas (a point source keeps its peak, the flux is kept); the
        kernel on the sky follows the WCS (position angle of a rotated
        kernel)."""
        import math
        import os
        from AstroFITSIOTest import celestialWCS, radioVelocityWCS, writeFITS
        from vtkSlicerAstroSmoothingModuleLogicPython import vtkSlicerAstroSmoothingLogic

        directory = os.path.join(slicer.app.temporaryPath, "AstroSmoothingSelfTest")
        os.makedirs(directory, exist_ok=True)
        cdelt = 0.001  # degrees: 3.6"
        beamPixels = 4.0  # FWHM of the beam
        n, channels = 64, 5
        y, x = np.mgrid[0:n, 0:n]
        sigma = beamPixels / (2 * math.sqrt(2 * math.log(2)))
        source = np.exp(-((x - 32) ** 2 + (y - 32) ** 2) / (2 * sigma ** 2))
        cube = np.repeat((2.0 * source)[np.newaxis], channels, axis=0).astype(np.float32)  # 2 Jy point source
        path = os.path.join(directory, "beam.fits")
        beam = beamPixels * cdelt
        writeFITS(path, cube, -32, celestialWCS(crpix=(33.0, 33.0), cdelt=(-cdelt, cdelt)) + radioVelocityWCS() +
                  [("BUNIT", "JY/BEAM"), ("BMAJ", beam), ("BMIN", beam), ("BPA", 0.0)])
        volumeNode = slicer.util.loadVolume(path)
        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroSmoothing")
        parametersNode = slicer.util.getNode("AstroSmoothingParameters")
        parametersNode.SetInputVolumeNodeID(volumeNode.GetID())

        def gaussian(p):
            p.SetFilter(GAUSSIAN)
            p.SetParameterX(3)
            p.SetParameterY(3)
            p.SetParameterZ(0)
            p.SetRx(0)
            p.SetRy(0)
            p.SetRz(0)
            p.SetAccuracy(4)  # the kernel to 4 sigma
        computed = self.smooth(parametersNode, gaussian)
        # kernel length: 4 sigma on each side
        sigmaKernel = 3 / (2 * math.sqrt(2 * math.log(2)))
        self.assertEqual(parametersNode.GetKernelLengthX(), 2 * math.ceil(4 * sigmaKernel) + 1)
        output = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        expectedBeam = math.hypot(beamPixels, 3.0) * cdelt
        self.assertAlmostEqual(float(output.GetAttribute("SlicerAstro.BMAJ")) / expectedBeam, 1.0, delta=1e-6)
        self.assertAlmostEqual(float(output.GetAttribute("SlicerAstro.BMIN")) / expectedBeam, 1.0, delta=1e-6)
        # point source: its peak in Jy/beam is kept (sampled Gaussians: 1%)
        self.assertAlmostEqual(computed[2].max() / cube[2].max(), 1.0, delta=0.01)
        # flux: sum * pixel area / beam area
        fluxIn = cube[2].sum() / ((math.pi / (4 * math.log(2))) * beamPixels ** 2)
        fluxOut = computed[2].sum() / ((math.pi / (4 * math.log(2))) * (expectedBeam / cdelt) ** 2)
        self.assertAlmostEqual(fluxOut / fluxIn, 1.0, delta=1e-3)

        # rotated elliptical kernel: FWHM 4 x 2 pixels, Rz 30 degrees; with
        # RA increasing to the left (CDELT1 < 0) its major axis is at
        # position angle 60 degrees on the sky
        parametersNode.SetFilter(GAUSSIAN)
        parametersNode.SetParameterX(4)
        parametersNode.SetParameterY(2)
        parametersNode.SetParameterZ(0)
        parametersNode.SetRz(30)
        newBeam, kernel = [0.0] * 3, [0.0] * 3
        self.assertTrue(vtkSlicerAstroSmoothingLogic.ComputeSmoothedBeam(volumeNode, parametersNode, newBeam, kernel))
        self.assertAlmostEqual(kernel[0], 4 * cdelt * 3600, delta=1e-3)
        self.assertAlmostEqual(kernel[1], 2 * cdelt * 3600, delta=1e-3)
        self.assertAlmostEqual(kernel[2], 60.0, delta=0.05)
        # box filters have no Gaussian beam
        parametersNode.SetFilter(BOX)
        self.assertFalse(vtkSlicerAstroSmoothingLogic.ComputeSmoothedBeam(volumeNode, parametersNode, newBeam, kernel))
        self.delayDisplay("Test passed")
