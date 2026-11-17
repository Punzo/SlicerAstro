"""Astronomical helpers of the Workbench: kinds of datasets, chips (beam,
channel width, line), channel velocities, lineage of derived products,
slice <-> channel, WCS readout."""

import math

import vtk

import slicer

# rest frequencies (Hz) of common lines, for the chips of the datasets
LINES = (
    (1.420405752e9, "HI 21 cm"),
    (115.2712018e9, "CO(1–0)"),
    (230.538e9, "CO(2–1)"),
    (345.7959899e9, "CO(3–2)"),
    (110.2013543e9, "13CO(1–0)"),
    (1.665401800e9, "OH 1665"),
    (1.667359000e9, "OH 1667"),
)


def attribute(node, key, default=None):
    value = node.GetAttribute("SlicerAstro." + key) if node else None
    return default if value is None else value


def floatAttribute(node, key, default=float("nan")):
    try:
        return float(attribute(node, key))
    except (TypeError, ValueError):
        return default


def isAstroVolume(node):
    return node is not None and node.IsA("vtkMRMLAstroVolumeNode")


def dimensions(node):
    try:
        return int(attribute(node, "NAXIS", "0"))
    except ValueError:
        return 0


def kindOf(node):
    """Kind of a dataset: "cube", "image" (later "particles", "grid")."""
    if node is not None and node.IsA("vtkMRMLSegmentationNode"):
        return "mask"
    return "cube" if dimensions(node) >= 3 else "image"


def noise(volume):
    try:
        value = volume.GetDisplayThreshold()
    except AttributeError:
        return 0.0
    return value if value and math.isfinite(value) and value > 0 else 0.0


def formatFlux(value, unit):
    unit = unit.replace("JY/BEAM", "Jy/beam")
    if unit.startswith("Jy") and value and abs(value) < 0.1:
        return f"{value * 1000:.3g} m{unit}"
    return f"{value:.3g} {unit}"


def formatAngle(degrees):
    arcsec = degrees * 3600.0
    if arcsec >= 120:
        return f"{arcsec / 60:.3g}′"
    return f"{arcsec:.3g}″"


def channelVelocities(volume):
    """Velocity (km/s, or the spectral unit) of each channel at the centre
    of the field."""
    if dimensions(volume) < 3:
        return []
    displayNode = volume.GetDisplayNode()
    if displayNode is None or not displayNode.IsA("vtkMRMLAstroVolumeDisplayNode"):
        return []
    channels = int(attribute(volume, "NAXIS3", "0"))
    unit = attribute(volume, "CUNIT3", "")
    factor = 0.001 if unit.strip().lower() == "m/s" else 1.0
    ijk = [float(attribute(volume, "NAXIS1", "1")) * 0.5, float(attribute(volume, "NAXIS2", "1")) * 0.5, 0.0]
    world = [0.0, 0.0, 0.0]
    velocities = []
    for k in range(channels):
        ijk[2] = float(k)
        displayNode.GetReferenceSpace(ijk, world)
        velocities.append(world[2] * factor)
    return velocities


def spectralUnit(volume):
    unit = attribute(volume, "CUNIT3", "").strip()
    return "km/s" if unit.lower() in ("m/s", "km/s", "") else unit


def spectralLabel(volume):
    """e.g. "VRAD · LSRK" """
    ctype = attribute(volume, "CTYPE3", "").strip()
    label = ctype.split("-")[0] if ctype else ""
    specsys = attribute(volume, "SPECSYS", "").strip()
    return " · ".join(part for part in (label, specsys) if part)


def chips(volume):
    """Short facts of a dataset: beam, channel width, line."""
    result = []
    bmaj = floatAttribute(volume, "BMAJ")
    bmin = floatAttribute(volume, "BMIN")
    if math.isfinite(bmaj) and bmaj > 0:
        if math.isfinite(bmin) and bmin > 0 and abs(bmaj - bmin) > 0.01 * bmaj:
            result.append(f"{formatAngle(bmaj)}×{formatAngle(bmin)} beam")
        else:
            result.append(f"{formatAngle(bmaj)} beam")
    velocities = channelVelocities(volume)
    if len(velocities) > 1:
        result.append(f"{abs(velocities[1] - velocities[0]):.2g} {spectralUnit(volume)}")
    restFrequency = floatAttribute(volume, "RESTFREQ")
    if not math.isfinite(restFrequency) or restFrequency <= 0:
        restFrequency = floatAttribute(volume, "RESTFRQ")
    if math.isfinite(restFrequency) and restFrequency > 0:
        for frequency, name in LINES:
            if abs(restFrequency - frequency) < 1e-4 * frequency:
                result.append(name)
                break
    return result


def astroVolumes(scene):
    return [node for node in slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode")
            if not node.GetHideFromEditors()] if scene else []


def parentOf(volume, volumes):
    """The dataset a product derives from: the longest name that is a
    prefix of the product name followed by "_" (e.g. NGC2403_mom0th_1)."""
    best = None
    name = volume.GetName() or ""
    for other in volumes:
        otherName = other.GetName() or ""
        if other is volume or not otherName:
            continue
        if name.startswith(otherName + "_") and (best is None or len(otherName) > len(best.GetName())):
            best = other
    return best


