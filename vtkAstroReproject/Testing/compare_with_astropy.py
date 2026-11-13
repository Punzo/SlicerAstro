"""Compare the SlicerAstro reprojections of reproject_antennae.py with astropy
reproject (developer validation, needs astropy and reproject):

  python compare_with_astropy.py <data dir> <output dir of reproject_antennae.py>

The astropy reprojections use the output grids written by SlicerAstro (FITS
header of the saved volumes), so the WCS written by SlicerAstro is checked as
well. Nearest neighbour, bilinear and exact overlap are compared (scipy cubic
interpolation is a B-spline, not the cubic convolution of SlicerAstro and
CASA). Results (2026-11-13, astropy 8.0.2, reproject 0.21.0): nearest
identical; bilinear and exact within 5e-8 of the peak (float32 rounding).
Exact overlap: SlicerAstro keeps output pixels next to blanks if at least
MinimumCoverage of them is covered (astropy: blank)."""
import sys, warnings
import numpy as np
from astropy.io import fits
from astropy.wcs import WCS, FITSFixedWarning
from reproject import reproject_interp, reproject_exact
warnings.simplefilter("ignore", FITSFixedWarning)
DATA, OUT = sys.argv[1], sys.argv[2]

def cube(path):
    hdu = fits.open(path)[0]
    data = hdu.data
    while data.ndim > 3:
        data = data[0]
    return data.astype(np.float64), WCS(hdu.header).celestial, hdu.header

def compare(tag, source, order):
    src, srcWCS, _ = cube(f"{DATA}/{source}.fits")
    out, outWCS, outHeader = cube(f"{OUT}/{tag}.fits")
    if src.ndim == 2:
        src = src[None]
    if out.ndim == 2:
        out = out[None]
    shape = out.shape[1:]
    ref = np.empty_like(out)
    for k in range(out.shape[0]):
        plane = src[k]
        if order == "exact":
            ref[k] = reproject_exact((plane, srcWCS), outWCS, shape_out=shape, return_footprint=False)
        else:
            ref[k] = reproject_interp((plane, srcWCS), outWCS, shape_out=shape, order=order, return_footprint=False)
    both = np.isfinite(ref) & np.isfinite(out)
    onlyRef = np.count_nonzero(np.isfinite(ref) & ~np.isfinite(out))
    onlyOut = np.count_nonzero(~np.isfinite(ref) & np.isfinite(out))
    scale = np.nanmax(np.abs(ref[both])) if both.any() else 1.
    diff = np.abs(out[both] - ref[both]) / scale
    print(f"{tag:26s} n={both.sum():9d} only_astropy={onlyRef:7d} only_slicer={onlyOut:7d} "
          f"max|d|/peak={diff.max():.2e} p99={np.percentile(diff, 99):.2e} median={np.median(diff):.2e}")

compare("R_on_south_nearest", "Antennae_R", "nearest-neighbor")
compare("R_on_south_bilinear", "Antennae_R", "bilinear")
compare("R_on_south_exact", "Antennae_R", "exact")
compare("R_on_south_area", "Antennae_R", "exact")
compare("HI_on_south_bilinear", "Antennae_HI", "bilinear")
compare("north_galactic_bilinear", "Antennae_CO_North", "bilinear")
compare("north_galactic_exact", "Antennae_CO_North", "exact")
