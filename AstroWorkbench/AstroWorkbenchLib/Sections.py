"""Sections of the Inspect tab of the Workbench for the active dataset:
contours, masks, 3D rendering and header. Each section is a thin interface
over the SlicerAstro logics; explanations are in the hover tooltips."""

import math

import qt
import vtk

import slicer

from . import Astro
from . import Contours


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


class DisplaySection:
    """2D colour function of the active dataset: colour table and its options
    (reverse, inverse, logarithmic), interpolation, window (minimum, maximum,
    percentages of the data) on the histogram, and the noise σ used by the 3D
    colour functions."""

    PERCENTAGES = (95.0, 98.0, 99.0, 99.5, 99.9, 100.0)

    def __init__(self):
        self.volume = None
        self.observedDisplay = None
        self.displayObserver = None
        self.observedVolume = None
        self.volumeObserver = None
        self.positive = None
        self.negative = None
        self.section = foldSection("Display", "2D colours, window, noise", "Display", collapsed=False)
        self.section.toolTip = "Colours of the active dataset in the 2D views, and its noise."
        layout = qt.QGridLayout()
        layout.setHorizontalSpacing(8)
        layout.setVerticalSpacing(6)

        self.colours = qt.QComboBox()
        self.colours.setObjectName("DisplayColourTable")
        self.colours.setSizeAdjustPolicy(qt.QComboBox.AdjustToMinimumContentsLengthWithIcon)
        self.colours.setMinimumContentsLength(8)
        self.colours.setIconSize(qt.QSize(48, 12))
        self.colours.toolTip = "Colour table of the 2D views (SlicerAstro tables and the custom ones)."
        self.colours.connect("activated(int)", self.setColourTable)
        layout.addWidget(label("Colours", self.colours.toolTip), 0, 0)
        layout.addWidget(self.colours, 0, 1)

        options = qt.QHBoxLayout()
        options.setSpacing(4)
        self.reverseButton = optionButton("Reverse", "Colours in reverse order (low values get the colours of "
                                          "the high ones).", "DisplayReverse")
        self.inverseButton = optionButton("Invert", "Complementary colours (e.g. black on white).",
                                          "DisplayInverse")
        self.logButton = optionButton("Log", "Logarithmic scale of the colours (faint emission brighter).",
                                      "DisplayLog")
        self.interpolateButton = optionButton("Smooth", "Interpolate between the pixels in the 2D views "
                                              "(off: the pixels of the data).", "DisplayInterpolate")
        for option, button in (("Reverse", self.reverseButton), ("Inverse", self.inverseButton),
                               ("Log", self.logButton)):
            button.connect("toggled(bool)", lambda on, o=option: self.setOption(o, on))
            options.addWidget(button)
        self.interpolateButton.connect("toggled(bool)", self.setInterpolate)
        options.addWidget(self.interpolateButton)
        options.addStretch(1)
        layout.addLayout(options, 1, 1)
        layout.setColumnStretch(1, 1)
        self.section.contentLayout().addLayout(layout)

        self.histogram = slicer.qSlicerAstroHistogramWidget()
        self.histogram.setObjectName("DisplayHistogram")
        self.histogram.setMinimumWidth(40)
        self.histogram.connect("windowChanging(double,double)", self.showWindow)
        self.histogram.connect("windowChanged(double,double)", self.setWindow)
        self.histogram.connect("noiseChanging(double)", self.showNoise)
        self.histogram.connect("noiseChanged(double)", self.setNoise)
        self.section.contentLayout().addWidget(self.histogram)

        fields = qt.QGridLayout()
        fields.setHorizontalSpacing(6)
        fields.setVerticalSpacing(6)
        self.minimumField = slicer.qSlicerAstroValueField()
        self.minimumField.setObjectName("DisplayMinimum")
        self.minimumField.leadingText = "min"
        self.minimumField.toolTip = "Lowest value of the 2D colours (the first colour below it)."
        self.maximumField = slicer.qSlicerAstroValueField()
        self.maximumField.setObjectName("DisplayMaximum")
        self.maximumField.leadingText = "max"
        self.maximumField.toolTip = "Highest value of the 2D colours (the last colour above it)."
        self.noiseField = slicer.qSlicerAstroValueField()
        self.noiseField.setObjectName("DisplayNoise")
        self.noiseField.leadingText = "σ"
        self.noiseField.toolTip = ("Noise σ of the data: the 3D colour functions (surfaces at 3σ, 7σ, …), the "
                                   "thresholds in σ and the contour levels in σ use it.")
        for field in (self.minimumField, self.maximumField, self.noiseField):
            field.decimals = 6
            field.connect("editingFinished()", self.onFieldsEdited)
        fields.addWidget(self.minimumField, 0, 0)
        fields.addWidget(self.maximumField, 0, 1)
        fields.addWidget(self.noiseField, 1, 0, 1, 2)
        self.section.contentLayout().addLayout(fields)

        self.percentages = slicer.qSlicerAstroSegmentedButton()
        self.percentages.setObjectName("DisplayPercentages")
        for percentage in self.PERCENTAGES:
            text = f"{percentage:g}%"
            self.percentages.addButton(text, f"Window with {percentage:g}% of the negative and of the positive "
                                       "values of the data (the extreme values outside)."
                                       if percentage < 100 else "Window from the minimum to the maximum of the data.")
        self.percentages.connect("currentIndexChanged(int)", self.applyPercentage)
        self.section.contentLayout().addWidget(self.percentages)
        self.windowLabel = qt.QLabel()
        self.windowLabel.setObjectName("DisplayWindowLabel")
        self.windowLabel.setProperty("astroRole", "monoLabel")
        self.windowLabel.toolTip = "Window of the 2D colours in units of the noise."
        self.section.contentLayout().addWidget(self.windowLabel)

    # ------------------------------------------------------------------
    def displayNode(self):
        displayNode = self.volume.GetDisplayNode() if self.volume is not None else None
        return displayNode if displayNode is not None and displayNode.IsA("vtkMRMLAstroVolumeDisplayNode") else None

    def observe(self, volume):
        if self.observedDisplay is not None and self.displayObserver is not None:
            self.observedDisplay.RemoveObserver(self.displayObserver)
        if self.observedVolume is not None and self.volumeObserver is not None:
            self.observedVolume.RemoveObserver(self.volumeObserver)
        self.observedDisplay = volume.GetDisplayNode() if volume is not None else None
        self.displayObserver = self.observedDisplay.AddObserver(vtk.vtkCommand.ModifiedEvent, self.update) \
            if self.observedDisplay is not None else None
        self.observedVolume = volume
        self.volumeObserver = volume.AddObserver(slicer.vtkMRMLAstroVolumeNode.DisplayThresholdModifiedEvent,
                                                 self.update) if volume is not None else None

    def setVolume(self, volume):
        changed = volume is not self.volume
        self.volume = volume
        self.section.visible = volume is not None
        if changed:
            self.observe(volume)
            self.computeHistogram()
        self.update()

    def computeHistogram(self):
        """Histogram of the voxels (sampled), and the sorted negative and
        positive values for the percentages."""
        import numpy as np
        from vtk.util.numpy_support import vtk_to_numpy
        self.positive = self.negative = None
        if self.volume is None or self.volume.GetImageData() is None:
            self.histogram.clearHistogram()
            return
        values = vtk_to_numpy(self.volume.GetImageData().GetPointData().GetScalars())
        values = values[::max(1, values.size // 2000000)].astype(np.float64)
        values = values[np.isfinite(values)]
        if values.size == 0:
            self.histogram.clearHistogram()
            return
        low, high = float(values.min()), float(values.max())
        if high <= low:
            high = low + 1.0
        counts, edges = np.histogram(values, bins=180, range=(low, high))
        self.histogram.setHistogram(list((edges[:-1] + edges[1:]) / 2), list(counts.astype(float)))
        self.positive = np.sort(values[values > 0])
        self.negative = np.sort(values[values < 0])
        self.dataRange = (low, high)

    def update(self, caller=None, event=None):
        displayNode = self.displayNode()
        if displayNode is None:
            return
        unit = Astro.attribute(self.volume, "BUNIT", "")
        self.histogram.setUnit(unit)
        noise = Astro.noise(self.volume)
        self.histogram.setNoise(noise if noise > 0 else float("nan"))
        self.histogram.setNoiseEditable(Astro.dimensions(self.volume) >= 3)
        window, level = displayNode.GetWindow(), displayNode.GetLevel()
        low, high = level - window / 2.0, level + window / 2.0
        self.histogram.setWindow(low, high)
        for field, value in ((self.minimumField, low), (self.maximumField, high), (self.noiseField, noise)):
            wasBlocked = field.blockSignals(True)
            if hasattr(self, "dataRange"):
                span = self.dataRange[1] - self.dataRange[0]
                field.setRange(self.dataRange[0] - span, self.dataRange[1] + span)
            field.unit = unit
            field.value = value
            field.blockSignals(wasBlocked)
        self.noiseField.enabled = Astro.dimensions(self.volume) >= 3
        self.showWindow(low, high)
        # colour tables
        colorNode = displayNode.GetColorNode()
        wasBlocked = self.colours.blockSignals(True)
        self.colours.clear()
        for node in slicer.util.getNodesByClass("vtkMRMLColorTableNode"):
            if slicer.vtkSlicerAstroVolumeLogic.HasColorFunctionOptions(node) or node is colorNode:
                self.colours.addItem(qt.QIcon(colourStrip(node)), node.GetName(), node.GetID())
        index = self.colours.findData(colorNode.GetID()) if colorNode is not None else -1
        self.colours.setCurrentIndex(index)
        self.colours.blockSignals(wasBlocked)
        hasOptions = slicer.vtkSlicerAstroVolumeLogic.HasColorFunctionOptions(colorNode)
        for option, button in (("Reverse", self.reverseButton), ("Inverse", self.inverseButton),
                               ("Log", self.logButton)):
            wasBlocked = button.blockSignals(True)
            button.checked = slicer.vtkSlicerAstroVolumeLogic.GetColorFunctionOption(colorNode, option)
            button.enabled = hasOptions
            button.blockSignals(wasBlocked)
        wasBlocked = self.interpolateButton.blockSignals(True)
        self.interpolateButton.checked = bool(displayNode.GetInterpolate())
        self.interpolateButton.blockSignals(wasBlocked)

    def showWindow(self, low, high):
        noise = Astro.noise(self.volume) if self.volume is not None else 0.0
        if noise > 0:
            self.windowLabel.text = f"window {low / noise:.1f}σ … {high / noise:.1f}σ"
        else:
            self.windowLabel.text = ""

    def showNoise(self, noise):
        wasBlocked = self.noiseField.blockSignals(True)
        self.noiseField.value = noise
        self.noiseField.blockSignals(wasBlocked)

    def setWindow(self, low, high):
        displayNode = self.displayNode()
        if displayNode is None or high <= low:
            return
        displayNode.SetAutoWindowLevel(0)
        displayNode.SetWindowLevel(high - low, (high + low) / 2.0)

    def setNoise(self, noise):
        if self.volume is not None and noise > 0 and abs(noise - Astro.noise(self.volume)) > 1e-12:
            self.volume.SetDisplayThreshold(noise)

    def onFieldsEdited(self):
        low, high = self.minimumField.value, self.maximumField.value
        if high > low:
            self.setWindow(low, high)
        if self.noiseField.enabled:
            self.setNoise(self.noiseField.value)

    def applyPercentage(self, index):
        if self.positive is None or index < 0:
            return
        import numpy as np
        percentage = self.PERCENTAGES[index] / 100.0
        low, high = self.dataRange
        if percentage < 1.0:
            # the classic SlicerAstro clipping: the percentage of the negative
            # values (from zero) and of the positive values
            if self.negative.size:
                low = float(np.quantile(self.negative, 1.0 - percentage))
            if self.positive.size:
                high = float(np.quantile(self.positive, percentage))
        self.setWindow(low, high)

    def setColourTable(self, index):
        displayNode = self.displayNode()
        nodeID = self.colours.itemData(index)
        if displayNode is not None and nodeID:
            displayNode.SetAndObserveColorNodeID(nodeID)

    def setOption(self, option, on):
        displayNode = self.displayNode()
        if displayNode is not None:
            slicer.vtkSlicerAstroVolumeLogic.SetColorFunctionOption(displayNode.GetColorNode(), option, on)
            self.update()

    def setInterpolate(self, on):
        displayNode = self.displayNode()
        if displayNode is not None:
            displayNode.SetInterpolate(on)

    def cleanup(self):
        self.observe(None)


class ContoursSection:
    """Contours over the active dataset: of the dataset itself or of other
    data (reprojected), levels in units of the noise, of HI column density or
    in the units of the data."""

    UNITS = ("σ", "10¹⁹ cm⁻²", "value")

    def __init__(self):
        self.volume = None
        self.section = foldSection("Contours", "levels over the data", "Contours", collapsed=False)
        self.section.toolTip = "Contour levels drawn over the active dataset in the 2D views."
        layout = qt.QGridLayout()
        layout.setHorizontalSpacing(8)
        layout.setVerticalSpacing(6)

        self.source = qt.QComboBox()
        self.source.setSizeAdjustPolicy(qt.QComboBox.AdjustToMinimumContentsLengthWithIcon)
        self.source.setMinimumContentsLength(8)
        self.source.setObjectName("ContoursSource")
        self.source.toolTip = ("Data of the contours: the active dataset, or other data (e.g. an HI cube or "
                               "moment-0 map over an optical image), reprojected on the sky grid of the active "
                               "dataset.")
        self.source.connect("currentIndexChanged(int)", lambda index: self.updateUnits())
        layout.addWidget(label("Source", self.source.toolTip), 0, 0)
        layout.addWidget(self.source, 0, 1, 1, 2)

        self.levels = qt.QLineEdit("3, 6, 12, 24")
        self.levels.setObjectName("ContoursLevels")
        self.levels.toolTip = ("Contour levels separated by commas, in the unit on the right. A negative level "
                               "selects the values below it.")
        self.unit = slicer.qSlicerAstroSegmentedButton()
        self.unit.setObjectName("ContoursUnit")
        self.unit.addButton("σ", "Levels in units of the noise σ of the source.")
        self.unit.addButton("N_HI", "Levels in HI column density, units of 10¹⁹ cm⁻² (HI moment-0 maps with a beam).")
        self.unit.addButton("value", "Levels in the units of the source data.")
        self.unit.connect("currentIndexChanged(int)", lambda index: self.updateUnits())
        layout.addWidget(label("Levels", self.levels.toolTip), 1, 0)
        layout.addWidget(self.levels, 1, 1)
        layout.addWidget(self.unit, 1, 2)
        self.unitInfo = qt.QLabel()
        self.unitInfo.setObjectName("ContoursUnitInfo")
        self.unitInfo.setProperty("astroRole", "monoLabel")
        self.unitInfo.toolTip = "Noise and unit of the source data."
        layout.addWidget(self.unitInfo, 2, 1, 1, 2)

        self.colours = qt.QComboBox()
        self.colours.setSizeAdjustPolicy(qt.QComboBox.AdjustToMinimumContentsLengthWithIcon)
        self.colours.setMinimumContentsLength(8)
        self.colours.setObjectName("ContoursColours")
        for name in Contours.familyNames():
            pixmap = qt.QPixmap(48, 12)
            pixmap.fill(qt.QColor(0, 0, 0, 0))
            painter = qt.QPainter(pixmap)
            colours = Contours.familyColours(name, 5)
            for index, rgb in enumerate(colours):
                painter.fillRect(index * 10, 0, 9, 12, qt.QColor.fromRgbF(*rgb))
            painter.end()
            self.colours.addItem(qt.QIcon(pixmap), name)
        self.colours.toolTip = "Colours of the levels, from the lowest to the highest."
        layout.addWidget(label("Colours", self.colours.toolTip), 3, 0)
        layout.addWidget(self.colours, 3, 1, 1, 2)
        layout.setColumnStretch(1, 1)
        self.section.contentLayout().addLayout(layout)

        buttons = qt.QHBoxLayout()
        buttons.setSpacing(4)
        self.drawButton = qt.QPushButton("Draw contours")
        self.drawButton.setObjectName("ContoursDraw")
        self.drawButton.setProperty("astroRole", "primary")
        self.drawButton.setIcon(theme().icon("contour", "on-accent"))
        self.drawButton.toolTip = ("Draw the levels. The contours of the same source are replaced, those of "
                                   "other sources are kept.")
        self.drawButton.connect("clicked()", self.draw)
        buttons.addWidget(self.drawButton, 1)
        self.showButton = iconButton("eye", "Show or hide all the contours of the dataset.", True, "ContoursShow")
        self.showButton.connect("toggled(bool)", self.setShown)
        self.smoothButton = iconButton("smoothing", "Smooth contour lines (slower) instead of the outlines of "
                                       "the pixels.", True, "ContoursSmooth")
        self.smoothButton.connect("toggled(bool)", self.setSmooth)
        self.deleteButton = iconButton("trash", "Delete all the contours of the dataset.", False, "ContoursDelete")
        self.deleteButton.connect("clicked()", self.deleteAll)
        for button in (self.showButton, self.smoothButton, self.deleteButton):
            buttons.addWidget(button)
        self.section.contentLayout().addLayout(buttons)

        self.status = qt.QLabel()
        self.status.setObjectName("ContoursStatus")
        self.status.setProperty("astroRole", "monoLabel")
        self.status.wordWrap = True
        self.status.toolTip = "Result of the last contours."
        self.section.contentLayout().addWidget(self.status)
        self.sourcesLayout = qt.QVBoxLayout()
        self.sourcesLayout.setSpacing(2)
        self.section.contentLayout().addLayout(self.sourcesLayout)

    # ------------------------------------------------------------------
    def contoursNode(self, create=False):
        if self.volume is None:
            return None
        return astroVolumeLogic().GetContoursSegmentationForVolume(self.volume, create)

    def sourceVolume(self):
        nodeID = self.source.currentData
        node = slicer.mrmlScene.GetNodeByID(nodeID) if nodeID else None
        return node if node is not None else self.volume

    def setVolume(self, volume):
        changed = volume is not self.volume
        self.volume = volume
        self.section.visible = volume is not None
        if volume is None:
            return
        # another dataset: its own contours by default
        current = None if changed else self.source.currentData
        wasBlocked = self.source.blockSignals(True)
        self.source.clear()
        self.source.addItem("This dataset", volume.GetID())
        for other in Astro.astroVolumes(slicer.mrmlScene):
            if other is not volume and other.GetImageData() is not None:
                kind = "HI map" if Contours.isHIMap(other) else ("cube" if Astro.dimensions(other) >= 3 else "image")
                self.source.addItem(f"{other.GetName()} · {kind}", other.GetID())
        index = self.source.findData(current) if current else 0
        self.source.setCurrentIndex(max(index, 0))
        self.source.blockSignals(wasBlocked)
        self.updateUnits()
        self.updateSources()

    def updateUnits(self):
        source = self.sourceVolume()
        if source is None:
            return
        factor = Contours.columnDensityFactor(source)
        self.unit.button(1).enabled = factor > 0
        if self.unit.currentIndex == 1 and factor <= 0:
            self.unit.setCurrentIndex(0)
        unit = Astro.attribute(source, "BUNIT", "")
        noise = Astro.noise(source)
        if self.unit.currentIndex == 0:
            self.unitInfo.text = f"σ = {Astro.formatFlux(noise, unit)}" if noise else "σ unknown"
        elif self.unit.currentIndex == 1:
            self.unitInfo.text = f"1 = {Contours.COLUMN_DENSITY_UNIT / factor:.3g} {Astro.displayUnit(unit)}"
        else:
            self.unitInfo.text = Astro.displayUnit(unit)

    def parsedLevels(self):
        values = []
        for text in self.levels.text.replace(";", ",").split(","):
            text = text.strip().replace("−", "-")
            if not text:
                continue
            values.append(float(text))
        return values

    def draw(self):
        source = self.sourceVolume()
        if self.volume is None or source is None:
            return
        try:
            values = self.parsedLevels()
        except ValueError:
            self.status.text = "Levels: numbers separated by commas"
            return
        if not values:
            self.status.text = "No levels"
            return
        if self.unit.currentIndex == 0:
            noise = Astro.noise(source)
            if noise <= 0:
                self.status.text = "The noise of the source is unknown: use values"
                return
            levels = [value * noise for value in values]
        elif self.unit.currentIndex == 1:
            factor = Contours.columnDensityFactor(source)
            levels = [value * Contours.COLUMN_DENSITY_UNIT / factor for value in values]
        else:
            levels = values
        with slicer.util.WaitCursor():
            created, message = Contours.drawContours(self.volume, source, levels, self.colours.currentText)
        if created:
            self.status.text = f"{created} level{'s' if created != 1 else ''} drawn" + \
                (f" · {message}" if message else "")
        else:
            self.status.text = message or "No contours"
        self.updateSources()

    def setShown(self, shown):
        contours = self.contoursNode()
        if contours is not None and contours.GetDisplayNode() is not None:
            contours.GetDisplayNode().SetVisibility(shown)
        self.updateSources()

    def setSmooth(self, smooth):
        contours = self.contoursNode()
        if contours is not None:
            with slicer.util.WaitCursor():
                slicer.vtkSlicerAstroVolumeLogic.SetContoursSmooth2D(contours, smooth)

    def deleteAll(self):
        contours = self.contoursNode()
        if contours is not None and contours.GetSegmentation() is not None:
            contours.GetSegmentation().RemoveAllSegments()
        self.status.text = ""
        self.updateSources()

    def updateSources(self):
        while self.sourcesLayout.count():
            item = self.sourcesLayout.takeAt(0)
            if item.widget() is not None:
                item.widget().delete()
        contours = self.contoursNode()
        sources = Contours.contourSources(self.volume, contours)
        hasContours = bool(sources)
        for button in (self.showButton, self.smoothButton, self.deleteButton):
            button.enabled = hasContours
        displayNode = contours.GetDisplayNode() if contours is not None else None
        wasBlocked = self.showButton.blockSignals(True)
        self.showButton.checked = bool(displayNode is not None and displayNode.GetVisibility() and hasContours)
        self.showButton.blockSignals(wasBlocked)
        for sourceID, segmentIDs in sources.items():
            node = slicer.mrmlScene.GetNodeByID(sourceID)
            name = node.GetName() if node is not None else sourceID
            row = qt.QWidget()
            rowLayout = qt.QHBoxLayout(row)
            rowLayout.setContentsMargins(0, 0, 0, 0)
            rowLayout.setSpacing(6)
            eye = iconButton("eye", f"Show or hide the contours of {name}.", True, f"ContoursSourceShow_{sourceID}")
            eye.checked = Contours.sourceVisible(contours, segmentIDs)
            eye.connect("toggled(bool)", lambda on, ids=segmentIDs: self.setSourceShown(ids, on))
            text = qt.QLabel(f"{name} · {len(segmentIDs)} level{'s' if len(segmentIDs) != 1 else ''}")
            text.setProperty("astroRole", "mono")
            text.toolTip = f"Contours of {name} drawn over {self.volume.GetName()}."
            rowLayout.addWidget(eye)
            rowLayout.addWidget(text, 1)
            self.sourcesLayout.addWidget(row)
            theme().applyTo(row)

    def setSourceShown(self, segmentIDs, shown):
        Contours.setSourceVisible(self.contoursNode(), segmentIDs, shown)


class MasksSection:
    """The mask of the active dataset (segments, e.g. SoFiA sources): show,
    edit, copy to other data, save as a FITS mask."""

    def __init__(self):
        self.volume = None
        self.editor = None
        self.section = foldSection("Masks", "segments of the data", "Masks")
        self.section.toolTip = "Mask of the active dataset: one segment per source or region."
        self.table = slicer.qMRMLSegmentsTableView()
        self.table.setObjectName("MasksTable")
        self.table.setMRMLScene(slicer.mrmlScene)
        self.table.setMinimumHeight(110)
        self.table.setMaximumHeight(170)
        self.table.toolTip = "Segments of the mask: visibility, colour, name. Select segments to copy them."
        self.section.contentLayout().addWidget(self.table)
        self.empty = label("No segments", "The mask of the dataset is empty: draw segments with Edit, or "
                           "load a FITS mask (e.g. SoFiA).")
        self.empty.setObjectName("MasksEmpty")
        self.section.contentLayout().addWidget(self.empty)

        buttons = qt.QHBoxLayout()
        buttons.setSpacing(4)
        self.showButton = iconButton("eye", "Show or hide the mask in the views.", True, "MasksShow")
        self.showButton.connect("toggled(bool)", self.setShown)
        self.editButton = qt.QPushButton("Edit")
        self.editButton.setObjectName("MasksEdit")
        self.editButton.checkable = True
        self.editButton.setIcon(theme().icon("lasso", "text"))
        self.editButton.toolTip = "Draw, erase and threshold segments (Segment Editor)."
        self.editButton.connect("toggled(bool)", self.setEditing)
        self.saveButton = iconButton("download", "Save the mask as a FITS mask (integer cube with the WCS of "
                                     "the data).", False, "MasksSave")
        self.saveButton.connect("clicked()", self.save)
        buttons.addWidget(self.showButton)
        buttons.addWidget(self.editButton, 1)
        buttons.addWidget(self.saveButton)
        self.section.contentLayout().addLayout(buttons)

        copyRow = qt.QHBoxLayout()
        copyRow.setSpacing(4)
        self.copyTarget = qt.QComboBox()
        self.copyTarget.setSizeAdjustPolicy(qt.QComboBox.AdjustToMinimumContentsLengthWithIcon)
        self.copyTarget.setMinimumContentsLength(8)
        self.copyTarget.setObjectName("MasksCopyTarget")
        self.copyTarget.toolTip = ("Dataset that receives the selected segments (all if none is selected): same "
                                   "grid, sky projection of a cube mask on an image, or reprojection.")
        self.copyButton = qt.QPushButton("Copy")
        self.copyButton.setObjectName("MasksCopy")
        self.copyButton.setIcon(theme().icon("copy", "text"))
        self.copyButton.toolTip = "Copy the selected segments to the mask of the chosen dataset."
        self.copyButton.connect("clicked()", self.copy)
        copyRow.addWidget(self.copyTarget, 1)
        copyRow.addWidget(self.copyButton)
        self.section.contentLayout().addLayout(copyRow)
        self.status = qt.QLabel()
        self.status.setObjectName("MasksStatus")
        self.status.setProperty("astroRole", "monoLabel")
        self.status.wordWrap = True
        self.status.toolTip = "Result of the last action on the mask."
        self.section.contentLayout().addWidget(self.status)
        self.editorHolder = qt.QVBoxLayout()
        self.section.contentLayout().addLayout(self.editorHolder)

    def mask(self, create=False):
        if self.volume is None:
            return None
        return astroVolumeLogic().GetMaskSegmentationForVolume(self.volume, create)

    def setVolume(self, volume):
        self.volume = volume
        self.section.visible = volume is not None
        if volume is None:
            self.table.setSegmentationNode(None)
            return
        mask = self.mask(create=False)
        self.table.setSegmentationNode(mask)
        count = mask.GetSegmentation().GetNumberOfSegments() if mask is not None and mask.GetSegmentation() else 0
        self.table.visible = count > 0
        self.empty.visible = count == 0
        self.section.summary = f"{count} segment{'s' if count != 1 else ''}" if count else "no segments"
        displayNode = mask.GetDisplayNode() if mask is not None else None
        wasBlocked = self.showButton.blockSignals(True)
        self.showButton.checked = bool(displayNode is not None and displayNode.GetVisibility())
        self.showButton.blockSignals(wasBlocked)
        for button in (self.showButton, self.saveButton, self.copyButton):
            button.enabled = count > 0
        self.copyTarget.clear()
        for other in Astro.rootDatasets(slicer.mrmlScene):
            if other is not volume:
                self.copyTarget.addItem(other.GetName(), other.GetID())
        self.copyTarget.enabled = self.copyTarget.count > 0 and count > 0
        if self.editor is not None and self.editButton.checked:
            self.editor.setSegmentationNode(self.mask(create=True))
            self.editor.setSourceVolumeNode(volume)

    def setShown(self, shown):
        mask = self.mask()
        if mask is not None and mask.GetDisplayNode() is not None:
            mask.GetDisplayNode().SetVisibility(shown)

    def setEditing(self, editing):
        if editing and self.editor is None:
            self.editor = slicer.qMRMLSegmentEditorWidget()
            self.editor.setObjectName("MasksSegmentEditor")
            self.editor.setMRMLScene(slicer.mrmlScene)
            editorNode = slicer.mrmlScene.GetSingletonNode("SegmentEditor", "vtkMRMLSegmentEditorNode")
            if editorNode is None:
                editorNode = slicer.vtkMRMLSegmentEditorNode()
                editorNode.SetSingletonTag("SegmentEditor")
                editorNode = slicer.mrmlScene.AddNode(editorNode)
            self.editor.setMRMLSegmentEditorNode(editorNode)
            self.editor.setSegmentationNodeSelectorVisible(False)
            self.editor.setSourceVolumeNodeSelectorVisible(False)
            self.editorHolder.addWidget(self.editor)
            theme().applyTo(self.editor)
        if self.editor is not None:
            self.editor.visible = editing
            if editing and self.volume is not None:
                self.editor.setSegmentationNode(self.mask(create=True))
                self.editor.setSourceVolumeNode(self.volume)
            elif not editing:
                self.editor.setActiveEffect(None)

    def selectedSegmentIDs(self):
        ids = vtk.vtkStringArray()
        for segmentID in self.table.selectedSegmentIDs():
            ids.InsertNextValue(segmentID)
        return ids

    def copy(self):
        mask = self.mask()
        target = slicer.mrmlScene.GetNodeByID(self.copyTarget.currentData) if self.copyTarget.currentData else None
        if mask is None or target is None:
            return
        ids = self.selectedSegmentIDs()
        if ids.GetNumberOfValues() == 0:
            segmentation = mask.GetSegmentation()
            for index in range(segmentation.GetNumberOfSegments()):
                ids.InsertNextValue(segmentation.GetNthSegmentID(index))
        logic = astroVolumeLogic()
        with slicer.util.WaitCursor():
            result = logic.CopyMaskSegmentsToVolume(mask, ids, target, False)
        self.status.text = f"{ids.GetNumberOfValues()} segment(s) copied to {target.GetName()}" if result is not None \
            else logic.GetLastMaskCopyError()

    def save(self):
        mask = self.mask()
        if mask is None:
            return
        path = qt.QFileDialog.getSaveFileName(slicer.util.mainWindow(), "Save the mask",
                                              f"{mask.GetName()}.fits", "FITS masks (*.fits *.fits.gz)")
        if path:
            self.status.text = f"Saved {path}" if slicer.util.saveNode(mask, path) else "The mask was not saved"

    def cleanup(self):
        if self.editor is not None:
            self.editor.setActiveEffect(None)
            self.editor.setSegmentationNode(None)


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


class ThreeDSection:
    """3D rendering of a cube: colour function, crop to a box (ROI), noise in
    the box."""

    def __init__(self):
        self.volume = None
        self.section = foldSection("3D", "rendering of the cube", "ThreeD")
        self.section.toolTip = "Volume rendering of the active cube in the 3D views."
        layout = qt.QGridLayout()
        layout.setHorizontalSpacing(8)
        layout.setVerticalSpacing(6)
        self.preset = qt.QComboBox()
        self.preset.setObjectName("ThreeDPreset")
        self.preset.setIconSize(qt.QSize(40, 40))
        self.preset.setSizeAdjustPolicy(qt.QComboBox.AdjustToMinimumContentsLengthWithIcon)
        self.preset.setMinimumContentsLength(8)
        self.preset.toolTip = ("3D colour function: surfaces at multiples of the noise, or emission ramps; "
                               "Observatory is colour-blind safe.")
        self.preset.connect("activated(int)", self.applyPreset)
        layout.addWidget(label("Colours", self.preset.toolTip), 0, 0)
        layout.addWidget(self.preset, 0, 1, 1, 3)
        self.visibleButton = iconButton("eye", "Show or hide the cube in the 3D views.", True, "ThreeDVisible")
        self.visibleButton.connect("toggled(bool)", self.setVisible)
        self.cropButton = qt.QPushButton("Crop")
        self.cropButton.setObjectName("ThreeDCrop")
        self.cropButton.checkable = True
        self.cropButton.setIcon(theme().icon("crop", "text"))
        self.cropButton.toolTip = "Render only the cube inside a box (ROI); drag its handles in the views."
        self.cropButton.connect("toggled(bool)", self.setCropped)
        self.fitButton = iconButton("roi", "Fit the box to the whole cube.", False, "ThreeDFitROI")
        self.fitButton.connect("clicked()", self.fitROI)
        self.noiseButton = qt.QPushButton("Noise")
        self.noiseButton.setObjectName("ThreeDNoiseInROI")
        self.noiseButton.setIcon(theme().icon("histogram", "text"))
        self.noiseButton.toolTip = ("Noise σ of the voxels in the box (choose a box without emission): it "
                                    "becomes the noise of the data (contours, thresholds, colour functions).")
        self.noiseButton.connect("clicked()", self.noiseInROI)
        layout.addWidget(self.visibleButton, 1, 0)
        layout.addWidget(self.cropButton, 1, 1)
        layout.addWidget(self.fitButton, 1, 2)
        layout.addWidget(self.noiseButton, 1, 3)
        layout.setColumnStretch(1, 1)
        layout.setColumnStretch(3, 1)
        self.section.contentLayout().addLayout(layout)
        self.status = qt.QLabel()
        self.status.setObjectName("ThreeDStatus")
        self.status.setProperty("astroRole", "monoLabel")
        self.status.toolTip = "Noise of the data."
        self.section.contentLayout().addWidget(self.status)
        self.presetNodes = []

    def displayNode(self, create=False):
        if self.volume is None:
            return None
        logic = slicer.modules.volumerendering.logic()
        displayNode = logic.GetFirstVolumeRenderingDisplayNode(self.volume)
        if displayNode is None and create:
            displayNode = logic.CreateDefaultVolumeRenderingNodes(self.volume)
        return displayNode

    def roi(self, create=False):
        displayNode = self.displayNode(create)
        if displayNode is None:
            return None
        roi = displayNode.GetMarkupsROINode()
        if roi is None and create:
            roi = slicer.modules.volumerendering.logic().CreateROINode(displayNode)
        return roi

    def setVolume(self, volume):
        self.volume = volume if volume is not None and Astro.dimensions(volume) >= 3 else None
        self.section.visible = self.volume is not None
        if self.volume is None:
            return
        presetsScene = astroVolumeLogic().GetPresetsScene()
        self.presetNodes = list(slicer.util.getNodesByClass("vtkMRMLVolumePropertyNode", presetsScene)) \
            if presetsScene is not None else []
        current = self.volume.GetPresetNode()
        currentID = slicer.vtkMRMLAstroVolumeNode.GetPresetIdentifier(current) if current is not None else ""
        wasBlocked = self.preset.blockSignals(True)
        self.preset.clear()
        for node in self.presetNodes:
            self.preset.addItem(qt.QIcon(f":/presets/{node.GetName()}"), node.GetName())
            if slicer.vtkMRMLAstroVolumeNode.GetPresetIdentifier(node) == currentID:
                self.preset.setCurrentIndex(self.preset.count - 1)
        self.preset.blockSignals(wasBlocked)
        displayNode = self.displayNode()
        for button, value in ((self.visibleButton, displayNode is not None and displayNode.GetVisibility()),
                              (self.cropButton, displayNode is not None and displayNode.GetCroppingEnabled())):
            wasBlocked = button.blockSignals(True)
            button.checked = bool(value)
            button.blockSignals(wasBlocked)
        self.updateStatus()

    def updateStatus(self):
        noise = Astro.noise(self.volume) if self.volume is not None else 0
        self.status.text = f"σ = {Astro.formatFlux(noise, Astro.attribute(self.volume, 'BUNIT', ''))}" if noise else ""

    def applyPreset(self, index):
        if self.volume is None or index < 0 or index >= len(self.presetNodes):
            return
        # the AstroVolume panel applies the colour functions (scaled to the
        # noise of the data)
        panel = slicer.modules.astrovolume.widgetRepresentation()
        panel.setMRMLVolumeNode(self.volume)
        combo = slicer.util.findChild(panel, "PresetsNodeComboBox")
        if combo.currentNode() is self.presetNodes[index]:
            combo.setCurrentNode(None)
        combo.setCurrentNode(self.presetNodes[index])

    def setVisible(self, visible):
        displayNode = self.displayNode(create=visible)
        if displayNode is not None:
            displayNode.SetVisibility(visible)

    def setCropped(self, cropped):
        displayNode = self.displayNode(create=cropped)
        if displayNode is None:
            return
        roi = self.roi(create=cropped)
        displayNode.SetCroppingEnabled(cropped)
        if roi is not None:
            roi.SetDisplayVisibility(cropped)

    def fitROI(self):
        displayNode = self.displayNode(create=True)
        if displayNode is not None and self.roi(create=True) is not None:
            slicer.modules.volumerendering.logic().FitROIToVolume(displayNode)

    def noiseInROI(self):
        roi = self.roi(create=True)
        if roi is None:
            return
        if not self.cropButton.checked:
            roi.SetDisplayVisibility(True)
        with slicer.util.WaitCursor():
            astroVolumeLogic().CalculateDisplayThresholdInROI(roi, self.volume)
        self.updateStatus()
        self.status.text += " (box)"


class HeaderSection:
    """Header of the dataset (FITS keywords, searchable) and its beam:
    estimate from the noise, edit, apply."""

    def __init__(self):
        self.volume = None
        self.section = foldSection("Header", "FITS keywords, beam", "Header")
        self.section.toolTip = "Header of the active dataset; the beam can be estimated from the noise."

        beamRow = qt.QHBoxLayout()
        beamRow.setSpacing(4)
        self.beamFields = []
        for name, unit, tip in (("BMAJ", "″", "Major axis (FWHM) of the beam, arc seconds."),
                                ("BMIN", "″", "Minor axis (FWHM) of the beam, arc seconds."),
                                ("BPA", "°", "Position angle of the major axis, degrees east of north.")):
            field = slicer.qSlicerAstroValueField()
            field.setObjectName(f"HeaderBeam_{name}")
            field.decimals = 2
            field.setRange(-180.0 if name == "BPA" else 0.0, 180.0 if name == "BPA" else 1e6)
            field.unit = unit
            field.toolTip = tip
            field.setMinimumWidth(50)
            self.beamFields.append(field)
            beamRow.addWidget(field, 1)
        self.estimateButton = iconButton("beam", "Estimate the beam from the noise (spatial autocorrelation of "
                                         "the first and last channels, about 10% accuracy); then Apply.", False,
                                         "HeaderEstimateBeam")
        self.estimateButton.connect("clicked()", self.estimateBeam)
        self.applyButton = iconButton("check", "Apply the beam to the data (saved in the FITS header).", False,
                                      "HeaderApplyBeam")
        self.applyButton.connect("clicked()", self.applyBeam)
        beamRow.addWidget(self.estimateButton)
        beamRow.addWidget(self.applyButton)
        beamLabel = label("Beam", "Beam of the data (needed for fluxes in Jy and column densities).")
        row = qt.QHBoxLayout()
        row.addWidget(beamLabel)
        row.addLayout(beamRow, 1)
        self.section.contentLayout().addLayout(row)
        self.beamStatus = qt.QLabel()
        self.beamStatus.setObjectName("HeaderBeamStatus")
        self.beamStatus.setProperty("astroRole", "monoLabel")
        self.beamStatus.wordWrap = True
        self.beamStatus.toolTip = "State of the beam."
        self.section.contentLayout().addWidget(self.beamStatus)

        self.filter = qt.QLineEdit()
        self.filter.setObjectName("HeaderFilter")
        self.filter.placeholderText = "Filter keywords"
        self.filter.toolTip = "Show only the keywords (or values) containing this text."
        self.filter.addAction(theme().icon("search", "subtle"), qt.QLineEdit.LeadingPosition)
        self.filter.connect("textChanged(QString)", lambda text: self.fillTable())
        self.section.contentLayout().addWidget(self.filter)
        self.table = qt.QTableWidget()
        self.table.setObjectName("HeaderTable")
        self.table.setColumnCount(2)
        self.table.setHorizontalHeaderLabels(["Keyword", "Value"])
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.verticalHeader().visible = False
        self.table.setEditTriggers(qt.QAbstractItemView.NoEditTriggers)
        self.table.setSelectionBehavior(qt.QAbstractItemView.SelectRows)
        self.table.setMinimumHeight(180)
        self.table.setMaximumHeight(260)
        self.table.setMinimumWidth(80)
        self.table.toolTip = "FITS keywords of the dataset (as read, with the values used by SlicerAstro)."
        self.section.contentLayout().addWidget(self.table)

    def setVolume(self, volume):
        self.volume = volume
        self.section.visible = volume is not None
        if volume is None:
            return
        self.updateBeam()
        self.fillTable()

    def keywords(self):
        result = []
        for name in self.volume.GetAttributeNames():
            if name.startswith("SlicerAstro."):
                result.append((name[len("SlicerAstro."):], self.volume.GetAttribute(name)))
        return sorted(result)

    def fillTable(self):
        if self.volume is None:
            return
        text = self.filter.text.strip().lower()
        rows = [(key, value) for key, value in self.keywords()
                if not text or text in key.lower() or text in (value or "").lower()]
        self.table.setRowCount(len(rows))
        for row, (key, value) in enumerate(rows):
            keyItem = qt.QTableWidgetItem(key)
            valueItem = qt.QTableWidgetItem(value)
            valueItem.setToolTip(f"{key} = {value}")
            self.table.setItem(row, 0, keyItem)
            self.table.setItem(row, 1, valueItem)
        self.table.resizeColumnToContents(0)

    def updateBeam(self):
        values = [Astro.floatAttribute(self.volume, key) for key in ("BMAJ", "BMIN", "BPA")]
        defined = all(math.isfinite(value) for value in values[:2]) and values[0] > 0 and values[1] > 0
        for index, field in enumerate(self.beamFields):
            value = values[index] if math.isfinite(values[index]) else 0.0
            field.value = value * 3600.0 if index < 2 else value
        self.beamStatus.text = "" if defined else "No beam: estimate it or enter it, then apply"

    def estimateBeam(self):
        beam = [0.0, 0.0, 0.0]
        with slicer.util.WaitCursor():
            ok = slicer.vtkSlicerAstroVolumeLogic.EstimateBeamFromNoise(self.volume, beam)
        if not ok:
            self.beamStatus.text = "The beam cannot be estimated (noise not correlated, or image too small)"
            return
        self.beamFields[0].value = beam[0] * 3600.0
        self.beamFields[1].value = beam[1] * 3600.0
        self.beamFields[2].value = beam[2] - 180.0 if beam[2] > 90.0 else beam[2]
        self.beamStatus.text = "Beam ESTIMATED from the noise: check it, then apply"

    def applyBeam(self):
        bmaj, bmin, bpa = (field.value for field in self.beamFields)
        if bmaj <= 0 or bmin <= 0:
            self.beamStatus.text = "Enter positive major and minor axes"
            return
        bmaj, bmin = max(bmaj, bmin), min(bmaj, bmin)
        wasModifying = self.volume.StartModify()
        self.volume.SetAttribute("SlicerAstro.BMAJ", repr(bmaj / 3600.0))
        self.volume.SetAttribute("SlicerAstro.BMIN", repr(bmin / 3600.0))
        self.volume.SetAttribute("SlicerAstro.BPA", repr(bpa))
        self.volume.EndModify(wasModifying)
        self.beamStatus.text = "Beam applied (saved in the FITS header)"
        self.fillTable()
