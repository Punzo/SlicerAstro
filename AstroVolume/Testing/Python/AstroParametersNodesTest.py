import math

import slicer
from slicer.ScriptedLoadableModule import (
    ScriptedLoadableModule,
    ScriptedLoadableModuleLogic,
    ScriptedLoadableModuleTest,
    ScriptedLoadableModuleWidget,
)


class AstroParametersNodesTest(ScriptedLoadableModule):
    def __init__(self, parent):
        ScriptedLoadableModule.__init__(self, parent)
        self.parent.title = "Astro Parameters Nodes Test"
        self.parent.categories = ["Testing.TestCases"]
        self.parent.dependencies = ["AstroVolume"]
        self.parent.contributors = ["Davide Punzo (Kapteyn Astronomical Institute)"]
        self.parent.helpText = """
The parameters nodes of the SlicerAstro modules keep every property when they
are copied and when the scene is saved and loaded (XML).
"""
        self.parent.hidden = True


class AstroParametersNodesTestWidget(ScriptedLoadableModuleWidget):
    pass


class AstroParametersNodesTestLogic(ScriptedLoadableModuleLogic):
    pass


PARAMETERS_NODES = (
    "vtkMRMLAstroMaskingParametersNode",
    "vtkMRMLAstroModelingParametersNode",
    "vtkMRMLAstroMomentMapsParametersNode",
    "vtkMRMLAstroProfilesParametersNode",
    "vtkMRMLAstroPVDiagramParametersNode",
    "vtkMRMLAstroPVSliceParametersNode",
    "vtkMRMLAstroReprojectParametersNode",
    "vtkMRMLAstroSmoothingParametersNode",
    "vtkMRMLAstroStatisticsParametersNode",
)

# properties that are not saved on purpose (state of a running computation,
# references handled by the node references, values recomputed on load)
NOT_SAVED = {"Status", "FitSuccess", "Tag", "SingletonTag", "HideFromEditors", "Selectable",
             "SaveWithScene", "AddToScene", "UndoEnabled", "Name", "Description", "InDefaultTag",
             # vtkObject and vtkMRMLNode properties, not parameters of the modules
             "Debug", "GlobalWarningDisplay", "ObjectName", "ReferenceCount", "DisableModifiedEvent",
             "Selected", "Selectable",
             # one-shot request to update the slice views (Modeling), not a setting
             "ForceSliceUpdate"}

# properties saved by name (enumerations): a valid value instead of default + n
VALID_VALUES = {"Operation": 2}


def properties(node):
    """Properties with a Get and a Set method of one simple value."""
    # only the properties of the parameters node, not those of vtkMRMLNode
    inherited = set(dir(slicer.vtkMRMLScriptedModuleNode()))
    names = []
    for attribute in dir(node):
        if not attribute.startswith("Get") or attribute in inherited:
            continue
        name = attribute[3:]
        if name in NOT_SAVED or not hasattr(node, "Set" + name):
            continue
        if name.endswith(("ID", "NodeID", "Node", "Reference", "MTime", "ClassName", "Scene")):
            continue
        try:
            value = getattr(node, attribute)()
        except TypeError:
            continue  # the getter needs arguments
        if isinstance(value, (bool, int, float, str)):
            names.append(name)
    return names


def changed(value, index):
    """A value different from the default one, of the same type."""
    if isinstance(value, bool):
        return not value
    if isinstance(value, int):
        return value + 3 + index
    if isinstance(value, float):
        return value + 1.25 + index
    return "value%d" % index


def same(a, b):
    if isinstance(a, float) and isinstance(b, float):
        return math.isclose(a, b, rel_tol=1e-6, abs_tol=1e-9)
    return a == b


class AstroParametersNodesTestTest(ScriptedLoadableModuleTest):
    def setUp(self):
        slicer.mrmlScene.Clear(0)

    def runTest(self):
        self.setUp()
        self.test_CopyAndSceneXML()

    def configured(self, className):
        """Node in the scene with every simple property changed; returns the
        node and the values that were set and read back."""
        node = slicer.mrmlScene.AddNewNodeByClass(className)
        self.assertIsNotNone(node, className)
        values = {}
        for index, name in enumerate(properties(node)):
            value = VALID_VALUES.get(name, changed(getattr(node, "Get" + name)(), index))
            try:
                getattr(node, "Set" + name)(value)
            except (TypeError, ValueError):
                continue
            readBack = getattr(node, "Get" + name)()
            if same(readBack, value):
                values[name] = value  # setters that clamp or ignore a value are skipped
        self.assertGreater(len(values), 0, className)
        return node, values

    def assertValues(self, node, values, what):
        for name, value in values.items():
            result = getattr(node, "Get" + name)()
            if not same(result, value):
                self.mismatches.append("%s %s: %r != %r" % (what, name, result, value))

    def test_CopyAndSceneXML(self):
        self.mismatches = []
        nodes = {}
        for className in PARAMETERS_NODES:
            node, values = self.configured(className)
            nodes[className] = (node.GetID(), values)
            print("%s: %d properties" % (className, len(values)))

            copy = slicer.mrmlScene.CreateNodeByClass(className)
            copy.UnRegister(None)
            copy.Copy(node)
            self.assertValues(copy, values, className + " copy")
            self.assertIn(className, str(node))  # PrintSelf

        # scene XML: write, clear, read
        scene = slicer.mrmlScene
        scene.SetSaveToXMLString(1)
        scene.Commit()
        xml = scene.GetSceneXMLString()
        scene.SetSaveToXMLString(0)
        scene.Clear(0)
        scene.SetLoadFromXMLString(1)
        scene.SetSceneXMLString(xml)
        scene.Import()
        scene.SetLoadFromXMLString(0)

        for className, (nodeID, values) in nodes.items():
            node = scene.GetNodeByID(nodeID)
            self.assertIsNotNone(node, className + " not loaded")
            self.assertValues(node, values, className + " scene")

        print("\n".join(self.mismatches))
        self.assertEqual(self.mismatches, [])
