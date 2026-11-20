"""Contours of the Workbench: levels (in units of the noise or of HI column
density), colour families, and the automatic HI contours on optical (and
other) images that overlap an HI dataset on the sky."""

import logging
import math

import vtk

import slicer

from . import Astro

def columnDensityFactor(volume):
    """cm^-2 per unit of a map of HI integrated intensity (0 if not one)."""
    if not Astro.isAstroVolume(volume) or Astro.dimensions(volume) != 2:
        return 0.0
    return slicer.vtkSlicerAstroVolumeLogic.GetHIColumnDensityFactor(volume)


def isHICube(volume):
    return Astro.isAstroVolume(volume) and Astro.dimensions(volume) >= 3 and "HI 21 cm" in Astro.chips(volume)


def isHIMap(volume):
    return columnDensityFactor(volume) > 0 and \
        Astro.attribute(volume, "DATAMODEL", "") in ("ZEROMOMENTMAP", "") and Astro.hasBeam(volume)


def isImage(volume):
    """A 2D image that is not a map derived from a cube (e.g. optical,
    infrared, radio continuum)."""
    return Astro.isAstroVolume(volume) and Astro.dimensions(volume) == 2 and \
        not Astro.attribute(volume, "DATAMODEL", "").endswith("MOMENTMAP") and columnDensityFactor(volume) == 0


def skyFootprint(volume):
    """Centre (RA, Dec in degrees) and radius (degrees) of the sky area of a
    dataset, from the WCS of its corners; None without a celestial WCS."""
    displayNode = volume.GetDisplayNode() if volume is not None else None
    image = volume.GetImageData() if volume is not None else None
    if displayNode is None or image is None or not displayNode.IsA("vtkMRMLAstroVolumeDisplayNode") or \
            displayNode.GetSpace() != "WCS":
        return None
    dims = image.GetDimensions()
    world = [0.0, 0.0, 0.0]
    displayNode.GetReferenceSpace([dims[0] / 2.0, dims[1] / 2.0, 0.0], world)
    centre = (world[0], world[1])
    if not all(math.isfinite(value) for value in centre):
        return None
    radius = 0.0
    for i, j in ((0, 0), (dims[0], 0), (0, dims[1]), (dims[0], dims[1])):
        corner = [0.0, 0.0, 0.0]
        displayNode.GetReferenceSpace([i - 0.5, j - 0.5, 0.0], corner)
        if all(math.isfinite(value) for value in corner[:2]):
            radius = max(radius, Astro.angularSeparation(centre, corner))
    return centre, radius


def overlap(volumeA, volumeB):
    """The sky areas of two datasets overlap."""
    a, b = skyFootprint(volumeA), skyFootprint(volumeB)
    if a is None or b is None:
        return False
    return Astro.angularSeparation(a[0], b[0]) < a[1] + b[1]


def roundNumber(value):
    """1, 2 or 5 times a power of ten, at most `value`."""
    if not value > 0:
        return value
    magnitude = 10.0 ** math.floor(math.log10(value))
    for step in (5.0, 2.0, 1.0):
        if step * magnitude <= value * (1 + 1e-9):
            return step * magnitude
    return magnitude


def doublingLevels(lowest, highest, maximumLevels=6):
    """Levels from a round lowest level, doubling, below the highest value."""
    levels = []
    level = roundNumber(lowest)
    while level < highest and len(levels) < maximumLevels and level > 0:
        levels.append(level)
        level *= 2.0
    return levels


def contourSources(contours):
    """Datasets whose contours are drawn in a contours segmentation: {source
    ID: number of levels} (vtkSlicerAstroVolumeLogic.GetContourSources)."""
    sourceIDs = vtk.vtkStringArray()
    levels = vtk.vtkIntArray()
    slicer.vtkSlicerAstroVolumeLogic.GetContourSources(contours, sourceIDs, levels)
    return {sourceIDs.GetValue(index): levels.GetValue(index) for index in range(sourceIDs.GetNumberOfValues())}


def familyColours(family, count):
    """`count` colours (r, g, b in 0-1) of a contour colour family of the
    AstroVolume logic, from the first level to the last."""
    colours = vtk.vtkDoubleArray()
    slicer.vtkSlicerAstroVolumeLogic.GetContourFamilyColors(family, count, colours)
    return colours


def zeroMomentMap(cube):
    """Moment 0 of an HI cube for its contours: an existing moment-0 map of
    the cube, or a new one (voxels above 3 sigma, all the channels)."""
    existing = slicer.vtkSlicerAstroVolumeLogic.GetMomentMap(cube, 0)
    if existing is not None:
        return existing
    noise = Astro.noise(cube)
    velocities = Astro.channelVelocities(cube)
    if noise <= 0 or len(velocities) < 2:
        return None
    logic = slicer.modules.astromomentmaps.logic()
    unit = (Astro.attribute(cube, "BUNIT", "") + " km/s").strip()
    parameters = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLAstroMomentMapsParametersNode")
    parameters.SetHideFromEditors(True)
    try:
        channelWidth = abs(velocities[1] - velocities[0])
        parameters.SetInputVolumeNodeID(cube.GetID())
        parameters.SetGenerateZero(True)
        parameters.SetGenerateFirst(False)
        parameters.SetGenerateSecond(False)
        parameters.SetMaskActive(False)
        parameters.SetIntensityMin(3.0 * noise)
        parameters.SetIntensityMax(Astro.floatAttribute(cube, "DATAMAX", 1e30))
        parameters.SetVelocityMin(min(velocities) - channelWidth)
        parameters.SetVelocityMax(max(velocities) + channelWidth)
        # the moment-0 map of the cube shared by the modules ("<cube>_mom0")
        moment = logic.CreateMomentMapVolume(cube, slicer.vtkSlicerAstroVolumeLogic.GetMomentMapName(cube, 0),
                                             unit, slicer.vtkSlicerAstroVolumeLogic.GetMomentMapColorTableName(0), "ZEROMOMENTMAP")
        if moment is None:
            return None
        parameters.SetZeroMomentVolumeNodeID(moment.GetID())
        if not logic.CalculateMomentMaps(parameters):
            slicer.mrmlScene.RemoveNode(moment)
            return None
        slicer.vtkSlicerAstroVolumeLogic.SetMomentMap(cube, 0, moment)
        return moment
    finally:
        slicer.mrmlScene.RemoveNode(parameters)


