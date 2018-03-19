import os
import unittest
import vtk, qt, ctk, slicer
import vtk.util.numpy_support
from slicer.ScriptedLoadableModule import *
from slicer.util import VTKObservationMixin
import logging
from astropy.io.fits import Header as fitsHeader
from astropy import wcs
from reproject import reproject_interp
from reproject import reproject_exact
import numpy

# Utility functions
def astroArrayFromVolume(volumeNode):
  """Return voxel array from volume node as numpy array.
  Voxels values are not copied. Voxel values in the volume node can be modified
  by changing values in the numpy array.
  After all modifications has been completed, call volumeNode.Modified().

  .. warning:: Memory area of the returned array is managed by VTK, therefore
    values in the array may be changed, but the array must not be reallocated
    (change array size, shallow-copy content from other array most likely causes
    application crash). To allow arbitrary numpy operations on a volume array:

      1. Make a deep-copy of the returned VTK-managed array using :func:`numpy.copy`.
      2. Perform any computations using the copied array.
      3. Write results back to the image data using :py:meth:`updateVolumeFromArray`.
  """
  scalarTypes = ['vtkMRMLAstroVolumeNode', 'vtkMRMLAstroLabelMapVolumeNode']
  vimage = volumeNode.GetImageData()
  nshape = tuple(reversed(volumeNode.GetImageData().GetDimensions()))
  narray = None
  if volumeNode.GetClassName() in scalarTypes:
    narray = vtk.util.numpy_support.vtk_to_numpy(vimage.GetPointData().GetScalars()).reshape(nshape)
  return narray


def astroUpdateVolumeFromArray(volumeNode, narray):
  """Sets voxels of a volume node from a numpy array.
  Voxels values are deep-copied, therefore if the numpy array
  is modified after calling this method, voxel values in the volume node will not change.
  Dimensions and data size of the source numpy array does not have to match the current
  content of the volume node.
  """

  vshape = tuple(reversed(narray.shape))
  if len(vshape) == 1:
    narray = numpy.expand_dims(narray, axis=0)
    narray = numpy.expand_dims(narray, axis=0)
    vshape = tuple(reversed(narray.shape))
    vcomponents = 1
  elif len(vshape) == 2:
    narray = numpy.expand_dims(narray, axis=0)
    vshape = tuple(reversed(narray.shape))
    vcomponents = 1
  elif len(vshape) == 3:
    vcomponents = 1
  else:
    raise RuntimeError("Unsupported numpy array shape: "+str(narray.shape))

  vimage = volumeNode.GetImageData()
  if vimage is None:
    vimage = vtk.vtkImageData()
    volumeNode.SetAndObserveImageData(vimage)

  vtype = vtk.util.numpy_support.get_vtk_array_type(narray.dtype)
  vimage.SetDimensions(vshape)
  vimage.AllocateScalars(vtype, vcomponents)

  narrayTarget = astroArrayFromVolume(volumeNode)
  narrayTarget[:] = narray

  # Notify the application that image data is changed
  # (same notifications as in vtkMRMLVolumeNode.SetImageDataConnection)
  imageData = volumeNode.GetImageData()
  pointData = imageData.GetPointData() if imageData else None
  if pointData is not None:
    if pointData.GetScalars() is not None:
      pointData.GetScalars().Modified()

  volumeNode.StorableModified()
  volumeNode.Modified()
  volumeNode.InvokeEvent(slicer.vtkMRMLVolumeNode.ImageDataModifiedEvent, volumeNode)


