# Validation of the analysis modules

Developer validation (not a ctest) of AstroMomentMaps, AstroProfiles and
AstroStatistics, without and with an input mask (a segment), against
independent implementations:

- moment maps (0, 1, 2) and integrated spectra: spectral-cube
  (radio-astro-tools, built on astropy; the conversion Jy/beam -> Jy uses the
  beam area of radio-beam);
- statistics in a mask: numpy and casacore (the image library of CASA,
  `image.statistics`).

Data: the sample data WEIN069 (frequency axis, shown as radio velocity; mask:
the largest source of its SoFiA mask) and NGC 2403 (AIPS `VELO-HEL` axis,
optical velocity; beam 29.7 x 29.3 arcsec; mask: emission above 3 sigma,
dilated by 2 voxels, made in Slicer and saved as a FITS mask). Moment maps
and spectra without mask use the voxels above 3 sigma (the defaults of the
modules otherwise); with the mask, the voxels of the segment (all channels).

## How to run

```
SlicerWithSlicerAstro --no-splash --python-script slicer_outputs.py <work dir>
# Python with astropy, spectral-cube, radio-beam, python-casacore
python compare.py <work dir>
```

## Results (2026-11-17)

astropy 8.0.2, spectral-cube 0.7.0, radio-beam 0.3.10, python-casacore
3.8.1. Largest |difference| / largest |value| of each quantity, and the
number of pixels blank in only one of the two results. Profiles: threshold,
voxels of the mask, and aperture (sky area of the mask, all channels).

```
== WEIN069 (WEIN069.fits, VRAD)
  moment 0 (threshold)         max |difference| / max |value| =  1.50e-07   blank mismatches: 0
  moment 1 (threshold)         max |difference| / max |value| =  2.34e-07   blank mismatches: 0
  moment 2 (threshold)         max |difference| / max |value| =  7.19e-07   blank mismatches: 0
  profile (threshold)          max |difference| / max |value| =  6.05e-08   blank mismatches: 0
  moment 0 (mask)              max |difference| / max |value| =  1.96e-07   blank mismatches: 0
  moment 1 (mask)              max |difference| / max |value| =  4.86e-07   blank mismatches: 0
  moment 2 (mask)              max |difference| / max |value| =  4.27e-06   blank mismatches: 0
  profile (mask)               max |difference| / max |value| =  7.92e-08   blank mismatches: 0
  profile (aperture)           max |difference| / max |value| =  8.93e-08   blank mismatches: 0
  statistics Npixels           max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Min               max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Max               max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Mean              max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Std               max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Median            max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Sum               max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Npixels (casacore) max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Min (casacore)    max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Max (casacore)    max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Mean (casacore)   max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Median (casacore) max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Sum (casacore)    max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Std (casacore)    max |difference| / max |value| =  1.34e-15   blank mismatches: 0
== NGC2403 (NGC2403.fits, VOPT)
  moment 0 (threshold)         max |difference| / max |value| =  2.44e-07   blank mismatches: 0
  moment 1 (threshold)         max |difference| / max |value| =  5.98e-07   blank mismatches: 0
  moment 2 (threshold)         max |difference| / max |value| =  1.51e-07   blank mismatches: 0
  profile (threshold)          max |difference| / max |value| =  9.01e-08   blank mismatches: 0
  moment 0 (mask)              max |difference| / max |value| =  2.82e-07   blank mismatches: 0
  moment 1 (mask)              max |difference| / max |value| =  5.64e-07   blank mismatches: 0
  moment 2 (mask)              max |difference| / max |value| =  1.71e-07   blank mismatches: 0
  profile (mask)               max |difference| / max |value| =  8.85e-08   blank mismatches: 0
  profile (aperture)           max |difference| / max |value| =  1.77e-07   blank mismatches: 0
  statistics Npixels           max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Min               max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Max               max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Mean              max |difference| / max |value| =  1.73e-16   blank mismatches: 0
  statistics Std               max |difference| / max |value| =  1.01e-15   blank mismatches: 0
  statistics Median            max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Sum               max |difference| / max |value| =  2.67e-16   blank mismatches: 0
  statistics TotalFlux         max |difference| / max |value| =  3.86e-16   blank mismatches: 0
  statistics Npixels (casacore) max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Min (casacore)    max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Max (casacore)    max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Mean (casacore)   max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Median (casacore) max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Sum (casacore)    max |difference| / max |value| =  0.00e+00   blank mismatches: 0
  statistics Std (casacore)    max |difference| / max |value| =  1.89e-14   blank mismatches: 0
largest relative difference: 4.27e-06
```

The differences of the moment maps are the rounding of the float32
accumulation of SlicerAstro (float32 cubes); profiles and statistics are
accumulated in double precision.

## Conventions (checked here)

- Moment 0 is the sum of the intensities times the channel width (the WCS
  channel width; until this validation it was the velocity range divided by
  the number of channels minus one, 1/(N-1) too large without a mask).
- Moment 2 is the intensity-weighted velocity dispersion,
  sqrt(sum I (v - m1)^2 / sum I) (spectral-cube `linewidth_sigma`). Pixels
  with emission in one channel only are blank in SlicerAstro (spectral-cube:
  0).
- Jy/beam -> Jy: pixel area / Gaussian beam area, pi / (4 ln 2) BMAJ BMIN
  (until this validation 1.13 BMAJ BMIN: 0.27% too large fluxes); only for
  data in Jy/beam with a beam (otherwise the sum of the intensities).
- Profiles: a channel without selected voxels is 0 (blank only without
  data). The aperture profile of WEIN069 shows a negative baseline (about
  -0.1 Jy/beam summed over the 1092 pixels of the source area, -0.3 sigma per
  pixel): a negative bowl of the data around the galaxy, not an error.
- Std is the population standard deviation (N); CASA's `sigma` uses N - 1.
- AIPS velocity axes (`VELO-HEL`, `VELO-LSR`) are optical velocities, linear
  in the header (`VELREF` < 256; WCSLIB `spcaips`). spectral-cube reads
  `VELO` as the relativistic velocity and its `with_spectral_unit`
  relinearizes optical velocity axes, also for a change of unit only: the
  comparison uses the axis of the header as WCSLIB translates it.
