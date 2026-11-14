# Getting started

## Load data

- **FITS files** (`.fits`, `.fit`, `.fts`, gzip-compressed `.fits.gz` and
  tile-compressed `.fz`): drag and drop them on the 3D Slicer window, or use
  *File > Add Data*. Images (2D) and data cubes (3D; a fourth Stokes axis of
  length 1 is accepted) are loaded with their world coordinates (WCS).
  FITS masks (e.g. SoFiA masks, `DATAMODEL = MASK`) are loaded as
  segmentations.
- **Sample data**: open the **Sample Data** module, category **Astronomy**
  (see [Sample data](sample_data.md)).

Once loaded, a data cube is shown in the slice views (sky plane and the two
position-velocity planes) and in the 3D view (volume rendering).

## Views and coordinates

- The slice views show **WCS axes** (right ascension, declination,
  velocity, or the angular offset along position-velocity slices) and the
  **beam** of the data (bottom left of the sky plane).
- The **Data Probe** panel (bottom left of the main window) shows the world
  coordinates and the value of the voxel under the mouse.
- The **AstroVolume** module sets the colour tables, the window and level,
  the 3D colour and opacity functions (presets), the contours and the masks.

## Typical analysis of an HI data cube

1. Load the cube (e.g. the **WEIN069** sample).
2. **AstroSmoothing**: smooth the cube to bring out faint emission, and
   optionally generate a mask of the emission.
3. **AstroVolume**: inspect the data in 2D and 3D, draw contours, edit the
   mask (Segment Editor, Cloud Lasso).
4. **AstroMomentMaps**: total intensity (moment 0), velocity field
   (moment 1) and velocity dispersion (moment 2) maps.
5. **AstroProfiles**: integrated spectrum.
6. **AstroPVSlice** and **AstroPVDiagram**: position-velocity diagrams.
7. **AstroModeling**: tilted-ring model of the kinematics (3DBarolo).
8. **AstroReproject**: overlay the data on images at other wavelengths, or
   combine several observations.

Markups (lines, curves, angles) measure distances, areas and angles on the
sky and velocity differences: see [Measurements](measurements.md).
