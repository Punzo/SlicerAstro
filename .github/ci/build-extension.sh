#!/bin/bash
# Build 3D Slicer (Release, Qt 6, without the parts SlicerAstro does not use),
# SlicerAstro against it, and run the tests of SlicerAstro.
#
#   build-extension.sh <SlicerAstro source dir> <work dir>
#
# Environment:
#   Qt6_DIR           Qt 6 CMake directory (required)
#   SLICER_REPOSITORY 3D Slicer repository (default: GitHub)
#   SLICER_SHA        3D Slicer commit (default: the one SlicerAstro is developed with)
#   JOBS              parallel jobs (default: number of processors)
#   SLICER_TIME_LIMIT time limit of the 3D Slicer build in minutes (default: none);
#                     when reached the script stops (exit code 124): the compiler
#                     cache keeps what was built for the next run
#   CTEST_EXCLUDE     regular expression of tests not run (default: none)
#
# ccache is used if installed (CCACHE_DIR, CCACHE_MAXSIZE).
set -euo pipefail

SOURCE=$(realpath "$1")
WORK=$(realpath -m "$2")
: "${Qt6_DIR:?Qt6_DIR must be set}"
SLICER_REPOSITORY=${SLICER_REPOSITORY:-https://github.com/Slicer/Slicer.git}
SLICER_SHA=${SLICER_SHA:-4b9cbc3fef87fdb8c6d2d406d64de40a6064bc52}
JOBS=${JOBS:-$(nproc)}
mkdir -p "$WORK"
cd "$WORK"

# compiler cache: through the compiler wrappers of ccache, also in the external
# projects of the superbuilds
if command -v ccache > /dev/null && [ -d /usr/lib/ccache ]; then
  export CC=/usr/lib/ccache/gcc CXX=/usr/lib/ccache/g++
  export CCACHE_BASEDIR="$WORK" CCACHE_COMPRESS=1 CCACHE_SLOPPINESS=time_macros,include_file_mtime
  ccache --zero-stats > /dev/null
fi

echo "== 3D Slicer $SLICER_SHA"
if [ ! -d Slicer/.git ]; then
  git init -q Slicer
  git -C Slicer remote add origin "$SLICER_REPOSITORY"
fi
git -C Slicer fetch -q --depth 1 origin "$SLICER_SHA"
git -C Slicer checkout -q FETCH_HEAD

cmake -S Slicer -B Slicer-build \
  -DCMAKE_BUILD_TYPE:STRING=Release \
  -DQt6_DIR:PATH="$Qt6_DIR" \
  -DBUILD_TESTING:BOOL=OFF \
  -DSlicer_BUILD_DOCUMENTATION:BOOL=OFF \
  -DSlicer_BUILD_DICOM_SUPPORT:BOOL=OFF \
  -DSlicer_BUILD_DIFFUSION_SUPPORT:BOOL=OFF \
  -DSlicer_BUILD_BRAINSTOOLS:BOOL=OFF \
  -DSlicer_BUILD_CLI:BOOL=OFF \
  -DSlicer_BUILD_LEGACY_CLI:BOOL=OFF \
  -DSlicer_BUILD_EXTENSIONMANAGER_SUPPORT:BOOL=OFF \
  -DSlicer_BUILD_APPLICATIONUPDATE_SUPPORT:BOOL=OFF \
  -DSlicer_BUILD_MULTIMEDIA_SUPPORT:BOOL=OFF \
  -DSlicer_BUILD_WEBENGINE_SUPPORT:BOOL=OFF \
  -DSlicer_BUILD_QT_DESIGNER_PLUGINS:BOOL=OFF \
  -DSlicer_BUILD_I18N_SUPPORT:BOOL=OFF \
  -DSlicer_BUILD_USAGE_LOGGING_SUPPORT:BOOL=OFF \
  -DSlicer_USE_QtTesting:BOOL=OFF \
  -DSlicer_USE_SimpleITK:BOOL=OFF \
  -DSlicer_USE_VTK_DEBUG_LEAKS:BOOL=OFF

if [ -n "${SLICER_TIME_LIMIT:-}" ]; then
  timeout --signal=TERM "${SLICER_TIME_LIMIT}m" cmake --build Slicer-build -j "$JOBS" || {
    status=$?
    [ $status -eq 124 ] && echo "== 3D Slicer build stopped at the time limit (${SLICER_TIME_LIMIT} min): run again"
    exit $status
  }
else
  cmake --build Slicer-build -j "$JOBS"
fi

# numpy/scipy references of the smoothing tests (scipy is not part of 3D Slicer)
"$WORK/Slicer-build/python-install/bin/PythonSlicer" -m pip install -q scipy

echo "== SlicerAstro"
cmake -S "$SOURCE" -B SlicerAstro-build \
  -DCMAKE_BUILD_TYPE:STRING=Release \
  -DSlicer_DIR:PATH="$WORK/Slicer-build/Slicer-build" \
  -DBUILD_TESTING:BOOL=ON
cmake --build SlicerAstro-build -j "$JOBS"

if command -v ccache > /dev/null; then
  ccache --show-stats || true
fi

echo "== Tests"
cd SlicerAstro-build/inner-build
export SLICERASTRO_COMPRESS_MOUSE_MOVES=0
xvfb-run -a -s "-screen 0 1600x1000x24" \
  ctest -j 2 --output-on-failure --timeout 1800 ${CTEST_EXCLUDE:+-E "$CTEST_EXCLUDE"}
