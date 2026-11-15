import logging
import os
import unittest
from math import fabs, floor

import ctk
import qt
import slicer
import vtk
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin


class SlicerAstroDataProbe(ScriptedLoadableModule):

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    parent.title = "SlicerAstro Data Probe"
    parent.categories = ["Quantification", "Astronomy"]
    parent.dependencies = ["DataProbe", "AstroVolume"]
    parent.contributors = ["""
    Davide Punzo (Kapteyn Astronomical Institute),
    Thijs van der Hulst (Kapteyn Astronomical Institute),
    Jos Roerdink (Johann Bernoulli Institute) and
    Jean-Christophe Fillion-Robin (Kitware)."""]
    parent.helpText = """
    Data probe factorization for WCS astronomical coordinates.
    """
    parent.acknowledgementText = """
    This module was developed by Davide Punzo. <br>
    This work was supported by ERC grant nr. 291531 and the Slicer Community.
    """

    if slicer.mrmlScene.GetTagByClassName( "vtkMRMLScriptedModuleNode" ) != 'ScriptedModule':
      slicer.mrmlScene.RegisterNodeClass(vtkMRMLScriptedModuleNode())

    # Trigger the override of DataProbe when application has started up
    if not slicer.app.commandOptions().noMainWindow :
      slicer.app.connect("startupCompleted()", self.override)


  def override(self):
    mainWindow = slicer.util.mainWindow()
    if mainWindow is None:
      return
    parent = slicer.util.findChild(mainWindow, "DataProbeCollapsibleWidget")
    if parent is None:
      logging.warning("No Data Probe frame - cannot override DataProbe")
      return
    SlicerAstroDataProbeLogic(parent)

    # disable 3DSlicer Annotations
    sliceAnnotations = slicer.modules.DataProbeInstance.infoWidget.sliceAnnotations
    sliceAnnotations.sliceViewAnnotationsEnabled = 0
    sliceAnnotations.updateSliceViewFromGUI()
    sliceAnnotations.updateEnabledButtons()
    sliceAnnotations.sliceViewAnnotationsCheckBox.checked = 0


