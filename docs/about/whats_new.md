# What's new in SlicerAstro 2.0

SlicerAstro 2.0 is a port to current 3D Slicer (5.13, VTK 9.6, Qt 5 and
Qt 6) with new features.

## Data

- Current libraries: CFITSIO 4.7, WCSLIB 8.10, 3DBarolo 1.8; ERFA for the
  reference frames.
- FITS: gzip (`.fits.gz`, without temporary copies in memory) and
  tile-compressed (`.fz`) files, cubes of more than 2³¹ voxels, integer
  masks (SoFiA), headers kept when saving, frequency and wavelength axes,
  CO and other lines.
- New sample data: NGC 2403, NGC 891, NGC 3198, NGC 4631 (HALOGAS), the
  Antennae galaxies (HI, CO, MUSE, optical), WEIN069 with its SoFiA mask and
  catalogue.

## Display and interaction

- World coordinate axes placed exactly on the sky (rotated data, position-
  velocity slices), the beam drawn on the sky; faster interaction (also on
  Qt 6 and Wayland).
- Masks and contours of each volume as segmentations; FITS masks; the Cloud
  Lasso effect.
- Measurements of markups on the sky: separations, lengths, areas, angles,
  velocity differences.
- Scenes restore the colour functions of the data.

## Analysis

- AstroReproject: new engine for reprojection, resampling (flux
  conserving), spectral regridding with reference frames, common and target
  beams, mosaics; validated against astropy, CASA and Montage.
- AstroSmoothing: multithreaded CPU filters, fixed GPU filters.
- AstroModeling: 3DBarolo 1.8, fits in the background (cancel), corrected
  rotation curves.
- AstroSoFiACatalog: SoFiA and other source catalogues.
- All long computations in the background, with progress and cancel.
