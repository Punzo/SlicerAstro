# Validation of the AstroReproject engine

Developer validation of `vtkAstroReprojector` (not a ctest) against
astropy reproject, casacore (`ImageRegrid`, the engine of CASA `imregrid`),
Montage and radio-beam, on the Antennae sample data of SlicerAstro (release
`SampleData-v2`: `Antennae_R`, `Antennae_HI`, `Antennae_CO_North`,
`Antennae_CO_South`). The references are computed on the grids written by
SlicerAstro (the FITS headers of its outputs), so the WCS written by
SlicerAstro is checked as well.

## How to run

```
# Python with astropy, reproject, python-casacore, radio-beam, scipy
python prepare_inputs.py <Antennae data dir> <work dir>
SlicerWithSlicerAstro --no-splash --python-script slicer_outputs.py <Antennae data dir> <work dir>
python compare.py <Antennae data dir> <work dir> [<Montage bin dir>]
```

Montage: build it from source (`make`; BSD-3, https://github.com/Caltech-IPAC/Montage).

## Results (2026-11-14)

astropy 8.0.2, reproject 0.21.0, python-casacore 3.8.1, Montage 2ef5000,
radio-beam 0.3.10. Columns: voxels compared, voxels valid only in the
reference or only in SlicerAstro, |difference| / peak (max, 99th percentile,
median).

```
astropy reproject (reproject_interp, reproject_exact)
  R_on_south_nearest (nearest-neighbor)        n=   160000 only_ref=      0 only_slicerastro=      0 max=0.0e+00 p99=0.0e+00 median=0.0e+00
  R_on_south_bilinear (bilinear)               n=   160000 only_ref=      0 only_slicerastro=      0 max=3.7e-08 p99=4.6e-09 median=5.3e-10
  R_on_south_exact (exact)                     n=   160000 only_ref=      0 only_slicerastro=      0 max=3.7e-08 p99=4.4e-09 median=4.5e-10
  R_on_south_area (exact)                      n=   160000 only_ref=      0 only_slicerastro=      0 max=5.2e-02 p99=3.4e-03 median=5.5e-05
  HI_on_south_bilinear (bilinear)              n= 14560000 only_ref=      0 only_slicerastro=      0 max=4.4e-08 p99=1.6e-08 median=1.2e-09
  north_galactic_bilinear (bilinear)           n=   458353 only_ref=     10 only_slicerastro=      0 max=3.0e-08 p99=5.7e-10 median=1.2e-11
  north_galactic_exact (exact)                 n=   453280 only_ref=      0 only_slicerastro=  10302 max=1.2e-04 p99=5.7e-10 median=1.2e-11
casacore ImageRegrid (CASA imregrid)
  Rfk5_on_south_nearest (nearest)              n=   160000 only_ref=      0 only_slicerastro=      0 max=0.0e+00 p99=0.0e+00 median=0.0e+00
  Rfk5_on_south_bilinear (linear)              n=   160000 only_ref=      0 only_slicerastro=      0 max=9.5e-09 p99=0.0e+00 median=0.0e+00
  Rfk5_on_south_bicubic (cubic)                n=   160000 only_ref=      0 only_slicerastro=      0 max=7.4e-08 p99=0.0e+00 median=0.0e+00
  Rfk5_on_south_lanczos (lanczos)              n=   160000 only_ref=      0 only_slicerastro=      0 max=9.6e-03 p99=1.2e-03 median=1.6e-04
  R_on_south_bilinear (linear)                 n=   160000 only_ref=      0 only_slicerastro=      0 max=1.5e-02 p99=1.4e-03 median=3.3e-05
  south_vrad_bary_linear (linear)              n=  2203860 only_ref=      0 only_slicerastro=      0 max=2.6e-07 p99=2.2e-08 median=4.1e-10
  south_vrad_bary_nearest (nearest)            n=  2203920 only_ref=      0 only_slicerastro=      0 max=0.0e+00 p99=0.0e+00 median=0.0e+00
Montage (mProject, mProjectCube, mShrink, mAddCube)
  R on CO South grid: mProject / exact         n=   158975 only_ref=      0 only_slicerastro=   1025 max=6.9e-02 p99=1.6e-05 median=7.6e-08
  R on CO South grid: mProject / area          n=   158975 only_ref=      0 only_slicerastro=   1025 max=7.4e-02 p99=3.5e-03 median=5.5e-05
  R pixels x2: mShrink / exact                 n=  1307460 only_ref=      0 only_slicerastro=   2287 max=6.2e-04 p99=4.5e-11 median=0.0e+00
  R pixels x2: mShrink / area                  n=  1307460 only_ref=      0 only_slicerastro=   2287 max=6.2e-04 p99=0.0e+00 median=0.0e+00
  CO North, TAN rotated: mProjectCube / exact  n=   572758 only_ref=  14053 only_slicerastro=      0 max=7.5e-02 p99=8.8e-05 median=8.6e-08
  CO mean mosaic: mAddCube / mean exact        n=  2831089 only_ref=  32576 only_slicerastro=      0 max=2.8e-03 p99=2.5e-05 median=1.3e-07
  arbiter: astropy exact / SlicerAstro exact   n=   160000 only_ref=      0 only_slicerastro=      0 max=3.7e-08 p99=4.4e-09 median=4.5e-10
  arbiter: astropy exact / Montage mProject    n=   158975 only_ref=   1025 only_slicerastro=      0 max=6.9e-02 p99=1.6e-05 median=7.6e-08
radio-beam (common beam, convolution, deconvolution)
  common beam: valid (all beams deconvolvable) 200/200; radio-beam converged 104/200; area SlicerAstro / radio-beam min 1.0000 median 1.0000 max 1.0020
  convolution: max axis difference 0.0e+00"; deconvolution of the convolution: max error 7.5e-15"
resolution and channels (astropy convolution, numpy)
  CO South to a 3" beam (25 px from blanks)    n=   971698 only_ref=      0 only_slicerastro=      0 max=1.3e-05 p99=4.1e-06 median=4.4e-08
  CO South channels x2: mean of channel pairs  n=   749330 only_ref=      0 only_slicerastro=     17 max=5.9e-08 p99=1.8e-09 median=1.4e-11
```

## Notes

- **astropy**: nearest neighbour identical; bilinear and exact overlap equal
  within float32 rounding (also with the ICRS to FK5 and FK5 to Galactic
  conversions, ERFA in both). Area average is compared with the exact overlap
  of astropy: it is an approximation of it (sub-grid), within a few 1e-3 of the
  peak except at bright compact sources. Exact overlap, Galactic grid:
  SlicerAstro keeps output pixels next to the blanks of the data when at least
  `MinimumCoverage` (0.5) of them is covered (renormalized); astropy blanks them.
- **casacore**: without a celestial frame conversion (image labelled FK5 J2000)
  nearest, linear and cubic are identical (SlicerAstro reimplements the
  interpolation of casacore, including the cubic convolution); the spectral
  regridding (LSRK frequencies to barycentric radio velocities) is equal within
  3e-7 and covers the same voxels. With the conversion of the ICRS image to the
  FK5 grid the values differ by up to 1.5e-2 of the peak: casacore converts FK5
  to ICRS with the IAU 2000 frame bias, ERFA/astropy/SlicerAstro with the
  FK5-Hipparcos frame tie (about 0.03" apart, a tenth of a pixel here).
  Lanczos: casacore does not normalize the Lanczos weights (a flat image of 1
  becomes 0.9998, with some 0 values inside), SlicerAstro does.
- **Montage**: the median differences are 1e-7; mShrink (2 x 2 blocks) equals
  the exact overlap and the area average with a pixel size factor 2. The
  largest differences are pixels where the overlap areas of Montage are wrong:
  astropy reproject_exact (derived from Montage, with fixes) agrees with
  SlicerAstro (3.7e-8) and not with Montage at the same pixels. The voxels
  valid only in Montage are next to the blanks of the data (coverage below
  `MinimumCoverage`: blank in SlicerAstro), and at the edges of the inputs.
- **radio-beam**: the common beam (CASA algorithm) is found and every beam
  deconvolves from it for all 200 random sets; radio-beam (minimal enclosing
  ellipse) does not converge for 96 sets, and where it does SlicerAstro's beam
  is at most 0.2% larger (CASA grows the ellipse by 0.1% steps). Convolution
  and deconvolution are exact.
- **Resolution**: CO South convolved to a 3" beam equals the astropy
  convolution with the radio-beam kernel and the CASA scale factor (Jy/beam:
  target beam area / data beam area) within 1.3e-5 of the peak (kernel
  truncation).
- **Channels**: channel overlap with a width factor 2 equals the mean of the
  pairs of channels; SlicerAstro keeps the pairs with one blank channel
  (coverage 0.5).
- Not compared with an external tool: the matching of channels by velocity
  (CASA imregrid with `specAsVelocity`, not in python-casacore) and the
  primary beam weighted mosaics (CASA linearmosaic): both have analytic tests
  in `AstroReproject/Testing/Python/AstroReprojectTest.py`.