class SlicerAstroDataProbeWidget(ScriptedLoadableModuleWidget):

  def setup(self):
    self.developerMode = False
    ScriptedLoadableModuleWidget.setup(self)

    annotationsCollapsibleButton = ctk.ctkCollapsibleButton()
    annotationsCollapsibleButton.text = "Astro Annotations"
    self.layout.addWidget(annotationsCollapsibleButton)
    # Layout within the dummy collapsible button
    annotationsFormLayout = qt.QVBoxLayout(annotationsCollapsibleButton)

    # Color
    horizontalLayout_1 = qt.QHBoxLayout()
    horizontalLayout_1.setObjectName("horizontalLayout_1");

    self.colorLabel = qt.QLabel()
    self.colorLabel.setText("Annotations color:")
    self.colorLabel.setFixedSize(qt.QSize(120, 30))
    horizontalLayout_1.addWidget(self.colorLabel)

    self.colorSelector = ctk.ctkColorPickerButton()
    self.colorSelector.setObjectName("ColorPickerButton")
    sizePolicy = qt.QSizePolicy()
    sizePolicy.setHorizontalPolicy(qt.QSizePolicy.Expanding)
    sizePolicy.setVerticalPolicy(qt.QSizePolicy.Fixed)
    sizePolicy.setHorizontalStretch(0)
    sizePolicy.setVerticalStretch(0)
    sizePolicy.setHeightForWidth(self.colorSelector.sizePolicy.hasHeightForWidth())
    self.colorSelector.setSizePolicy(sizePolicy)
    self.colorSelector.setMinimumSize(qt.QSize(0, 30))
    self.colorSelector.setIconSize(qt.QSize(32, 32))
    self.colorSelector.setColor(qt.QColor(255, 187, 20))
    self.colorSelector.setDisplayColorName(0)
    horizontalLayout_1.addWidget(self.colorSelector)

    annotationsFormLayout.addLayout(horizontalLayout_1)

    # Font Style
    horizontalLayout_2 = qt.QHBoxLayout()
    horizontalLayout_2.setObjectName("horizontalLayout_2");

    self.fontStyleLabel = qt.QLabel()
    self.fontStyleLabel.setText("Font style:")
    self.fontStyleLabel.setFixedSize(qt.QSize(120, 30))
    horizontalLayout_2.addWidget(self.fontStyleLabel)

    self.styleComboBox = qt.QComboBox()
    self.styleComboBox.addItem("Arial")
    self.styleComboBox.addItem("Courier")
    self.styleComboBox.addItem("Times")
    sizePolicy.setHeightForWidth(self.styleComboBox.sizePolicy.hasHeightForWidth())
    self.styleComboBox.setMinimumSize(qt.QSize(0, 30))
    horizontalLayout_2.addWidget(self.styleComboBox)

    annotationsFormLayout.addLayout(horizontalLayout_2)

    # Font size
    horizontalLayout_3 = qt.QHBoxLayout()
    horizontalLayout_3.setObjectName("horizontalLayout_3");

    self.fontSizeLabel = qt.QLabel()
    self.fontSizeLabel.setText("Font size:")
    self.fontSizeLabel.setFixedSize(qt.QSize(120, 30))
    horizontalLayout_3.addWidget(self.fontSizeLabel)

    self.sizeSpinBox = qt.QSpinBox()
    sizePolicy.setHeightForWidth(self.sizeSpinBox.sizePolicy.hasHeightForWidth())
    self.sizeSpinBox.setMinimumSize(qt.QSize(0, 30))
    self.sizeSpinBox.minimum = 1
    self.sizeSpinBox.maximum = 30
    self.sizeSpinBox.setValue(12)
    self.sizeSpinBox.setToolTip("This value is multiplied for 1.5 if the ruler is set to thick.")

    horizontalLayout_3.addWidget(self.sizeSpinBox)

    annotationsFormLayout.addLayout(horizontalLayout_3)

    verticalSpacer = qt.QSpacerItem(200, 200, qt.QSizePolicy.Minimum, qt.QSizePolicy.Expanding)
    self.layout.addItem(verticalSpacer)

    # Connections
    self.colorSelector.connect('colorChanged(QColor)', self.onAnnotationsColorChanged)

    self.styleComboBox.connect('currentTextChanged(QString)', self.onAnnotationsFontStyleChanged)

    self.sizeSpinBox.connect('valueChanged(int)', self.onAnnotationsFontSizeChanged)


  def onAnnotationsColorChanged(self, color):
    lm = slicer.app.layoutManager()
    for sliceName in lm.sliceViewNames():
      sWidget = lm.sliceWidget(sliceName)
      sView = sWidget.sliceView()
      DisplayableManagersCollection = vtk.vtkCollection()
      sView.getDisplayableManagers(DisplayableManagersCollection)
      for DisplayableManagersIndex in range(DisplayableManagersCollection.GetNumberOfItems()):
        AstroDisplayableManager = DisplayableManagersCollection.GetItemAsObject(DisplayableManagersIndex)
        if AstroDisplayableManager.GetClassName() == "vtkMRMLAstroTwoDAxesDisplayableManager" or \
           AstroDisplayableManager.GetClassName() == "vtkMRMLAstroBeamDisplayableManager" :
          AstroDisplayableManager.SetAnnotationsColor(color.red() / 255., color.green() / 255., color.blue() / 255.)

  def onAnnotationsFontStyleChanged(self, font):
    lm = slicer.app.layoutManager()
    for sliceName in lm.sliceViewNames():
      sWidget = lm.sliceWidget(sliceName)
      sView = sWidget.sliceView()
      DisplayableManagersCollection = vtk.vtkCollection()
      sView.getDisplayableManagers(DisplayableManagersCollection)
      for DisplayableManagersIndex in range(DisplayableManagersCollection.GetNumberOfItems()):
        AstroDisplayableManager = DisplayableManagersCollection.GetItemAsObject(DisplayableManagersIndex)
        if AstroDisplayableManager.GetClassName() == "vtkMRMLAstroTwoDAxesDisplayableManager":
          AstroDisplayableManager.SetAnnotationsFontStyle(font)

  def onAnnotationsFontSizeChanged(self, size):
    lm = slicer.app.layoutManager()
    for sliceName in lm.sliceViewNames():
      sWidget = lm.sliceWidget(sliceName)
      sView = sWidget.sliceView()
      DisplayableManagersCollection = vtk.vtkCollection()
      sView.getDisplayableManagers(DisplayableManagersCollection)
      for DisplayableManagersIndex in range(DisplayableManagersCollection.GetNumberOfItems()):
        AstroDisplayableManager = DisplayableManagersCollection.GetItemAsObject(DisplayableManagersIndex)
        if AstroDisplayableManager.GetClassName() == "vtkMRMLAstroTwoDAxesDisplayableManager":
          AstroDisplayableManager.SetAnnotationsFontSize(size)

