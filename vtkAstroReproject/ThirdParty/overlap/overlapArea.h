// Spherical polygon overlap of two pixels: see overlapArea.cxx (astropy
// reproject / Montage, BSD-3).
#ifndef SLICERASTRO_OVERLAP_AREA
#define SLICERASTRO_OVERLAP_AREA

/// Area (steradians) of the overlap of two spherical quadrilaterals given by
/// their corners (longitudes and latitudes in radians, 4 each). Thread safe.
double SlicerAstroComputeOverlap(double *ilon, double *ilat, double *olon, double *olat,
                                 int energyMode, double refArea, double *areaRatio);

#endif
