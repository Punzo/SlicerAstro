# AstroModeling

Tilted-ring models of the kinematics of disk galaxies with
[3DBarolo](https://bbarolo.readthedocs.io) (Di Teodoro & Fraternali 2015):
the model is fitted directly on the data cube, with the beam of the data.

- **Automatic** mode: 3DBarolo estimates the initial parameters (centre,
  systemic velocity, inclination, position angle, rotation velocity) and fits
  them.
- **Manual** mode: set the parameters of the rings, choose the free
  parameters, the fitting function and the weighting.

The model cube is compared with the data (contours of data and model in the
slice views, *Show Model*), and the rotation curve and the other ring
parameters are plotted. A fit can be cancelled. The messages of 3DBarolo are
in the *3DBarolo Output* section and in the application log.
