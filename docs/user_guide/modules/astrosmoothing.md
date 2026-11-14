# AstroSmoothing

Smooths data cubes to bring out faint emission. The filters ignore the blank
(NaN) voxels.

- **Box**: average over a box of voxels.
- **Gaussian**: Gaussian kernel with full widths at half maximum along the
  three axes (*FWHM_X, FWHM_Y, FWHM_Z*, in voxels) and rotation angles; the
  new beam of the data is computed.
- **Intensity-Driven Gradient**: iterative smoothing that preserves the
  structure of the bright emission (Punzo et al. 2016); parameters *Accuracy*,
  *TimeStep*, number of iterations.

*Hardware*: CPU (multithreaded) or GPU (OpenGL). *Generate masks* creates a
mask of the emission of the smoothed data (in the mask of the input data).
