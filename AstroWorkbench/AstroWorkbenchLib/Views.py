"""View bars of the Workbench: a slim bar on top of each view, on the dark
view background in both themes, replacing Slicer's view controllers.

Slice views: what the view shows (e.g. "XY · ch 59 · 132.45 km/s", "PV",
a map with its unit), orientation, layers (background, foreground and its
opacity), link, fit and maximize; the slice offset slider of Slicer is shown
vertically along the right side of the view.
3D view: the volume and its rendering, look from an axis (astronomical axis
labels), reset the camera, show/hide the volume rendering, maximize."""

import ctk
import qt

import slicer

from . import Astro

VIEW = "#070A12"
VIEW_LINE = "#26304A"
VIEW_TEXT = "#E7EBF3"
VIEW_SUBTLE = "#8592AC"
ACCENT = "#F2B24C"

BAR_STYLE = f"""
QFrame#AstroViewBar {{ background: {VIEW}; border: none; border-bottom: 1px solid {VIEW_LINE}; }}
QLabel {{ background: transparent; color: {VIEW_TEXT}; font-family: "Atkinson Hyperlegible Next"; font-size: 12px; }}
QLabel#AstroViewDetail {{ color: {VIEW_SUBTLE}; font-family: "Atkinson Hyperlegible Mono"; font-size: 11px; }}
QToolButton {{ background: transparent; border: none; border-radius: 6px; padding: 2px; }}
QToolButton:hover {{ background: #202A42; }}
QToolButton:checked {{ background: rgba(242, 178, 76, 40); }}
QToolButton::menu-indicator {{ image: none; width: 0px; }}
"""

