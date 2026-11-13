"""Reprojections of the Antennae sample data with SlicerAstro, saved as FITS
for compare_with_astropy.py (developer validation, not a ctest).

  SlicerWithSlicerAstro --no-splash --python-script reproject_antennae.py <data dir> <output dir>

<data dir>: Antennae_R.fits, Antennae_CO_North.fits, Antennae_CO_South.fits and
Antennae_HI.fits (AstroSampleData, release SampleData-v2)."""
import os, sys, time, traceback, json, slicer
DATA, OUT = (os.path.abspath(path) for path in sys.argv[-2:])
os.makedirs(OUT, exist_ok=True)
P = slicer.vtkMRMLAstroReprojectParametersNode
logic = slicer.modules.astroreproject.logic()
timings = {}
def load(name):
    node = slicer.util.loadVolume(os.path.join(DATA, name + ".fits"))
    node.SetName(name)
    return node
def run(tag, inputs, **values):
    pnode = P()
    pnode.SetInputVolumeNodeIDs(";".join(n.GetID() for n in inputs))
    pnode.SetInputVolumeNodeID(inputs[0].GetID())
    for k, v in values.items():
        getattr(pnode, "Set" + k)(v)
    t = time.time()
    ok = logic.Reproject(pnode)
    timings[tag] = time.time() - t
    if not ok:
        print("FAILED", tag, logic.GetLastError()); return None
    out = slicer.mrmlScene.GetNodeByID(pnode.GetOutputVolumeNodeID())
    slicer.util.saveNode(out, os.path.join(OUT, tag + ".fits"))
    print("done", tag, out.GetImageData().GetDimensions(), f"{timings[tag]:.2f}s")
    slicer.mrmlScene.RemoveNode(out)
    return out
try:
    r = load("Antennae_R"); south = load("Antennae_CO_South"); north = load("Antennae_CO_North"); hi = load("Antennae_HI")
    for method, name in ((P.NearestNeighbour, "nearest"), (P.Bilinear, "bilinear"), (P.ExactOverlap, "exact"), (P.AreaAverage, "area")):
        run("R_on_south_" + name, [r], SpatialGridMode=P.SpatialReference, ReferenceVolumeNodeID=south.GetID(),
            InterpolationOrder=method)
    run("HI_on_south_bilinear", [hi], SpatialGridMode=P.SpatialReference, ReferenceVolumeNodeID=south.GetID(),
        SpectralGridMode=P.SpectralInput, InterpolationOrder=P.Bilinear)
    run("north_galactic_bilinear", [north], SpatialGridMode=P.SpatialManual, CelestialFrame="GALACTIC", Projection="CAR",
        CenterLongitude=286.95837715, CenterLatitude=42.46202483, PixelSizeX=0.5, PixelSizeY=0.5, SizeX=200, SizeY=200, Rotation=0.,
        SpectralGridMode=P.SpectralInput, InterpolationOrder=P.Bilinear)
    run("north_galactic_exact", [north], SpatialGridMode=P.SpatialManual, CelestialFrame="GALACTIC", Projection="CAR",
        CenterLongitude=286.95837715, CenterLatitude=42.46202483, PixelSizeX=0.5, PixelSizeY=0.5, SizeX=200, SizeY=200, Rotation=0.,
        SpectralGridMode=P.SpectralInput, InterpolationOrder=P.ExactOverlap)
    json.dump(timings, open(os.path.join(OUT, "timings.json"), "w"), indent=1)
except Exception:
    traceback.print_exc(); print("FAILED")
sys.stdout.flush(); slicer.util.exit(0)
