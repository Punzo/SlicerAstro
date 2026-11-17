"""Workbench mode: Slicer's chrome (menus, toolbars, module panel, status bar)
is hidden and the Workbench is installed around the views:

    header bar (top)
    rail | Datasets drawer | views + axis scrubber | inspector
    WCS status bar (bottom)

Leaving the mode ("Classic Slicer", Ctrl+Shift+W) restores everything."""

import qt
import vtk

import slicer

from . import Astro
from .Inspector import Inspector
from .Views import ViewDecorator

VIEW_COLOR = (7 / 255.0, 10 / 255.0, 18 / 255.0)


def theme():
    return slicer.app.property("SlicerAstroTheme")


def overline(text, parent=None):
    """Section title: small, bold, spaced capitals."""
    label = qt.QLabel(text.upper(), parent)
    label.setProperty("astroRole", "overline")
    font = label.font
    font.setLetterSpacing(qt.QFont.AbsoluteSpacing, 0.9)
    label.setFont(font)
    return label


def iconButton(iconName, toolTip, role="toolbar", checkable=False, parent=None, token="muted"):
    button = qt.QToolButton(parent)
    button.setProperty("astroRole", role)
    button.setIcon(theme().icon(iconName, token))
    button.setIconSize(qt.QSize(18, 18))
    button.toolTip = toolTip
    button.checkable = checkable
    button.setAutoRaise(True)
    return button


class HeaderBar(qt.QFrame):
    """Title, context, tools, layout, theme and the way back to Slicer."""

    def __init__(self, workbench, parent=None):
        qt.QFrame.__init__(self, parent)
        self.workbench = workbench
        self.setProperty("astroRole", "bar")
        self.setObjectName("WorkbenchHeader")
        self.setFixedHeight(52)
        layout = qt.QHBoxLayout(self)
        layout.setContentsMargins(14, 0, 12, 0)
        layout.setSpacing(10)
        logo = qt.QLabel()
        logo.setPixmap(theme().icon("dish", "accent").pixmap(24, 24))
        layout.addWidget(logo)
        title = qt.QLabel("AstroWorkbench")
        title.setProperty("astroRole", "title")
        layout.addWidget(title)
        self.contextLabel = qt.QLabel()
        self.contextLabel.setProperty("astroRole", "label")
        self.contextLabel.setObjectName("WorkbenchContext")
        layout.addWidget(self.contextLabel)
        layout.addSpacing(18)

        # interaction tools
        self.toolGroup = qt.QButtonGroup(self)
        self.toolGroup.setExclusive(True)
        self.navigateButton = iconButton("target", "Navigate: pan, zoom, scroll through the channels", checkable=True)
        self.navigateButton.checked = True
        self.crosshairButton = iconButton("compass", "Crosshair: link the views at the cursor", checkable=True)
        self.lineButton = iconButton("ruler", "Line: angular separation and velocity difference", checkable=True)
        self.angleButton = iconButton("angle", "Angle on the sky", checkable=True)
        self.roiButton = iconButton("roi", "ROI box", checkable=True)
        for button, separator in ((self.navigateButton, False), (self.crosshairButton, True),
                                  (self.lineButton, False), (self.angleButton, False), (self.roiButton, False)):
            self.toolGroup.addButton(button)
            layout.addWidget(button)
            if separator:
                layout.addWidget(self.separator())
        self.navigateButton.connect("clicked()", lambda: self.workbench.setInteraction("navigate"))
        self.crosshairButton.connect("clicked()", lambda: self.workbench.setInteraction("crosshair"))
        self.lineButton.connect("clicked()", lambda: self.workbench.setInteraction("line"))
        self.angleButton.connect("clicked()", lambda: self.workbench.setInteraction("angle"))
        self.roiButton.connect("clicked()", lambda: self.workbench.setInteraction("roi"))
        layout.addStretch(1)

        # layout of the views
        self.layoutButton = slicer.qSlicerAstroSegmentedButton()
        self.layoutButton.setObjectName("WorkbenchLayoutButton")
        self.layoutButton.addButton("", "One view")
        self.layoutButton.addButton("", "Four views")
        self.layoutButton.button(0).setIcon(theme().icon("layout1"))
        self.layoutButton.button(1).setIcon(theme().icon("layout4"))
        self.layoutButton.connect("currentIndexChanged(int)", self.workbench.setViewLayout)
        layout.addWidget(self.layoutButton)
        self.themeButton = iconButton("sun", "Light or dark theme")
        self.themeButton.setObjectName("WorkbenchThemeButton")
        self.themeButton.connect("clicked()", self.toggleTheme)
        layout.addWidget(self.themeButton)
        helpButton = iconButton("help", "SlicerAstro documentation")
        helpButton.connect("clicked()", lambda: qt.QDesktopServices.openUrl(
            qt.QUrl("https://punzo.github.io/SlicerAstro/")))
        layout.addWidget(helpButton)
        self.classicButton = qt.QPushButton("Classic Slicer")
        self.classicButton.setObjectName("ClassicSlicerButton")
        self.classicButton.toolTip = "Back to the Slicer interface (Ctrl+Shift+W)"
        # the header is deleted by exit(): called after the click is handled
        self.classicButton.connect("clicked()", lambda: qt.QTimer.singleShot(0, self.workbench.exit))
        layout.addWidget(self.classicButton)
        theme().connect("themeChanged()", self.updateThemeIcon)
        self.updateThemeIcon()

    def separator(self):
        line = qt.QFrame()
        line.setFixedSize(1, 22)
        line.setStyleSheet(f"background: {theme().color('line').name()};")
        return line

    def toggleTheme(self):
        theme().setMode("light" if theme().dark else "dark")

    def updateThemeIcon(self):
        self.themeButton.setIcon(theme().icon("sun" if theme().dark else "moon"))

    def cleanup(self):
        theme().disconnect("themeChanged()", self.updateThemeIcon)


