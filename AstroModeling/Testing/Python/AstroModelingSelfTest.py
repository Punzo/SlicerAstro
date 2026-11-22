import time

import numpy as np
import qt

import slicer
import vtk
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroModelingSelfTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Modeling SelfTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroModeling"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
Self test of the AstroModeling module: 3DBarolo estimates the initial
parameters of the HALOGAS-DR1 HI cube of NGC 2403, compared with the values of
Fraternali et al. (2002); models, fits, cancel, masks, beams, results (rings,
contours, PV slices with the rotation curve) and scenes.
"""
        self.parent.hidden = True


class AstroModelingSelfTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroModelingSelfTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of an (astro) volume node."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


class AstroModelingSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        for test in (self.test_EstimateInitialParameters, self.test_CreateFitAndUpdate,
                     self.test_CancelFit, self.test_DoublePrecision, self.test_Mask, self.test_BeamStatus,
                     self.test_PVSlicesAfterSceneReload):
            self.setUp()
            test()

    #
    # helpers
    #

    def openModule(self):
        """NGC 2403 in the AstroModeling module."""
        import AstroSampleData
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("NGC2403")
        self.assertIsNotNone(volumeNode)
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroModeling")
        widget = slicer.modules.astromodeling.widgetRepresentation()
        parametersNode = widget.mrmlAstroModelingParametersNode()
        self.assertEqual(parametersNode.GetInputVolumeNodeID(), volumeNode.GetID())
        return volumeNode, widget, parametersNode

    def runOperation(self, start, cancelAfter=None, timeout=900):
        """Start a 3DBarolo operation of the panel (in the background) and
        wait for its end; returns the seconds it took."""
        logic = slicer.modules.astromodeling.logic()
        begin = time.time()
        start()
        self.assertTrue(logic.IsOperationRunning(), "the operation did not start: " + logic.GetLastError())
        cancelled = False
        while logic.IsOperationRunning() and time.time() < begin + timeout:
            if cancelAfter is not None and not cancelled and time.time() > begin + cancelAfter:
                slicer.modules.astromodeling.widgetRepresentation().cancelOperation()
                cancelled = True
            slicer.app.processEvents()
            time.sleep(0.05)
        self.assertFalse(logic.IsOperationRunning(), "the operation did not end")
        for _ in range(10):
            slicer.app.processEvents()
            time.sleep(0.02)
        return time.time() - begin

    def ringColumn(self, parametersNode, name):
        table = parametersNode.GetParamsTableNode().GetTable()
        column = table.GetColumnByName(name)
        self.assertIsNotNone(column, name)
        return np.array([column.GetValue(row) for row in range(table.GetNumberOfRows())])

    def estimate(self, widget, parametersNode):
        parametersNode.SetSystemicVelocity(-1.)
        self.runOperation(widget.estimate)
        self.assertEqual(slicer.modules.astromodeling.logic().GetLastOperationResult(), 1)
        self.assertLess(abs(parametersNode.GetSystemicVelocity() - 133.), 3.)
        # the estimated parameters are shown to be edited
        self.assertEqual(parametersNode.GetMode(), "Manual")

    def manualRings(self, parametersNode, rings=4):
        """Initial rings of NGC 2403 (Fraternali et al. 2002), geometry fixed."""
        parametersNode.SetMode("Manual")
        for name, value in (("NumberOfRings", rings), ("RadSep", 120.), ("XCenter", 163.4), ("YCenter", 117.4),
                            ("SystemicVelocity", 133.), ("RotationVelocity", 120.), ("VelocityDispersion", 10.),
                            ("Inclination", 62.), ("PositionAngle", 124.), ("ScaleHeight", 0.),
                            ("ColumnDensity", 1.), ("Distance", 3.2)):
            getattr(parametersNode, "Set" + name)(value)
        parametersNode.SetInclinationFit(False)
        parametersNode.SetPositionAngleFit(False)

    def markerPositions(self, major):
        points = slicer.modules.astromodeling.logic().GetRingMarkers(major)
        self.assertIsNotNone(points)
        positions = []
        for index in range(points.GetNumberOfControlPoints()):
            position = [0.0, 0.0, 0.0]
            points.GetNthControlPointPosition(index, position)
            positions.append(position + [float(points.GetNthControlPointVisibility(index))])
        return np.array(positions)

    #
    # tests
    #

    def test_CreateFitAndUpdate(self):
        """Model from the estimated parameters, a short fit of the rotation
        and dispersion, the results (rings plot and table, PV slices with the
        rotation curve, contours), and a model recomputed from an edited ring
        table."""
        volumeNode, widget, parametersNode = self.openModule()
        beam = [volumeNode.GetAttribute("SlicerAstro." + key) for key in ("BMAJ", "BMIN", "BPA")]
        self.estimate(widget, parametersNode)
        # the beam of the header keeps its precision (the fields show 2
        # decimals): BMAJ = 29.6644" for NGC 2403, not 29.66"
        self.assertEqual([volumeNode.GetAttribute("SlicerAstro." + key) for key in ("BMAJ", "BMIN", "BPA")], beam)

        # few rings and two free parameters: a short fit (Manual: the values
        # of the parameters node, not a new estimate)
        parametersNode.SetNumberOfRings(4)
        parametersNode.SetRadSep(120.)
        for name in ("Inclination", "PositionAngle", "SystemicVelocity", "XCenter", "YCenter",
                     "RadialVelocity", "ScaleHeight"):
            getattr(parametersNode, "Set" + name + "Fit")(False)
        parametersNode.SetRotationVelocityFit(True)
        parametersNode.SetVelocityDispersionFit(True)
        parametersNode.SetTwoStage(False)
        # the free toggles of the panel follow the parameters
        self.assertTrue(slicer.util.findChild(widget, "RotationVelocityFree").checked)
        self.assertFalse(slicer.util.findChild(widget, "InclinationFree").checked)

        initialNames = ("XCenter", "YCenter", "SystemicVelocity", "RotationVelocity", "VelocityDispersion",
                        "Inclination", "PositionAngle", "ColumnDensity")
        initialValues = [getattr(parametersNode, "Get" + name)() for name in initialNames]

        # free geometry: 3DBarolo refines the initial rings with a 2D
        # tilted-ring fit (the rings can differ from the values of the user)
        parametersNode.SetInclinationFit(True)
        parametersNode.SetPositionAngleFit(True)
        self.runOperation(widget.createModel)
        # Manual mode: Create keeps the values of the user (the next fit
        # starts from them, as upstream 3DBarolo with the same parameters)
        self.assertEqual([getattr(parametersNode, "Get" + name)() for name in initialNames], initialValues)
        parametersNode.SetInclinationFit(False)
        parametersNode.SetPositionAngleFit(False)
        self.runOperation(widget.createModel)
        modelNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        self.assertIsNotNone(modelNode)
        self.assertEqual(modelNode.GetAttribute("SlicerAstro.DATAMODEL"), "MODEL")
        model = voxelArray(modelNode)
        self.assertEqual(model.shape, voxelArray(volumeNode).shape)
        self.assertGreater(np.nansum(model), 0.)
        # one model and one residual cube: the previous ones are replaced
        models = [node for node in slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode")
                  if "_model_" in node.GetName()]
        self.assertEqual(len(models), 1)

        self.runOperation(widget.fit)
        self.assertTrue(parametersNode.GetFitSuccess())
        rotation = self.ringColumn(parametersNode, "VRot")
        dispersion = self.ringColumn(parametersNode, "VDisp")
        self.assertEqual(len(rotation), 4)
        # NGC 2403 (Fraternali et al. 2002): flat rotation curve at about
        # 130 km/s outside the inner arcminutes, dispersion about 10 km/s
        self.assertTrue(np.all(rotation > 40.) and np.all(rotation < 180.), rotation)
        self.assertGreater(rotation[-1], rotation[0])
        self.assertTrue(np.all(dispersion > 2.) and np.all(dispersion < 40.), dispersion)
        # only the model cube (the data in the region of the model: Masking)
        self.assertIsNone(parametersNode.GetResidualVolumeNodeID())
        self.assertEqual(len([node for node in slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode")
                              if "_maskedByModel_" in node.GetName()]), 0)

        # results: the panel shows them
        results = slicer.util.findChild(widget, "Results")
        self.assertTrue(results.visible)
        self.assertIn("3DBarolo", widget.outputLog())
        # the rings plot: the selected parameter of the table
        self.assertEqual(widget.ringParameter(), "VRot")
        np.testing.assert_allclose(np.array(widget.ringPlot().values()), rotation)
        widget.setRingParameter("VDisp")
        np.testing.assert_allclose(np.array(widget.ringPlot().values()), dispersion)
        widget.openPlotView()
        chart = slicer.util.getFirstNodeByClassByName("vtkMRMLPlotChartNode", "ModelingRingsChart")
        self.assertIsNotNone(chart)
        self.assertEqual(chart.GetXAxisTitle(), "Radius (arcsec)")
        self.assertTrue(chart.GetYAxisTitle().startswith("Dispersion velocity"), chart.GetYAxisTitle())
        # the points of the plot view move vertically only (set once the
        # layout has made the plot view)
        for _ in range(20):
            slicer.app.processEvents()
        for plotView in slicer.util.getNodesByClass("vtkMRMLPlotViewNode"):
            self.assertEqual(plotView.GetInteractionMode(), plotView.InteractionModeMovePoints)
            self.assertFalse(plotView.GetEnablePointMoveAlongX())
        widget.setRingParameter("VRot")
        # a point of the panel plot dragged: the value of its ring in the table
        plot = widget.ringPlot()
        plot.show()
        slicer.app.processEvents()
        point = plot.pointPosition(1)
        target = qt.QPointF(point.x(), point.y() - 20.)
        expected = plot.valueAt(target.y())
        for eventType, position, buttons in ((qt.QEvent.MouseButtonPress, point, qt.Qt.LeftButton),
                                             (qt.QEvent.MouseMove, target, qt.Qt.LeftButton),
                                             (qt.QEvent.MouseButtonRelease, target, qt.Qt.NoButton)):
            button = qt.Qt.LeftButton if eventType != qt.QEvent.MouseMove else qt.Qt.NoButton
            event = qt.QMouseEvent(eventType, position, position, button, buttons,
                                   qt.Qt.NoModifier)
            slicer.app.sendEvent(plot, event)
        slicer.app.processEvents()
        self.assertAlmostEqual(self.ringColumn(parametersNode, "VRot")[1], expected, places=6)
        self.assertNotAlmostEqual(expected, rotation[1], places=2)
        column = parametersNode.GetParamsTableNode().GetTable().GetColumnByName("VRot")
        column.SetValue(1, rotation[1])
        parametersNode.GetParamsTableNode().Modified()
        slicer.app.processEvents()
        # the contours of the results: data, model, region of the model
        contours = slicer.util.getFirstNodeByName("AstroModelingContoursSeg")
        self.assertEqual(contours.GetSegmentation().GetNumberOfSegments(), 3)
        modelContours = slicer.util.findChild(widget, "ModelContoursButton")
        self.assertTrue(modelContours.checked)
        modelContours.click()
        self.assertFalse(contours.GetDisplayNode().GetSegmentVisibility("ModelingModelContours"))
        modelContours.click()
        # the rotation curve shown or hidden
        markersButton = slicer.util.findChild(widget, "RingMarkersButton")
        self.assertTrue(markersButton.checked)
        markersButton.click()
        majorPoints = slicer.util.getFirstNodeByName("MarkupsFiducialsMajor")
        self.assertFalse(majorPoints.GetDisplayNode().GetVisibility())
        markersButton.click()
        self.assertTrue(majorPoints.GetDisplayNode().GetVisibility())
        # leaving the module hides the results, entering shows them again
        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        slicer.app.processEvents()
        self.assertFalse(contours.GetDisplayNode().GetVisibility())
        self.assertFalse(majorPoints.GetDisplayNode().GetVisibility())
        mainWindow.moduleSelector().selectModule("AstroModeling")
        slicer.app.processEvents()
        self.assertTrue(contours.GetDisplayNode().GetVisibility())
        self.assertTrue(majorPoints.GetDisplayNode().GetVisibility())
        self.assertTrue(modelContours.checked)
        # Show in views: the cube in the 2D views, XY and the PV slices
        for name in ("Red", "Yellow", "Green"):
            slicer.mrmlScene.GetNodeByID(f"vtkMRMLSliceCompositeNode{name}").SetBackgroundVolumeID(None)
        slicer.util.findChild(widget, "ShowResultsButton").click()
        slicer.app.processEvents()
        for name, orientation in (("Red", "XY"), ("Yellow", "PVMajor"), ("Green", "PVMinor")):
            self.assertEqual(slicer.mrmlScene.GetNodeByID(f"vtkMRMLSliceCompositeNode{name}").GetBackgroundVolumeID(),
                             volumeNode.GetID(), name)
            self.assertEqual(slicer.mrmlScene.GetNodeByID(f"vtkMRMLSliceNode{name}").GetOrientation(), orientation)
        # the PV slices along the axes, with two points per ring crossing them
        yellow = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow")
        self.assertEqual(yellow.GetOrientation(), "PVMajor")
        self.assertEqual(slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeGreen").GetOrientation(), "PVMinor")
        major = self.markerPositions(True)
        self.assertEqual(len(major), 8)
        self.assertEqual(int(major[:, 3].sum()), 8)
        # absolute orientation: a rotation of the major axis slice and back
        before = [yellow.GetSliceToRAS().GetElement(i, j) for i in range(4) for j in range(4)]
        majorOffset = slicer.util.findChild(widget, "MajorOffsetField")
        majorOffset.value = 20.
        majorOffset.editingFinished()
        self.assertEqual(parametersNode.GetYellowRotValue(), 20.)
        rotated = [yellow.GetSliceToRAS().GetElement(i, j) for i in range(4) for j in range(4)]
        self.assertGreater(np.max(np.abs(np.array(rotated) - np.array(before))), 0.1)
        majorOffset.value = 0.
        majorOffset.editingFinished()
        np.testing.assert_allclose([yellow.GetSliceToRAS().GetElement(i, j) for i in range(4) for j in range(4)],
                                   before, atol=1e-9)
        np.testing.assert_allclose(self.markerPositions(True), major, atol=1e-6)
        # the table as CSV
        import csv
        import os
        path = os.path.join(slicer.app.temporaryPath, "NGC2403_rings.csv")
        self.assertTrue(widget.exportTable(path))
        with open(path) as stream:
            rows = list(csv.reader(stream))
        self.assertEqual(len(rows), 5)
        self.assertTrue(rows[0][1].startswith("VRot"))
        self.assertAlmostEqual(float(rows[1][1]), rotation[0], places=4)

        # the model recomputed from the rings of the table (not edited) is the
        # fitted model: same rings, same units (e.g. densities in 1E20 cm^-2)
        fitted = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())).copy()
        self.runOperation(widget.updateModelFromTable)
        recomputed = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID()))
        peak = np.nanmax(np.abs(fitted))
        self.assertLess(np.nanmax(np.abs(np.nan_to_num(recomputed) - np.nan_to_num(fitted))), 1e-3 * peak)
        np.testing.assert_allclose(self.ringColumn(parametersNode, "Dens"), 1., rtol=1e-6)

        # edit the rings and recompute the model from the table
        before = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())).copy()
        column = parametersNode.GetParamsTableNode().GetTable().GetColumnByName("VRot")
        for row in range(column.GetNumberOfTuples()):
            column.SetValue(row, column.GetValue(row) * 0.5)
        parametersNode.GetParamsTableNode().GetTable().Modified()
        parametersNode.GetParamsTableNode().Modified()
        self.runOperation(widget.updateModelFromTable)
        after = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID()))
        self.assertEqual(after.shape, before.shape)
        self.assertFalse(np.allclose(np.nan_to_num(after), np.nan_to_num(before)),
                         "the model did not change with the rotation velocities")
        # the plot follows the edited table
        np.testing.assert_allclose(np.array(widget.ringPlot().values()), rotation * 0.5)

    def test_CancelFit(self):
        """A fit is cancelled within seconds and leaves the module usable."""
        volumeNode, widget, parametersNode = self.openModule()
        self.manualRings(parametersNode, 10)
        parametersNode.SetTwoStage(True)
        seconds = self.runOperation(widget.fit, cancelAfter=3.)
        self.assertEqual(slicer.modules.astromodeling.logic().GetLastOperationResult(), -1)
        self.assertFalse(parametersNode.GetFitSuccess())
        # no model left behind
        self.assertIsNone(parametersNode.GetOutputVolumeNodeID())
        print("fit cancelled after %.1f s" % seconds)
        # the module runs again after a cancel
        parametersNode.SetNumberOfRings(2)
        self.runOperation(widget.createModel)
        self.assertEqual(slicer.modules.astromodeling.logic().GetLastOperationResult(), 1)

    def test_DoublePrecision(self):
        """64-bit cube (BITPIX -64): model and model from the table; the model
        equals the one of the 32-bit cube."""
        from AstroVolumeLogicTest import doublePrecisionCopy

        floatNode, widget, parametersNode = self.openModule()
        volumeNode = doublePrecisionCopy(floatNode, "NGC2403_double")
        widget.setInputVolume(volumeNode)
        self.manualRings(parametersNode)
        self.runOperation(widget.createModel)
        modelNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        self.assertIsNotNone(modelNode)
        self.assertEqual(modelNode.GetImageData().GetScalarType(), vtk.VTK_DOUBLE)
        doubleModel = voxelArray(modelNode).astype(np.float64).copy()
        peak = np.nanmax(np.abs(doubleModel))
        self.assertGreater(peak, 0.)

        # model from the (not edited) ring table
        self.runOperation(widget.updateModelFromTable)
        recomputed = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID()))
        self.assertLess(np.nanmax(np.abs(np.nan_to_num(recomputed) - np.nan_to_num(doubleModel))), 1e-3 * peak)

        # same parameters on the 32-bit cube
        widget.setInputVolume(floatNode)
        self.runOperation(widget.createModel)
        floatModel = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID()))
        self.assertEqual(floatModel.shape, doubleModel.shape)
        self.assertLess(np.nanmax(np.abs(np.nan_to_num(floatModel) - np.nan_to_num(doubleModel))), 1e-3 * peak)

    def test_Mask(self):
        """Estimate, model and model from the table with an input mask: the
        segment of the mask of the cube is read directly (no label map)."""
        import scipy.ndimage

        volumeNode, widget, parametersNode = self.openModule()
        cube = voxelArray(volumeNode)
        with np.errstate(invalid="ignore"):
            emission = cube > 3. * volumeNode.GetDisplayThreshold()
        galaxy = scipy.ndimage.binary_dilation(emission, iterations=2)
        # the mask of the data (one per dataset): the galaxy segment; the
        # panel uses it without a selection (its first segment)
        segmentationNode = slicer.modules.astrovolume.logic().GetMaskSegmentationForVolume(volumeNode, True)
        self.assertIsNotNone(segmentationNode)
        galaxyID = segmentationNode.GetSegmentation().AddEmptySegment("Galaxy", "Galaxy")
        slicer.util.updateSegmentBinaryLabelmapFromArray(galaxy.astype(np.uint8), segmentationNode, galaxyID,
                                                         volumeNode)

        slicer.util.findChild(widget, "RegionButton").setCurrentIndex(1)
        self.assertTrue(parametersNode.GetMaskActive())
        self.assertEqual(parametersNode.GetMaskSegmentationNodeID(), segmentationNode.GetID())
        self.assertEqual(parametersNode.GetMaskSegmentID(), galaxyID)
        slicer.app.processEvents()
        self.assertFalse(slicer.util.findChild(widget, "SearchGroup").visible)

        self.estimate(widget, parametersNode)
        self.assertEqual(parametersNode.GetMaskSegmentID(), galaxyID)

        self.manualRings(parametersNode)
        self.runOperation(widget.createModel)
        modelNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        self.assertIsNotNone(modelNode)
        model = voxelArray(modelNode).astype(np.float64).copy()
        peak = np.nanmax(np.abs(model))
        self.assertGreater(peak, 0.)

        # model from the (not edited) ring table, with the same mask
        self.runOperation(widget.updateModelFromTable)
        recomputed = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID()))
        self.assertLess(np.nanmax(np.abs(np.nan_to_num(recomputed) - np.nan_to_num(model))), 1e-3 * peak)
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLLabelMapVolumeNode"), 0)
        slicer.util.findChild(widget, "RegionButton").setCurrentIndex(0)
        self.assertFalse(parametersNode.GetMaskActive())

    def test_BeamStatus(self):
        """The origin of the beam is shown: from the header, or estimated
        (the user must check it); the estimated beam is used and saved."""
        volumeNode, widget, parametersNode = self.openModule()
        status = slicer.util.findChild(widget, "BeamStatusLabel")
        self.assertIn("FITS header", status.text)
        # a copy without beam in the header: estimated, and said so
        copy = slicer.modules.astrovolume.logic().CloneAstroVolume(
            slicer.mrmlScene, volumeNode, None, "_nobeam", "NGC2403_nobeam")
        for key in ("BMAJ", "BMIN", "BPA"):
            copy.SetAttribute("SlicerAstro." + key, "UNDEFINED")
        widget.setInputVolume(copy)
        slicer.app.processEvents()
        self.assertIn("ESTIMATED", status.text)
        self.assertGreater(slicer.util.findChild(widget, "BeamMajorField").value, 0.)
        # a model of the copy: the estimated beam becomes the beam of the data
        self.manualRings(parametersNode, 2)
        self.runOperation(widget.createModel)
        self.assertGreater(float(copy.GetAttribute("SlicerAstro.BMAJ")), 0.)
        self.assertIn("estimated from the noise", widget.outputLog())

    def test_PVSlicesAfterSceneReload(self):
        """A scene with a model saved and loaded again: the same PV slices
        along the axes and the same points of the rotation curve."""
        import os

        volumeNode, widget, parametersNode = self.openModule()
        self.manualRings(parametersNode)
        self.runOperation(widget.createModel)
        major = self.markerPositions(True)
        minor = self.markerPositions(False)
        yellow = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow")
        orientation = [yellow.GetSliceToRAS().GetElement(i, j) for i in range(3) for j in range(4)]
        path = os.path.join(slicer.app.temporaryPath, "AstroModelingSelfTest.mrb")
        self.assertTrue(slicer.util.saveScene(path))
        # another module, the scene closed, then loaded again
        slicer.util.mainWindow().moduleSelector().selectModule("AstroVolume")
        slicer.mrmlScene.Clear(0)
        slicer.util.loadScene(path)
        slicer.util.mainWindow().moduleSelector().selectModule("AstroModeling")
        for _ in range(10):
            slicer.app.processEvents()
        parametersNode = widget.mrmlAstroModelingParametersNode()
        self.assertGreater(parametersNode.GetParamsTableNode().GetNumberOfRows(), 0)
        yellow = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow")
        self.assertEqual(yellow.GetOrientation(), "PVMajor")
        # (the centre is saved in single precision)
        np.testing.assert_allclose([yellow.GetSliceToRAS().GetElement(i, j) for i in range(3) for j in range(4)],
                                   orientation, atol=1e-4)
        np.testing.assert_allclose(self.markerPositions(True), major, atol=1e-4)
        np.testing.assert_allclose(self.markerPositions(False), minor, atol=1e-4)

    def test_EstimateInitialParameters(self):
        """The estimate of the initial parameters (fast, while the fit takes
        minutes)."""
        self.delayDisplay("Estimating the initial parameters of NGC 2403")
        volumeNode, widget, parametersNode = self.openModule()
        parametersNode.SetNumberOfRings(20)
        self.estimate(widget, parametersNode)
        # NGC 2403 (Fraternali et al. 2002): systemic velocity 133 km/s,
        # inclination 60-63 degrees, position angle 124 degrees
        self.assertGreater(parametersNode.GetNumberOfRings(), 0)
        self.assertGreater(parametersNode.GetInclination(), 40.)
        self.assertLess(parametersNode.GetInclination(), 80.)
        self.assertLess(abs(parametersNode.GetPositionAngle() - 124.), 20.)
        self.assertGreater(parametersNode.GetRotationVelocity(), 0.)
        # the estimate does not leave a model behind
        self.assertIsNone(parametersNode.GetOutputVolumeNodeID())
        self.delayDisplay("Test passed")
