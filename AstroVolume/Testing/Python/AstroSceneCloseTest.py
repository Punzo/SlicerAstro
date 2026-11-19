import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleWidget,
)

import AstroSceneTest


#
# AstroSceneCloseTest: File > Close Scene (Ctrl+W) after using every module
# (own process: a closed scene must not change the following work)
#

class AstroSceneCloseTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "AstroSceneCloseTest"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroSceneTest"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = "Every SlicerAstro module used, the scene closed (Ctrl+W), the same work again."
        self.parent.acknowledgementText = ""


class AstroSceneCloseTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroSceneCloseTestTest(AstroSceneTest.AstroSceneTestTest):
    # only the test of this file (the ones of AstroSceneTest run in their
    # own process)
    test_SceneSaveAndLoad = None
    test_OlderScene = None

    def runTest(self):
        self.setUp()
        self.test_CloseSceneAndRepeat()

    def useOtherModules(self):
        """Profiles, Statistics and PV slice on the cube (the modules not used
        by buildScene)."""
        cube = slicer.util.getFirstNodeByName("WEIN069")
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(cube.GetID())
        for moduleName, parametersClass in (("AstroProfiles", "vtkMRMLAstroProfilesParametersNode"),
                                            ("AstroStatistics", "vtkMRMLAstroStatisticsParametersNode")):
            self.selectModule(moduleName)
            parameters = slicer.mrmlScene.GetFirstNodeByClass(parametersClass)
            self.assertIsNotNone(parameters, moduleName)
            parameters.SetInputVolumeNodeID(cube.GetID())
            self.apply(moduleName)
        self.selectModule("AstroPVSlice")
        self.assertIsNotNone(slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroPVSliceParametersNode"))

    def test_CloseSceneAndRepeat(self):
        """File > Close Scene (Ctrl+W) after using every module: nothing of
        SlicerAstro is left, and the same work done again gives the same
        scene (the modules observe the new scene)."""
        self.delayDisplay("Every module used")
        self.buildScene()
        self.useOtherModules()
        expected = self.sceneState()

        self.delayDisplay("Scene closed (Ctrl+W)")
        slicer.mrmlScene.Clear(False)
        for _ in range(20):
            slicer.app.processEvents()
        for className in ("vtkMRMLAstroVolumeNode", "vtkMRMLSegmentationNode", "vtkMRMLTableNode",
                          "vtkMRMLMarkupsNode"):
            # (the modules create their empty tables and markups again for
            # the new scene: nothing with data is left)
            left = [node.GetName() for node in slicer.util.getNodesByClass(className)
                    if not (node.IsA("vtkMRMLTableNode") and node.GetNumberOfRows() == 0) and
                    not (node.IsA("vtkMRMLMarkupsNode") and node.GetNumberOfDefinedControlPoints() == 0)]
            self.assertEqual(left, [], className)

        self.delayDisplay("Same work again")
        self.tracked = []
        self.buildScene()
        self.useOtherModules()
        self.assertSameState(expected, self.sceneState(), "after close")
        self.delayDisplay("Test passed")
