"""Astronomical helpers of the Workbench: kinds of datasets, chips (beam,
channel width, line), channel velocities, lineage of derived products,
slice <-> channel, WCS readout."""

import math

import vtk

import slicer

# rest frequencies (Hz) of common lines, for the chips of the datasets
SPEED_OF_LIGHT = 299792458.0

LINES = (
    (1.420405752e9, "HI 21 cm"),
    (115.2712018e9, "CO(1–0)"),
    (230.538e9, "CO(2–1)"),
    (345.7959899e9, "CO(3–2)"),
    (110.2013543e9, "13CO(1–0)"),
    (1.665401800e9, "OH 1665"),
    (1.667359000e9, "OH 1667"),
)


def lines():
    """Named lines (rest frequency in Hz, name): the radio lines above and the
    optical lines of the AstroVolume logic (vacuum wavelengths)."""
    result = list(LINES)
    logic = slicer.vtkSlicerAstroVolumeLogic
    for index in range(logic.GetNumberOfOpticalLines()):
        result.append((SPEED_OF_LIGHT / logic.GetOpticalLineWavelength(index), logic.GetOpticalLineName(index)))
    return result


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


def displayUnit(unit):
    """A unit of the header as it is written in text (no "UNDEFINED")."""
    unit = (unit or "").strip()
    if unit.upper() == "UNDEFINED":
        return ""
    return slicer.app.property("SlicerAstroTheme").displayUnit(unit) if slicer.app.property("SlicerAstroTheme") \
        else unit.replace("JY/BEAM", "Jy/beam")


def formatFlux(value, unit):
    unit = displayUnit(unit)
    if unit.startswith("Jy") and value and abs(value) < 0.1:
        return f"{value * 1000:.3g} m{unit}"
    return f"{value:.3g} {unit}".strip()


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
    unit = attribute(volume, "CUNIT3", "").strip()
    # km/s, or Angstrom for wavelengths (optical data without a line)
    factor = 0.001 if unit.lower() == "m/s" else (1.e10 if unit == "m" else 1.0)
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
    if unit == "m":
        return "Å"
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
    # the line of data with a spectral axis (images get a default rest
    # frequency at loading)
    restFrequency = floatAttribute(volume, "RESTFREQ") if dimensions(volume) >= 3 else float("nan")
    if dimensions(volume) >= 3 and (not math.isfinite(restFrequency) or restFrequency <= 0):
        restFrequency = floatAttribute(volume, "RESTFRQ")
    if math.isfinite(restFrequency) and restFrequency > 0:
        for frequency, name in lines():
            if abs(restFrequency - frequency) < 1e-4 * frequency:
                result.append(name)
                break
    return result


def astroVolumes(scene):
    return [node for node in slicer.util.getNodesByClass("vtkMRMLAstroVolumeNode")
            if not node.GetHideFromEditors()] if scene else []


# outputs of the SlicerAstro modules: "<data>_<token>..." (e.g.
# NGC2403_mom0, NGC2403_Filtered_2) or a data model of a product
PRODUCT_TOKENS = ("mom0", "mom1", "mom2", "Profile", "PVDiagram", "PVSlice", "Filtered", "Crop", "Blank",
                  "model", "maskedByModel", "Reprojected", "Mosaic", "Weight", "Sensitivity", "Sault", "selection",
                  "Histogram")
PRODUCT_DATAMODELS = ("ZEROMOMENTMAP", "FIRSTMOMENTMAP", "SECONDMOMENTMAP", "PROFILE", "PVDIAGRAM", "MODEL")


def parentOf(volume, volumes):
    """The dataset a product derives from: the longest name that is a
    prefix of the product name followed by "_" and the name of a product
    (e.g. NGC2403_mom0, not NGC2403_DSS)."""
    best = None
    name = volume.GetName() or ""
    isProduct = attribute(volume, "DATAMODEL", "") in PRODUCT_DATAMODELS
    for other in volumes:
        otherName = other.GetName() or ""
        if other is volume or not otherName or not name.startswith(otherName + "_"):
            continue
        rest = name[len(otherName) + 1:]
        if not isProduct and not any(rest.startswith(token) for token in PRODUCT_TOKENS):
            continue
        if best is None or len(otherName) > len(best.GetName()):
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
            if datamodel == "PROFILE":
                # spectra of the Profiles panel: shown in its plot, not data
                continue
            icon = {"ZEROMOMENTMAP": "contour", "FIRSTMOMENTMAP": "spectrum",
                    "SECONDMOMENTMAP": "histogram"}.get(datamodel, "layers" if dimensions(other) >= 3 else "colormap")
            products.append((other.GetID(), other.GetName(), displayUnit(attribute(other, "BUNIT", "")), icon, other))
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
    for table in cataloguesOf(volume):
        rows = table.GetTable().GetNumberOfRows() if table.GetTable() else 0
        products.append((table.GetID(), table.GetName(), f"{rows} source{'s' if rows != 1 else ''}", "table", table))
    return products


