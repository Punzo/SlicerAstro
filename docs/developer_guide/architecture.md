# Architecture

## Libraries

| Library | Content |
|---|---|
| `vtkFits` | FITS reader and writer (CFITSIO, WCSLIB) |
| `vtkAstroCoordinates` | celestial and spectral reference frames (`vtkAstroFrames`, ERFA) |
| `vtkAstroReproject` | reprojection engine (`vtkAstroReprojector`), beams (`vtkAstroBeams`), exact overlap (from astropy reproject, BSD-3) |
| `vtkAstroImageFilters` | multithreaded smoothing filters (vtkSMPTools) |
| `vtkOpenGLFilters` | GPU smoothing filters (OpenGL) |
| `vtkAstroWebGPUFilters` | GPU smoothing filters (WebGPU, optional) |

## Modules

Each module has MRML nodes (parameters), a logic (computations) and a Qt
widget. The **AstroVolume** module provides the shared parts:

- MRML: `vtkMRMLAstroVolumeNode` and its display node (WCS, colour
  functions), label map volumes, the parameters nodes of all the modules,
  `vtkMRMLAstroSegmentationStorageNode` (FITS masks),
  `vtkMRMLAstroMeasurement` (measurements on the sky);
- displayable managers: WCS axes and beam of the slice views;
- logic: volume utilities, presets, contours and masks of each volume, the
  background runner, the reprojection service used by the contours of other
  data (registered by AstroReproject);
- `qSlicerAstroMouseMoveCompressor`: mouse moves of the VTK views (hover
  moves duplicated by Qt 6 dropped, compression on Wayland).
