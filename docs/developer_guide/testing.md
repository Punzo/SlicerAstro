# Testing

The tests are run with CTest in the build directory of the extension:

```bash
cd inner-build
ctest -j8 --output-on-failure        # all the tests (about 3 minutes, Debug)
ctest -R AstroReproject              # the tests of a module
```

- C++ tests of the MRML nodes and of the libraries.
- Python tests (`*/Testing/Python/*Test.py`) run in 3D Slicer: they load
  synthetic FITS files or the sample data and compare the results with
  references (analytic models, numpy/scipy).

How the tests run in parallel:

- **Own displays.** On Linux with Xvfb, the tests with windows (Slicer, Qt
  widgets, OpenGL) run on virtual X displays started for the tests
  (`SLICERASTRO_TEST_DISPLAYS`, 8 by default, from display
  `:SLICERASTRO_TEST_FIRST_DISPLAY`, 120): one test per display at a time,
  never on the desktop. With `SLICERASTRO_TEST_DISPLAYS=0` they share the
  display of the environment and run one at a time.
- **Sample data once.** `py_AstroTestSampleData` downloads the sample data of
  the tests into the cache of Slicer before them (CTest fixture); the tests
  only read the cache.
- **Long tests by method.** The Modeling, Workbench and scene tests are one
  CTest test per test method (`py_<Script>_<method>`,
  `slicerastro_add_python_unittest_methods` in
  `CMake/SlicerAstroTesting.cmake`, which finds the `def test_...` methods).
- **Time limit.** A test fails after 15 minutes (e.g. waiting on a dialog)
  instead of blocking the others. Tests never show modal dialogs in testing
  mode.

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

The moment maps, integrated spectra and statistics (with and without input
masks) are compared with spectral-cube, astropy, radio-beam and casacore by
the scripts of `AstroVolume/Testing/validation` (results in its README: the
differences are below 5e-6 of the peak values).