def derivedNodes(volume, scene):
    """Everything made from a dataset, in the order of deletion (the dataset
    itself not included): source points of its catalogues, plot series and
    tables of its profiles, catalogues, segmentations on its grid or on the
    grid of its products (masks, contours, 3DBarolo masks and contours),
    then the derived volumes (products of products first)."""
    volumes = astroVolumes(scene)
    derived = []
    pending = [volume]
    while pending:
        parent = pending.pop()
        for other in volumes:
            if other is not volume and other not in derived and parentOf(other, volumes) is parent:
                derived.append(other)
                pending.append(other)
    grids = [volume] + derived
    nodes = []
    logic = slicer.vtkSlicerAstroVolumeLogic
    catalogueTables = cataloguesOf(volume)
    for table in catalogueTables:
        points = logic.GetSourceCataloguePoints(table)
        if points is not None:
            nodes.append(points)
    # profiles: their tables (referenced by the profile volumes) and the plot
    # series showing them
    profileTables = []
    for product in derived:
        referenced = product.GetNodeReference("SlicerAstro.ProfileTable")
        if referenced is not None:
            profileTables.append(referenced)
    for series in slicer.util.getNodesByClass("vtkMRMLPlotSeriesNode"):
        if series.GetTableNode() in profileTables:
            nodes.append(series)
    nodes += profileTables + catalogueTables
    role = slicer.vtkMRMLSegmentationNode.GetReferenceImageGeometryReferenceRole()
    for segmentation in slicer.util.getNodesByClass("vtkMRMLSegmentationNode"):
        if segmentation.GetNodeReference(role) in grids:
            nodes.append(segmentation)
    nodes += list(reversed(derived))
    # each node once, in this order
    unique = []
    for node in nodes:
        if node is not None and node not in unique:
            unique.append(node)
    return unique


def catalogues(scene=None):
    """Source catalogues (SoFiA and other catalogues loaded by AstroSoFiACatalog)."""
    return [node for node in slicer.util.getNodesByClass("vtkMRMLTableNode")
            if node.GetAttribute("SlicerAstro.SoFiASources")]


def catalogueVolume(tableNode):
    """The dataset a catalogue is placed on (vtkSlicerAstroVolumeLogic)."""
    return slicer.vtkSlicerAstroVolumeLogic.GetSourceCatalogueVolume(tableNode) if tableNode is not None else None


def cataloguesOf(volume):
    """The catalogues placed on a dataset (vtkSlicerAstroVolumeLogic)."""
    tables = vtk.vtkCollection()
    slicer.vtkSlicerAstroVolumeLogic.GetSourceCatalogues(volume, tables)
    return [tables.GetItemAsObject(index) for index in range(tables.GetNumberOfItems())]


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


def sameSkyGrid(volume, other, tolerance=1e-6):
    """True if two volumes share their pixels on the sky (same columns and
    rows in RAS; they may differ along the spectral axis, e.g. a cube and its
    moment maps, or a cube and its smoothed or blanked copy)."""
    if not isAstroVolume(volume) or not isAstroVolume(other):
        return False
    if volume is other:
        return True
    a, b = _matrix(volume), _matrix(other)
    dimsA = volume.GetImageData().GetDimensions() if volume.GetImageData() else None
    dimsB = other.GetImageData().GetDimensions() if other.GetImageData() else None
    if dimsA is None or dimsB is None or dimsA[0] != dimsB[0] or dimsA[1] != dimsB[1]:
        return False
    scale = max(abs(a.GetElement(r, c)) for r in range(3) for c in range(3)) or 1.0
    for column in (0, 1):
        for row in range(3):
            if abs(a.GetElement(row, column) - b.GetElement(row, column)) > tolerance * scale:
                return False
    # origins: equal, or apart along the spectral axis only
    delta = [b.GetElement(row, 3) - a.GetElement(row, 3) for row in range(3)]
    axis = [a.GetElement(row, 2) for row in range(3)]
    axisNorm = sum(value * value for value in axis) ** 0.5 or 1.0
    along = sum(delta[row] * axis[row] for row in range(3)) / axisNorm
    across = [delta[row] - along * axis[row] / axisNorm for row in range(3)]
    return sum(value * value for value in across) ** 0.5 <= tolerance * scale * 10