class Rail(qt.QFrame):
    """Activities: data, sample data, sky search (VO), layers, settings."""

    def __init__(self, workbench, parent=None):
        qt.QFrame.__init__(self, parent)
        self.workbench = workbench
        self.setProperty("astroRole", "rail")
        self.setFixedWidth(60)
        layout = qt.QVBoxLayout(self)
        layout.setContentsMargins(6, 10, 6, 10)
        layout.setSpacing(6)
        self.group = qt.QButtonGroup(self)
        self.buttons = {}
        for name, icon, text, enabled in (("data", "layers", "Data", True), ("samples", "module-sample-data", "Samples", True),
                                          ("sky", "globe", "Sky", False), ("layers", "colormap", "Layers", False)):
            button = qt.QToolButton()
            button.setProperty("astroRole", "rail")
            button.setObjectName(f"Rail_{name}")
            button.setToolButtonStyle(qt.Qt.ToolButtonTextUnderIcon)
            button.setIcon(theme().icon(icon, "muted"))
            button.setIconSize(qt.QSize(20, 20))
            button.text = text
            button.checkable = True
            button.enabled = enabled
            if not enabled:
                button.toolTip = f"{text}: coming with the next steps of SlicerAstro 2.0"
            self.group.addButton(button)
            layout.addWidget(button, 0, qt.Qt.AlignHCenter)
            self.buttons[name] = button
            button.connect("clicked()", lambda n=name: self.workbench.drawer.showPage(n))
        self.buttons["data"].checked = True
        layout.addStretch(1)


