import os

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
)

#
# AstroSampleData
#
# Registers the SlicerAstro sample data sets in the core "Sample Data" module
# (category "Astronomy"). Data are hosted as GitHub release assets and
# verified with their SHA256 checksum.
#

SAMPLE_DATA_CATEGORY = "Astronomy"

SAMPLE_DATA_BASE_URL = "https://github.com/Punzo/SlicerAstro/releases/download/SampleData-v2/"

# (sample name, thumbnail, files); files: (file name, node name, file type,
# SHA256 checksum or None if not hosted yet). Provenance and citations: release
# notes of SampleData-v2.
SAMPLE_DATA_SOURCES = (
    ("WEIN069", "WEIN069.png", (
        ("WEIN069.fits", "WEIN069", "AstroVolumeFile",
         "35c256c0d552f317e20edb38598e93ef10d370f05a16ff9603db6771fe523905"),
    )),
    # SoFiA 2 mask (segmentation) and source catalogue (table and source points) of WEIN069
    ("WEIN069_MASK", "WEIN069_mask.png", (
        ("WEIN069_mask.fits", "WEIN069_mask", "SegmentationFile",
         "c7cd9c00180f78b664a7b9595559e3a2d2a4673507aa7e14f6231a38ea249fe4"),
        ("WEIN069_cat.txt", "WEIN069_catalogue", "SoFiACatalogFile",
         "ae5d212108a702a20476b8084da2172133355cd0c97de1d0a58b15d803a25322"),
    )),
    # HALOGAS-DR1 low-resolution (30 arcsec) VLA HI cube of NGC 2403 and DSS2 red image of the field
    ("NGC2403", "NGC2403.png", (
        ("NGC2403.fits", "NGC2403", "AstroVolumeFile",
         "09959b45590be00dc98dcee07fd8b99d6dbd49de7651f7ad7d4ef543ee8d2005"),
    )),
    ("NGC2403_DSS", "NGC2403_DSS.png", (
        ("NGC2403_DSS.fits", "NGC2403_DSS", "AstroVolumeFile",
         "ea47ce0030f767b4d25427539b944e170e1b302d0d9a0d83108653a6c8fab851"),
    )),
    ("NGC4111", "NGC4111.png", (("NGC4111.fits", "NGC4111", "AstroVolumeFile", None),)),
    ("NGC3379", "NGC3379.png", (("NGC3379.fits", "NGC3379", "AstroVolumeFile", None),)),
)


class AstroSampleData(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Sample Data"
        self.parent.categories = ["Astronomy.Utilities"]
        self.parent.dependencies = ["SampleData"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
This module registers the SlicerAstro sample data sets (radio HI data cubes
and optical images) in the <b>Sample Data</b> module, category "Astronomy".
"""
        self.parent.acknowledgementText = """
This module was developed by Davide Punzo. <br>
This work was supported by ERC grant nr. 291531 and the Slicer Community.
"""
        # Not shown in the module selector: data are accessed from Sample Data
        self.parent.hidden = True

        slicer.app.connect("startupCompleted()", registerSampleData)


def registerSampleData():
    """Add the SlicerAstro data sets to the Sample Data module."""
    import SampleData

    iconsPath = os.path.join(os.path.dirname(__file__), "Resources", "Icons")
    SampleData.SampleDataLogic.registerCustomSampleDataCategory(SAMPLE_DATA_CATEGORY, "Astronomy")
    for sampleName, thumbnail, files in SAMPLE_DATA_SOURCES:
        if any(checksum is None for _, _, _, checksum in files):
            # not hosted yet
            continue
        SampleData.SampleDataLogic.registerCustomSampleDataSource(
            category=SAMPLE_DATA_CATEGORY,
            sampleName=sampleName,
            uris=[SAMPLE_DATA_BASE_URL + fileName for fileName, _, _, _ in files],
            fileNames=[fileName for fileName, _, _, _ in files],
            nodeNames=[nodeName for _, nodeName, _, _ in files],
            checksums=[f"SHA256:{checksum}" for _, _, _, checksum in files],
            thumbnailFileName=os.path.join(iconsPath, thumbnail),
            loadFileTypes=[fileType for _, _, fileType, _ in files],
            # center: as the data cubes, so that masks and cubes overlap
            loadFileProperties={"center": True},
        )


class AstroSampleDataLogic(ScriptedLoadableModuleLogic):
    """Convenience functions kept for scripts written for older SlicerAstro versions."""

    def downloadSample(self, sampleName):
        import SampleData
        # Sources are registered at startupCompleted, which may not have been
        # emitted yet (e.g. when running tests): registration is idempotent.
        registerSampleData()
        return SampleData.downloadSample(sampleName)

    def downloadWEIN069(self):
        return self.downloadSample("WEIN069")


class AstroSampleDataTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear()

    def runTest(self):
        self.setUp()
        self.test_SourcesRegistered()
        self.setUp()
        self.test_WEIN069Mask()

    def test_SourcesRegistered(self):
        self.delayDisplay("Checking the registration of the Astronomy sample data")
        registerSampleData()
        sources = slicer.modules.sampleDataSources.get(SAMPLE_DATA_CATEGORY, [])
        names = [source.sampleName for source in sources]
        self.assertIn("WEIN069", names)
        # Registering twice must not duplicate the sources
        registerSampleData()
        sources = slicer.modules.sampleDataSources.get(SAMPLE_DATA_CATEGORY, [])
        self.assertEqual(len([s for s in sources if s.sampleName == "WEIN069"]), 1)
        self.assertIn("WEIN069_MASK", names)
        self.delayDisplay("Test passed")

    def test_WEIN069Mask(self):
        """The SoFiA mask of WEIN069 is a segmentation, with the sources of the catalogue."""
        registerSampleData()
        import SampleData

        SampleData.downloadSample("WEIN069_MASK")
        maskNode = slicer.util.getFirstNodeByClassByName("vtkMRMLSegmentationNode", "WEIN069_mask")
        self.assertIsNotNone(maskNode)
        segmentation = maskNode.GetSegmentation()
        self.assertEqual(segmentation.GetNumberOfSegments(), 3)
        self.assertEqual(segmentation.GetSegment("Source_1").GetName(), "SoFiA J050439.39+431804.7")
        points = [node for node in slicer.util.getNodesByClass("vtkMRMLMarkupsFiducialNode")
                  if node.GetAttribute("SlicerAstro.SoFiACatalog")]
        self.assertEqual(len(points), 1)
        self.assertEqual(points[0].GetNumberOfControlPoints(), 3)
        self.delayDisplay("Test passed")
