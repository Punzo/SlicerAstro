"""View bars of the Workbench: a slim bar on top of each view, on the dark
view background in both themes, replacing Slicer's view controllers.

Slice views: what the view shows (e.g. "XY · ch 59 · 132.45 km/s", "PV",
a map with its unit), orientation, layers (background, foreground and its
opacity), WCS axes, slice in 3D, fit and maximize; the slice offset slider of Slicer is shown
vertically along the right side of the view.
3D view: the volume and its rendering, look from an axis (astronomical axis
labels), reset the camera, show/hide the volume rendering, maximize."""

import ctk
import qt

import slicer
import vtk

from . import Astro

ACCENT = "#F2B24C"
# a volume rendering hidden with the eye of a 3D view bar: its view still
# shows the masks of the cube
HIDDEN_BY_EYE = "SlicerAstro.RenderingHiddenInView"


def viewColor(token):
    """A view token of the theme (dark views in the dark theme, light views
    in the light theme): view, view-text, view-subtle, view-line,
    view-hover, view-groove, view-data."""
    return theme().color(token).name()


def barStyle():
    return f"""
QFrame#AstroViewBar {{ background: {viewColor("view")}; border: none;
  border-bottom: 1px solid {viewColor("view-line")}; }}
QLabel {{ background: transparent; color: {viewColor("view-text")}; font-family: "Atkinson Hyperlegible Next";
  font-size: 12px; }}
QLabel#AstroViewDetail {{ color: {viewColor("view-subtle")}; font-family: "Atkinson Hyperlegible Mono";
  font-size: 11px; }}
QToolButton {{ background: transparent; border: none; border-radius: 6px; padding: 2px; }}
QToolButton:hover {{ background: {viewColor("view-hover")}; }}
QToolButton:checked {{ background: rgba(242, 178, 76, 40); }}
QToolButton::menu-indicator {{ image: none; width: 0px; }}
"""


def sliderStyle():
    groove = viewColor("view-groove")
    return f"""
QSlider {{ background: {viewColor("view")}; }}
QSlider::groove:vertical {{ width: 4px; background: {groove}; border-radius: 2px; margin: 4px 0px; }}
QSlider::sub-page:vertical {{ background: {groove}; border-radius: 2px; }}
QSlider::add-page:vertical {{ background: {groove}; border-radius: 2px; }}
QSlider::handle:vertical {{ height: 22px; width: 10px; margin: 0px -3px; border-radius: 5px; background: {ACCENT}; }}
QSlider::handle:vertical:hover {{ background: #F6C26C; }}
"""


ONE_UP = {
    "Red": slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpRedSliceView,
    "Yellow": slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpYellowSliceView,
    "Green": slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpGreenSliceView,
}


def theme():
    return slicer.app.property("SlicerAstroTheme")


def viewIcon(name):
    return theme().icon(name, "view-subtle")


class ViewBar(qt.QFrame):
    """Base of the bars: tag, detail and tool buttons on the right."""

    def __init__(self, workbench, parent=None):
        qt.QFrame.__init__(self, parent)
        self.workbench = workbench
        self.setObjectName("AstroViewBar")
        self.setFixedHeight(28)
        self.setStyleSheet(barStyle())
        self.barLayout = qt.QHBoxLayout(self)
        self.barLayout.setContentsMargins(10, 0, 6, 0)
        self.barLayout.setSpacing(6)
        self.tag = qt.QLabel()
        self.tag.setObjectName("AstroViewTag")
        self.tag.setStyleSheet("font-weight: 700;")
        self.detail = qt.QLabel()
        self.detail.setObjectName("AstroViewDetail")
        self.barLayout.addWidget(self.tag)
        self.barLayout.addWidget(self.detail, 1)
        self.observers = []

    def button(self, iconName, toolTip, checkable=False, objectName=None):
        button = qt.QToolButton()
        button.setIcon(viewIcon(iconName))
        button.setIconSize(qt.QSize(15, 15))
        button.toolTip = toolTip
        button.checkable = checkable
        button.setAutoRaise(True)
        if objectName:
            button.setObjectName(objectName)
        self.barLayout.addWidget(button)
        return button

    def menuButton(self, iconName, toolTip, objectName):
        button = self.button(iconName, toolTip, objectName=objectName)
        menu = qt.QMenu(button)
        theme().applyTo(menu)
        button.setMenu(menu)
        button.setPopupMode(qt.QToolButton.InstantPopup)
        return button, menu

    def observe(self, node, event, callback):
        if node is not None:
            self.observers.append((node, node.AddObserver(event, callback)))

    def cleanup(self):
        for node, tag in self.observers:
            node.RemoveObserver(tag)
        self.observers = []


