"""Comparison of the outputs of the SlicerAstro analysis modules
(slicer_outputs.py) with independent implementations:

- moment maps and integrated spectra: spectral-cube (radio-astro-tools,
  astropy units; Jy/beam -> Jy with the beam area of radio-beam);
- statistics in a mask: numpy and casacore (the image library of CASA).

Run with Python and spectral-cube, radio-beam, python-casacore:

    python compare.py <output folder of slicer_outputs.py>

Prints the largest differences, relative to the largest absolute value of
each quantity.
"""

import json
import os
import sys
import warnings

import numpy as np
from astropy import units as u
from astropy.io import fits
from spectral_cube import SpectralCube

warnings.simplefilter("ignore")


def relative(computed, expected):
    computed = np.asarray(computed, dtype=np.float64)
    expected = np.asarray(expected, dtype=np.float64)
    finite = np.isfinite(computed) & np.isfinite(expected)
    scale = np.nanmax(np.abs(expected[finite])) if np.any(finite) else 1.
    blankMismatch = int(np.count_nonzero(np.isfinite(computed) != np.isfinite(expected)))
    difference = np.max(np.abs(computed[finite] - expected[finite])) / scale if np.any(finite) else 0.
    return difference, blankMismatch


def spectralCube(entry):
    """The cube with standard units in the header (as WCSLIB unitfix and
    the FITS reader of SlicerAstro do for HZ, DEGREE, JY/BEAM, missing CUNIT3
    of an AIPS velocity axis)."""
    from astropy.wcs import WCS
    from radio_beam import Beam
    with fits.open(entry["cube"]) as hdus:
        header = hdus[0].header.copy()
        data = hdus[0].data.astype(np.float32).squeeze()
    fixes = {"HZ": "Hz", "KHZ": "kHz", "MHZ": "MHz", "GHZ": "GHz", "DEGREE": "deg", "DEGREES": "deg",
             "M/S": "m/s", "KM/S": "km/s"}
    for axis in (1, 2, 3):
        key = f"CUNIT{axis}"
        if key in header and str(header[key]).strip().upper() in fixes:
            header[key] = fixes[str(header[key]).strip().upper()]
    if "CUNIT3" not in header and str(header.get("CTYPE3", "")).startswith(("VELO", "VRAD", "VOPT")):
        header["CUNIT3"] = "m/s"
    # AIPS velocity axes (VELO-HEL, VELO-LSR): optical velocity unless
    # VELREF > 256 (radio), as translated by WCSLIB (spcaips) for SlicerAstro;
    # spectral-cube would read VELO as the relativistic velocity
    ctype3 = str(header.get("CTYPE3", ""))
    if ctype3.startswith("VELO-"):
        header["CTYPE3"] = "VRAD" if int(header.get("VELREF", 0)) > 256 else "VOPT"
        if "SPECSYS" not in header and ctype3.endswith("HEL"):
            header["SPECSYS"] = "BARYCENT"
    if str(header.get("BUNIT", "")).strip().upper() == "JY/BEAM":
        header["BUNIT"] = "Jy/beam"
    for key in [k for k in header if k.startswith(("NAXIS4", "CTYPE4", "CRVAL4", "CDELT4", "CRPIX4", "CUNIT4", "CROTA4"))]:
        del header[key]
    header["NAXIS"] = 3
    wcs = WCS(header).sub(3)
    unit = u.Unit(header["BUNIT"]) if str(header.get("BUNIT", "")).strip() else u.one
    cube = SpectralCube(data=data * unit, wcs=wcs, header=header)
    if header.get("BMAJ", -1) and header.get("BMAJ", -1) > 0 and not _beamDefined(cube):
        cube = cube.with_beam(Beam.from_fits_header(header), raise_error_jybm=False)
    if str(header["CTYPE3"]).startswith(("VOPT", "VRAD")):
        # already the velocity of SlicerAstro: kept as it is (in m/s; the
        # moments are converted to km/s). spectral-cube's with_spectral_unit
        # relinearizes an optical velocity axis, also for a change of unit only
        pass
    else:
        # frequency or wavelength axis: velocity with the convention and the
        # rest frequency used by SlicerAstro
        convention = "optical" if entry["velocityDefinition"].startswith("VOPT") else "radio"
        cube = cube.with_spectral_unit(u.km / u.s, velocity_convention=convention,
                                       rest_value=entry["restFrequency"] * u.Hz)
    return cube


def maskOf(entry):
    labels = fits.getdata(entry["maskFile"])
    return labels == entry["label"]


