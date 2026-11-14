import glob
import gzip
import os
import shutil
import stat
import tempfile

import numpy as np

import slicer
import vtk
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)

SPEED_OF_LIGHT = 299792458.0
HI_REST_FREQUENCY = 1.42040575177e9

# BITPIX -> big-endian numpy type of the FITS data
FITS_TYPES = {8: ">u1", 16: ">i2", 32: ">i4", 64: ">i8", -32: ">f4", -64: ">f8"}


class AstroFITSIOTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro FITS IO Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Reading and writing of FITS files: synthetic files (written by the test) with
known voxel values and world coordinates: data types and scaling, blank voxels,
projections, celestial frames, spectral axes and header dialects, dimensions,
extensions, compression (gzip, tile compression) and save/load round trips.
"""
        self.parent.hidden = True


class AstroFITSIOTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroFITSIOTestLogic(ScriptedLoadableModuleLogic):
    pass


#
# FITS writing (no dependencies)
#


def fitsCard(key, value=None, comment=None):
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
    card = f"{key:8s}= {text}"
    if comment:
        card += " / " + comment
    return card[:80].ljust(80)


def fitsHeader(cards):
    text = "".join(fitsCard(*card) if isinstance(card, tuple) else fitsCard(card) for card in cards)
    text += "END".ljust(80)
    text += " " * (-len(text) % 2880)
    return text.encode("ascii")


def fitsData(data, bitpix):
    payload = np.ascontiguousarray(data, dtype=FITS_TYPES[bitpix]).tobytes()
    return payload + b"\0" * (-len(payload) % 2880)


def imageCards(data, bitpix, wcs, primary=True):
    """Structural cards of an image HDU, then the WCS (list of (key, value))."""
    cards = [("SIMPLE", True)] if primary else [("XTENSION", "IMAGE")]
    cards += [("BITPIX", bitpix), ("NAXIS", data.ndim)]
    cards += [(f"NAXIS{axis}", size) for axis, size in enumerate(reversed(data.shape), start=1)]
    if not primary:
        cards += [("PCOUNT", 0), ("GCOUNT", 1)]
    return cards + list(wcs)


def writeFITS(path, data, bitpix, wcs, extension=False):
    """Single image HDU, or (extension) an empty primary HDU and an image extension."""
    if extension:
        content = fitsHeader([("SIMPLE", True), ("BITPIX", 8), ("NAXIS", 0), ("EXTEND", True)])
        content += fitsHeader(imageCards(data, bitpix, wcs, primary=False)) + fitsData(data, bitpix)
    else:
        content = fitsHeader(imageCards(data, bitpix, wcs)) + fitsData(data, bitpix)
    opener = gzip.open if path.endswith(".gz") else open
    with opener(path, "wb") as f:
        f.write(content)


def writeTileCompressedFITS(path, data, wcs):
    """Tile-compressed (.fz) float image: one tile, GZIP_1, no quantization (lossless)."""
    tile = gzip.compress(np.ascontiguousarray(data, dtype=">f4").tobytes())
    cards = [("XTENSION", "BINTABLE"), ("BITPIX", 8), ("NAXIS", 2), ("NAXIS1", 8), ("NAXIS2", 1),
             ("PCOUNT", len(tile)), ("GCOUNT", 1), ("TFIELDS", 1),
             ("TTYPE1", "COMPRESSED_DATA"), ("TFORM1", f"1PB({len(tile)})"),
             ("ZIMAGE", True), ("ZBITPIX", -32), ("ZNAXIS", data.ndim)]
    cards += [(f"ZNAXIS{axis}", size) for axis, size in enumerate(reversed(data.shape), start=1)]
    cards += [(f"ZTILE{axis}", size) for axis, size in enumerate(reversed(data.shape), start=1)]
    cards += [("ZCMPTYPE", "GZIP_1"), ("ZQUANTIZ", "NONE")] + list(wcs)
    table = np.array([len(tile), 0], dtype=">i4").tobytes() + tile
    table += b"\0" * (-len(table) % 2880)
    content = fitsHeader([("SIMPLE", True), ("BITPIX", 8), ("NAXIS", 0), ("EXTEND", True)])
    content += fitsHeader(cards) + table
    with open(path, "wb") as f:
        f.write(content)


def celestialWCS(projection="SIN", lon="RA--", lat="DEC-", crval=(180.0, 30.0), crpix=(3.0, 3.0),
                 cdelt=(-0.001, 0.001), frame=(("RADESYS", "FK5"), ("EQUINOX", 2000.0))):
    return [(f"CTYPE1", f"{lon}-{projection}"), ("CRVAL1", crval[0]), ("CDELT1", cdelt[0]),
            ("CRPIX1", crpix[0]), ("CUNIT1", "deg"),
            (f"CTYPE2", f"{lat}-{projection}"), ("CRVAL2", crval[1]), ("CDELT2", cdelt[1]),
            ("CRPIX2", crpix[1]), ("CUNIT2", "deg")] + list(frame)


def radioVelocityWCS(crval=1000.0, cdelt=5000.0, crpix=1.0):
    return [("CTYPE3", "VRAD"), ("CRVAL3", crval), ("CDELT3", cdelt), ("CRPIX3", crpix), ("CUNIT3", "m/s"),
            ("RESTFREQ", HI_REST_FREQUENCY), ("SPECSYS", "BARYCENT")]


def beamCards(bunit="JY/BEAM"):
    return [("BUNIT", bunit), ("BMAJ", 0.004), ("BMIN", 0.003), ("BPA", 30.0)]


def cubeData(shape=(4, 5, 6), dtype=np.float32):
    """Distinct values (no two voxels equal)."""
    return (np.arange(np.prod(shape)).reshape(shape) * 0.25 - 3.0).astype(dtype)


def voxelArray(volumeNode):
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


class AstroFITSIOTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroFITSIOTest")
        if os.path.isdir(self.directory):
            for path in glob.glob(os.path.join(self.directory, "**"), recursive=True):
                os.chmod(path, stat.S_IRWXU)
            shutil.rmtree(self.directory)
        os.makedirs(self.directory)

    def runTest(self):
        for test in (self.test_DataTypes, self.test_WesterborkUnits, self.test_Projections,
                     self.test_CelestialFrames, self.test_SpectralAxes, self.test_Dimensions,
                     self.test_MissingWCS, self.test_Extension, self.test_Compression, self.test_RoundTrip,
                     self.test_Memory, self.test_LargeCube):
            self.setUp()
            test()

    #
    # helpers
    #

    def load(self, path):
        """The astronomical volume read from path, or None."""
        try:
            node = slicer.util.loadVolume(path)
        except RuntimeError:
            return None
        return node

    def assertVoxels(self, node, expected, rtol=1.e-6):
        voxels = voxelArray(node)
        self.assertEqual(voxels.shape, expected.shape)
        nanExpected = np.isnan(expected)
        self.assertTrue(np.array_equal(np.isnan(voxels), nanExpected))
        np.testing.assert_allclose(voxels[~nanExpected], expected[~nanExpected], rtol=rtol, atol=1.e-12)

    def world(self, node, ijk):
        world = [0.0, 0.0, 0.0]
        self.assertTrue(node.GetAstroVolumeDisplayNode().GetReferenceSpace([float(v) for v in ijk], world))
        return world

    def assertWorld(self, node, ijk, expected, places=(9, 9, 6)):
        world = self.world(node, ijk)
        for axis in range(len(expected)):
            self.assertAlmostEqual(world[axis], expected[axis], places=places[axis],
                                   msg=f"{node.GetName()}: axis {axis} at {ijk}: {world} != {expected}")

    def assertRoundTripWCS(self, node):
        """Pixel -> world -> pixel at the corners."""
        dims = node.GetImageData().GetDimensions()
        displayNode = node.GetAstroVolumeDisplayNode()
        for i in (0, dims[0] - 1):
            for j in (0, dims[1] - 1):
                for k in {0, dims[2] - 1}:
                    world = self.world(node, (i, j, k))
                    ijk = [0.0, 0.0, 0.0]
                    self.assertTrue(displayNode.GetIJKSpace(world, ijk))
                    for axis, value in enumerate((i, j, k)):
                        self.assertAlmostEqual(ijk[axis], value, places=6)

    #
    # tests
    #

    def test_DataTypes(self):
        """BITPIX 8, 16, 32, 64, -32, -64 with BSCALE/BZERO and blank voxels (BLANK, NaN)."""
        self.delayDisplay("FITS data types")
        shape = (4, 5, 6)
        cases = {8: (1.0, 0.0, None), 16: (2.0, 10.0, -32768), 32: (0.5, -3.0, -2147483648),
                 64: (1.0, 0.0, None), -32: (1.0, 0.0, None), -64: (1.0, 0.0, None)}
        for bitpix, (bscale, bzero, blank) in cases.items():
            raw = (np.arange(np.prod(shape)).reshape(shape) % 200).astype(np.float64)
            if bitpix < 0:
                raw = raw * 0.37 - 5.0
                raw[0, 0, 1] = np.nan
            elif blank is not None:
                raw[0, 0, 1] = blank
            cards = celestialWCS() + radioVelocityWCS() + beamCards() + [("BSCALE", bscale), ("BZERO", bzero)]
            if blank is not None:
                cards.append(("BLANK", blank))
            path = os.path.join(self.directory, f"bitpix{bitpix:+d}.fits")
            writeFITS(path, raw, bitpix, cards)
            node = self.load(path)
            self.assertIsNotNone(node, f"BITPIX {bitpix}")
            self.assertTrue(node.IsA("vtkMRMLAstroVolumeNode"))
            expectedType = vtk.VTK_DOUBLE if bitpix in (64, -64) else vtk.VTK_FLOAT
            self.assertEqual(node.GetImageData().GetScalarType(), expectedType, f"BITPIX {bitpix}")
            expected = raw * bscale + bzero
            if blank is not None:
                expected[raw == blank] = np.nan
            self.assertVoxels(node, expected)
        self.delayDisplay("Test passed")

    def test_WesterborkUnits(self):
        """W.U. (Westerbork units) are converted to Jy/beam (5 mJy/beam)."""
        self.delayDisplay("Westerbork units")
        data = cubeData()
        path = os.path.join(self.directory, "wu.fits")
        writeFITS(path, data, -32, celestialWCS() + radioVelocityWCS() + beamCards("W.U."))
        node = self.load(path)
        self.assertIsNotNone(node)
        self.assertEqual(node.GetAttribute("SlicerAstro.BUNIT"), "JY/BEAM")
        self.assertVoxels(node, data.astype(np.float64) * 0.005)
        self.delayDisplay("Test passed")

    def test_Projections(self):
        """Celestial projections: the reference pixel is at the reference coordinates;
        pixel -> world -> pixel is the identity; CAR at the equator is linear."""
        self.delayDisplay("Celestial projections")
        data = cubeData()
        for projection in ("SIN", "TAN", "CAR", "SFL", "ZEA", "AIT", "ARC", "STG", "NCP"):
            crval = (180.0, 0.0) if projection in ("CAR", "SFL", "AIT") else (180.0, 30.0)
            path = os.path.join(self.directory, f"projection_{projection}.fits")
            writeFITS(path, data, -32, celestialWCS(projection, crval=crval) + radioVelocityWCS() + beamCards())
            node = self.load(path)
            self.assertIsNotNone(node, projection)
            self.assertEqual(node.GetAstroVolumeDisplayNode().GetWCSStatus(), 0, projection)
            self.assertWorld(node, (2, 2, 0), (crval[0], crval[1], 1000.0))
            self.assertRoundTripWCS(node)
            if projection == "CAR":
                # plate carree with the reference point at the equator: linear
                for i, j in ((0, 0), (5, 4), (1, 3)):
                    self.assertWorld(node, (i, j, 0), (180.0 - 0.001 * (i - 2), 0.001 * (j - 2), 1000.0))
        self.delayDisplay("Test passed")

    def test_CelestialFrames(self):
        """FK5, ICRS, FK4 (B1950) and Galactic coordinates."""
        self.delayDisplay("Celestial frames")
        data = cubeData()
        frames = {
            "fk5": ("RA--", "DEC-", (("RADESYS", "FK5"), ("EQUINOX", 2000.0))),
            "icrs": ("RA--", "DEC-", (("RADESYS", "ICRS"),)),
            "fk4": ("RA--", "DEC-", (("RADESYS", "FK4"), ("EQUINOX", 1950.0))),
            "galactic": ("GLON", "GLAT", ()),
        }
        for name, (lon, lat, frame) in frames.items():
            path = os.path.join(self.directory, f"frame_{name}.fits")
            writeFITS(path, data, -32, celestialWCS(lon=lon, lat=lat, frame=frame) + radioVelocityWCS() + beamCards())
            node = self.load(path)
            self.assertIsNotNone(node, name)
            self.assertEqual(node.GetAstroVolumeDisplayNode().GetWCSStatus(), 0, name)
            self.assertWorld(node, (2, 2, 0), (180.0, 30.0, 1000.0))
            self.assertRoundTripWCS(node)
            self.assertVoxels(node, data)
        self.delayDisplay("Test passed")

    def test_SpectralAxes(self):
        """Spectral axes and conventions: velocities (radio, optical, AIPS),
        frequency (RESTFREQ or RESTFRQ), wavelength. SlicerAstro shows frequency
        axes as radio velocities and wavelength axes as optical velocities."""
        self.delayDisplay("Spectral axes")
        data = cubeData()
        c = SPEED_OF_LIGHT
        f0 = HI_REST_FREQUENCY
        lambda0 = 0.21106114
        channels = range(data.shape[0])
        cases = {
            # name: (spectral cards, velocity definition, expected velocity of channel k [m/s])
            "vrad": (radioVelocityWCS(), "VRAD", lambda k: 1000.0 + 5000.0 * k),
            "vopt": ([("CTYPE3", "VOPT"), ("CRVAL3", 2000.0), ("CDELT3", -4000.0), ("CRPIX3", 1.0),
                      ("CUNIT3", "m/s"), ("RESTFREQ", f0)], "VOPT", lambda k: 2000.0 - 4000.0 * k),
            "freq_restfreq": ([("CTYPE3", "FREQ"), ("CRVAL3", 1.42e9), ("CDELT3", -2.0e5), ("CRPIX3", 1.0),
                               ("CUNIT3", "Hz"), ("RESTFREQ", f0), ("SPECSYS", "BARYCENT")], "VRAD",
                              lambda k: c * (1.0 - (1.42e9 - 2.0e5 * k) / f0)),
            "freq_restfrq": ([("CTYPE3", "FREQ"), ("CRVAL3", 1.42e9), ("CDELT3", -2.0e5), ("CRPIX3", 1.0),
                              ("CUNIT3", "Hz"), ("RESTFRQ", f0), ("SPECSYS", "LSRK")], "VRAD",
                             lambda k: c * (1.0 - (1.42e9 - 2.0e5 * k) / f0)),
            "aips_velo": ([("CTYPE3", "VELO-HEL"), ("CRVAL3", 3000.0), ("CDELT3", 2000.0), ("CRPIX3", 1.0),
                           ("RESTFREQ", f0)], "VOPT", lambda k: 3000.0 + 2000.0 * k),
            "wave": ([("CTYPE3", "WAVE"), ("CRVAL3", 0.2112), ("CDELT3", 1.0e-5), ("CRPIX3", 1.0),
                      ("CUNIT3", "m"), ("RESTWAV", lambda0)], "VOPT",
                     lambda k: c * ((0.2112 + 1.0e-5 * k) / lambda0 - 1.0)),
            # pixel steps only in a CD matrix (e.g. the MUSE cubes: CD3_3, no CDELT3)
            "wave_cd": ([("CTYPE3", "WAVE"), ("CRVAL3", 0.2112), ("CD3_3", 1.0e-5), ("CRPIX3", 1.0),
                         ("CUNIT3", "m"), ("RESTWAV", lambda0)], "VOPT",
                        lambda k: c * ((0.2112 + 1.0e-5 * k) / lambda0 - 1.0)),
        }
        for name, (spectral, definition, velocity) in cases.items():
            path = os.path.join(self.directory, f"spectral_{name}.fits")
            celestial = celestialWCS()
            if name.endswith("_cd"):
                celestial = [("CD1_1", value) if key == "CDELT1" else ("CD2_2", value) if key == "CDELT2"
                             else (key, value) for key, value in celestial]
            writeFITS(path, data, -32, celestial + spectral + beamCards())
            node = self.load(path)
            self.assertIsNotNone(node, name)
            displayNode = node.GetAstroVolumeDisplayNode()
            self.assertEqual(displayNode.GetWCSStatus(), 0, name)
            self.assertEqual(displayNode.GetVelocityDefinition()[:4], definition, name)
            for k in channels:
                self.assertAlmostEqual(self.world(node, (2, 2, k))[2], velocity(k), delta=1.e-3,
                                       msg=f"{name}: channel {k}")
            # radio <-> optical velocities: v_opt = c v_rad / (c - v_rad)
            radio = [self.world(node, (2, 2, k))[2] for k in channels] if definition == "VRAD" else None
            if radio:
                self.assertTrue(displayNode.SetOpticalVelocityDefinition())
                for k in channels:
                    expected = c * radio[k] / (c - radio[k])
                    self.assertAlmostEqual(self.world(node, (2, 2, k))[2], expected, delta=1.e-3,
                                           msg=f"{name}: optical velocity of channel {k}")
                self.assertTrue(displayNode.SetRadioVelocityDefinition())
                for k in channels:
                    self.assertAlmostEqual(self.world(node, (2, 2, k))[2], radio[k], delta=1.e-3)
        self.delayDisplay("Test passed")

    def test_Dimensions(self):
        """2D images, 4D cubes with a degenerate Stokes axis; polarization cubes
        (NAXIS4 > 1) are rejected."""
        self.delayDisplay("Dimensions")
        image = cubeData((5, 6))
        path = os.path.join(self.directory, "image2d.fits")
        writeFITS(path, image, -32, celestialWCS() + beamCards())
        node = self.load(path)
        self.assertIsNotNone(node)
        self.assertEqual(node.GetImageData().GetDimensions(), (6, 5, 1))
        self.assertVoxels(node, image[np.newaxis])
        self.assertWorld(node, (2, 2, 0), (180.0, 30.0))

        stokes = [("CTYPE4", "STOKES"), ("CRVAL4", 1.0), ("CDELT4", 1.0), ("CRPIX4", 1.0)]
        cube = cubeData()
        path = os.path.join(self.directory, "cube4d.fits")
        writeFITS(path, cube[np.newaxis], -32, celestialWCS() + radioVelocityWCS() + stokes + beamCards())
        node = self.load(path)
        self.assertIsNotNone(node)
        self.assertEqual(node.GetImageData().GetDimensions(), (6, 5, 4))
        self.assertVoxels(node, cube)
        self.assertWorld(node, (2, 2, 1), (180.0, 30.0, 6000.0))

        path = os.path.join(self.directory, "polarization.fits")
        writeFITS(path, np.stack([cube, cube]), -32, celestialWCS() + radioVelocityWCS() + stokes + beamCards())
        numberOfVolumes = slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLAstroVolumeNode")
        node = self.load(path)
        self.assertIsNone(node)
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLAstroVolumeNode"), numberOfVolumes)
        self.delayDisplay("Test passed")

    def test_MissingWCS(self):
        """Without world coordinates the data are still loaded."""
        self.delayDisplay("Missing WCS")
        data = cubeData()
        path = os.path.join(self.directory, "nowcs.fits")
        writeFITS(path, data, -32, [("BUNIT", "JY/BEAM")])
        node = self.load(path)
        self.assertIsNotNone(node)
        self.assertVoxels(node, data)
        self.delayDisplay("Test passed")

    def test_Extension(self):
        """Data in an image extension after an empty primary HDU."""
        self.delayDisplay("Image extension")
        data = cubeData()
        path = os.path.join(self.directory, "extension.fits")
        writeFITS(path, data, -32, celestialWCS() + radioVelocityWCS() + beamCards(), extension=True)
        node = self.load(path)
        self.assertIsNotNone(node)
        self.assertVoxels(node, data)
        self.assertWorld(node, (2, 2, 3), (180.0, 30.0, 16000.0))
        self.delayDisplay("Test passed")

    def test_Compression(self):
        """gzip-compressed (.fits.gz, also in a read-only directory) and
        tile-compressed (.fz) files: same voxels and world coordinates as the
        plain file, no file written next to them, no temporary file left."""
        self.delayDisplay("Compressed files")
        data = cubeData()
        data[1, 2, 3] = np.nan
        wcs = celestialWCS() + radioVelocityWCS() + beamCards()
        temporaryFiles = set(glob.glob(os.path.join(tempfile.gettempdir(), "SlicerAstro-*")))

        plainPath = os.path.join(self.directory, "plain.fits")
        writeFITS(plainPath, data, -32, wcs)
        gzipPath = os.path.join(self.directory, "compressed.fits.gz")
        writeFITS(gzipPath, data, -32, wcs)
        tilePath = os.path.join(self.directory, "compressed.fits.fz")
        writeTileCompressedFITS(tilePath, data, wcs)
        readOnlyDirectory = os.path.join(self.directory, "readonly")
        os.makedirs(readOnlyDirectory)
        readOnlyPath = os.path.join(readOnlyDirectory, "readonly.fits.gz")
        writeFITS(readOnlyPath, data, -32, wcs)
        os.chmod(readOnlyDirectory, stat.S_IRUSR | stat.S_IXUSR)
        filesBefore = sorted(os.listdir(self.directory)), sorted(os.listdir(readOnlyDirectory))

        reference = self.load(plainPath)
        self.assertIsNotNone(reference)
        for path in (gzipPath, tilePath, readOnlyPath):
            node = self.load(path)
            self.assertIsNotNone(node, path)
            self.assertVoxels(node, data)
            for ijk in ((0, 0, 0), (5, 4, 3), (2, 2, 1)):
                self.assertEqual(self.world(node, ijk), self.world(reference, ijk))
            for key in ("SlicerAstro.CTYPE3", "SlicerAstro.BMAJ", "SlicerAstro.BUNIT", "SlicerAstro.NAXIS3"):
                self.assertEqual(node.GetAttribute(key), reference.GetAttribute(key), f"{path}: {key}")

        self.assertEqual((sorted(os.listdir(self.directory)), sorted(os.listdir(readOnlyDirectory))), filesBefore)
        self.assertEqual(set(glob.glob(os.path.join(tempfile.gettempdir(), "SlicerAstro-*"))), temporaryFiles)
        os.chmod(readOnlyDirectory, stat.S_IRWXU)
        self.delayDisplay("Test passed")

    def peakMemoryOfLoad(self, path):
        """Loaded node and the peak increase of the resident memory (MB) while
        loading path (Linux: /proc/self/status, the peak reset with
        /proc/self/clear_refs)."""
        import gc

        def status(key):
            with open("/proc/self/status") as f:
                for line in f:
                    if line.startswith(key):
                        return int(line.split()[1]) / 1024.
        gc.collect()
        before = status("VmRSS")
        with open("/proc/self/clear_refs", "w") as f:
            f.write("5")
        node = self.load(path)
        return node, status("VmHWM") - before

    def test_Memory(self):
        """Peak memory of loading a 256 MB cube (plain and gzip compressed):
        at most 1.1 times the voxels (no copy of the cube); no temporary files
        left (Linux only)."""
        if not os.path.exists("/proc/self/clear_refs"):
            self.skipTest("peak memory measured on Linux only")
        self.delayDisplay("Memory")
        shape = (128, 512, 1024)
        data = np.zeros(shape, dtype=np.float32)
        data[:, 200:300, 400:600] = 1.
        data[-1, -1, -1] = 7.
        wcs = celestialWCS(crpix=(512.5, 256.5)) + radioVelocityWCS()
        voxelsMB = data.nbytes / 2. ** 20
        for name in ("memory.fits", "memory.fits.gz"):
            path = os.path.join(self.directory, name)
            writeFITS(path, data, -32, wcs)
            files = sorted(os.listdir(self.directory))
            node, peak = self.peakMemoryOfLoad(path)
            self.assertIsNotNone(node, name)
            voxels = voxelArray(node)
            self.assertEqual(voxels.shape, shape)
            self.assertEqual(float(voxels[-1, -1, -1]), 7.)
            self.assertEqual(float(voxels[0, 250, 500]), 1.)
            self.assertLess(peak, 1.1 * voxelsMB + 32., f"{name}: peak memory {peak:.0f} MB for {voxelsMB:.0f} MB")
            self.assertEqual(sorted(os.listdir(self.directory)), files, f"{name}: temporary files left")
            del voxels, node
            slicer.mrmlScene.Clear(0)
            os.remove(path)

    def test_LargeCube(self):
        """A cube of more than 2^31 voxels (64-bit voxel indices): voxels at
        the start and at the end, peak memory. Opt-in (8.7 GB of memory and
        of disk): SLICERASTRO_LARGE_TESTS=1."""
        if os.environ.get("SLICERASTRO_LARGE_TESTS") != "1" or not os.path.exists("/proc/self/clear_refs"):
            self.skipTest("set SLICERASTRO_LARGE_TESTS=1 (Linux)")
        self.delayDisplay("Cube of more than 2^31 voxels")
        nz, ny, nx = 520, 2048, 2048
        self.assertGreater(nx * ny * nz, 2 ** 31)
        header = fitsHeader([("SIMPLE", True), ("BITPIX", -32), ("NAXIS", 3), ("NAXIS1", nx), ("NAXIS2", ny),
                             ("NAXIS3", nz)] + celestialWCS(crpix=(1024.5, 1024.5)) + radioVelocityWCS())
        path = os.path.join(self.directory, "large.fits")
        voxels = nx * ny * nz
        # sparse file: zeros, except the first and the last voxels
        with open(path, "wb") as f:
            f.write(header)
            f.write(np.array([3.], dtype=">f4").tobytes())
            f.seek(len(header) + 4 * (voxels - 1))
            f.write(np.array([5.], dtype=">f4").tobytes())
            f.truncate(len(header) + 4 * voxels + (-4 * voxels) % 2880)
        try:
            node, peak = self.peakMemoryOfLoad(path)
            self.assertIsNotNone(node)
            self.assertEqual(node.GetImageData().GetDimensions(), (nx, ny, nz))
            values = voxelArray(node)
            self.assertEqual(float(values[0, 0, 0]), 3.)
            self.assertEqual(float(values[-1, -1, -1]), 5.)
            self.assertEqual(float(values[nz // 2, ny // 2, nx // 2]), 0.)
            voxelsMB = 4. * voxels / 2. ** 20
            self.assertLess(peak, 1.1 * voxelsMB + 32., f"peak memory {peak:.0f} MB for {voxelsMB:.0f} MB")
            del values, node
        finally:
            slicer.mrmlScene.Clear(0)
            os.remove(path)

    def test_RoundTrip(self):
        """Saved and loaded again (.fits and .fits.gz, float and double): same voxels and header."""
        self.delayDisplay("Save and load")
        keys = ("SlicerAstro.CTYPE1", "SlicerAstro.CTYPE2", "SlicerAstro.CTYPE3", "SlicerAstro.CRVAL1",
                "SlicerAstro.CRVAL2", "SlicerAstro.CRVAL3", "SlicerAstro.CDELT1", "SlicerAstro.CDELT3",
                "SlicerAstro.CRPIX1", "SlicerAstro.CRPIX3", "SlicerAstro.BUNIT", "SlicerAstro.BMAJ",
                "SlicerAstro.BMIN", "SlicerAstro.BPA", "SlicerAstro.OBJECT")
        for bitpix, dtype in ((-32, np.float32), (-64, np.float64)):
            data = cubeData(dtype=dtype)
            data[0, 1, 2] = np.nan
            path = os.path.join(self.directory, f"original{bitpix}.fits")
            writeFITS(path, data, bitpix,
                      celestialWCS() + radioVelocityWCS() + beamCards() + [("OBJECT", "SYNTHETIC")])
            original = self.load(path)
            self.assertIsNotNone(original)
            self.assertVoxels(original, data, rtol=0.0)
            for fileName in (f"saved{bitpix}.fits", f"saved{bitpix}.fits.gz"):
                savedPath = os.path.join(self.directory, fileName)
                self.assertTrue(slicer.util.saveNode(original, savedPath))
                self.assertTrue(os.path.isfile(savedPath), fileName)
                saved = self.load(savedPath)
                self.assertIsNotNone(saved, fileName)
                self.assertEqual(saved.GetImageData().GetScalarType(), original.GetImageData().GetScalarType())
                self.assertVoxels(saved, data, rtol=0.0)
                for key in keys:
                    originalValue, savedValue = original.GetAttribute(key), saved.GetAttribute(key)
                    try:
                        self.assertAlmostEqual(float(savedValue), float(originalValue), places=9,
                                               msg=f"{fileName}: {key}")
                    except (TypeError, ValueError):
                        self.assertEqual(savedValue, originalValue, f"{fileName}: {key}")
                for ijk in ((0, 0, 0), (5, 4, 3)):
                    for axis, value in enumerate(self.world(saved, ijk)):
                        self.assertAlmostEqual(value, self.world(original, ijk)[axis], places=9)
        self.delayDisplay("Test passed")
