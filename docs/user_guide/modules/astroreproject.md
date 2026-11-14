# AstroReproject

Reprojection, resampling and mosaicking: the input volumes are put on a new
grid of sky pixels and channels. The module panel has a *Quick guide*; each
option has a tooltip and the text under each choice explains the selected
option. The summary at the bottom of *Output grid* shows the grid that will
be created.

## Common tasks

| Task | Settings |
|---|---|
| Overlay or compare two data sets (e.g. HI contours on an optical image) | Sky grid: *Grid of a reference volume*; Reference: the other data |
| Larger or smaller pixels | Sky grid: *Input grid, other pixel size*; factor 2: pixels twice as large (use *Area average* or *Exact overlap*) |
| North up, another projection or celestial frame | Sky grid: *Manual*, *From input, north up*, then edit |
| Wider channels, frequency to velocity, another spectral frame | Channels: *Input channels, other width* or *Manual* |
| Same resolution for several data sets | Resolution: *Common beam of the inputs* |
| Mosaic of several pointings or fields | Check all of them in *Input volumes*; Sky grid and Channels: *Covering all the inputs*; primary beam options in *Combination* |
| Different lines (e.g. CO on the channels of HI) | Channels: *Channels of the reference volume*; Match channels by: *Velocity* |

## Output grid

- **Sky grid**: the grid of a reference volume, of the input (optionally
  with larger or smaller pixels), a grid covering all the inputs (union) or
  their common area (intersection), or a manual grid (celestial frame,
  equinox, projection, centre, pixel size, rotation, number of pixels).
- **Celestial frames**: ICRS, FK5 and FK4 at any equinox, Galactic,
  Supergalactic; the inputs are converted (ERFA, the conventions of
  astropy).
- **Channels**: those of the input or of a reference volume, wider or
  narrower channels, channels covering the inputs or their common range, or
  manual channels: spectral axis (radio, optical or relativistic velocity,
  frequency, wavelength), spectral frame (topocentric, geocentric,
  barycentric, LSRK, LSRD, galactocentric, Local Group, CMB), rest
  frequency, first channel, width and number.
- **Match channels by** frequency, or by velocity: each input with its own
  rest frequency (as CASA `imregrid`).

## Resampling

| Sky | |
|---|---|
| Nearest neighbour | value of the closest pixel (masks, labels) |
| Bilinear | default, for output pixels similar to or smaller than the input pixels |
| Bicubic | cubic convolution (as CASA) |
| Lanczos-3 | sharpest interpolation |
| Area average | flux conserving, for larger output pixels |
| Exact overlap | exact flux conservation (spherical overlap of the pixels, as Montage and astropy reproject) |

Channels: nearest, linear or cubic interpolation, or the flux-conserving
*Channel overlap* (averages the noise down); *Automatic* uses the overlap for
wider output channels. Jy/pixel data are scaled by the ratio of the pixel
areas. Output pixels covered by data for less than the *Minimum coverage* are
blank.

## Resolution

The inputs can be convolved to their **common beam** (the smallest beam all
their beams fit in, the algorithm of CASA) or to a **target beam**, before
the resampling. Jy/beam data keep the peak of point sources; Jy/pixel data
become Jy/beam.

## Combination (mosaics)

With several inputs, overlapping data are combined: mean, or linear mosaic
(Sault, Staveley-Smith & Brouw 1996): optimal (primary beam squared) or
primary beam weights, optionally with 1/noise² weights (Miriad `linmos`).
The inputs can be primary beam corrected or not; the primary beams are
Gaussian or Airy models (dish diameter, FWHM factor, cutoff) centred on the
pointings, or volumes. Optional outputs: weight, sensitivity (noise) and
Sault-weighted volumes.

## Accuracy

The results agree with astropy reproject, casacore (CASA) and Montage within
the rounding of the data: see
[Validation of the reprojection](../../developer_guide/reprojection_validation.md).
