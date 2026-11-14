# Installation

## From the Extensions Manager

1. Install the [3D Slicer](https://download.slicer.org) Preview release
   (SlicerAstro 2.0 is developed with 3D Slicer 5.13).
2. Open the **Extensions Manager** (toolbar icon, or *View > Extensions
   Manager*), search for **SlicerAstro** and click **Install**.
3. Restart 3D Slicer. The SlicerAstro modules are in the **Astronomy**
   category of the module selector.

SlicerAstro packages include the libraries it needs (CFITSIO, WCSLIB, ERFA,
FFTW and 3DBarolo): nothing else has to be installed.

## Platforms

SlicerAstro is built for Linux, Windows and macOS by the 3D Slicer extensions
build system. It is developed and tested on Linux, with Qt 5 and Qt 6 builds
of 3D Slicer.

## Building from source

See the [developer guide](../developer_guide/build.md).