def compareDataset(folder, name, entry):
    print(f"\n== {name} ({os.path.basename(entry['cube'])}, {entry['velocityDefinition']})")
    cube = spectralCube(entry)
    data = cube.unmasked_data[:].value.astype(np.float64)
    mask = maskOf(entry)
    rows = []
    for key in ("threshold", "mask"):
        result = entry[key]
        if key == "threshold":
            m = result["moments"]
            # SlicerAstro: intensity in (min, max), velocity range of the parameters
            selection = (data > m["intensityMin"]) & (data < m["intensityMax"])
        else:
            selection = mask & np.isfinite(data)
        selected = cube.with_mask(selection)
        # moment 0: spectral-cube integrates with the absolute channel width
        references = {"m0": (selected.moment0().to(u.Jy / u.beam * u.km / u.s).value
                             if cube.unit == u.Jy / u.beam else selected.moment0().value),
                      "m1": selected.moment1().to(u.km / u.s).value,
                      "m2": selected.linewidth_sigma().to(u.km / u.s).value}
        for moment, reference in references.items():
            computed = fits.getdata(os.path.join(folder, result["moments"]["files"][moment])).squeeze()
            if moment == "m2":
                # pixels with one channel: dispersion 0 in spectral-cube, blank in SlicerAstro
                reference = np.where(np.abs(reference) < 1e-6, 0., reference)
                computed = np.where(np.isnan(computed) & (reference == 0.), 0., computed)
            if moment == "m0":
                # spectral-cube: 0 where no voxel is selected, SlicerAstro: blank
                reference = np.where(np.isfinite(computed) | (reference != 0.), reference, np.nan)
                computed = np.where(np.isnan(computed) & (reference == 0.), 0., computed)
            difference, blanks = relative(computed, reference)
            rows.append((f"moment {moment[1]} ({key})", difference, blanks))

        # integrated spectrum: Jy (pixel area / beam area, radio-beam) or the sum
        # of the intensities; 0 without selected voxels, blank for channels
        # without data
        if key == "threshold":
            p = result["profile"]
            profileSelection = (data > p["intensityMin"]) & (data < p["intensityMax"])
        else:
            profileSelection = selection
        fluxes = _profile(cube, profileSelection, data)
        computed = np.array([np.nan if v is None else v for v in result["profile"]["values"]])
        difference, blanks = relative(computed, fluxes)
        rows.append((f"profile ({key})", difference, blanks))
        if "profileAperture" in result:
            footprint = np.broadcast_to(mask.any(axis=0), data.shape) & np.isfinite(data)
            fluxes = _profile(cube, footprint, data)
            computed = np.array([np.nan if v is None else v for v in result["profileAperture"]["values"]])
            difference, blanks = relative(computed, fluxes)
            rows.append((f"profile (aperture)", difference, blanks))

    rows += compareStatistics(entry, data, mask, cube)
    for label, difference, blanks in rows:
        print(f"  {label:28s} max |difference| / max |value| = {difference:9.2e}   blank mismatches: {blanks}")
    return rows


def _beamDefined(cube):
    try:
        return cube.beam is not None and cube.beam.major.value > 0
    except (AttributeError, ValueError):
        return False


def _profile(cube, selection, data):
    selected = cube.with_mask(selection)
    if cube.unit == u.Jy / u.beam and _beamDefined(cube):
        fluxes = selected.to(u.Jy / u.pix).sum(axis=(1, 2)).value
    else:
        fluxes = selected.sum(axis=(1, 2)).value
    fluxes = np.where(np.isnan(fluxes), 0., fluxes)
    return np.where(np.all(np.isnan(data), axis=(1, 2)), np.nan, fluxes)


def compareStatistics(entry, data, mask, cube):
    computed = entry["mask"]["statistics"]
    values = data[mask & np.isfinite(data)]
    expected = {"Npixels": values.size, "Min": values.min(), "Max": values.max(), "Mean": values.mean(),
                "Std": values.std(), "Median": np.median(values), "Sum": values.sum()}
    if _beamDefined(cube):
        pixelArea = abs(cube.wcs.celestial.proj_plane_pixel_area().to(u.sr).value)
        expected["TotalFlux"] = values.sum() * pixelArea / cube.beam.sr.value
    rows = []
    for key, value in expected.items():
        rows.append((f"statistics {key}", abs(computed[key] - value) / max(abs(value), 1e-30), 0))

    # casacore (CASA image statistics) on the same voxels
    try:
        from casacore.images import image
        masked = np.ma.masked_array(data.astype(np.float32), mask=~(mask & np.isfinite(data)))
        casaImage = image("", values=masked)
        stats = casaImage.statistics()
        casa = {"Npixels": float(stats["npts"][0]), "Min": float(stats["min"][0]), "Max": float(stats["max"][0]),
                "Mean": float(stats["mean"][0]), "Median": float(stats["median"][0]), "Sum": float(stats["sum"][0])}
        for key, value in casa.items():
            rows.append((f"statistics {key} (casacore)", abs(computed[key] - value) / max(abs(value), 1e-30), 0))
        # CASA's sigma uses N - 1 (sample standard deviation)
        sampleStd = float(stats["sigma"][0]) * np.sqrt((values.size - 1.) / values.size)
        rows.append(("statistics Std (casacore)", abs(computed["Std"] - sampleStd) / sampleStd, 0))
    except ImportError:
        print("  (python-casacore not available)")
    return rows


def main():
    folder = sys.argv[1]
    with open(os.path.join(folder, "slicerastro.json")) as f:
        results = json.load(f)
    worst = 0.
    for name, entry in results.items():
        for _, difference, _ in compareDataset(folder, name, entry):
            worst = max(worst, difference)
    print(f"\nlargest relative difference: {worst:.2e}")


if __name__ == "__main__":
    main()
