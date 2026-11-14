# Building from source

SlicerAstro is a 3D Slicer extension with a superbuild: the libraries it
depends on are downloaded and built first.

## Requirements

- A build of 3D Slicer (5.13 or later), with Qt 5 or Qt 6
  ([building 3D Slicer](https://slicer.readthedocs.io/en/latest/developer_guide/build_instructions/index.html)).
- CMake 3.28 or later, a C++17 compiler, Git.

## Build

```bash
git clone https://github.com/Punzo/SlicerAstro.git
mkdir SlicerAstro-build && cd SlicerAstro-build
cmake -DSlicer_DIR:PATH=/path/to/Slicer-SuperBuild/Slicer-build ../SlicerAstro
make -j8
```

The extension is built in `inner-build`. Start 3D Slicer with it:

```bash
./inner-build/SlicerWithSlicerAstro
```

Package it (archive for the Extensions Manager):

```bash
cd inner-build && make package
```

## Dependencies

| Library | Version | Licence | Use |
|---|---|---|---|
| [CFITSIO](https://heasarc.gsfc.nasa.gov/fitsio/) | 4.7.0 | NASA (permissive) | FITS files |
| [WCSLIB](https://www.atnf.csiro.au/people/mcalabre/WCS/) | 8.10 (CMake build in a fork) | LGPL-3 | world coordinates |
| [ERFA](https://github.com/liberfa/erfa) | 2.0.1 | BSD-3 | celestial and spectral reference frames |
| [FFTW](https://www.fftw.org) | 3.3.11 | GPL-2 | 3DBarolo |
| [3DBarolo](https://github.com/editeodoro/Bbarolo) | 1.8 (fork with SlicerAstro patches) | GPL-2 | kinematic modelling |

The pinned versions are in `SuperBuild/External_*.cmake`. The workflow
`.github/workflows/dependencies.yml` builds and tests these libraries on
Linux, Windows and macOS.

## Optional: WebGPU smoothing

The WebGPU smoothing filters (`vtkAstroWebGPUFilters`) are built only when
VTK provides the WebGPU module (SlicerAstroApp); otherwise AstroSmoothing
uses OpenGL on the GPU.

## Coding conventions

- C++17, the VTK and 3D Slicer style; explicit types (no `auto` or
  `decltype`).
- Messages for the user in the 3D Slicer error log; long computations in the
  background (`vtkSlicerAstroVolumeLogic::RunInBackground`).
