"""Comparison of the SlicerAstro reprojections with astropy reproject, casacore
(python-casacore, the ImageRegrid of CASA), Montage and radio-beam.

  python compare.py <Antennae sample data dir> <work dir> [<Montage bin dir>]

Run prepare_inputs.py and slicer_outputs.py first. The references are computed
on the grids written by SlicerAstro (FITS headers of its outputs), so the WCS
written by SlicerAstro is checked as well. Each line: voxels compared, voxels
valid only in one of the two, and |difference| / peak (max, 99th percentile,
median)."""
import json
import os
import subprocess
import sys
import warnings

import numpy as np
from astropy.io import fits
from astropy.wcs import WCS

warnings.simplefilter("ignore")
DATA, WORK = sys.argv[1], sys.argv[2]
MONTAGE = sys.argv[3] if len(sys.argv) > 3 else None


def cube(path):
    hdu = fits.open(path)[0]
    data = hdu.data.astype(np.float64)
    while data.ndim > 3:
        data = data[0]
    return (data if data.ndim == 3 else data[None]), WCS(hdu.header).celestial


def report(tag, reference, ours, onlyReference, onlyOurs):
    both = np.isfinite(reference) & np.isfinite(ours)
    difference = np.abs(reference[both] - ours[both]) / np.nanmax(np.abs(reference[both]))
    print(f"  {tag:44s} n={both.sum():9d} only_ref={onlyReference:7d} only_slicerastro={onlyOurs:7d} "
          f"max={difference.max():.1e} p99={np.percentile(difference, 99):.1e} median={np.median(difference):.1e}")


def compareArrays(tag, reference, ours):
    report(tag, reference, ours, np.count_nonzero(np.isfinite(reference) & ~np.isfinite(ours)),
           np.count_nonzero(~np.isfinite(reference) & np.isfinite(ours)))


print("astropy reproject (reproject_interp, reproject_exact)")
from reproject import reproject_exact, reproject_interp
for tag, source, method in (("R_on_south_nearest", "Antennae_R", "nearest-neighbor"),
                            ("R_on_south_bilinear", "Antennae_R", "bilinear"),
                            ("R_on_south_exact", "Antennae_R", "exact"),
                            ("R_on_south_area", "Antennae_R", "exact"),
                            ("HI_on_south_bilinear", "Antennae_HI", "bilinear"),
                            ("north_galactic_bilinear", "Antennae_CO_North", "bilinear"),
                            ("north_galactic_exact", "Antennae_CO_North", "exact")):
    data, dataWCS = cube(f"{DATA}/{source}.fits")
    ours, oursWCS = cube(f"{WORK}/{tag}.fits")
    reference = np.empty_like(ours)
    for k in range(ours.shape[0]):
        if method == "exact":
            reference[k] = reproject_exact((data[k], dataWCS), oursWCS, shape_out=ours.shape[1:], return_footprint=False)
        else:
            reference[k] = reproject_interp((data[k], dataWCS), oursWCS, shape_out=ours.shape[1:], order=method,
                                            return_footprint=False)
    compareArrays(f"{tag} ({method})", reference, ours)

print("casacore ImageRegrid (CASA imregrid)")
from casacore.images import image
for tag, source, axes, interpolation in (("Rfk5_on_south_nearest", "Antennae_R_FK5", [0, 1], "nearest"),
                                         ("Rfk5_on_south_bilinear", "Antennae_R_FK5", [0, 1], "linear"),
                                         ("Rfk5_on_south_bicubic", "Antennae_R_FK5", [0, 1], "cubic"),
                                         ("Rfk5_on_south_lanczos", "Antennae_R_FK5", [0, 1], "lanczos"),
                                         ("R_on_south_bilinear", "../Antennae_R", [0, 1], "linear"),
                                         ("south_vrad_bary_linear", "Antennae_CO_South_3D", [0], "linear"),
                                         ("south_vrad_bary_nearest", "Antennae_CO_South_3D", [0], "nearest")):
    path = f"{WORK}/{source}.fits" if not source.startswith("..") else f"{DATA}/{source[3:]}.fits"
    reference = image(f"{WORK}/{tag}.fits")
    result = image(path).regrid(axes, reference.coordinates(), outshape=list(reference.shape()),
                                interpolation=interpolation, decimate=0)
    casa = np.array(result.getdata(), dtype=np.float64)
    casa[np.array(result.getmask())] = np.nan
    compareArrays(f"{tag} ({interpolation})", casa, np.array(reference.getdata(), dtype=np.float64))