class SliceViewBar(ViewBar):

    def __init__(self, workbench, sliceWidget, parent=None):
        ViewBar.__init__(self, workbench, parent)
        self.sliceWidget = sliceWidget
        self.sliceNode = sliceWidget.mrmlSliceNode()
        self.compositeNode = sliceWidget.mrmlSliceCompositeNode()
        self.viewName = self.sliceNode.GetLayoutName()
        self.setObjectName("AstroViewBar")
        self.setProperty("viewName", self.viewName)

        self.orientationButton, self.orientationMenu = self.menuButton(
            "viewOrientation", "Orientation of the slice: sky plane (XY) or position-velocity (XZ, ZY)",
            f"ViewOrientation_{self.viewName}")
        for orientation, text in (("XY", "XY · sky plane"), ("XZ", "XZ · position-velocity (RA)"),
                                  ("ZY", "ZY · position-velocity (Dec)")):
            action = self.orientationMenu.addAction(text)
            action.setData(orientation)
            action.connect("triggered()", lambda o=orientation: self.setOrientation(o))
        self.layersButton, self.layersMenu = self.menuButton(
            "layers", "Data shown in the view", f"ViewLayers_{self.viewName}")
        self.layersMenu.connect("aboutToShow()", self.fillLayersMenu)
        self.beamButton = self.button("beam", "Show the beam of the data", True, f"ViewBeam_{self.viewName}")
        self.beamButton.connect("toggled(bool)", self.setBeamVisible)
        self.frame = None
        self.axesButton = self.button("wcsAxes", "Show the WCS axes (coordinates along the borders of the view)", True,
                                      f"ViewAxes_{self.viewName}")
        self.axesButton.connect("toggled(bool)", self.setAxesVisible)
        self.slice3DButton = self.button("sliceIn3D", "Show this slice in the 3D views", True,
                                         f"ViewSliceIn3D_{self.viewName}")
        self.slice3DButton.connect("toggled(bool)", self.setSliceVisibleIn3D)
        self.fitButton = self.button("viewFit", "Fit the data to the view", objectName=f"ViewFit_{self.viewName}")
        self.fitButton.connect("clicked()", self.fit)
        self.maximizeButton = self.button("viewMaximize", "Maximize this view / back to the previous layout",
                                          objectName=f"ViewMaximize_{self.viewName}")
        self.maximizeButton.connect("clicked()", self.toggleMaximized)

        self.observe(self.sliceNode, "ModifiedEvent", self.refresh)
        self.observe(self.compositeNode, "ModifiedEvent", self.refresh)
        self.refresh()

    def volume(self):
        volumeID = self.compositeNode.GetBackgroundVolumeID()
        return slicer.mrmlScene.GetNodeByID(volumeID) if volumeID else None

    def setBeamVisible(self, visible):
        self.sliceNode.SetAttribute("SlicerAstro.Beam", "on" if visible else "off")
        self.sliceNode.Modified()

    def updateOverlays(self, volume):
        """Compass and scale bar of a sky view (from the WCS)."""
        if self.frame is None:
            return
        geometry = Astro.skyGeometry(volume, self.sliceNode) if volume is not None else None
        if geometry is None:
            self.frame.setCompass(False, 0, -1, -1, 0)
            self.frame.setScaleBar(0, "")
            return
        arcsecPerPixel, north, east = geometry
        ratio = self.sliceWidget.sliceView().devicePixelRatioF()
        self.frame.setCompass(True, north[0], north[1], east[0], east[1])
        pixels, label = Astro.scaleBar(arcsecPerPixel * ratio)
        self.frame.setScaleBar(pixels, label)

    def refresh(self, caller=None, event=None):
        volume = self.volume()
        orientation = self.sliceNode.GetOrientation()
        beam = Astro.isAstroVolume(volume) and Astro.hasBeam(volume) and orientation == "XY"
        self.beamButton.visible = bool(beam)
        self.beamButton.blockSignals(True)
        self.beamButton.checked = self.sliceNode.GetAttribute("SlicerAstro.Beam") == "on"
        self.beamButton.blockSignals(False)
        self.updateOverlays(volume)
        for button, checked in ((self.axesButton,
                                 self.sliceNode.GetRulerType() != slicer.vtkMRMLAbstractViewNode.RulerTypeNone),
                                (self.slice3DButton, bool(self.sliceNode.GetSliceVisible()))):
            button.blockSignals(True)
            button.checked = checked
            button.blockSignals(False)
        if volume is None:
            self.tag.text = orientation
            self.detail.text = "no data"
            return
        if Astro.dimensions(volume) >= 3 and orientation == "XY":
            channel = Astro.channelOfSlice(volume, self.sliceNode)
            velocities = Astro.channelVelocities(volume)
            self.tag.text = "XY"
            if 0 <= channel < len(velocities):
                self.detail.text = f"ch {channel} · {velocities[channel]:.2f} {Astro.spectralUnit(volume)}"
                # a product (e.g. a smoothed copy): its name, so that the
                # views can be told apart
                if volume is not self.workbench.activeVolume():
                    self.detail.text = f"{volume.GetName()} · {self.detail.text}"
            else:
                self.detail.text = volume.GetName()
        elif Astro.dimensions(volume) >= 3:
            self.tag.text = "PV"
            self.detail.text = f"{orientation} · {volume.GetName()}"
        else:
            self.tag.text = volume.GetName()
            self.detail.text = Astro.displayUnit(Astro.attribute(volume, "BUNIT", ""))
        layoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode()
        maximized = layoutNode.GetViewArrangement() == ONE_UP.get(self.viewName)
        self.maximizeButton.setIcon(viewIcon("viewRestore" if maximized else "viewMaximize"))

    def setOrientation(self, orientation):
        # another orientation first: the slice is recomputed for the data
        self.sliceNode.SetOrientation("XZ" if orientation == "XY" else "XY")
        self.sliceNode.SetOrientation(orientation)
        self.fit()

    def fillLayersMenu(self):
        self.layersMenu.clear()
        volumes = Astro.astroVolumes(slicer.mrmlScene)
        background = self.layersMenu.addMenu("Background")
        foreground = self.layersMenu.addMenu("Foreground")
        none = foreground.addAction("None")
        none.checkable = True
        none.checked = not self.compositeNode.GetForegroundVolumeID()
        none.connect("triggered()", lambda: self.compositeNode.SetForegroundVolumeID(None))
        for volume in volumes:
            # background: the view is kept for data on the same grid (fitted
            # otherwise); foreground: over the background, the view kept
            action = background.addAction(volume.GetName())
            action.checkable = True
            action.checked = self.compositeNode.GetBackgroundVolumeID() == volume.GetID()
            action.connect("triggered()", lambda v=volume: self.workbench.showInSliceView(
                self.sliceNode, self.compositeNode, v, clearForeground=False))
            action = foreground.addAction(volume.GetName())
            action.checkable = True
            action.checked = self.compositeNode.GetForegroundVolumeID() == volume.GetID()
            action.connect("triggered()", lambda v=volume: self.compositeNode.SetForegroundVolumeID(v.GetID()))
        self.layersMenu.addSeparator()
        opacity = qt.QWidgetAction(self.layersMenu)
        row = qt.QWidget()
        rowLayout = qt.QHBoxLayout(row)
        rowLayout.setContentsMargins(10, 4, 10, 4)
        label = qt.QLabel("Foreground opacity")
        label.setProperty("astroRole", "label")
        slider = qt.QSlider(qt.Qt.Horizontal)
        slider.setRange(0, 100)
        slider.value = int(round(self.compositeNode.GetForegroundOpacity() * 100))
        slider.connect("valueChanged(int)", lambda value: self.compositeNode.SetForegroundOpacity(value / 100.0))
        rowLayout.addWidget(label)
        rowLayout.addWidget(slider)
        opacity.setDefaultWidget(row)
        self.layersMenu.addAction(opacity)

    def setAxesVisible(self, visible):
        """WCS axes of the view (SlicerAstro draws them with the ruler type)."""
        self.sliceNode.SetRulerType(slicer.vtkMRMLAbstractViewNode.RulerTypeThin if visible
                                    else slicer.vtkMRMLAbstractViewNode.RulerTypeNone)

    def setSliceVisibleIn3D(self, visible):
        self.sliceNode.SetSliceVisible(visible)

    def fit(self):
        sliceLogic = slicer.app.applicationLogic().GetSliceLogic(self.sliceNode)
        if sliceLogic is not None:
            sliceLogic.FitSliceToAll()

    def toggleMaximized(self):
        oneUp = ONE_UP.get(self.viewName)
        if oneUp is not None:
            self.workbench.toggleMaximized(oneUp)


