# SlicerAstro

SlicerAstro is an extension of [3D Slicer](https://www.slicer.org) for the
visualization and analysis of astronomical data cubes and images, in
particular of neutral hydrogen (HI) observations: interactive 2D and 3D
views with world coordinates (WCS), smoothing, masking, moment maps,
profiles, position-velocity diagrams, kinematic modelling with 3DBarolo,
reprojection and mosaicking, source catalogues and measurements on the sky.

::::{grid} 2
:gutter: 2

:::{grid-item-card} User guide
:link: user_guide/getting_started
:link-type: doc
Install SlicerAstro, load a data cube and explore it.
:::

:::{grid-item-card} Modules
:link: user_guide/modules/index
:link-type: doc
What each module does and how to use it.
:::

:::{grid-item-card} Developer guide
:link: developer_guide/build
:link-type: doc
Build from source, architecture, tests and validation.
:::

:::{grid-item-card} Citing SlicerAstro
:link: about/citing
:link-type: doc
References to cite when you use SlicerAstro.
:::
::::

```{toctree}
:caption: User guide
:maxdepth: 2

user_guide/installation
user_guide/getting_started
user_guide/sample_data
user_guide/modules/index
user_guide/measurements
user_guide/troubleshooting
```

```{toctree}
:caption: Developer guide
:maxdepth: 2

developer_guide/build
developer_guide/architecture
developer_guide/testing
developer_guide/continuous_integration
developer_guide/reprojection_validation
```

```{toctree}
:caption: About
:maxdepth: 1

about/whats_new
about/citing
about/license
```
