import qt

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)

SHORTCUT = "Ctrl+Shift+W"


def workbench():
    """The Workbench of the main window (one per application)."""
    from AstroWorkbenchLib.Workbench import Workbench
    if getattr(slicer.modules, "astroWorkbenchInstance", None) is None:
        slicer.modules.astroWorkbenchInstance = Workbench()
    return slicer.modules.astroWorkbenchInstance


#
# AstroWorkbench
#


class AstroWorkbench(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Workbench"
        self.parent.categories = ["Astronomy"]
        self.parent.dependencies = ["AstroVolume", "AstroMomentMaps"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = f"""
A workspace for spectral cubes around the views: the datasets with previews
computed from the data (moment-0 map and integrated spectrum) and their
products, a scrubber of the spectral axis over the integrated spectrum, an
inspector with the processing pipeline (smoothing, masks, moment maps,
profiles, PV diagrams), measurements on the sky and 3DBarolo, and the sky
coordinates under the cursor.<br>
Open it from this module or with {SHORTCUT}; the full Slicer button (or
{SHORTCUT}) goes back to the Slicer interface, the developer button shows the
Python console.
"""
        self.parent.acknowledgementText = """
This module was developed by Davide Punzo. <br>
This work was supported by ERC grant nr. 291531 and the Slicer Community.
"""
        slicer.app.connect("startupCompleted()", installShortcut)


def showAbout():
    from AstroWorkbenchLib import About
    About.showAbout()


def installShortcut():
    """Ctrl+Shift+W (Workbench) and Help > About SlicerAstro."""
    mainWindow = slicer.util.mainWindow()
    if mainWindow is None or mainWindow.findChild("QShortcut", "AstroWorkbenchShortcut") is not None:
        return
    helpMenu = mainWindow.findChild("QMenu", "HelpMenu")
    if helpMenu is not None:
        action = helpMenu.addAction("About SlicerAstro")
        action.setObjectName("AboutSlicerAstroAction")
        action.connect("triggered()", showAbout)
    shortcut = qt.QShortcut(qt.QKeySequence(SHORTCUT), mainWindow)
    shortcut.setObjectName("AstroWorkbenchShortcut")
    shortcut.setContext(qt.Qt.ApplicationShortcut)
    shortcut.connect("activated()", lambda: workbench().toggle())


#
# AstroWorkbenchWidget
#


class AstroWorkbenchWidget(ScriptedLoadableModuleWidget):
    def setup(self):
        ScriptedLoadableModuleWidget.setup(self)
        theme = slicer.app.property("SlicerAstroTheme")
        panel = qt.QWidget()
        layout = qt.QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        header = slicer.qSlicerAstroPanelHeader()
        header.title = "Workbench"
        header.iconName = "module-workbench"
        layout.addWidget(header)
        body = qt.QWidget()
        bodyLayout = qt.QVBoxLayout(body)
        bodyLayout.setContentsMargins(12, 12, 12, 14)
        bodyLayout.setSpacing(12)
        text = qt.QLabel(
            "A workspace for spectral cubes: your datasets with previews and their products, the views "
            "with a scrubber of the spectral axis, the processing pipeline and the sky coordinates under "
            "the cursor. Slicer's menus and panels are hidden while it is open.")
        text.wordWrap = True
        text.setProperty("astroRole", "label")
        bodyLayout.addWidget(text)
        self.openButton = slicer.qSlicerAstroRunButton()
        self.openButton.setObjectName("OpenWorkbenchButton")
        self.openButton.text = "Open the Workbench"
        self.openButton.connect("clicked()", lambda: workbench().enter())
        bodyLayout.addWidget(self.openButton)
        hint = qt.QLabel(f"{SHORTCUT} opens and closes it from anywhere.")
        hint.setProperty("astroRole", "note")
        bodyLayout.addWidget(hint)
        layout.addWidget(body)
        theme.applyTo(panel)
        self.layout.addWidget(panel)
        self.layout.addStretch(1)
        installShortcut()


#
# AstroWorkbenchLogic
#


class AstroWorkbenchLogic(ScriptedLoadableModuleLogic):
    pass


#
# AstroWorkbenchTest
#


class AstroWorkbenchTest(ScriptedLoadableModuleTest):
    def setUp(self):
        workbench().exit()
        slicer.mrmlScene.Clear(0)

    def tearDown(self):
        workbench().exit()

    def runTest(self):
        self.setUp()
        self.test_EnterExit()
        self.setUp()
        self.test_DatasetsAndScrubber()
        self.setUp()
        self.test_Theme()
        self.setUp()
        self.test_ViewsInteraction()
        self.setUp()
        self.test_About()
        self.tearDown()

    def cube(self):
        import SampleData
        return SampleData.downloadSample("WEIN069")

    def process(self, count=20):
        for _ in range(count):
            slicer.app.processEvents()

    def test_EnterExit(self):
        """The chrome of Slicer is hidden and restored."""
        mainWindow = slicer.util.mainWindow()
        panel = mainWindow.findChild("QDockWidget", "PanelDockWidget")
        menuVisible = mainWindow.menuBar().visible
        panelVisible = panel.visible
        toolbars = {bar.objectName: bar.visible for bar in mainWindow.findChildren("QToolBar")}
        layoutManager = slicer.app.layoutManager()
        redWidget = layoutManager.sliceWidget("Red")
        viewNode = layoutManager.threeDWidget(0).mrmlViewNode()
        boxVisible = viewNode.GetBoxVisible()
        sliderOrientation = redWidget.sliceOffsetSliderOrientation
        workbench().enter()
        self.process()
        self.assertTrue(workbench().active)
        # views: bars of the Workbench, vertical slice sliders, no 3D box
        bars = [bar for bar in mainWindow.findChildren("QFrame") if bar.objectName == "AstroViewBar"]
        self.assertGreaterEqual(len(bars), 2)
        self.assertEqual(redWidget.sliceOffsetSliderOrientation, qt.Qt.Vertical)
        self.assertIn("QMainWindow::separator", mainWindow.styleSheet)
        self.assertFalse(redWidget.sliceController().visible)
        self.assertFalse(viewNode.GetBoxVisible())
        self.assertFalse(mainWindow.menuBar().visible)
        self.assertFalse(panel.visible)
        self.assertIsNotNone(mainWindow.findChild("QDockWidget", "WorkbenchDatasets"))
        self.assertIsNotNone(mainWindow.findChild("QToolBar", "WorkbenchHeaderBar"))
        slicer.util.findChild(mainWindow, "FullSlicerButton").click()
        self.process()
        self.assertFalse(workbench().active)
        self.assertEqual(mainWindow.menuBar().visible, menuVisible)
        self.assertEqual(panel.visible, panelVisible)
        for bar in mainWindow.findChildren("QToolBar"):
            if bar.objectName in toolbars:
                self.assertEqual(bar.visible, toolbars[bar.objectName], bar.objectName)
        self.process()
        self.assertIsNone(mainWindow.findChild("QToolBar", "WorkbenchHeaderBar"))
        self.assertEqual(redWidget.sliceOffsetSliderOrientation, sliderOrientation)
        self.assertNotIn("QMainWindow::separator", mainWindow.styleSheet)
        self.assertTrue(redWidget.sliceController().visible)
        self.assertEqual(viewNode.GetBoxVisible(), boxVisible)
        self.assertEqual([bar for bar in mainWindow.findChildren("QFrame") if bar.objectName == "AstroViewBar"], [])

    def test_DatasetsAndScrubber(self):
        """A card per dataset (preview from the data, chips), the moment
        maps as its products, the scrubber moves the XY view."""
        import numpy as np
        volume = self.cube()
        selection = slicer.app.applicationLogic().GetSelectionNode()
        selection.SetActiveVolumeID(volume.GetID())
        slicer.app.applicationLogic().PropagateVolumeSelection(0)
        workbench().enter()
        self.process(30)
        mainWindow = slicer.util.mainWindow()
        card = slicer.util.findChild(mainWindow, f"DatasetCard_{volume.GetID()}")
        self.assertTrue(card.active)
        self.assertEqual(card.kind, "cube")
        self.assertTrue(any(chip.endswith("km/s") for chip in card.chips))
        thumbnail = slicer.qSlicerAstroThumbnails().image(volume, 76, 58)
        self.assertFalse(thumbnail.isNull())
        spectrum = slicer.qSlicerAstroThumbnails().integratedSpectrum(volume)
        self.assertEqual(len(spectrum), volume.GetImageData().GetDimensions()[2])

        # the integrated spectrum of the scrubber is the sum of each channel
        from vtk.util.numpy_support import vtk_to_numpy
        dims = volume.GetImageData().GetDimensions()
        cube = vtk_to_numpy(volume.GetImageData().GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])
        np.testing.assert_allclose(np.array(spectrum), np.nansum(cube, axis=(1, 2)), rtol=1e-5)

        scrubber = slicer.util.findChild(mainWindow, "AxisScrubber")
        self.assertEqual(scrubber.count(), dims[2])
        redNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeRed")
        from AstroWorkbenchLib import Astro
        for channel in (3, dims[2] // 2, dims[2] - 2):
            scrubber.setCurrentIndex(channel)
            self.process()
            self.assertEqual(Astro.channelOfSlice(volume, redNode), channel)
        # the scrubber follows the view
        Astro.setSliceToChannel(volume, redNode, 5)
        self.process()
        self.assertEqual(scrubber.currentIndex, 5)

        # Process: moment maps from the pipeline panel, listed as products
        inspector = slicer.util.findChild(mainWindow, "WorkbenchInspector")
        momentsPanel = slicer.util.findChild(inspector, "PipelinePanel_AstroMomentMaps")
        self.assertTrue(momentsPanel.visible)
        slicer.util.findChild(momentsPanel, "ApplyButton").click()
        self.process(30)
        workbench().refresh()
        self.process()
        card = slicer.util.findChild(mainWindow, f"DatasetCard_{volume.GetID()}")
        self.assertGreaterEqual(card.productCount(), 3)
        self.assertEqual(slicer.util.findChild(inspector, "StepState_Moments").text, "done")

    def test_Theme(self):
        """The theme switch restyles the Workbench."""
        theme = slicer.app.property("SlicerAstroTheme")
        mode = theme.mode
        workbench().enter()
        self.process()
        header = slicer.util.findChild(slicer.util.mainWindow(), "WorkbenchHeader")
        theme.setMode("dark")
        self.process()
        self.assertTrue(theme.dark)
        darkSheet = header.styleSheet
        theme.setMode("light")
        self.process()
        self.assertFalse(theme.dark)
        self.assertNotEqual(header.styleSheet, darkSheet)
        self.assertIn(theme.color("panel").name(), header.styleSheet.lower())
        theme.setMode(mode)

    def test_ViewsInteraction(self):
        """Active view (frame), data dropped on views, masks shown only in
        the views showing their cube, the Python console of the developer
        button."""
        volume = self.cube()
        mask = slicer.modules.astrovolume.logic().GetMaskSegmentationForVolume(volume, True)
        selection = slicer.app.applicationLogic().GetSelectionNode()
        selection.SetActiveVolumeID(volume.GetID())
        workbench().enter()
        workbench().header.layoutButton.setCurrentIndex(2)
        self.process(30)
        mainWindow = slicer.util.mainWindow()
        layoutManager = slicer.app.layoutManager()
        frames = {name: slicer.util.findChild(mainWindow, f"AstroViewFrame_{name}") for name in ("Red", "Yellow", "Green")}
        self.assertTrue(frames["Red"].active)

        # a click in a view makes it the active view
        view = layoutManager.sliceWidget("Green").sliceView()
        center = qt.QPointF(view.width / 2, view.height / 2)
        for eventType in (qt.QEvent.MouseButtonPress, qt.QEvent.MouseButtonRelease):
            event = qt.QMouseEvent(eventType, center, view.mapToGlobal(center), qt.Qt.LeftButton,
                                   qt.Qt.LeftButton if eventType == qt.QEvent.MouseButtonPress else qt.Qt.NoButton,
                                   qt.Qt.NoModifier)
            slicer.app.sendEvent(view, event)
        self.process()
        self.assertTrue(frames["Green"].active)
        self.assertFalse(frames["Red"].active)

        # a dataset dropped on a view: shown there, the view becomes active
        target = layoutManager.sliceWidget("Yellow").sliceView()
        mime = qt.QMimeData()
        mime.setData("application/x-slicerastro-node-id", qt.QByteArray(volume.GetID().encode()))
        point = qt.QPointF(target.width / 2, target.height / 2)
        slicer.app.sendEvent(target, qt.QDragEnterEvent(point.toPoint(), qt.Qt.CopyAction, mime,
                                                          qt.Qt.LeftButton, qt.Qt.NoModifier))
        slicer.app.sendEvent(target, qt.QDropEvent(point, qt.Qt.CopyAction, mime, qt.Qt.LeftButton, qt.Qt.NoModifier))
        self.process(20)
        yellowComposite = layoutManager.sliceWidget("Yellow").mrmlSliceCompositeNode()
        self.assertEqual(yellowComposite.GetBackgroundVolumeID(), volume.GetID())
        self.assertTrue(frames["Yellow"].active)

        # the mask of the cube: in the views showing the cube only
        workbench().visibility.update()
        displayNode = mask.GetDisplayNode()
        self.assertFalse(displayNode.GetShowInAllViewsByDefault())
        greenComposite = layoutManager.sliceWidget("Green").mrmlSliceCompositeNode()
        greenComposite.SetBackgroundVolumeID(None)
        self.process(20)
        workbench().visibility.update()
        self.assertTrue(displayNode.IsDisplayableInView("vtkMRMLSliceNodeYellow"))
        self.assertFalse(displayNode.IsDisplayableInView("vtkMRMLSliceNodeGreen"))

        # the cube dropped on the 3D view: rendered there
        threeD = layoutManager.threeDWidget(0)
        workbench().dropNode("3D0", volume.GetID())
        rendering = slicer.modules.volumerendering.logic().GetFirstVolumeRenderingDisplayNode(volume)
        self.assertTrue(rendering.GetVisibility())
        self.assertTrue(rendering.IsDisplayableInView(threeD.mrmlViewNode().GetID()))

        # developer button: the Python console
        developer = slicer.util.findChild(mainWindow, "DeveloperButton")
        console = mainWindow.findChild("QDockWidget", "PythonConsoleDockWidget")
        if console is not None:
            developer.click()
            self.process()
            self.assertTrue(console.visible)
            developer.click()
            self.process()
            self.assertFalse(console.visible)

        # leaving: the masks are shown in all the views again
        workbench().exit()
        self.process()
        self.assertTrue(displayNode.GetShowInAllViewsByDefault())
        self.assertTrue(displayNode.IsDisplayableInView("vtkMRMLSliceNodeGreen"))

    def test_About(self):
        """The About window: versions, shortcuts, credits; the shortcuts of
        the Workbench."""
        from AstroWorkbenchLib import About
        dialog = About.AboutDialog()
        dialog.show()
        self.process()
        version = slicer.util.findChild(dialog, "AboutVersion_SlicerAstro")
        self.assertEqual(version.text, slicer.vtkSlicerAstroVolumeLogic.GetSlicerAstroVersion())
        self.assertTrue(slicer.util.findChild(dialog, "AboutVersion_CFITSIO").text)
        self.assertTrue(slicer.util.findChild(dialog, "AboutVersion_WCSLIB").text.startswith("8."))
        self.assertFalse(slicer.util.findChild(dialog, "AboutLogo").pixmap.isNull())
        for index in range(3):
            dialog.tabs.setCurrentIndex(index)
            self.process()
            self.assertEqual(dialog.stack.currentIndex, index)
        slicer.util.findChild(dialog, "AboutClose").click()
        self.process()
        self.assertFalse(dialog.visible)
        # Workbench shortcuts exist while it is open
        workbench().enter()
        self.process()
        mainWindow = slicer.util.mainWindow()
        for keys in ("F1", "Ctrl+Shift+T", "Space"):
            self.assertIsNotNone(mainWindow.findChild("QShortcut", f"WorkbenchShortcut_{keys}"))
        workbench().exit()
        self.process()
        self.assertIsNone(mainWindow.findChild("QShortcut", "WorkbenchShortcut_F1"))
