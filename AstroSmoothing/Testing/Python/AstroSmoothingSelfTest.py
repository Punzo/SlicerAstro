import os
import unittest
import vtk, qt, ctk, slicer
import math
import sys

#
# AstroSmoothingSelfTest
#

class AstroSmoothingSelfTest:
  def __init__(self, parent):
    parent.title = "Astro Smoothing SelfTest"
    parent.categories = ["Testing.TestCases"]
    parent.dependencies = ["AstroVolume"]
    parent.contributors = ["""
    Davide Punzo (Kapteyn Astronomical Institute) and
    Thijs van der Hulst (Kapteyn Astronomical Institute)."""]
    parent.helpText = """
    This module was developed as a self test to perform the operations needed for smoothing astroVolumes.
    """
    parent.acknowledgementText = """
""" # replace with organization, grant and thanks.
    self.parent = parent

    # Add this test to the SelfTest module's list for discovery when the module
    # is created.  Since this module may be discovered before SelfTests itself,
    # create the list if it doesn't already exist.
    try:
      slicer.selfTests
    except AttributeError:
      slicer.selfTests = {}
    slicer.selfTests['Astro Smoothing SelfTest'] = self.runTest

  def runTest(self):
    tester = AstroSmoothingSelfTestTest()
    tester.runTest()

#
# qAstroSmoothingSelfTestWidget
#

class AstroSmoothingSelfTestWidget:
  def __init__(self, parent = None):
    if not parent:
      self.parent = slicer.qMRMLWidget()
      self.parent.setLayout(qt.QVBoxLayout())
      self.parent.setMRMLScene(slicer.mrmlScene)
    else:
      self.parent = parent
    self.layout = self.parent.layout()
    if not parent:
      self.setup()
      self.parent.show()

  def setup(self):
    # Instantiate and connect widgets ...

    # reload button
    # (use this during development, but remove it when delivering
    #  your module to users)
    self.reloadButton = qt.QPushButton("Reload")
    self.reloadButton.toolTip = "Reload this module."
    self.reloadButton.name = "AstroSmoothingSelfTest Reload"
    self.layout.addWidget(self.reloadButton)
    self.reloadButton.connect('clicked()', self.onReload)

    # reload and test button
    # (use this during development, but remove it when delivering
    #  your module to users)
    self.reloadAndTestButton = qt.QPushButton("Reload and Test")
    self.reloadAndTestButton.toolTip = "Reload this module and then run the self tests."
    self.layout.addWidget(self.reloadAndTestButton)
    self.reloadAndTestButton.connect('clicked()', self.onReloadAndTest)

    # Add vertical spacer
    self.layout.addStretch(1)

  def cleanup(self):
    pass

  def onReload(self,moduleName="AstroSmoothingSelfTest"):
    """Generic reload method for any scripted module.
    ModuleWizard will subsitute correct default moduleName.
    """
    globals()[moduleName] = slicer.util.reloadScriptedModule(moduleName)

  def onReloadAndTest(self,moduleName="AstroSmoothingSelfTest"):
    self.onReload()
    evalString = 'globals()["%s"].%sTest()' % (moduleName, moduleName)
    tester = eval(evalString)
    tester.runTest()

#
# AstroSmoothingSelfTestLogic
#

class AstroSmoothingSelfTestLogic:
  """This class should implement all the actual
  computation done by your module.  The interface
  should be such that other python code can import
  this class and make use of the functionality without
  requiring an instance of the Widget
  """
  def __init__(self):
    pass

  def hasImageData(self,volumeNode):
    """This is a dummy logic method that
    returns true if the passed in volume
    node has valid image data
    """
    if not volumeNode:
      print('no volume node')
      return False
    if volumeNode.GetImageData() is None:
      print('no image data')
      return False
    return True


class AstroSmoothingSelfTestTest(unittest.TestCase):
  """
  This is the test case for your scripted module.
  """

  def delayDisplay(self,message,msec=100):
    """This utility method displays a small dialog and waits.
    This does two things: 1) it lets the event loop catch up
    to the state of the test so that rendering and widget updates
    have all taken place before the test continues and 2) it
    shows the user/developer/tester the state of the test
    so that we'll know when it breaks.
    """
    print(message)
    self.info = qt.QDialog()
    self.infoLayout = qt.QVBoxLayout()
    self.info.setLayout(self.infoLayout)
    self.label = qt.QLabel(message,self.info)
    self.infoLayout.addWidget(self.label)
    qt.QTimer.singleShot(msec, self.info.close)
    self.info.exec_()

  def setUp(self):
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    self.setUp()
    self.test_AstroSmoothingSelfTest()

  def test_AstroSmoothingSelfTest(self):
    print("Running AstroSmoothingSelfTest Test case:")

    astroVolume = self.downloadWEIN069()

    mainWindow = slicer.util.mainWindow()
    mainWindow.moduleSelector().selectModule('AstroVolume')
    mainWindow.moduleSelector().selectModule('AstroSmoothing')

    astroSmoothingModule = module = slicer.modules.astrosmoothing
    astroSmoothingModuleWidget = astroSmoothingModule.widgetRepresentation()

    AstroSmoothingParameterNode = slicer.util.getNode("AstroSmoothingParameters")

    QPushButtonList = astroSmoothingModuleWidget.findChildren(qt.QPushButton)
    for QPushButton in (QPushButtonList):
        if QPushButton.name == "ApplyButton":
            ApplyPushButton = QPushButton

    self.delayDisplay('Generating smoothed datacube', 700)
    ApplyPushButton.click()

    outputVolume = slicer.mrmlScene.GetNodeByID(AstroSmoothingParameterNode.GetOutputVolumeNodeID())
    pixelValue = outputVolume.GetImageData().GetScalarComponentAsFloat(83, 53, 24, 0)

    if (math.fabs(pixelValue - 0.000662423) < 1.e-9):
       self.delayDisplay('Test passed', 700)
    else:
       self.delayDisplay('Test failed', 700)
       # if run from Slicer interface remove the followinf exit
       sys.exit()


  def downloadWEIN069(self):
    import AstroSampleData
    astroSampleDataLogic = AstroSampleData.AstroSampleDataLogic()
    self.delayDisplay('Getting WEIN069 Astro Volume')
    WEIN069Volume = astroSampleDataLogic.downloadSample("WEIN069")
    return WEIN069Volume


