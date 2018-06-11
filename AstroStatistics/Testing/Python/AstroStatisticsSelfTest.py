import os
import unittest
import vtk, qt, ctk, slicer
import math
import sys

#
# AstroStatisticsSelfTest
#

class AstroStatisticsSelfTest:
  def __init__(self, parent):
    parent.title = "Astro Statistics SelfTest"
    parent.categories = ["Testing.TestCases"]
    parent.dependencies = ["AstroVolume"]
    parent.contributors = ["""
    Davide Punzo (Kapteyn Astronomical Institute) and
    Thijs van der Hulst (Kapteyn Astronomical Institute)."""]
    parent.helpText = """
    This module was developed as a self test to perform the operations needed for calculating statistics in a region.
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
    slicer.selfTests['Astro Statistics SelfTest'] = self.runTest

  def runTest(self):
    tester = AstroStatisticsSelfTestTest()
    tester.runTest()

#
# qAstroStatisticsSelfTestWidget
#

class AstroStatisticsSelfTestWidget:
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
    self.reloadButton.name = "AstroStatisticsSelfTest Reload"
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

  def onReload(self,moduleName="AstroStatisticsSelfTest"):
    """Generic reload method for any scripted module.
    ModuleWizard will subsitute correct default moduleName.
    """
    globals()[moduleName] = slicer.util.reloadScriptedModule(moduleName)

  def onReloadAndTest(self,moduleName="AstroStatisticsSelfTest"):
    self.onReload()
    evalString = 'globals()["%s"].%sTest()' % (moduleName, moduleName)
    tester = eval(evalString)
    tester.runTest()

#
# AstroStatisticsSelfTestLogic
#

class AstroStatisticsSelfTestLogic:
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


class AstroStatisticsSelfTestTest(unittest.TestCase):
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
    self.test_AstroStatisticsSelfTest()

  def test_AstroStatisticsSelfTest(self):
    print("Running AstroStatisticsSelfTest Test case:")

    astroVolume = self.downloadWEIN069()

    mainWindow = slicer.util.mainWindow()
    mainWindow.moduleSelector().selectModule('AstroVolume')
    mainWindow.moduleSelector().selectModule('AstroStatistics')

    astroStatisticsModule = module = slicer.modules.astrostatistics
    astroStatisticsModuleWidget = astroStatisticsModule.widgetRepresentation()

    AstroStatisticsParameterNode = slicer.util.getNode("AstroStatisticsParameters")

    QPushButtonList = astroStatisticsModuleWidget.findChildren(qt.QPushButton)
    for QPushButton in (QPushButtonList):
        if QPushButton.name == "ApplyButton":
            ApplyPushButton = QPushButton

    self.delayDisplay('Calculating statistics', 700)
    ApplyPushButton.click()

    Table = AstroStatisticsParameterNode.GetTableNode().GetTable()
    N = Table.GetValue(0, 1).ToFloat()
    Min = Table.GetValue(0, 2).ToFloat()
    Max = Table.GetValue(0, 3).ToFloat()
    Mean = Table.GetValue(0, 4).ToFloat()
    Std = Table.GetValue(0, 5).ToFloat()
    Median = Table.GetValue(0, 6).ToFloat()
    Sum = Table.GetValue(0, 7).ToFloat()
    TotalFlux = Table.GetValue(0, 8).ToFloat()

    if (math.fabs(N - 954569) < 1.e-1 and \
        math.fabs(Min + 0.00215719) < 1.e-8 and \
        math.fabs(Max - 0.0118773) < 1.e-7 and \
        math.fabs(Mean - 2.09643e-05) < 1.e-10 and \
        math.fabs(Std - 0.000548967) < 1.e-9 and \
        math.fabs(Median + 3.24158e-06) < 1.e-11 and \
        math.fabs(Sum - 20.0119) < 1.e-4 and \
        math.fabs(TotalFlux - 1.74693) < 1.e-5):
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


