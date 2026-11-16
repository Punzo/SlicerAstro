import time

import numpy as np

import slicer
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
Fraternali et al. (2002).
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
                     self.test_CancelFit):
            self.setUp()
            test()

    #
    # helpers
    #

    def openModule(self):
        """NGC 2403 in the AstroModeling module; parameters estimated by 3DBarolo."""
        import AstroSampleData
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("NGC2403")
        self.assertIsNotNone(volumeNode)
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroModeling")
        widget = slicer.modules.astromodeling.widgetRepresentation()
        parametersNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroModelingParametersNode")
        parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
        return volumeNode, widget, parametersNode

    def runOperation(self, parametersNode, start, cancelAfter=None, timeout=1500):
        """Run a 3DBarolo operation of the widget (worker thread) and wait
        for its end; returns the seconds it took. The widget sets the status
        to 1 when it starts the worker, to 100 when the worker ends and back
        to 0 (idle) once the results are applied, both within one event: the
        end is a return to 0 or 100 after the start."""
        parametersNode.SetStatus(0)
        begin = time.time()
        start()
        self.assertNotEqual(parametersNode.GetStatus(), 0, "the operation did not start")
        cancelled = False
        while parametersNode.GetStatus() not in (0, 100) and time.time() < begin + timeout:
            if cancelAfter is not None and not cancelled and time.time() > begin + cancelAfter:
                slicer.util.findChild(slicer.modules.astromodeling.widgetRepresentation(), "CancelPushButton").click()
                cancelled = True
            slicer.app.processEvents()
            time.sleep(0.05)
        self.assertIn(parametersNode.GetStatus(), (0, 100), "the operation did not end")
        for _ in range(10):
            slicer.app.processEvents()
            time.sleep(0.05)
        return time.time() - begin

    def ringColumn(self, parametersNode, name):
        table = parametersNode.GetParamsTableNode().GetTable()
        column = table.GetColumnByName(name)
        self.assertIsNotNone(column, name)
        return np.array([column.GetValue(row) for row in range(table.GetNumberOfRows())])

    def estimate(self, widget, parametersNode):
        parametersNode.SetSystemicVelocity(-1.)
        self.runOperation(parametersNode, widget.onEstimateInitialParameters)
        self.assertLess(abs(parametersNode.GetSystemicVelocity() - 133.), 3.)

    #
    # tests
    #

    def test_CreateFitAndUpdate(self):
        """Model from the estimated parameters, a short fit of the rotation
        and dispersion, and a model recomputed from an edited ring table."""
        volumeNode, widget, parametersNode = self.openModule()
        beam = [volumeNode.GetAttribute("SlicerAstro." + key) for key in ("BMAJ", "BMIN", "BPA")]
        self.estimate(widget, parametersNode)
        # the beam of the header keeps its precision (the spin boxes show 2
        # decimals): BMAJ = 29.6644" for NGC 2403, not 29.66"
        self.assertEqual([volumeNode.GetAttribute("SlicerAstro." + key) for key in ("BMAJ", "BMIN", "BPA")], beam)

        # few rings and two free parameters: a short fit (Manual: the values
        # of the parameters node, not a new estimate)
        parametersNode.SetMode("Manual")
        parametersNode.SetNumberOfRings(4)
        parametersNode.SetRadSep(120.)
        for name in ("Inclination", "PositionAngle", "SystemicVelocity", "XCenter", "YCenter",
                     "RadialVelocity", "ScaleHeight"):
            getattr(parametersNode, "Set" + name + "Fit")(False)
        parametersNode.SetRotationVelocityFit(True)
        parametersNode.SetVelocityDispersionFit(True)
        parametersNode.SetTwoStage(False)

        initialNames = ("XCenter", "YCenter", "SystemicVelocity", "RotationVelocity", "VelocityDispersion",
                        "Inclination", "PositionAngle", "ColumnDensity")
        initialValues = [getattr(parametersNode, "Get" + name)() for name in initialNames]

        # free geometry: 3DBarolo refines the initial rings with a 2D
        # tilted-ring fit (the rings can differ from the values of the user)
        parametersNode.SetInclinationFit(True)
        parametersNode.SetPositionAngleFit(True)
        self.runOperation(parametersNode, widget.onCreate)
        # Manual mode: Create keeps the values of the user (the next fit
        # starts from them, as upstream 3DBarolo with the same parameters)
        self.assertEqual([getattr(parametersNode, "Get" + name)() for name in initialNames], initialValues)
        parametersNode.SetInclinationFit(False)
        parametersNode.SetPositionAngleFit(False)
        self.runOperation(parametersNode, widget.onCreate)
        modelNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())
        self.assertIsNotNone(modelNode)
        model = voxelArray(modelNode)
        self.assertEqual(model.shape, voxelArray(volumeNode).shape)
        self.assertGreater(np.nansum(model), 0.)

        self.runOperation(parametersNode, widget.onFit)
        self.assertTrue(parametersNode.GetFitSuccess())
        rotation = self.ringColumn(parametersNode, "VRot")
        dispersion = self.ringColumn(parametersNode, "VDisp")
        self.assertEqual(len(rotation), 4)
        # NGC 2403 (Fraternali et al. 2002): flat rotation curve at about
        # 130 km/s outside the inner arcminutes, dispersion about 10 km/s
        self.assertTrue(np.all(rotation > 40.) and np.all(rotation < 180.), rotation)
        self.assertGreater(rotation[-1], rotation[0])
        self.assertTrue(np.all(dispersion > 2.) and np.all(dispersion < 40.), dispersion)
        residualNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetResidualVolumeNodeID() or "")
        self.assertIsNotNone(residualNode)

        # the model recomputed from the rings of the table (not edited) is the
        # fitted model: same rings, same units (e.g. densities in 1E20 cm^-2)
        fitted = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())).copy()
        self.runOperation(parametersNode, widget.onCalculateAndVisualize)
        recomputed = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID()))
        peak = np.nanmax(np.abs(fitted))
        self.assertLess(np.nanmax(np.abs(np.nan_to_num(recomputed) - np.nan_to_num(fitted))), 1e-3 * peak)
        np.testing.assert_allclose(self.ringColumn(parametersNode, "Dens"), 1., rtol=1e-6)

        # edit the rings and recompute the model from the table
        before = voxelArray(
            slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID())).copy()
        column = parametersNode.GetParamsTableNode().GetTable().GetColumnByName("VRot")
        for row in range(column.GetNumberOfTuples()):
            column.SetValue(row, column.GetValue(row) * 0.5)
        parametersNode.GetParamsTableNode().GetTable().Modified()
        self.runOperation(parametersNode, widget.onCalculateAndVisualize)
        after = voxelArray(slicer.mrmlScene.GetNodeByID(parametersNode.GetOutputVolumeNodeID()))
        self.assertEqual(after.shape, before.shape)
        self.assertFalse(np.allclose(np.nan_to_num(after), np.nan_to_num(before)),
                         "the model did not change with the rotation velocities")

    def test_CancelFit(self):
        """A fit is cancelled within seconds and leaves the module usable."""
        volumeNode, widget, parametersNode = self.openModule()
        self.estimate(widget, parametersNode)
        parametersNode.SetMode("Manual")
        parametersNode.SetNumberOfRings(10)
        parametersNode.SetTwoStage(True)
        seconds = self.runOperation(parametersNode, widget.onFit, cancelAfter=3., timeout=600)
        self.assertFalse(parametersNode.GetFitSuccess())
        print("fit cancelled after %.1f s" % seconds)
        # the module runs again after a cancel
        parametersNode.SetNumberOfRings(2)
        self.runOperation(parametersNode, widget.onCreate)

    def test_EstimateInitialParameters(self):
        """Only the estimate of the initial parameters is tested: it is fast,
        while the fit takes minutes (it runs in a worker thread)."""
        import AstroSampleData

        self.delayDisplay("Estimating the initial parameters of NGC 2403")
        volumeNode = AstroSampleData.AstroSampleDataLogic().downloadSample("NGC2403")
        self.assertIsNotNone(volumeNode)

        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActiveVolumeID(volumeNode.GetID())
        mainWindow = slicer.util.mainWindow()
        mainWindow.moduleSelector().selectModule("AstroVolume")
        mainWindow.moduleSelector().selectModule("AstroModeling")
        widget = slicer.modules.astromodeling.widgetRepresentation()
        parametersNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroModelingParametersNode")
        self.assertIsNotNone(parametersNode)
        parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
        # not estimated yet (the estimate sets it)
        parametersNode.SetSystemicVelocity(-1.)

        parametersNode.SetNumberOfRings(20)
        parametersNode.SetStatus(0)
        widget.onEstimateInitialParameters()

        # the estimate runs in a worker thread: wait until it ends (status 100,
        # also on errors). Debug, coverage and sanitizer builds are many times
        # slower than Release, so the safety limit is generous.
        timeout = time.time() + 900
        while (parametersNode.GetSystemicVelocity() < 0 and parametersNode.GetStatus() != 100
               and time.time() < timeout):
            slicer.app.processEvents()
            time.sleep(0.1)

        # NGC 2403 (Fraternali et al. 2002): systemic velocity 133 km/s,
        # inclination 60-63 degrees, position angle 124 degrees
        self.assertLess(abs(parametersNode.GetSystemicVelocity() - 133.), 3.)
        self.assertGreater(parametersNode.GetNumberOfRings(), 0)
        self.assertGreater(parametersNode.GetInclination(), 40.)
        self.assertLess(parametersNode.GetInclination(), 80.)
        self.assertLess(abs(parametersNode.GetPositionAngle() - 124.), 20.)
        self.assertGreater(parametersNode.GetRotationVelocity(), 0.)
        self.delayDisplay("Test passed")