class ThreeDViewBar(ViewBar):

    def __init__(self, workbench, threeDWidget, parent=None):
        ViewBar.__init__(self, workbench, parent)
        self.threeDWidget = threeDWidget
        self.viewNode = threeDWidget.mrmlViewNode()
        self.tag.text = "3D"
        self.visibilityButton = self.button("eye", "Show or hide the volume rendering (the masks stay shown)", True,
                                            "View3DVisibility")
        self.visibilityButton.connect("toggled(bool)", self.setRenderingVisible)
        self.axesButton, self.axesMenu = self.menuButton("viewCamera", "Look at the data from an axis (north, south, east, west, along the velocity)",
                                                         "View3DAxes")
        self.axesMenu.connect("aboutToShow()", self.fillAxesMenu)
        self.resetButton = self.button("viewFit", "Fit the data to the view (camera reset to the data)",
                                       objectName="View3DReset")
        self.resetButton.connect("clicked()", self.resetCamera)
        self.maximizeButton = self.button("viewMaximize", "Maximize this view / back to the previous layout",
                                          objectName="View3DMaximize")
        self.maximizeButton.connect("clicked()", self.toggleMaximized)
        self.refresh()

    def shownVolume(self):
        """The cube rendered in this view (e.g. a smoothed copy shown in
        place of the dataset), otherwise the active dataset."""
        rendered = self.workbench.visibility.renderedVolume(self.viewNode.GetID()) \
            if getattr(self.workbench, "visibility", None) is not None else None
        return rendered if rendered is not None else self.workbench.activeVolume()

    def renderingDisplayNode(self):
        volume = self.shownVolume()
        if volume is None:
            return None
        logic = slicer.modules.volumerendering.logic()
        return logic.GetFirstVolumeRenderingDisplayNode(volume)

    def refresh(self, caller=None, event=None):
        volume = self.shownVolume()
        displayNode = self.renderingDisplayNode()
        details = [volume.GetName()] if volume is not None else ["no data"]
        presetNode = volume.GetPresetNode() if volume is not None else None
        if displayNode is not None and presetNode is not None:
            details.append(presetNode.GetName())
        self.detail.text = " · ".join(details)
        self.visibilityButton.blockSignals(True)
        self.visibilityButton.checked = displayNode is not None and bool(displayNode.GetVisibility())
        self.visibilityButton.setIcon(viewIcon("eye" if self.visibilityButton.checked else "eyeOff"))
        self.visibilityButton.blockSignals(False)
        layoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode()
        maximized = layoutNode.GetViewArrangement() == slicer.vtkMRMLLayoutNode.SlicerLayoutOneUp3DView
        self.maximizeButton.setIcon(viewIcon("viewRestore" if maximized else "viewMaximize"))

    def setRenderingVisible(self, visible):
        """Only the volume rendering: the masks of the cube stay in the view
        (marked on the display node, see ViewVisibility.viewsShowing)."""
        displayNode = self.renderingDisplayNode()
        if displayNode is not None:
            if visible:
                displayNode.RemoveAttribute(HIDDEN_BY_EYE)
            else:
                displayNode.SetAttribute(HIDDEN_BY_EYE, "1")
            displayNode.SetVisibility(visible)
        self.refresh()

    def fillAxesMenu(self):
        self.axesMenu.clear()
        # axis labels of the view (astronomical labels set by SlicerAstro);
        # label order of the view node: -x +x -y +y -z +z
        axes = ((ctk.ctkAxesWidget.Right, 1), (ctk.ctkAxesWidget.Left, 0), (ctk.ctkAxesWidget.Anterior, 3),
                (ctk.ctkAxesWidget.Posterior, 2), (ctk.ctkAxesWidget.Superior, 5), (ctk.ctkAxesWidget.Inferior, 4))
        view = self.threeDWidget.threeDView()
        for axis, labelIndex in axes:
            label = self.viewNode.GetAxisLabel(labelIndex)
            action = self.axesMenu.addAction(f"From {label}")
            action.connect("triggered()", lambda a=axis: view.lookFromViewAxis(a))

    def resetCamera(self):
        view = self.threeDWidget.threeDView()
        view.resetFocalPoint()
        view.resetCamera()

    def toggleMaximized(self):
        self.workbench.toggleMaximized(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUp3DView)