def parseHeaderFromVolume(volume):
  """Return an astropy header from the a volume node attributes.
  """
  Header = fitsHeader()
  Attributes = vtk.vtkStringArray()
  volume.GetAttributeNames(Attributes)
  Naxis = int(volume.GetAttribute("SlicerAstro.NAXIS"))

  if Naxis == 3:
    for AttributeIndex in range (Attributes.GetNumberOfValues()):
      AttributeKey = Attributes.GetValue(AttributeIndex)
      if AttributeKey.find("CUNIT3") != -1:
        Attribute = volume.GetAttribute(AttributeKey)
        if Attribute.find("km/s") != -1 or \
           Attribute.find("KM/S") != -1 or \
           Attribute.find("Km/s") != -1:
          volume.SetAttribute(AttributeKey, "m/s")
          CRVAL3 = float(volume.GetAttribute("SlicerAstro.CRVAL3"))
          CRVAL3 *= 1000.
          volume.SetAttribute("SlicerAstro.CRVAL3", str(CRVAL3))
          CDELT3 = float(volume.GetAttribute("SlicerAstro.CDELT3"))
          CDELT3 *= 1000.
          volume.SetAttribute("SlicerAstro.CDELT3", str(CDELT3))
    for AttributeIndex in range (Attributes.GetNumberOfValues()):
      AttributeKey = Attributes.GetValue(AttributeIndex)
      if AttributeKey.find("DUNIT3") != -1:
        Attribute = volume.GetAttribute(AttributeKey)
        if Attribute.find("km/s") != -1 or \
           Attribute.find("KM/S") != -1 or \
           Attribute.find("Km/s") != -1:
          volume.SetAttribute(AttributeKey, "m/s")
          DRVAL3 = float(volume.GetAttribute("SlicerAstro.DRVAL3"))
          DRVAL3 *= 1000.
          volume.SetAttribute("SlicerAstro.DRVAL3", str(DRVAL3))

  for AttributeIndex in range (Attributes.GetNumberOfValues()):
    AttributeKey = Attributes.GetValue(AttributeIndex)
    if AttributeKey.find("3DDisplayThreshold") != -1:
      continue
    if AttributeKey.find("3DDisplayThresholdMean") != -1:
      continue
    if AttributeKey.find("DATAMODEL") != -1:
      continue
    if AttributeKey.find("HistoMaxSel") != -1:
      continue
    if AttributeKey.find("HistoMinSel") != -1:
      continue
    if AttributeKey.find("BMAJ") != -1:
      continue
    if AttributeKey.find("BMIN") != -1:
      continue
    if AttributeKey.find("BPA") != -1:
      continue
    if AttributeKey.find("BTYPE") != -1:
      continue
    if AttributeKey.find("BUNIT") != -1:
      continue
    if AttributeKey.find("BZERO") != -1:
      continue
    if AttributeKey.find("CROTA") != -1:
      continue
    if AttributeKey.find("CELLSCAL") != -1:
      continue
    if Naxis == 2 and \
       (AttributeKey.find("NAXIS3") != -1 or \
       AttributeKey.find("CDELT3") != -1 or \
       AttributeKey.find("CROTA3") != -1 or \
       AttributeKey.find("CRPIX3") != -1 or \
       AttributeKey.find("CRVAL3") != -1 or \
       AttributeKey.find("CUNIT3") != -1 or \
       AttributeKey.find("CTYPE3") != -1 or \
       AttributeKey.find("DRVAL3") != -1 or \
       AttributeKey.find("DUNIT3") != -1):
      continue

    if AttributeKey.find("COMMENT") != -1:
      continue
    if AttributeKey.find("HISTORY") != -1:
      continue

    Attribute = volume.GetAttribute(AttributeKey)
    if Attribute is None:
      continue
    if Attribute.find("UNDEFINED") != -1:
      continue

    if (AttributeKey.find("BITPIX") != -1 or \
       AttributeKey.find("NAXIS") != -1 or \
       AttributeKey.find("NAXIS1") != -1 or \
       AttributeKey.find("NAXIS2") != -1 or \
       AttributeKey.find("NAXIS3") != -1 or \
       AttributeKey.find("BITPIX") != -1):
      Attribute = int(Attribute)
    if AttributeKey.find("ALTRPIX") != -1 or \
       AttributeKey.find("BLANK") != -1 or \
       (AttributeKey.find("CD") != -1 and AttributeKey.find("_") != -1) or \
       AttributeKey.find("CDELT1") != -1 or \
       AttributeKey.find("CDELT2") != -1 or \
       AttributeKey.find("CDELT3") != -1 or \
       AttributeKey.find("CROTA1") != -1 or \
       AttributeKey.find("CROTA2") != -1 or \
       AttributeKey.find("CROTA3") != -1 or \
       AttributeKey.find("CRPIX1") != -1 or \
       AttributeKey.find("CRPIX2") != -1 or \
       AttributeKey.find("CRPIX3") != -1 or \
       AttributeKey.find("CRVAL1") != -1 or \
       AttributeKey.find("CRVAL2") != -1 or \
       AttributeKey.find("CRVAL3") != -1 or \
       AttributeKey.find("DATAMAX") != -1 or \
       AttributeKey.find("DATAMIN") != -1 or \
       AttributeKey.find("DRVAL3") != -1 or \
       AttributeKey.find("EQUINOX") != -1 or \
       AttributeKey.find("EPOCH") != -1 or \
       AttributeKey.find("FREQ0") != -1 or \
       AttributeKey.find("RESTFREQ") != -1 or \
       AttributeKey.find("PCDEC") != -1 or \
       AttributeKey.find("PCRA") != -1 and \
       Attribute.find("."):
      Attribute = float(Attribute)

    AttributeKey = AttributeKey[12:]
    AttributeTuple = (AttributeKey, Attribute)
    Header.append(AttributeTuple)

  return Header


def PythonPrintError(error):
  logging.debug(error)
  print error

#
# Astro Reproject
#

class AstroReproject(ScriptedLoadableModule):

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    self.parent.title = "Astro Reproject"
    self.parent.categories = ["Astronomy", "Registration"]
    self.parent.dependencies = []
    self.parent.contributors = ["""
    Davide Punzo (Kapteyn Astronomical Institute) and
    Thijs van der Hulst (Kapteyn Astronomical Institute)."""]
    self.parent.helpText = """
    The AstroReproject module implements image reprojection (resampling) methods
    for astronomical images and more generally n-dimensional data. These assume
    that the WCS information contained in the data are correct. This package does
    not do image registration, which is the process of aligning images where one
    or more images may have incorrect or missing WCS.
    The reprojections are performed using the reproject python module
    (https://github.com/astrofrog/reproject)
    developed by Thomas Robitaille, Christoph Deil and Adam Ginsburg."""
    self.parent.helpText += self.getDefaultModuleDocumentationLink()
    self.parent.acknowledgementText = """
    This module was developed by Davide Punzo. <br>
    This work was supported by ERC grant nr. 291531 and the Slicer Community. <br><br>
    This module uses the following python modules: <br>
    1) astropy (https://github.com/astropy/astropy) <br>
    2) scipy (https://github.com/scipy/scipy) <br>
    3) reproject (https://github.com/astrofrog/reproject) <br>
    """

