"""Contours of the Workbench: levels (in units of the noise or of HI column
density), colour families, and the automatic HI contours on optical (and
other) images that overlap an HI dataset on the sky."""

import logging
import math

import vtk

import slicer

from . import Astro

# column densities are shown in units of 1e19 cm^-2
COLUMN_DENSITY_UNIT = 1e19

# colour families of the contour levels: (name, colours from the first to the
# last level, sRGB 0-255)
FAMILIES = (
    ("Amber", ((255, 236, 196), (246, 194, 108), (242, 178, 76), (217, 140, 36), (184, 104, 16))),
    ("Cyan", ((198, 244, 250), (128, 226, 238), (79, 209, 224), (38, 166, 190), (16, 120, 150))),
    ("Deep blue", ((140, 170, 255), (90, 130, 240), (53, 80, 176), (38, 56, 138), (24, 36, 100))),
    ("White", ((255, 255, 255), (230, 234, 242), (200, 208, 222), (170, 180, 198), (140, 150, 170))),
    ("Observatory ramp", ((43, 63, 140), (31, 143, 166), (127, 211, 196), (242, 178, 76), (255, 243, 214))),
)


def familyNames():
    return [name for name, colours in FAMILIES]


def familyColours(name, count):
    """`count` colours (r, g, b in 0-1) of a family, from the first level to
    the last."""
    stops = dict(FAMILIES).get(name, FAMILIES[0][1])
    colours = []
    for index in range(count):
        t = 0.0 if count == 1 else index / (count - 1)
        position = t * (len(stops) - 1)
        low = min(int(position), len(stops) - 2)
        u = position - low
        rgb = [(stops[low][c] + (stops[low + 1][c] - stops[low][c]) * u) / 255.0 for c in range(3)]
        colours.append(rgb)
    return colours


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


def contourSources(target, contours):
    """Datasets whose contours are drawn over the target: {source ID:
    [segment IDs]}."""
    sources = {}
    if contours is None or contours.GetSegmentation() is None:
        return sources
    tag = slicer.vtkSlicerAstroVolumeLogic.GetContoursVolumeTagName()
    segmentation = contours.GetSegmentation()
    for index in range(segmentation.GetNumberOfSegments()):
        segmentID = segmentation.GetNthSegmentID(index)
        value = vtk.reference("")
        if segmentation.GetSegment(segmentID).GetTag(tag, value) and str(value):
            sources.setdefault(str(value), []).append(segmentID)
    return sources


def setSourceVisible(contours, segmentIDs, visible):
    displayNode = contours.GetDisplayNode() if contours is not None else None
    if displayNode is None:
        return
    if visible:
        displayNode.SetVisibility(True)
    for segmentID in segmentIDs:
        displayNode.SetSegmentVisibility(segmentID, visible)


def sourceVisible(contours, segmentIDs):
    displayNode = contours.GetDisplayNode() if contours is not None else None
    return displayNode is not None and displayNode.GetVisibility() and \
        any(displayNode.GetSegmentVisibility(segmentID) for segmentID in segmentIDs)


def zeroMomentMap(cube):
    """Moment 0 of an HI cube for its contours: an existing moment-0 map of
    the cube, or a new one (voxels above 3 sigma, all the channels)."""
    for productID, name, detail, icon, node in Astro.productsOf(cube, slicer.mrmlScene):
        if node.IsA("vtkMRMLAstroVolumeNode") and Astro.attribute(node, "DATAMODEL", "") == "ZEROMOMENTMAP":
            return node
    noise = Astro.noise(cube)
    velocities = Astro.channelVelocities(cube)
    if noise <= 0 or len(velocities) < 2:
        return None
    logic = slicer.modules.astromomentmaps.logic()
    unit = (Astro.attribute(cube, "BUNIT", "") + " km/s").strip()
    serial = 1
    while slicer.mrmlScene.GetFirstNodeByName(f"{cube.GetName()}_mom0th_{serial}") is not None:
        serial += 1
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
        moment = logic.CreateMomentMapVolume(cube, f"{cube.GetName()}_mom0th_{serial}", unit, "", "ZEROMOMENTMAP")
        if moment is None:
            return None
        parameters.SetZeroMomentVolumeNodeID(moment.GetID())
        if not logic.CalculateMomentMaps(parameters):
            slicer.mrmlScene.RemoveNode(moment)
            return None
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
    colours = vtk.vtkDoubleArray()
    colours.SetNumberOfComponents(3)
    for rgb in familyColours(family, len(levels)):
        colours.InsertNextTuple3(*rgb)
    created = logic.CreateContours(target, source, levelArray, False, overlay, colours, False)
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
