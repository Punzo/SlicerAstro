import os
import re
import shlex
import xml.etree.ElementTree as ElementTree

import vtk

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


#
# AstroSoFiACatalog
#


class AstroSoFiACatalog(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro SoFiA Catalog"
        self.parent.categories = ["Astronomy.Utilities"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Loads the source catalogues of SoFiA and SoFiA-2 (https://gitlab.com/SoFiA-Admin/SoFiA-2):
plain text (<i>name</i>_cat.txt, SoFiA 1 <i>name</i>_cat.ascii) and VOTable (<i>name</i>_cat.xml).
Each catalogue is loaded as a table (all the columns) and as a point list with
a labelled point at the centroid of each source. The pixel positions of the
catalogue are placed on the data cube or on the mask of the catalogue (load
the cube or the mask first).
"""
        self.parent.acknowledgementText = """
This module was developed by Davide Punzo. <br>
This work was supported by ERC grant nr. 291531 and the Slicer Community.
"""
        self.parent.hidden = True


class AstroSoFiACatalogWidget(ScriptedLoadableModuleWidget):
    pass


#
# Reader (registered automatically for the module)
#


class AstroSoFiACatalogFileReader:
    def __init__(self, parent):
        self.parent = parent

    def description(self):
        return "SoFiA catalogue"

    def fileType(self):
        return "SoFiACatalogFile"

    def extensions(self):
        return [
            "SoFiA catalogue (*_cat.txt)",
            "SoFiA catalogue (*_cat.xml)",
            "SoFiA catalogue (*_cat.ascii)",
        ]

    def canLoadFile(self, filePath):
        return AstroSoFiACatalogLogic.isSoFiACatalog(filePath)

    def load(self, properties):
        try:
            logic = AstroSoFiACatalogLogic()
            referenceNode = None
            if "referenceNodeID" in properties:
                referenceNode = slicer.mrmlScene.GetNodeByID(properties["referenceNodeID"])
            tableNode, pointListNode = logic.loadCatalog(properties["fileName"], referenceNode)
        except Exception as e:
            import logging
            import traceback

            logging.error(f"Failed to load the SoFiA catalogue {properties['fileName']}: {e}")
            logging.debug(traceback.format_exc())
            self.parent.userMessages().AddMessage(vtk.vtkCommand.ErrorEvent, str(e))
            return False
        loadedNodes = [tableNode.GetID()]
        if pointListNode:
            loadedNodes.append(pointListNode.GetID())
        self.parent.loadedNodes = loadedNodes
        return True


#
# Logic
#


class SoFiACatalog:
    """Columns (names, units) and rows (one dict per source) of a SoFiA catalogue."""

    def __init__(self):
        self.columns = []
        self.units = {}
        self.rows = []


class AstroSoFiACatalogLogic(ScriptedLoadableModuleLogic):
    CATALOG_SUFFIX = re.compile(r"_cat\.(txt|xml|ascii)$", re.IGNORECASE)
    MASK_SUFFIX = re.compile(r"_mask(-2d|-raw)?\.fits(\.gz)?$", re.IGNORECASE)

    @staticmethod
    def isSoFiACatalog(filePath):
        if not AstroSoFiACatalogLogic.CATALOG_SUFFIX.search(os.path.basename(filePath)):
            return False
        try:
            with open(filePath, errors="replace") as f:
                head = f.read(4096)
        except OSError:
            return False
        return "SoFiA" in head

    @staticmethod
    def readCatalog(filePath):
        """Read a SoFiA catalogue (plain text or VOTable)."""
        if filePath.lower().endswith(".xml"):
            return AstroSoFiACatalogLogic._readVOTable(filePath)
        return AstroSoFiACatalogLogic._readText(filePath)

    @staticmethod
    def _convert(value):
        try:
            return float(value)
        except ValueError:
            return value

    @staticmethod
    def _readText(filePath):
        catalog = SoFiACatalog()
        headerRows = []
        with open(filePath, errors="replace") as f:
            for line in f:
                stripped = line.strip()
                if not stripped:
                    continue
                if stripped.startswith("#"):
                    headerRows.append(stripped[1:].split())
                    continue
                if not catalog.columns:
                    # the column names: the last header row with "name" and "x"
                    for index, row in enumerate(headerRows):
                        if "name" in row and "x" in row:
                            catalog.columns = row
                            if index + 1 < len(headerRows) and len(headerRows[index + 1]) == len(row):
                                catalog.units = dict(zip(row, headerRows[index + 1]))
                    if not catalog.columns:
                        raise ValueError(f"{filePath}: column names not found")
                values = shlex.split(stripped)
                if len(values) != len(catalog.columns):
                    raise ValueError(f"{filePath}: a row has {len(values)} values for {len(catalog.columns)} columns")
                catalog.rows.append({column: AstroSoFiACatalogLogic._convert(value)
                                     for column, value in zip(catalog.columns, values)})
        return catalog

    @staticmethod
    def _readVOTable(filePath):
        catalog = SoFiACatalog()
        root = ElementTree.parse(filePath).getroot()

        def local(tag):
            return tag.rsplit("}", 1)[-1]

        for element in root.iter():
            if local(element.tag) == "FIELD":
                name = element.get("name")
                catalog.columns.append(name)
                catalog.units[name] = element.get("unit", "")
        for element in root.iter():
            if local(element.tag) == "TR":
                values = [(cell.text or "").strip() for cell in element if local(cell.tag) == "TD"]
                if len(values) != len(catalog.columns):
                    raise ValueError(f"{filePath}: a row has {len(values)} values for {len(catalog.columns)} columns")
                catalog.rows.append({column: AstroSoFiACatalogLogic._convert(value)
                                     for column, value in zip(catalog.columns, values)})
        if "x" not in catalog.columns:
            raise ValueError(f"{filePath}: the catalogue has no pixel position columns")
        return catalog

    @staticmethod
    def referenceGeometry(node):
        """IJK to RAS matrix of an astronomical volume or of a segmentation (reference geometry)."""
        ijkToRAS = vtk.vtkMatrix4x4()
        if node.IsA("vtkMRMLVolumeNode"):
            node.GetIJKToRASMatrix(ijkToRAS)
            return ijkToRAS
        if node.IsA("vtkMRMLSegmentationNode"):
            geometry = node.GetSegmentation().GetConversionParameter(
                slicer.vtkSegmentationConverter.GetReferenceImageGeometryParameterName())
            if geometry:
                image = slicer.vtkOrientedImageData()
                slicer.vtkSegmentationConverter.DeserializeImageGeometry(geometry, image, False)
                image.GetImageToWorldMatrix(ijkToRAS)
                return ijkToRAS
        return None

    def findReferenceNode(self, catalogPath):
        """Mask of the catalogue (loaded as segmentation), else the active astronomical volume."""
        base = self.CATALOG_SUFFIX.sub("", os.path.basename(catalogPath))
        candidates = []
        nodeNameSuffix = re.compile(r"_mask(-2d|-raw)?$", re.IGNORECASE)
        for segmentationNode in slicer.util.getNodesByClass("vtkMRMLSegmentationNode"):
            # file of the mask, or name of the node (the storage node may have
            # been removed, e.g. by Sample Data)
            storageNode = segmentationNode.GetStorageNode()
            fileName = storageNode.GetFileName() if storageNode else None
            if fileName and self.MASK_SUFFIX.sub("", os.path.basename(fileName)) == base:
                candidates.append(segmentationNode)
            elif nodeNameSuffix.search(segmentationNode.GetName() or "") and \
                    nodeNameSuffix.sub("", segmentationNode.GetName()) == base:
                candidates.append(segmentationNode)
        if candidates:
            return candidates[-1]
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        activeVolume = slicer.mrmlScene.GetNodeByID(selectionNode.GetActiveVolumeID()) if selectionNode else None
        if activeVolume and activeVolume.IsA("vtkMRMLAstroVolumeNode"):
            return activeVolume
        volumes = slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode")
        return volumes[-1] if volumes else None

    def createTable(self, catalog, name):
        tableNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLTableNode", name)
        table = tableNode.GetTable()
        for column in catalog.columns:
            numeric = all(isinstance(row[column], float) for row in catalog.rows)
            array = vtk.vtkDoubleArray() if numeric else vtk.vtkStringArray()
            array.SetName(column)
            for row in catalog.rows:
                if numeric:
                    array.InsertNextValue(row[column])
                else:
                    array.InsertNextValue(str(row[column]))
            table.AddColumn(array)
            unit = catalog.units.get(column, "")
            if unit and unit != "-":
                tableNode.SetColumnUnitLabel(column, unit)
        tableNode.Modified()
        return tableNode

    def createPointList(self, catalog, name, referenceNode):
        """Point list with a locked, labelled point at the centroid (x, y, z) of each source."""
        ijkToRAS = self.referenceGeometry(referenceNode)
        if ijkToRAS is None:
            raise ValueError(f"{referenceNode.GetName()} has no geometry")
        pointListNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsFiducialNode", name)
        pointListNode.SetAttribute("SlicerAstro.SoFiACatalog", "true")
        pointListNode.SetAndObserveTransformNodeID(referenceNode.GetTransformNodeID())
        pointListNode.CreateDefaultDisplayNodes()
        displayNode = pointListNode.GetDisplayNode()
        displayNode.SetGlyphScale(3)
        displayNode.SetTextScale(3)
        displayNode.SliceProjectionOn()
        displayNode.SliceProjectionUseFiducialColorOff()
        displayNode.SetColor(0.4, 1.0, 1.0)
        displayNode.SetSelectedColor(0.4, 1.0, 1.0)
        displayNode.SetSliceProjectionColor(1.0, 0.733, 0.078)

        wasModifying = pointListNode.StartModify()
        for row in catalog.rows:
            # SoFiA pixel coordinates are zero-based, as the IJK coordinates
            ijk = [row["x"], row.get("y", 0.0), row.get("z", 0.0), 1.0]
            ras = ijkToRAS.MultiplyPoint(ijk)[:3]
            index = pointListNode.AddControlPoint(ras, str(row.get("name", "")))
            if "id" in row:
                pointListNode.SetNthControlPointDescription(index, "id %d" % int(row["id"]))
            pointListNode.SetNthControlPointLocked(index, True)
        pointListNode.EndModify(wasModifying)
        return pointListNode

    def loadCatalog(self, catalogPath, referenceNode=None):
        """Load a SoFiA catalogue: table with all the columns, and point list of the sources.

        The point list is created only if a data cube or mask (referenceNode,
        found automatically if not specified) is loaded: the positions of the
        catalogue are pixel coordinates.
        """
        catalog = self.readCatalog(catalogPath)
        base = self.CATALOG_SUFFIX.sub("", os.path.basename(catalogPath))
        tableNode = self.createTable(catalog, base + "_catalogue")
        if referenceNode is None:
            referenceNode = self.findReferenceNode(catalogPath)
        if referenceNode is None:
            raise ValueError("load the data cube or the mask of the catalogue first: the source positions "
                             "of the catalogue are pixel coordinates")
        pointListNode = self.createPointList(catalog, base + "_sources", referenceNode)
        tableNode.SetAttribute("SlicerAstro.SoFiASources", pointListNode.GetID())
        if referenceNode.IsA("vtkMRMLSegmentationNode"):
            self.nameSegments(catalog, referenceNode)
        return tableNode, pointListNode

    @staticmethod
    def nameSegments(catalog, segmentationNode):
        """Name the segments of a SoFiA mask after the sources of the catalogue.

        Only the segments with the default name ("Source <id>") are renamed: the
        mask may have been loaded before its catalogue was available.
        """
        names = {int(row["id"]): str(row["name"]) for row in catalog.rows if "id" in row and "name" in row}
        segmentation = segmentationNode.GetSegmentation()
        for segmentID in segmentation.GetSegmentIDs():
            segment = segmentation.GetSegment(segmentID)
            sourceID = vtk.reference("")
            if not segment.GetTag("SlicerAstro.SourceID", sourceID):
                continue
            sourceID = int(str(sourceID))
            if sourceID in names and segment.GetName() == f"Source {sourceID}":
                segment.SetName(names[sourceID])


#
# Test
#


def _fitsCard(key, value):
    if isinstance(value, bool):
        text = ("T" if value else "F").rjust(20)
    elif isinstance(value, int):
        text = str(value).rjust(20)
    elif isinstance(value, float):
        text = repr(value).upper().rjust(20)
    elif key in ("HISTORY", "COMMENT"):
        return f"{key:8s}{value}"[:80].ljust(80)
    else:
        text = ("'" + value.ljust(8) + "'").ljust(20)
    return f"{key:8s}= {text}"[:80].ljust(80)


class AstroSoFiACatalogTest(ScriptedLoadableModuleTest):
    SOURCES = {
        1: ("SoFiA J120000.00+300000.0", (2.5, 4.0, 3.5)),
        2: ("SoFiA J120001.00+300010.0", (14.0, 11.5, 7.0)),
    }

    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroSoFiACatalogTest")
        os.makedirs(self.directory, exist_ok=True)

    def runTest(self):
        self.setUp()
        self.test_LoadCatalogues()

    def writeMask(self, path):
        import numpy as np

        data = np.zeros((12, 16, 20), dtype=">i4")
        data[2:6, 3:6, 1:5] = 1
        data[6:9, 10:14, 12:17] = 2
        cards = [("SIMPLE", True), ("BITPIX", 32), ("NAXIS", 3), ("NAXIS1", 20), ("NAXIS2", 16), ("NAXIS3", 12),
                 ("BUNIT", " "),
                 ("CTYPE1", "RA---SIN"), ("CRVAL1", 180.0), ("CDELT1", -0.001), ("CRPIX1", 10.0), ("CUNIT1", "deg"),
                 ("CTYPE2", "DEC--SIN"), ("CRVAL2", 30.0), ("CDELT2", 0.001), ("CRPIX2", 8.0), ("CUNIT2", "deg"),
                 ("CTYPE3", "VRAD"), ("CRVAL3", 1000.0), ("CDELT3", 5000.0), ("CRPIX3", 1.0), ("CUNIT3", "m/s"),
                 ("HISTORY", "SoFiA control parameters")]
        text = "".join(_fitsCard(k, v) for k, v in cards) + "END".ljust(80)
        text += " " * (-len(text) % 2880)
        payload = data.tobytes()
        payload += b"\0" * (-len(payload) % 2880)
        with open(path, "wb") as f:
            f.write(text.encode("ascii") + payload)

    def writeTextCatalog(self, path):
        lines = ["# SoFiA source catalogue", "# Creator: SoFiA 2.7.1", "#",
                 "#                               1         2         3         4         5         6",
                 "#                            name        id         x         y         z      f_sum",
                 "#                               -         -       pix       pix       pix    Jy*m/s",
                 "#"]
        for sourceID, (name, (x, y, z)) in self.SOURCES.items():
            lines.append(f'  "{name}" {sourceID:9d} {x:9.3f} {y:9.3f} {z:9.3f} {sourceID * 1.5:9.3f}')
        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")

    def writeVOTable(self, path):
        rows = "".join(
            f"<TR><TD>{name}</TD><TD>{sourceID}</TD><TD>{x}</TD><TD>{y}</TD><TD>{z}</TD></TR>"
            for sourceID, (name, (x, y, z)) in self.SOURCES.items())
        with open(path, "w") as f:
            f.write(f"""<?xml version="1.0" ?>
<VOTABLE version="1.3" xmlns="http://www.ivoa.net/xml/VOTable/v1.3"><RESOURCE>
<DESCRIPTION>Source catalogue created by the Source Finding Application (SoFiA 2.7.1)</DESCRIPTION>
<TABLE><FIELD arraysize="32" datatype="char" name="name" unit=""/>
<FIELD datatype="long" name="id" unit=""/><FIELD datatype="double" name="x" unit="pix"/>
<FIELD datatype="double" name="y" unit="pix"/><FIELD datatype="double" name="z" unit="pix"/>
<DATA><TABLEDATA>{rows}</TABLEDATA></DATA></TABLE></RESOURCE></VOTABLE>
""")

    def checkSources(self, pointListNode, maskNode):
        ijkToRAS = AstroSoFiACatalogLogic.referenceGeometry(maskNode)
        self.assertEqual(pointListNode.GetNumberOfControlPoints(), len(self.SOURCES))
        for index, (sourceID, (name, (x, y, z))) in enumerate(self.SOURCES.items()):
            self.assertEqual(pointListNode.GetNthControlPointLabel(index), name)
            self.assertEqual(pointListNode.GetNthControlPointDescription(index), "id %d" % sourceID)
            self.assertTrue(pointListNode.GetNthControlPointLocked(index))
            expected = ijkToRAS.MultiplyPoint([x, y, z, 1.0])[:3]
            position = pointListNode.GetNthControlPointPosition(index)
            for axis in range(3):
                self.assertAlmostEqual(position[axis], expected[axis], places=6)

    def test_LoadCatalogues(self):
        maskPath = os.path.join(self.directory, "test_mask.fits")
        textPath = os.path.join(self.directory, "test_cat.txt")
        xmlPath = os.path.join(self.directory, "test_cat.xml")
        self.writeMask(maskPath)
        self.writeTextCatalog(textPath)
        self.writeVOTable(xmlPath)

        # The catalogue files are recognized
        ioManager = slicer.app.coreIOManager()
        self.assertEqual(ioManager.fileType(textPath), "SoFiACatalogFile")
        self.assertEqual(ioManager.fileType(xmlPath), "SoFiACatalogFile")

        # Without cube or mask: a clear error
        self.assertFalse(slicer.app.ioManager().loadFile(textPath))

        maskNode = slicer.util.loadSegmentation(maskPath)
        self.assertIsNotNone(maskNode)

        # Plain text catalogue: table and points on the mask of the catalogue
        loaded = vtk.vtkCollection()
        self.assertTrue(slicer.app.ioManager().loadNodes("SoFiACatalogFile", {"fileName": textPath}, loaded))
        tableNode = slicer.util.getNode("test_catalogue")
        self.assertEqual(tableNode.GetNumberOfRows(), 2)
        self.assertEqual(tableNode.GetColumnUnitLabel("f_sum"), "Jy*m/s")
        self.assertAlmostEqual(tableNode.GetTable().GetValueByName(1, "f_sum").ToDouble(), 3.0)
        pointListNode = slicer.util.getNode(tableNode.GetAttribute("SlicerAstro.SoFiASources"))
        self.checkSources(pointListNode, maskNode)

        # VOTable catalogue
        tableNode, pointListNode = AstroSoFiACatalogLogic().loadCatalog(xmlPath)
        self.assertEqual(tableNode.GetNumberOfColumns(), 5)
        self.checkSources(pointListNode, maskNode)
        self.delayDisplay("Test passed")