#
# AstroReprojectWidget
#

class AstroReprojectWidget(ScriptedLoadableModuleWidget, VTKObservationMixin):
  def __init__(self, parent):
    ScriptedLoadableModuleWidget.__init__(self, parent)
    VTKObservationMixin.__init__(self)

    # Members
    self.parametersNode = None
    self.selectionNode = None


  def setup(self):
    ScriptedLoadableModuleWidget.setup(self)

    # input volume selector
    horizontalLayout_1 = qt.QHBoxLayout()
    horizontalLayout_1.setObjectName("horizontalLayout_1");

    self.inputSelectorLabel = qt.QLabel()
    self.inputSelectorLabel.setText("Input Data:")
    self.inputSelectorLabel.setFixedSize(qt.QSize(110, 30))
    horizontalLayout_1.addWidget(self.inputSelectorLabel)

    self.inputSelector = slicer.qMRMLNodeComboBox()
    self.inputSelector.nodeTypes = ["vtkMRMLAstroVolumeNode", "vtkMRMLAstroLabelMapVolumeNode"]
    self.inputSelector.selectNodeUponCreation = True
    self.inputSelector.addEnabled = False
    self.inputSelector.removeEnabled = True
    self.inputSelector.noneEnabled = True
    self.inputSelector.showHidden = False
    self.inputSelector.showChildNodeTypes = False
    self.inputSelector.setMRMLScene(slicer.mrmlScene)
    sizePolicy = qt.QSizePolicy()
    sizePolicy.setHorizontalPolicy(qt.QSizePolicy.Expanding)
    sizePolicy.setVerticalPolicy(qt.QSizePolicy.Fixed)
    sizePolicy.setHorizontalStretch(0)
    sizePolicy.setVerticalStretch(0)
    sizePolicy.setHeightForWidth(self.inputSelector.sizePolicy.hasHeightForWidth())
    self.inputSelector.setMinimumSize(qt.QSize(0, 30))
    horizontalLayout_1.addWidget(self.inputSelector)
    self.layout.addLayout(horizontalLayout_1)

    # reference volume selector
    horizontalLayout_2 = qt.QHBoxLayout()
    horizontalLayout_2.setObjectName("horizontalLayout_2");

    self.referenceSelectorLabel = qt.QLabel()
    self.referenceSelectorLabel.setText("Reference Data:")
    self.referenceSelectorLabel.setFixedSize(qt.QSize(110, 30))
    self.referenceSelectorLabel.setEnabled(False)
    horizontalLayout_2.addWidget(self.referenceSelectorLabel)

    self.referenceSelector = slicer.qMRMLNodeComboBox()
    self.referenceSelector.nodeTypes = ["vtkMRMLAstroVolumeNode", "vtkMRMLAstroLabelMapVolumeNode"]
    self.referenceSelector.addEnabled = False
    self.referenceSelector.removeEnabled = True
    self.referenceSelector.noneEnabled = True
    self.referenceSelector.showHidden = False
    self.referenceSelector.showChildNodeTypes = False
    self.referenceSelector.setMRMLScene(slicer.mrmlScene)
    self.referenceSelector.setEnabled(False)
    sizePolicy.setHeightForWidth(self.referenceSelector.sizePolicy.hasHeightForWidth())
    self.referenceSelector.setMinimumSize(qt.QSize(0, 30))
    horizontalLayout_2.addWidget(self.referenceSelector)
    self.layout.addLayout(horizontalLayout_2)

    # Parameters Area
    self.nodesCollapsibleButton = ctk.ctkCollapsibleButton()
    self.nodesCollapsibleButton.text = "Nodes"
    self.nodesCollapsibleButton.setChecked(False)
    self.nodesCollapsibleButton.setEnabled(False)
    self.layout.addWidget(self.nodesCollapsibleButton)

    # Layout within the dummy collapsible button
    nodesFormLayout = qt.QFormLayout(self.nodesCollapsibleButton)

    # output volume selector
    self.parametersSelector = slicer.qMRMLNodeComboBox()
    self.parametersSelector.nodeTypes = ["vtkMRMLAstroReprojectParametersNode"]
    self.parametersSelector.selectNodeUponCreation = True
    self.parametersSelector.addEnabled = True
    self.parametersSelector.removeEnabled = True
    self.parametersSelector.noneEnabled = True
    self.parametersSelector.showHidden = True
    self.parametersSelector.showChildNodeTypes = False
    self.parametersSelector.setMRMLScene(slicer.mrmlScene)
    nodesFormLayout.addRow("Parameters Node: ", self.parametersSelector)

    # output volume selector
    self.outputSelector = slicer.qMRMLNodeComboBox()
    self.outputSelector.nodeTypes = ["vtkMRMLAstroVolumeNode", "vtkMRMLAstroLabelMapVolumeNode"]
    self.outputSelector.addEnabled = False
    self.outputSelector.removeEnabled = True
    self.outputSelector.noneEnabled = True
    self.outputSelector.showHidden = False
    self.outputSelector.showChildNodeTypes = False
    self.outputSelector.setMRMLScene(slicer.mrmlScene)
    nodesFormLayout.addRow("Output Volume: ", self.outputSelector)

    # Parameters
    self.parametersCollapsibleButton = ctk.ctkCollapsibleButton()
    self.parametersCollapsibleButton.text = "Parameters"
    self.parametersCollapsibleButton.setChecked(True)
    self.parametersCollapsibleButton.setEnabled(False)
    self.layout.addWidget(self.parametersCollapsibleButton)

    # Layout within the dummy collapsible button
    parametersFormLayout = qt.QVBoxLayout(self.parametersCollapsibleButton)
    horizontalLayout_3 = qt.QHBoxLayout()
    horizontalLayout_3.setObjectName("horizontalLayout_3");

    self.SpatialProjectLabel = qt.QLabel()
    self.SpatialProjectLabel.setText("Spatial Reprojection:")
    self.SpatialProjectLabel.setFixedSize(qt.QSize(150, 30))
    horizontalLayout_3.addWidget(self.SpatialProjectLabel)

    self.SpatialInterpolationRadioButton = qt.QRadioButton()
    self.SpatialInterpolationRadioButton.setText("Interpolated")
    sizePolicy.setHeightForWidth(self.SpatialInterpolationRadioButton.sizePolicy.hasHeightForWidth())
    self.SpatialInterpolationRadioButton.setMinimumSize(qt.QSize(0, 30))
    self.SpatialInterpolationRadioButton.setChecked(True)
    horizontalLayout_3.addWidget(self.SpatialInterpolationRadioButton)

    self.SpatialExactRadioButton = qt.QRadioButton()
    self.SpatialExactRadioButton.setText("Exact")
    sizePolicy.setHeightForWidth(self.SpatialExactRadioButton.sizePolicy.hasHeightForWidth())
    self.SpatialExactRadioButton.setMinimumSize(qt.QSize(0, 30))
    self.SpatialExactRadioButton.setChecked(False)
    horizontalLayout_3.addWidget(self.SpatialExactRadioButton)

    parametersFormLayout.addLayout(horizontalLayout_3)

    # Add vertical spacer
    self.layout.addStretch(20)

    # Apply Button
    self.applyButton = qt.QPushButton("Reproject")
    self.applyButton.toolTip = "Reproject the data"
    sizePolicy.setHeightForWidth(self.applyButton.sizePolicy.hasHeightForWidth())
    self.applyButton.setMinimumSize(qt.QSize(0, 30))
    self.applyButton.setEnabled(False)
    self.layout.addWidget(self.applyButton)

    # connections
    self.inputSelector.connect("currentNodeChanged(bool)", self.onInputVolumeActivated)
    self.parametersSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.setMRMLAstroReprojectParametersNode)
    self.inputSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.onInputVolumeChanged)
    self.referenceSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.onReferenceVolumeChanged)
    self.outputSelector.connect("currentNodeChanged(vtkMRMLNode*)", self.onOutputVolumeChanged)
    self.applyButton.connect("clicked(bool)", self.onApplyButton)
    self.SpatialInterpolationRadioButton.connect("clicked(bool)", self.onSpatialInterpolation)
    self.SpatialExactRadioButton.connect("clicked(bool)", self.onSpatialExact)

    # selection node
    self.selectionNode = slicer.app.applicationLogic().GetSelectionNode()

    # initializeNodes
    self.initializeNodes()

    # Connect observers to scene events
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.EndCloseEvent, self.onEndCloseEvent)
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.StartImportEvent, self.onStartImportEvent)
    self.addObserver(slicer.mrmlScene, slicer.mrmlScene.EndImportEvent, self.onEndImportEvent)
    self.selectionNode.AddObserver("ModifiedEvent", self.onMRMLSelectionNodeModified)
    self.onMRMLSelectionNodeModified(self.selectionNode, 2)


  def enter(self):
    LayoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode()
    LayoutNode.SetViewArrangement(LayoutNode.SlicerLayoutDual3DView)


  def onInputVolumeActivated(self, activated):
    self.referenceSelectorLabel.setEnabled(activated)
    self.referenceSelector.setEnabled(activated)
    self.nodesCollapsibleButton.setEnabled(activated)
    self.parametersCollapsibleButton.setEnabled(activated)
    self.applyButton.setEnabled(activated)


  def onEndCloseEvent(self):
    self.initializeNodes(True)
    self.onMRMLAstroReprojectParametersNodeModified(self.parametersNode, 1)


  def onStartImportEvent(self):
    self.selectionNode = None
    self.parametersNode.RemoveAllObservers()
    self.parametersNode = None


  def onEndImportEvent(self):
    self.initializeNodes()
    self.onMRMLAstroReprojectParametersNodeModified(self.parametersNode, 1)


  def onMRMLSelectionNodeModified(self, caller, event):
    if self.selectionNode is None or self.parametersNode is None or \
       self.parametersNode.GetInputVolumeNodeID() == self.selectionNode.GetActiveVolumeID():
      return
    self.parametersNode.SetInputVolumeNodeID(self.selectionNode.GetActiveVolumeID());


  def initializeNodes(self, forceNew = False):
    self.initializeParameterNode(forceNew)


  def initializeParameterNode(self, forceNew = False):
    if slicer.mrmlScene is None or self.selectionNode is None:
      return

    astroParametersNode = None;
    numNodes = slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLAstroReprojectParametersNode")

    if numNodes > 0 and not forceNew:
      astroParametersNode = slicer.mrmlScene.GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroReprojectParametersNode")
    else:
      astroParametersNode = slicer.vtkMRMLAstroReprojectParametersNode()
      astroParametersNode = slicer.mrmlScene.AddNode(astroParametersNode)
      wasModifying = astroParametersNode.StartModify()
      astroParametersNode.SetInputVolumeNodeID(self.selectionNode.GetActiveVolumeID())
      astroParametersNode.EndModify(wasModifying)

    self.parametersSelector.setCurrentNode(astroParametersNode);


  def setMRMLAstroReprojectParametersNode(self, mrmlNode):
    if mrmlNode is None:
      return
    if not mrmlNode.IsA("vtkMRMLAstroReprojectParametersNode"):
      return

    self.parametersNode = mrmlNode
    self.parametersNode.AddObserver("ModifiedEvent", self.onMRMLAstroReprojectParametersNodeModified)
    self.onMRMLAstroReprojectParametersNodeModified(self.parametersNode, 1)


  def onMRMLAstroReprojectParametersNodeModified(self, caller, event):
    if self.parametersNode is None:
      return

    if self.parametersNode.GetInputVolumeNodeID() is not None:
      inputVolume = slicer.mrmlScene.GetNodeByID(self.parametersNode.GetInputVolumeNodeID())
      self.inputSelector.setCurrentNode(inputVolume)

    if self.parametersNode.GetReferenceVolumeNodeID() is not None:
      referenceVolume = slicer.mrmlScene.GetNodeByID(self.parametersNode.GetReferenceVolumeNodeID())
      self.referenceSelector.setCurrentNode(referenceVolume)

    if self.parametersNode.GetOutputVolumeNodeID() is not None:
      outputVolume = slicer.mrmlScene.GetNodeByID(self.parametersNode.GetOutputVolumeNodeID())
      self.outputSelector.setCurrentNode(outputVolume)

    if self.parametersNode.GetSpatialInterpolated():
      self.SpatialInterpolationRadioButton.setChecked(True)
    else:
      self.SpatialExactRadioButton.setChecked(True)


  def onInputVolumeChanged(self, mrmlNode):
    if self.selectionNode is None or self.parametersNode is None:
      return

    if mrmlNode is not None:
      self.selectionNode.SetReferenceActiveVolumeID(mrmlNode.GetID())
      self.selectionNode.SetActiveVolumeID(mrmlNode.GetID())
    else:
      self.selectionNode.SetReferenceActiveVolumeID(None)
      self.selectionNode.SetActiveVolumeID(None)


  def onOutputVolumeChanged(self, mrmlNode):
    if self.parametersNode is None:
      return
    if mrmlNode is not None:
      self.parametersNode.SetOutputVolumeNodeID(mrmlNode.GetID())
    else:
      self.parametersNode.SetOutputVolumeNodeID(None)


  def onReferenceVolumeChanged(self, mrmlNode):
    if self.parametersNode is None:
      return
    if mrmlNode is not None:
      self.parametersNode.SetReferenceVolumeNodeID(mrmlNode.GetID())
    else:
      self.parametersNode.SetReferenceVolumeNodeID(None)

  def onSpatialInterpolation(self, toggled):
    if self.parametersNode is None:
      return

    if toggled:
      self.parametersNode.SetSpatialInterpolated(True)
    else:
      self.parametersNode.SetSpatialInterpolated(False)


  def onSpatialExact(self, toggled):
    if self.parametersNode is None:
      return

    if toggled:
      self.parametersNode.SetSpatialInterpolated(False)
    else:
      self.parametersNode.SetSpatialInterpolated(True)


  def onApplyButton(self):
    if self.parametersNode is None:
      return

    inputVolume = None
    if self.parametersNode.GetInputVolumeNodeID() is not None:
      inputVolume = slicer.mrmlScene.GetNodeByID(self.parametersNode.GetInputVolumeNodeID())
    if inputVolume is None:
      PythonPrintError('AstroReprojectWidget::onApplyButton: no inputVolume node found.')
      return

    referenceVolume = None
    if self.parametersNode.GetReferenceVolumeNodeID() is not None:
      referenceVolume = slicer.mrmlScene.GetNodeByID(self.parametersNode.GetReferenceVolumeNodeID())
    if referenceVolume is None:
      PythonPrintError('AstroReprojectWidget::onApplyButton: no referenceVolume node found.')
      return

    name = inputVolume.GetName()
    name += "_Reprojected_"
    serial = self.parametersNode.GetOutputSerial()
    name += str(serial)
    serial += 1
    self.parametersNode.SetOutputSerial(serial)

    outputVolume = None
    if self.parametersNode.GetOutputVolumeNodeID() is not None:
      outputVolume = slicer.mrmlScene.GetNodeByID(self.parametersNode.GetOutputVolumeNodeID())

    if outputVolume is not None:
      if outputVolume.GetName().find("_Reprojected_") != -1:
        slicer.mrmlScene.RemoveNode(outputVolume.GetStorageNode());
        numDisplays = outputVolume.GetNumberOfDisplayNodes()
        for displayIndex in range (numDisplays):
          displayNode = outputVolume.GetNthDisplayNode(displayIndex)
          if displayNode is not None:
            if displayNode.IsA("vtkMRMLVolumeRenderingDisplayNode"):
              slicer.mrmlScene.RemoveNode(displayNode.GetROINode())
            slicer.mrmlScene.RemoveNode(displayNode)

        slicer.mrmlScene.RemoveNode(outputVolume)

    outputVolume = slicer.modules.astrovolume.logic().CloneVolume(slicer.mrmlScene, inputVolume, name)

    self.parametersNode.SetOutputVolumeNodeID(outputVolume.GetID())

    numDisplays = outputVolume.GetNumberOfDisplayNodes()
    for displayIndex in range (numDisplays):
      displayNode = outputVolume.GetNthDisplayNode(displayIndex)
      if displayNode is not None:
        if displayNode.IsA("vtkMRMLVolumeRenderingDisplayNode"):
          outputVolume.RemoveNthDisplayNodeID(displayIndex)

    transformationMatrix = vtk.vtkMatrix4x4()
    referenceVolume.GetRASToIJKMatrix(transformationMatrix)
    outputVolume.SetRASToIJKMatrix(transformationMatrix)
    outputVolume.SetAndObserveTransformNodeID(referenceVolume.GetTransformNodeID())

    outputAttributes = vtk.vtkStringArray()
    outputVolume.GetAttributeNames(outputAttributes)

    for AttributeIndex in range (outputAttributes.GetNumberOfValues()):
      AttributeKey = outputAttributes.GetValue(AttributeIndex)
      if AttributeKey.find("BTYPE") != -1 or \
         AttributeKey.find("BUNIT") != -1 or \
         AttributeKey.find("DATAMODEL") != -1 or \
         AttributeKey.find("INSTRUME") != -1 or \
         AttributeKey.find("OBJECT") != -1 or \
         AttributeKey.find("OBSERVER") != -1 or \
         AttributeKey.find("TELESCOP") != -1 or \
         AttributeKey.find("NAXIS") != -1:
        continue
      outputVolume.RemoveAttribute(AttributeKey)

    referenceAttributes = vtk.vtkStringArray()
    referenceVolume.GetAttributeNames(referenceAttributes)
    for AttributeIndex in range (referenceAttributes.GetNumberOfValues()):
      AttributeKey = referenceAttributes.GetValue(AttributeIndex)
      if AttributeKey.find("BTYPE") != -1 or \
         AttributeKey.find("BUNIT") != -1 or \
         AttributeKey.find("DATAMODEL") != -1 or \
         AttributeKey.find("INSTRUME") != -1 or \
         AttributeKey.find("OBJECT") != -1 or \
         AttributeKey.find("OBSERVER") != -1 or \
         AttributeKey.find("TELESCOP") != -1 or \
         AttributeKey.find("NAXIS") != -1:
        continue
      AttributeValue = referenceVolume.GetAttribute(AttributeKey)
      outputVolume.SetAttribute(AttributeKey, AttributeValue)

    if outputVolume.IsA("vtkMRMLAstroVolumeNode") and referenceVolume.IsA("vtkMRMLAstroVolumeNode"):
      outputDisplayNode = outputVolume.GetAstroVolumeDisplayNode()
      referenceDisplayNode = referenceVolume.GetAstroVolumeDisplayNode()
      outputDisplayNode.SetFitSlices(referenceDisplayNode.GetFitSlices())
      outputDisplayNode.SetContoursColor(referenceDisplayNode.GetContoursColor())
      outputDisplayNode.SetSpaceQuantities(referenceDisplayNode.GetSpaceQuantities())
      outputDisplayNode.SetSpace(referenceDisplayNode.GetSpace())
      outputDisplayNode.WCSCopy(referenceDisplayNode)
    elif outputVolume.IsA("vtkMRMLAstroVolumeNode") and referenceVolume.IsA("vtkMRMLAstroLabelMapVolumeNode"):
      outputDisplayNode = outputVolume.GetAstroVolumeDisplayNode()
      referenceDisplayNode = referenceVolume.GetAstroLabelMapVolumeDisplayNode()
      outputDisplayNode.SetSpaceQuantities(referenceDisplayNode.GetSpaceQuantities())
      outputDisplayNode.SetSpace(referenceDisplayNode.GetSpace())
      outputDisplayNode.WCSCopy(referenceDisplayNode)
    elif outputVolume.IsA("vtkMRMLAstroLabelMapVolumeNode") and referenceVolume.IsA("vtkMRMLAstroVolumeNode"):
      outputDisplayNode = outputVolume.GetAstroLabelMapVolumeDisplayNode()
      referenceDisplayNode = referenceVolume.GetAstroVolumeDisplayNode()
      outputDisplayNode.SetSpaceQuantities(referenceDisplayNode.GetSpaceQuantities())
      outputDisplayNode.SetSpace(referenceDisplayNode.GetSpace())
      outputDisplayNode.WCSCopy(referenceDisplayNode)
    elif outputVolume.IsA("vtkMRMLAstroLabelMapVolumeNode") and referenceVolume.IsA("vtkMRMLAstroLabelMapVolumeNode"):
      outputDisplayNode = outputVolume.GetAstroLabelMapVolumeDisplayNode()
      referenceDisplayNode = referenceVolume.GetAstroLabelMapVolumeDisplayNode()
      outputDisplayNode.SetSpaceQuantities(referenceDisplayNode.GetSpaceQuantities())
      outputDisplayNode.SetSpace(referenceDisplayNode.GetSpace())
      outputDisplayNode.WCSCopy(referenceDisplayNode)

    logic = AstroReprojectLogic()
    if logic.Reproject(self.parametersNode):
      self.setThreeComparativeView()
    else:
      PythonPrintError('AstroReprojectWidget::onApplyButton: reprojection error.')
      slicer.mrmlScene.RemoveNode(outputVolume)
      self.parametersNode.SetOutputVolumeNodeID(None)
      self.onMRMLAstroReprojectParametersNodeModified(self.parametersNode, 1)


  def setThreeComparativeView(self):
    if self.parametersNode is None:
      return

    inputVolume = None
    if self.parametersNode.GetInputVolumeNodeID() is not None:
      inputVolume = slicer.mrmlScene.GetNodeByID(self.parametersNode.GetInputVolumeNodeID())
    if inputVolume is None:
      PythonPrintError('AstroReprojectWidget::onApplyButton: no inputVolume node found.')
      return

    referenceVolume = None
    if self.parametersNode.GetReferenceVolumeNodeID() is not None:
      referenceVolume = slicer.mrmlScene.GetNodeByID(self.parametersNode.GetReferenceVolumeNodeID())
    if referenceVolume is None:
      PythonPrintError('AstroReprojectWidget::onApplyButton: no referenceVolume node found.')
      return

    outputVolume = None
    if self.parametersNode.GetOutputVolumeNodeID() is not None:
      outputVolume = slicer.mrmlScene.GetNodeByID(self.parametersNode.GetOutputVolumeNodeID())
    if outputVolume is None:
      PythonPrintError('AstroReprojectWidget::onApplyButton: no outputVolume node found.')
      return

    if int(inputVolume.GetAttribute("SlicerAstro.NAXIS")) == 3:
       slicer.modules.astrovolume.widgetRepresentation().setComparative3DViews \
       (self.parametersNode.GetInputVolumeNodeID(), self.parametersNode.GetOutputVolumeNodeID(), False, False)

    redSliceComposite = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceCompositeNodeRed")
    redSliceComposite.SetForegroundVolumeID("")
    redSliceComposite.SetForegroundOpacity(0.)
    if (inputVolume.IsA("vtkMRMLAstroVolumeNode")):
      redSliceComposite.SetBackgroundVolumeID(self.parametersNode.GetInputVolumeNodeID())
      redSliceComposite.SetLabelVolumeID("")
    elif (inputVolume.IsA("vtkMRMLAstroLabelMapVolumeNode")):
      redSliceComposite.SetBackgroundVolumeID("")
      redSliceComposite.SetLabelVolumeID(self.parametersNode.GetInputVolumeNodeID())

    redSliceNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeRed")
    # setting to the XZ orientation is needed in order to force the refresh
    redSliceNode.SetOrientation("XZ")
    redSliceNode.SetOrientation("XY")
    redSliceNode.SetSliceOffset(0.)
    redSliceNode.SetRulerType(redSliceNode.RulerTypeThin)

    redSliceLogic = slicer.app.applicationLogic().GetSliceLogic(redSliceNode)
    dimsSlice = redSliceNode.GetDimensions()
    redSliceLogic.FitSliceToAll(dimsSlice[0], dimsSlice[1])
    redSliceLogic.SnapSliceOffsetToIJK()

    yellowSliceComposite = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceCompositeNodeYellow")
    yellowSliceComposite.SetForegroundVolumeID("")
    yellowSliceComposite.SetForegroundOpacity(0.)
    if (referenceVolume.IsA("vtkMRMLAstroVolumeNode")):
      yellowSliceComposite.SetBackgroundVolumeID(self.parametersNode.GetReferenceVolumeNodeID())
      yellowSliceComposite.SetLabelVolumeID("")
    elif (referenceVolume.IsA("vtkMRMLAstroLabelMapVolumeNode")):
      yellowSliceComposite.SetBackgroundVolumeID("")
      yellowSliceComposite.SetLabelVolumeID(self.parametersNode.GetReferenceVolumeNodeID())

    yellowSliceNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeYellow")
    # setting to the XZ orientation is needed in order to force the refresh
    yellowSliceNode.SetOrientation("XZ")
    yellowSliceNode.SetOrientation("XY")
    yellowSliceNode.SetSliceOffset(0.)
    yellowSliceNode.SetRulerType(yellowSliceNode.RulerTypeThin)

    yellowSliceLogic = slicer.app.applicationLogic().GetSliceLogic(yellowSliceNode)
    dimsSlice = yellowSliceNode.GetDimensions()
    yellowSliceLogic.FitSliceToAll(dimsSlice[0], dimsSlice[1])
    yellowSliceLogic.SnapSliceOffsetToIJK()

    greenSliceComposite = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceCompositeNodeGreen")
    greenSliceComposite.SetForegroundVolumeID("")
    greenSliceComposite.SetForegroundOpacity(0.)
    if (outputVolume.IsA("vtkMRMLAstroVolumeNode")):
      greenSliceComposite.SetBackgroundVolumeID(self.parametersNode.GetOutputVolumeNodeID())
      greenSliceComposite.SetLabelVolumeID("")
    elif (outputVolume.IsA("vtkMRMLAstroLabelMapVolumeNode")):
      greenSliceComposite.SetBackgroundVolumeID("")
      greenSliceComposite.SetLabelVolumeID(self.parametersNode.GetOutputVolumeNodeID())

    greenSliceNode = slicer.mrmlScene.GetNodeByID("vtkMRMLSliceNodeGreen")
    # setting to the XZ orientation is needed in order to force the refresh
    greenSliceNode.SetOrientation("XZ")
    greenSliceNode.SetOrientation("XY")
    greenSliceNode.SetSliceOffset(0.)
    greenSliceNode.SetRulerType(greenSliceNode.RulerTypeThin)

    greenSliceLogic = slicer.app.applicationLogic().GetSliceLogic(greenSliceNode)
    dimsSlice = greenSliceNode.GetDimensions()
    greenSliceLogic.FitSliceToAll(dimsSlice[0], dimsSlice[1])
    greenSliceLogic.SnapSliceOffsetToIJK()