SLIDER_STYLE = f"""
QSlider {{ background: {VIEW}; }}
QSlider::groove:vertical {{ width: 4px; background: #34405F; border-radius: 2px; margin: 4px 0px; }}
QSlider::sub-page:vertical {{ background: #34405F; border-radius: 2px; }}
QSlider::add-page:vertical {{ background: #34405F; border-radius: 2px; }}
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
        self.setStyleSheet(BAR_STYLE)
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
            "compass", "Orientation of the view", f"ViewOrientation_{self.viewName}")
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
        self.linkButton = self.button("link", "Link the views (same slice, zoom and layers)", True,
                                      f"ViewLink_{self.viewName}")
        self.linkButton.connect("toggled(bool)", self.setLinked)
        self.fitButton = self.button("target", "Fit the data to the view", objectName=f"ViewFit_{self.viewName}")
        self.fitButton.connect("clicked()", self.fit)
        self.maximizeButton = self.button("layout1", "Maximize this view / back to four views",
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
        self.linkButton.blockSignals(True)
        self.linkButton.checked = bool(self.compositeNode.GetLinkedControl())
        self.linkButton.blockSignals(False)
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
            else:
                self.detail.text = volume.GetName()
        elif Astro.dimensions(volume) >= 3:
            self.tag.text = "PV"
            self.detail.text = f"{orientation} · {volume.GetName()}"
        else:
            self.tag.text = volume.GetName()
            self.detail.text = Astro.attribute(volume, "BUNIT", "").replace("JY/BEAM", "Jy/beam")
        layoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode()
        maximized = layoutNode.GetViewArrangement() == ONE_UP.get(self.viewName)
        self.maximizeButton.setIcon(viewIcon("layout4" if maximized else "layout1"))

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
            for menu, getter, setter in ((background, self.compositeNode.GetBackgroundVolumeID,
                                          self.compositeNode.SetBackgroundVolumeID),
                                         (foreground, self.compositeNode.GetForegroundVolumeID,
                                          self.compositeNode.SetForegroundVolumeID)):
                action = menu.addAction(volume.GetName())
                action.checkable = True
                action.checked = getter() == volume.GetID()
                action.connect("triggered()", lambda v=volume, s=setter: (s(v.GetID()), self.fit()))
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

    def setLinked(self, linked):
        for composite in slicer.util.getNodesByClass("vtkMRMLSliceCompositeNode"):
            composite.SetLinkedControl(linked)

    def fit(self):
        sliceLogic = slicer.app.applicationLogic().GetSliceLogic(self.sliceNode)
        if sliceLogic is not None:
            sliceLogic.FitSliceToAll()

    def toggleMaximized(self):
        layoutManager = slicer.app.layoutManager()
        arrangement = layoutManager.layoutLogic().GetLayoutNode().GetViewArrangement()
        oneUp = ONE_UP.get(self.viewName)
        if oneUp is None:
            return
        layoutManager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView if arrangement == oneUp else oneUp)
        self.workbench.styleViews()


class ThreeDViewBar(ViewBar):

    def __init__(self, workbench, threeDWidget, parent=None):
        ViewBar.__init__(self, workbench, parent)
        self.threeDWidget = threeDWidget
        self.viewNode = threeDWidget.mrmlViewNode()
        self.tag.text = "3D"
        self.visibilityButton = self.button("eye", "Show or hide the volume rendering", True, "View3DVisibility")
        self.visibilityButton.connect("toggled(bool)", self.setRenderingVisible)
        self.axesButton, self.axesMenu = self.menuButton("compass", "Look from an axis", "View3DAxes")
        self.axesMenu.connect("aboutToShow()", self.fillAxesMenu)
        self.resetButton = self.button("target", "Reset the camera to the data", objectName="View3DReset")
        self.resetButton.connect("clicked()", self.resetCamera)
        self.maximizeButton = self.button("layout1", "Maximize this view / back to four views",
                                          objectName="View3DMaximize")
        self.maximizeButton.connect("clicked()", self.toggleMaximized)
        self.refresh()

    def renderingDisplayNode(self):
        volume = self.workbench.activeVolume()
        if volume is None:
            return None
        logic = slicer.modules.volumerendering.logic()
        return logic.GetFirstVolumeRenderingDisplayNode(volume)

    def refresh(self, caller=None, event=None):
        volume = self.workbench.activeVolume()
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
        self.maximizeButton.setIcon(viewIcon("layout4" if maximized else "layout1"))

    def setRenderingVisible(self, visible):
        displayNode = self.renderingDisplayNode()
        if displayNode is not None:
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
        layoutManager = slicer.app.layoutManager()
        arrangement = layoutManager.layoutLogic().GetLayoutNode().GetViewArrangement()
        oneUp = slicer.vtkMRMLLayoutNode.SlicerLayoutOneUp3DView
        layoutManager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView if arrangement == oneUp else oneUp)
        self.workbench.styleViews()


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
            controller = widget.sliceVerticalController()
            controller.setStyleSheet(f"background: {VIEW};")
            for slider in controller.findChildren("QSlider"):
                slider.setStyleSheet(SLIDER_STYLE)
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
        for bar in self.bars.values():
            bar.refresh()
        self.styleViewport()
        self.setActive(self.workbench.activeViewKey)

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
            else:
                widget = layoutManager.sliceWidget(key)
                if widget is not None:
                    widget.sliceController().visible = True
                    controller = widget.sliceVerticalController()
                    controller.setStyleSheet("")
                    for slider in controller.findChildren("QSlider"):
                        slider.setStyleSheet("")
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
        self.timer = qt.QTimer()
        self.timer.setSingleShot(True)
        self.timer.setInterval(50)
        self.timer.connect("timeout()", self.update)

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
        for displayNode in slicer.util.getNodesByClass("vtkMRMLVolumeRenderingDisplayNode"):
            volume = displayNode.GetDisplayableNode()
            if volume is None or not displayNode.GetVisibility():
                continue
            for viewNode in viewNodes:
                if displayNode.IsDisplayableInView(viewNode.GetID()):
                    showing.setdefault(volume.GetID(), set()).add(viewNode.GetID())
        return showing

    def allViews(self):
        return [node.GetID() for node in slicer.util.getNodesByClass("vtkMRMLSliceNode")] + \
               [node.GetID() for node in slicer.util.getNodesByClass("vtkMRMLViewNode")]

    def update(self):
        if not self.active:
            return
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

    def addToView(self, segmentation, viewNodeID):
        self.extra.setdefault(segmentation.GetID(), set()).add(viewNodeID)
        displayNode = segmentation.GetDisplayNode()
        if displayNode is not None:
            displayNode.SetVisibility(True)
        self.update()

    def renderInView(self, volume, viewNodeID):
        """Volume rendering of a cube in one 3D view (the other volumes are
        no more rendered there)."""
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
        self.update()

    def exit(self):
        self.active = False
        self.timer.stop()
        for node, tag in self.observers:
            node.RemoveObserver(tag)
        self.observers = []
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
