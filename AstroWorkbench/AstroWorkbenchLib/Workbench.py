"""Workbench mode: Slicer's chrome (menus, toolbars, module panel, status bar)
is hidden and the Workbench is installed around the views:

    header bar (top)
    rail | Datasets drawer | views + axis scrubber | inspector
    WCS status bar (bottom)

Leaving the mode (the full Slicer button, Ctrl+Shift+W) restores everything."""

import os

import qt
import vtk

import slicer

from . import Astro
from . import About
from . import Contours
from .Inspector import Inspector
from .Views import ViewDecorator, ViewVisibility
from . import Workspaces

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
        logo.setObjectName("WorkbenchLogo")
        logo.setPixmap(qt.QIcon(":/AstroTheme/Logo/SlicerAstroLogo.svg").pixmap(32, 32))
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
        self.navigateButton = iconButton("pointer", "Navigate: pan, zoom, scroll through the channels", checkable=True)
        self.navigateButton.checked = True
        self.crosshairButton = iconButton("target", "Crosshair: the same position in all the views", checkable=True)
        self.pointsButton = iconButton("point", "Points: sky positions and velocities (e.g. sources); click to "
                                                "place, several in a row (Esc to stop)", checkable=True)
        self.lineButton = iconButton("ruler", "Line: angular separation and velocity difference", checkable=True)
        self.curveButton = iconButton("curve", "Curve: length on the sky and velocity range along it", checkable=True)
        self.closedCurveButton = iconButton("closedCurve", "Closed curve: perimeter and area on the sky",
                                            checkable=True)
        self.angleButton = iconButton("angle", "Angle on the sky", checkable=True)
        self.roiButton = iconButton("roi", "ROI box", checkable=True)
        for name, button in (("WorkbenchToolPoints", self.pointsButton),
                             ("WorkbenchToolClosedCurve", self.closedCurveButton)):
            button.setObjectName(name)
        for button, separator in ((self.navigateButton, False), (self.crosshairButton, True),
                                  (self.pointsButton, False), (self.lineButton, False), (self.curveButton, False),
                                  (self.closedCurveButton, False), (self.angleButton, False),
                                  (self.roiButton, False)):
            self.toolGroup.addButton(button)
            layout.addWidget(button)
            if separator:
                layout.addWidget(self.separator())
        self.navigateButton.connect("clicked()", lambda: self.workbench.setInteraction("navigate"))
        self.crosshairButton.connect("clicked()", lambda: self.workbench.setInteraction("crosshair"))
        self.lineButton.connect("clicked()", lambda: self.workbench.setInteraction("line"))
        self.pointsButton.connect("clicked()", lambda: self.workbench.setInteraction("points"))
        self.curveButton.connect("clicked()", lambda: self.workbench.setInteraction("curve"))
        self.closedCurveButton.connect("clicked()", lambda: self.workbench.setInteraction("closedCurve"))
        self.angleButton.connect("clicked()", lambda: self.workbench.setInteraction("angle"))
        self.roiButton.connect("clicked()", lambda: self.workbench.setInteraction("roi"))
        layout.addStretch(1)

        # layout of the views
        self.layoutButton = slicer.qSlicerAstroSegmentedButton()
        self.layoutButton.setObjectName("WorkbenchLayoutButton")
        self.layoutButton.toolTip = "Layout of the views."
        self.layoutButton.addButton("", "One view: the channel map")
        self.layoutButton.addButton("", "Three views: channel map, moment 0 and 3D")
        self.layoutButton.addButton("", "Four views: channel map, 3D, position-velocity and moment 0")
        for index, icon in enumerate(("layout1", "layout3", "layout4")):
            self.layoutButton.button(index).setIcon(theme().icon(icon))
            self.layoutButton.button(index).setObjectName(f"WorkbenchLayout_{icon}")
        self.layoutButton.connect("currentIndexChanged(int)", self.workbench.setViewLayout)
        layout.addWidget(self.layoutButton)
        self.themeButton = iconButton("sun", "Light or dark theme")
        self.themeButton.setObjectName("WorkbenchThemeButton")
        self.themeButton.connect("clicked()", self.toggleTheme)
        layout.addWidget(self.themeButton)
        helpButton = iconButton("help", "About SlicerAstro: shortcuts, credits, documentation (F1)")
        helpButton.setObjectName("AboutButton")
        helpButton.connect("clicked()", lambda: About.showAbout())
        layout.addWidget(helpButton)
        self.developerButton = iconButton("terminal", "Developer: Python console", checkable=True)
        self.developerButton.setObjectName("DeveloperButton")
        self.developerButton.connect("toggled(bool)", self.workbench.setPythonConsoleVisible)
        layout.addWidget(self.developerButton)
        self.fullSlicerButton = iconButton("expand", "Full Slicer interface (Ctrl+Shift+W)")
        self.fullSlicerButton.setObjectName("FullSlicerButton")
        # the header is deleted by exit(): called after the click is handled
        self.fullSlicerButton.connect("clicked()", lambda: qt.QTimer.singleShot(0, self.workbench.exit))
        layout.addWidget(self.fullSlicerButton)
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
                                          ("workspaces", "folder", "Spaces", True),
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
            button.toolTip = {
                "data": "Datasets: the loaded data with their previews and derived products.",
                "samples": "Sample data: public HI cubes and images to try SlicerAstro.",
                "workspaces": "Workspaces: save the scene with a screenshot of the views; open a saved one.",
            }.get(name, f"{text}: coming with the next steps of SlicerAstro 2.0")
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
        # wider if the contents need it (e.g. with the vertical scroll bar)
        self.setMinimumWidth(300)
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
        openButton.toolTip = "Open data: FITS cubes and images, masks, catalogues, scenes."
        openButton.setIcon(theme().icon("folder", "text"))
        openButton.connect("clicked()", slicer.util.openAddDataDialog)
        titleRow.addWidget(openButton)
        pageLayout.addLayout(titleRow)
        self.filterEdit = qt.QLineEdit()
        self.filterEdit.setObjectName("DatasetFilter")
        self.filterEdit.toolTip = "Show only the datasets whose name, object or line contains this text."
        self.filterEdit.placeholderText = "Filter by name, object, line"
        self.filterEdit.addAction(theme().icon("search", "subtle"), qt.QLineEdit.LeadingPosition)
        self.filterEdit.connect("textChanged(QString)", lambda text: self.applyFilter())
        pageLayout.addWidget(self.filterEdit)
        scroll = qt.QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(qt.QFrame.NoFrame)
        scroll.setHorizontalScrollBarPolicy(qt.Qt.ScrollBarAlwaysOff)
        slicer.qSlicerAstroScrollAreaFitter(scroll)
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
        samplesPageLayout = qt.QVBoxLayout(samples)
        samplesPageLayout.setContentsMargins(14, 14, 14, 10)
        samplesPageLayout.setSpacing(8)
        title = qt.QLabel("Sample data")
        title.setProperty("astroRole", "title")
        samplesPageLayout.addWidget(title)
        note = qt.QLabel("Public HI cubes and images, downloaded once and cached.")
        note.setProperty("astroRole", "label")
        note.wordWrap = True
        samplesPageLayout.addWidget(note)
        samplesScroll = qt.QScrollArea()
        samplesScroll.setObjectName("SamplesScrollArea")
        samplesScroll.setWidgetResizable(True)
        samplesScroll.setFrameShape(qt.QFrame.NoFrame)
        samplesScroll.setHorizontalScrollBarPolicy(qt.Qt.ScrollBarAlwaysOff)
        slicer.qSlicerAstroScrollAreaFitter(samplesScroll)
        samplesList = qt.QWidget()
        samplesLayout = qt.QVBoxLayout(samplesList)
        samplesLayout.setContentsMargins(0, 0, 4, 0)
        samplesLayout.setSpacing(4)
        samplesScroll.setWidget(samplesList)
        samplesPageLayout.addWidget(samplesScroll, 1)
        self.samplesLayout = samplesLayout
        self.samplesFilled = False
        samplesLayout.addStretch(1)
        self.stack.addWidget(samples)

        # workspaces
        self.workspacesPage = Workspaces.WorkspacesPage(workbench)
        self.workspacesPage.setObjectName("WorkspacesPage")
        self.stack.addWidget(self.workspacesPage)

    def showPage(self, name):
        if name == "samples":
            self.fillSamples()
            self.stack.setCurrentIndex(1)
        elif name == "workspaces":
            self.workspacesPage.refresh()
            self.stack.setCurrentIndex(2)
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
        try:
            import AstroSampleData
            info = AstroSampleData.SAMPLE_DATA_INFO
        except Exception:
            info = {}
        for index, source in enumerate(sources):
            short, details, credit = info.get(source.sampleName, ("", "", ""))
            # a card: thumbnail, name, short description; details and credit
            # in the tooltip
            button = qt.QToolButton()
            button.setObjectName(f"Sample_{source.sampleName}")
            button.setProperty("astroRole", "sampleCard")
            button.setToolButtonStyle(qt.Qt.ToolButtonTextBesideIcon)
            button.setSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Fixed)
            button.setAutoRaise(True)
            thumbnail = getattr(source, "thumbnailFileName", None)
            if thumbnail and os.path.exists(thumbnail):
                button.setIcon(qt.QIcon(thumbnail))
            else:
                button.setIcon(theme().icon("download", "muted"))
            button.setIconSize(qt.QSize(56, 56))
            button.text = f"{source.sampleName}\n{short}" if short else source.sampleName
            button.toolTip = "<br>".join(text for text in (
                f"<b>{source.sampleName}</b>", details, f"<i>{credit}</i>" if credit else "",
                "Click: download (once, then cached) and load.") if text)
            button.connect("clicked()", lambda s=source: self.downloadSample(s))
            self.samplesLayout.insertWidget(index, button)

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
            self.addContourChips(card, volume)
            card.connect("clicked()", lambda v=volume: self.workbench.activate(v))
            card.connect("productClicked(QString)", self.workbench.showProduct)
            card.connect("deleteRequested()", lambda v=volume: self.deleteDataset(v))
            self.cardsLayout.insertWidget(len(self.cards), card)
            self.cards.append(card)
        self.applyFilter()

    def deleteDataset(self, volume, ask=True, withProducts=None):
        """Remove a dataset from the scene; its products (moment maps, masks,
        catalogues...) too if chosen. Returns True if removed."""
        products = [node for _, _, _, _, node in Astro.productsOf(volume, slicer.mrmlScene)]
        if ask:
            box = qt.QMessageBox(slicer.util.mainWindow())
            theme().applyTo(box)
            box.setWindowTitle("Delete dataset")
            box.setIcon(qt.QMessageBox.Question)
            box.setText(f"Delete {volume.GetName()} from the scene?")
            allButton = None
            if products:
                box.setInformativeText(f"It has {len(products)} product{'s' if len(products) != 1 else ''}.")
                allButton = box.addButton("Dataset and products", qt.QMessageBox.DestructiveRole)
            onlyButton = box.addButton("Dataset only" if products else "Delete", qt.QMessageBox.AcceptRole)
            box.addButton(qt.QMessageBox.Cancel)
            box.exec_()
            if box.clickedButton() not in (allButton, onlyButton) or box.clickedButton() is None:
                return False
            withProducts = allButton is not None and box.clickedButton() == allButton
        nodes = ([volume] + products) if withProducts else [volume]
        if not withProducts:
            # its own contours and mask go with it (they have its grid)
            logic = slicer.modules.astrovolume.logic()
            nodes += [node for node in (logic.GetContoursSegmentationForVolume(volume, False),)
                      if node is not None]
        for node in nodes:
            if node.GetScene() is not None:
                slicer.mrmlScene.RemoveNode(node)
        self.refresh()
        return True

    def addContourChips(self, card, volume):
        """A toggle chip per dataset whose contours are drawn over this one
        (e.g. "HI contours · NGC2403")."""
        contours = slicer.modules.astrovolume.logic().GetContoursSegmentationForVolume(volume, False)
        for sourceID, levels in Contours.contourSources(contours).items():
            source = slicer.mrmlScene.GetNodeByID(sourceID)
            if source is None:
                continue
            root = Astro.parentOf(source, Astro.astroVolumes(slicer.mrmlScene)) or source
            kind = "HI contours" if Contours.isHIMap(source) or Contours.isHICube(source) else "contours"
            name = root.GetName() if root is not volume else "self"
            visible = slicer.vtkSlicerAstroVolumeLogic.GetContourSourceVisibility(contours, sourceID)
            card.addActionChip(sourceID, f"{kind} · {name}", visible, "contour",
                               f"Show or hide the contours of {source.GetName()} ({levels} levels) "
                               f"over {volume.GetName()}.")
        card.connect("actionChipToggled(QString,bool)",
                     lambda sourceID, on, v=volume: self.toggleContours(v, sourceID, on))

    def toggleContours(self, volume, sourceID, on):
        contours = slicer.modules.astrovolume.logic().GetContoursSegmentationForVolume(volume, False)
        slicer.vtkSlicerAstroVolumeLogic.SetContourSourceVisibility(contours, sourceID, on)
        if self.workbench.inspector is not None and self.workbench.inspector.volume is volume:
            self.workbench.inspector.contoursWidget.updateSources()

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
        self.scrubber.toolTip = ("Channel of the active view over the integrated spectrum: drag or click to move, "
                                 "Space to play.")
        layout.addWidget(self.scrubber)
        self.volume = None
        self.sliceNode = None
        self.sliceObserver = None
        self.updating = False
        self.scrubber.connect("currentIndexChanged(int)", self.onIndexChanged)

    def setVolume(self, volume, sliceNode=None):
        """The spectral axis of a cube, moving a slice view (by default the
        red one)."""
        sliceNode = sliceNode if sliceNode is not None else slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeRed")
        if volume is self.volume and sliceNode is self.sliceNode and self.scrubber.count() > 0:
            self.onSliceModified()
            return
        self.volume = volume
        if volume is None or Astro.dimensions(volume) < 3:
            self.scrubber.clear()
            return
        velocities = Astro.channelVelocities(volume)
        self.scrubber.setAxis("channel", Astro.spectralUnit(volume), Astro.spectralLabel(volume), velocities)
        self.scrubber.setProfile(slicer.qSlicerAstroThumbnails().integratedSpectrum(volume))
        self.followSlice(sliceNode)
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
        # messages of the Workbench (empty otherwise)
        self.hint = qt.QLabel("")
        self.hint.setProperty("astroRole", "label")
        layout.addWidget(self.hint)
        self.crosshair = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLCrosshairNode")
        self.observer = self.crosshair.AddObserver(slicer.vtkMRMLCrosshairNode.CursorPositionModifiedEvent,
                                                   self.onCursor) if self.crosshair else None
        self.clear()

    def showMessage(self, text, milliseconds=8000):
        """A message in the status bar, for a few seconds."""
        self.hint.text = text
        qt.QTimer.singleShot(milliseconds, lambda: setattr(self.hint, "text", "") if self.hint.text == text else None)

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
        unit = Astro.displayUnit(result["unit"])
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
        self.automaticContours = Contours.AutomaticHIContours()
        self.automaticContoursTimer = qt.QTimer()
        self.automaticContoursTimer.setSingleShot(True)
        self.automaticContoursTimer.setInterval(400)
        self.automaticContoursTimer.connect("timeout()", self.runAutomaticContours)
        self.shortcuts = []
        self.views = ViewDecorator(self)
        self.visibility = ViewVisibility()
        self.activeViewKey = "Red"

    # ------------------------------------------------------------------
    def activeVolumeID(self):
        selection = slicer.app.applicationLogic().GetSelectionNode()
        return selection.GetActiveVolumeID() if selection else None

    def activeVolume(self):
        volumeID = self.activeVolumeID()
        volume = slicer.mrmlScene.GetNodeByID(volumeID) if volumeID else None
        return volume if Astro.isAstroVolume(volume) else None

    def activate(self, volume):
        """Opens a dataset: active volume, in the views (channel map, moment
        0, position-velocity, 3D)."""
        selection = slicer.app.applicationLogic().GetSelectionNode()
        selection.SetActiveVolumeID(volume.GetID())
        slicer.app.applicationLogic().PropagateVolumeSelection(0)
        self.refresh()
        self.arrangeViews()

    # ------------------------------------------------------------------
    def viewNodeID(self, key):
        layoutManager = slicer.app.layoutManager()
        if key.startswith("3D"):
            widget = layoutManager.threeDWidget(int(key[2:]))
            return widget.mrmlViewNode().GetID() if widget is not None else None
        widget = layoutManager.sliceWidget(key)
        return widget.mrmlSliceNode().GetID() if widget is not None else None

    def setActiveView(self, key):
        """The view clicked last (amber border): the scrubber moves it when
        it shows a cube."""
        self.activeViewKey = key
        self.views.setActive(key)
        if key.startswith("3D") or self.scrubberBar is None:
            return
        sliceNode = slicer.mrmlScene.GetNodeByID(self.viewNodeID(key))
        composite = slicer.app.layoutManager().sliceWidget(key).mrmlSliceCompositeNode()
        volumeID = composite.GetBackgroundVolumeID()
        volume = slicer.mrmlScene.GetNodeByID(volumeID) if volumeID else None
        if Astro.isAstroVolume(volume) and Astro.dimensions(volume) >= 3 and sliceNode.GetOrientation() == "XY":
            self.scrubberBar.setVolume(volume, sliceNode)

    def dropNode(self, key, nodeID):
        """Data dragged from the Datasets drawer onto a view: shown in that
        view only."""
        node = slicer.mrmlScene.GetNodeByID(nodeID)
        viewNodeID = self.viewNodeID(key)
        if node is None or viewNodeID is None:
            return
        if node.IsA("vtkMRMLSegmentationNode"):
            self.visibility.addToView(node, viewNodeID)
            self.setActiveView(key)
            return
        if not Astro.isAstroVolume(node):
            return
        if key.startswith("3D"):
            if Astro.dimensions(node) < 3:
                return
            self.visibility.renderInView(node, viewNodeID)
        else:
            sliceNode = slicer.mrmlScene.GetNodeByID(viewNodeID)
            composite = slicer.app.layoutManager().sliceWidget(key).mrmlSliceCompositeNode()
            composite.SetBackgroundVolumeID(node.GetID())
            composite.SetForegroundVolumeID(None)
            orientation = sliceNode.GetOrientation() if Astro.dimensions(node) >= 3 else "XY"
            sliceNode.SetOrientation("XZ" if orientation == "XY" else "XY")
            sliceNode.SetOrientation(orientation)
            sliceLogic = slicer.app.applicationLogic().GetSliceLogic(sliceNode)
            if sliceLogic is not None:
                sliceLogic.FitSliceToAll()
        # the dropped data become the active dataset (without moving the
        # other views)
        if Astro.dimensions(node) >= 3 or self.activeVolume() is None:
            slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(node.GetID())
        self.visibility.update()
        self.setActiveView(key)

    def onConsoleVisibilityChanged(self, visible):
        """The console closed with its own button: the developer button
        unchecked."""
        button = self.header.developerButton
        if button.checked != visible:
            button.blockSignals(True)
            button.checked = visible
            button.blockSignals(False)

    def setPythonConsoleVisible(self, visible):
        mainWindow = slicer.util.mainWindow()
        console = mainWindow.findChild("QDockWidget", "PythonConsoleDockWidget")
        if console is None:
            return
        if visible and mainWindow.dockWidgetArea(console) == qt.Qt.NoDockWidgetArea:
            mainWindow.addDockWidget(qt.Qt.BottomDockWidgetArea, console)
        console.visible = visible
        if visible:
            console.raise_()

    def showProduct(self, nodeID):
        node = slicer.mrmlScene.GetNodeByID(nodeID)
        if node is None:
            return
        if node.IsA("vtkMRMLSegmentationNode"):
            displayNode = node.GetDisplayNode()
            if displayNode is not None:
                displayNode.SetVisibility(not displayNode.GetVisibility())
            return
        if node.IsA("vtkMRMLTableNode"):
            # a catalogue: its sources in Inspect
            volume = Astro.catalogueVolume(node)
            if volume is not None and volume is not self.activeVolume():
                self.activate(volume)
            self.inspector.tabs["Inspect"].checked = True
            self.inspector.showTab(0)
            self.inspector.sourcesWidget.setVolumeNode(self.activeVolume())
            self.inspector.sourcesWidget.setCatalogueNode(node)
            self.inspector.sourcesFold.setCollapsed(False)
            return
        composite = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceCompositeNodeYellow" if self.layoutIndex() > 0
                                                 else "vtkMRMLSliceCompositeNodeRed")
        sliceNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow" if self.layoutIndex() > 0
                                                 else "vtkMRMLSliceNodeRed")
        logic = slicer.modules.astrovolume.logic()
        dataset = self.activeVolume()
        shown = composite.GetBackgroundVolumeID() == node.GetID()
        if Astro.dimensions(node) >= 3:
            # a derived cube (e.g. smoothed): in 3D instead of the dataset
            rendering = logic.GetVolumeRenderingDisplayNode(node, False)
            shown = shown or bool(rendering is not None and rendering.GetVisibility())
            if shown:
                self.showProductHidden(node, dataset, composite)
                return
            logic.PrepareVolumeRendering(node)
            if dataset is not None and dataset is not node:
                logic.SetVolumeRenderingVisibility(dataset, False, False)
            # fitted and centred in the 3D views
            logic.SetVolumeRenderingVisibility(node, True, True)
        elif shown:
            # second click: hidden, the dataset is shown again
            self.showProductHidden(node, dataset, composite)
            return
        composite.SetBackgroundVolumeID(node.GetID())
        # maps and channel maps in the sky plane
        sliceNode.SetOrientation("XZ")
        sliceNode.SetOrientation("XY")
        self.fitSliceView(sliceNode)

    def fitSliceView(self, sliceNode):
        """Fit one view to its data, keeping its orientation (the fit of the
        application logic resets the orientations of all the views)."""
        sliceLogic = slicer.app.applicationLogic().GetSliceLogic(sliceNode) if sliceNode is not None else None
        if sliceLogic is not None:
            sliceLogic.FitSliceToAll()

    def showProductHidden(self, node, dataset, composite):
        """Hide a product shown by showProduct: the dataset in its place."""
        logic = slicer.modules.astrovolume.logic()
        if Astro.dimensions(node) >= 3:
            logic.SetVolumeRenderingVisibility(node, False, False)
            if dataset is not None and dataset is not node:
                logic.SetVolumeRenderingVisibility(dataset, True, True)
        if composite.GetBackgroundVolumeID() == node.GetID():
            composite.SetBackgroundVolumeID(dataset.GetID() if dataset is not None and dataset is not node else None)
            self.fitSliceView(slicer.mrmlScene.GetNodeByID(composite.GetID().replace("CompositeNode", "Node")))

    # ------------------------------------------------------------------
    THREE_VIEWS = 3501
    THREE_VIEWS_LAYOUT = """
<layout type="horizontal" split="true">
 <item splitSize="600">
  <view class="vtkMRMLSliceNode" singletontag="Red">
   <property name="viewlabel" action="default">R</property>
   <property name="viewcolor" action="default">#F34A33</property>
  </view>
 </item>
 <item splitSize="400">
  <layout type="vertical" split="true">
   <item>
    <view class="vtkMRMLSliceNode" singletontag="Yellow">
     <property name="viewlabel" action="default">Y</property>
     <property name="viewcolor" action="default">#EDD54C</property>
    </view>
   </item>
   <item>
    <view class="vtkMRMLViewNode" singletontag="1">
     <property name="viewlabel" action="default">1</property>
    </view>
   </item>
  </layout>
 </item>
</layout>"""

    def layouts(self):
        """Layouts of the switch: one view, three views (channel map,
        moment 0, 3D) and four views."""
        layoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode()
        if not layoutNode.IsLayoutDescription(self.THREE_VIEWS):
            layoutNode.AddLayoutDescription(self.THREE_VIEWS, self.THREE_VIEWS_LAYOUT)
        return (slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpRedSliceView, self.THREE_VIEWS,
                slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)

    def fourViews(self):
        layoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode()
        return layoutNode.GetViewArrangement() == slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView

    def layoutIndex(self):
        arrangement = slicer.app.layoutManager().layoutLogic().GetLayoutNode().GetViewArrangement()
        layouts = self.layouts()
        return layouts.index(arrangement) if arrangement in layouts else 2

    def setViewLayout(self, index):
        slicer.app.layoutManager().setLayout(self.layouts()[index])
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
        if cube:
            threeD = self.viewNodeID("3D0")
            if threeD is not None:
                self.visibility.renderInView(volume, threeD)
        self.visibility.update()
        if cube and self.scrubberBar is not None:
            self.setActiveView("Red")
            self.scrubberBar.onIndexChanged(self.scrubberBar.scrubber.currentIndex)

    PLACE_CLASSES = {"points": "vtkMRMLMarkupsFiducialNode", "line": "vtkMRMLMarkupsLineNode",
                     "curve": "vtkMRMLMarkupsCurveNode", "closedCurve": "vtkMRMLMarkupsClosedCurveNode",
                     "angle": "vtkMRMLMarkupsAngleNode", "roi": "vtkMRMLMarkupsROINode"}

    def toolButtons(self):
        header = self.header
        return {"navigate": header.navigateButton, "crosshair": header.crosshairButton,
                "points": header.pointsButton, "line": header.lineButton, "curve": header.curveButton,
                "closedCurve": header.closedCurveButton, "angle": header.angleButton, "roi": header.roiButton}

    def checkTool(self, mode):
        button = self.toolButtons().get(mode)
        if button is not None and not button.checked:
            button.blockSignals(True)
            button.checked = True
            button.blockSignals(False)

    def onInteractionModeChanged(self, caller=None, event=None):
        """Placement ended outside the header (right click, Esc, last point):
        the placement button is unchecked (navigation again)."""
        interaction = slicer.app.applicationLogic().GetInteractionNode()
        if self.tool in self.PLACE_CLASSES and interaction.GetCurrentInteractionMode() != interaction.Place:
            self.tool = "navigate"
            self.checkTool("navigate")

    def setInteraction(self, mode):
        interaction = slicer.app.applicationLogic().GetInteractionNode()
        # the placement button clicked again: placement ended
        if mode == getattr(self, "tool", None) and mode in self.PLACE_CLASSES and \
                interaction.GetCurrentInteractionMode() == interaction.Place:
            mode = "navigate"
        self.tool = mode
        self.checkTool(mode)
        crosshair = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLCrosshairNode")
        if crosshair is not None:
            crosshair.SetCrosshairMode(slicer.vtkMRMLCrosshairNode.ShowBasic if mode == "crosshair"
                                       else slicer.vtkMRMLCrosshairNode.NoCrosshair)
        placeClasses = self.PLACE_CLASSES
        if mode in placeClasses:
            selection = slicer.app.applicationLogic().GetSelectionNode()
            selection.SetReferenceActivePlaceNodeClassName(placeClasses[mode])
            # points: several in a row (one list); the others: one markup
            interaction.SetPlaceModePersistence(1 if mode == "points" else 0)
            interaction.SetCurrentInteractionMode(slicer.vtkMRMLInteractionNode.Place)
        else:
            interaction.SetCurrentInteractionMode(slicer.vtkMRMLInteractionNode.ViewTransform)

    def styleViews(self):
        """Views on the ink background, with the view bars of the Workbench
        instead of Slicer's view controllers and vertical slice sliders."""
        self.style3DViews()
        self.views.install()

    def style3DViews(self):
        """3D views: dark background in both themes (the colour functions are
        made for a dark sky) and no bounding box. The default view node of the
        scene has the same values, so the views keep them when a scene is
        closed or loaded (the view nodes are reset to the default node)."""
        scene = slicer.mrmlScene
        if "defaultViewNode" not in self.saved:
            self.saved["defaultViewNode"] = scene.GetDefaultNodeByClass("vtkMRMLViewNode")
        default = slicer.vtkMRMLViewNode()
        previous = self.saved["defaultViewNode"]
        if previous is not None:
            default.Copy(previous)
        default.SetBackgroundColor(*VIEW_COLOR)
        default.SetBackgroundColor2(*VIEW_COLOR)
        default.SetBoxVisible(False)
        scene.AddDefaultNode(default)
        for viewNode in slicer.util.getNodesByClass("vtkMRMLViewNode"):
            if tuple(viewNode.GetBackgroundColor()) != VIEW_COLOR:
                viewNode.SetBackgroundColor(*VIEW_COLOR)
            if tuple(viewNode.GetBackgroundColor2()) != VIEW_COLOR:
                viewNode.SetBackgroundColor2(*VIEW_COLOR)
            if viewNode.GetBoxVisible():
                viewNode.SetBoxVisible(False)

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
        bar.setContextMenuPolicy(qt.Qt.PreventContextMenu)
        bar.addWidget(widget)
        slicer.util.mainWindow().addToolBar(area, bar)
        self.widgets.append(bar)
        return bar

    def enter(self):
        if self.active:
            return
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
            "views": [(node.GetID(), node.GetBackgroundColor(), node.GetBackgroundColor2(), node.GetBoxVisible())
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
        left.setMinimumWidth(360)
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

        self.header.layoutButton.setCurrentIndex(self.layoutIndex())
        self.visibility.enter()
        self.saved["styleSheet"] = mainWindow.styleSheet
        self.styleMainWindow()
        theme().connect("themeChanged()", self.styleMainWindow)
        self.styleViews()
        mainWindow.resizeDocks([mainWindow.findChild("QDockWidget", "WorkbenchDatasets")], [360], qt.Qt.Horizontal)
        for event in (slicer.mrmlScene.NodeAddedEvent, slicer.mrmlScene.NodeRemovedEvent,
                      slicer.mrmlScene.EndImportEvent, slicer.mrmlScene.EndCloseEvent):
            self.sceneObservers.append(slicer.mrmlScene.AddObserver(event, self.scheduleRefresh))
        selection = slicer.app.applicationLogic().GetSelectionNode()
        self.selectionObserver = selection.AddObserver(vtk.vtkCommand.ModifiedEvent, self.scheduleRefresh)
        # the tool buttons follow the interaction mode (placement ended by a
        # right click), the developer button the Python console
        self.tool = "navigate"
        interaction = slicer.app.applicationLogic().GetInteractionNode()
        self.interactionObserver = interaction.AddObserver(interaction.InteractionModeChangedEvent,
                                                           self.onInteractionModeChanged)
        console = mainWindow.findChild("QDockWidget", "PythonConsoleDockWidget")
        if console is not None:
            console.connect("visibilityChanged(bool)", self.onConsoleVisibilityChanged)
            self.onConsoleVisibilityChanged(console.visible)
        # no menu of the toolbars and docks of Slicer on a right click
        self.saved["contextMenuPolicy"] = mainWindow.contextMenuPolicy
        mainWindow.setContextMenuPolicy(qt.Qt.PreventContextMenu)
        # shortcuts of the Workbench (removed when leaving it)
        self.shortcuts = []
        for keys, callback in (("F1", lambda: About.showAbout()),
                               ("Ctrl+Shift+T", self.header.toggleTheme),
                               ("Space", self.scrubberBar.scrubber.togglePlaying)):
            shortcut = qt.QShortcut(qt.QKeySequence(keys), mainWindow)
            shortcut.setObjectName(f"WorkbenchShortcut_{keys}")
            shortcut.connect("activated()", callback)
            self.shortcuts.append(shortcut)
        self.active = True
        self.refresh()
        self.arrangeViews()

    def styleMainWindow(self):
        """Separators between the views and the panels (dock separators of
        the main window, and the gaps next to the docks of fixed size, which
        show the main window) in the background colour of the Workbench,
        instead of Slicer's light grey."""
        bg = theme().color("bg").name()
        hover = theme().color("line-strong").name()
        slicer.util.mainWindow().setStyleSheet(
            f"QMainWindow {{ background: {bg}; }}"
            f"QMainWindow::separator {{ background: {bg}; width: 6px; height: 6px; }}"
            f"QMainWindow::separator:hover {{ background: {hover}; }}")

    def scheduleRefresh(self, caller=None, event=None):
        if event == "EndCloseEvent":
            # node IDs are reused by the next data
            self.automaticContours.done.clear()
        if event in ("EndCloseEvent", "EndImportEvent"):
            self.style3DViews()
        self.refreshTimer.start()

    def runAutomaticContours(self):
        """HI contours over the images that overlap an HI dataset (new pairs
        only)."""
        if not self.active or not self.automaticContours.candidates():
            return
        updated = self.automaticContours.run()
        if updated:
            self.statusBar.showMessage(f"HI contours drawn on {', '.join(updated)}")
            self.refresh()

    def refresh(self):
        if not self.active:
            return
        if self.automaticContours.candidates():
            self.automaticContoursTimer.start()
        volume = self.activeVolume()
        self.drawer.refresh()
        self.scrubberBar.setVolume(volume)
        self.setActiveView(self.activeViewKey)
        self.statusBar.clear()
        self.inspector.setVolume(volume)
        self.views.refresh()
        self.visibility.update()
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
        self.automaticContoursTimer.stop()
        for observer in self.sceneObservers:
            slicer.mrmlScene.RemoveObserver(observer)
        self.sceneObservers = []
        slicer.app.applicationLogic().GetSelectionNode().RemoveObserver(self.selectionObserver)
        slicer.app.applicationLogic().GetInteractionNode().RemoveObserver(self.interactionObserver)
        mainWindow = slicer.util.mainWindow()
        console = mainWindow.findChild("QDockWidget", "PythonConsoleDockWidget")
        if console is not None:
            console.disconnect("visibilityChanged(bool)", self.onConsoleVisibilityChanged)
        mainWindow.setContextMenuPolicy(self.saved.get("contextMenuPolicy", qt.Qt.DefaultContextMenu))
        for shortcut in self.shortcuts:
            shortcut.delete()
        self.shortcuts = []
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
        theme().disconnect("themeChanged()", self.styleMainWindow)
        mainWindow.setStyleSheet(self.saved.get("styleSheet", ""))
        mainWindow.restoreState(self.saved["state"])
        self.views.remove()
        self.visibility.exit()
        # default view node of the scene restored
        previousDefault = self.saved.pop("defaultViewNode", None)
        if previousDefault is not None:
            slicer.mrmlScene.AddDefaultNode(previousDefault)
        else:
            slicer.mrmlScene.AddDefaultNode(slicer.vtkMRMLViewNode())
        # backgrounds and boxes of the 3D views (after the view decorator)
        for nodeID, color, color2, boxVisible in self.saved["views"]:
            node = slicer.mrmlScene.GetNodeByID(nodeID)
            if node is not None:
                node.SetBackgroundColor(*color)
                node.SetBackgroundColor2(*color2)
                node.SetBoxVisible(boxVisible)

    def toggle(self):
        if self.active:
            self.exit()
        else:
            self.enter()
