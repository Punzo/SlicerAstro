"""Inspector of the Workbench: Inspect (facts of the active dataset),
Process (pipeline strip of the processing steps, each step with its panel)
and Model (3DBarolo). Sky measurements of the markups are shown by Slicer's
markups (SlicerAstro measurements on the sky)."""

import qt

import slicer

from . import Astro
from . import Sections


def theme():
    return slicer.app.property("SlicerAstroTheme")


def overline(text):
    label = qt.QLabel(text.upper())
    label.setProperty("astroRole", "overline")
    font = label.font
    font.setLetterSpacing(qt.QFont.AbsoluteSpacing, 0.9)
    label.setFont(font)
    return label


def panelHolder():
    """Hidden widget of the main window keeping the panels of the steps
    while the Workbench is closed (not top-level windows)."""
    holder = slicer.util.mainWindow().findChild("QWidget", "AstroWorkbenchPanelHolder")
    if holder is None:
        holder = qt.QWidget(slicer.util.mainWindow())
        holder.setObjectName("AstroWorkbenchPanelHolder")
        holder.hide()
    return holder


class PipelineStep:
    """A processing step of the pipeline strip. New steps (e.g. the
    "Observe" step of simulations) register by subclassing:

    - isAvailable(volume): the step applies to the dataset
    - state(volume): "done", "ready" or a reason it is not available
    - widget(): the panel of the step (created once)
    """

    def __init__(self, name, icon, moduleName):
        self.name = name
        self.icon = icon
        self.moduleName = moduleName
        self.panel = None

    def isAvailable(self, volume):
        return volume is not None and Astro.dimensions(volume) >= 3

    def isDone(self, volume):
        return False

    def state(self, volume):
        if volume is None:
            return "no data"
        if not self.isAvailable(volume):
            return "needs a cube"
        return "done" if self.isDone(volume) else "ready"

    def widget(self):
        if self.panel is None:
            module = slicer.util.getModule(self.moduleName)
            self.panel = module.createNewWidgetRepresentation() if module is not None else None
            if self.panel is not None:
                self.panel.setObjectName(f"PipelinePanel_{self.moduleName}")
                # embedded: the panel does not rearrange the views
                self.panel.setProperty("astroEmbedded", True)
        return self.panel


class ProductStep(PipelineStep):
    """Done when the dataset has a product of one of the data models, or a
    3D product (e.g. a smoothed cube)."""

    def __init__(self, name, icon, moduleName, datamodels=(), cubeProduct=False, mask=False):
        PipelineStep.__init__(self, name, icon, moduleName)
        self.datamodels = datamodels
        self.cubeProduct = cubeProduct
        self.mask = mask

    def isDone(self, volume):
        for productID, name, detail, icon, node in Astro.productsOf(volume, slicer.mrmlScene):
            if self.mask and node.IsA("vtkMRMLSegmentationNode"):
                return True
            if node.IsA("vtkMRMLAstroVolumeNode"):
                if Astro.attribute(node, "DATAMODEL", "") in self.datamodels:
                    return True
                if self.cubeProduct and Astro.dimensions(node) >= 3:
                    return True
        return False


STEPS = [
    ProductStep("Smooth", "module-smoothing", "AstroSmoothing", cubeProduct=True),
    ProductStep("Mask", "module-masking", "AstroMasking", mask=True),
    ProductStep("Moments", "module-moment-maps", "AstroMomentMaps",
                datamodels=("ZEROMOMENTMAP", "FIRSTMOMENTMAP", "SECONDMOMENTMAP")),
    ProductStep("Profiles", "module-profiles", "AstroProfiles"),
    ProductStep("PV slice", "module-pv-slice", "AstroPVSlice"),
    ProductStep("PV diagram", "module-pv-diagram", "AstroPVDiagram"),
    PipelineStep("Reproject", "module-reproject", "AstroReproject"),
]