class SlicerAstroDataProbeLogic(ScriptedLoadableModuleLogic):

  def __init__(self, parent):
    ScriptedLoadableModuleLogic.__init__(self, parent)

    dataProbeInstance = slicer.modules.DataProbeInstance
    funcType = type(dataProbeInstance.infoWidget.generateViewDescription)
    dataProbeInstance.infoWidget.generateViewDescription = funcType(generateViewDescriptionAstro, dataProbeInstance.infoWidget)

    funcType = type(dataProbeInstance.infoWidget.generateLayerName)
    dataProbeInstance.infoWidget.generateLayerName = funcType(generateLayerNameAstro, dataProbeInstance.infoWidget)

    funcType = type(dataProbeInstance.infoWidget.generateIJKPixelDescription)
    dataProbeInstance.infoWidget.generateIJKPixelDescription = funcType(generateIJKPixelDescriptionAstro, dataProbeInstance.infoWidget)

    funcType = type(dataProbeInstance.infoWidget.generateIJKPixelValueDescription)
    dataProbeInstance.infoWidget.generateIJKPixelValueDescription = funcType(generateIJKPixelValueDescriptionAstro, dataProbeInstance.infoWidget)


class SlicerAstroDataProbeTest(ScriptedLoadableModuleTest):

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    for test in (self.test_Override, self.test_CubeReadout, self.test_ImageReadout,
                 self.test_NoWCS, self.test_ScalarVolume):
      self.setUp()
      test()

  #
  # helpers
  #

  def writeFITS(self, name, data, cards):
    """Single float image HDU with the header cards (list of (key, value))."""
    import os
    import numpy as np

    def card(key, value):
      if isinstance(value, bool):
        text = ("T" if value else "F").rjust(20)
      elif isinstance(value, int):
        text = str(value).rjust(20)
      elif isinstance(value, float):
        text = repr(value).upper().rjust(20)
      else:
        text = ("'" + str(value).ljust(8) + "'").ljust(20)
      return ("%-8s= %s" % (key, text))[:80].ljust(80)

    header = [("SIMPLE", True), ("BITPIX", -32), ("NAXIS", data.ndim)]
    header += [("NAXIS%d" % axis, size) for axis, size in enumerate(reversed(data.shape), start=1)]
    text = "".join(card(*c) for c in header + list(cards)) + "END".ljust(80)
    text += " " * (-len(text) % 2880)
    payload = np.ascontiguousarray(data, dtype=">f4").tobytes()
    path = os.path.join(slicer.app.temporaryPath, name)
    with open(path, "wb") as f:
      f.write(text.encode("ascii") + payload + b"\0" * (-len(payload) % 2880))
    node = slicer.util.loadVolume(path)
    self.assertIsNotNone(node)
    return node

  CELESTIAL = [("CTYPE1", "RA---SIN"), ("CRVAL1", 114.21417), ("CDELT1", -0.002), ("CRPIX1", 16.),
               ("CUNIT1", "deg"), ("CTYPE2", "DEC--SIN"), ("CRVAL2", 65.6025), ("CDELT2", 0.002),
               ("CRPIX2", 12.), ("CUNIT2", "deg"), ("RADESYS", "FK5"), ("EQUINOX", 2000.),
               ("BUNIT", "JY/BEAM"), ("BMAJ", 0.006), ("BMIN", 0.006), ("BPA", 0.)]
  SPECTRAL = [("CTYPE3", "VRAD"), ("CRVAL3", 133400.), ("CDELT3", 2580.), ("CRPIX3", 6.),
              ("CUNIT3", "m/s"), ("RESTFREQ", 1.420405752E9), ("SPECSYS", "BARYCENT")]

  def showInRedView(self, volumeNode):
    """Volume in the background of the red slice view; returns the slice
    node, its logic and the XY position of the centre of the view."""
    layoutManager = slicer.app.layoutManager()
    sliceWidget = layoutManager.sliceWidget("Red")
    sliceLogic = sliceWidget.sliceLogic()
    sliceLogic.GetSliceCompositeNode().SetBackgroundVolumeID(volumeNode.GetID())
    sliceLogic.FitSliceToAll()
    slicer.app.processEvents()
    sliceNode = sliceLogic.GetSliceNode()
    dimensions = sliceNode.GetDimensions()
    return sliceNode, sliceLogic, [dimensions[0] / 2., dimensions[1] / 2., 0.]

  #
  # tests
  #

  def test_Override(self):
    """The Data Probe of 3D Slicer uses the functions of this module."""
    infoWidget = slicer.modules.DataProbeInstance.infoWidget
    self.assertIs(infoWidget.generateViewDescription.__func__, generateViewDescriptionAstro)
    self.assertIs(infoWidget.generateLayerName.__func__, generateLayerNameAstro)
    self.assertIs(infoWidget.generateIJKPixelDescription.__func__, generateIJKPixelDescriptionAstro)
    self.assertIs(infoWidget.generateIJKPixelValueDescription.__func__, generateIJKPixelValueDescriptionAstro)

  def test_CubeReadout(self):
    import numpy as np
    data = np.arange(11 * 23 * 31, dtype=np.float32).reshape(11, 23, 31) * 0.001
    volumeNode = self.writeFITS("probe_cube.fits", data, self.CELESTIAL + self.SPECTRAL)
    sliceNode, sliceLogic, xyz = self.showInRedView(volumeNode)
    infoWidget = slicer.modules.DataProbeInstance.infoWidget
    description = infoWidget.generateViewDescription(xyz, [0., 0., 0.], sliceNode, sliceLogic)
    print(description)
    self.assertIn("WCS[", description)
    self.assertIn("J2000", description)
    # sexagesimal right ascension and declination near the reference values
    self.assertRegex(description, r"7h\s*36m")
    # all the fields, also when the arcminutes equal the minutes of the RA
    self.assertRegex(description, "65°\\s*36'\\s*9")
    self.assertIn("km/s", description)

    layerLogic = sliceLogic.GetBackgroundLayer()
    self.assertIn("probe_cube", infoWidget.generateLayerName(layerLogic))
    self.assertEqual(infoWidget.generateIJKPixelDescription([3, 4, 5], layerLogic), "(   3,    4,    5)")
    value = infoWidget.generateIJKPixelValueDescription([3, 4, 5], layerLogic)
    print(value)
    self.assertIn("JY/BEAM".lower(), value.lower())
    number = float(value.replace("<b>", "").split()[0])
    self.assertAlmostEqual(number, float(data[5, 4, 3]), delta=1e-6 + 1e-4 * abs(float(data[5, 4, 3])))

  def test_ImageReadout(self):
    """2D image: no spectral coordinate in the readout."""
    import numpy as np
    data = np.ones((23, 31), dtype=np.float32)
    volumeNode = self.writeFITS("probe_image.fits", data, self.CELESTIAL)
    sliceNode, sliceLogic, xyz = self.showInRedView(volumeNode)
    description = slicer.modules.DataProbeInstance.infoWidget.generateViewDescription(
      xyz, [0., 0., 0.], sliceNode, sliceLogic)
    print(description)
    self.assertIn("WCS[", description)
    self.assertNotIn("km/s", description)

  def test_NoWCS(self):
    """A cube without world coordinates is described in voxel coordinates."""
    import numpy as np
    data = np.ones((5, 7, 9), dtype=np.float32)
    volumeNode = self.writeFITS("probe_nowcs.fits", data, [("BUNIT", "JY/BEAM")])
    sliceNode, sliceLogic, xyz = self.showInRedView(volumeNode)
    description = slicer.modules.DataProbeInstance.infoWidget.generateViewDescription(
      xyz, [0., 0., 0.], sliceNode, sliceLogic)
    print(description)
    self.assertNotIn("WCS[", description)
    self.assertIn("not found", description)

  def test_ScalarVolume(self):
    """Volumes that are not astronomical use the standard descriptions."""
    import numpy as np
    volumeNode = slicer.util.addVolumeFromArray(np.full((4, 5, 6), 7., dtype=np.float32), name="scalar")
    sliceNode, sliceLogic, xyz = self.showInRedView(volumeNode)
    infoWidget = slicer.modules.DataProbeInstance.infoWidget
    layerLogic = sliceLogic.GetBackgroundLayer()
    value = infoWidget.generateIJKPixelValueDescription([1, 2, 3], layerLogic)
    self.assertIn("7", value)
    description = infoWidget.generateViewDescription(xyz, [0., 0., 0.], sliceNode, sliceLogic)
    self.assertIn("not found", description)


