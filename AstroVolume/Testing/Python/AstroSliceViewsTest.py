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