#
# AstroReprojectLogic
#

class AstroReprojectLogic(ScriptedLoadableModuleLogic):

  def hasImageData(self,volumeNode):
    if not volumeNode:
      PythonPrintError('AstroReprojectLogic::hasImageData failed: no volume node')
      return False
    if volumeNode.GetImageData() is None:
      PythonPrintError('AstroReprojectLogic::hasImageData failed: no image data in volume node')
      return False
    return True


  def Reproject(self, pnode):
    inputVolume = None
    if pnode.GetInputVolumeNodeID() is not None:
      inputVolume = slicer.mrmlScene.GetNodeByID(pnode.GetInputVolumeNodeID())
    if not self.hasImageData(inputVolume):
      return False

    referenceVolume = None
    if pnode.GetReferenceVolumeNodeID() is not None:
      referenceVolume = slicer.mrmlScene.GetNodeByID(pnode.GetReferenceVolumeNodeID())
    if not self.hasImageData(referenceVolume):
      return False

    outputVolume = None
    if pnode.GetOutputVolumeNodeID() is not None:
      outputVolume = slicer.mrmlScene.GetNodeByID(pnode.GetOutputVolumeNodeID())
    if not outputVolume:
      PythonPrintError('AstroReprojectLogic::Reproject: outputVolume not found.')
      return False

    inputArray = astroArrayFromVolume(inputVolume)
    inputArray = numpy.squeeze(inputArray)
    InputHeader = parseHeaderFromVolume(inputVolume)
    InputWCS = wcs.WCS(InputHeader)
    if int(inputVolume.GetAttribute("SlicerAstro.NAXIS")) == 3:
      InputWCS.wcs.sptr('VRAD-???', 2)
    InputHDU = (inputArray, InputWCS)

    ReferenceHeader = parseHeaderFromVolume(referenceVolume)
    ReferenceWCS = wcs.WCS(ReferenceHeader)
    if int(referenceVolume.GetAttribute("SlicerAstro.NAXIS")) == 3:
      ReferenceWCS.wcs.sptr('VRAD-???', 2)

    outputShape = referenceVolume.GetImageData().GetDimensions()
    outputShape = tuple(x for x in outputShape if x > 1)
    outputShape = outputShape[::-1]
    outputArray = None
    if pnode.GetSpatialInterpolated():
      try:
        outputArray, footprint = reproject_interp(InputHDU, ReferenceWCS, shape_out = outputShape)
      except ValueError as error:
        PythonPrintError("AstroReprojectLogic::Reproject: reproject_interp error : ")
        PythonPrintError(error)
        return False
    else:
      try:
        outputArray, footprint = reproject_exact(InputHDU, ReferenceWCS, shape_out = outputShape, parallel = False)
      except ValueError:
        PythonPrintError("AstroReprojectLogic::Reproject: reproject_exact error : ")
        PythonPrintError(error)
        return False

    if outputArray is not None:
      astroUpdateVolumeFromArray(outputVolume, outputArray)
      outputVolume.UpdateRangeAttributes()
      if (outputVolume.IsA("vtkMRMLAstroVolumeNode")):
        outputVolume.Update3DDisplayThresholdAttributes()
    else:
      return False

    return True

#
# AstroReprojectTest
#

class AstroReprojectTest(ScriptedLoadableModuleTest):

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self):
    """Run as few or as many tests as needed here.
    """
    self.setUp()