def rootDatasets(scene):
    volumes = astroVolumes(scene)
    return [volume for volume in volumes if parentOf(volume, volumes) is None]


def productsOf(volume, scene):
    """Derived products of a dataset: (id, name, detail, icon, node)."""
    products = []
    volumes = astroVolumes(scene)
    for other in volumes:
        if parentOf(other, volumes) is volume:
            datamodel = attribute(other, "DATAMODEL", "")
            icon = {"ZEROMOMENTMAP": "contour", "FIRSTMOMENTMAP": "spectrum",
                    "SECONDMOMENTMAP": "histogram"}.get(datamodel, "layers" if dimensions(other) >= 3 else "colormap")
            products.append((other.GetID(), other.GetName(), attribute(other, "BUNIT", ""), icon, other))
    logic = slicer.vtkSlicerAstroVolumeLogic
    for segmentation in slicer.util.getNodesByClass("vtkMRMLSegmentationNode"):
        if not logic.IsMaskSegmentation(segmentation):
            continue
        reference = segmentation.GetNodeReference(
            slicer.vtkMRMLSegmentationNode.GetReferenceImageGeometryReferenceRole())
        count = segmentation.GetSegmentation().GetNumberOfSegments() if segmentation.GetSegmentation() else 0
        if reference is volume and count > 0:
            products.append((segmentation.GetID(), segmentation.GetName(),
                             f"{count} segment{'s' if count != 1 else ''}", "lasso", segmentation))
    return products


def _matrix(volume, ijkToRAS=True):
    matrix = vtk.vtkMatrix4x4()
    if ijkToRAS:
        volume.GetIJKToRASMatrix(matrix)
    else:
        volume.GetRASToIJKMatrix(matrix)
    transform = volume.GetParentTransformNode()
    if transform is not None:
        worldTransform = vtk.vtkMatrix4x4()
        if ijkToRAS:
            transform.GetMatrixTransformToWorld(worldTransform)
            vtk.vtkMatrix4x4.Multiply4x4(worldTransform, matrix, matrix)
        else:
            transform.GetMatrixTransformFromWorld(worldTransform)
            vtk.vtkMatrix4x4.Multiply4x4(matrix, worldTransform, matrix)
    return matrix


def channelOfSlice(volume, sliceNode):
    """Channel (k) of the volume shown by a slice view."""
    sliceToRAS = sliceNode.GetSliceToRAS()
    point = [sliceToRAS.GetElement(0, 3), sliceToRAS.GetElement(1, 3), sliceToRAS.GetElement(2, 3), 1.0]
    ijk = _matrix(volume, False).MultiplyPoint(point)
    return int(round(ijk[2]))


def setSliceToChannel(volume, sliceNode, channel):
    """Moves a slice view (spectral axis along its normal) to a channel."""
    i0 = float(attribute(volume, "NAXIS1", "1")) * 0.5
    j0 = float(attribute(volume, "NAXIS2", "1")) * 0.5
    ras = _matrix(volume, True).MultiplyPoint([i0, j0, float(channel), 1.0])
    sliceToRAS = sliceNode.GetSliceToRAS()
    normal = [sliceToRAS.GetElement(r, 2) for r in range(3)]
    offset = sum(normal[r] * ras[r] for r in range(3))
    sliceLogic = slicer.app.applicationLogic().GetSliceLogic(sliceNode)
    if sliceLogic is not None:
        sliceLogic.SetSliceOffset(offset)
    else:
        sliceNode.SetSliceOffset(offset)


def readout(volume, ras):
    """RA, Dec, velocity and value of a volume at a RAS position, or None
    outside of the data."""
    if not isAstroVolume(volume) or volume.GetImageData() is None:
        return None
    ijkFloat = _matrix(volume, False).MultiplyPoint([ras[0], ras[1], ras[2], 1.0])[:3]
    dims = volume.GetImageData().GetDimensions()
    ijk = [int(round(value)) for value in ijkFloat]
    if any(ijk[a] < 0 or ijk[a] >= dims[a] for a in range(3)):
        return None
    displayNode = volume.GetDisplayNode()
    if displayNode is None or not displayNode.IsA("vtkMRMLAstroVolumeDisplayNode"):
        return None
    world = [0.0, 0.0, 0.0]
    displayNode.GetReferenceSpace(ijkFloat, world)
    noPrevious = [-1.0, -1.0, -1.0]
    output = [0.0, 0.0, 0.0]
    result = {
        "ra": displayNode.GetDisplayStringFromValueX(world[0], noPrevious, output, 2),
        "dec": displayNode.GetDisplayStringFromValueY(world[1], noPrevious, output, 1),
        "velocity": "",
        "value": volume.GetImageData().GetScalarComponentAsDouble(ijk[0], ijk[1], ijk[2], 0),
        "unit": attribute(volume, "BUNIT", ""),
        "ijk": ijk,
    }
    if dimensions(volume) >= 3:
        result["velocity"] = displayNode.GetDisplayStringFromValueZ(world[2], noPrevious, output, 2)
    return result
