# AstroWorkbench

AstroWorkbench (module **AstroWorkbench**, `Ctrl+Shift+W`) is a viewer-style
interface of SlicerAstro: the datasets on the left, the views in the centre
with the channel scrubber below them, and the inspector on the right (Inspect,
Mask, Sources, Process, Model). The full guide follows with the
documentation of the new user interface; this page describes where the
Workbench keeps its files.

## Workspaces

A workspace is a saved state of the Workbench: the scene (data, masks,
contours, products, views), a preview of the views and a description (name,
date, datasets, notes). Workspaces are listed in the **Spaces** entry of the
left rail, where they are saved, opened, updated, exported as a zip file,
renamed and deleted.

Each workspace is a folder in the workspaces folder:

```
<workspaces folder>/<workspace name>/
    scene.mrb          the scene
    preview.png        the views when the workspace was saved
    workspace.json     name, dates, datasets, notes
```

The workspaces folder is by default `~/SlicerAstro/Workspaces` (in the home
folder of the user). It is shown in **About › Folders** (`F1`), and it is
kept in the application setting `SlicerAstro/WorkspacesDirectory`. To use
another folder, set it from the Python console:

```python
from AstroWorkbenchLib import Workspaces
Workspaces.setWorkspacesDirectory("/data/my-workspaces")
```

The downloaded sample data are kept in the cache of Slicer, also shown in
**About › Folders**.