STEP_TOOLTIPS = {
    "Smooth": "Smooth the cube (box, Gaussian or intensity-driven gradient filter) to bring out faint emission.",
    "Mask": "Mask the cube: blank or crop the voxels outside a segment or box.",
    "Moments": "Moment maps: integrated intensity, velocity field and dispersion.",
    "Profiles": "Integrated spectrum (global profile) with W50, W20, systemic velocity and flux.",
    "PV slice": "Position-velocity slice through the cube along a straight line on the sky.",
    "PV diagram": "Position-velocity diagram along a curve on the sky (e.g. a spiral arm or a tidal tail).",
    "Reproject": "Reproject on another grid, mosaic, regrid the spectral axis or match the beams.",
}


class StepButton(qt.QToolButton):
    """A node of the pipeline strip: status dot, icon, name, state."""

    def __init__(self, step, parent=None):
        qt.QToolButton.__init__(self, parent)
        self.step = step
        self.toolTip = STEP_TOOLTIPS.get(step.name, step.name)
        self.setProperty("astroRole", "step")
        self.setObjectName(f"Step_{step.name}")
        self.checkable = True
        self.setSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Fixed)
        self.setMinimumHeight(38)
        layout = qt.QHBoxLayout(self)
        layout.setContentsMargins(10, 0, 10, 0)
        layout.setSpacing(10)
        self.dot = qt.QLabel()
        self.dot.setFixedSize(10, 10)
        self.iconLabel = qt.QLabel()
        self.iconLabel.setPixmap(theme().icon(step.icon, "text").pixmap(18, 18))
        self.nameLabel = qt.QLabel(step.name)
        self.nameLabel.setStyleSheet("font-weight: 600;")
        self.stateLabel = qt.QLabel()
        self.stateLabel.setObjectName(f"StepState_{step.name}")
        self.stateLabel.setProperty("astroRole", "monoLabel")
        for label in (self.dot, self.iconLabel, self.nameLabel, self.stateLabel):
            label.setAttribute(qt.Qt.WA_TransparentForMouseEvents)
        layout.addWidget(self.dot)
        layout.addWidget(self.iconLabel)
        layout.addWidget(self.nameLabel, 1)
        layout.addWidget(self.stateLabel)

    def setState(self, state):
        self.stateLabel.text = state
        color = {"done": "ok", "ready": "accent"}.get(state, "line-strong")
        self.dot.setStyleSheet(f"background: {theme().color(color).name()}; border-radius: 5px;")


