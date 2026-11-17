"""Outputs of the SlicerAstro analysis modules for the comparison with
independent implementations (compare.py: spectral-cube, astropy, radio-beam,
casacore).

Run in 3D Slicer with SlicerAstro:

    SlicerWithSlicerAstro --no-splash --python-script slicer_outputs.py <output folder>

For WEIN069 (with its SoFiA mask) and NGC 2403 (with a mask of its emission
made here and saved as a FITS mask), the script computes through the module
widgets: moment maps without mask (3 sigma threshold) and with a mask
segment, integrated spectra (AstroProfiles) without and with the mask, and
statistics in the mask (AstroStatistics). The outputs are written as FITS
files and a JSON file with the parameters and the statistics.
"""

import json
import os
import sys

import numpy as np

import slicer


def voxelArray(volumeNode):
    """(z, y, x) view on the voxels of a volume node (also astronomical ones)."""
    from vtk.util.numpy_support import vtk_to_numpy
    imageData = volumeNode.GetImageData()
    dims = imageData.GetDimensions()
    return vtk_to_numpy(imageData.GetPointData().GetScalars()).reshape(dims[2], dims[1], dims[0])


def selectModule(name):
    slicer.util.mainWindow().moduleSelector().selectModule(name)
    slicer.app.processEvents()
    return getattr(slicer.modules, name.lower()).widgetRepresentation()


def selectMask(widget, segmentationNode, segmentID, checkBoxName="MaskCheckBox"):
    if checkBoxName:
        # panels rebuilt with the SlicerAstro widgets: segmented "Threshold/Cube | Mask"
        segmented = [w for w in (slicer.util.findChildren(widget, "VoxelSelectionButton") +
                                 slicer.util.findChildren(widget, "RegionButton"))]
        if segmented:
            segmented[0].setCurrentIndex(1 if segmentationNode is not None else 0)
        else:
            slicer.util.findChild(widget, checkBoxName).setChecked(segmentationNode is not None)
    else:
        slicer.util.findChild(widget, "SegmentationModeRadioButton").click()
    if segmentationNode is None:
        return
    slicer.util.findChild(widget, "SegmentationNodeComboBox").setCurrentNode(segmentationNode)
    slicer.app.processEvents()
    slicer.util.findChild(widget, "SegmentsTableView").setSelectedSegmentIDs([segmentID])
    slicer.app.processEvents()


def momentMaps(volumeNode, segmentationNode, segmentID, prefix, outputFolder, result):
    widget = selectModule("AstroMomentMaps")
    parametersNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroMomentMapsParametersNode")
    parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
    slicer.app.processEvents()
    parametersNode.SetGenerateZero(True)
    parametersNode.SetGenerateFirst(True)
    parametersNode.SetGenerateSecond(True)
    if segmentationNode is None:
        parametersNode.SetIntensityMin(3. * volumeNode.GetDisplayThreshold())
    selectMask(widget, segmentationNode, segmentID)
    slicer.util.findChild(widget, "ApplyButton").click()
    files = {}
    for moment, nodeID in (("m0", parametersNode.GetZeroMomentVolumeNodeID()),
                           ("m1", parametersNode.GetFirstMomentVolumeNodeID()),
                           ("m2", parametersNode.GetSecondMomentVolumeNodeID())):
        path = os.path.join(outputFolder, f"{prefix}_{moment}.fits")
        slicer.util.saveNode(slicer.mrmlScene.GetNodeByID(nodeID), path)
        files[moment] = os.path.basename(path)
    result["moments"] = {"files": files, "intensityMin": parametersNode.GetIntensityMin(),
                         "intensityMax": parametersNode.GetIntensityMax(),
                         "velocityMin": parametersNode.GetVelocityMin(),
                         "velocityMax": parametersNode.GetVelocityMax()}
    selectMask(widget, None, None)


def profile(volumeNode, segmentationNode, segmentID, prefix, outputFolder, result, aperture=False, key="profile"):
    widget = selectModule("AstroProfiles")
    parametersNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroProfilesParametersNode")
    parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
    slicer.app.processEvents()
    if segmentationNode is None:
        parametersNode.SetIntensityMin(3. * volumeNode.GetDisplayThreshold())
    selectMask(widget, segmentationNode, segmentID)
    if segmentationNode is not None:
        slicer.util.findChild(widget, "MaskApertureCheckBox").setChecked(aperture)
    slicer.util.findChild(widget, "ApplyButton").click()
    profileNode = slicer.mrmlScene.GetNodeByID(parametersNode.GetProfileVolumeNodeID())
    values = voxelArray(profileNode).ravel().astype(np.float64)
    result[key] = {"values": [None if np.isnan(v) else float(v) for v in values],
                         "intensityMin": parametersNode.GetIntensityMin(),
                         "intensityMax": parametersNode.GetIntensityMax()}
    selectMask(widget, None, None)


