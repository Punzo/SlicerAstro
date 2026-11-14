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

Some tests are opt-in or need hardware:

| Test | Condition |
|---|---|
| `AstroFITSIOTest.test_LargeCube` | `SLICERASTRO_LARGE_TESTS=1` (cube of more than 2³¹ voxels, 8.7 GB of memory and disk) |
| `AstroFITSIOTest.test_Memory` | Linux (peak memory from `/proc`) |
| `AstroSmoothingSelfTest.test_AstroSmoothingGPU` | a GPU backend (OpenGL or WebGPU) |

The validation of the reprojection against other software is described in
[Validation of the reprojection](reprojection_validation.md).
