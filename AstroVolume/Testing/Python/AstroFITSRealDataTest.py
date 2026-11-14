import glob
import json
import math
import os

import numpy as np

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroFITSRealDataTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro FITS Real Data Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
FITS files of real instruments and archives (radio, millimetre, optical,
infrared, X-ray; release TestingData-v1 and the sample data SampleData-v2)
read by SlicerAstro: dimensions, voxel values and world coordinates against
the values of astropy (RealData/*.json, written by
RealData/prepare_real_data.py).
"""
        self.parent.hidden = True


class AstroFITSRealDataTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroFITSRealDataTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def expectedValues():
    """Expected values of the test files (RealData/*.json next to this file,
    or in the directory of the scripted modules once installed)."""
    directories = [os.path.join(os.path.dirname(os.path.abspath(__file__)), "RealData")]
    files = []
    for directory in directories:
        files += sorted(glob.glob(os.path.join(directory, "*.json")))
    return [json.load(open(path)) for path in files]


class AstroFITSRealDataTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_RealData()

    def localPath(self, expected):
        """The file: from SLICERASTRO_TESTING_DATA_DIR if set (e.g. before a
        release), downloaded into the cache otherwise (checksum verified)."""
        directory = os.environ.get("SLICERASTRO_TESTING_DATA_DIR")
        if directory and os.path.exists(os.path.join(directory, expected["file"])):
            return os.path.join(directory, expected["file"])
        import SampleData
        return SampleData.SampleDataLogic().downloadFileIntoCache(
            expected["url"], expected["file"], checksum="SHA256:" + expected["sha256"])

    def test_RealData(self):
        cases = expectedValues()
        self.assertGreaterEqual(len(cases), 10, "expected values (RealData/*.json) not found")
        for expected in cases:
            self.setUp()
            self.checkFile(expected)
        self.delayDisplay("Test passed")

    def checkFile(self, expected):
        name = expected["file"]
        self.delayDisplay(f"{name}: {expected['description']}")
        node = slicer.util.loadVolume(self.localPath(expected))
        self.assertIsNotNone(node, name)
        self.assertTrue(node.IsA("vtkMRMLAstroVolumeNode"), name)
        self.assertEqual(list(node.GetImageData().GetDimensions()), expected["dimensions"], name)

        # voxels (integer data are read as floating point values)
        voxels = voxelArray(node)
        for i, j, k, value in expected["voxels"]:
            actual = float(voxels[k, j, i])
            if value is None:
                self.assertTrue(math.isnan(actual), f"{name} ({i}, {j}, {k}): {actual} is not blank")
            else:
                self.assertAlmostEqual(actual, value, delta=1.e-6 * max(1., abs(value)),
                                       msg=f"{name} voxel ({i}, {j}, {k})")

        # world coordinates (SlicerAstro: spectral axis as a velocity, m/s)
        display = node.GetAstroVolumeDisplayNode()
        for point in expected["world"]:
            i, j, k = point[:3]
            reference = point[3:]
            world = [0., 0., 0.]
            self.assertTrue(display.GetReferenceSpace([float(i), float(j), float(k)], world), name)
            dLon = (world[0] - reference[0] + 180.) % 360. - 180.
            self.assertLess(abs(dLon) * math.cos(math.radians(reference[1])) * 3600., 1.e-3,
                            f"{name} ({i}, {j}, {k}): longitude {world[0]} instead of {reference[0]}")
            self.assertLess(abs(world[1] - reference[1]) * 3600., 1.e-3,
                            f"{name} ({i}, {j}, {k}): latitude {world[1]} instead of {reference[1]}")
            if len(reference) > 2:
                self.assertAlmostEqual(world[2], reference[2], delta=1.e-3,
                                       msg=f"{name} ({i}, {j}, {k}): velocity (m/s)")
        slicer.mrmlScene.Clear(0)
