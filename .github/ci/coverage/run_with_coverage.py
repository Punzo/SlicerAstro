"""Run the CTest tests of a SlicerAstro build with Python coverage.

The Python tests run 3D Slicer with --testing, which ignores the startup
script (SLICERRC), so the coverage start (slicerrc_coverage.py) is prepended to
the --python-code of each test. C++ tests run unchanged (gcov writes .gcda
files). Tests with the resource lock SlicerAstroDisplay (windows on the
display) run one at a time, the others in parallel.

  python run_with_coverage.py <inner build dir> <log dir> [jobs] [regex]
"""
import json
import os
import re
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

build = os.path.abspath(sys.argv[1])
logs = os.path.abspath(sys.argv[2])
jobs = int(sys.argv[3]) if len(sys.argv) > 3 else 4
pattern = re.compile(sys.argv[4]) if len(sys.argv) > 4 else None
rc = os.path.join(os.path.dirname(os.path.abspath(__file__)), "slicerrc_coverage.py")
os.makedirs(logs, exist_ok=True)

info = json.loads(subprocess.run(["ctest", "--show-only=json-v1"], cwd=build, capture_output=True,
                                 text=True, check=True).stdout)


def properties(test, name):
    return next((p["value"] for p in test.get("properties", []) if p["name"] == name), None)


def environment(test):
    env = dict(os.environ)
    for item in properties(test, "ENVIRONMENT") or []:
        key, _, value = item.partition("=")
        env[key] = value
    return env


def run(test):
    name = test["name"]
    command = list(test["command"])
    if "--python-code" in command:
        index = command.index("--python-code") + 1
        command[index] = "exec(open(%r).read()); " % rc + command[index]
    workdir = properties(test, "WORKING_DIRECTORY") or build
    start = time.time()
    with open(os.path.join(logs, name + ".log"), "w") as log:
        try:
            status = subprocess.run(command, cwd=workdir, env=environment(test), stdout=log,
                                    stderr=subprocess.STDOUT, timeout=1800).returncode
        except subprocess.TimeoutExpired:
            status = "timeout"
    # the failure expressions of the tests (e.g. VTK leaks)
    if status == 0:
        with open(os.path.join(logs, name + ".log")) as log:
            text = log.read()
        for expression in properties(test, "FAIL_REGULAR_EXPRESSION") or []:
            if re.search(expression, text):
                status = "matched " + expression
    return name, status, time.time() - start


tests = [t for t in info["tests"] if not pattern or pattern.search(t["name"])]
display = [t for t in tests if "SlicerAstroDisplay" in (properties(t, "RESOURCE_LOCK") or [])]
others = [t for t in tests if t not in display]
failed = []


def report(result):
    name, status, seconds = result
    print("%-50s %-8s %6.1f s" % (name, "ok" if status == 0 else "FAILED(%s)" % status, seconds), flush=True)
    if status != 0:
        failed.append(name)


with ThreadPoolExecutor(max_workers=jobs) as pool:
    pending = [pool.submit(run, test) for test in others]
    # the tests with windows one at a time, next to the others
    for test in display:
        report(run(test))
    for future in pending:
        report(future.result())
print("%d tests, %d failed: %s" % (len(tests), len(failed), " ".join(failed)))
sys.exit(1 if failed else 0)
