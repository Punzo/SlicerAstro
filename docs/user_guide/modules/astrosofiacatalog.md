# AstroSoFiACatalog

Source catalogues of [SoFiA](https://gitlab.com/SoFiA-Admin/SoFiA-2)
(text, CSV or VOTable) and other catalogues with celestial positions and
velocities: they are loaded as tables and as points at the positions of the
sources in the data (right ascension, declination and velocity, converted to
the velocity definition of the data). Sources can be selected, coloured and
deleted.

## The panel

The panel shows the catalogues of a dataset (it follows the active volume).
It is the same *Sources* section as the Sources tab of AstroVolume and of
the AstroWorkbench.

- **Import**: a catalogue file, placed on the dataset (SoFiA pixel
  positions on the cube or on its mask; other catalogues with the WCS of the
  cube). The bin button removes the catalogue (table and points).
- **Table**: a click on a source centres the views on it and selects its
  segment in the mask of the dataset; Ctrl/Shift select several sources,
  whose points are drawn with the selected colour.
- **Points**, **Labels**, **2D**: the points in the 3D views, their names,
  the points in the 2D views (only where a slice passes through them).
  **Move**: the points can be moved with the mouse (they are locked
  otherwise).
- The two swatches: colour of the points and of the selected points.
  **Size**: size of the points. **Delete**: the selected sources (rows of
  the table and points).
