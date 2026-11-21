import os

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
profiles, PV diagrams) and 3DBarolo, and the sky
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
    # the application quits with the Workbench open: it is left before the
    # modules are unloaded (the module panels it entered are exited and its
    # widgets removed while the modules still exist)
    factoryManager = slicer.app.moduleManager().factoryManager()
    factoryManager.connect("modulesAboutToBeUnloaded(QStringList)", lambda names: workbench().exit())


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
        self.test_ViewBarsAndWorkspaces()
        self.setUp()
        self.test_ScrollAreasFit()
        self.setUp()
        self.test_HeaderTools()
        self.setUp()
        self.test_LayoutsAndPlot()
        self.setUp()
        self.test_InspectorWidth()
        self.setUp()
        self.test_InspectSectionsOneOpen()
        self.setUp()
        self.test_MaskEditing()
        self.setUp()
        self.test_DeleteDatasetWithEverything()
        self.setUp()
        self.test_Theme()
        self.setUp()
        self.test_ViewsInteraction()
        self.setUp()
        self.test_About()
        self.setUp()
        self.test_HIContours()
        self.setUp()
        self.test_Sources()
        self.setUp()
        self.test_ProductsKeepViews()
        self.tearDown()

    def cube(self):
        import SampleData
        return SampleData.downloadSample("WEIN069")

    def process(self, count=20):
        for _ in range(count):
            slicer.app.processEvents()

    def wait(self, seconds=0.5):
        """Events processed for a while (the refresh of the Workbench is
        delayed by its timer)."""
        import time
        end = time.time() + seconds
        while time.time() < end:
            slicer.app.processEvents()
            time.sleep(0.01)

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

        # Inspect > Display: histogram of the data (counts per bin), window of
        # the 2D colours (fields, percentages), noise σ of the 3D colours,
        # colour table options
        inspector = workbench().inspector
        display = inspector.displayWidget
        histogram = slicer.util.findChild(inspector, "DisplayHistogram")
        counts = np.array(histogram.binCounts())
        self.assertEqual(len(counts), 180)
        values = cube[np.isfinite(cube)]
        self.assertEqual(int(counts.sum()), values.size)
        noise = volume.GetDisplayThreshold()
        display.setWindow(-1.0 * noise, 12.0 * noise)
        self.process()
        displayNode = volume.GetDisplayNode()
        self.assertAlmostEqual(displayNode.GetWindow() / noise, 13.0, places=3)
        self.assertAlmostEqual(displayNode.GetLevel() / noise, 5.5, places=3)
        self.assertIn("12.0σ", slicer.util.findChild(inspector, "DisplayWindowLabel").text)
        self.assertAlmostEqual(histogram.windowMaximum / noise, 12.0, places=3)
        # 99%: 99% of the negative and of the positive values in the window
        slicer.util.findChild(inspector, "DisplayPercentages").setCurrentIndex(2)
        self.process()
        low, high = displayNode.GetLevel() - displayNode.GetWindow() / 2, displayNode.GetLevel() + displayNode.GetWindow() / 2
        positive, negative = values[values > 0], values[values < 0]
        self.assertAlmostEqual(np.mean(positive <= high), 0.99, delta=0.002)
        self.assertAlmostEqual(np.mean(negative >= low), 0.99, delta=0.002)
        # noise σ of the 3D colour functions
        display.setNoise(1.5 * noise)
        self.process()
        self.assertAlmostEqual(volume.GetDisplayThreshold(), 1.5 * noise, places=9)
        self.assertAlmostEqual(histogram.noise, 1.5 * noise, places=9)
        display.setNoise(noise)
        # colour table options
        slicer.util.findChild(inspector, "DisplayReverse").click()
        self.process()
        self.assertTrue(slicer.vtkSlicerAstroVolumeLogic.GetColorFunctionOption(displayNode.GetColorNode(), "Reverse"))
        slicer.util.findChild(inspector, "DisplayReverse").click()
        self.assertFalse(slicer.vtkSlicerAstroVolumeLogic.GetColorFunctionOption(displayNode.GetColorNode(), "Reverse"))
        self.assertGreater(slicer.util.findChild(inspector, "DisplayColourTable").count, 1)

        # Process: moment maps from the pipeline panel, listed as products
        inspector.tabs["Process"].click()
        inspector.stepGroup.button(2).click()
        self.process(10)
        momentsPanel = slicer.util.findChild(inspector, "PipelinePanel_AstroMomentMaps")
        self.assertTrue(momentsPanel.visible)
        slicer.util.findChild(momentsPanel, "ApplyButton").click()
        slicer.qSlicerAstroBackgroundRunner().waitForJobs()
        self.process(30)
        workbench().refresh()
        self.process()
        card = slicer.util.findChild(mainWindow, f"DatasetCard_{volume.GetID()}")
        self.assertGreaterEqual(card.productCount(), 3)
        self.assertEqual(slicer.util.findChild(inspector, "StepState_Moments").text, "done")
        # one moment map per order and per cube, shared by the modules: a
        # second run replaces them
        logic = slicer.vtkSlicerAstroVolumeLogic
        mom0 = logic.GetMomentMap(volume, 0)
        self.assertIsNotNone(mom0)
        self.assertEqual(mom0.GetName(), volume.GetName() + "_mom0")
        products = card.productCount()
        slicer.util.findChild(momentsPanel, "ApplyButton").click()
        slicer.qSlicerAstroBackgroundRunner().waitForJobs()
        self.process(30)
        workbench().refresh()
        self.process()
        card = slicer.util.findChild(mainWindow, f"DatasetCard_{volume.GetID()}")
        self.assertEqual(card.productCount(), products)
        self.assertEqual(len([node for node in slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode")
                              if node.GetName() == volume.GetName() + "_mom0"]), 1)

        # a product click shows a map in the sky plane; a second click hides it
        mom0 = logic.GetMomentMap(volume, 0)
        for name in ("Red", "Yellow", "Green"):
            composite = slicer.mrmlScene.GetNodeByID(f"vtkMRMLSliceCompositeNode{name}")
            if composite.GetBackgroundVolumeID() == mom0.GetID():
                composite.SetBackgroundVolumeID(volume.GetID())
        workbench().showProduct(mom0.GetID())
        self.process()
        composites = [slicer.mrmlScene.GetNodeByID(f"vtkMRMLSliceCompositeNode{name}") for name in ("Red", "Yellow")]
        shown = [composite for composite in composites if composite.GetBackgroundVolumeID() == mom0.GetID()]
        self.assertEqual(len(shown), 1)
        sliceNode = slicer.mrmlScene.GetNodeByID(shown[0].GetID().replace("CompositeNode", "Node"))
        self.assertEqual(sliceNode.GetOrientation(), "XY",
                         f"{shown[0].GetID()} layout {workbench().layoutIndex()}")
        workbench().showProduct(mom0.GetID())
        self.process()
        self.assertNotEqual(shown[0].GetBackgroundVolumeID(), mom0.GetID())

        # product names: elided to the card, the full name in the tooltip
        button = slicer.util.findChild(card, f"Product_{mom0.GetID()}")
        self.assertIn(mom0.GetName(), button.toolTip)

        # dataset deleted with its products
        mapIDs = [logic.GetMomentMap(volume, order).GetID() for order in (0, 1, 2) if logic.GetMomentMap(volume, order)]
        self.assertTrue(workbench().drawer.deleteDataset(volume, ask=False, withProducts=True))
        self.process()
        self.assertIsNone(volume.GetScene())
        for mapID in mapIDs:
            self.assertIsNone(slicer.mrmlScene.GetNodeByID(mapID))

    def test_ViewBarsAndWorkspaces(self):
        """View bars: WCS axes and slice in 3D. Workspaces: saved with a
        preview, listed, renamed, opened and deleted."""
        import tempfile
        from AstroWorkbenchLib import Workspaces
        volume = self.cube()
        workbench().enter()
        self.process(30)
        mainWindow = slicer.util.mainWindow()
        redNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeRed")
        axes = slicer.util.findChild(mainWindow, "ViewAxes_Red")
        axes.checked = False
        self.assertEqual(redNode.GetRulerType(), slicer.vtkMRMLAbstractViewNode.RulerTypeNone)
        axes.checked = True
        self.assertNotEqual(redNode.GetRulerType(), slicer.vtkMRMLAbstractViewNode.RulerTypeNone)
        slice3D = slicer.util.findChild(mainWindow, "ViewSliceIn3D_Red")
        slice3D.checked = True
        self.assertTrue(redNode.GetSliceVisible())
        slice3D.checked = False
        self.assertFalse(redNode.GetSliceVisible())
        self.assertEqual(len(slicer.util.findChildren(mainWindow, "ViewLink_Red")), 0)

        directory = tempfile.mkdtemp(prefix="AstroWorkspaces")
        folder = Workspaces.saveWorkspace("Test WEIN069", "a note", directory)
        self.assertTrue(os.path.isfile(os.path.join(folder, Workspaces.SCENE_FILE)))
        self.assertTrue(os.path.isfile(os.path.join(folder, Workspaces.PREVIEW_FILE)))
        listed = Workspaces.listWorkspaces(directory)
        self.assertEqual([entry["name"] for entry in listed], ["Test WEIN069"])
        # datasets by name; their objects (FITS OBJECT, e.g. PP08AB for
        # WEIN069) only for the filter
        self.assertEqual(listed[0]["datasets"], ["WEIN069"])
        self.assertEqual(listed[0]["objects"], [volume.GetAttribute("SlicerAstro.OBJECT").strip()])
        self.assertEqual(Workspaces.dateGroup(listed[0]["modified"]), "Today")
        with self.assertRaises(FileExistsError):
            Workspaces.saveWorkspace("Test WEIN069", "", directory)
        folder = Workspaces.renameWorkspace(folder, "Renamed")
        self.assertEqual(Workspaces.listWorkspaces(directory)[0]["name"], "Renamed")
        self.assertTrue(Workspaces.setWorkspaceNotes(folder, "other notes"))
        self.assertEqual(Workspaces.listWorkspaces(directory)[0]["notes"], "other notes")
        # exported as a zip: the folder with the scene, preview and description
        import zipfile
        archivePath = os.path.join(tempfile.mkdtemp(prefix="AstroWorkspaceZip"), "renamed.zip")
        self.assertEqual(Workspaces.exportWorkspace(folder, archivePath), archivePath)
        with zipfile.ZipFile(archivePath) as archive:
            self.assertEqual(sorted(archive.namelist()),
                             sorted(f"Renamed/{name}" for name in (Workspaces.SCENE_FILE, Workspaces.PREVIEW_FILE,
                                                                   Workspaces.DESCRIPTION_FILE)))
        os.remove(archivePath)
        os.rmdir(os.path.dirname(archivePath))
        # the card of the page: name and notes edited in place
        page = Workspaces.WorkspacesPage(workbench())
        entry = Workspaces.WorkspaceEntry(page, Workspaces.readDescription(folder))
        self.assertEqual(len(slicer.util.findChildren(entry, "WorkspaceRename")), 0)
        self.assertEqual(len(slicer.util.findChildren(entry, "WorkspaceExport")), 1)
        # notes: a fixed height (the card does not move while typing)
        height = entry.notesEdit.maximumHeight
        entry.notesEdit.setPlainText("edited in the card\nsecond line")
        self.process()
        self.assertEqual(entry.notesEdit.maximumHeight, height)
        entry.onNotesEdited()
        self.assertEqual(Workspaces.readDescription(folder)["notes"], "edited in the card\nsecond line")
        # a long name: elided in the card, the full name in the tooltip
        entry.titleEdit.resize(120, 24)
        entry.titleEdit.setFullText("A very long workspace name that does not fit in the card")
        self.assertTrue(entry.titleEdit.text.endswith("…"))
        entry.onTitleEdited("Renamed in the card")
        folder = entry.description["folder"]
        self.assertEqual(Workspaces.readDescription(folder)["name"], "Renamed in the card")
        # the logbook refresh scheduled by the rename runs; the widgets are
        # released by Python (a deleteLater would run only at the exit of
        # the application, after Python)
        self.process()
        entry = None
        page = None
        # opening a workspace replaces the scene: other data loaded meanwhile
        # are removed, the data of the workspace are there once
        import SampleData
        other = SampleData.downloadSample("NGC2403")
        self.assertIsNotNone(other)
        self.assertTrue(Workspaces.loadWorkspace(folder))
        self.process()
        names = [node.GetName() for node in slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode")]
        self.assertEqual(names.count("WEIN069"), 1, names)
        self.assertNotIn("NGC2403", names)
        self.assertTrue(Workspaces.deleteWorkspace(folder))
        self.assertEqual(Workspaces.listWorkspaces(directory), [])
        os.rmdir(directory)

    def assertScrollAreasFit(self, root, where):
        """No visible scroll area clips its contents (the vertical scroll bar
        never covers them)."""
        for scrollArea in root.findChildren("QScrollArea"):
            contents = scrollArea.widget()
            if contents is None or not scrollArea.isVisible() or not contents.isVisible():
                continue
            viewport = scrollArea.viewport()
            self.assertLessEqual(contents.width, viewport.width,
                                 f"{where}: {scrollArea.objectName or scrollArea.className()} clips its contents "
                                 f"({contents.width} > {viewport.width} px)")

    def test_ScrollAreasFit(self):
        """On a short window (vertical scroll bars shown), the scroll areas of
        the Workbench and of the SlicerAstro module panels are wide enough for
        their contents and the scroll bar."""
        volume = self.cube()
        mainWindow = slicer.util.mainWindow()
        size = mainWindow.size
        mainWindow.resize(1400, 600)
        try:
            # classic Slicer: the module panels
            for module in ("AstroVolume", "AstroSmoothing", "AstroMasking", "AstroMomentMaps", "AstroProfiles",
                           "AstroStatistics", "AstroPVSlice", "AstroPVDiagram", "AstroReproject"):
                slicer.util.selectModule(module)
                self.process(10)
                panel = mainWindow.findChild("QDockWidget", "PanelDockWidget")
                self.assertScrollAreasFit(panel, module)
            workbench().enter()
            self.process(30)
            for page in ("data", "samples", "workspaces"):
                workbench().rail.buttons[page].click()
                self.process(10)
                self.assertScrollAreasFit(mainWindow, f"drawer {page}")
            workbench().rail.buttons["data"].click()
            inspector = workbench().inspector
            for index, name in enumerate(("Inspect", "Mask", "Sources", "Process", "Model")):
                inspector.tabs[name].click()
                self.process(10)
                self.assertScrollAreasFit(inspector, f"inspector {name}")
            inspector.tabs["Process"].click()
            for index, button in enumerate(inspector.stepButtons):
                button.click()
                self.process(10)
                self.assertScrollAreasFit(inspector, f"process step {index}")
        finally:
            mainWindow.resize(size)

    def test_LayoutsAndPlot(self):
        """The layout buttons of the header (one checked, following the
        layout); the layout with a plot shows the profiles of the cube (none
        before one is computed) in a plot view with a view bar and the dark
        view colours, restored when leaving the Workbench."""
        volume = self.cube()
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volume.GetID())
        workbench().enter()
        self.process(30)
        switch = workbench().header.layoutButton
        layoutManager = slicer.app.layoutManager()
        layouts = workbench().layouts()
        self.assertEqual(switch.count(), 4)
        for index in range(switch.count()):
            switch.button(index).click()
            self.process(20)
            self.assertEqual(layoutManager.layoutLogic().GetLayoutNode().GetViewArrangement(), layouts[index])
            self.assertEqual(switch.currentIndex, index)
            self.assertEqual([button.checked for button in switch.buttons],
                             [i == index for i in range(switch.count())])
        # the plot: empty (the view colour of the theme) until a profile of
        # the cube is computed
        plotWidget = layoutManager.plotWidget(0)
        self.assertIsNotNone(plotWidget)
        self.assertFalse(plotWidget.plotController().visible)
        self.assertIn("Plot0", workbench().views.bars)
        self.assertFalse(plotWidget.mrmlPlotViewNode().GetPlotChartNodeID())
        renderer = plotWidget.plotView().chart().GetScene().GetRenderer()
        from AstroWorkbenchLib.Views import viewColor
        viewColour = qt.QColor(viewColor("view"))
        self.assertEqual(tuple(round(value * 255) for value in renderer.GetBackground()),
                         (viewColour.red(), viewColour.green(), viewColour.blue()))
        # a profile computed in Profiles: in the plot view
        slicer.util.selectModule("AstroProfiles")
        profiles = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroProfilesParametersNode")
        profiles.SetInputVolumeNodeID(volume.GetID())
        panel = slicer.modules.astroprofiles.widgetRepresentation()
        slicer.util.findChild(panel, "ApplyButton").click()
        slicer.qSlicerAstroBackgroundRunner().waitForJobs()
        self.wait()
        chart = slicer.mrmlScene.GetNodeByID(plotWidget.mrmlPlotViewNode().GetPlotChartNodeID() or "")
        self.assertIsNotNone(chart)
        self.assertEqual(chart.GetName(), "ProfileChart")
        background = plotWidget.plotView().chart().GetBackgroundBrush().GetColorObject()
        self.assertEqual((background.GetRed(), background.GetGreen(), background.GetBlue()),
                         (viewColour.red(), viewColour.green(), viewColour.blue()))
        # "Open in plot view" of Profiles: the layout with a plot (3 views)
        layoutManager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)
        self.process(20)
        slicer.util.findChild(panel, "PlotViewButton").click()
        self.process(20)
        self.assertEqual(layoutManager.layoutLogic().GetLayoutNode().GetViewArrangement(), workbench().PLOT_VIEWS)
        self.assertEqual(switch.currentIndex, 3)
        # a layout set elsewhere (maximized plot): no button checked; back
        layoutManager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpPlotView)
        self.process(20)
        self.assertEqual(switch.currentIndex, -1)
        self.assertFalse(any(button.checked for button in switch.buttons))
        layoutManager.setLayout(workbench().PLOT_VIEWS)
        self.process(20)
        self.assertEqual(switch.currentIndex, 3)
        # leaving the Workbench: Slicer's plot colours and controller
        workbench().exit()
        self.process(20)
        background = plotWidget.plotView().chart().GetBackgroundBrush().GetColorObject()
        self.assertNotEqual((background.GetRed(), background.GetGreen(), background.GetBlue()), (7, 10, 18))
        self.assertTrue(plotWidget.plotController().visible)

    def test_InspectorWidth(self):
        """Every tab of the inspector fits its minimum width (nothing clipped
        on the right) and the dataset card fits the drawer."""
        self.cube()
        workbench().enter()
        self.process(30)
        mainWindow = slicer.util.mainWindow()
        inspector = workbench().inspector
        right = mainWindow.findChild("QDockWidget", "WorkbenchInspector")
        mainWindow.resizeDocks([right], [inspector.minimumWidth], qt.Qt.Horizontal)
        self.process(20)
        for name in ("Inspect", "Mask", "Sources", "Process", "Model"):
            inspector.tabs[name].click()
            self.process(30)
            self.assertLessEqual(inspector.minimumSizeHint.width(), inspector.minimumWidth + 1, name)
            for scroll in inspector.findChildren("QScrollArea"):
                if scroll.isVisibleTo(inspector) and scroll.widget() is not None:
                    self.assertLessEqual(scroll.widget().width, scroll.viewport().width + 1, name)

    def test_InspectSectionsOneOpen(self):
        """Inspect shows one section at a time: opening Info, Display or
        Contours collapses the other two (Display open at the start)."""
        self.cube()
        workbench().enter()
        self.process(30)
        inspector = workbench().inspector
        folds = {"Info": inspector.infoFold, "Display": inspector.displayFold, "Contours": inspector.contoursFold}

        def opened():
            return [name for name, fold in folds.items() if not fold.collapsed]

        self.assertLessEqual(len(opened()), 1, opened())
        for name in ("Info", "Contours", "Display", "Info"):
            folds[name].setCollapsed(False)
            self.process(2)
            self.assertEqual(opened(), [name])
        # collapsing the open one leaves none open
        folds["Info"].setCollapsed(True)
        self.assertEqual(opened(), [])
        # the 3D rendering inside Display is not one of them
        folds["Display"].setCollapsed(False)
        inspector.renderingFold.setCollapsed(False)
        self.process(2)
        self.assertEqual(opened(), ["Display"])
        workbench().exit()

    def test_MaskEditing(self):
        """The Mask tab shows the Masks section (always there); Edit shows the
        Segment Editor with the effects in a bar of the section and the
        segments of the section only; an effect widens the panel for its
        options and the width comes back without effect; leaving the tab
        ends the editing."""
        volume = self.cube()
        workbench().enter()
        self.process(30)
        mainWindow = slicer.util.mainWindow()
        inspector = workbench().inspector
        dock = mainWindow.findChild("QDockWidget", "WorkbenchInspector")
        mainWindow.resizeDocks([dock], [inspector.minimumWidth], qt.Qt.Horizontal)
        self.process(20)
        width = dock.width
        masks = inspector.masksWidget
        self.assertFalse(inspector.tabs["Mask"].isHidden())
        inspector.tabs["Mask"].click()
        self.process(10)
        self.assertEqual(inspector.stack.currentIndex, 3)
        self.assertTrue(inspector.stack.currentWidget().isAncestorOf(masks))
        # the copy to other data is hidden for now
        self.assertTrue(slicer.util.findChild(masks, "MasksCopyRow").isHidden())
        slicer.util.findChild(masks, "MasksEdit").click()
        self.process(20)
        self.assertTrue(masks.isEditing())
        editor = masks.segmentEditor()
        effects = slicer.util.findChild(masks, "MasksEffects")
        self.assertFalse(effects.isHidden())
        # (the effect buttons are moved from the editor to the bar of the section)
        self.assertTrue(effects.isAncestorOf(slicer.util.findChild(masks, "EffectsGroupBox")))
        self.assertTrue(slicer.util.findChild(editor, "SegmentsTableResizableFrame").isHidden())
        # effects of no use on masks are not offered; Threshold without the
        # local histogram; Margin and Hollow in pixels
        order = list(editor.effectNameOrder())
        for name in ("Scissors", "Mask volume", "Level tracing"):
            self.assertNotIn(name, order)
        self.assertIn("Paint", order)
        thresholdOptions = editor.effectByName("Threshold").optionsFrame()
        histogram = thresholdOptions.findChildren("ctkTransferFunctionView")[0]
        self.assertFalse(histogram.isVisibleTo(thresholdOptions))
        self.assertGreaterEqual(thresholdOptions.findChildren("ctkRangeWidget")[0].decimals, 2)
        for name, parameter in (("Margin", "MarginSizeMm"), ("Hollow", "ShellThicknessMm"),
                                ("Smoothing", "KernelSizeMm"), ("Smoothing", "GaussianStandardDeviationMm")):
            spinBox = slicer.util.findChild(editor.effectByName(name).optionsFrame(), f"AstroPixels_{parameter}")
            self.assertEqual(spinBox.suffix, " px", name)
            spinBox.value = 4
            self.assertAlmostEqual(editor.effectByName(name).doubleParameter(parameter), 4.0)
        for name in ("Draw",):
            self.assertNotIn(name, order)
        # a segment added by the section is the one edited
        segmentID = masks.addSegment()
        self.process(10)
        self.assertEqual(editor.currentSegmentID(), segmentID)
        # an effect with options wider than the panel widens it (Paint fits
        # the minimum width of the panel: at least not narrower)
        editor.setActiveEffectByName("Paint")
        self.process(30)
        self.assertGreaterEqual(dock.width, width)
        editor.setActiveEffectByName("")
        self.process(30)
        self.assertLessEqual(abs(dock.width - width), 4)
        # leaving the tab ends the editing
        inspector.tabs["Inspect"].click()
        self.process(10)
        self.assertFalse(masks.isEditing())
        self.assertFalse(inspector.tabs["Mask"].isHidden())

        # the eye of a 3D view bar hides the volume rendering, not the mask
        bar = workbench().views.bars.get("3D0")
        rendering = bar.renderingDisplayNode() if bar is not None else None
        self.assertIsNotNone(rendering)
        rendering.SetVisibility(True)
        self.process(20)
        maskDisplay = slicer.modules.astrovolume.logic().GetMaskSegmentationForVolume(volume, False).GetDisplayNode()
        viewID = bar.viewNode.GetID()
        self.assertIn(viewID, list(maskDisplay.GetViewNodeIDs()))
        bar.visibilityButton.click()
        self.process(20)
        self.assertFalse(rendering.GetVisibility())
        self.assertIn(viewID, list(maskDisplay.GetViewNodeIDs()))
        bar.visibilityButton.click()
        self.process(20)
        self.assertTrue(rendering.GetVisibility())

    def test_HeaderTools(self):
        """Placement tools: clicked again, placement ends; placement ended in
        the views (right click), navigation is checked again. The developer
        button follows the Python console. No toolbar menu on a right click."""
        self.cube()
        workbench().enter()
        self.process()
        header = workbench().header
        interaction = slicer.app.applicationLogic().GetInteractionNode()
        for button in (header.pointsButton, header.lineButton, header.closedCurveButton):
            button.click()
            self.process()
            self.assertEqual(interaction.GetCurrentInteractionMode(), interaction.Place)
            self.assertTrue(button.checked)
            button.click()
            self.process()
            self.assertEqual(interaction.GetCurrentInteractionMode(), interaction.ViewTransform)
            self.assertTrue(header.navigateButton.checked)
        header.curveButton.click()
        self.process()
        # placement ended in a view (right click switches to navigation)
        interaction.SetCurrentInteractionMode(interaction.ViewTransform)
        self.process()
        self.assertTrue(header.navigateButton.checked)
        self.assertFalse(header.curveButton.checked)
        self.assertEqual(slicer.app.applicationLogic().GetSelectionNode().GetActivePlaceNodeClassName(),
                         "vtkMRMLMarkupsCurveNode")
        mainWindow = slicer.util.mainWindow()
        self.assertEqual(mainWindow.contextMenuPolicy, qt.Qt.PreventContextMenu)
        self.checkColorFunctionTool(header, interaction)
        console = mainWindow.findChild("QDockWidget", "PythonConsoleDockWidget")
        if console is not None:
            header.developerButton.click()
            self.process()
            self.assertTrue(console.visible)
            console.close()
            self.process()
            self.assertFalse(header.developerButton.checked)
        workbench().exit()
        self.process()
        self.assertNotEqual(mainWindow.contextMenuPolicy, qt.Qt.PreventContextMenu)

    def checkColorFunctionTool(self, header, interaction):
        """Colour-function tool: window/level of Slicer in the 2D views; a
        drag in the 3D view moves the 3D colour function (shift left-right,
        opacity up-down) without moving the camera, and Inspect follows."""
        volume = workbench().activeVolume()
        self.assertIsNotNone(volume)
        logic = slicer.modules.astrovolume.logic()
        logic.PrepareVolumeRendering(volume)
        logic.SetVolumeRenderingVisibility(volume, True, True)
        self.process()
        header.colorFunctionButton.click()
        self.process()
        self.assertTrue(header.colorFunctionButton.checked)
        self.assertEqual(interaction.GetCurrentInteractionMode(), interaction.AdjustWindowLevel)
        layoutManager = slicer.app.layoutManager()
        view = layoutManager.threeDWidget(0).threeDView()
        camera = view.cameraNode()
        position = list(camera.GetPosition())
        displayNode = volume.GetAstroVolumeDisplayNode()
        shift, opacity = displayNode.GetPresetOffset(), displayNode.GetOpacityShift()
        width, height = view.renderWindow().GetSize()
        interactor = view.interactor()
        x, y = width // 2, height // 2
        interactor.SetEventInformation(x, y)
        interactor.InvokeEvent("LeftButtonPressEvent")
        interactor.SetEventInformation(x + width // 4, y + height // 10)
        interactor.InvokeEvent("MouseMoveEvent")
        interactor.InvokeEvent("LeftButtonReleaseEvent")
        self.process()
        dataRange = (float(volume.GetAttribute("SlicerAstro.DATAMAX")) -
                     float(volume.GetAttribute("SlicerAstro.DATAMIN")))
        self.assertAlmostEqual(displayNode.GetPresetOffset() - shift, 0.25 * dataRange, delta=0.02 * dataRange)
        self.assertAlmostEqual(displayNode.GetOpacityShift() - opacity, 0.1, delta=0.02)
        for a, b in zip(camera.GetPosition(), position):
            self.assertAlmostEqual(a, b, places=3)
        # Inspect follows (3D section of the dataset)
        shiftSlider = slicer.util.findChild(workbench().inspector, "RenderingShift")
        self.assertAlmostEqual(shiftSlider.value, displayNode.GetPresetOffset(), delta=0.01 * dataRange)
        # back to navigation: Slicer's window/level mode is off
        header.navigateButton.click()
        self.process()
        self.assertEqual(interaction.GetCurrentInteractionMode(), interaction.ViewTransform)
        self.assertEqual(workbench().colorFunctionDrag.observers, [])

    def test_DeleteDatasetWithEverything(self):
        """A dataset with a SoFiA catalogue (points), a profile (table, plot
        series), a PV diagram and its base mask (listed once it has a
        segment), deleted with its products: nothing made from it is left
        and the application keeps working."""
        import AstroSampleData
        sampleData = AstroSampleData.AstroSampleDataLogic()
        volume = sampleData.downloadSample("WEIN069")
        sampleData.downloadSample("WEIN069_MASK")
        selection = slicer.app.applicationLogic().GetSelectionNode()
        selection.SetActiveVolumeID(volume.GetID())
        workbench().enter()
        self.process(30)
        from AstroWorkbenchLib import Astro
        # the base mask of the cube: a product once it has a segment
        baseMask = slicer.modules.astrovolume.logic().GetMaskSegmentationForVolume(volume, True)
        self.assertIsNotNone(baseMask)
        baseMask.GetSegmentation().AddEmptySegment("tail")
        self.process(30)
        workbench().refresh()
        card = slicer.util.findChild(slicer.util.mainWindow(), f"DatasetCard_{volume.GetID()}")
        self.assertIsNotNone(slicer.util.findChild(card, f"Product_{baseMask.GetID()}"))
        # profile and PV diagram
        slicer.util.selectModule("AstroProfiles")
        profiles = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroProfilesParametersNode")
        profiles.SetInputVolumeNodeID(volume.GetID())
        slicer.util.findChild(slicer.modules.astroprofiles.widgetRepresentation(), "ApplyButton").click()
        slicer.qSlicerAstroBackgroundRunner().waitForJobs()
        self.process(20)
        slicer.util.selectModule("AstroPVDiagram")
        line = slicer.util.getNode("PVDiagramLine")
        line.AddControlPoint(0, 0, 0)
        line.AddControlPoint(30, 0, 0)
        line.AddControlPoint(30, 0, 40)
        self.process(20)
        # results of Modeling made from the cube (rings table, points of the
        # rotation curves, data and model contours, model cube)
        slicer.util.selectModule("AstroModeling")
        self.process(20)
        modeling = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroModelingParametersNode")
        modeling.SetInputVolumeNodeID(volume.GetID())
        self.process(10)
        # the results of a model (made by the operations of the module)
        rings = slicer.modules.astromodeling.logic().EnsureParamsTable(modeling)
        self.assertIsNotNone(rings)
        rings.GetTable().SetNumberOfRows(3)
        rings.Modified()
        majorPoints = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsFiducialNode", "MarkupsFiducialsMajor")
        majorPoints.AddControlPoint(1, 2, 3)
        modelContours = slicer.mrmlScene.GetFirstNodeByName("AstroModelingContoursSeg")
        if modelContours is None:
            modelContours = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentationNode", "AstroModelingContoursSeg")
        modelContours.GetSegmentation().AddEmptySegment("DataContours")
        model = slicer.modules.volumes.logic().CloneVolume(slicer.mrmlScene, volume, volume.GetName() + "_model_1")
        self.process(10)
        derived = Astro.derivedNodes(volume, slicer.mrmlScene)
        self.assertIn(model, derived)
        classes = {node.GetClassName() for node in derived}
        self.assertIn("vtkMRMLMarkupsFiducialNode", classes)
        self.assertIn("vtkMRMLPlotSeriesNode", classes)
        self.assertIn("vtkMRMLTableNode", classes)
        derivedIDs = [node.GetID() for node in derived]
        workbench().enter()
        self.process()
        self.assertTrue(workbench().drawer.deleteDataset(volume, ask=False, withProducts=True))
        self.process(30)
        self.assertIsNone(volume.GetScene())
        for nodeID in derivedIDs:
            self.assertIsNone(slicer.mrmlScene.GetNodeByID(nodeID), nodeID)
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLAstroVolumeNode"), 0)
        # the results of Modeling cleared
        self.assertEqual(rings.GetTable().GetNumberOfRows(), 0)
        self.assertEqual(majorPoints.GetNumberOfControlPoints(), 0)
        self.assertTrue(modelContours.GetScene() is None or modelContours.GetSegmentation().GetNumberOfSegments() == 0)
        # the plot of the Profiles panel cleared
        plot = slicer.util.findChild(slicer.modules.astroprofiles.widgetRepresentation(), "SpectrumPlot")
        self.assertEqual(len(plot.spectrumY()), 0)
        # the application keeps working: new data
        other = sampleData.downloadSample("WEIN069")
        self.assertIsNotNone(other)
        self.process(20)

    def test_ProductsKeepViews(self):
        """A product on the voxels of the dataset (e.g. its smoothed copy),
        clicked or dropped on a view: the views keep their zoom, channel and
        camera, the dataset stays active and the channel maps stay linked."""
        from AstroWorkbenchLib import Astro
        volume = self.cube()
        selection = slicer.app.applicationLogic().GetSelectionNode()
        selection.SetActiveVolumeID(volume.GetID())
        smoothed = slicer.modules.volumes.logic().CloneVolume(slicer.mrmlScene, volume,
                                                              volume.GetName() + "_Filtered_Box_1")
        self.assertTrue(Astro.sameGrid(volume, smoothed))
        wb = workbench()
        wb.enter()
        wb.setViewLayout(2)
        self.wait(0.6)
        layoutManager = slicer.app.layoutManager()
        red = layoutManager.sliceWidget("Red").mrmlSliceNode()
        yellow = layoutManager.sliceWidget("Yellow").mrmlSliceNode()
        wb.setActiveView("Red")
        wb.scrubberBar.scrubber.currentIndex = 20
        self.process()
        fov = list(red.GetFieldOfView())
        red.SetFieldOfView(fov[0] / 2, fov[1] / 2, fov[2])
        camera = layoutManager.threeDWidget(0).threeDView().cameraNode()
        camera.GetCamera().Azimuth(50)
        camera.Modified()
        self.process()
        position = list(camera.GetPosition())
        redFOV = list(red.GetFieldOfView())

        # click: in place of the dataset in all the views showing it (channel
        # maps, position-velocity, 3D); zoom, channel and camera kept; the
        # dataset stays active
        logic = slicer.modules.astrovolume.logic()
        showing = [name for name in layoutManager.sliceViewNames()
                   if layoutManager.sliceWidget(name).mrmlSliceCompositeNode().GetBackgroundVolumeID() == volume.GetID()]
        self.assertGreaterEqual(len(showing), 2, showing)

        def checkViews(shown):
            for name in showing:
                self.assertEqual(layoutManager.sliceWidget(name).mrmlSliceCompositeNode().GetBackgroundVolumeID(),
                                 shown.GetID(), name)
            # the zoom: the height of the field of view (its width follows the
            # shape of the view, e.g. while the window is laid out)
            self.assertAlmostEqual(red.GetFieldOfView()[1], redFOV[1], places=3)
            self.assertEqual(Astro.channelOfSlice(shown, red), 20)
            for a, b in zip(camera.GetPosition(), position):
                self.assertAlmostEqual(a, b, places=3)
            other = volume if shown is smoothed else smoothed
            self.assertTrue(logic.GetVolumeRenderingDisplayNode(shown, False).GetVisibility())
            otherRendering = logic.GetVolumeRenderingDisplayNode(other, False)
            self.assertTrue(otherRendering is None or not otherRendering.GetVisibility())
            self.assertIs(wb.activeVolume(), volume)

        wb.showProduct(smoothed.GetID())
        self.wait(0.4)
        checkViews(smoothed)
        self.assertTrue(logic.IsProductShownInViews(smoothed))
        bar = wb.views.bars["3D0"]
        bar.refresh()
        self.assertIn(smoothed.GetName(), bar.detail.text)
        # the dataset card clicked: the dataset back in all its views
        wb.onDatasetClicked(volume)
        self.wait(0.4)
        checkViews(volume)
        self.assertFalse(logic.IsProductShownInViews(smoothed))
        # the product clicked twice: shown, then the dataset back (the Show
        # buttons of the panels toggle the same way)
        wb.showProduct(smoothed.GetID())
        self.wait(0.3)
        checkViews(smoothed)
        logic.ToggleProductInViews(smoothed, volume)
        self.wait(0.3)
        checkViews(volume)

        # dropped on the red view: zoom and channel kept, dataset active
        wb.dropNode("Red", smoothed.GetID())
        self.wait(0.4)
        composite = layoutManager.sliceWidget("Red").mrmlSliceCompositeNode()
        self.assertEqual(composite.GetBackgroundVolumeID(), smoothed.GetID())
        self.assertAlmostEqual(red.GetFieldOfView()[1], redFOV[1], places=3)
        self.assertEqual(Astro.channelOfSlice(smoothed, red), 20)
        self.assertIs(wb.activeVolume(), volume)
        # the channel maps are linked: the cube in yellow follows the
        # scrubber moving the red one
        yellowComposite = layoutManager.sliceWidget("Yellow").mrmlSliceCompositeNode()
        yellowComposite.SetBackgroundVolumeID(volume.GetID())
        yellow.SetOrientation("XY")
        wb.setActiveView("Red")
        wb.scrubberBar.scrubber.currentIndex = 30
        self.process()
        self.assertEqual(Astro.channelOfSlice(smoothed, red), 30)
        self.assertEqual(Astro.channelOfSlice(volume, yellow), 30)
        # the dataset card clicked: the dataset in every view again (also in
        # the view the product was dropped on), zoom and channel kept
        wb.onDatasetClicked(volume)
        self.wait(0.4)
        self.assertEqual(composite.GetBackgroundVolumeID(), volume.GetID())
        self.assertAlmostEqual(red.GetFieldOfView()[1], redFOV[1], places=3)
        self.assertEqual(Astro.channelOfSlice(volume, red), 30)

        # Inspect: the dataset or one of its products, chosen in its selector
        inspector = wb.inspector
        wb.refresh()
        self.wait(0.4)
        selector = inspector.inspectSelector
        self.assertFalse(selector.isHidden())
        self.assertEqual(sorted(selector.nodeIDFilter()), sorted([volume.GetID(), smoothed.GetID()]))
        selector.setCurrentNode(smoothed)
        self.process()
        self.assertIs(inspector.inspected, smoothed)
        self.assertIs(wb.activeVolume(), volume)
        # kept by the refreshes of the Workbench
        wb.refresh()
        self.wait(0.4)
        self.assertIs(inspector.inspected, smoothed)

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
        # the views follow: light bars and slice backgrounds, Slicer's
        # gradient in the 3D views
        sliceView = slicer.app.layoutManager().sliceWidget("Red").sliceView()
        self.assertEqual(sliceView.backgroundColor.name(), theme.color("view").name())
        viewNode = slicer.app.layoutManager().threeDWidget(0).mrmlViewNode()
        self.assertAlmostEqual(viewNode.GetBackgroundColor()[2], 0.9098039215686275, places=6)
        self.assertAlmostEqual(viewNode.GetBackgroundColor2()[2], 0.7450980392156863, places=6)
        theme.setMode("dark")
        self.process()
        self.assertEqual(sliceView.backgroundColor.name(), "#070a12")
        self.assertAlmostEqual(viewNode.GetBackgroundColor2()[2], 18 / 255.0, places=6)
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

        # compass and scale bar of the sky view: from the WCS
        import math
        from AstroWorkbenchLib import Astro
        redNode = layoutManager.sliceWidget("Red").mrmlSliceNode()
        geometry = Astro.skyGeometry(volume, redNode)
        self.assertIsNotNone(geometry)
        arcsecPerPixel, north, east = geometry
        self.assertLess(abs(north[0]), 0.1)
        self.assertLess(north[1], -0.99)       # north up
        self.assertLess(east[0], -0.99)        # east left
        xyToRAS = redNode.GetXYToRAS()
        mmPerPixel = math.hypot(xyToRAS.GetElement(0, 0), xyToRAS.GetElement(1, 0), xyToRAS.GetElement(2, 0))
        cdelt = abs(float(volume.GetAttribute("SlicerAstro.CDELT1"))) * 3600.0
        self.assertAlmostEqual(arcsecPerPixel / (cdelt / volume.GetSpacing()[0] * mmPerPixel), 1.0, delta=0.03)
        pixels, label = Astro.scaleBar(arcsecPerPixel)
        self.assertTrue(30 < pixels < 300)
        self.assertTrue(label[-1] in "″′°")

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

    def test_HIContours(self):
        """HI contours over an optical image: nothing is computed when the
        two are loaded (no moment map, no contours); the user draws them
        from the Contours section of the image (source: the HI cube); the
        chip of the image card toggles them; contours in units of the noise
        of a cube over itself."""
        import AstroSampleData
        from AstroWorkbenchLib import Contours

        workbench().enter()
        self.process()
        logic = AstroSampleData.AstroSampleDataLogic()
        cube = logic.downloadSample("NGC2403")
        image = logic.downloadSample("NGC2403_DSS")
        workbench().refresh()
        self.process(30)
        astroLogic = slicer.modules.astrovolume.logic()

        # nothing automatic
        moments = [node for node in slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode")
                   if node.GetAttribute("SlicerAstro.DATAMODEL") == "ZEROMOMENTMAP"]
        self.assertEqual(moments, [])
        contours = astroLogic.GetContoursSegmentationForVolume(image, False)
        self.assertTrue(contours is None or contours.GetSegmentation().GetNumberOfSegments() == 0)

        # drawn by the user: the HI cube over the image
        workbench().activate(image)
        self.process()
        section = workbench().inspector.contoursWidget
        section.setSourceNode(cube)
        section.setLevels("3, 6", section.Sigma)
        self.assertEqual(section.drawContours(), 2)
        slicer.qSlicerAstroBackgroundRunner().waitForJobs()
        self.process(20)
        contours = astroLogic.GetContoursSegmentationForVolume(image, False)
        sources = Contours.contourSources(contours)
        self.assertEqual(len(sources), 1)
        sourceID = list(sources.keys())[0]
        source = slicer.mrmlScene.GetNodeByID(sourceID)
        self.assertTrue(Contours.isHIMap(source) or Contours.isHICube(source))

        # the chip of the image card toggles them
        workbench().refresh()
        self.process()
        card = slicer.util.findChild(workbench().drawer, f"DatasetCard_{image.GetID()}")
        self.assertEqual(card.actionChipCount(), 1)
        chip = slicer.util.findChild(card, f"ActionChip_{sourceID}")
        self.assertIn("HI contours", chip.text)
        self.assertTrue(chip.checked)
        chip.click()
        self.process()
        self.assertFalse(slicer.vtkSlicerAstroVolumeLogic.GetContourSourceVisibility(contours, sourceID))

        # Contours section: 3, 6 sigma of the cube over itself
        workbench().activate(cube)
        self.process()
        section.setSourceNode(None)
        section.setLevels("3, 6", section.Sigma)
        self.assertEqual(section.drawContours(), 2)
        self.process()
        own = Contours.contourSources(astroLogic.GetContoursSegmentationForVolume(cube, False))
        self.assertEqual(own.get(cube.GetID()), 2)
        self.assertIn("2 levels drawn", section.status())
        eye = slicer.util.findChild(section, f"ContoursSourceShow_{cube.GetID()}")
        self.assertTrue(eye.checked)
        eye.click()
        self.assertFalse(slicer.vtkSlicerAstroVolumeLogic.GetContourSourceVisibility(
            astroLogic.GetContoursSegmentationForVolume(cube, False), cube.GetID()))
        self.delayDisplay("Test passed")

    def test_Sources(self):
        """A SoFiA catalogue: a product of its cube ("N sources"), its table
        in Inspect > Sources; a click on a source moves the views to it and
        selects its point."""
        import os

        import AstroSampleData
        import vtk

        workbench().enter()
        self.process()
        logic = AstroSampleData.AstroSampleDataLogic()
        cube = logic.downloadSample("WEIN069")
        logic.downloadSample("WEIN069_MASK")
        slicer.util.selectModule("AstroWorkbench")
        workbench().activate(cube)
        self.process(30)
        from AstroWorkbenchLib import Astro
        catalogues = Astro.cataloguesOf(cube)
        self.assertEqual(len(catalogues), 1)
        table = catalogues[0]
        rows = table.GetTable().GetNumberOfRows()
        self.assertGreater(rows, 0)
        products = [product for product in Astro.productsOf(cube, slicer.mrmlScene) if product[4] is table]
        self.assertEqual(len(products), 1)
        self.assertIn(f"{rows} source", products[0][2])

        # the product opens the Sources section with the catalogue
        workbench().showProduct(table.GetID())
        self.process()
        section = workbench().inspector.sourcesWidget
        self.assertEqual(workbench().inspector.stack.currentIndex, 4)
        self.assertEqual(section.catalogueCount(), 1)
        self.assertEqual(section.sourceCount(), rows)
        tableView = slicer.util.findChild(section, "SourcesTable")
        self.assertEqual(tableView.model().rowCount(), rows)

        # a source: the views move to it, its point is selected, its segment
        # in the mask of the cube is found
        points = slicer.vtkSlicerAstroVolumeLogic.GetSourceCataloguePoints(table)
        tableView.selectRow(0)
        self.process()
        self.assertTrue(points.GetNthControlPointSelected(0))
        self.assertFalse(points.GetNthControlPointSelected(1) if points.GetNumberOfControlPoints() > 1 else False)
        position = [0.0, 0.0, 0.0]
        points.GetNthControlPointPositionWorld(0, position)
        redNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeRed")
        offset = redNode.GetSliceOffset()
        normal = [redNode.GetSliceToRAS().GetElement(r, 2) for r in range(3)]
        self.assertAlmostEqual(offset, sum(n * p for n, p in zip(normal, position)), delta=1.0)
        self.assertIn(points.GetNthControlPointLabel(0), section.status())
        mask = slicer.modules.astrovolume.logic().GetMaskSegmentationForVolume(cube, False)
        segmentID = section.selectedSegmentID()
        self.assertNotEqual(segmentID, "")
        sourceID = vtk.reference("")
        mask.GetSegmentation().GetSegment(segmentID).GetTag("SlicerAstro.SourceID", sourceID)
        self.assertEqual("id " + str(sourceID), points.GetNthControlPointDescription(0))
        # points, labels and 2D display
        slicer.util.findChild(section, "SourcesPoints").checked = True
        self.assertTrue(points.GetDisplayNode().GetVisibility3D())
        slicer.util.findChild(section, "SourcesLabels").checked = False
        self.assertFalse(points.GetDisplayNode().GetPointLabelsVisibility())
        slicer.util.findChild(section, "SourcesSlices").checked = True
        self.assertTrue(points.GetDisplayNode().GetVisibility2D())

        # the catalogue loaded again from the section (file type
        # SoFiACatalogFile), placed on the active cube
        cataloguePath = os.path.join(slicer.app.cachePath, "WEIN069_cat.txt")
        self.assertTrue(os.path.exists(cataloguePath), cataloguePath)
        self.assertTrue(section.loadCatalogue(cataloguePath), section.status())
        self.assertEqual(section.catalogueCount(), 2)
        self.assertEqual(section.status(), f"{rows} sources imported")
        self.assertFalse(section.loadCatalogue("/nonexistent/catalogue.txt"))
        self.assertNotEqual(section.status(), "")
        self.delayDisplay("Test passed")
