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

# (sample name, file name, node name, SHA256 checksum or None if not hosted yet,
#  labelmap, thumbnail)
SAMPLE_DATA_SOURCES = (
    ("WEIN069", "WEIN069.fits", "WEIN069",
     "35c256c0d552f317e20edb38598e93ef10d370f05a16ff9603db6771fe523905", False, "WEIN069.png"),
    ("WEIN069_MASK", "WEIN069_mask.fits", "WEIN069_mask", None, True, "WEIN069_mask.png"),
    ("NGC2403", "NGC2403.fits", "NGC2403", None, False, "NGC2403.png"),
    ("NGC2403_DSS", "NGC2403_DSS.fits", "NGC2403_DSS", None, False, "NGC2403_DSS.png"),
    ("NGC4111", "NGC4111.fits", "NGC4111", None, False, "NGC4111.png"),
    ("NGC3379", "NGC3379.fits", "NGC3379", None, False, "NGC3379.png"),
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
    for sampleName, fileName, nodeName, checksum, labelmap, thumbnail in SAMPLE_DATA_SOURCES:
        if checksum is None:
            # not hosted yet
            continue
        loadFileProperties = {"center": True}
        if labelmap:
            loadFileProperties["labelmap"] = True
        SampleData.SampleDataLogic.registerCustomSampleDataSource(
            category=SAMPLE_DATA_CATEGORY,
            sampleName=sampleName,
            uris=SAMPLE_DATA_BASE_URL + fileName,
            fileNames=fileName,
            nodeNames=nodeName,
            checksums=f"SHA256:{checksum}",
            thumbnailFileName=os.path.join(iconsPath, thumbnail),
            loadFileTypes="AstroVolumeFile",
            loadFileProperties=loadFileProperties,
        )


class AstroSampleDataLogic(ScriptedLoadableModuleLogic):
    """Convenience functions kept for scripts written for older SlicerAstro versions."""

    def downloadSample(self, sampleName):
        import SampleData
        return SampleData.downloadSample(sampleName)

    def downloadWEIN069(self):
        return self.downloadSample("WEIN069")


class AstroSampleDataTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear()

    def runTest(self):
        self.setUp()
        self.test_SourcesRegistered()

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
        self.delayDisplay("Test passed")
