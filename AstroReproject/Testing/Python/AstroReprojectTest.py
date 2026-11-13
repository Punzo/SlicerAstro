import math
import os
import shutil

import numpy as np

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)

SPEED_OF_LIGHT = 299792458.0
HI_REST_FREQUENCY = 1.420405751768e9
CO_REST_FREQUENCY = 115.2712018e9


class AstroReprojectTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Reproject Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroReproject"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Tests of the AstroReproject engine on synthetic data with an analytic truth:
identity, rotation and projection changes, celestial frames, flux conservation
(area average and exact overlap), resampling factors, spectral regridding
(frequency, radio and optical velocities, spectral frames, velocity matching of
different lines), linear mosaics with primary beams, common and target beams.
"""
        self.parent.hidden = True


class AstroReprojectTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroReprojectTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of an (astro) volume node."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def fitsCard(key, value=None):
    """80-character header card."""
    if value is None:
        return key.ljust(80)[:80]
    if isinstance(value, bool):
        text = ("T" if value else "F").rjust(20)
    elif isinstance(value, (int, np.integer)):
        text = str(int(value)).rjust(20)
    elif isinstance(value, (float, np.floating)):
        text = repr(float(value)).upper().rjust(20)
    else:
        text = ("'" + str(value).replace("'", "''").ljust(8) + "'").ljust(20)
    return f"{key:8s}= {text}"[:80].ljust(80)


def writeFITS(path, data, cards):
    """Single float image HDU with the WCS cards (list of (key, value))."""
    header = [("SIMPLE", True), ("BITPIX", -32), ("NAXIS", data.ndim)]
    header += [(f"NAXIS{axis}", size) for axis, size in enumerate(reversed(data.shape), start=1)]
    text = "".join(fitsCard(*card) for card in header + list(cards)) + "END".ljust(80)
    text += " " * (-len(text) % 2880)
    payload = np.ascontiguousarray(data, dtype=">f4").tobytes()
    with open(path, "wb") as f:
        f.write(text.encode("ascii") + payload + b"\0" * (-len(payload) % 2880))


def angularDistance(lon1, lat1, lon2, lat2):
    """Great-circle distance (degrees)."""
    lon1, lat1, lon2, lat2 = (math.radians(v) for v in (lon1, lat1, lon2, lat2))
    s = (math.sin((lat2 - lat1) / 2.) ** 2 +
         math.cos(lat1) * math.cos(lat2) * math.sin((lon2 - lon1) / 2.) ** 2)
    return math.degrees(2. * math.asin(min(1., math.sqrt(s))))


def gaussianSource(lon0, lat0, fwhm, peak=1.):
    """Circular Gaussian on the sky (degrees)."""
    sigma = fwhm / (2. * math.sqrt(2. * math.log(2.)))
    return lambda lon, lat: peak * math.exp(-0.5 * (angularDistance(lon, lat, lon0, lat0) / sigma) ** 2)


def celestialCards(projection="SIN", lon="RA--", lat="DEC-", crval=(180., 30.), crpix=(32.5, 32.5),
                   cdelt=(-0.002, 0.002), crota=None, frame=(("RADESYS", "FK5"), ("EQUINOX", 2000.))):
    cards = [("CTYPE1", f"{lon}-{projection}"), ("CRVAL1", crval[0]), ("CDELT1", cdelt[0]),
             ("CRPIX1", crpix[0]), ("CUNIT1", "deg"),
             ("CTYPE2", f"{lat}-{projection}"), ("CRVAL2", crval[1]), ("CDELT2", cdelt[1]),
             ("CRPIX2", crpix[1]), ("CUNIT2", "deg")]
    if crota is not None:
        cards.append(("CROTA2", crota))
    return cards + list(frame)


class AstroReprojectTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroReprojectTest")
        if os.path.isdir(self.directory):
            shutil.rmtree(self.directory)
        os.makedirs(self.directory)
        self.logic = slicer.modules.astroreproject.logic()

    def runTest(self):
        for test in (self.test_Identity, self.test_RotationAndProjection, self.test_CelestialFrames,
                     self.test_FluxConservation, self.test_ResampleFactors, self.test_SpectralAxes,
                     self.test_SpectralFrames, self.test_VelocityMatching, self.test_SpectralOverlap,
                     self.test_Mosaic, self.test_CommonBeam, self.test_TargetBeam, self.test_ManualGrid,
                     self.test_Widget):
            self.setUp()
            test()

    #
    # helpers
    #

    def volume(self, name, shape, cards, model=None, spectralModel=None):
        """Astro volume of shape (z, y, x) with the WCS cards; voxels: model(lon,
        lat) (degrees) times spectralModel(v) (display spectral value of the
        channel, m/s), evaluated at the world coordinates of each voxel."""
        data = np.zeros(shape, dtype=np.float32)
        path = os.path.join(self.directory, name + ".fits")
        writeFITS(path, data if len(shape) == 3 else data[0], cards)
        node = slicer.util.loadVolume(path)
        self.assertIsNotNone(node, name)
        node.SetName(name)
        if model is not None:
            voxels = voxelArray(node)
            nz, ny, nx = voxels.shape
            plane = np.zeros((ny, nx))
            for j in range(ny):
                for i in range(nx):
                    world = self.world(node, (i, j, 0))
                    plane[j, i] = model(world[0], world[1])
            spectrum = np.ones(nz)
            if spectralModel is not None:
                for k in range(nz):
                    spectrum[k] = spectralModel(self.world(node, (0, 0, k))[2])
            voxels[:] = (spectrum[:, None, None] * plane[None, :, :]).astype(np.float32)
            node.GetImageData().Modified()
            node.UpdateRangeAttributes()
        return node

    def world(self, node, ijk):
        world = [0., 0., 0.]
        self.assertTrue(node.GetAstroVolumeDisplayNode().GetReferenceSpace([float(v) for v in ijk], world))
        return world

    def parameters(self, inputs, **values):
        """Parameters node (not in the scene) with the inputs and values
        (name: value, e.g. SpatialGridMode=...)."""
        pnode = slicer.vtkMRMLAstroReprojectParametersNode()
        pnode.SetInputVolumeNodeIDs(";".join(node.GetID() for node in inputs))
        pnode.SetInputVolumeNodeID(inputs[0].GetID())
        for key, value in values.items():
            getattr(pnode, "Set" + key)(value)
        return pnode

    def reproject(self, pnode):
        success = self.logic.Reproject(pnode)
        self.assertTrue(success, self.logic.GetLastError())
        output = slicer.mrmlScene.GetNodeByID(pnode.GetOutputVolumeNodeID())
        self.assertIsNotNone(output)
        return output

    def compareWithModel(self, node, model, tolerance, spectralModel=None, step=3, margin=4):
        """Max |voxel - model| over the non-blank voxels (sampled every step
        pixels, margin pixels from the edges) below tolerance; returns the
        number of compared voxels."""
        voxels = voxelArray(node)
        nz, ny, nx = voxels.shape
        compared = 0
        worst = 0.
        for k in range(nz):
            spectral = spectralModel(self.world(node, (0, 0, k))[2]) if spectralModel else 1.
            for j in range(margin, ny - margin, step):
                for i in range(margin, nx - margin, step):
                    value = float(voxels[k, j, i])
                    if math.isnan(value):
                        continue
                    world = self.world(node, (i, j, k))
                    worst = max(worst, abs(value - spectral * model(world[0], world[1])))
                    compared += 1
        self.assertGreater(compared, 0)
        self.assertLess(worst, tolerance, f"{node.GetName()}: max error {worst}")
        return compared

    def pixelArea(self, node):
        """Pixel solid angle at the reference pixel (deg^2)."""
        return abs(float(node.GetAttribute("SlicerAstro.CDELT1")) * float(node.GetAttribute("SlicerAstro.CDELT2")))

    def lineCentre(self, node, i, j):
        """Display spectral value (m/s) of the intensity-weighted line centre at pixel (i, j)."""
        spectrum = np.nan_to_num(voxelArray(node)[:, j, i].astype(float))
        values = np.array([self.world(node, (i, j, k))[2] for k in range(len(spectrum))])
        return float(np.sum(spectrum * values) / np.sum(spectrum))

    #
    # tests
    #

    def test_Identity(self):
        """Reprojection on the own grid: the voxels are unchanged (every method)."""
        self.delayDisplay("Identity")
        model = gaussianSource(180.01, 30.005, 0.02)
        node = self.volume("identity", (1, 40, 48), celestialCards(crpix=(24.5, 20.5)), model)
        P = slicer.vtkMRMLAstroReprojectParametersNode
        for method in (P.NearestNeighbour, P.Bilinear, P.Bicubic, P.Lanczos3, P.AreaAverage, P.ExactOverlap):
            pnode = self.parameters([node], ReferenceVolumeNodeID=node.GetID(),
                                    SpatialGridMode=P.SpatialReference, InterpolationOrder=method)
            output = self.reproject(pnode)
            self.assertEqual(output.GetImageData().GetDimensions(), node.GetImageData().GetDimensions())
            np.testing.assert_allclose(voxelArray(output), voxelArray(node), rtol=0., atol=2.e-6,
                                       err_msg=f"method {method}")
            slicer.mrmlScene.RemoveNode(output)

    def test_RotationAndProjection(self):
        """A source reprojected from SIN (north up) on a rotated TAN grid with
        other pixel sizes: the voxels follow the sky model; the source keeps
        its position."""
        self.delayDisplay("Rotation and projection")
        model = gaussianSource(180.0, 30.0, 0.03)
        node = self.volume("sin", (1, 64, 64), celestialCards(), model)
        reference = self.volume("tan", (1, 50, 56), celestialCards(projection="TAN", crpix=(28.5, 25.5),
                                                                   cdelt=(-0.0025, 0.0025), crota=30.))
        P = slicer.vtkMRMLAstroReprojectParametersNode
        for method, tolerance in ((P.Bilinear, 0.02), (P.Bicubic, 0.006), (P.Lanczos3, 0.006)):
            pnode = self.parameters([node], ReferenceVolumeNodeID=reference.GetID(),
                                    SpatialGridMode=P.SpatialReference, InterpolationOrder=method)
            output = self.reproject(pnode)
            self.assertEqual(output.GetImageData().GetDimensions(), (56, 50, 1))
            self.assertAlmostEqual(float(output.GetAttribute("SlicerAstro.CROTA2")), 30., places=6)
            self.compareWithModel(output, model, tolerance)
            slicer.mrmlScene.RemoveNode(output)

    def test_CelestialFrames(self):
        """Galactic (CAR) data on an equatorial FK5 grid and on an FK4 B1950
        grid: the source positions are converted (vtkAstroFrames)."""
        self.delayDisplay("Celestial frames")
        from vtkAstroCoordinatesPython import vtkAstroFrames
        # a source near (l, b) = (120, 10), in the equatorial frames
        lon0, lat0 = 120.0, 10.0
        equatorial = [0., 0.]
        vtkAstroFrames.ConvertCelestial(lon0, lat0, vtkAstroFrames.Galactic, 0., vtkAstroFrames.FK5, 2000.,
                                        51544.5, equatorial)
        galactic = self.volume("galactic", (1, 64, 64),
                               celestialCards(projection="CAR", lon="GLON", lat="GLAT", crval=(lon0, lat0),
                                              frame=()),
                               gaussianSource(lon0, lat0, 0.03))
        P = slicer.vtkMRMLAstroReprojectParametersNode
        fk5 = self.volume("fk5", (1, 48, 48), celestialCards(crval=tuple(equatorial), crpix=(24.5, 24.5)))
        output = self.reproject(self.parameters([galactic], ReferenceVolumeNodeID=fk5.GetID(),
                                                SpatialGridMode=P.SpatialReference, InterpolationOrder=P.Bicubic))
        self.compareWithModel(output, gaussianSource(equatorial[0], equatorial[1], 0.03), 0.006)

        b1950 = [0., 0.]
        vtkAstroFrames.ConvertCelestial(lon0, lat0, vtkAstroFrames.Galactic, 0., vtkAstroFrames.FK4, 1950.,
                                        33281.923, b1950)
        fk4 = self.volume("fk4", (1, 48, 48),
                          celestialCards(crval=tuple(b1950), crpix=(24.5, 24.5),
                                         frame=(("RADESYS", "FK4"), ("EQUINOX", 1950.))))
        output = self.reproject(self.parameters([galactic], ReferenceVolumeNodeID=fk4.GetID(),
                                                SpatialGridMode=P.SpatialReference, InterpolationOrder=P.Bicubic))
        self.compareWithModel(output, gaussianSource(b1950[0], b1950[1], 0.03), 0.006)

    def test_FluxConservation(self):
        """Jy/pixel data on 2.5 times larger pixels: the total flux is kept by
        the area average and the exact overlap; Jy/beam data keep their
        surface brightness."""
        self.delayDisplay("Flux conservation")
        P = slicer.vtkMRMLAstroReprojectParametersNode
        model = gaussianSource(180.003, 29.998, 0.025)
        node = self.volume("jypix", (1, 64, 64), celestialCards() + [("BUNIT", "JY/PIXEL")], model)
        total = float(np.nansum(voxelArray(node)))
        for method in (P.AreaAverage, P.ExactOverlap):
            output = self.reproject(self.parameters([node], SpatialGridMode=P.SpatialResample,
                                                    SpatialResampleFactor=2.5, InterpolationOrder=method))
            self.assertEqual(output.GetImageData().GetDimensions()[:2], (26, 26))
            self.assertAlmostEqual(float(np.nansum(voxelArray(output))) / total, 1., delta=2.e-3,
                                   msg=f"method {method}")
            slicer.mrmlScene.RemoveNode(output)

        beam = self.volume("jybeam", (1, 64, 64), celestialCards() + [("BUNIT", "JY/BEAM"), ("BMAJ", 0.01),
                                                                      ("BMIN", 0.01), ("BPA", 0.)], model)
        for method in (P.AreaAverage, P.ExactOverlap):
            output = self.reproject(self.parameters([beam], SpatialGridMode=P.SpatialResample,
                                                    SpatialResampleFactor=2.5, InterpolationOrder=method))
            ratio = self.pixelArea(output) / self.pixelArea(beam)
            self.assertAlmostEqual(float(np.nansum(voxelArray(output))) * ratio /
                                   float(np.nansum(voxelArray(beam))), 1., delta=2.e-3)
            slicer.mrmlScene.RemoveNode(output)

    def test_ResampleFactors(self):
        """Smaller pixels (factor 0.5) and wider channels (factor 2): sizes,
        pixel steps and the reference position."""
        self.delayDisplay("Resample factors")
        P = slicer.vtkMRMLAstroReprojectParametersNode
        cards = celestialCards(crpix=(16.5, 12.5)) + [
            ("CTYPE3", "VRAD"), ("CRVAL3", 1000.), ("CDELT3", 5000.), ("CRPIX3", 1.), ("CUNIT3", "m/s"),
            ("RESTFREQ", HI_REST_FREQUENCY), ("SPECSYS", "BARYCENT")]
        node = self.volume("cube", (9, 24, 32), cards, gaussianSource(180., 30., 0.02),
                           lambda v: math.exp(-0.5 * ((v - 21000.) / 8000.) ** 2))
        output = self.reproject(self.parameters([node], SpatialGridMode=P.SpatialResample,
                                                SpatialResampleFactor=0.5, SpectralGridMode=P.SpectralResample,
                                                SpectralResampleFactor=2.))
        self.assertEqual(output.GetImageData().GetDimensions(), (64, 48, 5))
        self.assertAlmostEqual(float(output.GetAttribute("SlicerAstro.CDELT1")), -0.001, places=12)
        self.assertAlmostEqual(float(output.GetAttribute("SlicerAstro.CDELT3")), 10000., places=6)
        # the same sky position at the centre of input pixel (16, 12) and of output pixel (32.5, 24.5)
        inputWorld = self.world(node, (15., 11., 0.))
        outputWorld = self.world(output, (30.5, 22.5, 0.))
        self.assertAlmostEqual(inputWorld[0], outputWorld[0], places=9)
        self.assertAlmostEqual(inputWorld[1], outputWorld[1], places=9)
        # the first wide channel covers input channels 0 and 1
        self.assertAlmostEqual(self.world(output, (0, 0, 0))[2], 3500., places=3)

    def test_SpectralAxes(self):
        """A line in a VRAD cube regridded on FREQ, VOPT and WAVE channels: the
        line centre stays at the same velocity."""
        self.delayDisplay("Spectral axes")
        P = slicer.vtkMRMLAstroReprojectParametersNode
        cards = celestialCards(crpix=(4.5, 4.5)) + [
            ("CTYPE3", "VRAD"), ("CRVAL3", 100000.), ("CDELT3", 2000.), ("CRPIX3", 1.), ("CUNIT3", "m/s"),
            ("RESTFREQ", HI_REST_FREQUENCY), ("SPECSYS", "BARYCENT")]
        v0 = 160000.
        node = self.volume("line", (64, 8, 8), cards, lambda lon, lat: 1.,
                           lambda v: math.exp(-0.5 * ((v - v0) / 10000.) ** 2))
        self.assertAlmostEqual(self.lineCentre(node, 4, 4), v0, delta=50.)
        c = SPEED_OF_LIGHT
        f0 = HI_REST_FREQUENCY
        for spectralType, start, step in (("FREQ", f0 * (1. - 110000. / c), -8.e3),
                                          ("VOPT", 110000., 2100.),
                                          ("WAVE", c / f0 * (1. + 110000. / c), 1.5e-6)):
            pnode = self.parameters([node], SpatialGridMode=P.SpatialInput, SpectralGridMode=P.SpectralManual,
                                    SpectralType=spectralType,
                                    SpectralFrame="BARYCENT", RestFrequency=f0, SpectralStart=start,
                                    SpectralStep=step, SpectralCount=60, SpectralMethod=P.SpectralCubic)
            output = self.reproject(pnode)
            self.assertEqual(output.GetImageData().GetDimensions()[2], 60)
            # display axis: radio velocity for FREQ, optical velocity for VOPT/WAVE
            centre = self.lineCentre(output, 4, 4)
            vrad = centre if spectralType == "FREQ" else c * (1. - 1. / (1. + centre / c))
            self.assertAlmostEqual(vrad, v0, delta=300., msg=spectralType)
            slicer.mrmlScene.RemoveNode(output)

    def test_SpectralFrames(self):
        """Barycentric data on an LSRK axis: the line moves by the LSRK
        velocity of the source direction (vtkAstroFrames)."""
        self.delayDisplay("Spectral frames")
        from vtkAstroCoordinatesPython import vtkAstroFrames
        P = slicer.vtkMRMLAstroReprojectParametersNode
        c = SPEED_OF_LIGHT
        f0 = HI_REST_FREQUENCY
        cards = celestialCards(crval=(270., 30.), crpix=(4.5, 4.5)) + [
            ("CTYPE3", "FREQ"), ("CRVAL3", 1.42e9), ("CDELT3", -1.e4), ("CRPIX3", 1.), ("CUNIT3", "Hz"),
            ("RESTFREQ", f0), ("SPECSYS", "BARYCENT")]
        fLine = 1.4196e9
        node = self.volume("bary", (100, 8, 8), cards, lambda lon, lat: 1.,
                           lambda v: math.exp(-0.5 * ((f0 * (1. - v / c) - fLine) / 4.e4) ** 2))
        pnode = self.parameters([node], SpatialGridMode=P.SpatialInput, SpectralGridMode=P.SpectralInput,
                                SpectralFrame="LSRK")
        self.assertTrue(self.logic.SetManualGridFromInput(pnode, False, True, False), self.logic.GetLastError())
        pnode.SetSpectralGridMode(P.SpectralManual)
        pnode.SetSpectralFrame("LSRK")
        output = self.reproject(pnode)
        self.assertEqual(output.GetAttribute("SlicerAstro.SPECSYS"), "LSRK")
        world = self.world(node, (4, 4, 0))
        import vtk
        lsrk = vtk.reference(0.)
        self.assertTrue(vtkAstroFrames.ConvertFrequency(fLine, vtkAstroFrames.Barycentric, vtkAstroFrames.LSRK,
                                                        world[0], world[1], 51544.5, (0., 0., 0.), lsrk))
        expected = c * (1. - float(lsrk) / f0)
        self.assertAlmostEqual(self.lineCentre(output, 4, 4), expected, delta=100.)
        self.assertGreater(abs(expected - c * (1. - fLine / f0)), 5000.)

    def test_VelocityMatching(self):
        """A CO cube on the channels of an HI cube: matched by velocity (each
        cube with its rest frequency) the lines align; matched by frequency
        the CO data are outside the HI band."""
        self.delayDisplay("Velocity matching")
        P = slicer.vtkMRMLAstroReprojectParametersNode
        c = SPEED_OF_LIGHT
        v0 = 250000.
        line = lambda v: math.exp(-0.5 * ((v - v0) / 15000.) ** 2)

        def cube(name, f0, df):
            cards = celestialCards(crpix=(4.5, 4.5)) + [
                ("CTYPE3", "FREQ"), ("CRVAL3", f0 * (1. - 150000. / c)), ("CDELT3", -df), ("CRPIX3", 1.),
                ("CUNIT3", "Hz"), ("RESTFREQ", f0), ("SPECSYS", "BARYCENT")]
            return self.volume(name, (50, 8, 8), cards, lambda lon, lat: 1., line)

        hi = cube("hi", HI_REST_FREQUENCY, 2.e4)
        co = cube("co", CO_REST_FREQUENCY, 1.5e6)
        output = self.reproject(self.parameters([co], ReferenceVolumeNodeID=hi.GetID(), SpatialGridMode=P.SpatialInput,
                                                SpectralGridMode=P.SpectralReference,
                                                SpectralMatching=P.MatchVelocity))
        self.assertEqual(output.GetImageData().GetDimensions()[2], 50)
        self.assertAlmostEqual(self.lineCentre(output, 4, 4), self.lineCentre(hi, 4, 4), delta=500.)
        slicer.mrmlScene.RemoveNode(output)
        pnode = self.parameters([co], ReferenceVolumeNodeID=hi.GetID(), SpatialGridMode=P.SpatialInput,
                                SpectralGridMode=P.SpectralReference,
                                SpectralMatching=P.MatchFrequency)
        success = self.logic.Reproject(pnode)
        if success:
            output = slicer.mrmlScene.GetNodeByID(pnode.GetOutputVolumeNodeID())
            self.assertTrue(np.all(np.isnan(voxelArray(output))))

    def test_SpectralOverlap(self):
        """Channels 3 times wider (channel overlap): the integrated flux and
        the mean noise level are kept, the noise drops by sqrt(3)."""
        self.delayDisplay("Spectral overlap")
        P = slicer.vtkMRMLAstroReprojectParametersNode
        cards = celestialCards(crpix=(8.5, 8.5)) + [
            ("CTYPE3", "VRAD"), ("CRVAL3", 0.), ("CDELT3", 1000.), ("CRPIX3", 1.), ("CUNIT3", "m/s"),
            ("RESTFREQ", HI_REST_FREQUENCY), ("SPECSYS", "BARYCENT")]
        node = self.volume("noise", (90, 16, 16), cards)
        rng = np.random.default_rng(3)
        voxels = voxelArray(node)
        voxels[:] = rng.normal(1., 1., voxels.shape).astype(np.float32)
        node.GetImageData().Modified()
        output = self.reproject(self.parameters([node], SpatialGridMode=P.SpatialInput, SpectralGridMode=P.SpectralResample,
                                                SpectralResampleFactor=3., SpectralMethod=P.SpectralAuto))
        result = voxelArray(output)
        self.assertEqual(result.shape[0], 30)
        np.testing.assert_allclose(result.sum(axis=0) * 3., voxels.sum(axis=0), rtol=1.e-4)
        self.assertAlmostEqual(float(np.std(result)) * math.sqrt(3.), float(np.std(voxels)), delta=0.05)

    def test_Mosaic(self):
        """Two overlapping pointings of the same sky (primary beam corrected,
        Gaussian primary beams): the optimal mosaic is the sky model and the
        weight is sum(A^2) (Sault et al. 1996)."""
        self.delayDisplay("Mosaic")
        P = slicer.vtkMRMLAstroReprojectParametersNode
        c = SPEED_OF_LIGHT
        frequency = 1.4e9
        model = lambda lon, lat: 0.5 + gaussianSource(180., 30., 0.15)(lon, lat)
        cards = lambda ra: (celestialCards(crval=(ra, 30.), crpix=(32.5, 32.5), cdelt=(-0.01, 0.01)) +
                            [("CTYPE3", "FREQ"), ("CRVAL3", frequency), ("CDELT3", 1.e5), ("CRPIX3", 1.),
                             ("CUNIT3", "Hz"), ("RESTFREQ", HI_REST_FREQUENCY), ("SPECSYS", "BARYCENT"),
                             ("OBSRA", ra), ("OBSDEC", 30.), ("BUNIT", "JY/BEAM")])
        east = self.volume("east", (1, 64, 64), cards(180.2), model)
        west = self.volume("west", (1, 64, 64), cards(179.8), model)
        dish = 25.
        fwhm = math.degrees(1.13 * c / frequency / dish)
        pnode = self.parameters([east, west], SpatialGridMode=P.SpatialUnion, CelestialFrame="FK5",
                                Projection="SIN", PixelSizeX=36., PixelSizeY=36., Combination=P.CombineOptimal,
                                InputCorrection=P.FluxCorrect, PrimaryBeamModel=P.GaussianPrimaryBeam, DishDiameter=dish, PrimaryBeamFactor=1.13,
                                PrimaryBeamCutoff=0.2, InputNoises="0.01;0.02", ComputeWeight=1, ComputeSensitivity=1, SaultImage=1,
                                SaultFraction=0.1)
        output = self.reproject(pnode)
        self.compareWithModel(output, model, 2.e-2, margin=2)
        weight = slicer.mrmlScene.GetNodeByID(pnode.GetWeightVolumeNodeID())
        self.assertIsNotNone(weight)
        self.assertIsNotNone(slicer.mrmlScene.GetNodeByID(pnode.GetSensitivityVolumeNodeID()))
        self.assertIsNotNone(slicer.mrmlScene.GetNodeByID(pnode.GetSaultVolumeNodeID()))
        # the union covers both pointings: wider than one
        self.assertGreater(output.GetImageData().GetDimensions()[0], 70)

        def beam(node, ra, world):
            """Primary beam of a pointing (0 below the cutoff or outside its data)."""
            ijk = [0., 0., 0.]
            node.GetAstroVolumeDisplayNode().GetIJKSpace(world, ijk)
            dims = node.GetImageData().GetDimensions()
            if not (-0.5 <= ijk[0] <= dims[0] - 0.5 and -0.5 <= ijk[1] <= dims[1] - 0.5):
                return 0.
            sigma = fwhm / (2. * math.sqrt(2. * math.log(2.)))
            a = math.exp(-0.5 * (angularDistance(world[0], world[1], ra, 30.) / sigma) ** 2)
            return a if a >= 0.2 else 0.
        weights = voxelArray(weight)
        ny, nx = weights.shape[1:]
        for i, j in ((nx // 2, ny // 2), (nx // 3, ny // 2), (nx // 4, ny // 3), (2 * nx // 3, 2 * ny // 3)):
            world = self.world(weight, (i, j, 0))
            expected = beam(east, 180.2, world) ** 2 + beam(west, 179.8, world) ** 2
            self.assertAlmostEqual(float(weights[0, j, i]), expected, delta=0.01, msg=f"pixel {i} {j}")

    def test_CommonBeam(self):
        """Common beam of a circular and an elongated beam (CASA algorithm):
        every beam deconvolves from it and it is close to the smallest
        enclosing ellipse."""
        self.delayDisplay("Common beam")
        import vtk
        from vtkAstroReprojectPython import vtkAstroBeams
        beams = vtk.vtkDoubleArray()
        beams.SetNumberOfComponents(3)
        for beam in ((3., 3., 0.), (4., 2., 0.), (3.5, 1.5, 60.)):
            beams.InsertNextTuple3(*beam)
        common = [0., 0., 0.]
        self.assertTrue(vtkAstroBeams.CommonBeam(beams, common))
        self.assertGreaterEqual(common[0], 4. - 1.e-9)
        self.assertGreaterEqual(common[1], 3. - 1.e-9)
        self.assertLess(common[0] * common[1], 1.25 * 4. * 3.)
        for index in range(beams.GetNumberOfTuples()):
            kernel = [0., 0., 0.]
            self.assertNotEqual(vtkAstroBeams.Deconvolve(common, beams.GetTuple3(index), kernel),
                                vtkAstroBeams.NotDeconvolvable)
        # equal beams: the beam itself
        beams.Reset()
        beams.InsertNextTuple3(5., 3., 20.)
        beams.InsertNextTuple3(5., 3., 20.)
        self.assertTrue(vtkAstroBeams.CommonBeam(beams, common))
        np.testing.assert_allclose(common, (5., 3., 20.), atol=1.e-6)
        # convolution and deconvolution are inverse
        convolved = [0., 0., 0.]
        vtkAstroBeams.Convolve((5., 3., 20.), (4., 2., -30.), convolved)
        back = [0., 0., 0.]
        self.assertEqual(vtkAstroBeams.Deconvolve(convolved, (4., 2., -30.), back), vtkAstroBeams.Deconvolved)
        np.testing.assert_allclose(back[:2], (5., 3.), atol=1.e-6)
        self.assertAlmostEqual((back[2] - 20. + 90.) % 180. - 90., 0., places=5)

    def test_TargetBeam(self):
        """A point source convolved to a twice larger beam: the peak is kept in
        Jy/beam; Jy/pixel data become Jy/beam with the total flux as peak."""
        self.delayDisplay("Target beam")
        P = slicer.vtkMRMLAstroReprojectParametersNode
        beam = 0.012
        source = gaussianSource(180., 30., beam)
        node = self.volume("point", (1, 64, 64),
                           celestialCards() + [("BUNIT", "JY/BEAM"), ("BMAJ", beam), ("BMIN", beam), ("BPA", 0.)],
                           source)
        output = self.reproject(self.parameters([node], SpatialGridMode=P.SpatialInput, ResolutionMode=P.ResolutionTarget,
                                                TargetBeamMajor=2. * beam * 3600., TargetBeamMinor=2. * beam * 3600.,
                                                TargetBeamPA=0.))
        self.assertAlmostEqual(float(np.nanmax(voxelArray(output))), 1., delta=0.02)
        self.assertAlmostEqual(float(output.GetAttribute("SlicerAstro.BMAJ")), 2. * beam, places=9)

        pixels = self.volume("pixels", (1, 64, 64), celestialCards() + [("BUNIT", "JY/PIXEL")], source)
        total = float(np.nansum(voxelArray(pixels)))
        output = self.reproject(self.parameters([pixels], SpatialGridMode=P.SpatialInput, ResolutionMode=P.ResolutionTarget,
                                                TargetBeamMajor=2. * beam * 3600., TargetBeamMinor=2. * beam * 3600.,
                                                TargetBeamPA=0.))
        self.assertEqual(output.GetAttribute("SlicerAstro.BUNIT").upper(), "JY/BEAM")
        # unit-peak kernel (CASA): the peak of a Gaussian source of FWHM s with
        # a beam of FWHM k is S k^2 / (k^2 + s^2)
        k = 2. * beam
        self.assertAlmostEqual(float(np.nanmax(voxelArray(output))) / total, k * k / (k * k + beam * beam),
                               delta=0.01)

    def test_ManualGrid(self):
        """Manual grid from the input (north up), the description of the
        output grid, and a manual Galactic grid."""
        self.delayDisplay("Manual grid")
        P = slicer.vtkMRMLAstroReprojectParametersNode
        model = gaussianSource(180., 30., 0.03)
        node = self.volume("rotated", (1, 48, 48), celestialCards(crpix=(24.5, 24.5), crota=25.), model)
        pnode = self.parameters([node], SpatialGridMode=P.SpatialManual)
        self.assertTrue(self.logic.SetManualGridFromInput(pnode, True, False, True), self.logic.GetLastError())
        self.assertEqual(pnode.GetRotation(), 0.)
        self.assertAlmostEqual(pnode.GetPixelSizeX(), 7.2, places=5)
        self.assertGreater(pnode.GetSizeX(), 48)
        description = self.logic.DescribeOutputGrid(pnode)
        self.assertTrue(description, self.logic.GetLastError())
        output = self.reproject(pnode)
        self.assertAlmostEqual(float(output.GetAttribute("SlicerAstro.CROTA2") or 0.), 0., places=9)
        self.compareWithModel(output, model, 0.02)
        slicer.mrmlScene.RemoveNode(output)

        from vtkAstroCoordinatesPython import vtkAstroFrames
        galactic = [0., 0.]
        vtkAstroFrames.ConvertCelestial(180., 30., vtkAstroFrames.FK5, 2000., vtkAstroFrames.Galactic, 0.,
                                        51544.5, galactic)
        pnode = self.parameters([node], SpatialGridMode=P.SpatialManual, CelestialFrame="GALACTIC",
                                Projection="CAR", CenterLongitude=galactic[0], CenterLatitude=galactic[1],
                                PixelSizeX=10., PixelSizeY=10., SizeX=40, SizeY=40, Rotation=0.)
        output = self.reproject(pnode)
        self.assertTrue(output.GetAttribute("SlicerAstro.CTYPE1").startswith("GLON"))
        self.compareWithModel(output, gaussianSource(galactic[0], galactic[1], 0.03), 0.02)

    def test_Widget(self):
        """The module widget: its parameters node, the grid summary and Apply."""
        self.delayDisplay("Widget")
        model = gaussianSource(180., 30., 0.03)
        node = self.volume("widget", (1, 32, 32), celestialCards(crpix=(16.5, 16.5)), model)
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActiveVolumeID(node.GetID())
        slicer.util.mainWindow().moduleSelector().selectModule("AstroReproject")
        widget = slicer.modules.astroreproject.widgetRepresentation()
        pnode = widget.mrmlAstroReprojectParametersNode()
        self.assertIsNotNone(pnode)
        P = slicer.vtkMRMLAstroReprojectParametersNode
        pnode.SetInputVolumeNodeIDs(node.GetID())
        pnode.SetSpatialGridMode(P.SpatialResample)
        pnode.SetSpatialResampleFactor(2.)
        slicer.app.processEvents()
        summary = slicer.util.findChild(widget, "GridSummaryLabel")
        self.assertIn("16", summary.text)
        slicer.util.findChild(widget, "ApplyButton").click()
        slicer.app.processEvents()
        output = slicer.mrmlScene.GetNodeByID(pnode.GetOutputVolumeNodeID())
        self.assertIsNotNone(output)
        self.assertEqual(output.GetImageData().GetDimensions(), (16, 16, 1))
        self.delayDisplay("Test passed")
