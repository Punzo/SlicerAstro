"""Sample data of the tests, downloaded once into the cache of Slicer before
the tests (CTest fixture SlicerAstroSampleData): the tests then only read the
cache and run in parallel."""

import unittest

import slicer

# the samples loaded by the tests (AstroSampleData names)
TEST_SAMPLES = ("WEIN069", "WEIN069_MASK", "NGC2403", "NGC2403_DSS")


class AstroTestSampleDataTest(unittest.TestCase):
    def test_DownloadSamples(self):
        import AstroSampleData
        import SampleData

        AstroSampleData.registerSampleData()
        for name in TEST_SAMPLES:
            nodes = SampleData.downloadSamples(name)
            self.assertTrue(nodes, name)
        slicer.mrmlScene.Clear()