class DatasetDrawer(qt.QFrame):
    """Datasets with previews computed from the data, and the sample data."""

    def __init__(self, workbench, parent=None):
        qt.QFrame.__init__(self, parent)
        self.workbench = workbench
        self.setProperty("astroRole", "drawer")
        self.setObjectName("DatasetDrawer")
        self.setFixedWidth(300)
        self.cards = []
        layout = qt.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        self.stack = qt.QStackedWidget()
        layout.addWidget(self.stack)

        # datasets
        page = qt.QWidget()
        pageLayout = qt.QVBoxLayout(page)
        pageLayout.setContentsMargins(14, 14, 14, 10)
        pageLayout.setSpacing(10)
        titleRow = qt.QHBoxLayout()
        title = qt.QLabel("Datasets")
        title.setProperty("astroRole", "title")
        titleRow.addWidget(title)
        titleRow.addStretch(1)
        openButton = qt.QPushButton("Open")
        openButton.setObjectName("OpenDataButton")
        openButton.setIcon(theme().icon("folder", "text"))
        openButton.connect("clicked()", slicer.util.openAddDataDialog)
        titleRow.addWidget(openButton)
        pageLayout.addLayout(titleRow)
        self.filterEdit = qt.QLineEdit()
        self.filterEdit.setObjectName("DatasetFilter")
        self.filterEdit.placeholderText = "Filter by name, object, line"
        self.filterEdit.addAction(theme().icon("search", "subtle"), qt.QLineEdit.LeadingPosition)
        self.filterEdit.connect("textChanged(QString)", lambda text: self.applyFilter())
        pageLayout.addWidget(self.filterEdit)
        scroll = qt.QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(qt.QFrame.NoFrame)
        scroll.setHorizontalScrollBarPolicy(qt.Qt.ScrollBarAlwaysOff)
        self.cardsWidget = qt.QWidget()
        self.cardsLayout = qt.QVBoxLayout(self.cardsWidget)
        self.cardsLayout.setContentsMargins(0, 0, 4, 0)
        self.cardsLayout.setSpacing(8)
        self.emptyLabel = qt.QLabel("No data yet: open a FITS file or download a sample.")
        self.emptyLabel.setProperty("astroRole", "label")
        self.emptyLabel.wordWrap = True
        self.cardsLayout.addWidget(self.emptyLabel)
        self.cardsLayout.addStretch(1)
        scroll.setWidget(self.cardsWidget)
        pageLayout.addWidget(scroll, 1)
        self.stack.addWidget(page)

        # sample data
        samples = qt.QWidget()
        samplesLayout = qt.QVBoxLayout(samples)
        samplesLayout.setContentsMargins(14, 14, 14, 10)
        samplesLayout.setSpacing(8)
        title = qt.QLabel("Sample data")
        title.setProperty("astroRole", "title")
        samplesLayout.addWidget(title)
        note = qt.QLabel("Public HI cubes and images, downloaded once and cached.")
        note.setProperty("astroRole", "label")
        note.wordWrap = True
        samplesLayout.addWidget(note)
        self.samplesLayout = samplesLayout
        self.samplesFilled = False
        samplesLayout.addStretch(1)
        self.stack.addWidget(samples)

    def showPage(self, name):
        if name == "samples":
            self.fillSamples()
            self.stack.setCurrentIndex(1)
        else:
            self.stack.setCurrentIndex(0)

    def fillSamples(self):
        if self.samplesFilled:
            return
        self.samplesFilled = True
        try:
            import SampleData
            sources = SampleData.SampleDataLogic.sampleDataSourcesByCategory("Astronomy")
        except Exception:
            sources = []
        for index, source in enumerate(sources):
            button = qt.QPushButton(source.sampleName)
            button.setObjectName(f"Sample_{source.sampleName}")
            button.setProperty("astroRole", "ghost")
            button.setIcon(theme().icon("download", "muted"))
            button.setStyleSheet("text-align: left;")
            button.connect("clicked()", lambda s=source: self.downloadSample(s))
            self.samplesLayout.insertWidget(2 + index, button)

    def downloadSample(self, source):
        import SampleData
        with slicer.util.tryWithErrorDisplay("The sample could not be downloaded.", waitCursor=True):
            SampleData.SampleDataLogic().downloadFromSource(source)
        self.workbench.rail.buttons["data"].click()

    def refresh(self):
        for card in self.cards:
            self.cardsLayout.removeWidget(card)
            card.deleteLater()
        self.cards = []
        activeID = self.workbench.activeVolumeID()
        datasets = Astro.rootDatasets(slicer.mrmlScene)
        self.emptyLabel.visible = not datasets
        for volume in datasets:
            card = slicer.qSlicerAstroDatasetCard()
            card.setObjectName(f"DatasetCard_{volume.GetID()}")
            card.setVolume(volume)
            card.kind = Astro.kindOf(volume)
            card.chips = Astro.chips(volume)
            for productID, name, detail, icon, node in Astro.productsOf(volume, slicer.mrmlScene):
                card.addProduct(productID, name, detail, icon)
            card.setActive(volume.GetID() == activeID)
            card.connect("clicked()", lambda v=volume: self.workbench.activate(v))
            card.connect("productClicked(QString)", self.workbench.showProduct)
            self.cardsLayout.insertWidget(len(self.cards), card)
            self.cards.append(card)
        self.applyFilter()

    def applyFilter(self):
        text = self.filterEdit.text.strip().lower()
        for card in self.cards:
            volume = card.volume()
            haystack = " ".join([volume.GetName() or "", Astro.attribute(volume, "OBJECT", ""),
                                 " ".join(card.chips)]).lower() if volume else ""
            card.visible = not text or text in haystack


