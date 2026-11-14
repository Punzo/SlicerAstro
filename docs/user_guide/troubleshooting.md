# Troubleshooting

## Messages about the FITS header

Problems found in the headers (missing or inconsistent keywords, WCSLIB and
CFITSIO errors) are reported in the **Error Log** (*View > Error Log*), once
per file. Data without valid world coordinates are shown in pixel
coordinates.

DSS images with only the old plate solution (`PPO`, `AMDX`, `AMDY`
keywords, without a standard WCS) cannot be reprojected.

## Large data cubes

Loading a cube needs memory for its voxels only (about 4 bytes per voxel;
compressed `.fits.gz` files are decompressed to a temporary file, not in
memory). Cubes of more than 2³¹ voxels are supported. The analysis modules
create new volumes (e.g. smoothed cubes, reprojections): each needs memory
for its own voxels.

## Slow interaction on Linux with Wayland

On Wayland every mouse motion is delivered to the application. SlicerAstro
combines the mouse motions of the views (as Qt does on X11); the
environment variable `SLICERASTRO_COMPRESS_MOUSE_MOVES=0` disables it and
`=1` enables it on other platforms.

## GPU smoothing

AstroSmoothing uses the GPU through OpenGL. If no GPU backend is available
the module uses the CPU (multithreaded).
