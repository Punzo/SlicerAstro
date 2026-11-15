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


class AstroLabelMapTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Label Map Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
FITS files loaded as label maps (astronomical label map volumes): world
coordinates, readouts, velocity definitions, display in the slice views and
saving to FITS.
"""
        self.parent.hidden = True


class AstroLabelMapTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroLabelMapTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of a volume node."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def fitsCard(key, value=None):
    if isinstance(value, bool):
        text = ("T" if value else "F").rjust(20)
    elif isinstance(value, (int, np.integer)):
        text = str(int(value)).rjust(20)
    elif isinstance(value, (float, np.floating)):
        text = repr(float(value)).upper().rjust(20)
    else:
        text = ("'" + str(value).replace("'", "''").ljust(8) + "'").ljust(20)
    return f"{key:8s}= {text}"[:80].ljust(80)


def writeFITS(path, data, cards, bitpix):
    header = [("SIMPLE", True), ("BITPIX", bitpix), ("NAXIS", data.ndim)]
    header += [(f"NAXIS{axis}", size) for axis, size in enumerate(reversed(data.shape), start=1)]
    text = "".join(fitsCard(*card) for card in header + list(cards)) + "END".ljust(80)
    text += " " * (-len(text) % 2880)
    dtype = {16: ">i2", 32: ">i4", -32: ">f4"}[bitpix]
    payload = np.ascontiguousarray(data, dtype=dtype).tobytes()
    with open(path, "wb") as f:
        f.write(text.encode("ascii") + payload + b"\0" * (-len(payload) % 2880))


WCS_CARDS = [
    ("CTYPE1", "RA---SIN"), ("CRVAL1", 114.21417), ("CDELT1", -0.002), ("CRPIX1", 16.), ("CUNIT1", "deg"),
    ("CTYPE2", "DEC--SIN"), ("CRVAL2", 65.6025), ("CDELT2", 0.002), ("CRPIX2", 12.), ("CUNIT2", "deg"),
    ("CTYPE3", "FREQ"), ("CRVAL3", 1.4198e9), ("CDELT3", -12207.), ("CRPIX3", 6.), ("CUNIT3", "Hz"),
    ("RESTFREQ", 1.420405752E9), ("SPECSYS", "BARYCENT"), ("RADESYS", "FK5"), ("EQUINOX", 2000.)]


class AstroLabelMapTestTest(ScriptedLoadableModuleTest):
    SHAPE = (11, 23, 31)

    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroLabelMapTest")
        if os.path.isdir(self.directory):
            shutil.rmtree(self.directory)
        os.makedirs(self.directory)

    def runTest(self):
        for test in (self.test_LoadAndCoordinates, self.test_VelocityDefinitions,
                     self.test_SliceViews, self.test_SaveAndReload):
            self.setUp()
            test()

    #
    # helpers
    #

    def labels(self):
        labels = np.zeros(self.SHAPE, dtype=np.int16)
        labels[2:6, 5:12, 4:15] = 1
        labels[6:9, 14:20, 18:27] = 7
        return labels

    def loadLabelMap(self, labels=None):
        labels = self.labels() if labels is None else labels
        path = os.path.join(self.directory, "labels.fits")
        writeFITS(path, labels, WCS_CARDS + [("BUNIT", "")], 16)
        node = slicer.util.loadVolume(path, {"labelmap": True})
        self.assertIsNotNone(node)
        self.assertTrue(node.IsA("vtkMRMLAstroLabelMapVolumeNode"), node.GetClassName())
        display = node.GetDisplayNode()
        self.assertTrue(display.IsA("vtkMRMLAstroLabelMapVolumeDisplayNode"), display.GetClassName())
        np.testing.assert_array_equal(voxelArray(node), labels)
        return node, display, labels

    def loadCube(self):
        """Float cube with the same WCS, as a reference for the coordinates."""
        path = os.path.join(self.directory, "cube.fits")
        writeFITS(path, np.zeros(self.SHAPE, dtype=np.float32), WCS_CARDS + [("BUNIT", "JY/BEAM")], -32)
        node = slicer.util.loadVolume(path)
        self.assertTrue(node.IsA("vtkMRMLAstroVolumeNode"))
        return node

    #
    # tests
    #

    def test_LoadAndCoordinates(self):
        node, display, labels = self.loadLabelMap()
        cube = self.loadCube()
        cubeDisplay = cube.GetAstroVolumeDisplayNode()
        self.assertEqual(display.GetSpace(), "WCS")
        for ijk in ((0, 0, 0), (15, 11, 5), (30, 22, 10), (7.5, 3.25, 2.5)):
            world = [0., 0., 0.]
            reference = [0., 0., 0.]
            self.assertTrue(display.GetReferenceSpace(list(map(float, ijk)), world))
            self.assertTrue(cubeDisplay.GetReferenceSpace(list(map(float, ijk)), reference))
            np.testing.assert_allclose(world, reference, rtol=0., atol=1e-9)
            back = [0., 0., 0.]
            self.assertTrue(display.GetIJKSpace(world, back))
            np.testing.assert_allclose(back, ijk, rtol=0., atol=1e-6)

        # readouts: label value and sexagesimal coordinates
        self.assertIn("7", display.GetPixelString([20., 15., 7.]))
        self.assertIn("1", display.GetPixelString([5., 6., 3.]))
        previous, output = [-1., -1., -1.], [0., 0., 0.]
        world = [0., 0., 0.]
        display.GetReferenceSpace([15., 11., 5.], world)
        self.assertRegex(display.GetDisplayStringFromValueX(world[0], previous, output, 3), r"7h\s*36m")
        self.assertRegex(display.GetDisplayStringFromValueY(world[1], previous, output, 3), "65°\\s*36'")
        self.assertIn("km/s", display.GetDisplayStringFromValueZ(world[2], previous, output, 3))

        # a copy of the display node keeps the WCS
        other = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLAstroLabelMapVolumeDisplayNode")
        other.Copy(display)
        world2 = [0., 0., 0.]
        self.assertTrue(other.GetReferenceSpace([15., 11., 5.], world2))
        np.testing.assert_allclose(world2, world, rtol=0., atol=1e-9)

    def test_VelocityDefinitions(self):
        node, display, labels = self.loadLabelMap()
        world = [0., 0., 0.]
        self.assertTrue(display.SetOpticalVelocityDefinition())
        self.assertTrue(display.GetVelocityDefinition().startswith("VOPT"), display.GetVelocityDefinition())
        display.GetReferenceSpace([0., 0., 5.], world)
        optical = world[2]
        self.assertIn("VOPT", display.AddVelocityInfoToDisplayStringZ("1 km/s"))
        self.assertTrue(display.SetRadioVelocityDefinition())
        self.assertTrue(display.GetVelocityDefinition().startswith("VRAD"), display.GetVelocityDefinition())
        display.GetReferenceSpace([0., 0., 5.], world)
        radio = world[2]
        # v_opt = c (f0 - f) / f, v_rad = c (f0 - f) / f0: the optical one is larger for f < f0
        c = 299792458.
        f0, f = 1.420405752e9, 1.4198e9
        # world coordinates in m/s (SI units of the WCS)
        self.assertAlmostEqual(optical, c * (f0 - f) / f, delta=50.)
        self.assertAlmostEqual(radio, c * (f0 - f) / f0, delta=50.)

    def test_SliceViews(self):
        """Label map in the slice views: WCS axes and first tick positions."""
        node, display, labels = self.loadLabelMap()
        layoutManager = slicer.app.layoutManager()
        sliceLogic = layoutManager.sliceWidget("Red").sliceLogic()
        sliceLogic.GetSliceCompositeNode().SetLabelVolumeID(node.GetID())
        sliceLogic.GetSliceCompositeNode().SetBackgroundVolumeID(None)
        sliceLogic.FitSliceToAll()
        layoutManager.sliceWidget("Red").sliceView().forceRender()
        slicer.app.processEvents()
        world = [0., 0., 0.]
        display.GetReferenceSpace([15., 11., 5.], world)
        # first tick: the multiple of the step at or below the start, minus one
        # step (the axes start drawing outside the view)
        step = 0.005
        for axis, first in ((0, display.GetFirstWcsTickAxisX), (1, display.GetFirstWcsTickAxisY)):
            start = world[axis] - 0.01
            tick = first(start, world[axis] + 0.01, step)
            self.assertAlmostEqual(tick / step, round(tick / step), places=6, msg=axis)
            self.assertLessEqual(tick, start - step + 1e-9, axis)
            self.assertGreater(tick, start - 2. * step - 1e-9, axis)

    def test_SaveAndReload(self):
        node, display, labels = self.loadLabelMap()
        path = os.path.join(self.directory, "saved.fits")
        self.assertTrue(slicer.util.saveNode(node, path))
        slicer.mrmlScene.RemoveNode(node)
        reloaded = slicer.util.loadVolume(path, {"labelmap": True})
        self.assertTrue(reloaded.IsA("vtkMRMLAstroLabelMapVolumeNode"))
        np.testing.assert_array_equal(voxelArray(reloaded), labels)
        world, reference = [0., 0., 0.], [0., 0., 0.]
        reloaded.GetDisplayNode().GetReferenceSpace([30., 22., 10.], world)
        self.loadCube().GetAstroVolumeDisplayNode().GetReferenceSpace([30., 22., 10.], reference)
        np.testing.assert_allclose(world, reference, rtol=0., atol=1e-6)
