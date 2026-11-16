#!/bin/bash
# Code coverage of SlicerAstro: SlicerAstro built in Debug with --coverage
# against an existing 3D Slicer build tree, its tests run, reports of the C++
# (gcovr) and Python (coverage.py) code written.
#
#   coverage.sh <SlicerAstro source dir> <Slicer build tree (Slicer-build)> <work dir>
#
# Environment: Qt6_DIR or Qt5_DIR (as the 3D Slicer build), JOBS.
# Reports: <work dir>/coverage/cxx/index.html, cxx-summary.json,
#          <work dir>/coverage/python/html/index.html, python.json.
set -euo pipefail

SOURCE=$(realpath "$1")
SLICER_BUILD=$(realpath "$2")
WORK=$(realpath -m "$3")
JOBS=${JOBS:-$(nproc)}
HERE=$(dirname "$(realpath "$0")")
mkdir -p "$WORK"

QT_ARGS=()
[ -n "${Qt6_DIR:-}" ] && QT_ARGS=(-DQt6_DIR:PATH="$Qt6_DIR")
[ -n "${Qt5_DIR:-}" ] && QT_ARGS=(-DQt5_DIR:PATH="$Qt5_DIR")

echo "== SlicerAstro with coverage"
cmake -S "$SOURCE" -B "$WORK/build" \
  -DCMAKE_BUILD_TYPE:STRING=Debug \
  -DSlicer_DIR:PATH="$SLICER_BUILD" \
  "${QT_ARGS[@]}" \
  -DADDITIONAL_C_FLAGS:STRING=--coverage \
  -DADDITIONAL_CXX_FLAGS:STRING=--coverage \
  -DBUILD_TESTING:BOOL=ON
cmake --build "$WORK/build" -j "$JOBS"
INNER="$WORK/build/inner-build"

echo "== Tools (gcovr, coverage.py)"
python3 -m venv "$WORK/venv"
"$WORK/venv/bin/pip" install -q gcovr coverage
# coverage.py in the Python of 3D Slicer, from its own folder
PYLIB="$WORK/pylib"
"$WORK/venv/bin/pip" install -q --target "$PYLIB" --no-deps coverage

echo "== Tests"
export SLICERASTRO_COVERAGE_DIR="$WORK/coverage"
export SLICERASTRO_SOURCE_DIR="$SOURCE"
export SLICERASTRO_COVERAGE_MODULES="$INNER/lib/Slicer-5.13/qt-scripted-modules"
export SLICERASTRO_COVERAGE_PYLIB="$PYLIB"
export COVERAGE_PROCESS_START="$HERE/coveragerc"
export QT_QPA_PLATFORM=xcb
mkdir -p "$SLICERASTRO_COVERAGE_DIR/python"
status=0
RUNNER=("$WORK/venv/bin/python" "$HERE/run_with_coverage.py" "$INNER" "$WORK/coverage/logs" "$JOBS")
if command -v xvfb-run > /dev/null; then
  xvfb-run -a -s "-screen 0 1600x1000x24" "${RUNNER[@]}" || status=$?
else
  "${RUNNER[@]}" || status=$?
fi

echo "== Reports"
mkdir -p "$WORK/coverage/cxx"
"$WORK/venv/bin/gcovr" -r "$SOURCE" "$INNER" --object-directory "$INNER" \
  --exclude '.*/Testing/.*' --exclude '.*moc_.*' --exclude '.*ui_.*' --exclude '.*qrc_.*' \
  --exclude '.*vtkMRML.*Python.*' --exclude '.*PythonQt.*' --exclude '.*/inner-build/.*' \
  --gcov-ignore-errors=no_working_dir_found --gcov-ignore-parse-errors=negative_hits.warn \
  --html-details "$WORK/coverage/cxx/index.html" --json-summary "$WORK/coverage/cxx-summary.json" \
  --print-summary | tee "$WORK/coverage/cxx-summary.txt"
cd "$WORK/coverage/python"
"$WORK/venv/bin/coverage" combine --rcfile "$COVERAGE_PROCESS_START" > /dev/null || true
"$WORK/venv/bin/coverage" html --rcfile "$COVERAGE_PROCESS_START" > /dev/null || true
"$WORK/venv/bin/coverage" json --rcfile "$COVERAGE_PROCESS_START" -o "$WORK/coverage/python.json" > /dev/null || true
"$WORK/venv/bin/coverage" report --rcfile "$COVERAGE_PROCESS_START" | tail -1 | tee "$WORK/coverage/python-summary.txt" || true
exit $status
