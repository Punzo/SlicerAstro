"""SlicerAstro side of the validation of the AstroReproject engine.

  SlicerWithSlicerAstro --no-splash --python-script slicer_outputs.py <Antennae sample data dir> <work dir>

Run prepare_inputs.py first. Writes the reprojections (FITS) and the beam
results (beams_slicerastro.json) to <work dir>."""
import json
import os
import sys
import traceback

import slicer
import vtk

DATA, WORK = (os.path.abspath(path) for path in sys.argv[-2:])
P = slicer.vtkMRMLAstroReprojectParametersNode
logic = slicer.modules.astroreproject.logic()


def run(tag, inputs, **values):
    pnode = P()
    pnode.SetInputVolumeNodeIDs(";".join(node.GetID() for node in inputs))
    pnode.SetInputVolumeNodeID(inputs[0].GetID())
    for key, value in values.items():
        getattr(pnode, "Set" + key)(value)
    if not logic.Reproject(pnode):
        raise RuntimeError(f"{tag}: {logic.GetLastError()}")
    output = slicer.mrmlScene.GetNodeByID(pnode.GetOutputVolumeNodeID())
    slicer.util.saveNode(output, os.path.join(WORK, tag + ".fits"))
    print("done", tag, output.GetImageData().GetDimensions())
    slicer.mrmlScene.RemoveNode(output)


try:
    load = lambda directory, name: slicer.util.loadVolume(os.path.join(directory, name + ".fits"))
    r, hi = load(DATA, "Antennae_R"), load(DATA, "Antennae_HI")
    rFK5 = load(WORK, "Antennae_R_FK5")
    south, north = load(WORK, "Antennae_CO_South_3D"), load(WORK, "Antennae_CO_North_3D")
    methods = {"nearest": P.NearestNeighbour, "bilinear": P.Bilinear, "bicubic": P.Bicubic, "lanczos": P.Lanczos3,
               "area": P.AreaAverage, "exact": P.ExactOverlap}
    galactic = dict(SpatialGridMode=P.SpatialManual, CelestialFrame="GALACTIC", Projection="CAR",
                    CenterLongitude=286.95837715, CenterLatitude=42.46202483, PixelSizeX=0.5, PixelSizeY=0.5,
                    SizeX=200, SizeY=200, Rotation=0., SpectralGridMode=P.SpectralInput)
    # astropy reproject: ICRS image on an FK5 grid, a cube, a Galactic grid
    for name in ("nearest", "bilinear", "exact", "area"):
        run("R_on_south_" + name, [r], SpatialGridMode=P.SpatialReference, ReferenceVolumeNodeID=south.GetID(),
            InterpolationOrder=methods[name])
    run("HI_on_south_bilinear", [hi], SpatialGridMode=P.SpatialReference, ReferenceVolumeNodeID=south.GetID(),
        SpectralGridMode=P.SpectralInput, InterpolationOrder=P.Bilinear)
    run("north_galactic_bilinear", [north], InterpolationOrder=P.Bilinear, **galactic)
    run("north_galactic_exact", [north], InterpolationOrder=P.ExactOverlap, **galactic)
    # casacore: interpolation (FK5 image: no frame conversion) and spectral regridding with frames
    for name in ("nearest", "bilinear", "bicubic", "lanczos", "exact", "area"):
        run("Rfk5_on_south_" + name, [rFK5], SpatialGridMode=P.SpatialReference, ReferenceVolumeNodeID=south.GetID(),
            InterpolationOrder=methods[name])
    for name, method in (("linear", P.SpectralLinear), ("nearest", P.SpectralNearest)):
        run("south_vrad_bary_" + name, [south], SpatialGridMode=P.SpatialInput, SpectralGridMode=P.SpectralManual,
            SpectralType="VRAD", SpectralFrame="BARYCENT", RestFrequency=230.538e9, SpectralStart=1300.e3,
            SpectralStep=10.e3, SpectralCount=50, SpectralMethod=method)
    # Montage: block average, rotated TAN cube, mean mosaic
    for name in ("exact", "area"):
        run("Rfk5_shrink2_" + name, [rFK5], SpatialGridMode=P.SpatialResample, SpatialResampleFactor=2.,
            InterpolationOrder=methods[name])
    run("north_tan_rot_exact", [north], SpatialGridMode=P.SpatialManual, CelestialFrame="FK5", Equinox=2000.,
        Projection="TAN", CenterLongitude=180.4706612974, CenterLatitude=-18.86747777778, PixelSizeX=0.45,
        PixelSizeY=0.45, Rotation=30., SizeX=180, SizeY=180, SpectralGridMode=P.SpectralInput,
        InterpolationOrder=P.ExactOverlap)
    run("mosaic_mean_exact", [north, south], SpatialGridMode=P.SpatialUnion, CelestialFrame="FK5", Equinox=2000.,
        Projection="SIN", PixelSizeX=0.3, PixelSizeY=0.3, Rotation=0., SpectralGridMode=P.SpectralInput,
        InterpolationOrder=P.ExactOverlap, Combination=P.CombineMean)
    # resolution and channels (astropy convolution and numpy references)
    run("south_target3", [south], SpatialGridMode=P.SpatialInput, SpectralGridMode=P.SpectralInput,
        ResolutionMode=P.ResolutionTarget, TargetBeamMajor=3., TargetBeamMinor=3., TargetBeamPA=0.)
    run("south_chan2", [south], SpatialGridMode=P.SpatialInput, SpectralGridMode=P.SpectralResample,
        SpectralResampleFactor=2., SpectralMethod=P.SpectralOverlap)
    # beams (vtkAstroBeams)
    from vtkAstroReprojectPython import vtkAstroBeams as B
    results = []
    for case in json.load(open(os.path.join(WORK, "beams.json"))):
        beams = vtk.vtkDoubleArray()
        beams.SetNumberOfComponents(3)
        for beam in case["beams"]:
            beams.InsertNextTuple3(*beam)
        common = [0., 0., 0.]
        found = B.CommonBeam(beams, common)
        convolved, back = [0., 0., 0.], [0., 0., 0.]
        B.Convolve(case["beams"][0], case["beams"][1], convolved)
        B.Deconvolve(convolved, case["beams"][1], back)
        results.append({"found": bool(found), "common": common, "convolved01": convolved, "back0": back,
                        "deconvolvable": [B.Deconvolve(common, beam, [0., 0., 0.]) != B.NotDeconvolvable
                                          for beam in case["beams"]] if found else []})
    json.dump(results, open(os.path.join(WORK, "beams_slicerastro.json"), "w"))
    print("SlicerAstro outputs written to", WORK)
except Exception:
    traceback.print_exc()
    print("FAILED")
sys.stdout.flush()
slicer.util.exit(0)
