"""Real-data FITS files of the AstroFITSRealDataTest and their expected values.

  python prepare_real_data.py <raw dir> <sample data dir> <output dir>

<raw dir>: the files downloaded from the archives (see SOURCES); <sample
data dir>: the SampleData-v2 files. Writes to <output dir> the cutouts of the
TestingData-v1 release (original headers; only the reference pixels moved by
the crop) and, next to this script, one JSON file per test file with its
expected values computed with astropy: dimensions, voxel values and the world
coordinates of five voxels (the spectral axis converted as SlicerAstro shows
it: radio velocity for frequency axes, optical velocity otherwise).
"""
import gzip
import hashlib
import json
import os
import shutil
import sys
import warnings

import numpy as np
from astropy.io import fits
from astropy.wcs import WCS, FITSFixedWarning

warnings.simplefilter("ignore", FITSFixedWarning)
RAW, SAMPLES, OUT = sys.argv[1:4]
HERE = os.path.dirname(os.path.abspath(__file__))
TESTING_URL = "https://github.com/Punzo/SlicerAstro/releases/download/TestingData-v1/"
SAMPLE_URL = "https://github.com/Punzo/SlicerAstro/releases/download/SampleData-v2/"
os.makedirs(OUT, exist_ok=True)

# (test file, archive file, HDU, crop (x0, y0, nx, ny) or None, description)
SOURCES = [
    ("NVSS_M51.fits", "nvss/nvss_skyview_m51.fits", 0, None,
     "NVSS 1.4 GHz radio continuum of M51 (VLA; SkyView resampling, SIN)"),
    ("2MASS_J_M51.fits.gz", "2mass/2mass_j.fits.gz", 0, (150, 600, 200, 220),
     "2MASS J-band atlas image near M51 (IRSA; SIN with CROTA2, gzip compressed)"),
    ("SDSS_r_M51.fits", "sdss/frame-r.fits.bz2", 0, (900, 600, 300, 240),
     "SDSS DR17 r-band frame 3699-6-99 (TAN with a rotated CD matrix, nanomaggy)"),
    ("HST_ACS_M51.fits", "hst/j97c31ruq_flt.fits", 1, (1800, 900, 260, 240),
     "HST ACS/WFC F814W exposure j97c31ruq (SCI extension: image in an extension, TAN-SIP)"),
    ("Chandra_ACIS_M51.fits", "chandra/acisf13813N002_cntr_img2.fits.gz", 0, (380, 230, 260, 260),
     "Chandra ACIS image of M51, ObsID 13813 (integer counts, physical WCS)"),
]
# SampleData-v2 files: tested as they are (not copied to TestingData-v1)
SAMPLES_TESTED = [
    ("WEIN069.fits", "WHISP HI cube (WSRT)"),
    ("NGC3198.fits", "HALOGAS HI cube (WSRT)"),
    ("NGC2403_DSS.fits", "DSS2 optical image (plate solution and standard WCS)"),
    ("Antennae_HI.fits", "VLA HI cube (AIPS header, FELO-OBS, Stokes axis)"),
    ("Antennae_CO_North.fits", "ALMA CO(2-1) cube (CASA header, FREQ, Stokes axis)"),
    ("Antennae_MUSE_Center02.fits", "VLT/MUSE cube (CD matrix, wavelength axis)"),
    ("Antennae_R.fits", "CTIO R-band image (ARC, CD matrix)"),
]


def readHDU(path, index):
    opener = fits.open
    if path.endswith(".bz2"):
        import bz2
        with bz2.open(path, "rb") as f:
            hdul = fits.HDUList.fromstring(f.read())
        return hdul[index].header.copy(), np.array(hdul[index].data)
    with opener(path) as hdul:
        return hdul[index].header.copy(), np.array(hdul[index].data)


def crop(header, data, box):
    """Cutout of the last two axes; the reference pixels (also of the
    alternate WCS and of the physical coordinates, LTV) follow the crop."""
    x0, y0, nx, ny = box
    data = data[..., y0:y0 + ny, x0:x0 + nx]
    header = header.copy()
    for key in list(header.keys()):
        for axis, offset in (("1", x0), ("2", y0)):
            if key.startswith("CRPIX" + axis) or key == "LTV" + axis:
                header[key] = header[key] - offset if key.startswith("CRPIX") else header[key] - offset
    header["NAXIS1"], header["NAXIS2"] = nx, ny
    return header, data


def write(name, header, data, extension):
    path = os.path.join(OUT, name)
    # lookup-table distortions refer to extensions that are not copied (the SIP
    # polynomial of HST images is kept)
    prefixes = ("D2IM", "CPDIS", "CPERR", "DP1", "DP2", "DQ1", "DQ2", "NPOLEXT")
    for keyword in [card.keyword for card in header.cards if card.keyword.startswith(prefixes)]:
        header.remove(keyword, ignore_missing=True, remove_all=True)
    if extension:
        hdul = fits.HDUList([fits.PrimaryHDU(), fits.ImageHDU(data, header, name=header.get("EXTNAME", "SCI"))])
    else:
        hdul = fits.HDUList([fits.PrimaryHDU(data, header)])
    if path.endswith(".gz"):
        with gzip.open(path, "wb") as f:
            hdul.writeto(f)
    else:
        hdul.writeto(path, overwrite=True)
    return path


