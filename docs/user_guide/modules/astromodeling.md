# AstroModeling

Tilted-ring models of the kinematics of disk galaxies with
[3DBarolo](https://bbarolo.readthedocs.io) (Di Teodoro & Fraternali 2015):
the model is fitted directly on the data cube, with the beam of the data.

## Setup

- **Galaxy**: *Find source* (3DBarolo searches the largest source in the
  cube; the search parameters are in *Advanced*) or *Mask*: a segment of the
  mask of the cube (e.g. a SoFiA source), the first one if none is selected.
  The fit and the normalization of the model use its voxels.
- **Automatic** / **Manual**: in Automatic mode 3DBarolo estimates the
  initial parameters ("auto"); in Manual mode the values of the panel are
  used ("auto" values are still estimated). **Estimate** runs the estimate
  of 3DBarolo and shows its values in Manual mode, to be edited.
- **Beam**: the beam of the data (FITS header). Without one, the beam is
  estimated from the noise (highlighted: check it), used for the model and
  saved with the data; it can be edited here or in the Info section.
- **Rings**: number and width (arcsec).
- **Geometry and kinematics**: centre, systemic velocity (optical),
  rotation velocity, dispersion, inclination, position angle, scale height;
  **free** marks the parameters fitted by 3DBarolo.
- **Advanced**: radial and vertical velocities, vertical gradient of the
  rotation (dV/dz, ZCYL), column density, distance, ranges of the fitted
  parameters, layer profile, clouds, tolerance, spectral resolution, first
  ring, weight of the blank pixels, residuals function, azimuthal weight,
  side, normalization (none, local, azimuthal), second stage with its
  regularization, asymmetric drift, normalized cube, errors, redshift,
  emission lines, and the source search.

## Operations

**Create model** (the model of the parameters, no fit) and **Fit** run in the
background: the views and the other panels stay usable, the button shows
the progress and **Cancel** stops the operation (a fit at its next
iteration). One operation runs at a time. The output of 3DBarolo is in the
*3DBarolo log* and in the application log. Each operation makes the model
cube (`<cube>_model_N`) and the residual cube (`<cube>_maskedByModel_N`: the
data outside the model) and replaces those of the previous operation; a
failed or cancelled operation leaves none.

## Results

- **Rings**: the plot of a parameter of the rings against the radius
  (rotation curve, dispersion, inclination, ...; move the mouse over a ring
  for its values), also in a plot view of the layout, and the table of the
  rings. The table can be edited or pasted into (Ctrl+V); **Calculate
  model** makes the model of the rings of the table. The table can be saved
  as CSV.
- **Display**: the contours of the data and of the model at a contour level
  (in units of the noise of the data) in the 2D views, the model and its
  region in 3D.
- **PV slices**: the yellow view along the major axis and the green view
  along the minor axis, through the centre of the rings (mean position
  angle), with the points of the rotation curve: for each ring, the two
  points where it crosses the slice, at the velocity of the model there.
  *Major axis* and *Minor axis* rotate the slices about the velocity axis.
  The orientations are absolute: a saved scene shows the same slices and
  points when it is loaded again.
- The model and residual cubes can be shown in place of the data or
  deleted.

In classic Slicer the results use a layout with a plot view; in the
AstroWorkbench (Model tab) the layout of the Workbench is kept.

## Scripting

```python
logic = slicer.modules.astromodeling.logic()
parameters = slicer.mrmlScene.AddNewNodeByClass("vtkMRMLAstroModelingParametersNode")
parameters.SetInputVolumeNodeID(cube.GetID())
parameters.SetMode("Manual")
parameters.SetNumberOfRings(6)
parameters.SetRadSep(120.)
# ... initial values and free parameters
logic.StartOperation(parameters, parameters.FIT, True)   # synchronous
print(logic.GetLastOperationResult(), logic.GetLastError())
logic.ShowResults(parameters, True)                       # views
chart = logic.UpdateRingPlot(parameters, "VRot")
```