class Inspector(qt.QFrame):

    def __init__(self, workbench, parent=None):
        qt.QFrame.__init__(self, parent)
        self.workbench = workbench
        self.volume = None
        self.shownPanel = None
        self.setProperty("astroRole", "inspector")
        self.setObjectName("WorkbenchInspector")
        self.setMinimumWidth(340)
        self.setMaximumWidth(640)
        layout = qt.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        tabs = qt.QFrame()
        tabs.setProperty("astroRole", "bar")
        tabsLayout = qt.QHBoxLayout(tabs)
        tabsLayout.setContentsMargins(12, 10, 12, 10)
        tabsLayout.setSpacing(4)
        self.tabGroup = qt.QButtonGroup(self)
        self.stack = qt.QStackedWidget()
        self.tabs = {}
        for index, name in enumerate(("Inspect", "Process", "Model")):
            button = qt.QToolButton()
            button.setProperty("astroRole", "tab")
            button.setObjectName(f"InspectorTab_{name}")
            button.toolTip = {
                "Inspect": "Facts of the active dataset, its colours, contours, masks, sources, 3D and header.",
                "Process": "Processing steps of the active dataset: smoothing, masking, moments, profiles, "
                           "position-velocity, reprojection.",
                "Model": "Tilted-ring model of a galaxy with 3DBarolo.",
            }[name]
            button.text = name
            button.checkable = True
            self.tabGroup.addButton(button, index)
            tabsLayout.addWidget(button)
            self.tabs[name] = button
        tabsLayout.addStretch(1)
        layout.addWidget(tabs)
        layout.addWidget(self.stack, 1)
        self.tabGroup.connect("idClicked(int)", self.showTab)

        self.stack.addWidget(self.buildInspect())
        self.stack.addWidget(self.buildProcess())
        self.stack.addWidget(self.buildModel())
        self.tabs["Process"].checked = True
        self.stack.setCurrentIndex(1)

    # ------------------------------------------------------------------
    def scrollPage(self):
        scroll = qt.QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(qt.QFrame.NoFrame)
        scroll.setHorizontalScrollBarPolicy(qt.Qt.ScrollBarAlwaysOff)
        # wide enough for the contents and the vertical scroll bar
        slicer.qSlicerAstroScrollAreaFitter(scroll)
        page = qt.QWidget()
        layout = qt.QVBoxLayout(page)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(12)
        scroll.setWidget(page)
        return scroll, layout

    def buildInspect(self):
        scroll, layout = self.scrollPage()
        # the reusable C++ sections of AstroVolume (the same widgets as the
        # AstroVolume panel), in fold sections
        self.infoWidget = slicer.qSlicerAstroInfoSection()
        self.infoFold = Sections.foldSection("Info", "facts, beam, FITS header", "Info", collapsed=False)
        self.infoFold.toolTip = self.infoWidget.toolTip
        self.infoFold.contentLayout().addWidget(self.infoWidget)
        layout.addWidget(self.infoFold)
        self.displayWidget = slicer.qSlicerAstroDisplaySection()
        self.displayFold = Sections.foldSection("Display", "2D colours, window, noise", "Display", collapsed=False)
        self.displayFold.toolTip = self.displayWidget.toolTip
        self.displayFold.contentLayout().addWidget(self.displayWidget)
        layout.addWidget(self.displayFold)
        self.contoursWidget = slicer.qSlicerAstroContoursSection()
        self.contoursFold = Sections.foldSection("Contours", "levels over the data", "Contours", collapsed=False)
        self.contoursFold.toolTip = self.contoursWidget.toolTip
        self.contoursFold.contentLayout().addWidget(self.contoursWidget)
        layout.addWidget(self.contoursFold)
        self.masksWidget = slicer.qSlicerAstroMasksSection()
        self.masksFold = Sections.foldSection("Masks", "segments of the data", "Masks")
        self.masksFold.toolTip = self.masksWidget.toolTip
        self.masksFold.contentLayout().addWidget(self.masksWidget)
        layout.addWidget(self.masksFold)
        self.sourcesWidget = slicer.qSlicerAstroSourcesSection()
        self.sourcesFold = Sections.foldSection("Sources", "catalogues", "Sources")
        self.sourcesFold.toolTip = self.sourcesWidget.toolTip
        self.sourcesFold.contentLayout().addWidget(self.sourcesWidget)
        self.renderingWidget = slicer.qSlicerAstroRenderingSection()
        self.renderingFold = Sections.foldSection("3D", "rendering of the cube", "ThreeD")
        self.renderingFold.toolTip = self.renderingWidget.toolTip
        self.renderingFold.contentLayout().addWidget(self.renderingWidget)
        layout.addWidget(self.renderingFold)
        layout.addWidget(self.sourcesFold)
        layout.addStretch(1)
        return scroll

    # ------------------------------------------------------------------
    def buildProcess(self):
        scroll, layout = self.scrollPage()
        header = qt.QHBoxLayout()
        self.pipelineTitle = overline("Pipeline")
        header.addWidget(self.pipelineTitle)
        header.addStretch(1)
        self.pipelineSummary = qt.QLabel()
        self.pipelineSummary.setProperty("astroRole", "label")
        header.addWidget(self.pipelineSummary)
        layout.addLayout(header)
        strip = qt.QWidget()
        stripLayout = qt.QVBoxLayout(strip)
        stripLayout.setContentsMargins(0, 0, 0, 0)
        stripLayout.setSpacing(2)
        self.stepGroup = qt.QButtonGroup(self)
        self.stepButtons = []
        for index, step in enumerate(STEPS):
            button = StepButton(step)
            self.stepGroup.addButton(button, index)
            stripLayout.addWidget(button)
            self.stepButtons.append(button)
        self.stepGroup.connect("idClicked(int)", self.showStep)
        layout.addWidget(strip)
        self.stepContainer = qt.QFrame()
        self.stepContainer.setProperty("astroRole", "card")
        self.stepLayout = qt.QVBoxLayout(self.stepContainer)
        self.stepLayout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.stepContainer)
        layout.addStretch(1)
        return scroll

    def buildModel(self):
        scroll, layout = self.scrollPage()
        layout.addWidget(overline("3DBarolo · tilted rings"))
        self.modelContainer = qt.QVBoxLayout()
        layout.addLayout(self.modelContainer)
        layout.addStretch(1)
        self.modelStep = PipelineStep("Model", "module-modelling", "AstroModeling")
        return scroll

    # ------------------------------------------------------------------
    def showTab(self, index):
        self.stack.setCurrentIndex(index)
        if index == 2:
            panel = self.modelStep.widget()
            if panel is not None and self.modelContainer.indexOf(panel) < 0:
                self.modelContainer.addWidget(panel)
                theme().applyTo(panel)
            if panel is not None:
                panel.show()
            self.enterPanel(panel)
        elif index == 1 and self.stepGroup.checkedId() >= 0:
            self.enterPanel(STEPS[self.stepGroup.checkedId()].widget())
        else:
            self.enterPanel(None)

    def showStep(self, index):
        panel = STEPS[index].widget()
        if panel is None:
            return
        if self.stepLayout.indexOf(panel) < 0:
            for i in reversed(range(self.stepLayout.count())):
                item = self.stepLayout.itemAt(i)
                if item.widget() is not None:
                    item.widget().hide()
            self.stepLayout.addWidget(panel)
            theme().applyTo(panel)
        for i in range(self.stepLayout.count()):
            widget = self.stepLayout.itemAt(i).widget()
            if widget is not None:
                widget.visible = widget is panel
        self.enterPanel(panel)

    def enterPanel(self, panel):
        """The module panel shown in the inspector is entered (as the module
        panel of Slicer does); the previous one is exited. A panel already
        entered by Slicer (its module selected in Slicer) is neither entered
        again nor exited here: Slicer keeps it."""
        if panel is self.shownPanel:
            return
        current = self.shownPanel
        if current is not None and getattr(self, "enteredPanel", None) is current:
            try:
                if current.isEntered:
                    current.exit()
            except (AttributeError, ValueError):
                pass
        self.enteredPanel = None
        self.shownPanel = panel
        if panel is not None:
            try:
                if not panel.isEntered:
                    panel.enter()
                    self.enteredPanel = panel
            except (AttributeError, ValueError):
                pass

    def setVolume(self, volume):
        changed = volume is not self.volume
        self.volume = volume
        self.infoWidget.setVolumeNode(volume)
        self.infoFold.visible = volume is not None
        self.displayWidget.setVolumeNode(volume)
        self.displayFold.visible = volume is not None
        self.contoursWidget.setVolumeNode(volume)
        self.contoursFold.visible = volume is not None
        self.masksWidget.setVolumeNode(volume)
        self.masksFold.visible = volume is not None
        self.renderingWidget.setVolumeNode(volume)
        self.renderingFold.visible = volume is not None and Astro.dimensions(volume) >= 3
        self.sourcesWidget.setVolumeNode(volume)
        self.sourcesFold.visible = volume is not None
        count = self.sourcesWidget.catalogueCount()
        self.sourcesFold.summary = f"{count} catalogue{'s' if count != 1 else ''}" if count else "catalogues"
        done = 0
        for button in self.stepButtons:
            state = button.step.state(volume)
            button.setState(state)
            done += 1 if state == "done" else 0
        self.pipelineTitle.text = f"PIPELINE · {volume.GetName().upper()}" if volume is not None else "PIPELINE"
        self.pipelineSummary.text = f"{done} of {len(STEPS)} done" if volume is not None else ""
        if self.stepGroup.checkedId() < 0:
            self.stepButtons[2].checked = True
            self.showStep(2)

    def cleanup(self):
        self.infoWidget.setVolumeNode(None)
        self.displayWidget.setVolumeNode(None)
        self.contoursWidget.setVolumeNode(None)
        self.masksWidget.setEditing(False)
        self.masksWidget.setVolumeNode(None)
        self.renderingWidget.setVolumeNode(None)
        self.sourcesWidget.setVolumeNode(None)
        self.enterPanel(None)
        # the step panels are kept by their modules: they are taken out of
        # the inspector before it is deleted
        holder = panelHolder()
        for step in STEPS + [self.modelStep]:
            if step.panel is not None:
                step.panel.hide()
                step.panel.setParent(holder)