def displayWCS(header):
    """WCS as SlicerAstro shows it: spectral axis as a velocity."""
    wcs = WCS(header)
    while wcs.naxis > 3:
        wcs = wcs.dropaxis(wcs.naxis - 1)
    if wcs.naxis == 3 and wcs.wcs.spec >= 0:
        ctype = wcs.wcs.ctype[wcs.wcs.spec][:4]
        if ctype in ("VRAD", "VOPT"):
            target = None
        elif ctype in ("FREQ", "AFRQ", "ENER", "WAVN"):
            target = "VRAD-???"
        else:
            target = "VOPT-???"
        if target:
            if wcs.wcs.has_cd():
                # (WCSLIB sptr loses the spectral step of a CD matrix: PC and
                # CDELT form first, as SlicerAstro does)
                cd = np.array(wcs.wcs.cd)
                cdelt = np.array([cd[i, i] if cd[i, i] != 0. else 1. for i in range(cd.shape[0])])
                del wcs.wcs.cd
                wcs.wcs.cdelt = cdelt
                wcs.wcs.pc = cd / cdelt[:, None]
                wcs.wcs.set()
            wcs.wcs.sptr(target, wcs.wcs.spec)
        wcs.wcs.set()
    return wcs


def truth(name, path, url, description):
    with fits.open(path) as hdul:
        index = next(i for i, h in enumerate(hdul) if h.is_image and h.data is not None and h.data.ndim >= 2)
        header, data = hdul[index].header, np.array(hdul[index].data, dtype=np.float64)
        bitpix = header["BITPIX"]
    while data.ndim > 3:
        data = data[0]
    if data.ndim == 2:
        data = data[None]
    nz, ny, nx = data.shape
    wcs = displayWCS(header)
    rng = np.random.default_rng(7)
    voxels = [(0, 0, 0), (nx - 1, ny - 1, nz - 1), (nx // 2, ny // 2, nz // 2)]
    voxels += [tuple(int(v) for v in (rng.integers(nx), rng.integers(ny), rng.integers(nz))) for _ in range(3)]
    finite = np.argwhere(np.isfinite(data))
    if len(finite):
        k, j, i = finite[len(finite) // 3]
        voxels.append((int(i), int(j), int(k)))
    blank = np.argwhere(~np.isfinite(data))
    if len(blank):
        k, j, i = blank[len(blank) // 2]
        voxels.append((int(i), int(j), int(k)))
    values = [[i, j, k, None if not np.isfinite(data[k, j, i]) else float(data[k, j, i])] for i, j, k in voxels]
    points = [(0, 0, 0), (nx - 1, 0, nz - 1), (0, ny - 1, nz // 2), (nx - 1, ny - 1, 0), (nx // 2, ny // 2, nz // 2)]
    world = []
    for i, j, k in points:
        pixel = [i, j, k][:wcs.naxis]
        w = wcs.all_pix2world([pixel], 0)[0]
        world.append([i, j, k] + [float(v) for v in w])
    with open(path, "rb") as f:
        sha256 = hashlib.sha256(f.read()).hexdigest()
    result = {"file": name, "url": url, "sha256": sha256, "description": description, "bitpix": bitpix,
              "dimensions": [nx, ny, nz], "voxels": values, "world": world,
              "spectral": wcs.wcs.ctype[2] if wcs.naxis == 3 else None}
    with open(os.path.join(HERE, os.path.splitext(name.replace(".gz", ""))[0] + ".json"), "w") as f:
        json.dump(result, f, indent=1)
    print(f"{name:30s} {nx}x{ny}x{nz} BITPIX {bitpix} {os.path.getsize(path) / 1024:.0f} KB {list(wcs.wcs.ctype)}")


for name, raw, hdu, box, description in SOURCES:
    header, data = readHDU(os.path.join(RAW, raw), hdu)
    if box:
        header, data = crop(header, data, box)
    path = write(name, header, data, extension=(hdu > 0))
    truth(name, path, TESTING_URL + name, description)
for name, description in SAMPLES_TESTED:
    truth(name, os.path.join(SAMPLES, name), SAMPLE_URL + name, description)
with open(os.path.join(OUT, "SHA256SUMS"), "w") as f:
    for name, *_ in SOURCES:
        with open(os.path.join(OUT, name), "rb") as g:
            f.write(f"{hashlib.sha256(g.read()).hexdigest()}  {name}\n")
shutil.copy(os.path.abspath(__file__), os.path.join(OUT, os.path.basename(__file__)))
