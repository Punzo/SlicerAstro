# AstroWorkbench

AstroWorkbench (module **AstroWorkbench**, `Ctrl+Shift+W`) is a viewer-style
interface of SlicerAstro: the datasets on the left, the views in the centre
with the channel scrubber below them, and the inspector on the right (Inspect,
Mask, Sources, Process, Model). The full guide follows with the
documentation of the new user interface; this page describes the home page
and where the Workbench keeps its files.

## Home

The rail on the left has **Home**, **Scenes** and **Data** (and **Sky**, the
Virtual Observatory search of a later version). **Home** is the welcome page
of SlicerAstro in place of the views and panels: open data, the scenes,
the documentation, the recent files and scenes (newest first) and the
sample data (downloaded once, then cached). It opens when the Workbench starts
without data; loading data, or **Scenes** or **Data**, brings the views and
the panels back. The same page, in one column, is the **Welcome** module of
classic Slicer.

## Scenes

A saved scene keeps the state of the Workbench: the Slicer scene (data,
masks, contours, products, views), a preview of the views and a description
(name, date, datasets, notes). Saved scenes are listed in the **Scenes**
entry of the left rail, where they are saved, opened, updated, exported as a
zip file, renamed and deleted.

Each saved scene is a folder in the scenes folder:

```
<scenes folder>/<scene name>/
    scene.mrb          the scene
    preview.png        the views when the scene was saved
    scene.json         name, dates, datasets, notes
```

The scenes folder is by default `~/SlicerAstro/Scenes` (in the home
folder of the user). It is shown in **About › Folders** (`F1`), and it is
kept in the application setting `SlicerAstro/ScenesDirectory`. To use
another folder, choose it in the SlicerAstro settings, or set it from the
Python console:

```python
from AstroWorkbenchLib import Scenes
Scenes.setScenesDirectory("/data/my-scenes")
```

The downloaded sample data are kept in the cache of Slicer, also shown in
**About › Folders**.
