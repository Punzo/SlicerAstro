# AstroPVSlice

Interactive position-velocity (PV) slice: a straight line on the sky defines
a slice through the cube along the line and the spectral axis.

## The line

The line is drawn on the moment-0 map of the cube, in the panel preview
(north up) and in the red view. The map is the moment-0 map of the cube
shared by the modules (`<cube>_mom0`): the one made by AstroMomentMaps, or
one computed when there is none (voxels above twice the noise, all the
channels).

- Drag an end of the line to turn or stretch it, its centre to move it
  (in the preview or in the red view).
- **RA**, **Dec**: centre of the line (degrees; also shown in
  sexagesimal). **PA**: position angle at the centre, from North towards
  East (0–180°). **Length**: arcminutes. The ends are placed on the great
  circle through the centre with that position angle.
- **Centre on peak**: the line through the brightest pixel of the map (same
  angle and length).
- **Major axis**: the line through the intensity-weighted centre of the
  pixels above 20% of the peak, along the axis of their largest second
  moment (same length).

## Views

The yellow view shows the PV slice: the angular offset along the line and
the velocity. The **3D view** buttons point the camera of the 3D view along
the velocity axis (*Sky*, north up), along the line (*Along line*: the PV
plane edge-on) or perpendicular to the PV plane (*Face-on*, velocity up).
