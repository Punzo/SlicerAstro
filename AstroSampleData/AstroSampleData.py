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
         "cc78b23cf848b8e4fa4ab29a46659d2f97535ae7c3b65ef037b9427703ab637e"),
        ("WEIN069_cat.txt", "WEIN069_catalogue", "SoFiACatalogFile",
         "ae5d212108a702a20476b8084da2172133355cd0c97de1d0a58b15d803a25322"),
    )),
    # HALOGAS-DR1 low-resolution (30 arcsec) VLA HI cube of NGC 2403 and DSS2 red image of the field
    ("NGC2403", "NGC2403.png", (
        ("NGC2403.fits", "NGC2403", "AstroVolumeFile",
         "1da73e9bc141cbd1367a331d0ee047a9d4543123d4c8bd7c3922b8f6149991f7"),
    )),
    ("NGC2403_DSS", "NGC2403_DSS.png", (
        ("NGC2403_DSS.fits", "NGC2403_DSS", "AstroVolumeFile",
         "ea47ce0030f767b4d25427539b944e170e1b302d0d9a0d83108653a6c8fab851"),
    )),
    # HALOGAS-DR1 low-resolution WSRT HI cubes, cropped to the emission:
    # edge-on disk with extraplanar gas, regular rotating disk, interacting group with tidal streams
    ("NGC891", "NGC891.png", (
        ("NGC891.fits", "NGC891", "AstroVolumeFile",
         "5e30500172c3c048a612acbc54f7099e0ab6f55b2e2b83fda801dc845bc2c2da"),
    )),
    ("NGC3198", "NGC3198.png", (
        ("NGC3198.fits", "NGC3198", "AstroVolumeFile",
         "306ff41d14eb60262ee6ee262109dc7139bebef449012eb2d7a0f3f055ab197f"),
    )),
    ("NGC4631", "NGC4631.png", (
        ("NGC4631.fits", "NGC4631", "AstroVolumeFile",
         "3a0d9e4ed2be1c85c9f3a485268de68aab852f98501071b614bdaa446c3c308d"),
    )),
    # Antennae galaxies (NGC 4038/4039): HI (VLA, Hibbard et al. 2001), CO(2-1)
    # (ALMA Science Verification), Halpha and [NII] (VLT/MUSE, continuum
    # subtracted) and optical image (CTIO R band)
    ("Antennae_HI", "Antennae_HI.png", (
        ("Antennae_HI.fits", "Antennae_HI", "AstroVolumeFile", "83a4555c33394ea41460716183a102272a9e59be6bc26e2565a612452b95defe"),
    )),
    ("Antennae_CO", "Antennae_CO.png", (
        ("Antennae_CO_North.fits", "Antennae_CO_North", "AstroVolumeFile", "a6235fee40c7af33b25d75c415f4b29387175a688d03b0676cc359e89e29c8e1"),
        ("Antennae_CO_South.fits", "Antennae_CO_South", "AstroVolumeFile", "25078257e0b868550e9a497b88f46099b8ef8b5a710cdd6b36f37a13184b4b3b"),
    )),
    ("Antennae_MUSE", "Antennae_MUSE.png", (
        ("Antennae_MUSE_Center02.fits", "Antennae_MUSE_Center02", "AstroVolumeFile", "131c80ce5a949855168bef0d63f4f19aeaeae06f422ea791c7104c858d26c0f0"),
        ("Antennae_MUSE_Center03.fits", "Antennae_MUSE_Center03", "AstroVolumeFile", "2c195045787fcffe2fd53fbee5c80405223d61f23337ff7146cdb67c3c827ea5"),
    )),
    ("Antennae_R", "Antennae_R.png", (
        ("Antennae_R.fits", "Antennae_R", "AstroVolumeFile", "5fd68ff4c3cef27847afbd6abb2488d57d6129159dde646f78d91c3a37247d6a"),
    )),
)


