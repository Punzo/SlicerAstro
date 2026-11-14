import math
import os
import time

import numpy as np
import vtk

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroSliceViewsTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Slice Views Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Test of the SlicerAstro slice views: astronomical orientation presets, WCS
axes and beam displayable managers, beam button of the slice controller and
mouse interactions (scroll, zoom, pan).
"""
        self.parent.hidden = True


class AstroSliceViewsTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroSliceViewsTestLogic(ScriptedLoadableModuleLogic):
    pass


class AstroSliceViewsTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_AstroSliceViews()
        self.setUp()
        self.test_WCSAxes()
        self.setUp()
        self.test_MouseMoveCompression()
        self.setUp()
        self.test_LineMeasurements()

    def sliceWidgets(self):
        layoutManager = slicer.app.layoutManager()
        return {name: layoutManager.sliceWidget(name) for name in layoutManager.sliceViewNames()}

    def test_AstroSliceViews(self):
        import AstroSampleData

        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("WEIN069")
        self.assertIsNotNone(volumeNode)
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        slicer.app.layoutManager().setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourUpView)
        slicer.app.processEvents()

        widgets = self.sliceWidgets()
        expectedOrientations = {"Red": "XY", "Yellow": "XZ", "Green": "ZY"}
        for name, widget in widgets.items():
            sliceNode = widget.mrmlSliceNode()

            # Only the SlicerAstro slice widgets, with astronomical presets
            self.assertEqual(widget.className(), "qMRMLSliceAstroWidget", name)
            presets = vtk.vtkStringArray()
            sliceNode.GetSliceOrientationPresetNames(presets)
            names = {presets.GetValue(i) for i in range(presets.GetNumberOfValues())}
            self.assertTrue({"XY", "XZ", "ZY"} <= names, name)
            self.assertFalse(names & {"Axial", "Sagittal", "Coronal"}, name)
            self.assertEqual(sliceNode.GetOrientation(), expectedOrientations[name], name)

            # WCS axes and beam displayable managers; the Slicer ruler is replaced
            view = widget.sliceView()
            self.assertIsNotNone(view.displayableManagerByClassName("vtkMRMLAstroTwoDAxesDisplayableManager"), name)
            self.assertIsNotNone(view.displayableManagerByClassName("vtkMRMLAstroBeamDisplayableManager"), name)
            self.assertIsNone(view.displayableManagerByClassName("vtkMRMLRulerDisplayableManager"), name)
            self.assertNotEqual(sliceNode.GetRulerType(), slicer.vtkMRMLAbstractViewNode.RulerTypeNone, name)

        # The Slicer application logic uses the slice logics of the views
        appLogic = slicer.app.applicationLogic()
        for name, widget in widgets.items():
            self.assertEqual(appLogic.GetSliceLogic(widget.mrmlSliceNode()), widget.sliceLogic(), name)

        # Beam button: enabled only in the XY views showing data with a beam
        def beamButton(name):
            return slicer.util.findChild(widgets[name], "BeamButton")

        self.assertFalse(beamButton("Red").enabled)  # WEIN069 has no beam (BMAJ = -1)
        volumeNode.SetAttribute("SlicerAstro.BMAJ", "0.012")
        volumeNode.SetAttribute("SlicerAstro.BMIN", "0.006")
        volumeNode.SetAttribute("SlicerAstro.BPA", "30")
        volumeNode.Modified()
        slicer.app.processEvents()
        self.assertTrue(beamButton("Red").enabled)
        self.assertFalse(beamButton("Yellow").enabled)  # not the sky plane

        # Mouse interactions: scroll (slice offset), right drag (zoom), middle drag (pan)
        for name in ("Red", "Yellow"):
            widget = widgets[name]
            view = widget.sliceView()
            sliceNode = widget.mrmlSliceNode()
            interactor = view.interactor()
            x, y = view.width // 2, view.height // 2

            offset = sliceNode.GetSliceOffset()
            interactor.SetEventInformation(x, y, 0, 0, chr(0), 0, None)
            interactor.MouseMoveEvent()
            interactor.MouseWheelForwardEvent()
            slicer.app.processEvents()
            self.assertNotAlmostEqual(sliceNode.GetSliceOffset(), offset, msg=name + ": scroll")

            fieldOfView = np.array(sliceNode.GetFieldOfView())
            interactor.SetEventInformation(x, y, 0, 0, chr(0), 0, None)
            interactor.RightButtonPressEvent()
            interactor.SetEventInformation(x, y + 40, 0, 0, chr(0), 0, None)
            interactor.MouseMoveEvent()
            interactor.RightButtonReleaseEvent()
            slicer.app.processEvents()
            self.assertFalse(np.allclose(sliceNode.GetFieldOfView(), fieldOfView), name + ": zoom")

            origin = np.array(sliceNode.GetXYZOrigin())
            interactor.SetEventInformation(x, y, 0, 0, chr(0), 0, None)
            interactor.MiddleButtonPressEvent()
            interactor.SetEventInformation(x + 30, y + 20, 0, 0, chr(0), 0, None)
            interactor.MouseMoveEvent()
            interactor.MiddleButtonReleaseEvent()
            slicer.app.processEvents()
            self.assertFalse(np.allclose(sliceNode.GetXYZOrigin(), origin), name + ": pan")

        # Layout changes keep the astronomical orientations and the interactions
        slicer.app.layoutManager().setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutFourOverFourView)
        slicer.app.processEvents()
        for name, widget in self.sliceWidgets().items():
            self.assertIn(widget.mrmlSliceNode().GetOrientation(), ("XY", "XZ", "ZY"), name)
            self.assertEqual(appLogic.GetSliceLogic(widget.mrmlSliceNode()), widget.sliceLogic(), name)

        self.delayDisplay("Test passed")

    #
    # WCS axes
    #

    def writeRotatedCube(self):
        """Synthetic cube: TAN projection at declination +60 degrees, rotated by
        30 degrees (CROTA2), radio velocity axis."""
        from AstroFITSIOTest import beamCards, celestialWCS, radioVelocityWCS, writeFITS

        shape = (8, 120, 160)
        y, x = np.mgrid[0:shape[1], 0:shape[2]]
        plane = 0.05 * np.exp(-(((x - 80.) / 20.) ** 2 + ((y - 60.) / 15.) ** 2))
        data = np.repeat(plane[np.newaxis], shape[0], axis=0).astype(np.float32)
        wcs = celestialWCS(projection="TAN", crval=(150.0, 60.0), crpix=(80.5, 60.5),
                           cdelt=(-0.000833333, 0.000833333))
        wcs += [("CROTA2", 30.0)] + radioVelocityWCS() + beamCards() + [("OBJECT", "ROTATED")]
        path = os.path.join(slicer.app.temporaryPath, "AstroSliceViewsTestRotated.fits")
        writeFITS(path, data, -32, wcs)
        return path

    def waitForRenders(self, view, seconds=0.5):
        """Process the events (the views render when they are scheduled to);
        return the number of renders."""
        renders = [0]
        renderWindow = view.renderWindow()
        tag = renderWindow.AddObserver(vtk.vtkCommand.EndEvent, lambda caller, event: renders.__setitem__(0, renders[0] + 1))
        start = time.time()
        while time.time() - start < seconds:
            slicer.app.processEvents()
            time.sleep(0.005)
        renderWindow.RemoveObserver(tag)
        return renders[0]

    def axesActors(self, view):
        """Polydata of the ticks and visible labels of the WCS axes."""
        displayableManager = view.displayableManagerByClassName("vtkMRMLAstroTwoDAxesDisplayableManager")
        renderer = displayableManager.vtkMarkerRenderer()
        ticks = None
        labels = []
        actors = renderer.GetActors2D()
        actors.InitTraversal()
        for _ in range(actors.GetNumberOfItems()):
            actor = actors.GetNextActor2D()
            if actor.IsA("vtkTextActor"):
                if actor.GetVisibility():
                    labels.append((actor.GetInput(), actor.GetPosition()))
            elif actor.GetMapper() and actor.GetMapper().GetInput():
                ticks = actor.GetMapper().GetInput()
        return ticks, labels

    def majorTicks(self, view):
        """Positions of the major ticks along the bottom (x) and left (y)
        sides: segments perpendicular to the side, 6 to 20 pixels long."""
        ticks, _ = self.axesActors(view)
        self.assertIsNotNone(ticks)
        points = [ticks.GetPoint(i) for i in range(ticks.GetNumberOfPoints())]
        bottom, left = [], []
        for start, end in zip(points[0::2], points[1::2]):
            if start[0] == end[0] and 6 < abs(end[1] - start[1]) < 20 and start[1] < 20:
                bottom.append(start[0])
            elif start[1] == end[1] and 6 < abs(end[0] - start[0]) < 20 and start[0] < 20:
                left.append(start[1])
        return sorted(bottom), sorted(left)

    def assertLabelsOnTicks(self, view, bottom, left, message):
        """The rendered labels are centred on a major tick: horizontally for
        the bottom axis, vertically for the left axis (within 2 pixels)."""
        displayableManager = view.displayableManagerByClassName("vtkMRMLAstroTwoDAxesDisplayableManager")
        renderer = displayableManager.vtkMarkerRenderer()
        actors = renderer.GetActors2D()
        actors.InitTraversal()
        for _ in range(actors.GetNumberOfItems()):
            actor = actors.GetNextActor2D()
            if not actor.IsA("vtkTextActor") or not actor.GetVisibility():
                continue
            # bounding box of the rendered text, relative to the anchor (position)
            box = [0., 0., 0., 0.]
            actor.GetBoundingBox(renderer, box)
            x, y = actor.GetPosition()
            if y <= 20.:
                centre, ticks = x + 0.5 * (box[0] + box[1]), bottom
            else:
                centre, ticks = y + 0.5 * (box[2] + box[3]), left
            self.assertTrue(ticks, f"{message}: label {actor.GetInput()} without ticks")
            distance = min(abs(centre - tick) for tick in ticks)
            self.assertLess(distance, 2., f"{message}: label {actor.GetInput()} {distance:.1f} px from its tick")

    def worldAt(self, sliceNode, volumeNode, x, y):
        """WCS coordinates of the point (x, y) of the slice view."""
        ras = sliceNode.GetXYToRAS().MultiplyPoint([x, y, 0., 1.])
        rasToIJK = vtk.vtkMatrix4x4()
        volumeNode.GetRASToIJKMatrix(rasToIJK)
        ijk = rasToIJK.MultiplyPoint(ras)[:3]
        world = [0., 0., 0.]
        self.assertTrue(volumeNode.GetAstroVolumeDisplayNode().GetReferenceSpace(ijk, world))
        return world

    def assertTicksOnWCS(self, view, volumeNode, horizontalAxis, verticalAxis, message):
        """The major ticks are where the sides of the view cross multiples of
        the tick step (within half a pixel), and they have labels."""
        sliceNode = view.mrmlSliceNode()
        bottom, left = self.majorTicks(view)
        _, labels = self.axesActors(view)
        self.assertGreaterEqual(len(labels), 2, message)
        self.assertLabelsOnTicks(view, bottom, left, message)
        for side, positions, axis in (("bottom", bottom, horizontalAxis), ("left", left, verticalAxis)):
            self.assertGreaterEqual(len(positions), 2, f"{message}: {side} ticks")
            def value(position):
                world = self.worldAt(sliceNode, volumeNode, position, 0.) if side == "bottom" \
                    else self.worldAt(sliceNode, volumeNode, 0., position)
                return world[axis]
            values = np.array([value(position) for position in positions])
            if axis == 0:
                values = np.degrees(np.unwrap(np.radians(values)))
            step = np.median(np.abs(np.diff(values)))
            self.assertGreater(step, 0., f"{message}: {side} step")
            for position, tickValue in zip(positions, values):
                valuePerPixel = abs(value(position + 1.) - value(position))
                residual = (tickValue / step - round(tickValue / step)) * step
                self.assertLess(abs(residual) / valuePerPixel, 0.5,
                                f"{message}: {side} tick at {position:.1f} px, value {tickValue}, step {step}")
        return bottom, left

    def test_LineMeasurements(self):
        """Markups lines measure the angular separation on the sky and the
        velocity difference with the WCS of the active volume (not the length
        in millimetres), also after saving and loading the scene."""
        import math
        volumeNode = slicer.util.loadVolume(self.writeRotatedCube())
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActiveVolumeID(volumeNode.GetID())
        display = volumeNode.GetAstroVolumeDisplayNode()
        ijkToRAS = vtk.vtkMatrix4x4()
        volumeNode.GetIJKToRASMatrix(ijkToRAS)

        def ras(ijk):
            return ijkToRAS.MultiplyPoint(list(ijk) + [1.])[:3]

        def world(ijk):
            w = [0., 0., 0.]
            self.assertTrue(display.GetReferenceSpace([float(v) for v in ijk], w))
            return w

        def separation(w1, w2):
            l1, b1, l2, b2 = (math.radians(v) for v in (w1[0], w1[1], w2[0], w2[1]))
            c = math.sin(b1) * math.sin(b2) + math.cos(b1) * math.cos(b2) * math.cos(l2 - l1)
            return math.degrees(math.acos(min(1., c)))

        start, end = (20., 30., 1.), (130., 95., 6.)
        line = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsLineNode", "L")
        line.AddControlPointWorld(vtk.vtkVector3d(*ras(start)))
        line.AddControlPointWorld(vtk.vtkVector3d(*ras(end)))
        slicer.app.processEvents()

        measurement = line.GetMeasurement("separation")
        self.assertIsNotNone(measurement)
        self.assertTrue(measurement.IsA("vtkMRMLAstroMeasurementLine"))
        expected = separation(world(start), world(end))
        self.assertAlmostEqual(measurement.GetValue(), expected, delta=1.e-9)
        self.assertEqual(measurement.GetUnits(), "'")  # 0.1 deg: arcmin
        velocity = line.GetMeasurement("velocity difference")
        self.assertTrue(velocity.GetValueDefined())
        self.assertAlmostEqual(velocity.GetValue(), world(end)[2] - world(start)[2], delta=1.e-6)
        self.assertIn("km/s", velocity.GetUnits())
        self.assertFalse(line.GetMeasurement("length").GetEnabled())
        self.assertIn("separation", line.GetPropertiesLabelText())

        # a line in one channel (sky plane): no velocity difference
        line.SetNthControlPointPositionWorld(1, ras((130., 95., 1.)))
        self.assertFalse(line.GetMeasurement("velocity difference").GetValueDefined())
        self.assertAlmostEqual(line.GetMeasurement("separation").GetValue(),
                               separation(world(start), world((130., 95., 1.))), delta=1.e-9)

        # saved and loaded: computed again with the WCS
        scenePath = os.path.join(slicer.app.temporaryPath, "AstroSliceViewsTestMeasurements.mrb")
        if os.path.exists(scenePath):
            os.remove(scenePath)
        self.assertTrue(slicer.util.saveScene(scenePath))
        slicer.mrmlScene.Clear(0)
        slicer.util.loadScene(scenePath)
        os.remove(scenePath)
        line = slicer.util.getNode("L")
        selectionNode.SetActiveVolumeID(slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroVolumeNode").GetID())
        slicer.app.processEvents()
        measurement = line.GetMeasurement("separation")
        self.assertTrue(measurement.IsA("vtkMRMLAstroMeasurementLine"))
        self.assertAlmostEqual(measurement.GetValue(), separation(world(start), world((130., 95., 1.))), delta=1.e-9)

    def test_MouseMoveCompression(self):
        """Mouse moves of the slice views: hover moves dropped (duplicates of the
        mouse moves), moves compressed (by default on Wayland): a drag of queued
        moves pans once, to the same position as without compression."""
        import qt

        volumeNode = slicer.util.loadVolume(self.writeRotatedCube())
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpRedSliceView)
        widget = layoutManager.sliceWidget("Red")
        view = widget.sliceView()
        sliceNode = widget.mrmlSliceNode()
        widget.sliceLogic().GetSliceCompositeNode().SetBackgroundVolumeID(volumeNode.GetID())
        sliceNode.SetOrientation("XY")
        self.waitForRenders(view)
        target = [child for child in view.children() if child.inherits("QVTKOpenGLNativeWidget")][0]
        module = slicer.modules.astrovolume
        enabled = module.mouseMoveCompression()

        def post(kind, x, y, button, buttons):
            event = qt.QMouseEvent(kind, qt.QPointF(x, y), qt.QPointF(target.mapToGlobal(qt.QPoint(int(x), int(y)))),
                                   button, buttons, qt.Qt.NoModifier)
            qt.QCoreApplication.postEvent(target, event)

        def drag(moves=50):
            widget.sliceLogic().FitSliceToAll()
            slicer.app.processEvents()
            x, y = view.width / 2, view.height / 2
            post(qt.QEvent.MouseButtonPress, x, y, qt.Qt.MiddleButton, qt.Qt.MiddleButton)
            for step in range(1, moves + 1):
                post(qt.QEvent.MouseMove, x + step, y + 0.5 * step, qt.Qt.NoButton, qt.Qt.MiddleButton)
            post(qt.QEvent.MouseButtonRelease, x + moves, y + 0.5 * moves, qt.Qt.MiddleButton, qt.Qt.NoButton)
            for _ in range(5):
                slicer.app.processEvents()
            return [round(v, 6) for v in sliceNode.GetXYZOrigin()]

        try:
            module.setMouseMoveCompression(False)
            droppedBefore = module.droppedHoverMoves()
            uncompressed = drag()
            # Qt sends a hover move with every mouse move: dropped (VTK turns both
            # into a mouse move of the interactor)
            self.assertGreaterEqual(module.droppedHoverMoves() - droppedBefore, 50)
            self.assertNotEqual(uncompressed, [0., 0., 0.], "the drag pans")
            module.setMouseMoveCompression(True)
            self.assertTrue(module.mouseMoveCompression())
            compressedBefore = module.compressedMouseMoves()
            compressed = drag()
            self.assertEqual(module.compressedMouseMoves() - compressedBefore, 49)
            self.assertEqual(compressed, uncompressed)
        finally:
            module.setMouseMoveCompression(enabled)

    def test_WCSAxes(self):
        """WCS axes of a rotated cube: ticks where the sides of the view cross
        the coordinate values, updated by panning, zooming and changing the
        orientation, once per render."""
        volumeNode = slicer.util.loadVolume(self.writeRotatedCube())
        self.assertTrue(volumeNode.IsA("vtkMRMLAstroVolumeNode"))
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUpRedSliceView)
        widget = layoutManager.sliceWidget("Red")
        view = widget.sliceView()
        sliceNode = widget.mrmlSliceNode()
        widget.sliceLogic().GetSliceCompositeNode().SetBackgroundVolumeID(volumeNode.GetID())
        sliceNode.SetOrientation("XY")
        sliceNode.SetRulerType(slicer.vtkMRMLAbstractViewNode.RulerTypeThin)
        widget.sliceLogic().FitSliceToAll()
        self.waitForRenders(view)

        # sky plane: right ascension (bottom), declination (left)
        bottom, left = self.assertTicksOnWCS(view, volumeNode, 0, 1, "XY")

        # Pan (middle drag): the view is rendered again and the ticks move with the image
        interactor = view.interactor()
        x, y = view.width // 2, view.height // 2
        interactor.SetEventInformation(x, y, 0, 0, chr(0), 0, None)
        interactor.MiddleButtonPressEvent()
        interactor.SetEventInformation(x + 40, y + 25, 0, 0, chr(0), 0, None)
        interactor.MouseMoveEvent()
        interactor.MiddleButtonReleaseEvent()
        self.assertGreaterEqual(self.waitForRenders(view), 1, "pan: render")
        pannedBottom, pannedLeft = self.assertTicksOnWCS(view, volumeNode, 0, 1, "XY panned")
        self.assertNotEqual(pannedBottom, bottom, "pan: bottom ticks updated")
        self.assertNotEqual(pannedLeft, left, "pan: left ticks updated")

        # Zoom (right drag): tick spacing (pixels) or step changes, ticks still exact
        fieldOfView = sliceNode.GetFieldOfView()[0]
        interactor.SetEventInformation(x, y, 0, 0, chr(0), 0, None)
        interactor.RightButtonPressEvent()
        interactor.SetEventInformation(x, y + 80, 0, 0, chr(0), 0, None)
        interactor.MouseMoveEvent()
        interactor.RightButtonReleaseEvent()
        self.assertGreaterEqual(self.waitForRenders(view), 1, "zoom: render")
        self.assertNotAlmostEqual(sliceNode.GetFieldOfView()[0], fieldOfView, msg="zoom")
        zoomedBottom, zoomedLeft = self.assertTicksOnWCS(view, volumeNode, 0, 1, "XY zoomed")
        self.assertNotEqual(zoomedBottom, pannedBottom, "zoom: bottom ticks updated")

        # Orientation: right ascension and velocity, velocity and declination
        for orientation, horizontalAxis, verticalAxis in (("XZ", 0, 2), ("ZY", 2, 1), ("XY", 0, 1)):
            sliceNode.SetOrientation(orientation)
            widget.sliceLogic().FitSliceToAll()
            self.assertGreaterEqual(self.waitForRenders(view), 1, orientation + ": render")
            self.assertTicksOnWCS(view, volumeNode, horizontalAxis, verticalAxis, orientation)

        # PV slice (reformatted, containing the spectral axis): angular offset
        # along the slice (bottom) and velocity (left), labels on their ticks
        sliceNode.SetOrientation("XZ")
        transform = vtk.vtkTransform()
        transform.SetMatrix(sliceNode.GetSliceToRAS())
        transform.RotateY(30.)
        sliceNode.GetSliceToRAS().DeepCopy(transform.GetMatrix())
        sliceNode.UpdateMatrices()
        widget.sliceLogic().FitSliceToAll()
        self.assertGreaterEqual(self.waitForRenders(view), 1, "PV: render")
        self.assertEqual(sliceNode.GetOrientation(), "Reformat")
        pvBottom, pvLeft = self.majorTicks(view)
        self.assertGreaterEqual(len(pvBottom), 2, "PV: offset ticks")
        self.assertGreaterEqual(len(pvLeft), 2, "PV: velocity ticks")
        self.assertLabelsOnTicks(view, pvBottom, pvLeft, "PV")
        sliceNode.SetOrientation("XY")
        widget.sliceLogic().FitSliceToAll()
        self.waitForRenders(view)

        # The axes are updated once per render, not for every modification of
        # the slice node (e.g. every mouse move while panning)
        ticks, _ = self.axesActors(view)
        ticksTime = ticks.GetMTime()
        for step in range(5):
            sliceNode.SetXYZOrigin(step * 3., 0., 0.)
        self.assertEqual(ticks.GetMTime(), ticksTime, "no update without render")
        view.forceRender()
        self.assertGreater(ticks.GetMTime(), ticksTime, "update at render")
        self.assertTicksOnWCS(view, volumeNode, 0, 1, "XY after render")

        # Beam: an ellipse on the sky with full axes BMAJ and BMIN (FWHM) and the
        # major axis at BPA from north through east (cube rotated by 30 degrees)
        sliceNode.SetAttribute("SlicerAstro.Beam", "on")
        sliceNode.Modified()
        self.waitForRenders(view)
        self.assertBeam(view, volumeNode, "beam")
        sliceNode.SetXYZOrigin(20., -10., 0.)
        self.waitForRenders(view)
        self.assertBeam(view, volumeNode, "beam after pan")

        # one render per pan step (updating the axes does not render the view again)
        self.waitForRenders(view)
        for step in range(3):
            sliceNode.SetXYZOrigin(step * 5., 2., 0.)
            self.assertEqual(self.waitForRenders(view, 0.3), 1, f"renders of pan step {step}")

        self.delayDisplay("Test passed")

    def assertBeam(self, view, volumeNode, message):
        displayableManager = view.displayableManagerByClassName("vtkMRMLAstroBeamDisplayableManager")
        renderer = displayableManager.vtkMarkerRenderer()
        beam = None
        actors = renderer.GetActors2D()
        actors.InitTraversal()
        for _ in range(actors.GetNumberOfItems()):
            actor = actors.GetNextActor2D()
            if actor.GetVisibility() and actor.GetMapper() and actor.GetMapper().GetInput():
                beam = actor.GetMapper().GetInput()
        self.assertIsNotNone(beam, message)
        self.assertGreater(beam.GetNumberOfPoints(), 30, message)
        sliceNode = view.mrmlSliceNode()
        world = np.array([self.worldAt(sliceNode, volumeNode, *beam.GetPoint(i)[:2])[:2]
                          for i in range(beam.GetNumberOfPoints() - 1)])  # last point = first
        center = world.mean(axis=0)
        east = (world[:, 0] - center[0]) * math.cos(math.radians(center[1]))
        north = world[:, 1] - center[1]
        distance = np.hypot(east, north)
        bmaj = float(volumeNode.GetAttribute("SlicerAstro.BMAJ"))
        bmin = float(volumeNode.GetAttribute("SlicerAstro.BMIN"))
        bpa = float(volumeNode.GetAttribute("SlicerAstro.BPA"))
        self.assertAlmostEqual(distance.max() / (bmaj / 2.), 1., delta=0.01, msg=message + ": major axis")
        self.assertAlmostEqual(distance.min() / (bmin / 2.), 1., delta=0.01, msg=message + ": minor axis")
        major = np.argmax(distance)
        positionAngle = math.degrees(math.atan2(east[major], north[major])) % 180.
        self.assertAlmostEqual(positionAngle, bpa % 180., delta=1., msg=message + ": position angle")

