# AstroPVDiagram

Position-velocity (PV) diagram along a path: the spectra of the cube along a
curve drawn on the sky, side by side. Unlike AstroPVSlice (a straight line),
the path can bend, e.g. along a spiral arm, a tidal tail or a ridge.

## The path

The path is drawn on the moment-0 map of the cube shared by the modules
(`<cube>_mom0`: the map made by AstroMomentMaps, or one computed when there
is none), in the panel preview (north up) or in the red view.

- In the preview: click to add a point at the end of the path, drag a point
  to move it, right-click a point to remove it. The first point (ringed) is
  offset 0 of the PV diagram.
- **Draw in views**: points are added by clicking on the map in the views;
  click the button again, or right-click in a view, to stop.
- **Clear**: removes the points (the PV diagram is kept until the new path
  has two points).
- **Straight** / **Spline**: straight segments between the points, or a
  smooth curve (cardinal spline) through them.
- **Width**: pixels across the path (default 1).

The points stay on the map: points placed outside it are moved to its edge.
The line under the preview gives the number of points, the length of the
path on the sky and the number of columns of the PV diagram.

## The PV diagram

The PV diagram follows the path: it is updated when a point is added, moved
or removed, or when the shape or the width changes. There is no button to
press and no output to choose: each cube has one PV diagram,
`<cube>_PVDiagram`, listed with its products (Workbench) and shown in the
yellow view.

- **Columns**: samples one pixel apart along the path (and its end). Each
  column is the spectrum of the cube at the sample (bilinear interpolation
  in the plane of the sky; neighbours with a zero weight are left out, a
  blank neighbour with a weight blanks the value). With a width of *w*
  pixels, the column is the mean of the *w* spectra at positions one pixel
  apart along the normal of the path, centred on it; blank values and
  positions outside the cube are left out.
- **Axes**: the horizontal axis is the angular offset along the path
  (`CTYPE1 = 'OFFSET'`, `CUNIT1 = 'deg'`; `CDELT1`: the length of the path on the sky, great-circle distances
  between the samples, divided by the number of columns minus one, in
  degrees); the vertical axis is the spectral axis of the cube.
- **Header**: the path (shape, width, sky positions of the points) is
  written as `HISTORY` cards, replaced at each update.
- The 2D colours of the PV diagram are those of the cube when it is made.

The view is fitted to the PV diagram when it is made; later updates keep the
zoom of the view.

## Views

In Slicer the module shows four views: the moment-0 map with the path (red),
the second moment map (green), the PV diagram (yellow) and 3D. In the
AstroWorkbench (Process, *PV diagram*) the layout of the Workbench is kept.

## Scripting

```python
logic = slicer.modules.astropvdiagram.logic()
parameters = slicer.mrmlScene.GetFirstNodeByClass("vtkMRMLAstroPVDiagramParametersNode")
parameters.SetInputVolumeNodeID(cube.GetID())
logic.PreparePath(parameters)            # moment-0 map and path
path = logic.GetPath(parameters)         # Markups open curve "PVDiagramLine"
path.AddControlPoint(...)                # RAS on the moment-0 map
parameters.SetWidth(3)
logic.UpdatePVDiagram(parameters)        # synchronous
pv = logic.GetPVDiagram(cube)
```
