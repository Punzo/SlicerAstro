# Testing

The tests are run with CTest in the build directory of the extension:

```bash
cd inner-build
ctest -j4 --output-on-failure        # all the tests
ctest -R AstroReproject              # the tests of a module
```

- C++ tests of the MRML nodes and of the libraries.
- Python tests (`*/Testing/Python/*Test.py`) run in 3D Slicer: they load
  synthetic FITS files or the sample data (downloaded and cached) and compare
  the results with references (analytic models, numpy/scipy).

`AstroFITSRealDataTest` reads FITS files of real instruments and archives
(NVSS, 2MASS, SDSS, HST, Chandra from the `TestingData-v1` release; WSRT,
VLA, ALMA, VLT/MUSE, DSS and CTIO data of `SampleData-v2`) and compares the
dimensions, the voxels and the world coordinates with the values of astropy
(`AstroVolume/Testing/Python/RealData/*.json`, written by
`RealData/prepare_real_data.py`). `SLICERASTRO_TESTING_DATA_DIR` reads the
files from a directory instead of downloading them.

Some tests are opt-in or need hardware:

| Test | Condition |
|---|---|
| `AstroFITSIOTest.test_LargeCube` | `SLICERASTRO_LARGE_TESTS=1` (cube of more than 2³¹ voxels, 8.7 GB of memory and disk) |
| `AstroFITSIOTest.test_Memory` | Linux (peak memory from `/proc`) |
| `AstroSmoothingSelfTest.test_AstroSmoothingGPU` | a GPU backend (OpenGL or WebGPU) |

The tests also run in GitHub Actions: see
[Continuous integration](continuous_integration.md).

The validation of the reprojection against other software is described in
[Validation of the reprojection](reprojection_validation.md).