def sameGrid(volume, other, tolerance=1e-6):
    """True if two volumes share their voxels (same sky grid and the same
    channels), e.g. a cube and its smoothed or blanked copy."""
    if not sameSkyGrid(volume, other, tolerance):
        return False
    if dimensions(volume) != dimensions(other):
        return False
    a, b = _matrix(volume), _matrix(other)
    for row in range(3):
        for column in (2, 3):
            if abs(a.GetElement(row, column) - b.GetElement(row, column)) > 1e-6 * max(1.0, abs(a.GetElement(row, column))):
                return False
    return volume.GetImageData().GetDimensions()[2] == other.GetImageData().GetDimensions()[2]


def sourceOf(volume, scene=None):
    """The dataset a product derives from (its root), or the volume itself."""
    volumes = astroVolumes(scene or slicer.mrmlScene)
    current = volume
    for _ in range(16):
        parent = parentOf(current, volumes) if current is not None else None
        if parent is None:
            return current
        current = parent
    return current


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


def angularSeparation(world0, world1):
    """Angle (degrees) between two (RA, Dec) positions in degrees."""
    ra0, dec0, ra1, dec1 = (math.radians(v) for v in (world0[0], world0[1], world1[0], world1[1]))
    s = (math.sin((dec1 - dec0) / 2) ** 2 + math.cos(dec0) * math.cos(dec1) * math.sin((ra1 - ra0) / 2) ** 2)
    return math.degrees(2 * math.asin(min(1.0, math.sqrt(s))))


def hasBeam(volume):
    return math.isfinite(floatAttribute(volume, "BMAJ")) and floatAttribute(volume, "BMAJ") > 0


def skyGeometry(volume, sliceNode):
    """Of a slice view on the sky plane of a volume, at the centre of the
    view: arc seconds per pixel of the view (XY), and the directions of north
    and east on the screen (unit vectors, y down). None if the view does not
    show the sky."""
    if not isAstroVolume(volume) or sliceNode.GetOrientation() != "XY":
        return None
    displayNode = volume.GetDisplayNode()
    if displayNode is None or not displayNode.IsA("vtkMRMLAstroVolumeDisplayNode") or displayNode.GetSpace() != "WCS":
        return None
    xyToRAS = sliceNode.GetXYToRAS()
    rasToXY = vtk.vtkMatrix4x4()
    vtk.vtkMatrix4x4.Invert(xyToRAS, rasToXY)
    rasToIJK = _matrix(volume, False)
    ijkToRAS = _matrix(volume, True)
    dims = sliceNode.GetDimensions()
    centre = (dims[0] / 2.0, dims[1] / 2.0)

    def world(x, y):
        ras = xyToRAS.MultiplyPoint([x, y, 0.0, 1.0])
        ijk = rasToIJK.MultiplyPoint(ras)[:3]
        out = [0.0, 0.0, 0.0]
        displayNode.GetReferenceSpace(ijk, out)
        return out

    def screen(w):
        ijk = [0.0, 0.0, 0.0]
        displayNode.GetIJKSpace(w, ijk)
        ras = ijkToRAS.MultiplyPoint([ijk[0], ijk[1], ijk[2], 1.0])
        xy = rasToXY.MultiplyPoint(ras)
        return xy[0], xy[1]

    w0 = world(*centre)
    w1 = world(centre[0] + 100.0, centre[1])
    arcsecPerPixel = angularSeparation(w0, w1) * 3600.0 / 100.0
    if not arcsecPerPixel > 0:
        return None
    x0, y0 = screen(w0)
    step = 1.0 / 60.0
    directions = []
    for w in ([w0[0], w0[1] + step, w0[2]], [w0[0] + step / max(1e-6, math.cos(math.radians(w0[1]))), w0[1], w0[2]]):
        x, y = screen(w)
        dx, dy = x - x0, -(y - y0)      # XY has y up, the screen y down
        norm = math.hypot(dx, dy)
        if norm == 0:
            return None
        directions.append((dx / norm, dy / norm))
    return arcsecPerPixel, directions[0], directions[1]


SCALE_LENGTHS = [1, 2, 5, 10, 20, 30, 60, 120, 300, 600, 900, 1200, 1800, 3600, 7200, 18000]


def scaleBar(arcsecPerPixel, targetPixels=90.0):
    """A round angular length close to targetPixels: (pixels, label)."""
    best = min(SCALE_LENGTHS, key=lambda a: abs(math.log(a / (arcsecPerPixel * targetPixels))))
    if best >= 3600:
        label = f"{best // 3600}°"
    elif best >= 60:
        label = f"{best // 60}′"
    else:
        label = f"{best}″"
    return best / arcsecPerPixel, label
