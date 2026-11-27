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
        self.setUp()
        self.test_Beam3D()

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
        # a beam entered by the user (Info section): the button enabled (the
        # volume observed, the view not changed)
        self.assertTrue(slicer.vtkSlicerAstroVolumeLogic.SetBeam(volumeNode, [0.012, 0.006, 30.]))
        slicer.app.processEvents()
        self.assertTrue(beamButton("Red").enabled)
        self.assertFalse(beamButton("Yellow").enabled)  # not the sky plane
        # in the bar of the controller (not in its "more" options): shown in
        # the sky plane, hidden in the position-velocity views
        self.assertFalse(beamButton("Red").isHidden())
        self.assertTrue(beamButton("Yellow").isHidden())
        moreButton = slicer.util.findChild(widgets["Red"], "MoreButton")
        self.assertFalse(moreButton.checked)
        self.assertTrue(beamButton("Red").isVisibleTo(widgets["Red"]))

        # Mouse interactions: scroll (slice offset), right drag (zoom), middle drag (pan)
        for name in ("Red", "Yellow"):
            widget = widgets[name]
            view = widget.sliceView()
            sliceNode = widget.mrmlSliceNode()
            interactor = view.interactor()
            # the view laid out and rendered (with other tests running on the
            # same display the layout can be late): events at its centre
            self.waitForView(view)
            x, y = view.width // 2, view.height // 2

            offset = sliceNode.GetSliceOffset()
            for _ in range(20):
                interactor.SetEventInformation(x, y, 0, 0, chr(0), 0, None)
                interactor.MouseMoveEvent()
                interactor.MouseWheelForwardEvent()
                slicer.app.processEvents()
                if abs(sliceNode.GetSliceOffset() - offset) > 1e-7:
                    break
                self.waitForView(view)
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

    def waitForView(self, view, timeout=10.0):
        """Wait until a slice view has a size and has been rendered."""
        import time

        end = time.time() + timeout
        while (view.width < 10 or view.height < 10) and time.time() < end:
            slicer.app.processEvents()
            time.sleep(0.05)
        self.assertGreater(view.width, 10)
        view.forceRender()
        slicer.app.processEvents()

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
        """Markups measure with the WCS of the active volume (not millimetres or
        angles in the slice plane): lines the separation and the velocity
        difference, curves the length on the sky, closed curves the solid
        angle, angles the angle on the sky; also after saving and loading the
        scene."""
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
        self.assertTrue(measurement.IsA("vtkMRMLAstroMeasurement"))
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

        # open curve: length on the sky along the curve (2 points: the
        # separation), velocity difference of its ends
        curve = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsCurveNode", "C")
        curve.AddControlPointWorld(vtk.vtkVector3d(*ras(start)))
        curve.AddControlPointWorld(vtk.vtkVector3d(*ras(end)))
        self.assertAlmostEqual(curve.GetMeasurement("sky length").GetValue() / separation(world(start), world(end)), 1.,
                               delta=1.e-6)
        self.assertAlmostEqual(curve.GetMeasurement("velocity difference").GetValue(), world(end)[2] - world(start)[2],
                               delta=1.e-6)
        self.assertFalse(curve.GetMeasurement("length").GetEnabled())

        # closed curve (square of 40 pixels in one channel): perimeter and solid angle
        pixel = 0.000833333
        closed = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsClosedCurveNode", "CC")
        closed.SetCurveTypeToLinear()
        for corner in ((60., 40., 2.), (100., 40., 2.), (100., 80., 2.), (60., 80., 2.)):
            closed.AddControlPointWorld(vtk.vtkVector3d(*ras(corner)))
        self.assertAlmostEqual(closed.GetMeasurement("sky length").GetValue() / (4. * 40. * pixel), 1., delta=2.e-3)
        area = closed.GetMeasurement("sky area")
        self.assertAlmostEqual(area.GetValue() / (40. * pixel) ** 2, 1., delta=2.e-3)
        self.assertIn("arcmin", area.GetUnits())
        self.assertFalse(closed.GetMeasurement("area").GetEnabled())

        # angle: right angle in pixels on the sky (conformal projection near the
        # centre); points in different channels: not defined
        angle = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLMarkupsAngleNode", "A")
        for point in ((100., 60., 3.), (80., 60., 3.), (80., 85., 3.)):
            angle.AddControlPointWorld(vtk.vtkVector3d(*ras(point)))
        self.assertAlmostEqual(angle.GetMeasurement("sky angle").GetValue(), 90., delta=0.05)
        self.assertFalse(angle.GetMeasurement("angle").GetEnabled())
        angle.SetNthControlPointPositionWorld(2, ras((80., 85., 6.)))
        self.assertFalse(angle.GetMeasurement("sky angle").GetValueDefined())

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
        self.assertTrue(measurement.IsA("vtkMRMLAstroMeasurement"))
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
        # (the view does not render meanwhile: under load, CTK renders at once
        # when enough time passed since the last render)
        ticks, _ = self.axesActors(view)
        ticksTime = ticks.GetMTime()
        view.renderEnabled = False
        for step in range(5):
            sliceNode.SetXYZOrigin(step * 3., 0., 0.)
        self.assertEqual(ticks.GetMTime(), ticksTime, "no update without render")
        view.renderEnabled = True
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
        # in the bottom-left corner of the visible data, inside the labels of
        # the 2D axes, also when zoomed in (the corner of the data out of the
        # view)
        self.assertBeamInCorner(view, "beam after pan")
        fieldOfView = sliceNode.GetFieldOfView()
        sliceNode.SetFieldOfView(fieldOfView[0] / 4., fieldOfView[1] / 4., fieldOfView[2])
        self.waitForRenders(view)
        self.assertBeam(view, volumeNode, "beam zoomed in")
        self.assertBeamInCorner(view, "beam zoomed in")
        sliceNode.SetFieldOfView(*fieldOfView)
        self.waitForRenders(view)

        # one render per pan step (updating the axes does not render the view again)
        self.waitForRenders(view)
        for step in range(3):
            sliceNode.SetXYZOrigin(step * 5., 2., 0.)
            self.assertEqual(self.waitForRenders(view, 0.3), 1, f"renders of pan step {step}")

        self.delayDisplay("Test passed")

    def test_Beam3D(self):
        """3D resolution element: an ellipsoid with the beam (FWHM) on the sky
        and the spectral FWHM along the spectral axis, in the corner of the
        first pixel and channel of the cube rendered in the view; shown with
        the button of the 3D view controller."""
        volumeNode = slicer.util.loadVolume(self.writeRotatedCube())
        dimensions = volumeNode.GetImageData().GetDimensions()
        layoutManager = slicer.app.layoutManager()
        layoutManager.setLayout(slicer.vtkMRMLLayoutNode.SlicerLayoutOneUp3DView)
        slicer.app.processEvents()
        # the 3D view of the layout (after a scene clear, widgets of former
        # view nodes can remain, hidden)
        widget = next(layoutManager.threeDWidget(index) for index in range(layoutManager.threeDViewCount)
                      if layoutManager.threeDWidget(index).isVisible())
        viewNode = widget.mrmlViewNode()
        view = widget.threeDView()
        self.waitForView(view)
        logic = slicer.modules.volumerendering.logic()
        displayNode = logic.GetFirstVolumeRenderingDisplayNode(volumeNode) or \
            logic.CreateDefaultVolumeRenderingNodes(volumeNode)
        displayNode.SetVisibility(True)
        self.assertEqual(slicer.vtkMRMLAstroVolumeNode.GetBeam3DUnavailableReason(viewNode), "")

        # the toggle in the bar of the 3D view controller: enabled while a
        # cube with a beam is rendered in the view
        button = slicer.util.findChild(widget.threeDController(), "Beam3DButton")
        self.assertTrue(button.enabled)
        self.assertFalse(button.checked)
        beam = [float(volumeNode.GetAttribute(f"SlicerAstro.{key}")) for key in ("BMAJ", "BMIN", "BPA")]
        volumeNode.SetAttribute("SlicerAstro.BMAJ", "UNDEFINED")
        slicer.app.processEvents()
        self.assertFalse(button.enabled)
        self.assertIn("BMAJ", button.toolTip)
        # a beam entered by the user (Info section)
        self.assertTrue(slicer.vtkSlicerAstroVolumeLogic.SetBeam(volumeNode, beam))
        slicer.app.processEvents()
        self.assertTrue(button.enabled)
        button.click()
        self.assertEqual(viewNode.GetAttribute("SlicerAstro.Beam3D"), "on")
        displayableManager = view.displayableManagerByClassName("vtkMRMLAstroBeam3DDisplayableManager")

        def ellipsoidIJK():
            # rendered (the ellipsoid is updated before a render) until it is
            # there: the first renders of a new layout can be skipped
            import time
            end = time.time() + 5.
            while True:
                view.forceRender()
                slicer.app.processEvents()
                if displayableManager.GetEllipsoidActor().GetVisibility() or time.time() > end:
                    break
                time.sleep(0.05)
            data = displayableManager.GetEllipsoidPolyData()
            rasToIJK = vtk.vtkMatrix4x4()
            volumeNode.GetRASToIJKMatrix(rasToIJK)
            return np.array([rasToIJK.MultiplyPoint(list(data.GetPoint(i)) + [1.])[:3]
                             for i in range(data.GetNumberOfPoints())])

        points = ellipsoidIJK()
        self.assertTrue(displayableManager.GetEllipsoidActor().GetVisibility())
        # the label "beam" next to it
        self.assertEqual(displayableManager.GetLabelActor().GetInput(), "beam")
        self.assertTrue(displayableManager.GetLabelActor().GetVisibility())
        self.assertGreater(len(points), 100)
        # one channel deep (spectral FWHM of top-hat channels), inside the cube
        # near its first pixel and channel
        self.assertAlmostEqual(points[:, 2].max() - points[:, 2].min(), 1., places=3)
        self.assertTrue(np.all(points.min(axis=0) > -0.5))
        self.assertTrue(np.all(points.max(axis=0) < np.array(dimensions) * 0.5))
        # the equator: the beam on the sky (axes BMAJ and BMIN, position angle BPA)
        center = points.mean(axis=0)
        equator = points[np.abs(points[:, 2] - center[2]) < 1e-6]
        self.assertGreater(len(equator), 30)
        world = []
        for ijk in equator:
            coordinates = [0., 0., 0.]
            self.assertTrue(volumeNode.GetAstroVolumeDisplayNode().GetReferenceSpace(list(ijk), coordinates))
            world.append(coordinates[:2])
        self.assertEllipseOnSky(np.array(world), volumeNode, "3D beam")

        # the spectral FWHM (e.g. written by AstroSmoothing)
        volumeNode.SetAttribute("SlicerAstro.SpectralFWHM", "2.5")
        points = ellipsoidIJK()
        self.assertAlmostEqual(points[:, 2].max() - points[:, 2].min(), 2.5, places=3)

        # hidden: button off, or the cube not rendered in the view
        button.click()
        view.forceRender()
        self.assertFalse(displayableManager.GetEllipsoidActor().GetVisibility())
        self.assertFalse(displayableManager.GetLabelActor().GetVisibility())
        button.click()
        displayNode.SetVisibility(False)
        slicer.app.processEvents()
        view.forceRender()
        self.assertFalse(displayableManager.GetEllipsoidActor().GetVisibility())
        self.assertNotEqual(slicer.vtkMRMLAstroVolumeNode.GetBeam3DUnavailableReason(viewNode), "")
        self.assertFalse(button.enabled)
        self.assertTrue(button.checked)  # the choice kept for when a cube is rendered again
        displayNode.SetVisibility(True)
        slicer.app.processEvents()
        self.assertTrue(button.enabled)
        # a cube removed from the scene
        slicer.mrmlScene.RemoveNode(volumeNode)
        slicer.app.processEvents()
        self.assertFalse(button.enabled)
        self.delayDisplay("Test passed")

    def beamPolyData(self, view):
        displayableManager = view.displayableManagerByClassName("vtkMRMLAstroBeamDisplayableManager")
        renderer = displayableManager.vtkMarkerRenderer()
        actors = renderer.GetActors2D()
        actors.InitTraversal()
        for _ in range(actors.GetNumberOfItems()):
            actor = actors.GetNextActor2D()
            data = actor.GetMapper().GetInput() if actor.GetVisibility() and actor.GetMapper() else None
            if data is not None and data.GetNumberOfLines() > 0:
                return data
        return None

    def assertBeamInCorner(self, view, message):
        """The beam in the bottom-left corner of the visible part of the data
        (view coordinates), inside the labels of the 2D axes."""
        sliceNode = view.mrmlSliceNode()
        sliceLogic = slicer.app.applicationLogic().GetSliceLogic(sliceNode)
        layer = sliceLogic.GetBackgroundLayer()
        dimensions = layer.GetVolumeNode().GetImageData().GetDimensions()
        ijkToXY = layer.GetXYToIJKTransform().GetInverse()
        channel = layer.GetXYToIJKTransform().TransformPoint(
            sliceNode.GetDimensions()[0] / 2., sliceNode.GetDimensions()[1] / 2., 0.)[2]
        corners = [ijkToXY.TransformPoint(i, j, channel) for i in (-0.5, dimensions[0] - 0.5)
                   for j in (-0.5, dimensions[1] - 0.5)]
        axes = sliceNode.GetRulerType() != slicer.vtkMRMLAbstractViewNode.RulerTypeNone
        left = max(min(c[0] for c in corners), 80. if axes else 0.)
        bottom = max(min(c[1] for c in corners), 34. if axes else 0.)
        beam = self.beamPolyData(view)
        self.assertIsNotNone(beam, message)
        bounds = beam.GetBounds()
        self.assertAlmostEqual(bounds[0], left + 10., delta=1., msg=message + ": left")
        self.assertAlmostEqual(bounds[2], bottom + 10., delta=1., msg=message + ": bottom")
        self.assertLessEqual(bounds[1], sliceNode.GetDimensions()[0], message)
        self.assertLessEqual(bounds[3], sliceNode.GetDimensions()[1], message)
        # the label "beam" right of the ellipse, at its height
        displayableManager = view.displayableManagerByClassName("vtkMRMLAstroBeamDisplayableManager")
        props = displayableManager.vtkMarkerRenderer().GetViewProps()
        labels = [props.GetItemAsObject(i) for i in range(props.GetNumberOfItems())
                  if props.GetItemAsObject(i).IsA("vtkTextActor")]
        self.assertEqual([label.GetInput() for label in labels], ["beam"], message)
        x, y = labels[0].GetPosition()
        self.assertAlmostEqual(x, bounds[1] + 6., delta=1., msg=message + ": label")
        self.assertAlmostEqual(y, (bounds[2] + bounds[3]) / 2., delta=1., msg=message + ": label")

    def assertBeam(self, view, volumeNode, message):
        beam = self.beamPolyData(view)  # the outline
        self.assertIsNotNone(beam, message)
        self.assertGreater(beam.GetNumberOfPoints(), 30, message)
        sliceNode = view.mrmlSliceNode()
        world = np.array([self.worldAt(sliceNode, volumeNode, *beam.GetPoint(i)[:2])[:2]
                          for i in range(beam.GetNumberOfPoints() - 1)])  # last point = first
        self.assertEllipseOnSky(world, volumeNode, message)

    def assertEllipseOnSky(self, world, volumeNode, message):
        """Points (longitude, latitude in degrees) on the FWHM ellipse of the
        beam of the volume: axes BMAJ and BMIN, position angle BPA."""
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