def generateViewDescriptionAstro(self, xyz, ras, sliceNode, sliceLogic):
  if sliceLogic:

    worldX = ""
    worldY = ""
    worldZ = ""
    equinoxString = ""
    world = [0., 0., 0.]

    CoordinateSystemName = "IJK"
    dimensionality = 3

    layerLogicCalls = (('B', sliceLogic.GetBackgroundLayer),
                      ('F', sliceLogic.GetForegroundLayer),
                      ('L', sliceLogic.GetLabelLayer))

    for layer,logicCall in layerLogicCalls:
      layerLogic = logicCall()
      volumeNode = layerLogic.GetVolumeNode()
      if volumeNode is not None and (volumeNode.IsA("vtkMRMLAstroVolumeNode") or volumeNode.IsA("vtkMRMLAstroLabelMapVolumeNode")):
        dimensionality = int(volumeNode.GetAttribute("SlicerAstro.NAXIS"))
        xyToIJK = layerLogic.GetXYToIJKTransform()
        ijkFloat = xyToIJK.TransformDoublePoint(xyz)
        displayNode = volumeNode.GetDisplayNode()
        if displayNode is not None and (displayNode.IsA("vtkMRMLAstroVolumeDisplayNode") or displayNode.IsA("vtkMRMLAstroLabelMapVolumeDisplayNode")):
          CoordinateSystemName = displayNode.GetSpace()
          displayNode.GetReferenceSpace(ijkFloat, world)
          # the formatter leaves out the fields equal to the previous value
          # (labels of the WCS axes): -1 never equals a field, all are shown
          noPrevious = [-1., -1., -1.]
          output = [0., 0., 0.]
          worldX = displayNode.GetDisplayStringFromValueX(world[0], noPrevious, output, 3)
          equinox = float(volumeNode.GetAttribute("SlicerAstro.EQUINOX"))
          if equinox > 1975:
            equinoxString = "J" + str(equinox)[:4]
          elif equinox < 1975:
            equinoxString = "B" + str(equinox)[:4]
          if dimensionality > 1:
            worldY = displayNode.GetDisplayStringFromValueY(world[1], noPrevious, output, 3)
          if dimensionality > 2:
            worldZ = displayNode.GetDisplayStringFromValueZ(world[2], noPrevious, output, 3)
            worldZ = displayNode.AddVelocityInfoToDisplayStringZ(worldZ)
          break

    if CoordinateSystemName == "WCS":
      if dimensionality > 2:
        return " {sys}[{orient}]: {worldX}, {worldY} ({equinoxString}), {worldZ}" \
                .format(
                sys = CoordinateSystemName,
                orient = sliceNode.GetOrientation(),
                worldX = worldX,
                worldY = worldY,
                equinoxString = equinoxString,
                worldZ = worldZ
                )
      elif dimensionality > 1:
        return " {sys}[{orient}]: {worldX}, {worldY} ({equinoxString})" \
                .format(
                sys = CoordinateSystemName,
                orient = sliceNode.GetOrientation(),
                worldX = worldX,
                worldY = worldY,
                equinoxString = equinoxString
                )
      else:
        return " {sys}[{orient}]: {worldX} ({equinoxString})" \
                .format(
                sys = CoordinateSystemName,
                orient = sliceNode.GetOrientation(),
                worldX = worldX,
                equinoxString = equinoxString
                )
    else:
      return " {layoutName} WCS in View {orient} not found" \
        .format(layoutName=sliceNode.GetLayoutName(),
                orient=sliceNode.GetOrientation()
                )


def generateLayerNameAstro(self, slicerLayerLogic):
  volumeNode = slicerLayerLogic.GetVolumeNode()
  return "<b>%s</b>" % (self.fitName(volumeNode.GetName()) if volumeNode else "None")

def generateIJKPixelDescriptionAstro(self, ijk, slicerLayerLogic):
  volumeNode = slicerLayerLogic.GetVolumeNode()
  return "({i:4d}, {j:4d}, {k:4d})".format(i=ijk[0], j=ijk[1], k=ijk[2]) if volumeNode else ""

def generateIJKPixelValueDescriptionAstro(self, ijk, slicerLayerLogic):
  volumeNode = slicerLayerLogic.GetVolumeNode()
  if volumeNode is not None:
    displayNode = volumeNode.GetDisplayNode()
    if displayNode is not None and (displayNode.IsA("vtkMRMLAstroVolumeDisplayNode") or displayNode.IsA("vtkMRMLAstroLabelMapVolumeDisplayNode")):
      return "<b>%s</b>" % displayNode.GetPixelString(ijk)
    # Not an astronomical volume: use the standard Data Probe pixel description
    return "<b>%s</b>" % self.getPixelString(volumeNode, ijk)
  return ""
