import csv
import math
import os
import re
import shlex
import xml.etree.ElementTree as ElementTree

import ctk
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
the cube or the mask first). Selecting a source in the table centers the
views on it.
"""
        self.parent.acknowledgementText = """
This module was developed by Davide Punzo. <br>
This work was supported by ERC grant nr. 291531 and the Slicer Community.
"""


class AstroSoFiACatalogWidget(ScriptedLoadableModuleWidget):
    def setup(self):
        ScriptedLoadableModuleWidget.setup(self)

        self.logic = AstroSoFiACatalogLogic()

        # Load
        loadSection = ctk.ctkCollapsibleButton()
        loadSection.text = "Load catalogue"
        self.layout.addWidget(loadSection)
        loadLayout = qt.QFormLayout(loadSection)

        self.catalogPathEdit = ctk.ctkPathLineEdit()
        self.catalogPathEdit.filters = ctk.ctkPathLineEdit.Files
        self.catalogPathEdit.nameFilters = [
            "SoFiA catalogue (*_cat.txt *_cat.xml *_cat.ascii)",
            "Source catalogue (*.csv *.tsv *.txt *.xml *.vot)", "All files (*)"]
        self.catalogPathEdit.settingKey = "AstroSoFiACatalog/CatalogPath"
        self.catalogPathEdit.toolTip = (
            "Plain text (_cat.txt, _cat.ascii) or VOTable (_cat.xml) catalogue of SoFiA, or another source "
            "catalogue (CSV, plain text table or VOTable) with the columns ra and dec (degrees) and "
            "optionally v_opt, v_rad, v_app (km/s, or the unit in the column name, e.g. v_opt [m/s]) or "
            "freq (Hz).")
        loadLayout.addRow("Catalogue:", self.catalogPathEdit)

        self.referenceSelector = slicer.qMRMLNodeComboBox()
        self.referenceSelector.nodeTypes = ["vtkMRMLAstroVolumeNode", "vtkMRMLSegmentationNode"]
        self.referenceSelector.noneEnabled = True
        self.referenceSelector.noneDisplay = "Automatic (mask of the catalogue, or active cube)"
        self.referenceSelector.addEnabled = False
        self.referenceSelector.removeEnabled = False
        self.referenceSelector.showChildNodeTypes = False
        self.referenceSelector.setMRMLScene(slicer.mrmlScene)
        self.referenceSelector.toolTip = ("Data cube or mask on which the pixel positions of the catalogue are "
                                          "placed (the SoFiA catalogue must have been created from it), or data "
                                          "cube whose world coordinates place the celestial positions of other "
                                          "catalogues.")
        loadLayout.addRow("Positions on:", self.referenceSelector)

        self.loadButton = qt.QPushButton("Load")
        self.loadButton.toolTip = "Load the catalogue as a table and as labelled source points."
        loadLayout.addRow(self.loadButton)

        # Sources
        sourcesSection = ctk.ctkCollapsibleButton()
        sourcesSection.text = "Sources"
        self.layout.addWidget(sourcesSection)
        sourcesLayout = qt.QVBoxLayout(sourcesSection)

        selectorLayout = qt.QFormLayout()
        sourcesLayout.addLayout(selectorLayout)
        self.catalogSelector = slicer.qMRMLNodeComboBox()
        self.catalogSelector.nodeTypes = ["vtkMRMLTableNode"]
        self.catalogSelector.addAttribute("vtkMRMLTableNode", "SlicerAstro.SoFiASources")
        self.catalogSelector.noneEnabled = True
        self.catalogSelector.addEnabled = False
        self.catalogSelector.removeEnabled = True
        self.catalogSelector.renameEnabled = True
        self.catalogSelector.setMRMLScene(slicer.mrmlScene)
        self.catalogSelector.toolTip = "Loaded SoFiA catalogues."
        selectorLayout.addRow("Catalogue:", self.catalogSelector)

        self.tableView = slicer.qMRMLTableView()
        self.tableView.setMRMLScene(slicer.mrmlScene)
        self.tableView.setSelectionBehavior(qt.QAbstractItemView.SelectRows)
        self.tableView.setSelectionMode(qt.QAbstractItemView.ExtendedSelection)
        self.tableView.minimumHeight = 250
        self.tableView.toolTip = ("Select sources (Ctrl/Shift for several): the views are centered on the "
                                  "current one and their points are shown with the selected color.")
        sourcesLayout.addWidget(self.tableView)

        editLayout = qt.QHBoxLayout()
        self.deleteSelectedButton = qt.QPushButton("Delete selected")
        self.deleteSelectedButton.toolTip = "Delete the selected sources (table rows and points)."
        self.deleteAllButton = qt.QPushButton("Delete all points")
        self.deleteAllButton.toolTip = "Delete all the source points of the catalogue (the table is kept)."
        editLayout.addWidget(self.deleteSelectedButton)
        editLayout.addWidget(self.deleteAllButton)
        editLayout.addStretch(1)
        sourcesLayout.addLayout(editLayout)

        # Source points (markups point list of the catalogue)
        pointsSection = ctk.ctkCollapsibleButton()
        pointsSection.text = "Source points"
        self.layout.addWidget(pointsSection)
        pointsLayout = qt.QVBoxLayout(pointsSection)

        togglesLayout = qt.QHBoxLayout()
        self.showPointsCheckBox = qt.QCheckBox("Show points")
        self.showPointsCheckBox.checked = True
        self.show2DCheckBox = qt.QCheckBox("Show in 2D views")
        self.show2DCheckBox.checked = False
        self.show2DCheckBox.toolTip = ("Show the source points also in the slice views, where a slice passes "
                                       "through the centre of the source (by default only in the 3D views).")
        self.showNamesCheckBox = qt.QCheckBox("Show names")
        self.showNamesCheckBox.checked = True
        self.lockedCheckBox = qt.QCheckBox("Lock positions")
        self.lockedCheckBox.checked = True
        self.lockedCheckBox.toolTip = "Unlock to move the source points with the mouse."
        self.markupsModuleButton = qt.QPushButton("Edit in Markups")
        self.markupsModuleButton.toolTip = "Open the source points in the Markups module."
        for widget in (self.showPointsCheckBox, self.show2DCheckBox, self.showNamesCheckBox, self.lockedCheckBox):
            togglesLayout.addWidget(widget)
        togglesLayout.addStretch(1)
        togglesLayout.addWidget(self.markupsModuleButton)
        pointsLayout.addLayout(togglesLayout)

        colorsLayout = qt.QHBoxLayout()
        colorsLayout.addWidget(qt.QLabel("Color:"))
        self.colorButton = ctk.ctkColorPickerButton()
        self.colorButton.toolTip = "Color of the source points."
        colorsLayout.addWidget(self.colorButton)
        colorsLayout.addWidget(qt.QLabel("Selected color:"))
        self.selectedColorButton = ctk.ctkColorPickerButton()
        self.selectedColorButton.toolTip = "Color of the points of the sources selected in the table."
        colorsLayout.addWidget(self.selectedColorButton)
        colorsLayout.addStretch(1)
        pointsLayout.addLayout(colorsLayout)

        # opacity, glyph and text size; Advanced: glyph type, text, 2D projection
        self.markupsDisplayWidget = slicer.qMRMLMarkupsDisplayNodeWidget()
        self.markupsDisplayWidget.setMRMLScene(slicer.mrmlScene)
        for groupName in ("CollapsibleGroupBox", "ScalarsCollapsibleGroupBox"):  # interaction handles, scalars
            group = slicer.util.findChild(self.markupsDisplayWidget, groupName)
            group.hide()
        pointsLayout.addWidget(self.markupsDisplayWidget)

        self.layout.addStretch(1)

        self.catalogPathEdit.connect("currentPathChanged(QString)", self.updateLoadButton)
        self.loadButton.connect("clicked()", self.onLoad)
        self.catalogSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.onCatalogChanged)
        self.showPointsCheckBox.connect("toggled(bool)", self.updatePointDisplay)
        self.show2DCheckBox.connect("toggled(bool)", self.updatePointDisplay)
        self.showNamesCheckBox.connect("toggled(bool)", self.updatePointDisplay)
        self.lockedCheckBox.connect("toggled(bool)", self.updatePointDisplay)
        self.markupsModuleButton.connect("clicked()", self.onEditInMarkups)
        self.deleteSelectedButton.connect("clicked()", self.onDeleteSelected)
        self.deleteAllButton.connect("clicked()", self.onDeleteAllPoints)
        self.colorButton.connect("colorChanged(QColor)", self.onColorChanged)
        self.selectedColorButton.connect("colorChanged(QColor)", self.onColorChanged)
        self.tableView.selectionModel().connect(
            "selectionChanged(QItemSelection,QItemSelection)", self.onSelectionChanged)
        self.tableView.selectionModel().connect(
            "currentRowChanged(QModelIndex,QModelIndex)", self.onSourceSelected)
        self.updateLoadButton()
        self.onCatalogChanged(self.catalogSelector.currentNode())

    def updateLoadButton(self, path=None):
        path = self.catalogPathEdit.currentPath
        self.loadButton.enabled = bool(path) and os.path.isfile(path)

    def onLoad(self):
        path = self.catalogPathEdit.currentPath
        try:
            tableNode, _ = self.logic.loadCatalog(path, self.referenceSelector.currentNode())
        except Exception as e:
            slicer.util.errorDisplay(f"Failed to load the catalogue {os.path.basename(path)}: {e}")
            return
        self.catalogPathEdit.addCurrentPathToHistory()
        self.catalogSelector.setCurrentNode(tableNode)

    def pointListNode(self):
        tableNode = self.catalogSelector.currentNode()
        pointListID = tableNode.GetAttribute("SlicerAstro.SoFiASources") if tableNode else None
        return slicer.mrmlScene.GetNodeByID(pointListID) if pointListID else None

    def onCatalogChanged(self, tableNode):
        self.tableView.setMRMLTableNode(tableNode)
        pointListNode = self.pointListNode()
        self.markupsDisplayWidget.setMRMLMarkupsNode(pointListNode)
        toggles = (self.showPointsCheckBox, self.show2DCheckBox, self.showNamesCheckBox, self.lockedCheckBox,
                   self.colorButton, self.selectedColorButton)
        enabled = pointListNode is not None and pointListNode.GetDisplayNode() is not None
        for widget in toggles + (self.markupsModuleButton, self.markupsDisplayWidget,
                                 self.deleteSelectedButton, self.deleteAllButton):
            widget.enabled = enabled
        if not enabled:
            return
        displayNode = pointListNode.GetDisplayNode()
        for widget in toggles:
            widget.blockSignals(True)
        self.colorButton.color = qt.QColor.fromRgbF(*displayNode.GetColor())
        self.selectedColorButton.color = qt.QColor.fromRgbF(*displayNode.GetSelectedColor())
        self.showPointsCheckBox.checked = displayNode.GetVisibility()
        self.show2DCheckBox.checked = displayNode.GetVisibility2D()
        self.showNamesCheckBox.checked = displayNode.GetPointLabelsVisibility()
        self.lockedCheckBox.checked = pointListNode.GetLocked() or all(
            pointListNode.GetNthControlPointLocked(index) for index in range(pointListNode.GetNumberOfControlPoints()))
        for widget in toggles:
            widget.blockSignals(False)

    def updatePointDisplay(self, value=None):
        pointListNode = self.pointListNode()
        displayNode = pointListNode.GetDisplayNode() if pointListNode else None
        if not displayNode:
            return
        wasModifying = displayNode.StartModify()
        displayNode.SetVisibility(self.showPointsCheckBox.checked)
        displayNode.SetVisibility2D(self.show2DCheckBox.checked)
        displayNode.SetPointLabelsVisibility(self.showNamesCheckBox.checked)
        displayNode.EndModify(wasModifying)
        # the points are locked one by one by the catalogue reader
        locked = self.lockedCheckBox.checked
        wasModifying = pointListNode.StartModify()
        pointListNode.SetLocked(locked)
        for index in range(pointListNode.GetNumberOfControlPoints()):
            pointListNode.SetNthControlPointLocked(index, locked)
        pointListNode.EndModify(wasModifying)

    def selectedRows(self):
        return sorted({index.row() for index in self.tableView.selectionModel().selectedRows()})

    def onSelectionChanged(self, selected=None, deselected=None):
        """Points of the selected sources are drawn with the selected color."""
        pointListNode = self.pointListNode()
        if not pointListNode:
            return
        rows = set(self.selectedRows())
        wasModifying = pointListNode.StartModify()
        for index in range(pointListNode.GetNumberOfControlPoints()):
            pointListNode.SetNthControlPointSelected(index, index in rows)
        pointListNode.EndModify(wasModifying)

    def onColorChanged(self, color=None):
        pointListNode = self.pointListNode()
        displayNode = pointListNode.GetDisplayNode() if pointListNode else None
        if not displayNode:
            return
        color = self.colorButton.color
        selectedColor = self.selectedColorButton.color
        wasModifying = displayNode.StartModify()
        displayNode.SetColor(color.redF(), color.greenF(), color.blueF())
        displayNode.SetSelectedColor(selectedColor.redF(), selectedColor.greenF(), selectedColor.blueF())
        displayNode.EndModify(wasModifying)

    def onDeleteSelected(self):
        """Delete the selected sources: rows of the table and points."""
        tableNode = self.catalogSelector.currentNode()
        pointListNode = self.pointListNode()
        rows = self.selectedRows()
        if not tableNode or not rows:
            return
        self.logic.deleteSources(tableNode, rows)
        self.tableView.clearSelection()

    def onDeleteAllPoints(self):
        pointListNode = self.pointListNode()
        if not pointListNode:
            return
        if not slicer.util.confirmOkCancelDisplay(
                f"Delete all the {pointListNode.GetNumberOfControlPoints()} source points of "
                f"{self.catalogSelector.currentNode().GetName()}?"):
            return
        pointListNode.RemoveAllControlPoints()

    def onEditInMarkups(self):
        pointListNode = self.pointListNode()
        if not pointListNode:
            return
        slicer.modules.markups.logic().SetActiveList(pointListNode)
        slicer.util.selectModule("Markups")

    def onSourceSelected(self, current, previous=None):
        pointListNode = self.pointListNode()
        if not pointListNode or not current.isValid():
            return
        row = current.row()
        if row < 0 or row >= pointListNode.GetNumberOfControlPoints():
            return
        position = [0.0, 0.0, 0.0]
        pointListNode.GetNthControlPointPositionWorld(row, position)
        slicer.modules.markups.logic().JumpSlicesToLocation(position[0], position[1], position[2], True)


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
        if referenceNode.IsA("vtkMRMLAstroVolumeNode") or referenceNode.IsA("vtkMRMLAstroLabelMapVolumeNode"):
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
        pointListID = tableNode.GetAttribute("SlicerAstro.SoFiASources")
        pointListNode = slicer.mrmlScene.GetNodeByID(pointListID) if pointListID else None
        table = tableNode.GetTable()
        for row in sorted(set(rows), reverse=True):
            if 0 <= row < table.GetNumberOfRows():
                table.RemoveRow(row)
            if pointListNode and 0 <= row < pointListNode.GetNumberOfControlPoints():
                pointListNode.RemoveNthControlPoint(row)
        table.Modified()
        tableNode.Modified()

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
        tableNode = self.createTable(catalog, base + "_catalogue")
        if referenceNode is None:
            referenceNode = self.findReferenceNode(catalogPath)
        if referenceNode is None:
            raise ValueError("load the data cube or the mask of the catalogue first: the source positions "
                             "of the catalogue are pixel coordinates")
        pointListNode = self.createPointList(catalog, base + "_sources", referenceNode)
        tableNode.SetAttribute("SlicerAstro.SoFiASources", pointListNode.GetID())
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
        maskPath = os.path.join(self.directory, "test_mask.fits")
        textPath = os.path.join(self.directory, "test_cat.txt")
        self.writeMask(maskPath)
        self.writeTextCatalog(textPath)
        maskNode = slicer.util.loadSegmentation(maskPath)

        slicer.util.selectModule("AstroSoFiACatalog")
        widget = slicer.modules.astrosofiacatalog.widgetRepresentation().self()
        widget.catalogPathEdit.currentPath = textPath
        self.assertTrue(widget.loadButton.enabled)
        widget.loadButton.click()
        tableNode = widget.catalogSelector.currentNode()
        self.assertIsNotNone(tableNode)
        self.assertEqual(tableNode.GetName(), "test_catalogue")
        pointListNode = widget.pointListNode()
        self.checkSources(pointListNode, maskNode)
        self.assertTrue(widget.showNamesCheckBox.checked)
        self.assertTrue(widget.lockedCheckBox.checked)

        # only in the 3D views by default, never projected on all the slices
        displayNode = pointListNode.GetDisplayNode()
        self.assertFalse(widget.show2DCheckBox.checked)
        self.assertFalse(displayNode.GetVisibility2D())
        self.assertTrue(displayNode.GetVisibility3D())
        self.assertFalse(displayNode.GetSliceProjection())
        widget.show2DCheckBox.checked = True
        self.assertTrue(displayNode.GetVisibility2D())
        self.assertFalse(displayNode.GetSliceProjection())
        widget.show2DCheckBox.checked = False
        self.assertFalse(displayNode.GetVisibility2D())

        # display controls
        widget.showNamesCheckBox.checked = False
        self.assertFalse(pointListNode.GetDisplayNode().GetPointLabelsVisibility())
        self.assertTrue(widget.markupsDisplayWidget.enabled)
        widget.lockedCheckBox.checked = False
        self.assertFalse(pointListNode.GetNthControlPointLocked(0))
        widget.lockedCheckBox.checked = True
        self.assertTrue(pointListNode.GetNthControlPointLocked(1))

        # only the Advanced group of the markups display options
        self.assertFalse(slicer.util.findChild(widget.markupsDisplayWidget, "ScalarsCollapsibleGroupBox").visible)
        self.assertFalse(slicer.util.findChild(widget.markupsDisplayWidget, "CollapsibleGroupBox").visible)

        # selected sources: selected points (selected color)
        widget.tableView.selectRow(0)
        self.assertTrue(pointListNode.GetNthControlPointSelected(0))
        self.assertFalse(pointListNode.GetNthControlPointSelected(1))
        widget.selectedColorButton.color = qt.QColor(255, 0, 0)
        self.assertEqual(pointListNode.GetDisplayNode().GetSelectedColor(), (1.0, 0.0, 0.0))

        # selecting a source centers the slice views on it
        widget.tableView.selectRow(1)
        position = [0.0, 0.0, 0.0]
        pointListNode.GetNthControlPointPositionWorld(1, position)
        redSliceNode = slicer.app.layoutManager().sliceWidget("Red").mrmlSliceNode()
        rasToXY = vtk.vtkMatrix4x4()
        vtk.vtkMatrix4x4.Invert(redSliceNode.GetXYToRAS(), rasToXY)
        self.assertAlmostEqual(rasToXY.MultiplyPoint(position + [1.0])[2], 0.0, places=3)

        # delete the selected source: table row and point
        secondName = pointListNode.GetNthControlPointLabel(1)
        widget.onDeleteSelected()
        self.assertEqual(tableNode.GetNumberOfRows(), 1)
        self.assertEqual(pointListNode.GetNumberOfControlPoints(), 1)
        self.assertNotEqual(pointListNode.GetNthControlPointLabel(0), secondName)
        self.assertEqual(tableNode.GetTable().GetValueByName(0, "name").ToString(),
                         pointListNode.GetNthControlPointLabel(0))
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
        self.delayDisplay("Test passed")
