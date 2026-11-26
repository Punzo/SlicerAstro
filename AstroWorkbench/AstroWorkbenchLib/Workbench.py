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
from . import Sections
from .Inspector import Inspector
from .Views import ViewDecorator, ViewVisibility
from . import Scenes

VIEW_COLOR = (7 / 255.0, 10 / 255.0, 18 / 255.0)
# Slicer's default gradient of the 3D views (vtkMRMLViewNode), in the light theme
LIGHT_VIEW_COLORS = ((0.7568627450980392, 0.7647058823529412, 0.9098039215686275),
                     (0.4549019607843137, 0.4705882352941176, 0.7450980392156863))


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


class ColorFunctionDrag:
    """The colour-function tool in the 3D views: a left drag moves the 3D
    colour function of the rendered cube along the data values (left-right,
    the whole range of the data across the view) and shifts its opacities
    (up-down). The camera does not move meanwhile."""

    EVENTS = ("LeftButtonPressEvent", "MouseMoveEvent", "LeftButtonReleaseEvent")

    def __init__(self, workbench):
        self.workbench = workbench
        self.observers = []
        self.drag = None

    def setEnabled(self, enabled):
        self.remove()
        if not enabled:
            return
        layoutManager = slicer.app.layoutManager()
        for index in range(layoutManager.threeDViewCount):
            view = layoutManager.threeDWidget(index).threeDView()
            interactor = view.interactor()
            viewNodeID = view.mrmlViewNode().GetID()
            for event in self.EVENTS:
                # before the interactor style of the view (it would rotate)
                tag = interactor.AddObserver(event, lambda caller, eventName, v=view, i=viewNodeID:
                                             self.onEvent(caller, eventName, v, i), 10.0)
                self.observers.append((interactor, tag))

    def remove(self):
        for interactor, tag in self.observers:
            interactor.RemoveObserver(tag)
        self.observers = []
        self.drag = None

    def abort(self, interactor):
        for observed, tag in self.observers:
            if observed is interactor:
                command = interactor.GetCommand(tag)
                if command is not None:
                    command.SetAbortFlag(1)

    def onEvent(self, interactor, eventName, view, viewNodeID):
        if eventName == "LeftButtonPressEvent":
            volume = self.workbench.visibility.renderedVolume(viewNodeID)
            displayNode = volume.GetAstroVolumeDisplayNode() if volume is not None else None
            if displayNode is None:
                return
            maximum = Astro.floatAttribute(volume, "DATAMAX", 1.0)
            minimum = Astro.floatAttribute(volume, "DATAMIN", 0.0)
            width = maximum - minimum if maximum > minimum else 1.0
            self.drag = {"volume": volume, "position": interactor.GetEventPosition(), "width": width,
                         "shift": displayNode.GetPresetOffset(), "opacity": displayNode.GetOpacityShift(),
                         "size": view.renderWindow().GetSize()}
            self.abort(interactor)
        elif eventName == "MouseMoveEvent" and self.drag is not None:
            drag = self.drag
            x, y = interactor.GetEventPosition()
            dx = (x - drag["position"][0]) / max(1, drag["size"][0])
            dy = (y - drag["position"][1]) / max(1, drag["size"][1])
            logic = slicer.modules.astrovolume.logic()
            logic.SetColorFunctionShift(drag["volume"], drag["shift"] + dx * drag["width"])
            logic.SetColorFunctionOpacityShift(drag["volume"], max(-1.0, min(1.0, drag["opacity"] + dy)))
            self.abort(interactor)
        elif eventName == "LeftButtonReleaseEvent" and self.drag is not None:
            self.drag = None
            self.abort(interactor)


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
        self.colorFunctionButton = iconButton(
            "colormap", "<b>Colour function</b><br>"
                        "Click this button, then left-drag in a view:<br>"
                        "&bull; <b>2D view</b>: left-right changes the contrast (window), "
                        "up-down the brightness (level) of the 2D colour function;<br>"
                        "&bull; <b>3D view</b>: left-right moves the 3D colour function along the data values, "
                        "up-down makes it more or less opaque.<br>"
                        "Inspect (Display and its 3D rendering) shows the new values. The pointer button returns to navigation. "
                        "All the shortcuts: About (F1).", checkable=True)
        self.colorFunctionButton.setObjectName("WorkbenchToolColorFunction")
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
        for button, separator in ((self.navigateButton, False), (self.crosshairButton, False),
                                  (self.colorFunctionButton, True),
                                  (self.pointsButton, False), (self.lineButton, False), (self.curveButton, False),
                                  (self.closedCurveButton, False), (self.angleButton, False),
                                  (self.roiButton, False)):
            self.toolGroup.addButton(button)
            layout.addWidget(button)
            if separator:
                layout.addWidget(self.separator())
        self.navigateButton.connect("clicked()", lambda: self.workbench.setInteraction("navigate"))
        self.crosshairButton.connect("clicked()", lambda: self.workbench.setInteraction("crosshair"))
        self.colorFunctionButton.connect("clicked()", lambda: self.workbench.setInteraction("colorFunction"))
        self.lineButton.connect("clicked()", lambda: self.workbench.setInteraction("line"))
        self.pointsButton.connect("clicked()", lambda: self.workbench.setInteraction("points"))
        self.curveButton.connect("clicked()", lambda: self.workbench.setInteraction("curve"))
        self.closedCurveButton.connect("clicked()", lambda: self.workbench.setInteraction("closedCurve"))
        self.angleButton.connect("clicked()", lambda: self.workbench.setInteraction("angle"))
        self.roiButton.connect("clicked()", lambda: self.workbench.setInteraction("roi"))
        layout.addStretch(1)

        # layout of the views: toolbar buttons like the tools, one checked
        layout.addWidget(self.separator())
        self.layoutButton = LayoutSwitch(self.workbench.setViewLayout)
        layout.addWidget(self.layoutButton)
        layout.addWidget(self.separator())
        self.themeButton = iconButton("sun", "Light or dark theme")
        self.themeButton.setObjectName("WorkbenchThemeButton")
        self.themeButton.connect("clicked()", self.toggleTheme)
        layout.addWidget(self.themeButton)
        helpButton = iconButton("help", "About SlicerAstro: shortcuts, credits, documentation (F1)")
        helpButton.setObjectName("AboutButton")
        helpButton.connect("clicked()", lambda: About.showAbout())
        layout.addWidget(helpButton)
        settingsButton = iconButton("settings", "Settings of SlicerAstro: theme, saving, 3D views, loading")
        settingsButton.setObjectName("SettingsButton")
        settingsButton.connect("clicked()", self.showSettings)
        layout.addWidget(settingsButton)
        self.developerButton = iconButton("terminal", "Developer: Python console", checkable=True)
        self.developerButton.setObjectName("DeveloperButton")
        self.developerButton.connect("toggled(bool)", self.workbench.setPythonConsoleVisible)
        layout.addWidget(self.developerButton)
        self.fullSlicerButton = iconButton("exitWorkbench", "Leave the Workbench: full Slicer interface (Ctrl+Shift+W)")
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

    def showSettings(self):
        """The settings of SlicerAstro (a dialog of the main window)."""
        dialog = slicer.qSlicerAstroSettingsDialog(slicer.util.mainWindow())
        dialog.exec()
        return dialog

    def toggleTheme(self):
        theme().setMode("light" if theme().dark else "dark")

    def updateThemeIcon(self):
        self.themeButton.setIcon(theme().icon("sun" if theme().dark else "moon"))

    def cleanup(self):
        theme().disconnect("themeChanged()", self.updateThemeIcon)



