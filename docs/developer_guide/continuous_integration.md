# Continuous integration

SlicerAstro is built and tested by GitHub Actions. The workflows are in
`.github/workflows/`, and the scripts they run are in `.github/ci/`. The
scripts can also be run on your own computer.

| Workflow | What it does | Runs on | When |
|---|---|---|---|
| `extension.yml` (Extension (Linux)) | Builds 3D Slicer from source, then SlicerAstro, then runs all the tests | Ubuntu 24.04 | weekly (Sunday 03:00 UTC), on demand, on pushes to `master` (not for documentation-only changes) |
| `dependencies.yml` (Dependencies) | Builds CFITSIO, WCSLIB, ERFA, FFTW and 3DBarolo with the pinned versions and runs a smoke test | Linux, Windows (MSVC), macOS | changes to `SuperBuild/` or `.github/ci/dependencies/`, on demand |
| `docs.yml` (Documentation) | Builds this documentation with Sphinx, with warnings as errors, and publishes it to GitHub Pages | Ubuntu | changes to `docs/`; published from `master` only |

The full extension is built only on Linux. On Windows and macOS it is built by
the 3D Slicer extensions factory once SlicerAstro is in the
[ExtensionsIndex](https://github.com/Slicer/ExtensionsIndex).

## Extension (Linux)

### Steps

1. **System packages and Qt** (`.github/ci/install-system-packages.sh`):
   - compilers, CMake, ccache, Mesa (OpenGL), the X11/xcb libraries, OpenSSL
     and Xvfb from apt;
   - Qt 6.10.1 with [aqtinstall](https://github.com/miurahr/aqtinstall),
     including the `qt5compat` and `qtscxml` modules that 3D Slicer needs.
2. **3D Slicer** (`.github/ci/build-extension.sh`):
   - Checks out the pinned commit (`SLICER_SHA` in the script).
   - Configures a Release superbuild with Qt 6, without the parts SlicerAstro
     does not use: DICOM, diffusion, BRAINSTools, CLI modules, SimpleITK,
     extensions manager, web engine, multimedia, internationalization,
     documentation and the 3D Slicer tests.
   - Installs scipy into 3D Slicer's Python. The smoothing tests compare
     their results with scipy.
3. **SlicerAstro**: built against that 3D Slicer build tree with
   `BUILD_TESTING=ON`.
4. **Tests**: `ctest -j2 --output-on-failure` under Xvfb, with a 30-minute
   limit per test. Set `CTEST_EXCLUDE` to a regular expression to leave tests
   out.

### Time and cache

A GitHub runner has 4 cores, 16 GB of memory and a 6-hour limit per job.
Building 3D Slicer from scratch takes about 4 to 6 hours on it. The workflow
handles this as follows:

- Every compilation goes through **ccache**. The cache (up to 8 GB) is kept in
  the GitHub Actions cache and is saved even when the job fails or is stopped.
- The 3D Slicer build stops after `SLICER_TIME_LIMIT` (290 minutes), so there
  is time left to save the cache. The script then exits with code 124 and the
  log says "run again".
- The next run (re-run the job, push, or wait for the weekly run) restores the
  cache. It recompiles only what is missing and goes on to SlicerAstro and the
  tests. The first run on a new 3D Slicer commit usually stops at the time
  limit; the second run completes.
- The build runs on `/mnt`, the runner's large scratch disk, after deleting
  toolchains the build does not need (Android, .NET, Haskell).

Caches created on a branch other than `master` are visible only to that
branch. A new branch therefore starts with a cold cache unless `master` has
one.

### Updating 3D Slicer

Change `SLICER_SHA` in `.github/ci/build-extension.sh` to the 3D Slicer commit
SlicerAstro is developed against. The next run rebuilds most of 3D Slicer, so
expect it to stop at the time limit once.

## Reading failures

Job logs can be downloaded only by repository administrators. To make
failures readable by everyone, the workflows run `.github/ci/annotate-errors.sh`
when a step fails. It publishes the compiler, linker and test errors of the
log as **annotations**:

- They appear on the run's summary page.
- They can also be fetched without a token:

  ```bash
  curl -s https://api.github.com/repos/Punzo/SlicerAstro/check-runs/<job id>/annotations
  ```

GitHub keeps about 4 KB per annotation, so the script leaves out compiler and
linker command lines and splits the errors over several annotations. When a
test fails, the CTest logs are also uploaded as the `test-logs` artifact.

## Running the CI build locally

The scripts work on any Ubuntu 24.04 system, for example a container:

```bash
docker run --rm -it -v $PWD:/src:ro -v $HOME/ci-work:/work ubuntu:24.04 bash
# inside the container
/src/.github/ci/install-system-packages.sh /work/Qt     # prints Qt6_DIR=...
export Qt6_DIR=/work/Qt/6.10.1/gcc_64/lib/cmake/Qt6 JOBS=$(nproc)
/src/.github/ci/build-extension.sh /src /work/build
```

Environment variables of `build-extension.sh`:

| Variable | Meaning |
|---|---|
| `Qt6_DIR` | Qt 6 CMake directory (required) |
| `SLICER_REPOSITORY`, `SLICER_SHA` | 3D Slicer repository and commit |
| `JOBS` | parallel jobs (default: all processors) |
| `SLICER_TIME_LIMIT` | time limit of the 3D Slicer build, in minutes |
| `CTEST_EXCLUDE` | regular expression of tests not to run |
| `CCACHE_DIR`, `CCACHE_MAXSIZE` | compiler cache, when ccache is installed |

## Writing tests that pass in CI

- **Registration:** Python tests are registered with `slicer_add_python_unittest`
  whenever 3D Slicer has Python. They do not depend on
  `Slicer_USE_QtTesting`, which is off in Qt 6 builds of 3D Slicer.
- **Skipping:** a test that cannot run on the machine (no GPU backend, missing
  opt-in variable) should return early with a message. Don't raise
  `unittest.SkipTest`: `runTest()` calls the test methods directly, so the
  exception fails the test.
- **No window manager:** Xvfb has none, so with Qt 6 calling
  `activateWindow()` does not make a window active. Tests that need keyboard
  shortcuts of the main window call `QApplication.setActiveWindow()` when the
  window is still not active (see `AstroCloudLassoTest`).
- **Software OpenGL:** OpenGL is Mesa llvmpipe. The OpenGL smoothing works;
  there is no WebGPU adapter.
- **Data:** sample and testing data are downloaded from the GitHub releases
  (`SampleData-v2`, `TestingData-v1`) and cached.

## Portability of the dependencies (Windows)

The Dependencies workflow found these MSVC problems. Keep them in mind when
changing the forks or the code that uses them:

- **`wcsset()` clash.** The Microsoft C runtime declares a non-standard
  `wcsset()` (a wide-character string function) that clashes with WCSLIB's
  `wcsset()`. WCSLIB, 3DBarolo and SlicerAstro are compiled with
  `_CRT_DECLARE_NONSTDC_NAMES=0`, so code including `wcs.h` must not use the
  non-underscore POSIX names of the CRT (`strdup`, `fileno`, …).
- **No global data across DLLs.** The libraries are DLLs built with
  `WINDOWS_EXPORT_ALL_SYMBOLS`, which exports functions but no usable data. Do
  not read global arrays such as `wcs_errmsg` from another library:
  SlicerAstro uses `vtkAstroWCSStatusDescription()` and 3DBarolo uses
  `wcsStatusMessage()`. FFTW's own data (`fftw_version`) needs `FFTW_DLL`.
- **POSIX-only headers and functions.** `getopt.h`, `unistd.h`,
  `sys/ioctl.h`, `mkstemp` and `popen` need `_WIN32` alternatives, and so do
  the alternative operator spellings (`and`, `or`).
- **A quick check on Linux** finds some of these problems before CI does:

  ```bash
  g++ -std=c++17 -fsyntax-only -fno-operator-names -Werror=vla <file>
  ```