def columnDensityLevels(moment, cube=None, maximumLevels=8, lineWidth=30.0):
    """Column density levels (cm^-2) for an HI moment-0 map, doubling up to
    the peak. The lowest level is 3 times the noise of the map, estimated
    from the cube as sigma dv sqrt(N) for a line of `lineWidth` km/s (N
    channels), and at least peak / 2^(maximumLevels - 1) so that the levels
    reach the peak. Without the cube: the 2nd percentile of the map."""
    import numpy as np
    from vtk.util.numpy_support import vtk_to_numpy
    factor = columnDensityFactor(moment)
    if factor <= 0 or moment.GetImageData() is None:
        return []
    values = vtk_to_numpy(moment.GetImageData().GetPointData().GetScalars()).astype(np.float64)
    values = values[np.isfinite(values) & (values > 0)]
    if values.size == 0:
        return []
    peak = float(values.max()) * factor
    lowest = 0.0
    velocities = Astro.channelVelocities(cube) if cube is not None else []
    if cube is not None and Astro.noise(cube) > 0 and len(velocities) > 1:
        channelWidth = abs(velocities[1] - velocities[0])
        channels = max(3.0, lineWidth / channelWidth)
        lowest = 3.0 * Astro.noise(cube) * channelWidth * math.sqrt(channels) * factor
    if not lowest > 0:
        lowest = float(np.percentile(values, 2)) * factor
    lowest = max(lowest, peak / 2.0 ** (maximumLevels - 1))
    return doublingLevels(lowest, peak, maximumLevels)


def drawContours(target, source, levels, family, overlay=True):
    """Contours of the source (levels in its units) over the target, one
    colour of the family per level. Returns (created, message)."""
    logic = slicer.modules.astrovolume.logic()
    levelArray = vtk.vtkDoubleArray()
    for level in levels:
        levelArray.InsertNextValue(level)
    created = logic.CreateContours(target, source, levelArray, False, overlay, familyColours(family, len(levels)),
                                   False)
    return created, logic.GetLastContoursMessage()


def isBrightImage(image):
    """Most of the image is shown bright (e.g. a negative optical plate):
    the median value is in the upper half of the display window."""
    import numpy as np
    from vtk.util.numpy_support import vtk_to_numpy
    displayNode = image.GetDisplayNode()
    if displayNode is None or image.GetImageData() is None or not displayNode.GetWindow() > 0:
        return False
    values = vtk_to_numpy(image.GetImageData().GetPointData().GetScalars())
    values = values[::max(1, values.size // 200000)]
    values = values[np.isfinite(values)]
    if values.size == 0:
        return False
    low = displayNode.GetLevel() - displayNode.GetWindow() / 2.0
    return (float(np.median(values)) - low) / displayNode.GetWindow() > 0.5


class AutomaticHIContours:
    """HI contours drawn automatically over the images that overlap an HI
    dataset on the sky (once per pair; the user toggles them on the card of
    the image)."""

    def __init__(self):
        self.done = set()
        self.pending = False

    def candidates(self):
        volumes = Astro.astroVolumes(slicer.mrmlScene)
        images = [volume for volume in volumes if isImage(volume)]
        hiData = [volume for volume in volumes if isHICube(volume) or isHIMap(volume)]
        pairs = []
        for image in images:
            for hi in hiData:
                key = (image.GetID(), hi.GetID())
                if key not in self.done and overlap(image, hi):
                    pairs.append((image, hi))
        return pairs

    def run(self):
        """Contours of the new pairs. Returns the names of the images that
        received contours."""
        updated = []
        for image, hi in self.candidates():
            self.done.add((image.GetID(), hi.GetID()))
            cube = hi if isHICube(hi) else None
            moment = zeroMomentMap(hi) if cube is not None else hi
            if moment is None:
                continue
            self.done.add((image.GetID(), moment.GetID()))
            factor = columnDensityFactor(moment)
            levels = columnDensityLevels(moment, cube)
            if not levels or factor <= 0:
                continue
            family = "Deep blue" if isBrightImage(image) else "Amber"
            created, message = drawContours(image, moment, [level / factor for level in levels], family)
            if created:
                updated.append(image.GetName())
            elif message and message != "cancelled":
                logging.warning(f"HI contours of {moment.GetName()} on {image.GetName()}: {message}")
        return updated