class LayoutSwitch(qt.QWidget):
    """Layouts of the views as checkable toolbar buttons (one checked):
    one view, three views, four views, views and a plot. currentIndex /
    setCurrentIndex as a segmented control; onChanged(index) is called when
    the layout changes."""

    LAYOUTS = (("layout1", "One view: the channel map"),
               ("layout3", "Three views: channel map, moment 0 and 3D"),
               ("layout4", "Four views: channel map, 3D, position-velocity and moment 0"),
               ("layoutPlot", "Views and a plot: channel map and 3D above, the spectrum of the data below "
                              "(or the profiles made in Process)"))

    def __init__(self, onChanged, parent=None):
        qt.QWidget.__init__(self, parent)
        self.setObjectName("WorkbenchLayoutButton")
        self.toolTip = "Layout of the views."
        self.onChanged = onChanged
        self.index = -1
        layout = qt.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)
        self.group = qt.QButtonGroup(self)
        self.group.setExclusive(True)
        self.buttons = []
        for index, (icon, toolTip) in enumerate(self.LAYOUTS):
            button = iconButton(icon, toolTip, checkable=True)
            button.setObjectName(f"WorkbenchLayout_{icon}")
            self.group.addButton(button, index)
            layout.addWidget(button)
            self.buttons.append(button)
        self.group.connect("idClicked(int)", self.setCurrentIndex)

    def count(self):
        return len(self.buttons)

    def button(self, index):
        return self.buttons[index]

    @property
    def currentIndex(self):
        return self.index

    def setCurrentIndex(self, index, notify=True):
        if index < 0 or index >= len(self.buttons):
            return
        button = self.buttons[index]
        if not button.checked:
            button.blockSignals(True)
            button.checked = True
            button.blockSignals(False)
        if index == self.index:
            return
        self.index = index
        if notify and self.onChanged is not None:
            self.onChanged(index)

    def clear(self):
        """No button checked: a layout of Slicer that is not in the switch
        (e.g. a maximized view)."""
        self.group.setExclusive(False)
        for button in self.buttons:
            button.blockSignals(True)
            button.checked = False
            button.blockSignals(False)
        self.group.setExclusive(True)
        self.index = -1

class Rail(qt.QFrame):
    """Activities: home, scenes, data, sky search (VO)."""

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
        # the home page (welcome dashboard) in place of the views and panels
        self.homeButton = qt.QToolButton()
        self.homeButton.setProperty("astroRole", "rail")
        self.homeButton.setObjectName("Rail_home")
        self.homeButton.setToolButtonStyle(qt.Qt.ToolButtonTextUnderIcon)
        self.homeButton.setIcon(theme().icon("module-sample-data", "muted"))
        self.homeButton.setIconSize(qt.QSize(20, 20))
        self.homeButton.text = "Home"
        self.homeButton.checkable = True
        self.homeButton.toolTip = ("Home: open data, recent files and scenes, sample data. "
                                   "Data or Scenes: back to the views.")
        self.group.addButton(self.homeButton)
        self.homeButton.connect("clicked()", lambda: self.workbench.setHomeVisible(True))
        layout.addWidget(self.homeButton, 0, qt.Qt.AlignHCenter)
        self.buttons["home"] = self.homeButton
        for name, icon, text, enabled in (("scenes", "archive", "Scenes", True),
                                          ("data", "layers", "Data", True),
                                          ("sky", "globe", "Sky", False)):
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
                "scenes": "Scenes: save the scene with a screenshot of the views; open a saved one.",
            }.get(name, f"{text}: coming with the next steps of SlicerAstro 2.0")
            self.group.addButton(button)
            layout.addWidget(button, 0, qt.Qt.AlignHCenter)
            self.buttons[name] = button
            button.connect("clicked()", lambda n=name: self.workbench.showDrawerPage(n))
        self.buttons["data"].checked = True
        layout.addStretch(1)


