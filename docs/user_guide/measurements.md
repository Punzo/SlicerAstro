# Measurements on the sky

Markups (*Markups* module, or the markups toolbar) measure astronomical
quantities with the world coordinates (WCS) of the **active volume** (the
volume selected in the AstroVolume module), instead of the lengths in
millimetres and the angles in the plane of the slice of 3D Slicer:

| Markups | Measurements |
|---|---|
| Line | **separation**: angular distance of the two points on the sky (great circle); **velocity difference** of the two points (km/s, with the velocity definition of the data), when they are in different channels |
| Open curve | **sky length**: length of the curve on the sky; **velocity difference** of its first and last points |
| Closed curve | **sky length** (perimeter) and **sky area**: solid angle enclosed by the curve, when it lies in one channel |
| Angle | **sky angle**: angle at the vertex between the great circles to the two other points, when the three points lie in one channel |

Angular values are shown in arcsec, arcmin or degrees, areas in arcsec²,
arcmin² or deg². The measurements are shown next to the markups in the
views and in the *Measurements* section of the Markups module; they are
updated when the markups are moved and when another volume becomes active.
For volumes without world coordinates the measurements of 3D Slicer are used.