def _rgb(hexColor):
    color = qt.QColor(hexColor)
    return color.red(), color.green(), color.blue()


class PlotViewBar(ViewBar):
    """Plot view: the chart shown (spectrum of the cube or profiles), fit
    and maximize. The chart is drawn on the view background with the text
    and lines of the view bars (dark or light, as the theme)."""

    def __init__(self, workbench, plotWidget, parent=None):
        ViewBar.__init__(self, workbench, parent)
        self.plotWidget = plotWidget
        self.viewNode = plotWidget.mrmlPlotViewNode()
        self.tag.text = "PLOT"
        self.fitButton = self.button("viewFit", "Fit the plot to its data", objectName="ViewPlotFit")
        self.fitButton.connect("clicked()", lambda: self.plotWidget.plotView().fitToContent())
        self.maximizeButton = self.button("viewMaximize", "Maximize this view / back to the views with a plot",
                                          objectName="ViewPlotMaximize")
        self.maximizeButton.connect("clicked()", self.toggleMaximized)
        self.observe(self.viewNode, vtk.vtkCommand.ModifiedEvent, self.refresh)
        self.saved = None
        self.applyChartStyle()
        self.refresh()

    def chart(self):
        view = self.plotWidget.plotView()
        return view.chart() if view is not None else None

    def renderer(self, chart):
        scene = chart.GetScene() if chart is not None else None
        return scene.GetRenderer() if scene is not None else None

    def chartParts(self, chart):
        axes = [chart.GetAxis(index) for index in range(4)]
        return [axis for axis in axes if axis is not None]

    def applyChartStyle(self):
        """The view colours of the theme: background, text, dim grid (Slicer's
        values kept to be restored when leaving the Workbench)."""
        chart = self.chart()
        if chart is None:
            return
        view = _rgb(viewColor("view"))
        text = [value / 255.0 for value in _rgb(viewColor("view-text"))]
        subtle = [value / 255.0 for value in _rgb(viewColor("view-subtle"))]
        line = _rgb(viewColor("view-line"))
        grid = _rgb(viewColor("view-hover"))
        if self.saved is None:
            background = chart.GetBackgroundBrush().GetColorObject()
            self.saved = {
                "background": (background.GetRed(), background.GetGreen(), background.GetBlue(),
                               background.GetAlpha()),
                "title": chart.GetTitleProperties().GetColor(),
                "legend": chart.GetLegend().GetLabelProperties().GetColor(),
                "renderer": self.renderer(chart).GetBackground() if self.renderer(chart) is not None else None,
                "axes": [],
            }
            for axis in self.chartParts(chart):
                pen = axis.GetPen().GetColorObject()
                gridPen = axis.GetGridPen().GetColorObject()
                self.saved["axes"].append({
                    "title": axis.GetTitleProperties().GetColor(),
                    "label": axis.GetLabelProperties().GetColor(),
                    "pen": (pen.GetRed(), pen.GetGreen(), pen.GetBlue(), pen.GetAlpha()),
                    "grid": (gridPen.GetRed(), gridPen.GetGreen(), gridPen.GetBlue(), gridPen.GetAlpha()),
                })
        chart.GetBackgroundBrush().SetColor(*view, 255)
        # the renderer too: an empty plot (no chart) shows only the renderer
        renderer = self.renderer(chart)
        if renderer is not None:
            renderer.SetBackground(*[value / 255.0 for value in view])
        chart.GetTitleProperties().SetColor(*text)
        chart.GetLegend().GetLabelProperties().SetColor(*text)
        chart.GetLegend().GetBrush().SetColor(*view, 220)
        chart.GetLegend().GetPen().SetColor(*line, 255)
        for axis in self.chartParts(chart):
            axis.GetTitleProperties().SetColor(*subtle)
            axis.GetLabelProperties().SetColor(*subtle)
            axis.GetPen().SetColor(*line, 255)
            axis.GetGridPen().SetColor(*grid, 255)
        self.renderPlot(chart)

    def restoreChartStyle(self):
        chart = self.chart()
        if chart is None or self.saved is None:
            return
        chart.GetBackgroundBrush().SetColor(*self.saved["background"])
        renderer = self.renderer(chart)
        if renderer is not None and self.saved.get("renderer") is not None:
            renderer.SetBackground(*self.saved["renderer"])
        chart.GetTitleProperties().SetColor(*self.saved["title"])
        chart.GetLegend().GetLabelProperties().SetColor(*self.saved["legend"])
        chart.GetLegend().GetBrush().SetColor(255, 255, 255, 255)
        chart.GetLegend().GetPen().SetColor(0, 0, 0, 255)
        for axis, saved in zip(self.chartParts(chart), self.saved["axes"]):
            axis.GetTitleProperties().SetColor(*saved["title"])
            axis.GetLabelProperties().SetColor(*saved["label"])
            axis.GetPen().SetColor(*saved["pen"])
            axis.GetGridPen().SetColor(*saved["grid"])
        self.saved = None
        self.renderPlot(chart)

    def renderPlot(self, chart):
        if chart.GetScene() is not None:
            chart.GetScene().SetDirty(True)
        self.plotWidget.plotView().update()

    def refresh(self, caller=None, event=None):
        chartID = self.viewNode.GetPlotChartNodeID() if self.viewNode is not None else None
        chart = slicer.mrmlScene.GetNodeByID(chartID) if chartID else None
        title = chart.GetTitle() if chart is not None else ""
        self.detail.text = title or (chart.GetName() if chart is not None else
                                     "no profile yet · compute one in Process › Profiles")
        layoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode()
        maximized = layoutNode.GetViewArrangement() == slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpPlotView
        self.maximizeButton.setIcon(viewIcon("viewRestore" if maximized else "viewMaximize"))

    def toggleMaximized(self):
        self.workbench.toggleMaximized(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpPlotView)

    def cleanup(self):
        self.restoreChartStyle()
        ViewBar.cleanup(self)


