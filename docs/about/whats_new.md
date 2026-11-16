# What's new in SlicerAstro 2.0

SlicerAstro 2.0 is a port to current 3D Slicer (5.13, VTK 9.6, Qt 5 and
Qt 6) with new features.

## Data

- Current libraries: CFITSIO 4.7, WCSLIB 8.10, 3DBarolo 1.8; ERFA for the
  reference frames.
- FITS: gzip (`.fits.gz`, without temporary copies in memory) and
  tile-compressed (`.fz`) files, cubes of more than 2³¹ voxels, integer
  masks (SoFiA), headers kept when saving, frequency and wavelength axes,
  CO and other lines, one-dimensional spectra with their spectral axis.
- Corrected GIPSY headers with optical velocities (FREQ-OHEL, FREQ-OLSR:
  the barycentric frequency of the reference pixel had the wrong sign of the
  velocity) and DSS images whose plate solution cannot be used (the standard
  WCS keywords are used instead).
- New sample data: NGC 2403, NGC 891, NGC 3198, NGC 4631 (HALOGAS), the
  Antennae galaxies (HI, CO, MUSE, optical), WEIN069 with its SoFiA mask and
  catalogue.

## Display and interaction

- World coordinate axes placed exactly on the sky (rotated data, position-
  velocity slices), the beam drawn on the sky; faster interaction (also on
  Qt 6 and Wayland).
- Masks and contours of each volume as segmentations (no label map
  volumes: the modules read their input mask from the segment); FITS masks,
  also loaded or saved on their own; the Cloud Lasso effect; an astronomical terminology of the segments (sources,
  galaxies, gas structures, kinematic and analysis regions).
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

## Not compatible with SlicerAstro 1.x

Scenes saved with SlicerAstro 1.x cannot be used: masks were label map
volumes (now segmentations) and the parameters of the modules changed. When
such a scene is loaded, SlicerAstro says so (and the message is in the Error
Log); its masks and module settings are not restored. Load the FITS files of
the data and masks instead, and save a new scene.

The 0th moment maps computed without a mask over the whole velocity range
are lower by a factor (N - 1) / N than those of SlicerAstro 1.x (N channels):
1.x multiplied the sum of the intensities by the velocity range (N channel
widths) divided by N - 1 instead of by the channel width. Fluxes in Jy (AstroProfiles,
total flux of AstroStatistics) use the exact area of the Gaussian beam,
pi / (4 ln 2) BMAJ BMIN (1.x: 1.13 BMAJ BMIN, 0.27% higher fluxes). Both were
found by comparing with spectral-cube, astropy and casacore.

