# AstroProfiles

Integrated spectrum of the data: in each channel the sum of the selected
voxels. The profile is a 1D volume and a plot.

- **Threshold**: the voxels within the threshold range, in the channels of
  the velocity range (the other channels are blank). Faint emission below the
  threshold is not included and noise peaks above it are.
- **Mask, aperture** (default with a mask): all the voxels in the sky area of
  the mask segment (its voxels in any channel), in all the channels. The faint
  emission of the wings is included and the noise averages out: the global
  profile of a source. A negative baseline in the line-free channels shows a
  negative bowl of the data around the source (e.g. interferometric data).
- **Mask, voxels**: the voxels of the mask segment only (0 in the channels
  without voxels of the mask).

A channel without selected voxels is 0; channels without data are blank.
Data in Jy/beam with a beam (BMAJ, BMIN) give fluxes in Jy (pixel area divided
by the beam area, pi / (4 ln 2) BMAJ BMIN); otherwise the profile is the sum
of the intensities, in the unit of the data (shown on the plot axis).
