# AstroStatistics

Statistics of the voxels in a region of interest (ROI) or in the segments of
a mask: number of pixels, minimum, maximum, mean, standard deviation, median,
sum and total flux. The results are a table (copy and paste to a spreadsheet).

The total flux (Jy) is the sum of the intensities times the pixel area divided
by the beam area (pi / (4 ln 2) BMAJ BMIN). It needs data in Jy/beam with a
beam in the FITS header: otherwise its check box is disabled (e.g. WEIN069,
whose header has BMAJ = -1).

