# Configuration of the SlicerAstro documentation (Sphinx + MyST Markdown,
# as the 3D Slicer documentation).
import os
import re

project = "SlicerAstro"
author = "Davide Punzo and the SlicerAstro contributors"
copyright = "2016-2026, the SlicerAstro contributors"

# version from the top-level CMakeLists.txt
_cmake = open(os.path.join(os.path.dirname(__file__), "..", "CMakeLists.txt")).read()
version = ".".join(re.search(r'SLICERASTRO_VERSION_%s "(\d+)"' % part, _cmake).group(1)
                   for part in ("MAJOR", "MINOR", "PATCH"))
release = version

extensions = [
    "myst_parser",
    "sphinx_design",
]
myst_enable_extensions = [
    "colon_fence",
    "dollarmath",
    "attrs_inline",
]
myst_heading_anchors = 3
source_suffix = {".md": "markdown"}
root_doc = "index"
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store", "requirements.txt"]

html_theme = "sphinx_rtd_theme"
html_title = "SlicerAstro %s" % version
html_logo = "../SlicerAstroIcon.png"
html_static_path = ["_static"]
html_theme_options = {
    "navigation_depth": 3,
    "collapse_navigation": False,
}
