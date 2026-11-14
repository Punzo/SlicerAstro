"""Inputs of the validation of the AstroReproject engine (astropy, radio-beam).

  python prepare_inputs.py <Antennae sample data dir> <work dir>

Writes to <work dir>: 3D copies of the Antennae CO cubes (without the Stokes
axis, for casacore and Montage), the R-band image labelled FK5 J2000 (no
FK5 <-> ICRS conversion, whose conventions differ between ERFA and casacore)
and random beam sets with the radio-beam results (beams.json)."""
import json
import os
import sys
import warnings

import numpy as np
import astropy.units as u
from astropy.io import fits
from astropy.wcs import WCS

warnings.simplefilter("ignore")
DATA, WORK = sys.argv[1], sys.argv[2]
os.makedirs(WORK, exist_ok=True)

for name in ("Antennae_CO_South", "Antennae_CO_North"):
    with fits.open(f"{DATA}/{name}.fits") as hdul:
        header, data = hdul[0].header, hdul[0].data[0]
        new = WCS(header).dropaxis(3).to_header()
        for key in ("BUNIT", "BMAJ", "BMIN", "BPA", "OBJECT", "TELESCOP", "DATE-OBS"):
            if key in header:
                new[key] = header[key]
        fits.PrimaryHDU(data, new).writeto(f"{WORK}/{name}_3D.fits", overwrite=True)

with fits.open(f"{DATA}/Antennae_R.fits") as hdul:
    header = hdul[0].header.copy()
    header["RADESYS"], header["EQUINOX"] = "FK5", 2000.0
    fits.PrimaryHDU(hdul[0].data, header).writeto(f"{WORK}/Antennae_R_FK5.fits", overwrite=True)

from radio_beam import Beam, Beams
rng = np.random.default_rng(7)
cases = []
for _ in range(200):
    count = int(rng.integers(2, 7))
    major = rng.uniform(1., 5., count)
    minor = major * rng.uniform(0.3, 1., count)
    pa = rng.uniform(-90., 90., count)
    try:
        common = Beams(major=major * u.arcsec, minor=minor * u.arcsec, pa=pa * u.deg).common_beam(tolerance=1e-6)
        commonBeam = [common.major.to_value(u.arcsec), common.minor.to_value(u.arcsec), common.pa.to_value(u.deg)]
    except Exception:
        commonBeam = None  # radio-beam did not converge
    convolved = Beam(major[0] * u.arcsec, minor[0] * u.arcsec, pa[0] * u.deg).convolve(
        Beam(major[1] * u.arcsec, minor[1] * u.arcsec, pa[1] * u.deg))
    cases.append({"beams": [[float(a), float(b), float(c)] for a, b, c in zip(major, minor, pa)],
                  "radiobeam_common": commonBeam,
                  "convolved01": [convolved.major.to_value(u.arcsec), convolved.minor.to_value(u.arcsec),
                                  convolved.pa.to_value(u.deg)]})
json.dump(cases, open(f"{WORK}/beams.json", "w"))
print("inputs written to", WORK)
