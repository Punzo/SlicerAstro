import math
import os
import shutil
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


class AstroSceneTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Scene Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume", "AstroSmoothing", "AstroMasking", "AstroMomentMaps",
                                    "AstroPVDiagram", "AstroReproject", "AstroModeling"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
A scene with the data and the outputs of the SlicerAstro modules (smoothing,
masking, moment maps, PV diagram, reprojection, 3DBarolo model, SoFiA mask
and catalogue, contours, astronomical terminology) saved as a Medical Reality
Bundle (.mrb) and as a scene folder (.mrml and data files), loaded again and
compared: voxels, WCS and header, display, segments, tables, markups and the
references of the parameters nodes.
"""
        self.parent.hidden = True


class AstroSceneTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroSceneTestLogic(ScriptedLoadableModuleLogic):
    pass


def voxelArray(volumeNode):
    """Zero-copy (z, y, x) view on the voxels of a volume node."""
    from vtk.util.numpy_support import vtk_to_numpy

    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


# header attributes compared after a save and a load
HEADER_KEYS = ("NAXIS", "NAXIS1", "NAXIS2", "NAXIS3", "CTYPE1", "CTYPE2", "CTYPE3", "CRVAL1", "CRVAL2", "CRVAL3",
               "CDELT1", "CDELT2", "CDELT3", "CRPIX1", "CRPIX2", "CRPIX3", "CUNIT1", "CUNIT2", "CUNIT3", "BUNIT",
               "BMAJ", "BMIN", "BPA", "RESTFREQ", "SPECSYS", "DATAMODEL")

GAS_CLOUD = ("Segmentation category and type - SlicerAstro~99SLICERASTRO^SA-C3^Gas structure~"
             "99SLICERASTRO^SA-T306^Gas cloud~^^~~^^~^^")
TIDAL_TAIL = ("Segmentation category and type - SlicerAstro~99SLICERASTRO^SA-C3^Gas structure~"
              "99SLICERASTRO^SA-T304^Tidal tail~^^~~^^~^^")


def number(value):
    """Float of a numeric string (header values are written again by the FITS
    writer, e.g. 0.002 -> 2.0E-03), else the string."""
    try:
        return float(value)
    except (TypeError, ValueError):
        return value


class AstroSceneTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)
        self.directory = os.path.join(slicer.app.temporaryPath, "AstroSceneTest")
        if os.path.isdir(self.directory):
            shutil.rmtree(self.directory)
        os.makedirs(self.directory)
        # names of the nodes compared after a save and a load
        self.tracked = []

    def runTest(self):
        self.setUp()
        self.test_SceneSaveAndLoad()
        self.setUp()
        self.test_OlderScene()

    #
    # scene
    #

    def track(self, *nodes):
        for node in nodes:
            self.assertIsNotNone(node)
            if node.GetName() not in self.tracked:
                self.tracked.append(node.GetName())

    def selectModule(self, name):
        slicer.util.mainWindow().moduleSelector().selectModule(name)
        slicer.app.processEvents()
        return getattr(slicer.modules, name.lower()).widgetRepresentation()

    def apply(self, moduleName):
        slicer.util.findChild(self.selectModule(moduleName), "ApplyButton").click()
        slicer.qSlicerAstroBackgroundRunner().waitForJobs()
        slicer.app.processEvents()

    def runModeling(self, parametersNode, start, timeout=1500):
        """3DBarolo operation in the worker thread (see AstroModelingSelfTest.runOperation)."""
        parametersNode.SetStatus(0)
        begin = time.time()
        start()
        self.assertNotEqual(parametersNode.GetStatus(), 0, "the operation did not start")
        while parametersNode.GetStatus() not in (0, 100) and time.time() < begin + timeout:
            slicer.app.processEvents()
            time.sleep(0.05)
        self.assertIn(parametersNode.GetStatus(), (0, 100), "the operation did not end")
        for _ in range(10):
            slicer.app.processEvents()
            time.sleep(0.05)

    def waitForModeling(self, timeout=600):
        """A loaded scene with a model: the Modeling module computes it again
        from the table (worker thread); wait for its end."""
        parametersNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroModelingParametersNode")
        begin = time.time()
        while parametersNode and parametersNode.GetStatus() not in (0, 100) and time.time() < begin + timeout:
            slicer.app.processEvents()
            time.sleep(0.05)
        for _ in range(10):
            slicer.app.processEvents()
            time.sleep(0.05)

    def buildScene(self):
        import AstroSampleData

        sampleData = AstroSampleData.AstroSampleDataLogic()
        cube = sampleData.downloadSample("WEIN069")
        sampleData.downloadSample("WEIN069_MASK")
        selectionNode = slicer.app.applicationLogic().GetSelectionNode()
        selectionNode.SetActiveVolumeID(cube.GetID())
        astroLogic = slicer.modules.astrovolume.logic()
        self.track(cube, cube.GetNodeReference(astroLogic.GetMaskReferenceRole()),
                   cube.GetNodeReference(astroLogic.GetContoursReferenceRole()))

        # SoFiA mask (one source retyped by the user) and catalogue
        sofiaMask = slicer.util.getFirstNodeByClassByName("vtkMRMLSegmentationNode", "WEIN069_mask")
        self.track(sofiaMask)
        sofiaSegments = sofiaMask.GetSegmentation()
        self.assertGreater(sofiaSegments.GetNumberOfSegments(), 1)
        sofiaSegments.GetNthSegment(0).SetTerminology(GAS_CLOUD)
        for table in slicer.util.getNodesByClass("vtkMRMLTableNode"):
            if table.GetAttribute("SlicerAstro.SoFiASources"):
                self.track(table)
        for markups in slicer.util.getNodesByClass("vtkMRMLMarkupsFiducialNode"):
            # the source points (not the empty point lists of the modules)
            if markups.GetNumberOfControlPoints() > 0:
                self.track(markups)

        # contours: a type of the terminology and a custom color
        moduleWidget = self.selectModule("AstroVolume")
        contoursSection = moduleWidget.contoursSection()
        contoursSection.setLevels("3, 6, 12", slicer.qSlicerAstroContoursSection.Sigma)
        self.assertEqual(contoursSection.drawContours(), 3)
        slicer.app.processEvents()
        contoursNode = cube.GetNodeReference(astroLogic.GetContoursReferenceRole())
        self.assertGreater(contoursNode.GetSegmentation().GetNumberOfSegments(), 0)
        slicer.vtkSlicerTerminologiesModuleLogic.SetDefaultTerminologyEntryAsString(contoursNode, TIDAL_TAIL)
        contoursNode.GetSegmentation().GetNthSegment(0).SetColor(80 / 255.0, 200 / 255.0, 120 / 255.0)
        slicer.app.processEvents()

        # smoothing: box filter
        self.selectModule("AstroSmoothing")
        smoothing = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroSmoothingParametersNode")
        smoothing.SetInputVolumeNodeID(cube.GetID())
        smoothing.SetFilter(0)
        smoothing.SetParameterX(3)
        smoothing.SetParameterY(3)
        smoothing.SetParameterZ(3)
        smoothing.SetGaussianKernels()
        self.apply("AstroSmoothing")
        self.track(slicer.mrmlScene.GetNodeByID(smoothing.GetOutputVolumeNodeID()))

        # moment maps 0 and 1
        selectionNode.SetActiveVolumeID(cube.GetID())
        self.selectModule("AstroMomentMaps")
        moments = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroMomentMapsParametersNode")
        moments.SetInputVolumeNodeID(cube.GetID())
        moments.SetMaskActive(False)
        moments.SetGenerateZero(True)
        moments.SetGenerateFirst(True)
        moments.SetGenerateSecond(False)
        moments.SetIntensityMin(3. * cube.GetDisplayThreshold())
        self.apply("AstroMomentMaps")
        self.track(slicer.mrmlScene.GetNodeByID(moments.GetZeroMomentVolumeNodeID()),
                   slicer.mrmlScene.GetNodeByID(moments.GetFirstMomentVolumeNodeID()))

        # masking: blank outside a SoFiA source
        selectionNode.SetActiveVolumeID(cube.GetID())
        maskingWidget = self.selectModule("AstroMasking")
        masking = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroMaskingParametersNode")
        masking.SetInputVolumeNodeID(cube.GetID())
        masking.SetOperation("Blank")
        masking.SetBlankRegion("Outside")
        masking.SetBlankValue("0")
        slicer.util.findChild(maskingWidget, "RegionButton").setCurrentIndex(1)
        slicer.util.findChild(maskingWidget, "SegmentationNodeComboBox").setCurrentNode(sofiaMask)
        slicer.app.processEvents()
        slicer.util.findChild(maskingWidget, "SegmentsTableView").setSelectedSegmentIDs(
            [sofiaSegments.GetNthSegmentID(1)])
        slicer.app.processEvents()
        self.apply("AstroMasking")
        self.track(slicer.mrmlScene.GetNodeByID(masking.GetOutputVolumeNodeID()))
        # the input mask is the segment (saved with the scene)
        self.assertEqual(masking.GetMaskSegmentationNodeID(), sofiaMask.GetID())
        self.assertEqual(masking.GetMaskSegmentID(), sofiaSegments.GetNthSegmentID(1))

        # PV diagram along a line on the moment map
        selectionNode.SetActiveVolumeID(cube.GetID())
        self.selectModule("AstroPVDiagram")
        pvDiagram = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroPVDiagramParametersNode")
        line = slicer.util.getNode("PVDiagramLine")
        line.AddControlPoint(0, 0, 0)
        line.AddControlPoint(30, 0, 0)
        line.AddControlPoint(30, 0, 40)
        slicer.app.processEvents()
        self.track(line, slicer.mrmlScene.GetNodeByID(pvDiagram.GetOutputVolumeNodeID()))

        # reprojection: pixels twice as large (the module opened, as a user
        # would: its parameters node is in the scene)
        self.selectModule("AstroReproject")
        reproject = slicer.vtkMRMLAstroReprojectParametersNode()
        reproject.SetInputVolumeNodeIDs(cube.GetID())
        reproject.SetInputVolumeNodeID(cube.GetID())
        reproject.SetSpatialGridMode(reproject.SpatialResample)
        reproject.SetSpatialResampleFactor(2.)
        reprojectLogic = slicer.modules.astroreproject.logic()
        self.assertTrue(reprojectLogic.Reproject(reproject), reprojectLogic.GetLastError())
        self.track(slicer.mrmlScene.GetNodeByID(reproject.GetOutputVolumeNodeID()))

        # 3DBarolo model of NGC 2403 (Manual, few rings)
        galaxy = sampleData.downloadSample("NGC2403")
        self.track(galaxy)
        selectionNode.SetActiveVolumeID(galaxy.GetID())
        self.selectModule("AstroVolume")
        modelingWidget = self.selectModule("AstroModeling")
        modeling = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroModelingParametersNode")
        modeling.SetInputVolumeNodeID(galaxy.GetID())
        modeling.SetMode("Manual")
        for name, value in (("NumberOfRings", 4), ("RadSep", 120.), ("XCenter", 163.4), ("YCenter", 117.4),
                            ("SystemicVelocity", 133.), ("RotationVelocity", 120.), ("VelocityDispersion", 10.),
                            ("Inclination", 62.), ("PositionAngle", 124.), ("ScaleHeight", 0.),
                            ("ColumnDensity", 1.), ("Distance", 3.2)):
            getattr(modeling, "Set" + name)(value)
        self.runModeling(modeling, modelingWidget.onCreate)
        self.track(slicer.mrmlScene.GetNodeByID(modeling.GetOutputVolumeNodeID()), modeling.GetParamsTableNode())

    #
    # state of the scene
    #

    def volumeState(self, node):
        values = voxelArray(node).astype(np.float64)
        state = {
            "class": node.GetClassName(),
            "shape": values.shape,
            "sum": float(np.nansum(values)),
            "blank": int(np.isnan(values).sum()),
            "header": {key: number(node.GetAttribute("SlicerAstro." + key)) for key in HEADER_KEYS},
            "ijk to ras": self.ijkToRAS(node),
        }
        display = node.GetDisplayNode()
        if display and display.IsA("vtkMRMLAstroVolumeDisplayNode"):
            colorNode = display.GetColorNode()
            state["display"] = {
                "color": colorNode.GetName() if colorNode else None,
                "window": display.GetWindow(),
                "level": display.GetLevel(),
                "contours color": [display.GetContoursColor().GetValue(i) for i in range(3)],
            }
        return state

    @staticmethod
    def ijkToRAS(node):
        matrix = vtk.vtkMatrix4x4()
        node.GetIJKToRASMatrix(matrix)
        return [matrix.GetElement(row, column) for row in range(3) for column in range(4)]

    def segmentationState(self, node):
        from vtk.util.numpy_support import vtk_to_numpy

        binary = slicer.vtkSegmentationConverter.GetBinaryLabelmapRepresentationName()
        segmentation = node.GetSegmentation()
        segments = {}
        for index in range(segmentation.GetNumberOfSegments()):
            segment = segmentation.GetNthSegment(index)
            labelmap = segment.GetRepresentation(binary)
            scalars = labelmap.GetPointData().GetScalars() if labelmap else None
            voxels = int((vtk_to_numpy(scalars) == segment.GetLabelValue()).sum()) if scalars else 0
            segments[segment.GetName()] = {
                "terminology": segment.GetTerminology(),
                # 8 bits per channel (#rrggbb in FITS masks)
                "color": [int(round(255 * c)) for c in segment.GetColor()],
                "voxels": voxels,
            }
        reference = node.GetNodeReference(slicer.vtkMRMLSegmentationNode.GetReferenceImageGeometryReferenceRole())
        display = node.GetDisplayNode()
        return {
            "segments": segments,
            "default terminology": slicer.vtkSlicerTerminologiesModuleLogic.GetDefaultTerminologyEntryAsString(node),
            "reference": reference.GetName() if reference else None,
            "opacity": display.GetOpacity() if display else None,
        }

    def tableState(self, node):
        table = node.GetTable()
        columns = [table.GetColumnName(c) for c in range(table.GetNumberOfColumns())]
        return {
            "rows": table.GetNumberOfRows(),
            "columns": columns,
            "values": [[number(table.GetValue(r, c).ToString()) for c in range(len(columns))]
                       for r in range(table.GetNumberOfRows())],
        }

    def markupsState(self, node):
        points = []
        for index in range(node.GetNumberOfControlPoints()):
            position = [0., 0., 0.]
            node.GetNthControlPointPosition(index, position)
            points.append((node.GetNthControlPointLabel(index), position))
        return {"class": node.GetClassName(), "points": points}

    def parametersState(self):
        """Nodes referenced by the parameters nodes of the modules (names)."""
        state = {}
        for node in slicer.util.getNodesByClass("vtkMRMLNode"):
            className = node.GetClassName()
            if not (className.startswith("vtkMRMLAstro") and className.endswith("ParametersNode")):
                continue
            references = {}
            for name in dir(node):
                if name.startswith("Get") and name.endswith("NodeID"):
                    try:
                        nodeID = getattr(node, name)()
                    except TypeError:
                        continue
                    referenced = slicer.mrmlScene.GetNodeByID(nodeID) if nodeID else None
                    if referenced is not None and referenced.GetName() in self.tracked:
                        references[name[3:]] = referenced.GetName()
            # input mask: a segment of a segmentation
            if hasattr(node, "GetMaskSegmentID") and node.GetMaskSegmentID():
                references["MaskSegmentID"] = node.GetMaskSegmentID()
            if className in state:
                # one parameters node per module (no copy added by a scene load)
                state[className]["count"] += 1
            else:
                references["count"] = 1
                state[className] = references
        return state

    def sceneState(self, strict=True):
        state = {}
        for name in self.tracked:
            nodes = [node for node in slicer.util.getNodes(name, useLists=True).get(name, [])
                     if node.IsA("vtkMRMLVolumeNode") or node.IsA("vtkMRMLSegmentationNode") or
                     node.IsA("vtkMRMLTableNode") or node.IsA("vtkMRMLMarkupsNode")]
            self.assertEqual(len(nodes), 1, f"{name}: {len(nodes)} nodes")
            node = nodes[0]
            if node.IsA("vtkMRMLVolumeNode"):
                state[name] = self.volumeState(node)
            elif node.IsA("vtkMRMLSegmentationNode"):
                state[name] = self.segmentationState(node)
            elif node.IsA("vtkMRMLTableNode"):
                state[name] = self.tableState(node)
            else:
                state[name] = self.markupsState(node)
        state["parameters nodes"] = self.parametersState()
        tables = [node for node in slicer.util.getNodesByClass("vtkMRMLColorTableNode")
                  if node.GetSingletonTag() == "SlicerAstro.Terminology"]
        state["terminology color table"] = [tables[0].GetNumberOfColors() if tables else 0, len(tables)]
        return state

    def assertSameState(self, expected, actual, path=""):
        if isinstance(expected, dict):
            self.assertIsInstance(actual, dict, path)
            self.assertEqual(sorted(expected.keys()), sorted(actual.keys()), path)
            for key in expected:
                self.assertSameState(expected[key], actual[key], f"{path}/{key}")
        elif isinstance(expected, (list, tuple)):
            self.assertEqual(len(expected), len(actual), path)
            for index, (e, a) in enumerate(zip(expected, actual)):
                self.assertSameState(e, a, f"{path}[{index}]")
        elif isinstance(expected, float) and isinstance(actual, (float, int)):
            if math.isnan(expected):
                self.assertTrue(math.isnan(actual), path)
            else:
                # float32 voxels, FITS header values written again, display
                # values in the scene XML of Slicer (6 significant digits)
                self.assertTrue(math.isclose(expected, actual, rel_tol=1e-5, abs_tol=1e-9),
                                f"{path}: {expected} != {actual}")
        else:
            self.assertEqual(expected, actual, path)

    #
    # tests
    #

    def test_SceneSaveAndLoad(self):
        self.delayDisplay("Scene with the outputs of the modules")
        self.buildScene()
        expected = self.sceneState()
        self.assertEqual(expected["terminology color table"], [29, 1])

        # Medical Reality Bundle
        self.delayDisplay("Saved and loaded as .mrb")
        bundle = os.path.join(self.directory, "AstroSceneTest.mrb")
        self.assertTrue(slicer.util.saveScene(bundle))
        slicer.mrmlScene.Clear(0)
        slicer.util.loadScene(bundle)
        self.waitForModeling()
        self.assertSameState(expected, self.sceneState(), "mrb")

        # scene folder: .mrml and the data files
        self.delayDisplay("Saved and loaded as a scene folder")
        folder = os.path.join(self.directory, "AstroSceneTestFolder")
        os.makedirs(folder)
        self.assertTrue(slicer.app.applicationLogic().SaveSceneToSlicerDataBundleDirectory(folder, None))
        scenes = [name for name in os.listdir(folder) if name.endswith(".mrml")]
        self.assertEqual(len(scenes), 1, os.listdir(folder))
        dataFiles = os.listdir(os.path.join(folder, "Data"))
        # astronomical volumes and masks as FITS files
        for name in self.tracked:
            node = slicer.util.getFirstNodeByName(name)
            storage = node.GetStorageNode() if node and hasattr(node, "GetStorageNode") else None
            if node.IsA("vtkMRMLAstroVolumeNode") or (node.IsA("vtkMRMLSegmentationNode") and
                                                       slicer.modules.astrovolume.logic().IsAstroSegmentation(node)):
                self.assertIsNotNone(storage, name)
                fileName = os.path.basename(storage.GetFileName())
                self.assertIn(fileName, dataFiles, name)
                self.assertTrue(fileName.endswith((".fits", ".fits.gz")), f"{name}: {fileName}")
        slicer.mrmlScene.Clear(0)
        slicer.util.loadScene(os.path.join(folder, scenes[0]))
        self.waitForModeling()
        self.assertSameState(expected, self.sceneState(), "folder")

        # the modules show the loaded scene and keep it unchanged
        self.delayDisplay("Modules on the loaded scene")
        galaxy = slicer.util.getFirstNodeByName("NGC2403")
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(galaxy.GetID())
        self.selectModule("AstroVolume")
        modelingWidget = self.selectModule("AstroModeling")
        modeling = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroModelingParametersNode")
        self.assertEqual(slicer.util.findChild(modelingWidget, "TableView").model().rowCount(),
                         modeling.GetParamsTableNode().GetNumberOfRows())
        # (not AstroPVDiagram: shown, it follows the active volume, NGC 2403
        # here, and moves its line on the moment map of that volume)
        for moduleName in ("AstroSmoothing", "AstroMomentMaps", "AstroMasking", "AstroReproject"):
            self.selectModule(moduleName)
        self.assertSameState(expected, self.sceneState(), "modules")
        self.delayDisplay("Test passed")

    def test_OlderScene(self):
        """Scenes are saved with the SlicerAstro scene format; scenes of
        SlicerAstro < 2.0 (no format, label map volumes, old parameters) are
        reported as not compatible, their label maps are not loaded."""
        from AstroFITSIOTest import beamCards, celestialWCS, radioVelocityWCS, writeFITS

        self.delayDisplay("Scene format")
        logic = slicer.modules.astrovolume.logic()
        path = os.path.join(self.directory, "cube.fits")
        writeFITS(path, np.ones((4, 6, 8), dtype=np.float32), -32,
                  celestialWCS() + radioVelocityWCS() + beamCards())
        cube = slicer.util.loadVolume(path)
        self.assertEqual(cube.GetSceneFormat(), slicer.vtkMRMLAstroVolumeNode.GetCurrentSceneFormat())
        folder = os.path.join(self.directory, "FormatScene")
        os.makedirs(folder)
        self.assertTrue(slicer.app.applicationLogic().SaveSceneToSlicerDataBundleDirectory(folder, None))
        sceneFile = os.path.join(folder, [name for name in os.listdir(folder) if name.endswith(".mrml")][0])
        with open(sceneFile) as f:
            text = f.read()
        self.assertIn('slicerAstroSceneFormat="2"', text)

        # a scene of SlicerAstro 2: compatible
        slicer.mrmlScene.Clear(0)
        slicer.util.loadScene(sceneFile)
        self.assertFalse(logic.GetIncompatibleSceneImported())
        self.assertEqual(slicer.util.getNode("cube").GetSceneFormat(), 2)

        # the same scene as written by SlicerAstro 1.x: no format, a label map
        # volume (mask) and a parameters node with a mask label map
        import re
        oldText = text.replace(' slicerAstroSceneFormat="2"', "")
        oldText = re.sub(r"(</MRML>)",
                         '<AstroLabelMapVolume id="vtkMRMLAstroLabelMapVolumeNode1" name="cube_mask" '
                         'references="display:vtkMRMLAstroLabelMapVolumeDisplayNode1;" ></AstroLabelMapVolume>\n'
                         '<AstroLabelMapVolumeDisplay id="vtkMRMLAstroLabelMapVolumeDisplayNode1" '
                         'name="AstroLabelMapVolumeDisplay" ></AstroLabelMapVolumeDisplay>\n'
                         '<AstroMomentMapsParameters id="vtkMRMLAstroMomentMapsParametersNode9" '
                         'name="AstroMomentMapsParameters" MaskVolumeNodeID="vtkMRMLAstroLabelMapVolumeNode1" '
                         'MaskActive="true" ></AstroMomentMapsParameters>\n\\1', oldText)
        oldScene = os.path.join(folder, "older.mrml")
        with open(oldScene, "w") as f:
            f.write(oldText)
        slicer.mrmlScene.Clear(0)
        try:
            slicer.util.loadScene(oldScene)
        except RuntimeError:
            pass  # the nodes of unknown classes are reported as errors
        self.assertTrue(logic.GetIncompatibleSceneImported())
        self.assertIsNotNone(slicer.util.getFirstNodeByName("cube"))
        self.assertEqual(slicer.util.getNode("cube").GetSceneFormat(), 0)
        self.assertEqual(slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLLabelMapVolumeNode"), 0)
        for node in slicer.util.getNodesByClass("vtkMRMLAstroMomentMapsParametersNode"):
            self.assertIsNone(node.GetMaskSegmentationNodeID())
        self.assertIn("older than 2.0", slicer.vtkSlicerAstroVolumeLogic.GetIncompatibleSceneMessage())

        # a new scene: compatible again
        slicer.mrmlScene.Clear(0)
        slicer.util.loadScene(sceneFile)
        self.assertFalse(logic.GetIncompatibleSceneImported())
        self.delayDisplay("Test passed")
