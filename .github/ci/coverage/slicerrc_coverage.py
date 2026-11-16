# 3D Slicer startup script (SLICERRC) measuring the Python coverage of the
# SlicerAstro scripted modules. It runs after the modules were imported, so the
# module files are executed again (in a throwaway namespace) to count the lines
# run at import time (imports, class and function definitions).
import glob
import os
import sys

_config = os.environ.get("COVERAGE_PROCESS_START")
_modules = os.environ.get("SLICERASTRO_COVERAGE_MODULES")
if _config:
    sys.path.insert(0, os.environ["SLICERASTRO_COVERAGE_PYLIB"])
    import coverage

    _coverage = coverage.Coverage(config_file=_config)
    _coverage.start()
    if _modules:
        for _file in sorted(glob.glob(os.path.join(_modules, "*.py"))):
            if _file.endswith("Test.py"):
                continue
            with open(_file) as _source:
                exec(compile(_source.read(), _file, "exec"), {"__name__": "_coverage_" + os.path.basename(_file)[:-3], "__file__": _file})

    def _saveCoverage():
        _coverage.stop()
        _coverage.save()

    slicer.app.connect("aboutToQuit()", _saveCoverage)
