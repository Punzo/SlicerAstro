import time

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


class AstroModelingSelfTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_EstimateInitialParameters()

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
        widget.onEstimateInitialParameters()

        # the estimate runs in a worker thread: wait for the results
        timeout = time.time() + 120
        while parametersNode.GetSystemicVelocity() < 0 and time.time() < timeout:
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
