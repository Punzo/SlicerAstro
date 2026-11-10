import os

import numpy as np
import vtk

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroSegmentationMaskTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Segmentation Mask Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Test of FITS masks as segmentations: a SoFiA-like mask is loaded directly as
a segmentation (one segment per source, names from the SoFiA catalogue),
edited and saved back as a FITS mask without label map volumes.
"""
        self.parent.hidden = True


class AstroSegmentationMaskTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroSegmentationMaskTestLogic(ScriptedLoadableModuleLogic):
    pass


#
# Minimal FITS input/output (independent of vtkFits)
#

def fitsCard(key, value=None):
    if key in ("HISTORY", "COMMENT"):
        return f"{key:8s}{value}"[:80].ljust(80)
    if isinstance(value, bool):
        text = ("T" if value else "F").rjust(20)
    elif isinstance(value, int):
        text = str(value).rjust(20)
    elif isinstance(value, float):
        text = repr(value).upper().rjust(20)
    else:
        text = ("'" + value.ljust(8) + "'").ljust(20)
    return f"{key:8s}= {text}"[:80].ljust(80)


def writeFITS(path, data, cards):
    """Write a 3D int32 array (z, y, x) as a FITS primary image."""
    nz, ny, nx = data.shape
    header = [fitsCard("SIMPLE", True), fitsCard("BITPIX", 32), fitsCard("NAXIS", 3),
              fitsCard("NAXIS1", nx), fitsCard("NAXIS2", ny), fitsCard("NAXIS3", nz)]
    header += [fitsCard(key, value) for key, value in cards]
    header.append("END".ljust(80))
    text = "".join(header)
    text += " " * (-len(text) % 2880)
    payload = data.astype(">i4").tobytes()
    payload += b"\0" * (-len(payload) % 2880)
    with open(path, "wb") as f:
        f.write(text.encode("ascii") + payload)


def readFITS(path):
    """Return (header dict, data array (z, y, x)) of a FITS primary image."""
    with open(path, "rb") as f:
        raw = f.read()
    header = {}
    offset = 0
    while True:
        card = raw[offset:offset + 80].decode("ascii")
        offset += 80
        key = card[:8].strip()
        if key == "END":
            break
        if key == "HIERARCH" and "=" in card:
            # HIERARCH convention for keywords longer than 8 characters
            key, text = card[9:].split("=", 1)
            key = key.strip()
        elif card[8:10] == "= ":
            text = card[10:]
        else:
            continue
        header[key] = text.split("'")[1].strip() if "'" in text else text.split("/")[0].strip()
    offset += -offset % 2880
    bitpix = int(header["BITPIX"])
    dtype = {8: ">u1", 16: ">i2", 32: ">i4", -32: ">f4", -64: ">f8"}[bitpix]
    shape = tuple(int(header["NAXIS%d" % axis]) for axis in range(int(header["NAXIS"]), 0, -1))
    count = int(np.prod(shape))
    data = np.frombuffer(raw, dtype=dtype, count=count, offset=offset).reshape(shape)
    return header, data


WCS_CARDS = [
    ("BUNIT", " "),
    ("CTYPE1", "RA---SIN"), ("CRVAL1", 180.0), ("CDELT1", -0.001), ("CRPIX1", 10.0), ("CUNIT1", "deg"),
    ("CTYPE2", "DEC--SIN"), ("CRVAL2", 30.0), ("CDELT2", 0.001), ("CRPIX2", 8.0), ("CUNIT2", "deg"),
    ("CTYPE3", "FREQ"), ("CRVAL3", 1.42e9), ("CDELT3", -1.0e5), ("CRPIX3", 1.0), ("CUNIT3", "Hz"),
    ("EQUINOX", 2000.0), ("RADESYS", "FK5"), ("SPECSYS", "BARYCENT"), ("RESTFRQ", 1.420405752e9),
    ("HISTORY", "SoFiA control parameters"),
    ("HISTORY", "input.data = cube.fits"),
]

SOURCES = {
    # source ID: (name, (z0, z1, y0, y1, x0, x1))
    1: ("SoFiA J120000.00+300000.0", (1, 4, 2, 6, 3, 8)),
    2: ("SoFiA J120001.00+300010.0", (6, 9, 9, 13, 2, 5)),
    5: ("SoFiA J115959.00+295950.0", (2, 10, 10, 14, 12, 18)),
    40000: ("SoFiA J115958.00+300020.0", (0, 2, 0, 3, 15, 20)),
}


def syntheticMask():
    data = np.zeros((12, 16, 20), dtype=np.int32)
    for sourceID, (name, (z0, z1, y0, y1, x0, x1)) in SOURCES.items():
        data[z0:z1, y0:y1, x0:x1] = sourceID
    return data


def writeCatalog(path):
    lines = [
        "# SoFiA source catalogue",
        "#",
        "#                               1         2         3",
        "#                            name        id         x",
        "#                               -         -       pix",
        "#",
    ]
    for sourceID, (name, extent) in SOURCES.items():
        lines.append(f'  "{name}" {sourceID:9d} {float(extent[4]):9.3f}')
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def sourceIDTag(segment):
    value = vtk.reference("")
    return str(value) if segment.GetTag("SlicerAstro.SourceID", value) else None


class AstroSegmentationMaskTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroSegmentationMaskTest")
        os.makedirs(self.directory, exist_ok=True)

    def runTest(self):
        self.setUp()
        self.test_SoFiAMaskRoundTrip()
        self.setUp()
        self.test_SegmentationOnAstroVolume()

    def sourceArray(self, segmentationNode, segmentID):
        """Voxels of a segment in the geometry of the mask (z, y, x)."""
        segment = segmentationNode.GetSegmentation().GetSegment(segmentID)
        labelmap = segment.GetRepresentation(slicer.vtkSegmentationConverter.GetBinaryLabelmapRepresentationName())
        from vtk.util.numpy_support import vtk_to_numpy

        dims = labelmap.GetDimensions()
        values = vtk_to_numpy(labelmap.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])
        return labelmap, values == segment.GetLabelValue()

    def test_SoFiAMaskRoundTrip(self):
        maskPath = os.path.join(self.directory, "survey_mask.fits")
        original = syntheticMask()
        writeFITS(maskPath, original, WCS_CARDS)
        writeCatalog(os.path.join(self.directory, "survey_cat.txt"))

        # The mask is recognized and loaded as a segmentation
        self.assertEqual(slicer.app.coreIOManager().fileType(maskPath), "SegmentationFile")
        segmentationNode = slicer.util.loadSegmentation(maskPath)
        self.assertIsNotNone(segmentationNode)
        self.assertTrue(segmentationNode.IsA("vtkMRMLSegmentationNode"))
        storageNode = segmentationNode.GetStorageNode()
        self.assertEqual(storageNode.GetClassName(), "vtkMRMLAstroSegmentationStorageNode")

        segmentation = segmentationNode.GetSegmentation()
        self.assertEqual(segmentation.GetNumberOfSegments(), len(SOURCES))
        for sourceID, (name, extent) in SOURCES.items():
            segmentID = "Source_%d" % sourceID
            segment = segmentation.GetSegment(segmentID)
            self.assertIsNotNone(segment, segmentID)
            self.assertEqual(segment.GetName(), name)
            self.assertEqual(sourceIDTag(segment), str(sourceID))
            labelmap, inside = self.sourceArray(segmentationNode, segmentID)
            self.assertEqual(labelmap.GetDimensions(), (20, 16, 12))
            np.testing.assert_array_equal(inside, original == sourceID)
        # All the segments share the voxels of the file
        labelmaps = {segmentation.GetSegment(segmentID).GetRepresentation(
            slicer.vtkSegmentationConverter.GetBinaryLabelmapRepresentationName())
            for segmentID in segmentation.GetSegmentIDs()}
        self.assertEqual(len(labelmaps), 1)

        # Saving writes the FITS mask, with the header of the original mask
        savedPath = os.path.join(self.directory, "saved.fits")
        self.assertTrue(slicer.util.saveNode(segmentationNode, savedPath))
        header, data = readFITS(savedPath)
        np.testing.assert_array_equal(data, original)
        self.assertEqual(header["BITPIX"], "32")
        self.assertEqual(header["DATAMODEL"], "MASK")
        self.assertEqual(header["CTYPE1"], "RA---SIN")
        self.assertEqual(header["CTYPE3"], "FREQ")
        self.assertAlmostEqual(float(header["CRVAL1"]), 180.0)
        self.assertAlmostEqual(float(header["CDELT3"]), -1.0e5)
        self.assertNotIn("BLANK", header)
        self.assertNotIn("BSCALE", header)

        # Edit: remove a source, add a new one; then save again
        segmentation.RemoveSegment("Source_2")
        labelmap, _ = self.sourceArray(segmentationNode, "Source_1")
        newLabelmap = slicer.vtkOrientedImageData()
        newLabelmap.SetExtent(labelmap.GetExtent())
        matrix = vtk.vtkMatrix4x4()
        labelmap.GetImageToWorldMatrix(matrix)
        newLabelmap.SetImageToWorldMatrix(matrix)
        newLabelmap.AllocateScalars(vtk.VTK_UNSIGNED_CHAR, 1)
        from vtk.util.numpy_support import vtk_to_numpy

        newVoxels = vtk_to_numpy(newLabelmap.GetPointData().GetScalars()).reshape(12, 16, 20)
        newVoxels[:] = 0
        newVoxels[8:11, 1:4, 1:4] = 1
        newSegment = slicer.vtkSegment()
        newSegment.SetName("New source")
        newSegment.AddRepresentation(slicer.vtkSegmentationConverter.GetBinaryLabelmapRepresentationName(), newLabelmap)
        segmentation.AddSegment(newSegment, "NewSource")

        editedPath = os.path.join(self.directory, "edited.fits")
        self.assertTrue(slicer.util.saveNode(segmentationNode, editedPath))
        _, edited = readFITS(editedPath)
        expected = original.copy()
        expected[expected == 2] = 0
        expected[8:11, 1:4, 1:4] = 40001  # new IDs follow the largest one
        np.testing.assert_array_equal(edited, expected)

        # Reload: recognized as a mask from DATAMODEL (the name has no "mask")
        self.assertEqual(slicer.app.coreIOManager().fileType(editedPath), "SegmentationFile")
        reloaded = slicer.util.loadSegmentation(editedPath)
        ids = sorted(int(sourceIDTag(reloaded.GetSegmentation().GetSegment(segmentID)))
                     for segmentID in reloaded.GetSegmentation().GetSegmentIDs())
        self.assertEqual(ids, [1, 5, 40000, 40001])

    def test_SegmentationOnAstroVolume(self):
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        dims = volumeNode.GetImageData().GetDimensions()

        # A segmentation drawn in Slicer on the cube
        segmentationNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentationNode")
        segmentationNode.SetReferenceImageGeometryParameterFromVolumeNode(volumeNode)
        segmentID = segmentationNode.GetSegmentation().AddEmptySegment("Galaxy")
        voxels = np.zeros((dims[2], dims[1], dims[0]), dtype=np.uint8)
        voxels[10:20, 30:40, 20:35] = 1
        slicer.util.updateSegmentBinaryLabelmapFromArray(voxels, segmentationNode, segmentID, volumeNode)

        # Saved as a FITS mask with the header of the cube
        storageNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLAstroSegmentationStorageNode")
        segmentationNode.SetAndObserveStorageNodeID(storageNode.GetID())
        path = os.path.join(self.directory, "WEIN069_galaxy.fits")
        storageNode.SetFileName(path)
        self.assertTrue(storageNode.WriteData(segmentationNode))
        header, data = readFITS(path)
        self.assertEqual(data.shape, (dims[2], dims[1], dims[0]))
        np.testing.assert_array_equal(data != 0, voxels != 0)
        self.assertEqual(set(np.unique(data)), {0, 1})
        self.assertEqual(header["DATAMODEL"], "MASK")
        self.assertEqual(header["CTYPE1"], volumeNode.GetAttribute("SlicerAstro.CTYPE1"))
        self.assertAlmostEqual(float(header["CRVAL3"]), float(volumeNode.GetAttribute("SlicerAstro.CRVAL3")), places=3)
        self.delayDisplay("Test passed")
