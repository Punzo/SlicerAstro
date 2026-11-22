import csv
import math
import os
import re
import shlex
import xml.etree.ElementTree as ElementTree

import qt
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
        self.parent.categories = ["Astronomy"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Loads the source catalogues of SoFiA and SoFiA-2 (https://gitlab.com/SoFiA-Admin/SoFiA-2):
plain text (<i>name</i>_cat.txt, SoFiA 1 <i>name</i>_cat.ascii) and VOTable (<i>name</i>_cat.xml).
Each catalogue is loaded as a table (all the columns) and as a point list with
a labelled point at the centroid of each source. The pixel positions of the
catalogue are placed on the data cube or on the mask of the catalogue (load
the cube or the mask first). Other catalogues (CSV, text or VOTable with
RA, Dec and a velocity or frequency) are placed with the WCS of the cube.
The panel shows the catalogues of a dataset: selecting a source centres
the views on it (Ctrl/Shift: several sources); points, labels, 2D views,
moving, colours, size and deletion of sources.
"""
        self.parent.acknowledgementText = """
This module was developed by Davide Punzo. <br>
This work was supported by ERC grant nr. 291531 and the Slicer Community.
"""


class AstroSoFiACatalogWidget(ScriptedLoadableModuleWidget):
    """The catalogues of a dataset: the reusable C++ Sources section of
    SlicerAstro (the same widget as the Sources tab of AstroVolume and of the
    AstroWorkbench), with a dataset selector that follows the active volume."""

    def setup(self):
        ScriptedLoadableModuleWidget.setup(self)
        self.logic = AstroSoFiACatalogLogic()
        theme = slicer.app.property("SlicerAstroTheme")

        panel = qt.QWidget()
        panelLayout = qt.QVBoxLayout(panel)
        panelLayout.setContentsMargins(0, 0, 0, 0)
        panelLayout.setSpacing(0)
        header = slicer.qSlicerAstroPanelHeader()
        header.title = "Source catalogues"
        header.iconName = "module-sofia-sources"
        panelLayout.addWidget(header)
        body = qt.QWidget()
        bodyLayout = qt.QVBoxLayout(body)
        bodyLayout.setContentsMargins(12, 12, 12, 14)
        bodyLayout.setSpacing(12)
        panelLayout.addWidget(body)

        self.datasetSelector = slicer.qSlicerAstroDatasetSelector()
        self.datasetSelector.setObjectName("InputVolumeNodeSelector")
        self.datasetSelector.nodeTypes = ["vtkMRMLAstroVolumeNode"]
        self.datasetSelector.noneText = "Select a dataset"
        self.datasetSelector.toolTip = "Dataset whose catalogues of sources are shown (and on which they are placed)."
        self.datasetSelector.setMRMLScene(slicer.mrmlScene)
        bodyLayout.addWidget(self.datasetSelector)

        self.sourcesSection = slicer.qSlicerAstroSourcesSection()
        bodyLayout.addWidget(self.sourcesSection)
        bodyLayout.addStretch(1)
        self.layout.addWidget(panel)
        self.layout.addStretch(1)
        if theme:
            theme.applyTo(panel)

        self.datasetSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.sourcesSection.setVolumeNode)
        self.selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        self.selectionObserver = self.selectionNode.AddObserver(vtk.vtkCommand.ModifiedEvent,
                                                                self.onSelectionNodeModified)
        self.onSelectionNodeModified()

    def cleanup(self):
        if getattr(self, "selectionObserver", None):
            self.selectionNode.RemoveObserver(self.selectionObserver)
            self.selectionObserver = None

    def onSelectionNodeModified(self, caller=None, event=None):
        """The panel follows the active volume."""
        activeID = self.selectionNode.GetActiveVolumeID()
        volume = slicer.mrmlScene.GetNodeByID(activeID) if activeID else None
        if volume and volume.IsA("vtkMRMLAstroVolumeNode") and volume != self.datasetSelector.currentNode():
            self.datasetSelector.setCurrentNode(volume)

    def showCatalogue(self, tableNode):
        """Show a catalogue: its dataset selected, the catalogue in the table."""
        volume = slicer.vtkSlicerAstroVolumeLogic.GetSourceCatalogueVolume(tableNode)
        if volume:
            self.datasetSelector.setCurrentNode(volume)
            self.sourcesSection.setVolumeNode(volume)
        self.sourcesSection.setCatalogueNode(tableNode)


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
    """Columns (names, units) and rows (one dict per source) of a SoFiA catalogue
    (or of another source catalogue with celestial positions)."""

    def __init__(self):
        self.columns = []
        self.units = {}
        self.rows = []

    def column(self, names):
        """First column (case insensitive) among names, or None."""
        lowerColumns = {column.lower(): column for column in self.columns}
        for name in names:
            if name in lowerColumns:
                return lowerColumns[name]
        return None


SPEED_OF_LIGHT = 299792458.0  # m/s

# Columns of the celestial positions (degrees) and of the spectral positions
# (velocity definition or frequency) of source catalogues
RA_COLUMNS = ("ra", "raj2000", "_raj2000", "ra_deg", "ra2000")
DEC_COLUMNS = ("dec", "dej2000", "_dej2000", "decj2000", "dec_deg", "de2000", "dec2000")
SPECTRAL_COLUMNS = (
    ("v_opt", "VOPT"), ("vopt", "VOPT"), ("cz", "VOPT"),
    ("v_rad", "VRAD"), ("vrad", "VRAD"),
    ("v_app", "VELO"), ("vapp", "VELO"), ("velo", "VELO"),
    ("freq", "FREQ"), ("frequency", "FREQ"),
)


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
        """Read a SoFiA catalogue (plain text or VOTable), or another source
        catalogue (VOTable, CSV or plain text table with the column names in
        the first row) with celestial positions (ra, dec in degrees) and
        optionally a velocity (v_opt, v_rad, v_app) or frequency column."""
        lowerPath = filePath.lower()
        if lowerPath.endswith((".xml", ".vot", ".votable")):
            catalog = AstroSoFiACatalogLogic._readVOTable(filePath)
        elif AstroSoFiACatalogLogic.isSoFiACatalog(filePath):
            catalog = AstroSoFiACatalogLogic._readText(filePath)
        else:
            catalog = AstroSoFiACatalogLogic._readTable(filePath)
        if "x" not in catalog.columns and not (catalog.column(RA_COLUMNS) and catalog.column(DEC_COLUMNS)):
            raise ValueError(f"{filePath}: the catalogue has no pixel (x, y, z) or celestial (ra, dec) "
                             "position columns")
        return catalog

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
    def _readTable(filePath):
        """CSV or whitespace separated table: column names in the first row
        (units in brackets, e.g. "v_opt [km/s]"), lines starting with # are comments."""
        catalog = SoFiACatalog()
        with open(filePath, errors="replace", newline="") as f:
            lines = [line.strip() for line in f if line.strip() and not line.lstrip().startswith("#")]
        if not lines:
            raise ValueError(f"{filePath}: empty catalogue")
        delimiter = next((d for d in (",", ";", "\t") if d in lines[0]), None)
        if delimiter:
            rows = [[value.strip() for value in row] for row in csv.reader(lines, delimiter=delimiter)]
        else:
            rows = [shlex.split(line) for line in lines]
        unitPattern = re.compile(r"^(.*?)\s*[\[(]([^\])]*)[\])]$")
        for name in rows[0]:
            match = unitPattern.match(name)
            column = match.group(1) if match else name
            catalog.columns.append(column)
            if match:
                catalog.units[column] = match.group(2)
        for values in rows[1:]:
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
        return catalog

    @staticmethod
    def spectralWorldCoordinate(value, unit, columnType, volumeNode):
        """Spectral world coordinate of volumeNode (m/s, in the velocity definition
        of its display: radio, optical or relativistic) of a catalogue velocity
        (columnType VRAD, VOPT, VELO; m/s or km/s, km/s if no unit) or
        frequency (FREQ; Hz if no unit). The conversion goes through the ratio
        of the frequency to the rest frequency, so that e.g. optical velocities
        of a catalogue are placed correctly on a radio velocity axis."""
        c = SPEED_OF_LIGHT
        unit = (unit or "").strip().lower().replace(" ", "")
        restFrequency = float(volumeNode.GetAttribute("SlicerAstro.RESTFREQ") or 0.)
        if columnType == "FREQ":
            scales = {"": 1., "hz": 1., "khz": 1.e3, "mhz": 1.e6, "ghz": 1.e9}
            if unit not in scales:
                raise ValueError(f"unknown frequency unit {unit}")
            if restFrequency <= 0.:
                raise ValueError(f"{volumeNode.GetName()} has no rest frequency: the frequencies of the "
                                 "catalogue cannot be placed")
            ratio = value * scales[unit] / restFrequency
        else:
            scales = {"": 1.e3, "m/s": 1., "ms-1": 1., "m.s-1": 1., "m.s**-1": 1.,
                      "km/s": 1.e3, "kms-1": 1.e3, "km.s-1": 1.e3, "km.s**-1": 1.e3}
            if unit not in scales:
                raise ValueError(f"unknown velocity unit {unit}")
            beta = value * scales[unit] / c
            if columnType == "VRAD":
                ratio = 1. - beta
            elif columnType == "VOPT":
                ratio = 1. / (1. + beta)
            else:
                ratio = math.sqrt((1. - beta) / (1. + beta))
        target = volumeNode.GetAstroVolumeDisplayNode().GetVelocityDefinition()[:4]
        if target == "VRAD":
            return c * (1. - ratio)
        if target == "VOPT":
            return c * (1. / ratio - 1.)
        if target == "VELO":
            return c * (1. - ratio * ratio) / (1. + ratio * ratio)
        raise ValueError(f"{volumeNode.GetName()}: unsupported spectral axis {target}")

    @staticmethod
    def worldPositionVolume(referenceNode):
        """Data cube with the WCS for placing celestial positions: referenceNode,
        or the reference volume of a segmentation."""
        if referenceNode.IsA("vtkMRMLAstroVolumeNode"):
            return referenceNode
        if referenceNode.IsA("vtkMRMLSegmentationNode"):
            volumeNode = referenceNode.GetNodeReference(
                slicer.vtkMRMLSegmentationNode.GetReferenceImageGeometryReferenceRole())
            if volumeNode and volumeNode.IsA("vtkMRMLAstroVolumeNode"):
                return volumeNode
        return None

    @staticmethod
    def worldToIJK(catalog, row, volumeNode):
        """IJK position of a source of a catalogue with celestial (and spectral) positions."""
        displayNode = volumeNode.GetAstroVolumeDisplayNode()
        if displayNode is None:
            raise ValueError(f"{volumeNode.GetName()} has no world coordinates")
        raColumn = catalog.column(RA_COLUMNS)
        decColumn = catalog.column(DEC_COLUMNS)
        is3D = volumeNode.GetImageData() is not None and volumeNode.GetImageData().GetDimensions()[2] > 1
        world = [float(row[raColumn]), float(row[decColumn]), 0.]
        spectralColumn, spectralType = None, None
        for name, columnType in SPECTRAL_COLUMNS:
            spectralColumn = catalog.column((name,))
            if spectralColumn:
                spectralType = columnType
                break
        if is3D:
            if spectralColumn is None:
                # no spectral position: the central channel
                center = [0., 0., (volumeNode.GetImageData().GetDimensions()[2] - 1) * 0.5]
                centerWorld = [0., 0., 0.]
                displayNode.GetReferenceSpace(center, centerWorld)
                world[2] = centerWorld[2]
            else:
                world[2] = AstroSoFiACatalogLogic.spectralWorldCoordinate(
                    float(row[spectralColumn]), catalog.units.get(spectralColumn, ""), spectralType, volumeNode)
        ijk = [0., 0., 0.]
        if not displayNode.GetIJKSpace(world, ijk):
            raise ValueError(f"the world coordinates {world} are not valid for {volumeNode.GetName()}")
        if not is3D:
            ijk[2] = 0.
        return ijk

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
        activeID = selectionNode.GetActiveVolumeID() if selectionNode else None
        activeVolume = slicer.mrmlScene.GetNodeByID(activeID) if activeID else None
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
        tableNode.SetUseColumnTitleAsColumnHeader(True)
        tableNode.SetLocked(True)
        tableNode.Modified()
        return tableNode

    def createPointList(self, catalog, name, referenceNode):
        """Point list with a locked, labelled point at the centroid (x, y, z) of each source."""
        ijkToRAS = self.referenceGeometry(referenceNode)
        if ijkToRAS is None:
            raise ValueError(f"{referenceNode.GetName()} has no geometry")
        # pixel positions (SoFiA), else celestial positions placed with the WCS of the cube
        worldVolume = None
        if "x" not in catalog.columns:
            worldVolume = self.worldPositionVolume(referenceNode)
            if worldVolume is None:
                raise ValueError("the positions of the catalogue are celestial coordinates: select a data cube "
                                 "(or a mask of a data cube) for placing them")
            ijkToRAS = vtk.vtkMatrix4x4()
            worldVolume.GetIJKToRASMatrix(ijkToRAS)
        pointListNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsFiducialNode", name)
        pointListNode.SetAttribute("SlicerAstro.SoFiACatalog", "true")
        pointListNode.SetAndObserveTransformNodeID(referenceNode.GetTransformNodeID())
        pointListNode.CreateDefaultDisplayNodes()
        displayNode = pointListNode.GetDisplayNode()
        displayNode.SetGlyphScale(3)
        displayNode.SetTextScale(3)
        # only in the 3D views by default; in the slice views (if enabled) only
        # where a slice passes through the centre of a source: no projection
        # of the points on all the slices
        displayNode.SetVisibility2D(False)
        displayNode.SetVisibility3D(True)
        displayNode.SliceProjectionOff()
        # points of the sources selected in the table: selected color
        displayNode.SetColor(0.4, 1.0, 1.0)
        displayNode.SetSelectedColor(1.0, 0.733, 0.078)

        wasModifying = pointListNode.StartModify()
        nameColumn = catalog.column(("name", "id", "source", "object", "main_id"))
        for row in catalog.rows:
            if worldVolume is None:
                # SoFiA pixel coordinates are zero-based, as the IJK coordinates
                ijk = [row["x"], row.get("y", 0.0), row.get("z", 0.0), 1.0]
            else:
                ijk = self.worldToIJK(catalog, row, worldVolume) + [1.0]
            ras = ijkToRAS.MultiplyPoint(ijk)[:3]
            label = row.get(nameColumn, "") if nameColumn else ""
            if isinstance(label, float) and label.is_integer():
                label = int(label)
            index = pointListNode.AddControlPoint(ras, str(label))
            if "id" in row and isinstance(row["id"], float):
                pointListNode.SetNthControlPointDescription(index, "id %d" % int(row["id"]))
            pointListNode.SetNthControlPointLocked(index, True)
            pointListNode.SetNthControlPointSelected(index, False)
        pointListNode.SetLocked(True)
        pointListNode.EndModify(wasModifying)
        return pointListNode

    @staticmethod
    def deleteSources(tableNode, rows):
        """Delete sources of a loaded catalogue: rows of the table and their points."""
        rowArray = vtk.vtkIntArray()
        for row in rows:
            rowArray.InsertNextValue(row)
        return slicer.vtkSlicerAstroVolumeLogic.DeleteSources(tableNode, rowArray)

    def loadCatalog(self, catalogPath, referenceNode=None):
        """Load a SoFiA catalogue: table with all the columns, and point list of the sources.

        The point list is created only if a data cube or mask (referenceNode,
        found automatically if not specified) is loaded: the positions of
        SoFiA catalogues are pixel coordinates. Other catalogues (see
        readCatalog) are placed with the world coordinates of the data cube.
        """
        catalog = self.readCatalog(catalogPath)
        base = self.CATALOG_SUFFIX.sub("", os.path.basename(catalogPath))
        if base == os.path.basename(catalogPath):
            base = os.path.splitext(base)[0]
        if referenceNode is None:
            referenceNode = self.findReferenceNode(catalogPath)
        if referenceNode is None:
            # nothing is added to the scene: the catalogue is loaded again
            # once its data are there
            raise ValueError(
                f"The catalogue {os.path.basename(catalogPath)} places its sources on a data cube, and no data "
                f"cube or mask is loaded. Load the data cube of the catalogue (e.g. {base}.fits) or its SoFiA mask "
                f"(e.g. {base}_mask.fits) first, then load the catalogue again.")
        tableNode = self.createTable(catalog, base + "_catalogue")
        pointListNode = self.createPointList(catalog, base + "_sources", referenceNode)
        tableNode.SetAttribute("SlicerAstro.SoFiASources", pointListNode.GetID())
        # the data the sources are placed on (a cube, or the mask of a cube)
        tableNode.SetAttribute("SlicerAstro.SoFiAReference", referenceNode.GetID())
        if referenceNode.IsA("vtkMRMLSegmentationNode") and "x" in catalog.columns:
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
        self.setUp()
        self.test_ModuleWidget()
        self.setUp()
        self.test_CelestialCatalogues()

    def writeMask(self, path, cube=False):
        import numpy as np

        data = np.zeros((12, 16, 20), dtype=">i4")
        data[2:6, 3:6, 1:5] = 1
        data[6:9, 10:14, 12:17] = 2
        if cube:
            data = data.astype(">f4")
        cards = [("SIMPLE", True), ("BITPIX", -32 if cube else 32), ("NAXIS", 3), ("NAXIS1", 20), ("NAXIS2", 16),
                 ("NAXIS3", 12), ("BUNIT", "JY/BEAM" if cube else " "), ("RESTFREQ", 1.42040575177e9),
                 ("CTYPE1", "RA---SIN"), ("CRVAL1", 180.0), ("CDELT1", -0.001), ("CRPIX1", 10.0), ("CUNIT1", "deg"),
                 ("CTYPE2", "DEC--SIN"), ("CRVAL2", 30.0), ("CDELT2", 0.001), ("CRPIX2", 8.0), ("CUNIT2", "deg"),
                 ("CTYPE3", "VRAD"), ("CRVAL3", 1000.0), ("CDELT3", 5000.0), ("CRPIX3", 1.0), ("CUNIT3", "m/s"),
                 ] + ([] if cube else [("HISTORY", "SoFiA control parameters")])
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

    def test_ModuleWidget(self):
        """The panel: the C++ Sources section on the dataset (active volume)."""
        cubePath = os.path.join(self.directory, "test.fits")
        textPath = os.path.join(self.directory, "test_cat.txt")
        self.writeMask(cubePath, cube=True)
        self.writeTextCatalog(textPath)
        cube = slicer.util.loadVolume(cubePath)
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(cube.GetID())

        slicer.util.selectModule("AstroSoFiACatalog")
        widget = slicer.modules.astrosofiacatalog.widgetRepresentation().self()
        section = widget.sourcesSection
        self.assertEqual(widget.datasetSelector.currentNode(), cube)
        self.assertEqual(section.catalogueCount(), 0)
        # no node selectors, path editors or markups widgets of the old panel
        panel = slicer.modules.astrosofiacatalog.widgetRepresentation()
        self.assertEqual(slicer.util.findChildren(panel, className="qMRMLNodeComboBox"), [])
        self.assertEqual(slicer.util.findChildren(panel, className="ctkPathLineEdit"), [])
        self.assertEqual(slicer.util.findChildren(panel, className="qMRMLMarkupsDisplayNodeWidget"), [])

        self.assertTrue(section.loadCatalogue(textPath))
        self.assertEqual(section.catalogueCount(), 1)
        self.assertEqual(section.sourceCount(), 2)
        tableNode = slicer.util.getNode("test_catalogue")
        pointListNode = slicer.vtkSlicerAstroVolumeLogic.GetSourceCataloguePoints(tableNode)
        self.checkSources(pointListNode, cube)

        # only in the 3D views by default, never projected on all the slices
        displayNode = pointListNode.GetDisplayNode()
        self.assertFalse(displayNode.GetVisibility2D())
        self.assertTrue(displayNode.GetVisibility3D())
        self.assertFalse(displayNode.GetSliceProjection())
        slices = slicer.util.findChild(panel, "SourcesSlices")
        slices.click()
        self.assertTrue(displayNode.GetVisibility2D())
        self.assertFalse(displayNode.GetSliceProjection())
        slices.click()
        self.assertFalse(displayNode.GetVisibility2D())
        labels = slicer.util.findChild(panel, "SourcesLabels")
        self.assertTrue(labels.checked)
        labels.click()
        self.assertFalse(displayNode.GetPointLabelsVisibility())

        # locked points; Move unlocks them
        move = slicer.util.findChild(panel, "SourcesMove")
        self.assertFalse(move.checked)
        move.click()
        self.assertFalse(pointListNode.GetNthControlPointLocked(0))
        move.click()
        self.assertTrue(pointListNode.GetNthControlPointLocked(1))

        # colours and size of the points
        section.setSelectedPointColor(qt.QColor(255, 0, 0))
        self.assertEqual(displayNode.GetSelectedColor(), (1.0, 0.0, 0.0))
        section.setPointColor(qt.QColor(0, 0, 255))
        self.assertEqual(displayNode.GetColor(), (0.0, 0.0, 1.0))
        slicer.util.findChild(panel, "SourcesSize").value = 4.0
        self.assertAlmostEqual(displayNode.GetGlyphScale(), 4.0)

        # a selected source: its point selected, the slice views centred on it
        table = slicer.util.findChild(panel, "SourcesTable")
        table.selectRow(1)
        self.assertTrue(pointListNode.GetNthControlPointSelected(1))
        self.assertFalse(pointListNode.GetNthControlPointSelected(0))
        position = [0.0, 0.0, 0.0]
        pointListNode.GetNthControlPointPositionWorld(1, position)
        redSliceNode = slicer.app.layoutManager().sliceWidget("Red").mrmlSliceNode()
        rasToXY = vtk.vtkMatrix4x4()
        vtk.vtkMatrix4x4.Invert(redSliceNode.GetXYToRAS(), rasToXY)
        self.assertAlmostEqual(rasToXY.MultiplyPoint(position + [1.0])[2], 0.0, places=3)

        # several sources selected: all their points selected
        table.selectAll()
        self.assertEqual(list(section.selectedRows()), [0, 1])
        self.assertTrue(pointListNode.GetNthControlPointSelected(0))
        self.assertTrue(pointListNode.GetNthControlPointSelected(1))

        # delete the selected source: table row and point
        table.selectRow(1)
        secondName = pointListNode.GetNthControlPointLabel(1)
        section.deleteSelectedSources()
        self.assertEqual(tableNode.GetNumberOfRows(), 1)
        self.assertEqual(pointListNode.GetNumberOfControlPoints(), 1)
        self.assertNotEqual(pointListNode.GetNthControlPointLabel(0), secondName)
        self.assertEqual(tableNode.GetTable().GetValueByName(0, "name").ToString(),
                         pointListNode.GetNthControlPointLabel(0))

        # the catalogue removed with its points
        pointListID = pointListNode.GetID()
        section.removeCatalogue(False)
        self.assertIsNone(slicer.mrmlScene.GetNodeByID(pointListID))
        self.assertEqual(slicer.util.getNodesByClass("vtkMRMLTableNode"), [])
        self.assertEqual(section.catalogueCount(), 0)
        self.delayDisplay("Test passed")

    def test_CelestialCatalogues(self):
        """Catalogues with celestial positions and optical velocities or
        frequencies are placed with the WCS of a radio velocity cube (#118)."""
        cubePath = os.path.join(self.directory, "cube.fits")
        self.writeMask(cubePath, cube=True)
        cubeNode = slicer.util.loadVolume(cubePath)
        self.assertTrue(cubeNode.IsA("vtkMRMLAstroVolumeNode"))
        displayNode = cubeNode.GetAstroVolumeDisplayNode()
        self.assertEqual(displayNode.GetVelocityDefinition()[:4], "VRAD")
        c = 299792458.0
        restFrequency = 1.42040575177e9
        sources = {"A": (2.5, 4.0, 3.5), "B": (14.0, 11.5, 7.0)}
        csvLines = ["name,ra,dec,v_opt [km/s]"]
        textLines = ["# a source list", "name ra dec freq[MHz]"]
        for name, ijk in sources.items():
            world = [0.0, 0.0, 0.0]
            displayNode.GetReferenceSpace(list(ijk), world)
            radioVelocity = world[2]  # m/s
            opticalVelocity = c * radioVelocity / (c - radioVelocity)
            frequency = restFrequency * (1.0 - radioVelocity / c)
            csvLines.append(f"{name},{world[0]!r},{world[1]!r},{opticalVelocity / 1000.0!r}")
            textLines.append(f"{name} {world[0]!r} {world[1]!r} {frequency / 1.e6!r}")
        csvPath = os.path.join(self.directory, "sources.csv")
        textPath = os.path.join(self.directory, "sources.txt")
        with open(csvPath, "w") as f:
            f.write("\n".join(csvLines) + "\n")
        with open(textPath, "w") as f:
            f.write("\n".join(textLines) + "\n")

        ijkToRAS = vtk.vtkMatrix4x4()
        cubeNode.GetIJKToRASMatrix(ijkToRAS)
        logic = AstroSoFiACatalogLogic()
        for path in (csvPath, textPath):
            tableNode, pointListNode = logic.loadCatalog(path, cubeNode)
            self.assertEqual(tableNode.GetNumberOfRows(), 2)
            self.assertEqual(pointListNode.GetNumberOfControlPoints(), 2)
            for index, (name, ijk) in enumerate(sources.items()):
                self.assertEqual(pointListNode.GetNthControlPointLabel(index), name)
                expected = ijkToRAS.MultiplyPoint(list(ijk) + [1.0])[:3]
                position = pointListNode.GetNthControlPointPosition(index)
                for axis in range(3):
                    self.assertAlmostEqual(position[axis], expected[axis], places=4)

        # same positions with the optical velocity definition on the cube
        displayNode.SetOpticalVelocityDefinition()
        tableNode, pointListNode = logic.loadCatalog(csvPath, cubeNode)
        for index, ijk in enumerate(sources.values()):
            expected = ijkToRAS.MultiplyPoint(list(ijk) + [1.0])[:3]
            position = pointListNode.GetNthControlPointPosition(index)
            for axis in range(3):
                self.assertAlmostEqual(position[axis], expected[axis], places=4)

        # without celestial or pixel positions: a clear error
        badPath = os.path.join(self.directory, "bad.csv")
        with open(badPath, "w") as f:
            f.write("name,flux\nA,1.0\n")
        with self.assertRaises(ValueError):
            logic.loadCatalog(badPath, cubeNode)

        # no data cube or mask loaded (no active volume): a message that tells
        # what to load first, and nothing added to the scene
        slicer.mrmlScene.Clear(0)
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(None)
        tables = slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLTableNode")
        with self.assertRaises(ValueError) as context:
            logic.loadCatalog(csvPath)
        self.assertIn("Load the data cube of the catalogue", str(context.exception))
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLTableNode"), tables)
        self.delayDisplay("Test passed")
