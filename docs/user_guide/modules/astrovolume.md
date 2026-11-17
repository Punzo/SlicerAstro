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

Volume rendering with presets of the colour and opacity functions (3D colour
functions), defined in multiples of the noise σ of the data; a region of
interest (ROI) crops the rendering and measures the noise inside it (*RMS in
ROI*).

| 3D colour function | Rendering |
|---|---|
| Emission above 3σ · faint, medium, dense | all the emission above 3σ, constant opacity |
| Surface at 3σ · green, white | one surface at 3σ |
| Surfaces at 3σ, 7σ | two surfaces |
| Surfaces at 3σ, 7σ, 15σ | three surfaces, green, blue and red |
| Surfaces every 3σ · rainbow | eight surfaces, from 3σ to 24σ |
| Surfaces at 3σ, 7σ, 15σ · Observatory | three surfaces in colours safe for colour-blind people (teal, mint, amber), faint outside (default) |
| Surfaces every 3σ · Observatory | eight surfaces along the Observatory colour map |
| Emission 3σ–25σ · Observatory, magma, viridis | continuous rendering of the emission from 3σ to 25σ, opacity and colour rising with the intensity (magma and viridis are perceptually uniform colour maps) |

The names are shown in the selector; scenes store a stable identifier of
each function (e.g. `ThreeSurfaces`).

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
segment per source, also on their own (before or without their data cube).

Masks are always segmentations: SlicerAstro has no label map volumes. The
modules with an input mask (AstroMomentMaps, AstroProfiles, AstroStatistics,
AstroMasking, AstroModeling) use the selected segment directly, on the voxel
grid of their input data.

A FITS mask is attached to the data cube on the same voxel grid (compared
with the WCS), whichever is loaded first; the mask of a part of the cube
(e.g. SoFiA run on a cutout) is placed at its position in the cube and is
still saved on its own grid. All the astronomical data are centered on their
voxel grid in the views.

**Copy to** copies (or **Move** moves) the selected segments (all if none is
selected) to the mask of another volume: voxel by voxel on the same voxel
grid (e.g. a smoothed copy or a cutout), projected along the channels on a 2D
image of the same sky grid (e.g. a moment map), otherwise reprojected with
the WCS (nearest neighbour). A mask on another grid is not used directly as
the input of a module: copy it to the input data first.

### Astronomical terminology

The segments of astronomical masks and contours are described with an
astronomical terminology instead of Slicer's medical ones. In the Segment
Editor, double-click the color of a segment to choose its category and type:

| Category | Types |
|---|---|
| Source detection | source, source candidate, continuum source, absorption feature, artefact, noise peak |
| Galaxy | target galaxy, companion galaxy, background galaxy, foreground star |
| Gas structure | gas disk, warp, extraplanar gas, tidal tail, bridge, gas cloud, high-velocity cloud, ring, anomalous gas |
| Kinematics | approaching side, receding side, non-circular motion |
| Analysis region | mask, region of interest, noise region, exclusion region, contour, model |

New segments of a mask are *Mask*, the sources of a FITS mask (e.g. SoFiA)
*Source*, contours *Contour*, the outputs of 3DBarolo *Model*. The type of
each segment is saved in the FITS mask (keyword `SEGTRMn`) and read back. The
*SlicerAstro* color table has one color per type, with the recommended
colors of the terminology.