if MONTAGE:
    print("Montage (mProject, mProjectCube, mShrink, mAddCube)")
    montage = os.path.join(WORK, "montage")
    os.makedirs(os.path.join(montage, "projected"), exist_ok=True)
    command = lambda *args: subprocess.run([os.path.join(MONTAGE, args[0])] + list(args[1:]), check=True,
                                           capture_output=True, cwd=montage)
    command("mGetHdr", f"{WORK}/Rfk5_on_south_exact.fits", "south.hdr")
    command("mProject", f"{WORK}/Antennae_R_FK5.fits", "R_on_south.fits", "south.hdr")
    command("mShrink", f"{WORK}/Antennae_R_FK5.fits", "R_shrink2.fits", "2")
    command("mGetHdr", f"{WORK}/north_tan_rot_exact.fits", "tanrot.hdr")
    command("mProjectCube", f"{WORK}/Antennae_CO_North_3D.fits", "north_tan_rot.fits", "tanrot.hdr")
    command("mGetHdr", f"{WORK}/mosaic_mean_exact.fits", "mosaic.hdr")
    for name in ("North", "South"):
        command("mProjectCube", f"{WORK}/Antennae_CO_{name}_3D.fits", f"projected/{name}.fits", "mosaic.hdr")
    command("mImgtbl", "projected", "projected.tbl")
    command("mAddCube", "-p", "projected", "-a", "mean", "projected.tbl", "mosaic.hdr", "mosaic.fits")

    def onOurGrid(montageFile, oursFile):
        m, mWCS = cube(os.path.join(montage, montageFile))
        ours, oursWCS = cube(f"{WORK}/{oursFile}")
        # Montage crops its outputs: integer offset on the SlicerAstro grid
        corner = oursWCS.world_to_pixel(mWCS.pixel_to_world(np.array([0.]), np.array([0.])))
        ox, oy = int(round(float(corner[0][0]))), int(round(float(corner[1][0])))
        placed = np.full(ours.shape, np.nan)
        y0, x0 = max(0, oy), max(0, ox)
        part = m[:, max(0, -oy):, max(0, -ox):][:, :ours.shape[1] - y0, :ours.shape[2] - x0]
        placed[:, y0:y0 + part.shape[1], x0:x0 + part.shape[2]] = part
        return placed, ours

    for tag, montageFile, oursFile in (("R on CO South grid: mProject / exact", "R_on_south.fits", "Rfk5_on_south_exact.fits"),
                                       ("R on CO South grid: mProject / area", "R_on_south.fits", "Rfk5_on_south_area.fits"),
                                       ("R pixels x2: mShrink / exact", "R_shrink2.fits", "Rfk5_shrink2_exact.fits"),
                                       ("R pixels x2: mShrink / area", "R_shrink2.fits", "Rfk5_shrink2_area.fits"),
                                       ("CO North, TAN rotated: mProjectCube / exact", "north_tan_rot.fits", "north_tan_rot_exact.fits"),
                                       ("CO mean mosaic: mAddCube / mean exact", "mosaic.fits", "mosaic_mean_exact.fits")):
        placed, ours = onOurGrid(montageFile, oursFile)
        compareArrays(tag, placed, ours)
    # arbiter for the exact overlap: astropy reproject_exact on the same input and grid
    data, dataWCS = cube(f"{WORK}/Antennae_R_FK5.fits")
    ours, oursWCS = cube(f"{WORK}/Rfk5_on_south_exact.fits")
    exact = reproject_exact((data[0], dataWCS), oursWCS, shape_out=ours.shape[1:], return_footprint=False)[None]
    placed, _ = onOurGrid("R_on_south.fits", "Rfk5_on_south_exact.fits")
    compareArrays("arbiter: astropy exact / SlicerAstro exact", exact, ours)
    compareArrays("arbiter: astropy exact / Montage mProject", exact, placed)

print("radio-beam (common beam, convolution, deconvolution)")
cases = json.load(open(f"{WORK}/beams.json"))
ours = json.load(open(f"{WORK}/beams_slicerastro.json"))
area = lambda beam: beam[0] * beam[1]
valid = sum(result["found"] and all(result["deconvolvable"]) for result in ours)
ratios = np.array([area(result["common"]) / area(case["radiobeam_common"])
                   for case, result in zip(cases, ours) if case["radiobeam_common"] and result["found"]])
print(f"  common beam: valid (all beams deconvolvable) {valid}/{len(ours)}; radio-beam converged {len(ratios)}/{len(cases)}; "
      f"area SlicerAstro / radio-beam min {ratios.min():.4f} median {np.median(ratios):.4f} max {ratios.max():.4f}")
convolution = max(max(abs(r["convolved01"][i] - c["convolved01"][i]) for i in (0, 1)) for c, r in zip(cases, ours))
roundTrip = max(max(abs(r["back0"][i] - c["beams"][0][i]) for i in (0, 1)) for c, r in zip(cases, ours))
print(f"  convolution: max axis difference {convolution:.1e}\"; deconvolution of the convolution: max error {roundTrip:.1e}\"")

print("resolution and channels (astropy convolution, numpy)")
import astropy.units as u
from astropy.convolution import convolve_fft
from radio_beam import Beam
from scipy import ndimage
hdu = fits.open(f"{WORK}/Antennae_CO_South_3D.fits")[0]
data, header = hdu.data.astype(np.float64), hdu.header
beam = Beam(header["BMAJ"] * u.deg, header["BMIN"] * u.deg, header["BPA"] * u.deg)
target = Beam(3 * u.arcsec, 3 * u.arcsec, 0 * u.deg)
kernel = target.deconvolve(beam).as_kernel(abs(header["CDELT2"]) * u.deg)
scale = (target.sr / beam.sr).decompose().value  # Jy/beam: the peak of point sources is kept (CASA)
reference = np.stack([convolve_fft(plane, kernel, nan_treatment="interpolate", preserve_nan=True) for plane in data]) * scale
interior = np.stack([ndimage.binary_erosion(np.isfinite(plane), iterations=25) for plane in data])
smoothed, _ = cube(f"{WORK}/south_target3.fits")
reference[~interior] = np.nan
smoothed[~interior] = np.nan
compareArrays("CO South to a 3\" beam (25 px from blanks)", reference, smoothed)
channels, _ = cube(f"{WORK}/south_chan2.fits")
compareArrays("CO South channels x2: mean of channel pairs", 0.5 * (data[0:34:2] + data[1:35:2]), channels[:17])
