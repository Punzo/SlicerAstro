from __future__ import print_function
import os
import unittest
import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
import logging
import textwrap

def downloadFromURL(uris=None, fileNames=None, nodeNames=None, loadFiles=None,
  customDownloader=None, loadFileTypes=None, loadFileProperties={}):
  """Download and optionally load data into the application.
  :param uris: Download URL(s).
  :param fileNames: File name(s) that will be downloaded (and loaded).
  :param nodeNames: Node name(s) in the scene.
  :param loadFiles: Boolean indicating if file(s) should be loaded. By default, the function decides.
  :param customDownloader: Custom function for downloading.
  :param loadFileTypes: file format name(s) ('VolumeFile' by default).
  :param loadFileProperties: custom properties passed to the IO plugin.
  If the given ``fileNames`` are not found in the application cache directory, they
  are downloaded using the associated URIs.
  See ``slicer.mrmlScene.GetCacheManager().GetRemoteCacheDirectory()``
  If not explicitly provided or if set to ``None``, the ``loadFileTypes`` are
  guessed based on the corresponding filename extensions.
  If a given fileName has the ``.mrb`` or ``.mrml`` extension, it will **not** be loaded
  by default. To ensure the file is loaded, ``loadFiles`` must be set.
  The ``loadFileProperties`` are common for all files. If different properties
  need to be associated with files of different types, downloadFromURL must
  be called for each.
  """
  return SampleDataLogic().downloadFromURL(
    uris, fileNames, nodeNames, loadFiles, customDownloader, loadFileTypes, loadFileProperties)

def downloadSample(sampleName):
  """For a given sample name this will search the available sources
  and load it if it is available.  Returns the first loaded node."""
  return SampleDataLogic().downloadSamples(sampleName)[0]

def downloadSamples(sampleName):
  """For a given sample name this will search the available sources
  and load it if it is available.  Returns the loaded nodes."""
  return SampleDataLogic().downloadSamples(sampleName)

#
# AstroSampleData
#

