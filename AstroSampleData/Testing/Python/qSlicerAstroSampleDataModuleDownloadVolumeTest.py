import unittest
import slicer
import qt

class qSlicerSampleDataModuleTest(unittest.TestCase):
  def setUp(self):
    mainWindow = slicer.util.mainWindow()
    mainWindow.moduleSelector().selectModule('AstroSampleData')
    module = slicer.modules.astrosampledata
    self.moduleWidget = module.widgetRepresentation()

  def test_download_volume(self):
    QToolButtonList = self.moduleWidget.findChildren(qt.QToolButton)
    QToolButtonWEIN069 = QToolButtonList[0]
    QToolButtonWEIN069.click()
    scene = slicer.mrmlScene
    self.assertEqual(scene.GetNumberOfNodesByClass('vtkMRMLAstroVolumeNode'), 1)