class ScrubberBar(qt.QFrame):
    """The axis scrubber under the views: the spectral axis of the active
    cube with its integrated spectrum; it moves the XY view."""

    def __init__(self, workbench, parent=None):
        qt.QFrame.__init__(self, parent)
        self.workbench = workbench
        self.setProperty("astroRole", "scrubber")
        self.setFixedHeight(92)
        layout = qt.QVBoxLayout(self)
        layout.setContentsMargins(0, 4, 0, 4)
        self.scrubber = slicer.qSlicerAstroAxisScrubber()
        self.scrubber.setObjectName("AxisScrubber")
        layout.addWidget(self.scrubber)
        self.volume = None
        self.sliceNode = None
        self.sliceObserver = None
        self.updating = False
        self.scrubber.connect("currentIndexChanged(int)", self.onIndexChanged)

    def setVolume(self, volume):
        self.volume = volume
        if volume is None or Astro.dimensions(volume) < 3:
            self.scrubber.clear()
            return
        velocities = Astro.channelVelocities(volume)
        self.scrubber.setAxis("channel", Astro.spectralUnit(volume), Astro.spectralLabel(volume), velocities)
        self.scrubber.setProfile(slicer.qSlicerAstroThumbnails().integratedSpectrum(volume))
        self.followSlice(slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeRed"))
        self.onSliceModified()

    def followSlice(self, sliceNode):
        if self.sliceNode is not None and self.sliceObserver is not None:
            self.sliceNode.RemoveObserver(self.sliceObserver)
        self.sliceNode = sliceNode
        self.sliceObserver = sliceNode.AddObserver(vtk.vtkCommand.ModifiedEvent,
                                                   lambda caller, event: self.onSliceModified()) if sliceNode else None

    def onSliceModified(self):
        if self.updating or self.volume is None or self.sliceNode is None or self.scrubber.count() == 0:
            return
        if self.sliceNode.GetOrientation() != "XY":
            return
        self.updating = True
        try:
            self.scrubber.setCurrentIndex(Astro.channelOfSlice(self.volume, self.sliceNode))
        finally:
            self.updating = False

    def onIndexChanged(self, index):
        if self.updating or self.volume is None or self.sliceNode is None:
            return
        self.updating = True
        try:
            if self.sliceNode.GetOrientation() != "XY":
                self.sliceNode.SetOrientation("XY")
            Astro.setSliceToChannel(self.volume, self.sliceNode, index)
        finally:
            self.updating = False

    def cleanup(self):
        self.scrubber.setPlaying(False)
        self.followSlice(None)


class StatusBar(qt.QFrame):
    """WCS readout under the cursor: RA, Dec, velocity, value, beam."""

    def __init__(self, workbench, parent=None):
        qt.QFrame.__init__(self, parent)
        self.workbench = workbench
        self.setProperty("astroRole", "statusBar")
        self.setObjectName("WorkbenchStatusBar")
        self.setFixedHeight(28)
        layout = qt.QHBoxLayout(self)
        layout.setContentsMargins(14, 0, 14, 0)
        layout.setSpacing(22)
        self.labels = {}
        for name in ("ra", "dec", "velocity", "value", "beam"):
            label = qt.QLabel()
            label.setProperty("astroRole", "monoLabel")
            label.setObjectName(f"Status_{name}")
            layout.addWidget(label)
            self.labels[name] = label
        layout.addStretch(1)
        self.hint = qt.QLabel("Ctrl+Shift+W: classic Slicer")
        self.hint.setProperty("astroRole", "label")
        layout.addWidget(self.hint)
        self.crosshair = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLCrosshairNode")
        self.observer = self.crosshair.AddObserver(slicer.vtkMRMLCrosshairNode.CursorPositionModifiedEvent,
                                                   self.onCursor) if self.crosshair else None
        self.clear()

    def subtle(self, text):
        return f"<span style='color:{theme().color('subtle').name()}'>{text}</span>"

    def clear(self):
        volume = self.workbench.activeVolume()
        for name in ("ra", "dec", "velocity", "value"):
            self.labels[name].text = ""
        self.labels["ra"].text = self.subtle("Move the cursor over the data")
        beam = Astro.chips(volume)[0] if volume is not None and Astro.chips(volume) and \
            Astro.chips(volume)[0].endswith("beam") else ""
        self.labels["beam"].text = f"{self.subtle('beam')} {beam[:-5]}" if beam else ""

    def onCursor(self, caller=None, event=None):
        if self.crosshair is None:
            return
        ras = [0.0, 0.0, 0.0]
        if not self.crosshair.GetCursorPositionRAS(ras):
            return
        volume = self.workbench.activeVolume()
        result = Astro.readout(volume, ras)
        if result is None:
            self.clear()
            return
        data = theme().color("data").name()
        self.labels["ra"].text = f"{self.subtle('RA')} {result['ra']}"
        self.labels["dec"].text = f"{self.subtle('Dec')} {result['dec']}"
        self.labels["velocity"].text = f"{self.subtle('v')} {result['velocity']}" if result["velocity"] else ""
        unit = result["unit"].replace("JY/BEAM", "Jy/beam")
        self.labels["value"].text = f"<span style='color:{data}'>{result['value']:.4g} {unit}</span>"

    def cleanup(self):
        if self.crosshair is not None and self.observer is not None:
            self.crosshair.RemoveObserver(self.observer)
        self.observer = None


class Workbench:
    """Enters and leaves the workbench mode of the main window."""

    def __init__(self):
        self.active = False
        self.saved = {}
        self.widgets = []
        self.sceneObservers = []
        self.refreshTimer = qt.QTimer()
        self.refreshTimer.setSingleShot(True)
        self.refreshTimer.setInterval(200)
        self.refreshTimer.connect("timeout()", self.refresh)
        self.inspector = None
        self.scrubberBar = None
        self.views = ViewDecorator(self)

    # ------------------------------------------------------------------
    def activeVolumeID(self):
        selection = slicer.app.applicationLogic().GetSelectionNode()
        return selection.GetActiveVolumeID() if selection else None

    def activeVolume(self):
        volumeID = self.activeVolumeID()
        volume = slicer.mrmlScene.GetNodeByID(volumeID) if volumeID else None
        return volume if Astro.isAstroVolume(volume) else None

    def activate(self, volume):
        selection = slicer.app.applicationLogic().GetSelectionNode()
        selection.SetActiveVolumeID(volume.GetID())
        slicer.app.applicationLogic().PropagateVolumeSelection(0)
        self.refresh()
        self.arrangeViews()

    def showProduct(self, nodeID):
        node = slicer.mrmlScene.GetNodeByID(nodeID)
        if node is None:
            return
        if node.IsA("vtkMRMLSegmentationNode"):
            displayNode = node.GetDisplayNode()
            if displayNode is not None:
                displayNode.SetVisibility(not displayNode.GetVisibility())
            return
        composite = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceCompositeNodeYellow" if self.fourViews()
                                                 else "vtkMRMLSliceCompositeNodeRed")
        sliceNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow" if self.fourViews()
                                                 else "vtkMRMLSliceNodeRed")
        composite.SetBackgroundVolumeID(node.GetID())
        sliceNode.SetOrientation("XZ")
        sliceNode.SetOrientation("XY")
        slicer.app.applicationLogic().FitSliceToAll()

    # ------------------------------------------------------------------
    def fourViews(self):
        layoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode()
        return layoutNode.GetViewArrangement() == slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView

    def setViewLayout(self, index):
        layout = slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpRedSliceView if index == 0 else \
            slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView
        slicer.app.layoutManager().setLayout(layout)
        self.styleViews()
        self.arrangeViews()

    def arrangeViews(self):
        """Red: channel map (XY) of the active cube; yellow: its moment-0
        map (or the sky plane); green: position-velocity (XZ); 3D."""
        volume = self.activeVolume()
        if volume is None:
            return
        zero = None
        for productID, name, detail, icon, node in Astro.productsOf(volume, slicer.mrmlScene):
            if node.IsA("vtkMRMLAstroVolumeNode") and Astro.attribute(node, "DATAMODEL", "") == "ZEROMOMENTMAP":
                zero = node
        cube = Astro.dimensions(volume) >= 3
        views = (("Red", volume, "XY"), ("Yellow", zero if zero is not None else volume, "XY"),
                 ("Green", volume, "XZ" if cube else "XY"))
        for name, node, orientation in views:
            composite = slicer.mrmlScene.GetNodeByID(f"vtkMRMLSliceCompositeNode{name}")
            sliceNode = slicer.mrmlScene.GetNodeByID(f"vtkMRMLSliceNode{name}")
            if composite is None or sliceNode is None:
                continue
            composite.SetBackgroundVolumeID(node.GetID())
            composite.SetForegroundVolumeID(None)
            # another orientation first: the slice is recomputed for the new
            # background (as the moment maps panel does)
            sliceNode.SetOrientation("XZ" if orientation == "XY" else "XY")
            sliceNode.SetOrientation(orientation)
            sliceLogic = slicer.app.applicationLogic().GetSliceLogic(sliceNode)
            if sliceLogic is not None:
                sliceLogic.FitSliceToAll()
        if cube and self.scrubberBar is not None:
            self.scrubberBar.onIndexChanged(self.scrubberBar.scrubber.currentIndex)

    def setInteraction(self, mode):
        interaction = slicer.app.applicationLogic().GetInteractionNode()
        crosshair = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLCrosshairNode")
        if crosshair is not None:
            crosshair.SetCrosshairMode(slicer.vtkMRMLCrosshairNode.ShowBasic if mode == "crosshair"
                                       else slicer.vtkMRMLCrosshairNode.NoCrosshair)
        placeClasses = {"line": "vtkMRMLMarkupsLineNode", "angle": "vtkMRMLMarkupsAngleNode",
                        "roi": "vtkMRMLMarkupsROINode"}
        if mode in placeClasses:
            selection = slicer.app.applicationLogic().GetSelectionNode()
            selection.SetReferenceActivePlaceNodeClassName(placeClasses[mode])
            interaction.SetPlaceModePersistence(0)
            interaction.SetCurrentInteractionMode(slicer.vtkMRMLInteractionNode.Place)
        else:
            interaction.SetCurrentInteractionMode(slicer.vtkMRMLInteractionNode.ViewTransform)

    def styleViews(self):
        """Views on the ink background, with the view bars of the Workbench
        instead of Slicer's view controllers and vertical slice sliders."""
        for viewNode in slicer.util.getNodesByClass("vtkMRMLViewNode"):
            viewNode.SetBackgroundColor(*VIEW_COLOR)
            viewNode.SetBackgroundColor2(*VIEW_COLOR)
        self.views.install()

    # ------------------------------------------------------------------
    def dock(self, widget, area, name):
        dock = qt.QDockWidget()
        dock.setObjectName(name)
        dock.setTitleBarWidget(qt.QWidget())
        dock.setFeatures(qt.QDockWidget.NoDockWidgetFeatures)
        dock.setWidget(widget)
        slicer.util.mainWindow().addDockWidget(area, dock)
        self.widgets.append(dock)
        return dock

    def toolbar(self, widget, area, name):
        bar = qt.QToolBar()
        bar.setObjectName(name)
        bar.setMovable(False)
        bar.setFloatable(False)
        bar.setStyleSheet("QToolBar { border: none; padding: 0px; spacing: 0px; }")
        bar.addWidget(widget)
        slicer.util.mainWindow().addToolBar(area, bar)
        self.widgets.append(bar)
        return bar

    def enter(self):
        if self.active:
            return
        # the AstroVolume panel adds the colour tables of SlicerAstro (e.g.
        # VelocityField) to the scene: created here when Slicer started in
        # the Workbench
        slicer.modules.astrovolume.widgetRepresentation()
        mainWindow = slicer.util.mainWindow()
        layoutManager = slicer.app.layoutManager()
        self.saved = {
            "state": mainWindow.saveState(),
            "menuBar": mainWindow.menuBar().visible,
            "statusBar": mainWindow.statusBar().visible,
            "toolbars": [(bar, bar.visible) for bar in mainWindow.findChildren("QToolBar")],
            "docks": [(dock, dock.visible) for dock in mainWindow.findChildren("QDockWidget")],
            "corners": [mainWindow.corner(corner) for corner in (qt.Qt.BottomLeftCorner, qt.Qt.BottomRightCorner)],
            "layout": layoutManager.layout,
            "views": [(node.GetID(), node.GetBackgroundColor(), node.GetBackgroundColor2())
                      for node in slicer.util.getNodesByClass("vtkMRMLViewNode")],
        }
        mainWindow.menuBar().visible = False
        mainWindow.statusBar().visible = False
        for bar, visible in self.saved["toolbars"]:
            bar.visible = False
        for dock, visible in self.saved["docks"]:
            dock.visible = False
        mainWindow.setCorner(qt.Qt.BottomLeftCorner, qt.Qt.LeftDockWidgetArea)
        mainWindow.setCorner(qt.Qt.BottomRightCorner, qt.Qt.RightDockWidgetArea)

        self.header = HeaderBar(self)
        self.toolbar(self.header, qt.Qt.TopToolBarArea, "WorkbenchHeaderBar")
        left = qt.QWidget()
        left.setFixedWidth(360)
        leftLayout = qt.QHBoxLayout(left)
        leftLayout.setContentsMargins(0, 0, 0, 0)
        leftLayout.setSpacing(0)
        self.rail = Rail(self)
        self.drawer = DatasetDrawer(self)
        leftLayout.addWidget(self.rail)
        leftLayout.addWidget(self.drawer)
        self.dock(left, qt.Qt.LeftDockWidgetArea, "WorkbenchDatasets")
        self.inspector = Inspector(self)
        self.dock(self.inspector, qt.Qt.RightDockWidgetArea, "WorkbenchInspector")
        self.scrubberBar = ScrubberBar(self)
        self.dock(self.scrubberBar, qt.Qt.BottomDockWidgetArea, "WorkbenchScrubber")
        self.statusBar = StatusBar(self)
        self.toolbar(self.statusBar, qt.Qt.BottomToolBarArea, "WorkbenchStatusBar")
        for widget in (self.header, left, self.inspector, self.scrubberBar, self.statusBar):
            theme().applyTo(widget)

        self.header.layoutButton.setCurrentIndex(1 if self.fourViews() else 0)
        self.styleViews()
        mainWindow.resizeDocks([mainWindow.findChild("QDockWidget", "WorkbenchDatasets")], [360], qt.Qt.Horizontal)
        for event in (slicer.mrmlScene.NodeAddedEvent, slicer.mrmlScene.NodeRemovedEvent,
                      slicer.mrmlScene.EndImportEvent, slicer.mrmlScene.EndCloseEvent):
            self.sceneObservers.append(slicer.mrmlScene.AddObserver(event, self.scheduleRefresh))
        selection = slicer.app.applicationLogic().GetSelectionNode()
        self.selectionObserver = selection.AddObserver(vtk.vtkCommand.ModifiedEvent, self.scheduleRefresh)
        self.active = True
        self.refresh()
        self.arrangeViews()

    def scheduleRefresh(self, caller=None, event=None):
        self.refreshTimer.start()

    def refresh(self):
        if not self.active:
            return
        volume = self.activeVolume()
        self.drawer.refresh()
        self.scrubberBar.setVolume(volume)
        self.statusBar.clear()
        self.inspector.setVolume(volume)
        self.views.refresh()
        if volume is not None:
            context = [Astro.attribute(volume, "OBJECT", "").strip(), Astro.attribute(volume, "TELESCOP", "").strip()]
            self.header.contextLabel.text = " · ".join(part for part in context if part) or volume.GetName()
        else:
            self.header.contextLabel.text = ""

    def exit(self):
        if not self.active:
            return
        self.active = False
        self.refreshTimer.stop()
        for observer in self.sceneObservers:
            slicer.mrmlScene.RemoveObserver(observer)
        self.sceneObservers = []
        slicer.app.applicationLogic().GetSelectionNode().RemoveObserver(self.selectionObserver)
        self.header.cleanup()
        self.scrubberBar.cleanup()
        self.statusBar.cleanup()
        self.inspector.cleanup()
        mainWindow = slicer.util.mainWindow()
        for widget in self.widgets:
            if isinstance(widget, qt.QDockWidget):
                mainWindow.removeDockWidget(widget)
            else:
                mainWindow.removeToolBar(widget)
            # deleted now: exit() is never called from a child of these
            # widgets (the Classic Slicer button defers it to the event loop)
            widget.delete()
        self.widgets = []
        corners = self.saved["corners"]
        mainWindow.setCorner(qt.Qt.BottomLeftCorner, corners[0])
        mainWindow.setCorner(qt.Qt.BottomRightCorner, corners[1])
        mainWindow.menuBar().visible = self.saved["menuBar"]
        mainWindow.statusBar().visible = self.saved["statusBar"]
        for bar, visible in self.saved["toolbars"]:
            bar.visible = visible
        for dock, visible in self.saved["docks"]:
            dock.visible = visible
        mainWindow.restoreState(self.saved["state"])
        self.views.remove()
        for nodeID, color, color2 in self.saved["views"]:
            node = slicer.mrmlScene.GetNodeByID(nodeID)
            if node is not None:
                node.SetBackgroundColor(*color)
                node.SetBackgroundColor2(*color2)

    def toggle(self):
        if self.active:
            self.exit()
        else:
            self.enter()
