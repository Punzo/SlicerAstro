"""Helpers of the Inspect tab of the Workbench (fold sections, buttons) and
the Sources section (SoFiA catalogues; to be replaced by its C++ section).
The other sections are the reusable C++ sections of AstroVolume."""

import qt
import vtk

import slicer

from . import Astro


def theme():
    return slicer.app.property("SlicerAstroTheme")


def iconButton(iconName, toolTip, checkable=False, objectName=None):
    button = qt.QToolButton()
    button.setProperty("astroRole", "toolbar")
    button.setIcon(theme().icon(iconName, "muted"))
    button.setIconSize(qt.QSize(16, 16))
    button.toolTip = toolTip
    button.checkable = checkable
    button.setAutoRaise(True)
    if objectName:
        button.setObjectName(objectName)
    return button


def label(text, toolTip=""):
    widget = qt.QLabel(text)
    widget.setProperty("astroRole", "label")
    widget.toolTip = toolTip
    return widget


def foldSection(title, summary, key, collapsed=True):
    section = slicer.qSlicerAstroFoldSection()
    section.setObjectName(f"InspectSection_{title.replace(' ', '')}")
    section.title = title
    section.summary = summary
    section.settingsKey = f"SlicerAstro/AstroWorkbench/{key}Collapsed"
    section.setCollapsed(collapsed)
    return section


def astroVolumeLogic():
    return slicer.modules.astrovolume.logic()


def colourStrip(colorNode, width=48, height=12):
    """Pixmap of a colour table, from the lowest to the highest value."""
    pixmap = qt.QPixmap(width, height)
    table = colorNode.GetLookupTable() if colorNode is not None else None
    if table is None:
        pixmap.fill(qt.QColor(0, 0, 0, 0))
        return pixmap
    image = qt.QImage(width, height, qt.QImage.Format_RGB32)
    count = table.GetNumberOfTableValues()
    rgba = [0.0, 0.0, 0.0, 1.0]
    for x in range(width):
        table.GetTableValue(min(count - 1, int(x * count / width)), rgba)
        colour = qt.QColor.fromRgbF(rgba[0], rgba[1], rgba[2]).rgb()
        for y in range(height):
            image.setPixel(x, y, colour)
    return qt.QPixmap.fromImage(image)


def optionButton(text, toolTip, objectName):
    button = qt.QToolButton()
    button.setProperty("astroRole", "actionChip")
    button.text = text
    button.checkable = True
    button.toolTip = toolTip
    button.setObjectName(objectName)
    button.setCursor(qt.Qt.PointingHandCursor)
    return button