class ElidedLabel(qt.QLabel):
    """A label elided to its width (full text in the tooltip); it never
    widens its parent."""

    def __init__(self, text="", mode=qt.Qt.ElideRight, parent=None):
        qt.QLabel.__init__(self, parent)
        self.fullText = text
        self.mode = mode
        self.setSizePolicy(qt.QSizePolicy.Ignored, qt.QSizePolicy.Preferred)
        self.setMinimumWidth(24)
        self.setText(text)

    def resizeEvent(self, event):
        qt.QLabel.resizeEvent(self, event)
        metrics = qt.QFontMetrics(self.font)
        elided = metrics.elidedText(self.fullText, self.mode, max(10, self.width))
        if elided != self.text:
            self.setText(elided)
        self.toolTip = self.fullText if elided != self.fullText else ""


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
        openPartButton = qt.QPushButton()
        openPartButton.setObjectName("OpenPartButton")
        openPartButton.toolTip = ("Open part of a FITS file: choose the extension (e.g. DATA or STAT of a "
                                  "MUSE cube) and the pixel and channel ranges; only those voxels are read.")
        openPartButton.setIcon(theme().icon("crop", "text"))
        openPartButton.connect("clicked()", self.openPart)
        titleRow.addWidget(openPartButton)
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
        self.emptyLabel = qt.QLabel("No data yet: open a FITS file, or a sample from Home.")
        self.emptyLabel.setProperty("astroRole", "label")
        self.emptyLabel.wordWrap = True
        self.cardsLayout.addWidget(self.emptyLabel)
        self.cardsLayout.addStretch(1)
        scroll.setWidget(self.cardsWidget)
        pageLayout.addWidget(scroll, 1)
        self.stack.addWidget(page)

        # scenes
        self.scenesPage = Scenes.ScenesPage(workbench)
        self.scenesPage.setObjectName("ScenesPage")
        self.stack.addWidget(self.scenesPage)

    def showPage(self, name):
        if name == "scenes":
            self.scenesPage.refresh()
            self.stack.setCurrentIndex(1)
        else:
            self.stack.setCurrentIndex(0)

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
            card.connect("clicked()", lambda v=volume: self.workbench.onDatasetClicked(v))
            card.connect("productClicked(QString)", self.workbench.showProduct)
            card.connect("deleteRequested()", lambda v=volume: self.deleteDataset(v))
            self.cardsLayout.insertWidget(len(self.cards), card)
            self.cards.append(card)
        self.applyFilter()

    def deleteDataset(self, volume, ask=True, withProducts=None):
        """Remove a dataset from the scene; its products (moment maps, masks,
        catalogues...) too if chosen. Returns True if removed."""
        # the listed products and everything else made from the dataset
        # (source points, profiles, masks and contours of the modules)
        products = Astro.derivedNodes(volume, slicer.mrmlScene)
        if ask:
            box = qt.QMessageBox(slicer.util.mainWindow())
            theme().applyToDialog(box)
            box.setWindowTitle("Delete dataset")
            box.setIcon(qt.QMessageBox.Question)
            box.setText(f"Delete {volume.GetName()} from the scene?")
            allButton = None
            if products:
                listed = len(Astro.productsOf(volume, slicer.mrmlScene))
                box.setInformativeText(f"It has {listed} product{'s' if listed != 1 else ''} "
                                       "(and their masks, contours and catalogue points).")
                allButton = box.addButton("Dataset and products", qt.QMessageBox.DestructiveRole)
            onlyButton = box.addButton("Dataset only" if products else "Delete", qt.QMessageBox.AcceptRole)
            box.addButton(qt.QMessageBox.Cancel)
            box.exec_()
            if box.clickedButton() not in (allButton, onlyButton) or box.clickedButton() is None:
                return False
            withProducts = allButton is not None and box.clickedButton() == allButton
        if withProducts:
            nodes = products
        else:
            # its own contours go with it (they have its grid)
            contours = slicer.modules.astrovolume.logic().GetContoursSegmentationForVolume(volume, False)
            nodes = [contours] if contours is not None else []
        # the products first, the dataset last: the panels showing the
        # dataset change their input once
        for node in nodes + [volume]:
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

    def openPart(self, path=None):
        """Opens a part of a FITS file: the file (asked if not given), then
        the extension and the pixel and channel ranges; only those voxels are
        read. Returns the loaded dataset (None: cancelled)."""
        settingsKey = "AstroWorkbench/OpenPartDirectory"
        if not path:
            directory = qt.QSettings().value(settingsKey, "") or ""
            path = qt.QFileDialog.getOpenFileName(
                slicer.util.mainWindow(), "Open part of a FITS file", directory,
                "FITS files (*.fits *.fit *.fts *.fits.gz *.fz);;All files (*)")
            if not path:
                return None
            qt.QSettings().setValue(settingsKey, os.path.dirname(path))
        dialog = slicer.qSlicerAstroSubCubeDialog(slicer.util.mainWindow())
        if not dialog.setFile(path, 0):
            slicer.util.errorDisplay(f"{os.path.basename(path)} has no image data that can be loaded.",
                                     windowTitle="Open part of a FITS file")
            return None
        if not dialog.exec_():
            return None
        return dialog.load()

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
            channel = Astro.channelOfSlice(self.volume, self.sliceNode)
            self.scrubber.setCurrentIndex(channel)
            self.syncChannel(channel)
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
            self.syncChannel(index)
        finally:
            self.updating = False

    def syncChannel(self, channel):
        """The other channel maps (XY views) showing data on the same voxels
        (the cube, its smoothed, blanked... copies) follow the channel: the
        views stay linked."""
        layoutManager = slicer.app.layoutManager()
        if layoutManager is None or self.volume is None:
            return
        for name in layoutManager.sliceViewNames():
            widget = layoutManager.sliceWidget(name)
            sliceNode = widget.mrmlSliceNode() if widget is not None else None
            if sliceNode is None or sliceNode is self.sliceNode or sliceNode.GetOrientation() != "XY":
                continue
            backgroundID = widget.mrmlSliceCompositeNode().GetBackgroundVolumeID()
            background = slicer.mrmlScene.GetNodeByID(backgroundID) if backgroundID else None
            if background is None or Astro.dimensions(background) < 3 or not Astro.sameGrid(background, self.volume):
                continue
            if Astro.channelOfSlice(background, sliceNode) != channel:
                Astro.setSliceToChannel(background, sliceNode, channel)

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
        qt.QTimer.singleShot(milliseconds, lambda: self.clearMessage(text))

    def clearMessage(self, text):
        try:
            if self.hint.text == text:
                self.hint.text = ""
        except ValueError:
            # the status bar was deleted (Workbench left)
            pass

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
        self.observedSegmentations = {}
        self.refreshTimer = qt.QTimer()
        self.refreshTimer.setSingleShot(True)
        self.refreshTimer.setInterval(200)
        self.refreshTimer.connect("timeout()", self.refresh)
        self.inspector = None
        self.scrubberBar = None
        self.shortcuts = []
        self.views = ViewDecorator(self)
        self.visibility = ViewVisibility()
        self.visibility.onUpdated = self.refresh3DViewBars
        self.colorFunctionDrag = ColorFunctionDrag(self)
        self.activeViewKey = "Red"
        self.home = None
        self.homeShown = None
        self.datasetCount = 0

    # ------------------------------------------------------------------
    def activeVolumeID(self):
        selection = slicer.app.applicationLogic().GetSelectionNode()
        return selection.GetActiveVolumeID() if selection else None

    def activeVolume(self):
        volumeID = self.activeVolumeID()
        volume = slicer.mrmlScene.GetNodeByID(volumeID) if volumeID else None
        return volume if Astro.isAstroVolume(volume) else None

    def refresh3DViewBars(self):
        """The eye and the data named in the bars of the 3D views (a
        rendering shown or hidden in Inspect, a panel or the logic)."""
        for key, bar in self.views.bars.items():
            if key.startswith("3D"):
                bar.refresh()

    def onDatasetClicked(self, volume):
        """A dataset card clicked: another dataset opens in the views; the
        active one gets its views back (its products shown in its place go
        away, the zoom, channel and cameras are kept)."""
        self.setHomeVisible(False)
        if volume is self.activeVolume():
            # the dataset back in every view: products shown in its place and
            # data dropped on single views are replaced (zoom, channel and
            # cameras kept for the views on its grid)
            slicer.modules.astrovolume.logic().HideProductsInViews(volume)
            self.visibility.extra = {}
            self.arrangeViews()
            self.views.refresh()
            return
        self.activate(volume)

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
        if key.startswith("Plot"):
            widget = layoutManager.plotWidget(int(key[4:]))
            return widget.mrmlPlotViewNode().GetID() if widget is not None else None
        widget = layoutManager.sliceWidget(key)
        return widget.mrmlSliceNode().GetID() if widget is not None else None

    def setActiveView(self, key):
        """The view clicked last (amber border): the scrubber moves it when
        it shows a cube."""
        self.activeViewKey = key
        self.views.setActive(key)
        if key.startswith("3D") or key.startswith("Plot") or self.scrubberBar is None:
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
        self.setHomeVisible(False)
        node = slicer.mrmlScene.GetNodeByID(nodeID)
        viewNodeID = self.viewNodeID(key)
        if node is None or viewNodeID is None:
            return
        if key.startswith("Plot"):
            # a cube dropped on the plot: its spectrum
            if Astro.isAstroVolume(node) and Astro.dimensions(node) >= 3:
                self.showSpectrumPlot(node)
            self.setActiveView(key)
            return
        if node.IsA("vtkMRMLSegmentationNode"):
            self.visibility.addToView(node, viewNodeID)
            self.setMaskShown(node, True)
            self.setActiveView(key)
            return
        if not Astro.isAstroVolume(node):
            return
        if key.startswith("3D"):
            if Astro.dimensions(node) < 3:
                return
            # data on the sky grid of what the view renders (e.g. a smoothed
            # copy of the cube): the camera is kept
            rendered = self.visibility.renderedVolume(viewNodeID)
            self.visibility.renderInView(node, viewNodeID,
                                         centerCamera=rendered is None or not Astro.sameSkyGrid(rendered, node))
        else:
            sliceNode = slicer.mrmlScene.GetNodeByID(viewNodeID)
            composite = slicer.app.layoutManager().sliceWidget(key).mrmlSliceCompositeNode()
            orientation = sliceNode.GetOrientation() if Astro.dimensions(node) >= 3 else "XY"
            self.showInSliceView(sliceNode, composite, node, orientation)
        # dropped data of another dataset: that dataset becomes the active
        # one (a product keeps its source active: the scrubber, Inspect and
        # the other views stay with the source)
        source = Astro.sourceOf(node)
        if self.activeVolume() is None or (Astro.dimensions(source) >= 3 and source is not self.activeVolume()):
            slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(source.GetID())
        self.visibility.update()
        self.setActiveView(key)

    def showInSliceView(self, sliceNode, composite, node, orientation=None, clearForeground=True):
        """Data shown in a slice view. Data on the sky grid of what the view
        shows (the dataset and its products) keep the view (zoom, pan and
        channel; a 2D map moves the view to its plane only); other data: the
        orientation is set and the view fitted."""
        previousID = composite.GetBackgroundVolumeID()
        previous = slicer.mrmlScene.GetNodeByID(previousID) if previousID else None
        composite.SetBackgroundVolumeID(node.GetID())
        if clearForeground:
            composite.SetForegroundVolumeID(None)
        sliceLogic = slicer.app.applicationLogic().GetSliceLogic(sliceNode)
        orientation = orientation or sliceNode.GetOrientation()
        if previous is node and sliceNode.GetOrientation() == orientation:
            # already shown so: the view is kept
            return
        if (previous is not None and previous is not node and sliceNode.GetOrientation() == orientation
                and Astro.sameSkyGrid(previous, node)):
            if Astro.sameGrid(previous, node) or sliceLogic is None:
                return
            if Astro.dimensions(node) >= 3 and orientation == "XY":
                # back to a cube: the channel of the scrubber
                scrubber = self.scrubberBar
                channel = scrubber.scrubber.currentIndex if scrubber is not None and scrubber.scrubber.count() > 0 else None
                if channel is not None and channel >= 0:
                    Astro.setSliceToChannel(node, sliceNode, channel)
                    return
            # a map: its plane (zoom and pan kept)
            bounds = [0.0] * 6
            sliceLogic.GetVolumeSliceBounds(node, bounds)
            sliceLogic.SetSliceOffset(0.5 * (bounds[4] + bounds[5]))
            return
        # another orientation first: the slice is recomputed for the new data
        sliceNode.SetOrientation("XZ" if orientation == "XY" else "XY")
        sliceNode.SetOrientation(orientation)
        if sliceLogic is not None:
            sliceLogic.FitSliceToAll()

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
            # shown (also when its segments were hidden, e.g. the eye of the
            # Mask tab), or hidden if it is shown
            self.setMaskShown(node, not self.isMaskShown(node))
            return
        if node.IsA("vtkMRMLTableNode"):
            # a catalogue: its sources in the Sources tab
            volume = Astro.catalogueVolume(node)
            if volume is not None and volume is not self.activeVolume():
                self.activate(volume)
            self.inspector.sourcesWidget.setVolumeNode(self.activeVolume())
            self.inspector.showSources(node)
            return
        # data: in place of the dataset in its views, or back (the same as
        # the Show buttons of the panels; vtkSlicerAstroVolumeLogic)
        if not Astro.isAstroVolume(node):
            return
        dataset = self.activeVolume()
        source = Astro.sourceOf(node)
        if dataset is None or (source is not dataset and source is not node):
            dataset = source
        slicer.modules.astrovolume.logic().ToggleProductInViews(node, dataset)
        self.visibility.update()
        self.views.refresh()

    @staticmethod
    def isMaskShown(segmentation):
        """The mask is shown: its display and at least one of its segments."""
        displayNode = segmentation.GetDisplayNode()
        if displayNode is None or not displayNode.GetVisibility():
            return False
        ids = segmentation.GetSegmentation().GetSegmentIDs() if segmentation.GetSegmentation() else []
        return any(displayNode.GetSegmentVisibility(segmentID) for segmentID in ids)

    @staticmethod
    def setMaskShown(segmentation, shown):
        displayNode = segmentation.GetDisplayNode()
        if displayNode is None:
            segmentation.CreateDefaultDisplayNodes()
            displayNode = segmentation.GetDisplayNode()
        if displayNode is None:
            return
        if shown:
            displayNode.SetVisibility(True)
            displayNode.SetAllSegmentsVisibility(True)
        else:
            displayNode.SetVisibility(False)

    def fitSliceView(self, sliceNode):
        """Fit one view to its data, keeping its orientation (the fit of the
        application logic resets the orientations of all the views)."""
        sliceLogic = slicer.app.applicationLogic().GetSliceLogic(sliceNode) if sliceNode is not None else None
        if sliceLogic is not None:
            sliceLogic.FitSliceToAll()

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

    # the layout with a plot of SlicerAstro (vtkSlicerAstroVolumeLogic):
    # channel map and 3D view above, plot view below
    PLOT_VIEWS = 3502

    def layouts(self):
        """Layouts of the switch: one view, three views (channel map,
        moment 0, 3D), four views, and views with a plot."""
        layoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode()
        if not layoutNode.IsLayoutDescription(self.THREE_VIEWS):
            layoutNode.AddLayoutDescription(self.THREE_VIEWS, self.THREE_VIEWS_LAYOUT)
        slicer.vtkSlicerAstroVolumeLogic.AddPlotLayout(layoutNode)
        return (slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpRedSliceView, self.THREE_VIEWS,
                slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView, self.PLOT_VIEWS)

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

    def toggleMaximized(self, oneUp):
        """Maximize button of a view: the view alone; clicked again, back to
        the layout before."""
        layoutManager = slicer.app.layoutManager()
        arrangement = layoutManager.layoutLogic().GetLayoutNode().GetViewArrangement()
        if arrangement == oneUp:
            previous = getattr(self, "layoutBeforeMaximize", None)
            layoutManager.setLayout(previous if previous is not None
                                    else slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)
        else:
            self.layoutBeforeMaximize = arrangement
            layoutManager.setLayout(oneUp)
        self.styleViews()

    def onLayoutChanged(self, arrangement=None):
        """Layout changed elsewhere (maximize buttons of the views, "Open in
        plot view" of Profiles): view bars on the new views, the layout
        switch follows."""
        if not self.active:
            return
        self.styleViews()
        # the colour-function tool on the 3D views of the new layout
        if getattr(self, "tool", None) == "colorFunction":
            self.colorFunctionDrag.setEnabled(True)
        layouts = self.layouts()
        arrangement = slicer.app.layoutManager().layoutLogic().GetLayoutNode().GetViewArrangement()
        switch = self.header.layoutButton
        if arrangement in layouts:
            switch.setCurrentIndex(layouts.index(arrangement), notify=False)
        else:
            switch.clear()

    def showSpectrumPlot(self, volume):
        """The plot view of the layout with a plot: the profiles of the cube
        computed in Process (Profiles); empty until one is computed."""
        layoutManager = slicer.app.layoutManager()
        arrangement = layoutManager.layoutLogic().GetLayoutNode().GetViewArrangement()
        if arrangement not in (self.PLOT_VIEWS, slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpPlotView):
            return None
        plotWidget = layoutManager.plotWidget(0) if layoutManager.plotViewCount > 0 else None
        if plotWidget is None:
            return None
        chart = None
        profiles = slicer.util.getFirstNodeByClassByName("vtkMRMLPlotChartNode", "ProfileChart")
        if volume is not None and profiles is not None:
            for index in range(profiles.GetNumberOfPlotSeriesNodes()):
                series = profiles.GetNthPlotSeriesNode(index)
                if series is not None and series.GetName().startswith(volume.GetName() + "_Profile"):
                    chart = profiles
                    break
        viewNode = plotWidget.mrmlPlotViewNode()
        chartID = chart.GetID() if chart is not None else None
        # another chart shown in the view (e.g. the rings of Modeling) stays:
        # only an empty view or the profiles are replaced
        currentID = viewNode.GetPlotChartNodeID()
        current = slicer.mrmlScene.GetNodeByID(currentID) if currentID else None
        if current is not None and current is not profiles:
            return current
        if currentID != chartID:
            viewNode.SetPlotChartNodeID(chartID)
        return chart

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
            # views already on the grid of the data keep zoom, pan and
            # channel; the others are oriented and fitted
            self.showInSliceView(sliceNode, composite, node, orientation)
        if cube:
            threeD = self.viewNodeID("3D0")
            if threeD is not None:
                rendered = self.visibility.renderedVolume(threeD)
                self.visibility.renderInView(volume, threeD,
                                             centerCamera=rendered is None or not Astro.sameSkyGrid(rendered, volume))
        self.visibility.update()
        if cube:
            self.showSpectrumPlot(volume)
        if cube and self.scrubberBar is not None:
            self.setActiveView("Red")
            self.scrubberBar.onIndexChanged(self.scrubberBar.scrubber.currentIndex)

    PLACE_CLASSES = {"points": "vtkMRMLMarkupsFiducialNode", "line": "vtkMRMLMarkupsLineNode",
                     "curve": "vtkMRMLMarkupsCurveNode", "closedCurve": "vtkMRMLMarkupsClosedCurveNode",
                     "angle": "vtkMRMLMarkupsAngleNode", "roi": "vtkMRMLMarkupsROINode"}

    def toolButtons(self):
        header = self.header
        return {"navigate": header.navigateButton, "crosshair": header.crosshairButton,
                "colorFunction": header.colorFunctionButton,
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
        elif self.tool == "colorFunction" and interaction.GetCurrentInteractionMode() != interaction.AdjustWindowLevel:
            # window/level ended elsewhere (e.g. a placement started)
            self.colorFunctionDrag.setEnabled(False)
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
        elif mode == "colorFunction":
            # 2D views: window/level of Slicer; 3D views: the drag below
            interaction.SetCurrentInteractionMode(slicer.vtkMRMLInteractionNode.AdjustWindowLevel)
        else:
            interaction.SetCurrentInteractionMode(slicer.vtkMRMLInteractionNode.ViewTransform)
        self.colorFunctionDrag.setEnabled(mode == "colorFunction")

    def styleViews(self):
        """Views on the ink background, with the view bars of the Workbench
        instead of Slicer's view controllers and vertical slice sliders."""
        self.style3DViews()
        self.views.install()

    def style3DViews(self):
        """3D views: the dark sky in the dark theme, Slicer's gradient in the
        light theme, and no bounding box. The default view node of the scene
        has the same values, so the views keep them when a scene is closed or
        loaded (the view nodes are reset to the default node)."""
        color, color2 = (VIEW_COLOR, VIEW_COLOR) if theme().dark else LIGHT_VIEW_COLORS
        scene = slicer.mrmlScene
        if "defaultViewNode" not in self.saved:
            self.saved["defaultViewNode"] = scene.GetDefaultNodeByClass("vtkMRMLViewNode")
        default = slicer.vtkMRMLViewNode()
        previous = self.saved["defaultViewNode"]
        if previous is not None:
            default.Copy(previous)
        default.SetBackgroundColor(*color)
        default.SetBackgroundColor2(*color2)
        default.SetBoxVisible(False)
        scene.AddDefaultNode(default)
        for viewNode in slicer.util.getNodesByClass("vtkMRMLViewNode"):
            if tuple(viewNode.GetBackgroundColor()) != color:
                viewNode.SetBackgroundColor(*color)
            if tuple(viewNode.GetBackgroundColor2()) != color2:
                viewNode.SetBackgroundColor2(*color2)
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
        self.leftPanel = left
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
        # a line set in Info (wavelengths to velocities): the axis of the
        # scrubber, the view bars and the chips again
        self.inspector.infoWidget.connect("spectralAxisChanged()", self.onSpectralAxisChanged)
        self.statusBar = StatusBar(self)
        self.toolbar(self.statusBar, qt.Qt.BottomToolBarArea, "WorkbenchStatusBar")
        for widget in (self.header, left, self.inspector, self.scrubberBar, self.statusBar):
            theme().applyTo(widget)

        # the home page: the welcome dashboard over the views (at start
        # without data, and from the rail)
        self.home = slicer.qSlicerAstroWelcomeWidget()
        self.home.setObjectName("WorkbenchHome")
        self.home.wide = True
        self.home.workbenchMode = True
        self.home.setOverlayTarget(mainWindow.centralWidget())
        self.home.connect("dataLoaded()", lambda: self.setHomeVisible(False))
        self.home.connect("scenesRequested()", self.openScenes)
        self.datasetCount = len(slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode"))
        self.setHomeVisible(self.datasetCount == 0)

        self.header.layoutButton.setCurrentIndex(self.layoutIndex())
        self.visibility.enter()
        self.saved["styleSheet"] = mainWindow.styleSheet
        self.styleMainWindow()
        theme().connect("themeChanged()", self.styleMainWindow)
        theme().connect("themeChanged()", self.style3DViews)
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
        slicer.app.layoutManager().connect("layoutChanged(int)", self.onLayoutChanged)
        console = mainWindow.findChild("QDockWidget", "PythonConsoleDockWidget")
        if console is not None:
            console.connect("visibilityChanged(bool)", self.onConsoleVisibilityChanged)
            self.onConsoleVisibilityChanged(console.visible)
        # the menus (e.g. the right-click menus of the views) and the dialogs
        # (e.g. Add data, file dialogs) in the theme; the Python console too
        theme().setPopupsThemed(True)
        self.styleConsole()
        theme().connect("themeChanged()", self.styleConsole)
        # no menu of the toolbars and docks of Slicer on a right click
        self.saved["contextMenuPolicy"] = mainWindow.contextMenuPolicy
        mainWindow.setContextMenuPolicy(qt.Qt.PreventContextMenu)
        # shortcuts of the Workbench (removed when leaving it)
        self.shortcuts = []
        # (keys listed in About › Shortcuts from the same constants)
        for keys, callback in ((About.KEY_ABOUT, lambda: About.showAbout()),
                               (About.KEY_THEME, self.header.toggleTheme),
                               (About.KEY_PLAY, self.scrubberBar.scrubber.togglePlaying)):
            shortcut = qt.QShortcut(qt.QKeySequence(keys), mainWindow)
            shortcut.setObjectName(f"WorkbenchShortcut_{keys}")
            shortcut.connect("activated()", callback)
            self.shortcuts.append(shortcut)
        self.active = True
        self.refresh()
        self.arrangeViews()

    def setHomeVisible(self, visible):
        """The home page (welcome dashboard) in place of the views, without
        the Datasets drawer, the inspector and the scrubber (the rail stays);
        hidden: the views and the panels back."""
        if self.home is None or self.homeShown == visible:
            return
        self.homeShown = visible
        mainWindow = slicer.util.mainWindow()
        self.drawer.visible = not visible
        self.leftPanel.setMinimumWidth(60 if visible else 360)
        for name in ("WorkbenchInspector", "WorkbenchScrubber"):
            dock = mainWindow.findChild("QDockWidget", name)
            if dock is not None:
                dock.visible = not visible
        left = mainWindow.findChild("QDockWidget", "WorkbenchDatasets")
        if left is not None:
            # only the rail on the home page (the dock keeps its width otherwise)
            left.setMaximumWidth(60 if visible else 16777215)
            self.leftPanel.setMaximumWidth(60 if visible else 16777215)
            # the dock geometry is applied again (it may keep its width)
            left.hide()
            left.show()
            mainWindow.layout().invalidate()
            mainWindow.resizeDocks([left], [60 if visible else 360], qt.Qt.Horizontal)
        # shown over the views (raised by the widget)
        self.home.visible = visible
        button = self.rail.buttons["home" if visible else "data"]
        if visible or self.rail.homeButton.checked:
            button.checked = True
        if not visible:
            self.drawer.showPage("scenes" if self.rail.buttons["scenes"].checked else "data")

    def showDrawerPage(self, name):
        """A page of the Datasets drawer (Data, Scenes), with the views."""
        self.setHomeVisible(False)
        self.rail.buttons[name].checked = True
        self.drawer.showPage(name)

    def isHomeVisible(self):
        return self.home is not None and bool(self.homeShown)

    def openScenes(self):
        self.showDrawerPage("scenes")

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

    def observeSegmentations(self):
        """Segments added or removed (e.g. in the base mask of a cube): the
        products of the datasets change."""
        for segmentation in slicer.util.getNodesByClass("vtkMRMLSegmentationNode"):
            if segmentation.GetID() in self.observedSegmentations:
                continue
            tags = [segmentation.AddObserver(event, self.scheduleRefresh)
                    for event in (slicer.vtkSegmentation.SegmentAdded, slicer.vtkSegmentation.SegmentRemoved)]
            self.observedSegmentations[segmentation.GetID()] = (segmentation, tags)

    def unobserveSegmentations(self):
        for segmentation, tags in self.observedSegmentations.values():
            for tag in tags:
                segmentation.RemoveObserver(tag)
        self.observedSegmentations = {}

    def onSpectralAxisChanged(self):
        self.scrubberBar.volume = None
        self.scheduleRefresh()

    def scheduleRefresh(self, caller=None, event=None):
        if event in ("EndCloseEvent", "EndImportEvent"):
            self.style3DViews()
        if event == "EndCloseEvent":
            self.unobserveSegmentations()
        if self.active:
            self.observeSegmentations()
        self.refreshTimer.start()

    def refresh(self):
        if not self.active:
            return
        # new data (loaded or made by a script) while the home page is shown:
        # the views
        count = len(slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode"))
        if count > self.datasetCount and self.isHomeVisible():
            self.setHomeVisible(False)
        self.datasetCount = count
        volume = self.activeVolume()
        self.drawer.refresh()
        self.scrubberBar.setVolume(volume)
        self.setActiveView(self.activeViewKey)
        self.statusBar.clear()
        self.inspector.setVolume(volume)
        # a profile computed meanwhile: in the plot view
        self.showSpectrumPlot(volume)
        self.views.refresh()
        self.visibility.update()
        if volume is not None:
            # the active dataset (its object and telescope in the tooltip:
            # OBJECT is often a survey field name, unclear on its own)
            self.header.contextLabel.text = volume.GetName()
            details = [("Object", Astro.attribute(volume, "OBJECT", "").strip()),
                       ("Telescope", Astro.attribute(volume, "TELESCOP", "").strip())]
            self.header.contextLabel.toolTip = "\n".join(
                [f"Active dataset: {volume.GetName()}"] + [f"{key}: {value}" for key, value in details if value])
        else:
            self.header.contextLabel.text = ""
            self.header.contextLabel.toolTip = "Active dataset"

    CONSOLE_COLORS = {"backgroundColor": "panel", "outputTextColor": "text", "errorTextColor": "err",
                      "promptColor": "accent", "stdinTextColor": "data", "commandTextColor": "data",
                      "welcomeTextColor": "subtle"}

    def styleConsole(self):
        """The Python console (and its dock) in the colours of the theme;
        Slicer's colours are kept to be restored (restoreConsole)."""
        console = slicer.app.pythonConsole() if hasattr(slicer.app, "pythonConsole") else None
        dock = slicer.util.mainWindow().findChild("QDockWidget", "PythonConsoleDockWidget")
        if console is None:
            return
        if "console" not in self.saved:
            self.saved["console"] = {name: getattr(console, name) for name in self.CONSOLE_COLORS}
        for name, token in self.CONSOLE_COLORS.items():
            setattr(console, name, theme().color(token))
        if dock is not None:
            theme().applyTo(dock)

    def restoreConsole(self):
        console = slicer.app.pythonConsole() if hasattr(slicer.app, "pythonConsole") else None
        saved = self.saved.pop("console", None)
        if console is not None and saved:
            for name, color in saved.items():
                setattr(console, name, color)
        dock = slicer.util.mainWindow().findChild("QDockWidget", "PythonConsoleDockWidget")
        if dock is not None:
            theme().removeFrom(dock)

    def exit(self):
        if not self.active:
            return
        self.active = False
        self.refreshTimer.stop()
        self.colorFunctionDrag.remove()
        if getattr(self, "tool", None) == "colorFunction":
            # Slicer's own window/level mode is not left on
            slicer.app.applicationLogic().GetInteractionNode().SetCurrentInteractionMode(
                slicer.vtkMRMLInteractionNode.ViewTransform)
            self.tool = "navigate"
            self.checkTool("navigate")
        for observer in self.sceneObservers:
            slicer.mrmlScene.RemoveObserver(observer)
        self.sceneObservers = []
        self.unobserveSegmentations()
        slicer.app.applicationLogic().GetSelectionNode().RemoveObserver(self.selectionObserver)
        slicer.app.applicationLogic().GetInteractionNode().RemoveObserver(self.interactionObserver)
        mainWindow = slicer.util.mainWindow()
        slicer.app.layoutManager().disconnect("layoutChanged(int)", self.onLayoutChanged)
        console = mainWindow.findChild("QDockWidget", "PythonConsoleDockWidget")
        if console is not None:
            console.disconnect("visibilityChanged(bool)", self.onConsoleVisibilityChanged)
        mainWindow.setContextMenuPolicy(self.saved.get("contextMenuPolicy", qt.Qt.DefaultContextMenu))
        theme().setPopupsThemed(False)
        theme().disconnect("themeChanged()", self.styleConsole)
        self.restoreConsole()
        for shortcut in self.shortcuts:
            shortcut.delete()
        self.shortcuts = []
        if self.home is not None:
            self.home.setOverlayTarget(None)
            self.home.hide()
            self.home.setParent(None)
            self.home.delete()
            self.home = None
            self.homeShown = None
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
        theme().disconnect("themeChanged()", self.style3DViews)
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
