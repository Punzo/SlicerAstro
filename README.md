# SlicerAstro

SlicerAstro is an extension of [3D Slicer](https://www.slicer.org) for the
visualization and analysis of astronomical data cubes and images, in
particular of neutral hydrogen (HI) observations.

- **Data**: FITS images and data cubes (also `.fits.gz` and `.fz`) with
  their world coordinates (WCS), FITS masks (SoFiA), source catalogues.
- **Visualization**: 2D views with WCS axes and beam, 3D volume rendering,
  contours, masks, channel maps, measurements on the sky.
- **Analysis**: smoothing (CPU and GPU), masking, statistics, profiles,
  moment maps, position-velocity slices and diagrams, kinematic modelling
  with [3DBarolo](https://github.com/editeodoro/Bbarolo).
- **Reprojection**: regridding on the sky and in frequency/velocity with
  celestial and spectral reference frames, flux-conserving resampling,
  common and target beams, mosaics (validated against astropy, CASA and
  Montage).

## Documentation

The documentation (user guide, modules, developer guide) is in
[`docs/`](docs/index.md); it is built with Sphinx:

```bash
pip install -r docs/requirements.txt
sphinx-build docs docs/_build/html
```

## Installation

Install SlicerAstro from the Extensions Manager of 3D Slicer (category
*Astronomy*). To build it from source, see the
[developer guide](docs/developer_guide/build.md).

## Citing

If you use SlicerAstro, please cite Punzo et al. 2017, Astronomy and
Computing, 19, 45 ([arXiv:1703.06651](https://arxiv.org/abs/1703.06651)); see
[Citing SlicerAstro](docs/about/citing.md).

## Licence

BSD 3-Clause (see [LICENSE](LICENSE)). The packages include third-party
libraries with their own licences (see [Licence](docs/about/license.md)).
