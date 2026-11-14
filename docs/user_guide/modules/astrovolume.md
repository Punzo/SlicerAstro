# AstroVolume

Loads astronomical data (FITS) and sets how they are shown.

## Data

- FITS images and data cubes with their world coordinates (WCS, read with
  WCSLIB): celestial frames (ICRS, FK5, FK4, Galactic, ...), projections and
  spectral axes. Frequency axes are shown as radio velocities, wavelength
  axes as optical velocities; the file keeps its spectral axis when saved.
- Brightness units (`BUNIT`) and beam (`BMAJ`, `BMIN`, `BPA`); the noise of
  the data is estimated when they are loaded (*Display threshold*).
- *Dataset Information*: the FITS header and the properties of the volume.

## 2D display

Colour tables (including the astronomical *Heat*, *Ronekers* and
*VelocityField*), window and level, reverse, invert and logarithmic
scaling, thresholds. The slice views show the WCS axes and the beam.

## 3D display

Volume rendering with presets of the colour and opacity functions, which
follow the noise of the data; a region of interest (ROI) crops the rendering
and measures the noise inside it (*RMS in ROI*).

## Contours

Contours of the active data, or of other data (reprojected on the grid of
the active data). The levels are written in the *Contours* field:

| Syntax | Levels |
|---|---|
| `3;7;15` | a list of levels |
| `0.5,1;2,4` | a list of ranges (minimum, maximum) |
| `FIRST:LAST+STEP` | linearly spaced levels |
| `FIRST:LAST*FACTOR` | geometrically spaced levels |
| `DT ...` | levels in units of the noise of the data (e.g. `DT -3;3;7;15`) |
| `... overlay` | the contours of subsequent levels can overlap |
| `Histogram` | the range selected in the histogram plot |

The contours are segments (one per level) that can be shown, hidden,
smoothed (closed surfaces) and deleted. Contours of a data cube over a 2D
image are shown as a channel map: the *Channel* slider selects the channel.

## Masks

Each data volume has a mask (a segmentation of its grid), edited with the
Segment Editor (also the *Cloud Lasso* effect of SlicerAstro) and saved as a
FITS mask. FITS masks (e.g. from SoFiA) are loaded as segmentations, one
segment per source.