# Description of each sample (Workbench sample list, About window,
# documentation): (short description, details, credit). Full provenance and
# acknowledgements: release notes of SampleData-v2.
SAMPLE_DATA_INFO = {
    "WEIN069": (
        "HI cube · galaxy group with a tidal tail",
        "WEIN069, a companion, a tidal tail and a faint filament between them. Sub-cube of the "
        "Westerbork Synthesis Radio Telescope (WSRT) Zone of Avoidance Perseus-Pisces Filament survey.",
        "Ramatsoku et al. 2016, MNRAS 460, 923"),
    "WEIN069_MASK": (
        "SoFiA 2 mask and source catalogue of WEIN069",
        "Source mask (loaded as a segmentation) and source catalogue (table and labelled points) made "
        "by SoFiA 2.7.1 from the WEIN069 cube.",
        "Westmeier et al. 2021, MNRAS 506, 3962"),
    "NGC2403": (
        "HI cube · spiral galaxy, 30″ beam",
        "NGC 2403 at low resolution (beam 29.7″ × 29.3″, 2.58 km/s channels) from HALOGAS DR1, cropped "
        "to the emission: the faint extraplanar HI. Used by the Modeling (3DBarolo) and Reproject modules.",
        "Fraternali et al. 2002, AJ 123, 3124; Heald et al. 2011, A&A 526, A118"),
    "NGC2403_DSS": (
        "Optical image of the NGC 2403 field",
        "Digitized Sky Survey 2 red image, 24′ × 24′, with standard WCS and plate solution.",
        "DSS2, STScI/AURA"),
    "NGC891": (
        "HI cube · edge-on disk with extraplanar gas",
        "NGC 891 and its companion UGC 1807 (WSRT, beam 35.1″ × 32.2″, 8.24 km/s channels) from HALOGAS "
        "DR1: the HI halo above the disk.",
        "Oosterloo, Fraternali & Sancisi 2007, AJ 134, 1019; Heald et al. 2011"),
    "NGC3198": (
        "HI cube · regularly rotating disk",
        "NGC 3198 (WSRT, beam 35.2″ × 33.5″, 4.12 km/s channels) from HALOGAS DR1: a regular rotating "
        "disk, suited to tilted-ring models (3DBarolo).",
        "Heald et al. 2011, A&A 526, A118"),
    "NGC4631": (
        "HI cube · interacting group with tidal streams",
        "NGC 4631 and NGC 4656 (WSRT, beam 45.0″ × 39.1″, 4.12 km/s channels) from HALOGAS DR1, binned "
        "2 × 2: the tidal streams of the interaction.",
        "Heald et al. 2011, A&A 526, A118"),
    "Antennae_HI": (
        "HI cube · Antennae galaxies (NGC 4038/4039)",
        "VLA HI cube (beam 20.7″ × 15.4″, 5.2 km/s channels): the two tidal tails and a tidal dwarf "
        "galaxy candidate.",
        "Hibbard et al. 2001, AJ 122, 2969"),
    "Antennae_CO": (
        "CO(2-1) cubes · Antennae, north and south",
        "ALMA Science Verification mosaics of NGC 4038 and of NGC 4039 with the overlap region (0.3″ pixels, "
        "20 km/s channels): mosaicking and reprojection.",
        "ALMA ADS/JAO.ALMA#2011.0.00003.SV"),
    "Antennae_MUSE": (
        "Hα and [N II] cubes · Antennae (VLT/MUSE)",
        "Two MUSE pointings, spectral cutouts 6570–6650 Å with the stellar continuum subtracted (line "
        "emission only), 0.2″ pixels.",
        "Weilbacher et al. 2018, A&A 611, A95; ESO programme 095.B-0042(A)"),
    "Antennae_R": (
        "Optical R-band image · Antennae",
        "CTIO 0.9 m R-band image of the Antennae.",
        "Hibbard et al. 2001, AJ 122, 2969"),
}


def sampleThumbnailPath(sampleName):
    """Thumbnail file of a sample (None if unknown)."""
    for name, thumbnail, _ in SAMPLE_DATA_SOURCES:
        if name == sampleName:
            return os.path.join(os.path.dirname(__file__), "Resources", "Icons", thumbnail)
    return None


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


def loadSoFiASample(source):
    """Download and load a SoFiA sample (mask and catalogue), then show the
    catalogue in the AstroSoFiACatalog module."""
    import SampleData

    nodes = SampleData.SampleDataLogic().downloadFromSource(source)
    tables = [node for node in slicer.util.getNodesByClass("vtkMRMLTableNode")
              if node.GetAttribute("SlicerAstro.SoFiASources")]
    if tables and hasattr(slicer.modules, "astrosofiacatalog"):
        slicer.util.selectModule("AstroSoFiACatalog")
        widget = slicer.modules.astrosofiacatalog.widgetRepresentation().self()
        widget.catalogSelector.setCurrentNode(tables[-1])
    return nodes


# samples loaded with a custom function (Sample Data module buttons)
CUSTOM_DOWNLOADERS = {"WEIN069_MASK": loadSoFiASample}


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
            customDownloader=CUSTOM_DOWNLOADERS.get(sampleName),
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
        for name in ("NGC2403", "NGC2403_DSS", "NGC891", "NGC3198", "NGC4631",
                     "Antennae_HI", "Antennae_CO", "Antennae_MUSE", "Antennae_R"):
            self.assertIn(name, names)
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