class ViewDecorator:
    """Installs the view bars and the vertical slice sliders on the views
    (also on views created later by a change of layout) and removes them."""

    def __init__(self, workbench):
        self.workbench = workbench
        self.bars = {}
        self.frames = {}
        self.saved = {}
        self.viewport = None

    def install(self):
        layoutManager = slicer.app.layoutManager()
        for name in layoutManager.sliceViewNames():
            widget = layoutManager.sliceWidget(name)
            if widget is None or name in self.bars:
                continue
            self.saved[name] = widget.sliceOffsetSliderOrientation
            # the beam of the data in the slice views
            sliceNode = widget.mrmlSliceNode()
            self.saved[f"beam:{name}"] = sliceNode.GetAttribute("SlicerAstro.Beam")
            sliceNode.SetAttribute("SlicerAstro.Beam", "on")
            widget.sliceController().visible = False
            widget.setSliceOffsetSliderOrientation(qt.Qt.Vertical)
            # Slicer's background of the slice view (outside the data)
            color = widget.sliceView().backgroundColor
            self.saved[f"background:{name}"] = qt.QColor(color.red(), color.green(), color.blue())
            bar = SliceViewBar(self.workbench, widget)
            widget.layout().insertWidget(0, bar)
            self.bars[name] = bar
            self.addFrame(name, widget, widget.sliceView())
        for index in range(layoutManager.threeDViewCount):
            widget = layoutManager.threeDWidget(index)
            key = f"3D{index}"
            if widget is None or key in self.bars:
                continue
            widget.threeDController().visible = False
            viewNode = widget.mrmlViewNode()
            self.saved[key] = viewNode.GetBoxVisible()
            viewNode.SetBoxVisible(False)
            bar = ThreeDViewBar(self.workbench, widget)
            widget.layout().insertWidget(0, bar)
            self.bars[key] = bar
            self.addFrame(key, widget, widget.threeDView())
        for index in range(layoutManager.plotViewCount):
            widget = layoutManager.plotWidget(index)
            key = f"Plot{index}"
            if widget is None or key in self.bars:
                continue
            widget.plotController().visible = False
            bar = PlotViewBar(self.workbench, widget)
            widget.layout().insertWidget(0, bar)
            self.bars[key] = bar
            self.addFrame(key, widget, widget.plotView())
        for bar in self.bars.values():
            bar.refresh()
        self.styleViewport()
        self.setActive(self.workbench.activeViewKey)

    def styleViews(self):
        """Bars, slice sliders, slice backgrounds and plots in the view colours
        of the theme (again when the theme changes)."""
        layoutManager = slicer.app.layoutManager()
        background = qt.QColor(viewColor("view"))
        for key, bar in self.bars.items():
            bar.setStyleSheet(barStyle())
            if isinstance(bar, PlotViewBar):
                bar.applyChartStyle()
            elif not key.startswith("3D"):
                widget = layoutManager.sliceWidget(key)
                if widget is None:
                    continue
                controller = widget.sliceVerticalController()
                controller.setStyleSheet(f"background: {viewColor('view')};")
                for slider in controller.findChildren("QSlider"):
                    slider.setStyleSheet(sliderStyle())
                widget.sliceView().setBackgroundColor(background)
                widget.sliceView().scheduleRender()

    def addFrame(self, key, container, view):
        """Rounded corners, active border and drop target of a view."""
        frame = slicer.qSlicerAstroViewFrame()
        frame.attach(container, view)
        frame.setObjectName(f"AstroViewFrame_{key}")
        frame.connect("activated()", lambda k=key: self.workbench.setActiveView(k))
        frame.connect("nodeDropped(QString)", lambda nodeID, k=key: self.workbench.dropNode(k, nodeID))
        self.frames[key] = frame
        bar = self.bars.get(key)
        if bar is not None and hasattr(bar, "frame"):
            bar.frame = frame
            bar.refresh()

    def setActive(self, key):
        for name, frame in self.frames.items():
            frame.active = name == key

    def styleViewport(self, caller=None):
        """The area around the views in the colour of the corners."""
        viewport = slicer.app.layoutManager().viewport()
        if self.viewport is None:
            self.viewport = (viewport.objectName, viewport.styleSheet)
            if not viewport.objectName:
                viewport.setObjectName("AstroWorkbenchViewport")
            theme().connect("themeChanged()", self.styleViewport)
        bg = theme().color("bg").name()
        viewport.setStyleSheet(f"QWidget#{viewport.objectName} {{ background: {bg}; }}"
                               f"QWidget#{viewport.objectName} QSplitter::handle {{ background: {bg}; }}")
        self.styleViews()

    def refresh(self):
        for bar in self.bars.values():
            bar.refresh()

    def remove(self):
        layoutManager = slicer.app.layoutManager()
        for key, bar in self.bars.items():
            bar.cleanup()
            # deleted now (not at the exit of the application, after Python)
            if bar.parentWidget() is not None and bar.parentWidget().layout() is not None:
                bar.parentWidget().layout().removeWidget(bar)
            bar.delete()
            if key.startswith("3D"):
                widget = layoutManager.threeDWidget(int(key[2:]))
                if widget is not None:
                    widget.threeDController().visible = True
                    widget.mrmlViewNode().SetBoxVisible(self.saved.get(key, True))
            elif key.startswith("Plot"):
                widget = layoutManager.plotWidget(int(key[4:]))
                if widget is not None:
                    widget.plotController().visible = True
            else:
                widget = layoutManager.sliceWidget(key)
                if widget is not None:
                    widget.sliceController().visible = True
                    controller = widget.sliceVerticalController()
                    controller.setStyleSheet("")
                    for slider in controller.findChildren("QSlider"):
                        slider.setStyleSheet("")
                    background = self.saved.get(f"background:{key}")
                    if background is not None:
                        widget.sliceView().setBackgroundColor(background)
                        widget.sliceView().scheduleRender()
                    widget.setSliceOffsetSliderOrientation(self.saved.get(key, qt.Qt.Horizontal))
                    beam = self.saved.get(f"beam:{key}")
                    sliceNode = widget.mrmlSliceNode()
                    if beam is None:
                        sliceNode.RemoveAttribute("SlicerAstro.Beam")
                    else:
                        sliceNode.SetAttribute("SlicerAstro.Beam", beam)
                    sliceNode.Modified()
        for frame in self.frames.values():
            frame.delete()
        self.frames = {}
        if self.viewport is not None:
            theme().disconnect("themeChanged()", self.styleViewport)
            viewport = layoutManager.viewport()
            viewport.setStyleSheet(self.viewport[1])
            viewport.setObjectName(self.viewport[0])
            self.viewport = None
        self.bars = {}
        self.saved = {}