class AstroSampleData(ScriptedLoadableModule):
  """Uses ScriptedLoadableModule base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self, parent):
    ScriptedLoadableModule.__init__(self, parent)
    self.parent.title = "Astro Sample Data"
    self.parent.categories = ["Astronomy"]
    self.parent.dependencies = ["AstroVolume"]
    self.parent.contributors = ["""
    Davide Punzo (Kapteyn Astronomical Institute),
    Thijs van der Hulst (Kapteyn Astronomical Institute) and
    Jos Roerdink (Johann Bernoulli Institute)."""]
    self.parent.helpText = """
    The AstroSampleData module can be used to download data for working with in SlicerAstro.
    Use of this module requires an active network connection."""
    self.parent.acknowledgementText = """
    This module was developed by Davide Punzo. <br>
    This work was supported by ERC grant nr. 291531 and the Slicer Community. <br><br>
    Data acknowledgement: <br>
    WEIN069: Mpati Ramatsoku and Marc Verheijen (Kapteyn Astronomical Institute); <br>
    WEIN069_MASK: mask generated using SoFiA (https://github.com/SoFiA-Admin/SoFiA); <br>
    NGC2403: THING survey; <br>
    NGC2403_DSS: optical image from DSS; <br>
    NGC3379 and NGC4111: ATLAS3D survey. <br>
    This file has been originally edited by Steve Pieper.
    """
    self.parent.icon = qt.QIcon(':Icons/XLarge/NGC2841.png')
    self.parent = parent


    if slicer.mrmlScene.GetTagByClassName( "vtkMRMLScriptedModuleNode" ) != 'ScriptedModule':
      slicer.mrmlScene.RegisterNodeClass(vtkMRMLScriptedModuleNode())

    # Trigger the menu to be added when application has started up
    if not slicer.app.commandOptions().noMainWindow :
      slicer.app.connect("startupCompleted()", self.addMenu)

    # allow other modules to register sample data sources by appending
    # instances or subclasses SampleDataSource objects on this list
    try:
      slicer.modules.sampleDataSources
    except AttributeError:
      slicer.modules.sampleDataSources = {}

  def addMenu(self):
    actionIcon = self.parent.icon
    a = qt.QAction(actionIcon, 'Download Sample Data', slicer.util.mainWindow())
    a.setToolTip('Go to the SampleData module to download data from the network')
    a.connect('triggered()', self.select)

    fileMenu = slicer.util.lookupTopLevelWidget('FileMenu')
    if fileMenu:
      for action in fileMenu.actions():
        if action.text == 'Save':
          fileMenu.insertAction(action,a)


  def select(self):
    m = slicer.util.mainWindow()
    m.moduleSelector().selectModule('AstroSampleData')

#
# AstroSampleDataSource
#
class AstroSampleDataSource:
  """
  Describe a set of sample data associated with one or multiple URIs and filenames.
  """

  def __init__(self, sampleName=None, sampleDescription=None, uris=None, fileNames=None, nodeNames=None, loadFiles=None,
    customDownloader=None, thumbnailFileName=None,
    loadFileType=None, loadFileProperties={}):
    """
    :param sampleName: Name identifying the data set.
    :param sampleDescription: Displayed name of data set in SampleData module GUI. (default is ``sampleName``)
    :param thumbnailFileName: Displayed thumbnail of data set in SampleData module GUI,
    :param uris: Download URL(s).
    :param fileNames: File name(s) that will be downloaded (and loaded).
    :param nodeNames: Node name(s) in the scene.
    :param loadFiles: Boolean indicating if file(s) should be loaded.
    :param customDownloader: Custom function for downloading.
    :param loadFileType: file format name(s) ('VolumeFile' by default if node name is specified).
    :param loadFileProperties: custom properties passed to the IO plugin.
    """
    self.sampleName = sampleName
    if sampleDescription is None:
      sampleDescription = sampleName
    self.sampleDescription = sampleDescription
    if (isinstance(uris, list) or isinstance(uris, tuple)):
      if isinstance(loadFileType, str) or loadFileType is None:
        loadFileType = [loadFileType] * len(uris)
      if nodeNames is None:
        nodeNames = [None] * len(uris)
      if loadFiles is None:
        loadFiles = [None] * len(uris)
    elif isinstance(uris, str):
      uris = [uris,]
      fileNames = [fileNames,]
      nodeNames = [nodeNames,]
      loadFiles = [loadFiles,]
      loadFileType = [loadFileType,]

    updatedFileType = []
    for fileName, nodeName, fileType in zip(fileNames, nodeNames, loadFileType):
      # If not explicitly specified, attempt to guess fileType
      if fileType is None:
        if nodeName is not None:
          # TODO: Use method from Slicer IO logic ?
          fileType = "VolumeFile"
        else:
          ext = os.path.splitext(fileName.lower())[1]
          if ext in [".mrml", ".mrb"]:
            fileType = "SceneFile"
          elif ext in [".zip"]:
            fileType = "ZipFile"
      updatedFileType.append(fileType)

    self.uris = uris
    self.fileNames = fileNames
    self.nodeNames = nodeNames
    self.loadFiles = loadFiles
    self.customDownloader = customDownloader
    self.thumbnailFileName = thumbnailFileName
    self.loadFileType = updatedFileType
    self.loadFileProperties = loadFileProperties
    if not len(uris) == len(fileNames) == len(nodeNames) == len(loadFiles) == len(updatedFileType):
      raise Exception("All fields of sample data source must have the same length")

  def __str__(self):
     output = [
       "sampleName        : %s" % self.sampleName,
       "sampleDescription : %s" % self.sampleDescription,
       "thumbnailFileName : %s" % self.thumbnailFileName,
       "loadFileProperties: %s" % self.loadFileProperties,
       "customDownloader  : %s" % self.customDownloader,
       ""
     ]
     for fileName, uri, nodeName, loadFile, fileType in zip(self.fileNames, self.uris, self.nodeNames, self.loadFiles, self.loadFileType):
       output.extend([
         "fileName    : %s" % fileName,
         "uri         : %s" % uri,
         "nodeName    : %s" % nodeName,
         "loadFile    : %s" % loadFile,
         "loadFileType: %s" % fileType,
         ""
       ])
     return "\n".join(output)

#
# SampleData widget
#

class AstroSampleDataWidget(ScriptedLoadableModuleWidget):
  """Uses ScriptedLoadableModuleWidget base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def setup(self):
    ScriptedLoadableModuleWidget.setup(self)

    # This module is often used in developer mode, therefore
    # collapse reload & test section by default.
    if hasattr(self, "reloadCollapsibleButton"):
      self.reloadCollapsibleButton.collapsed = True

    self.observerTags = []
    self.logic = AstroSampleDataLogic(self.logMessage)

    numberOfColumns = 3
    iconPath = os.path.join(os.path.dirname(__file__).replace('\\','/'), 'Resources','Icons')
    desktop = qt.QDesktopWidget()
    mainScreenSize = desktop.availableGeometry(desktop.primaryScreen)
    iconSize = qt.QSize(mainScreenSize.width()/15,mainScreenSize.height()/10)

    categories = sorted(slicer.modules.sampleDataSources.keys())
    if self.logic.builtInCategoryName in categories:
      categories.remove(self.logic.builtInCategoryName)
    if 'AstronomicalData' in categories:
        categories.remove('AstronomicalData')
    categories.insert(0, 'AstronomicalData')
    for category in categories:
      if category == self.logic.developmentCategoryName and self.developerMode is False:
        continue
      frame = ctk.ctkCollapsibleGroupBox(self.parent)
      self.layout.addWidget(frame)
      frame.title = category
      frame.name = '%sCollapsibleGroupBox' % category
      layout = qt.QGridLayout(frame)
      columnIndex = 0
      rowIndex = 0
      for source in slicer.modules.sampleDataSources[category]:
        name = source.sampleDescription
        if not name:
          name = source.nodeNames[0]

        b = qt.QToolButton()
        b.setText(name)

        # Set thumbnail
        if source.thumbnailFileName:
          # Thumbnail provided
          thumbnailImage = source.thumbnailFileName
        else:
          # Look for thumbnail image with the name of any node name with .png extension
          thumbnailImage = None
          for nodeName in source.nodeNames:
            if not nodeName:
              continue
            thumbnailImageAttempt = os.path.join(iconPath, nodeName+'.png')
            if os.path.exists(thumbnailImageAttempt):
              thumbnailImage = thumbnailImageAttempt
              break
        if thumbnailImage and os.path.exists(thumbnailImage):
          b.setIcon(qt.QIcon(thumbnailImage))

        b.setIconSize(iconSize)
        b.setToolButtonStyle(qt.Qt.ToolButtonTextUnderIcon)
        qSize = qt.QSizePolicy()
        qSize.setHorizontalPolicy(qt.QSizePolicy.Expanding)
        b.setSizePolicy(qSize)

        b.name = '%sPushButton' % name
        layout.addWidget(b, rowIndex, columnIndex)
        columnIndex += 1
        if columnIndex==numberOfColumns:
          rowIndex += 1
          columnIndex = 0
        if source.customDownloader:
          b.connect('clicked()', source.customDownloader)
        else:
          b.connect('clicked()', lambda s=source: self.logic.downloadFromSource(s))

    self.log = qt.QTextEdit()
    self.log.readOnly = True
    self.layout.addWidget(self.log)
    self.logMessage('<p>Status: <i>Idle</i>')

    # Add spacer to layout
    self.layout.addStretch(1)

  def logMessage(self, message, logLevel=logging.INFO):
    # Set text color based on log level
    if logLevel >= logging.ERROR:
      message = '<font color="red">' + message + '</font>'
    elif logLevel >= logging.WARNING:
      message = '<font color="orange">' + message + '</font>'
    # Show message in status bar
    doc = qt.QTextDocument()
    doc.setHtml(message)
    slicer.util.showStatusMessage(doc.toPlainText(),3000)
    # Show message in log window at the bottom of the module widget
    self.log.insertHtml(message)
    self.log.insertPlainText('\n')
    self.log.ensureCursorVisible()
    self.log.repaint()
    logging.log(logLevel, message)
    slicer.app.processEvents(qt.QEventLoop.ExcludeUserInputEvents)

#
# SampleData logic
#

class AstroSampleDataLogic:
  """Manage the slicer.modules.sampleDataSources dictionary.
  The dictionary keys are categories of sample data sources.
  The BuiltIn category is managed here.  Modules or extensions can
  register their own sample data by creating instances of the
  AstroSampleDataSource class.  These instances should be stored in a
  list that is assigned to a category following the model
  used in registerBuiltInSampleDataSources below.
  """

  @staticmethod
  def registerCustomSampleDataSource(category='Custom',
    sampleName=None, uris=None, fileNames=None, nodeNames=None,
    customDownloader=None, thumbnailFileName=None,
    loadFileType='VolumeFile', loadFiles=None, loadFileProperties={}):
    """Adds custom data sets to SampleData.
    :param category: Section title of data set in SampleData module GUI.
    :param sampleName: Displayed name of data set in SampleData module GUI.
    :param thumbnailFileName: Displayed thumbnail of data set in SampleData module GUI,
    :param uris: Download URL(s).
    :param fileNames: File name(s) that will be loaded.
    :param nodeNames: Node name(s) in the scene.
    :param customDownloader: Custom function for downloading.
    :param loadFileType: file format name(s) ('VolumeFile' by default).
    :param loadFiles: Boolean indicating if file(s) should be loaded. By default, the function decides.
    :param loadFileProperties: custom properties passed to the IO plugin.
    """

    try:
      slicer.modules.sampleDataSources
    except AttributeError:
      slicer.modules.sampleDataSources = {}

    if category not in slicer.modules.sampleDataSources:
      slicer.modules.sampleDataSources[category] = []

    slicer.modules.sampleDataSources[category].append(AstroSampleDataSource(
      sampleName=sampleName,
      uris=uris,
      fileNames=fileNames,
      nodeNames=nodeNames,
      thumbnailFileName=thumbnailFileName,
      loadFileType=loadFileType,
      loadFiles=loadFiles,
      loadFileProperties=loadFileProperties
      ))

  def __init__(self, logMessage=None):
    if logMessage:
      self.logMessage = logMessage
    self.builtInCategoryName = 'BuiltIn'
    self.developmentCategoryName = 'Development'
    self.registerBuiltInAstroSampleDataSources()

  def registerBuiltInAstroSampleDataSources(self):
    """Fills in the pre-define sample data sources"""
    sourceArguments = (
        ('WEIN069', None, 'http://slicer.kitware.com/midas3/download/item/337752/WEIN069.fits', 'WEIN069.fits', 'WEIN069'),
        ('WEIN069_MASK', None, 'http://slicer.kitware.com/midas3/download/item/266403/WEIN069_mask.fits', 'WEIN069_mask.fits', 'WEIN069_mask'),
        ('NGC2403_DSS', None, 'http://slicer.kitware.com/midas3/download/item/365486/NGC2403_DSS.fits', 'NGC2403_DSS.fits', 'NGC2403_DSS'),
        ('NGC2403', None, 'http://slicer.kitware.com/midas3/download/item/359776/NGC2403.fits+%281%29', 'NGC2403.fits', 'NGC2403'),
        ('NGC4111', None, 'http://slicer.kitware.com/midas3/download/item/242880/NGC4111.fits', 'NGC4111.fits', 'NGC4111'),
        ('NGC3379', None, 'http://slicer.kitware.com/midas3/download/item/242866/NGC3379.fits', 'NGC3379.fits', 'NGC3379'),
        )

    if 'AstronomicalData' not in slicer.modules.sampleDataSources:
      slicer.modules.sampleDataSources['AstronomicalData'] = []
    for sourceArgument in sourceArguments:
      slicer.modules.sampleDataSources['AstronomicalData'].append(AstroSampleDataSource(*sourceArgument))

  def downloadFileIntoCache(self, uri, name):
    """Given a uri and and a filename, download the data into
    a file of the given name in the scene's cache"""
    destFolderPath = slicer.mrmlScene.GetCacheManager().GetRemoteCacheDirectory()

    if not os.access(destFolderPath, os.W_OK):
      try:
        os.mkdir(destFolderPath)
      except:
        self.logMessage('<b>Failed to create cache folder %s</b>' % destFolderPath, logging.ERROR)
      if not os.access(destFolderPath, os.W_OK):
        self.logMessage('<b>Cache folder %s is not writable</b>' % destFolderPath, logging.ERROR)
    return self.downloadFile(uri, destFolderPath, name)

  def downloadSourceIntoCache(self, source):
    """Download all files for the given source and return a
    list of file paths for the results"""
    filePaths = []
    for uri,fileName in zip(source.uris,source.fileNames):
      filePaths.append(self.downloadFileIntoCache(uri, fileName))
    return filePaths

  def downloadFromSource(self,source,attemptCount=0):
    """Given an instance of AstroSampleDataSource, downloads the associated data and
    load them into Slicer if it applies.
    The function always returns a list.
    Based on the fileType(s), nodeName(s) and loadFile(s) associated with
    the source, different values may be appended to the returned list:
      - if nodeName is specified, appends loaded nodes but if ``loadFile`` is False appends downloaded filepath
      - if fileType is ``SceneFile``, appends downloaded filepath
      - if fileType is ``ZipFile``, appends directory of extracted archive but if ``loadFile`` is False appends downloaded filepath
    If no ``nodeNames`` and no ``fileTypes`` are specified or if ``loadFiles`` are all False,
    returns the list of all downloaded filepaths.
    """
    nodes = []
    filePaths = []

    for uri,fileName,nodeName,loadFile,loadFileType in zip(source.uris,source.fileNames,source.nodeNames,source.loadFiles,source.loadFileType):

      current_source = AstroSampleDataSource(uris=uri, fileNames=fileName, nodeNames=nodeName, loadFiles=loadFile, loadFileType=loadFileType, loadFileProperties=source.loadFileProperties)
      filePath = self.downloadFileIntoCache(uri, fileName)
      filePaths.append(filePath)

      if loadFileType == 'ZipFile':
        if loadFile == False:
          nodes.append(filePath)
          continue
        outputDir = slicer.mrmlScene.GetCacheManager().GetRemoteCacheDirectory() + "/" + os.path.splitext(os.path.basename(filePath))[0]
        qt.QDir().mkpath(outputDir)
        success = slicer.util.extractArchive(filePath, outputDir)
        if not success and attemptCount < 5:
          file = qt.QFile(filePath)
          if not file.remove():
            self.logMessage('<b>Load failed! Unable to delete and try again loading %s!</b>' % filePath, logging.ERROR)
            nodes.append(None)
            break
          attemptCount += 1
          self.logMessage('<b>Load failed! Trying to download again (%d of 5 attempts)...</b>' % (attemptCount), logging.ERROR)
          outputDir = self.downloadFromSource(current_source,attemptCount)[0]
        nodes.append(outputDir)

      elif loadFileType == 'SceneFile':
        if not loadFile:
          nodes.append(filePath)
          continue
        success = self.loadScene(filePath, source.loadFileProperties)
        if not success and attemptCount < 5:
          file = qt.QFile(filePath)
          if not file.remove():
            self.logMessage('<b>Load failed! Unable to delete and try again loading %s!</b>' % filePath, logging.ERROR)
            nodes.append(None)
            break
          attemptCount += 1
          self.logMessage('<b>Load failed! Trying to download again (%d of 5 attempts)...</b>' % (attemptCount), logging.ERROR)
          filePath = self.downloadFromSource(current_source,attemptCount)[0]
        nodes.append(filePath)

      elif nodeName:
        if loadFile == False:
          nodes.append(filePath)
          continue
        loadedNode = self.loadNode(filePath, nodeName, loadFileType, source.loadFileProperties)
        if loadedNode is None and attemptCount < 5:
          file = qt.QFile(filePath)
          if not file.remove():
            self.logMessage('<b>Load failed! Unable to delete and try again loading %s!</b>' % filePath, logging.ERROR)
            nodes.append(None)
            break
          attemptCount += 1
          self.logMessage('<b>Load failed! Trying to download again (%d of 5 attempts)...</b>' % (attemptCount), logging.ERROR)
          loadedNode = self.downloadFromSource(current_source,attemptCount)[0]
        nodes.append(loadedNode)

    if nodes:
      return nodes
    else:
      return filePaths

  def sourceForSampleName(self,sampleName):
      """For a given sample name this will search the available sources.
      Returns SampleDataSource instance."""
      for category in slicer.modules.sampleDataSources.keys():
        for source in slicer.modules.sampleDataSources[category]:
          if sampleName == source.sampleName:
            return source
      return None

  def downloadFromURL(self, uris=None, fileNames=None, nodeNames=None, loadFiles=None,
    customDownloader=None, loadFileTypes=None, loadFileProperties={}):
    """Download and optionally load data into the application.
    :param uris: Download URL(s).
    :param fileNames: File name(s) that will be downloaded (and loaded).
    :param nodeNames: Node name(s) in the scene.
    :param loadFiles: Boolean indicating if file(s) should be loaded. By default, the function decides.
    :param customDownloader: Custom function for downloading.
    :param loadFileTypes: file format name(s) ('VolumeFile' by default).
    :param loadFileProperties: custom properties passed to the IO plugin.
    If the given ``fileNames`` are not found in the application cache directory, they
    are downloaded using the associated URIs.
    See ``slicer.mrmlScene.GetCacheManager().GetRemoteCacheDirectory()``
    If not explicitly provided or if set to ``None``, the ``loadFileTypes`` are
    guessed based on the corresponding filename extensions.
    If a given fileName has the ``.mrb`` or ``.mrml`` extension, it will **not** be loaded
    by default. To ensure the file is loaded, ``loadFiles`` must be set.
    The ``loadFileProperties`` are common for all files. If different properties
    need to be associated with files of different types, downloadFromURL must
    be called for each.
    """
    return self.downloadFromSource(AstroSampleDataSource(
      uris=uris, fileNames=fileNames, nodeNames=nodeNames, loadFiles=loadFiles,
      loadFileType=loadFileTypes, loadFileProperties=loadFileProperties
    ))

  def downloadSample(self,sampleName):
    """For a given sample name this will search the available sources
    and load it if it is available.  Returns the first loaded node."""
    return self.downloadSamples(sampleName)[0]

  def downloadSamples(self,sampleName):
    """For a given sample name this will search the available sources
    and load it if it is available.  Returns the loaded nodes."""
    source = self.sourceForSampleName(sampleName)
    nodes = []
    if source:
      nodes = self.downloadFromSource(source)
    return nodes

  def logMessage(self,message):
    print(message)

  def humanFormatSize(self,size):
    """ from http://stackoverflow.com/questions/1094841/reusable-library-to-get-human-readable-version-of-file-size"""
    for x in ['bytes','KB','MB','GB']:
      if size < 1024.0 and size > -1024.0:
        return "%3.1f %s" % (size, x)
      size /= 1024.0
    return "%3.1f %s" % (size, 'TB')

  def reportHook(self,blocksSoFar,blockSize,totalSize):
    # we clamp to 100% because the blockSize might be larger than the file itself
    percent = min(int((100. * blocksSoFar * blockSize) / totalSize), 100)
    if percent == 100 or (percent - self.downloadPercent >= 10):
      # we clamp to totalSize when blockSize is larger than totalSize
      humanSizeSoFar = self.humanFormatSize(min(blocksSoFar * blockSize, totalSize))
      humanSizeTotal = self.humanFormatSize(totalSize)
      self.logMessage('<i>Downloaded %s (%d%% of %s)...</i>' % (humanSizeSoFar, percent, humanSizeTotal))
      self.downloadPercent = percent

  def downloadFile(self, uri, destFolderPath, name):
    filePath = destFolderPath + '/' + name
    if not os.path.exists(filePath) or os.stat(filePath).st_size == 0:
      import urllib.request, urllib.parse, urllib.error
      self.logMessage('<b>Requesting download</b> <i>%s</i> from %s...' % (name, uri))
      # add a progress bar
      self.downloadPercent = 0
      try:
        urllib.request.urlretrieve(uri, filePath, self.reportHook)
        self.logMessage('<b>Download finished</b>')
      except IOError as e:
        self.logMessage('<b>\tDownload failed: %s</b>' % e, logging.ERROR)
    else:
      self.logMessage('<b>File already exists in cache - reusing it.</b>')
    return filePath

  def loadScene(self, uri,  fileProperties = {}):
    self.logMessage('<b>Requesting load</b> %s...' % uri)
    fileProperties['fileName'] = uri
    success = slicer.app.coreIOManager().loadNodes('SceneFile', fileProperties)
    if not success:
      self.logMessage('<b>\tLoad failed!</b>', logging.ERROR)
      return False
    self.logMessage('<b>Load finished</b>')
    return True

  def loadNode(self, uri, name, fileType = 'VolumeFile', fileProperties = {}):
    self.logMessage('<b>Requesting load</b> <i>%s</i> from %s...' % (name, uri))

    fileProperties['fileName'] = uri
    fileProperties['name'] = name
    fileProperties['center'] = True
    if "mask" in name:
      fileProperties['labelmap'] = True
    firstLoadedNode = None
    loadedNodes = vtk.vtkCollection()
    success = slicer.app.coreIOManager().loadNodes(fileType, fileProperties, loadedNodes)

    if not success or loadedNodes.GetNumberOfItems()<1:
      self.logMessage('<b>\tLoad failed!</b>', logging.ERROR)
      return None

    self.logMessage('<b>Load finished</b>')

    # since nodes were read from a temp directory remove the storage nodes
    for i in range(loadedNodes.GetNumberOfItems()):
      loadedNode = loadedNodes.GetItemAsObject(i)
      if not loadedNode.IsA("vtkMRMLStorableNode"):
        continue
      storageNode = loadedNode.GetStorageNode()
      if not storageNode:
        continue
      slicer.mrmlScene.RemoveNode(storageNode)
      loadedNode.SetAndObserveStorageNodeID(None)

    return loadedNodes.GetItemAsObject(0)