class SourcesSection:
    """Source catalogues of the active dataset (SoFiA, or other catalogues
    with positions): table of the sources; a click moves the views to the
    source, selects its point and its segment in the mask."""

    SEGMENT_TAG = "SlicerAstro.SourceID"

    def __init__(self):
        self.volume = None
        self.section = foldSection("Sources", "catalogues", "Sources")
        self.section.toolTip = "Catalogues of sources placed on the active dataset (e.g. SoFiA)."
        row = qt.QHBoxLayout()
        row.setSpacing(4)
        self.catalogue = qt.QComboBox()
        self.catalogue.setObjectName("SourcesCatalogue")
        self.catalogue.setSizeAdjustPolicy(qt.QComboBox.AdjustToMinimumContentsLengthWithIcon)
        self.catalogue.setMinimumContentsLength(8)
        self.catalogue.toolTip = "Catalogue shown in the table."
        self.catalogue.connect("currentIndexChanged(int)", lambda index: self.showCatalogue())
        self.loadButton = qt.QPushButton("Load")
        self.loadButton.setObjectName("SourcesLoad")
        self.loadButton.setIcon(theme().icon("folder", "text"))
        self.loadButton.toolTip = ("Load a catalogue (SoFiA text or VOTable, CSV or text with RA, Dec and "
                                   "velocity or frequency) and place its sources on the active dataset.")
        self.loadButton.connect("clicked()", self.load)
        row.addWidget(self.catalogue, 1)
        row.addWidget(self.loadButton)
        self.section.contentLayout().addLayout(row)

        self.table = slicer.qMRMLTableView()
        self.table.setObjectName("SourcesTable")
        self.table.setMRMLScene(slicer.mrmlScene)
        self.table.setMinimumHeight(140)
        self.table.setMaximumHeight(240)
        self.table.setMinimumWidth(80)
        self.table.setSelectionBehavior(qt.QAbstractItemView.SelectRows)
        self.table.setSelectionMode(qt.QAbstractItemView.SingleSelection)
        self.table.toolTip = "Sources of the catalogue: click a source to move the views to it."
        self.section.contentLayout().addWidget(self.table)
        self.empty = label("No catalogue", "Load a catalogue of sources (e.g. the SoFiA output of this cube).")
        self.empty.setObjectName("SourcesEmpty")
        self.section.contentLayout().addWidget(self.empty)

        toggles = qt.QHBoxLayout()
        toggles.setSpacing(4)
        self.pointsButton = optionButton("Points", "Show the positions of the sources in the 3D views.",
                                         "SourcesPoints")
        self.labelsButton = optionButton("Labels", "Show the names of the sources next to their points.",
                                         "SourcesLabels")
        self.slicesButton = optionButton("2D", "Show the sources in the 2D views (only where a slice passes "
                                         "through them).", "SourcesSlices")
        self.pointsButton.connect("toggled(bool)", self.setPointsShown)
        self.labelsButton.connect("toggled(bool)", self.setLabelsShown)
        self.slicesButton.connect("toggled(bool)", self.setSlicesShown)
        for button in (self.pointsButton, self.labelsButton, self.slicesButton):
            toggles.addWidget(button)
        toggles.addStretch(1)
        self.section.contentLayout().addLayout(toggles)
        self.status = qt.QLabel()
        self.status.setObjectName("SourcesStatus")
        self.status.setProperty("astroRole", "monoLabel")
        self.status.wordWrap = True
        self.status.toolTip = "The selected source, or the result of the last load."
        self.section.contentLayout().addWidget(self.status)
        self.selectionConnected = False

    def tableNode(self):
        nodeID = self.catalogue.currentData
        return slicer.mrmlScene.GetNodeByID(nodeID) if nodeID else None

    def pointList(self):
        table = self.tableNode()
        pointsID = table.GetAttribute("SlicerAstro.SoFiASources") if table is not None else None
        return slicer.mrmlScene.GetNodeByID(pointsID) if pointsID else None

    def setVolume(self, volume, catalogueID=None):
        self.volume = volume
        self.section.visible = volume is not None
        if volume is None:
            return
        current = catalogueID or self.catalogue.currentData
        catalogues = Astro.cataloguesOf(volume)
        wasBlocked = self.catalogue.blockSignals(True)
        self.catalogue.clear()
        for table in catalogues:
            self.catalogue.addItem(table.GetName(), table.GetID())
        index = self.catalogue.findData(current) if current else 0
        self.catalogue.setCurrentIndex(max(index, 0))
        self.catalogue.blockSignals(wasBlocked)
        count = sum(table.GetTable().GetNumberOfRows() for table in catalogues if table.GetTable())
        self.section.summary = f"{count} source{'s' if count != 1 else ''}" if catalogues else "catalogues"
        self.showCatalogue()

    def showCatalogue(self):
        table = self.tableNode()
        self.table.setMRMLTableNode(table)
        hasCatalogue = table is not None
        self.table.visible = hasCatalogue
        self.catalogue.visible = hasCatalogue
        self.empty.visible = not hasCatalogue
        for button in (self.pointsButton, self.labelsButton, self.slicesButton):
            button.enabled = hasCatalogue
        points = self.pointList()
        displayNode = points.GetDisplayNode() if points is not None else None
        for button, value in ((self.pointsButton, displayNode is not None and displayNode.GetVisibility() and
                               displayNode.GetVisibility3D()),
                              (self.labelsButton, displayNode is not None and displayNode.GetPointLabelsVisibility()),
                              (self.slicesButton, displayNode is not None and displayNode.GetVisibility2D())):
            wasBlocked = button.blockSignals(True)
            button.checked = bool(value)
            button.blockSignals(wasBlocked)
        if hasCatalogue and not self.selectionConnected and self.table.selectionModel() is not None:
            self.table.selectionModel().connect("currentRowChanged(QModelIndex,QModelIndex)", self.onSourceSelected)
            self.selectionConnected = True

    def load(self):
        path = qt.QFileDialog.getOpenFileName(slicer.util.mainWindow(), "Load a catalogue of sources", "",
                                              "Catalogues (*.txt *.cat *.csv *.xml *.vot *.votable);;All files (*)")
        if path:
            self.loadPath(path)

    def loadPath(self, path):
        import AstroSoFiACatalog
        try:
            with slicer.util.WaitCursor():
                table, points = AstroSoFiACatalog.AstroSoFiACatalogLogic().loadCatalog(path, self.volume)
        except Exception as error:
            self.status.text = str(error)
            return None
        rows = table.GetTable().GetNumberOfRows()
        self.status.text = f"{rows} source{'s' if rows != 1 else ''} loaded"
        self.setVolume(self.volume, table.GetID())
        return table

    def onSourceSelected(self, current, previous=None):
        points = self.pointList()
        if points is None or not current.isValid():
            return
        row = current.row()
        if row < 0 or row >= points.GetNumberOfControlPoints():
            return
        for index in range(points.GetNumberOfControlPoints()):
            points.SetNthControlPointSelected(index, index == row)
        position = [0.0, 0.0, 0.0]
        points.GetNthControlPointPositionWorld(row, position)
        slicer.modules.markups.logic().JumpSlicesToLocation(position[0], position[1], position[2], True)
        name = points.GetNthControlPointLabel(row)
        description = points.GetNthControlPointDescription(row)
        self.status.text = " · ".join(part for part in (name, description) if part)
        # the segment of the source in the mask of the dataset (SoFiA id)
        if description.startswith("id ") and self.volume is not None:
            mask = astroVolumeLogic().GetMaskSegmentationForVolume(self.volume, False)
            segmentation = mask.GetSegmentation() if mask is not None else None
            if segmentation is not None:
                for index in range(segmentation.GetNumberOfSegments()):
                    segmentID = segmentation.GetNthSegmentID(index)
                    value = vtk.reference("")
                    if segmentation.GetSegment(segmentID).GetTag(self.SEGMENT_TAG, value) and \
                            str(value) == description[3:]:
                        self.selectedSegmentID = segmentID
                        editorNode = slicer.mrmlScene.GetSingletonNode("SegmentEditor", "vtkMRMLSegmentEditorNode")
                        if editorNode is not None and editorNode.GetSegmentationNode() is mask:
                            editorNode.SetSelectedSegmentID(segmentID)
                        break

    def displayNode(self):
        points = self.pointList()
        return points.GetDisplayNode() if points is not None else None

    def setPointsShown(self, shown):
        displayNode = self.displayNode()
        if displayNode is not None:
            displayNode.SetVisibility(shown or displayNode.GetVisibility2D())
            displayNode.SetVisibility3D(shown)

    def setLabelsShown(self, shown):
        displayNode = self.displayNode()
        if displayNode is not None:
            displayNode.SetPointLabelsVisibility(shown)

    def setSlicesShown(self, shown):
        displayNode = self.displayNode()
        if displayNode is not None:
            displayNode.SetVisibility2D(shown)
            if shown:
                displayNode.SetVisibility(True)