class ViewVisibility:
    """Which data each view shows, with the view node IDs of the display
    nodes (ShowInAllViewsByDefault off: a display node without views is
    shown nowhere, not everywhere):

    - masks and contours (segmentations) are shown in the views that show
      their cube (as background or foreground of a slice view, or rendered
      in a 3D view), plus the views they were dropped on;
    - a cube dropped on a 3D view is rendered there only.

    Leaving the Workbench restores the display nodes (shown in all views)."""

    SEGMENTATION_DISPLAY = "vtkMRMLSegmentationDisplayNode"

    def __init__(self):
        self.active = False
        self.saved = {}
        self.extra = {}
        self.defaultNode = None
        self.observers = []
        self.observedRenderings = set()
        self.timer = qt.QTimer()
        self.timer.setSingleShot(True)
        self.timer.setInterval(50)
        self.timer.connect("timeout()", self.update)
        # called after each update (e.g. the 3D view bars follow the
        # renderings shown or hidden anywhere: Inspect, panels, logic)
        self.onUpdated = None

    def enter(self):
        scene = slicer.mrmlScene
        self.defaultNode = scene.GetDefaultNodeByClass(self.SEGMENTATION_DISPLAY)
        default = slicer.vtkMRMLSegmentationDisplayNode()
        default.SetShowInAllViewsByDefault(False)
        scene.AddDefaultNode(default)
        for displayNode in slicer.util.getNodesByClass(self.SEGMENTATION_DISPLAY):
            self.saved[displayNode.GetID()] = (displayNode.GetShowInAllViewsByDefault(),
                                               list(displayNode.GetViewNodeIDs()))
        for composite in slicer.util.getNodesByClass("vtkMRMLSliceCompositeNode"):
            self.observers.append((composite, composite.AddObserver("ModifiedEvent", self.schedule)))
        self.observers.append((scene, scene.AddObserver(scene.NodeAddedEvent, self.schedule)))
        self.active = True
        self.update()

    def schedule(self, caller=None, event=None):
        if self.active:
            self.timer.start()

    def viewsShowing(self):
        """View node IDs showing each volume (ID)."""
        showing = {}
        for composite in slicer.util.getNodesByClass("vtkMRMLSliceCompositeNode"):
            sliceNode = slicer.mrmlScene.GetSingletonNode(composite.GetLayoutName(), "vtkMRMLSliceNode")
            if sliceNode is None:
                continue
            for volumeID in (composite.GetBackgroundVolumeID(), composite.GetForegroundVolumeID()):
                if volumeID:
                    showing.setdefault(volumeID, set()).add(sliceNode.GetID())
        viewNodes = slicer.util.getNodesByClass("vtkMRMLViewNode")
        activeID = slicer.app.applicationLogic().GetSelectionNode().GetActiveVolumeID()
        for displayNode in slicer.util.getNodesByClass("vtkMRMLVolumeRenderingDisplayNode"):
            volume = displayNode.GetDisplayableNode()
            if volume is None:
                continue
            if displayNode.GetVisibility():
                if displayNode.GetAttribute(HIDDEN_BY_EYE):
                    displayNode.RemoveAttribute(HIDDEN_BY_EYE)
            elif not displayNode.GetAttribute(HIDDEN_BY_EYE) or \
                    activeID not in (volume.GetID(), volume.GetAttribute("SlicerAstro.ShownInPlaceOf")):
                # hidden (not with the eye of the view bar, or no more the
                # active dataset): its masks leave the view
                continue
            for viewNode in viewNodes:
                if displayNode.IsDisplayableInView(viewNode.GetID()):
                    showing.setdefault(volume.GetID(), set()).add(viewNode.GetID())
        # a product shown in place of its dataset (smoothed cube, map...):
        # the masks of the dataset stay in its views
        for volume in slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode"):
            datasetID = volume.GetAttribute("SlicerAstro.ShownInPlaceOf")
            if datasetID and volume.GetID() in showing:
                showing.setdefault(datasetID, set()).update(showing[volume.GetID()])
        return showing

    def allViews(self):
        return [node.GetID() for node in slicer.util.getNodesByClass("vtkMRMLSliceNode")] + \
               [node.GetID() for node in slicer.util.getNodesByClass("vtkMRMLViewNode")]

    def observeRenderings(self):
        """3D renderings shown or hidden (e.g. the automatic 3D display of the
        active cube): the masks of the cube follow it into the 3D views."""
        for displayNode in slicer.util.getNodesByClass("vtkMRMLVolumeRenderingDisplayNode"):
            if displayNode.GetID() in self.observedRenderings:
                continue
            self.observedRenderings.add(displayNode.GetID())
            self.observers.append((displayNode, displayNode.AddObserver("ModifiedEvent", self.schedule)))

    def update(self):
        if not self.active:
            return
        self.observeRenderings()
        showing = self.viewsShowing()
        role = slicer.vtkMRMLSegmentationNode.GetReferenceImageGeometryReferenceRole()
        for segmentation in slicer.util.getNodesByClass("vtkMRMLSegmentationNode"):
            displayNode = segmentation.GetDisplayNode()
            if displayNode is None:
                continue
            if displayNode.GetID() not in self.saved:
                self.saved[displayNode.GetID()] = (True, [])
            reference = segmentation.GetNodeReference(role)
            views = set(showing.get(reference.GetID(), set())) if reference is not None else set(self.allViews())
            views |= self.extra.get(segmentation.GetID(), set())
            displayNode.SetShowInAllViewsByDefault(False)
            if set(displayNode.GetViewNodeIDs()) != views:
                displayNode.SetViewNodeIDs(sorted(views))
        if self.onUpdated is not None:
            self.onUpdated()

    def addToView(self, segmentation, viewNodeID):
        self.extra.setdefault(segmentation.GetID(), set()).add(viewNodeID)
        displayNode = segmentation.GetDisplayNode()
        if displayNode is not None:
            displayNode.SetVisibility(True)
        self.update()

    def renderedVolume(self, viewNodeID):
        """The astronomical volume rendered in a 3D view (None if none)."""
        for displayNode in slicer.util.getNodesByClass("vtkMRMLVolumeRenderingDisplayNode"):
            volume = displayNode.GetDisplayableNode()
            if (volume is not None and volume.IsA("vtkMRMLAstroVolumeNode") and displayNode.GetVisibility()
                    and displayNode.IsDisplayableInView(viewNodeID)):
                return volume
        return None

    def renderInView(self, volume, viewNodeID, centerCamera=True):
        """Volume rendering of a cube in one 3D view (the other volumes are
        no more rendered there); centerCamera: the camera is pointed at the
        cube, otherwise kept."""
        logic = slicer.modules.volumerendering.logic()
        viewIDs = [node.GetID() for node in slicer.util.getNodesByClass("vtkMRMLViewNode")]
        displayNode = logic.GetFirstVolumeRenderingDisplayNode(volume)
        if displayNode is None:
            displayNode = logic.CreateDefaultVolumeRenderingNodes(volume)
        for other in slicer.util.getNodesByClass("vtkMRMLVolumeRenderingDisplayNode"):
            if other is displayNode or not other.IsDisplayableInView(viewNodeID):
                continue
            keep = [viewID for viewID in viewIDs if viewID != viewNodeID and other.IsDisplayableInView(viewID)]
            other.SetShowInAllViewsByDefault(False)
            other.SetViewNodeIDs(keep)
        if displayNode is not None:
            keep = [viewID for viewID in viewIDs if viewID != viewNodeID and displayNode.IsDisplayableInView(viewID)
                    and list(displayNode.GetViewNodeIDs())]
            displayNode.SetShowInAllViewsByDefault(False)
            displayNode.SetViewNodeIDs(keep + [viewNodeID])
            displayNode.SetVisibility(True)
            # fitted and centred in the view (cameras of the views showing it)
            if volume.IsA("vtkMRMLAstroVolumeNode"):
                slicer.modules.astrovolume.logic().SetVolumeRenderingVisibility(volume, True, centerCamera)
        self.update()

    def exit(self):
        self.active = False
        self.timer.stop()
        for node, tag in self.observers:
            node.RemoveObserver(tag)
        self.observers = []
        self.observedRenderings = set()
        scene = slicer.mrmlScene
        if self.defaultNode is not None:
            scene.AddDefaultNode(self.defaultNode)
        else:
            scene.AddDefaultNode(slicer.vtkMRMLSegmentationDisplayNode())
        self.defaultNode = None
        for displayNodeID, (showAll, viewIDs) in self.saved.items():
            displayNode = scene.GetNodeByID(displayNodeID)
            if displayNode is not None:
                displayNode.SetViewNodeIDs(viewIDs)
                displayNode.SetShowInAllViewsByDefault(showAll)
        self.saved = {}
        self.extra = {}