def statistics(volumeNode, segmentationNode, segmentID, result):
    widget = selectModule("AstroStatistics")
    parametersNode = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroStatisticsParametersNode")
    parametersNode.SetInputVolumeNodeID(volumeNode.GetID())
    for name in ("Max", "Min", "Npixels", "Sum", "Mean", "Std", "Median", "TotalFlux"):
        getattr(parametersNode, "Set" + name)(True)
    selectMask(widget, segmentationNode, segmentID, checkBoxName=None)
    slicer.util.findChild(widget, "ApplyButton").click()
    table = parametersNode.GetTableNode().GetTable()
    row = table.GetNumberOfRows() - 1
    result["statistics"] = {name: table.GetValueByName(row, name).ToDouble()
                            for name in ("Npixels", "Min", "Max", "Mean", "Std", "Median", "Sum", "TotalFlux")}


def largestSegment(segmentationNode, volumeNode):
    import vtk
    from vtk.util.numpy_support import vtk_to_numpy
    sizes = {}
    for segmentID in segmentationNode.GetSegmentation().GetSegmentIDs():
        mask = vtk.vtkImageData()
        slicer.vtkSlicerAstroVolumeLogic.CreateSegmentMask(segmentationNode, segmentID, volumeNode, mask)
        sizes[segmentID] = int(np.count_nonzero(vtk_to_numpy(mask.GetPointData().GetScalars())))
    return max(sizes, key=sizes.get)


def run(outputFolder):
    import AstroSampleData
    import scipy.ndimage

    os.makedirs(outputFolder, exist_ok=True)
    logic = AstroSampleData.AstroSampleDataLogic()
    results = {}

    # WEIN069 and its SoFiA mask: the largest source
    cube = logic.downloadSample("WEIN069")
    sofia = logic.downloadSample("WEIN069_MASK")
    sofiaID = largestSegment(sofia, cube)
    import vtk
    value = vtk.reference("")
    sofia.GetSegmentation().GetSegment(sofiaID).GetTag("SlicerAstro.SourceID", value)
    # the original files (download cache of the sample data)
    cache = slicer.mrmlScene.GetCacheManager().GetRemoteCacheDirectory()
    files = {"WEIN069": os.path.join(cache, "WEIN069.fits"), "NGC2403": os.path.join(cache, "NGC2403.fits")}
    datasets = [("WEIN069", cube, sofia, sofiaID, os.path.join(cache, "WEIN069_mask.fits"), int(str(value)))]

    # NGC 2403: mask of the emission (3 sigma, dilated), saved as a FITS mask
    galaxy = logic.downloadSample("NGC2403")
    data = voxelArray(galaxy)
    with np.errstate(invalid="ignore"):
        emission = scipy.ndimage.binary_dilation(data > 3. * galaxy.GetDisplayThreshold(), iterations=2)
    maskNode = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLSegmentationNode", "NGC2403_emission")
    maskNode.CreateDefaultDisplayNodes()
    maskNode.SetReferenceImageGeometryParameterFromVolumeNode(galaxy)
    emissionID = maskNode.GetSegmentation().AddEmptySegment("Emission", "Emission")
    slicer.util.updateSegmentBinaryLabelmapFromArray(emission.astype(np.uint8), maskNode, emissionID, galaxy)
    maskPath = os.path.join(outputFolder, "NGC2403_emission_mask.fits")
    slicer.util.saveNode(maskNode, maskPath)
    datasets.append(("NGC2403", galaxy, maskNode, emissionID, maskPath, 1))

    for name, volumeNode, segmentationNode, segmentID, maskFile, label in datasets:
        slicer.app.applicationLogic().GetSelectionNode().SetActiveVolumeID(volumeNode.GetID())
        selectModule("AstroVolume")
        display = volumeNode.GetAstroVolumeDisplayNode()
        entry = {"cube": files[name], "maskFile": maskFile, "label": label,
                 "rms": volumeNode.GetDisplayThreshold(),
                 "velocityDefinition": display.GetVelocityDefinition(),
                 "restFrequency": float(volumeNode.GetAttribute("SlicerAstro.RESTFREQ"))}
        for masked in (False, True):
            key = "mask" if masked else "threshold"
            entry[key] = {}
            momentMaps(volumeNode, segmentationNode if masked else None, segmentID,
                       f"{name}_{key}", outputFolder, entry[key])
            profile(volumeNode, segmentationNode if masked else None, segmentID,
                    f"{name}_{key}", outputFolder, entry[key])
            if masked:
                profile(volumeNode, segmentationNode, segmentID, f"{name}_{key}", outputFolder, entry[key],
                        aperture=True, key="profileAperture")
        statistics(volumeNode, segmentationNode, segmentID, entry["mask"])
        results[name] = entry

    with open(os.path.join(outputFolder, "slicerastro.json"), "w") as f:
        json.dump(results, f, indent=1)


if __name__ == "__main__":
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(slicer.app.temporaryPath, "AnalysisValidation")
    status = 0
    try:
        run(folder)
        print("Outputs written to", folder)
    except Exception:
        import traceback
        traceback.print_exc()
        status = 1
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(status)
