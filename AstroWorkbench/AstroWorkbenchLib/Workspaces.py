"""Workspaces of the Workbench: saved scenes with a screenshot of the views,
listed as a logbook (newest first, grouped by date).

A workspace is a folder in the workspaces directory (setting
"SlicerAstro/WorkspacesDirectory", default <home>/SlicerAstro/Workspaces)
with the scene (scene.mrb), a screenshot of the views (preview.png) and a
description (workspace.json: name, created, modified, datasets, layout,
notes). The functions of this file are the logic (tested); WorkspacesPage is
the page of the Workbench drawer."""

import datetime
import json
import os
import re
import shutil

import qt
import slicer

from . import Sections

SETTINGS_KEY = "SlicerAstro/WorkspacesDirectory"
SCENE_FILE = "scene.mrb"
PREVIEW_FILE = "preview.png"
DESCRIPTION_FILE = "workspace.json"
PREVIEW_WIDTH = 640


def theme():
    return slicer.app.property("SlicerAstroTheme")


# ----------------------------------------------------------------------
# Logic

def workspacesDirectory():
    default = os.path.join(os.path.expanduser("~"), "SlicerAstro", "Workspaces")
    directory = qt.QSettings().value(SETTINGS_KEY, default) or default
    return str(directory)


def setWorkspacesDirectory(directory):
    qt.QSettings().setValue(SETTINGS_KEY, directory)


def folderName(name):
    """File-system safe folder name of a workspace name."""
    safe = re.sub(r"[^A-Za-z0-9._ -]+", "_", name).strip(" .") or "workspace"
    return safe[:80]


def _now():
    return datetime.datetime.now().isoformat(timespec="seconds")


def _datasets():
    """Names of the datasets of the scene (and their objects, FITS OBJECT, for
    the filter of the logbook)."""
    from . import Astro
    names, objects = [], []
    # the datasets, not their products (moment maps, smoothed copies...)
    for node in Astro.rootDatasets(slicer.mrmlScene):
        names.append(node.GetName())
        objectName = node.GetAttribute("SlicerAstro.OBJECT")
        if objectName and objectName.strip():
            objects.append(objectName.strip())
    return sorted(set(names)), sorted(set(objects))


def saveScreenshot(path):
    """Screenshot of the views (layout viewport), PREVIEW_WIDTH wide."""
    layoutManager = slicer.app.layoutManager()
    viewport = layoutManager.viewport() if layoutManager is not None else None
    if viewport is None:
        return False
    for index in range(layoutManager.threeDViewCount):
        layoutManager.threeDWidget(index).threeDView().forceRender()
    image = viewport.grab().toImage()
    if image.isNull():
        return False
    if image.width() > PREVIEW_WIDTH:
        image = image.scaledToWidth(PREVIEW_WIDTH, qt.Qt.SmoothTransformation)
    return image.save(path)


def saveWorkspace(name, notes="", directory=None, overwrite=False):
    """Save the scene as a workspace; returns its folder (None on failure).
    An existing workspace of the same name is replaced only if overwrite."""
    directory = directory or workspacesDirectory()
    folder = os.path.join(directory, folderName(name))
    description = readDescription(folder) if os.path.isdir(folder) else None
    if description is not None and not overwrite:
        raise FileExistsError(f'A workspace named "{name}" exists')
    os.makedirs(folder, exist_ok=True)
    saveScreenshot(os.path.join(folder, PREVIEW_FILE))
    if not slicer.util.saveScene(os.path.join(folder, SCENE_FILE)):
        return None
    layoutNode = slicer.app.layoutManager().layoutLogic().GetLayoutNode() if slicer.app.layoutManager() else None
    created = description.get("created") if description else None
    datasets, objects = _datasets()
    data = {
        "name": name,
        "created": created or _now(),
        "modified": _now(),
        "datasets": datasets,
        "objects": objects,
        "layout": layoutNode.GetViewArrangement() if layoutNode is not None else None,
        "notes": notes,
        "application": "SlicerAstro " + _version(),
    }
    with open(os.path.join(folder, DESCRIPTION_FILE), "w", encoding="utf-8") as file:
        json.dump(data, file, indent=2, ensure_ascii=False)
    return folder


def _version():
    return slicer.vtkSlicerAstroVolumeLogic.GetSlicerAstroVersion()


def readDescription(folder):
    try:
        with open(os.path.join(folder, DESCRIPTION_FILE), encoding="utf-8") as file:
            data = json.load(file)
    except (OSError, ValueError):
        return None
    if "objects" not in data:
        # saved by an earlier version: "datasets" were the FITS objects
        data["objects"] = data.get("datasets", [])
        data["datasets"] = []
    data["folder"] = folder
    data["preview"] = os.path.join(folder, PREVIEW_FILE)
    data["scene"] = os.path.join(folder, SCENE_FILE)
    return data


def listWorkspaces(directory=None):
    """Workspaces with a scene, newest first."""
    directory = directory or workspacesDirectory()
    if not os.path.isdir(directory):
        return []
    workspaces = []
    for entry in os.listdir(directory):
        description = readDescription(os.path.join(directory, entry))
        if description is not None and os.path.exists(description["scene"]):
            workspaces.append(description)
    return sorted(workspaces, key=lambda data: data.get("modified", ""), reverse=True)


def loadWorkspace(folder):
    """Replace the scene with the one of the workspace."""
    description = readDescription(folder)
    if description is None:
        return False
    # the current scene is closed first (as File > Close Scene), then the
    # workspace replaces it
    slicer.mrmlScene.Clear(0)
    slicer.util.loadScene(description["scene"], {"clear": True})
    layout = description.get("layout")
    if layout is not None and slicer.app.layoutManager() is not None:
        slicer.app.layoutManager().setLayout(int(layout))
    return True


def renameWorkspace(folder, name):
    """New name (and folder) of a workspace; returns the new folder."""
    description = readDescription(folder)
    if description is None:
        return None
    target = os.path.join(os.path.dirname(folder), folderName(name))
    if os.path.abspath(target) != os.path.abspath(folder):
        if os.path.exists(target):
            raise FileExistsError(f'A workspace named "{name}" exists')
        shutil.move(folder, target)
    description["name"] = name
    _writeDescription(target, description)
    return target


def _writeDescription(folder, description):
    data = {key: value for key, value in description.items() if key not in ("folder", "preview", "scene")}
    with open(os.path.join(folder, DESCRIPTION_FILE), "w", encoding="utf-8") as file:
        json.dump(data, file, indent=2, ensure_ascii=False)


def setWorkspaceNotes(folder, notes):
    """New notes of a workspace; returns False if it is not a workspace."""
    description = readDescription(folder)
    if description is None:
        return False
    description["notes"] = notes
    _writeDescription(folder, description)
    return True


def exportWorkspace(folder, path):
    """The workspace folder (scene, preview, description) as a zip file at
    path, with the folder at the top of the archive; returns path (None if
    it is not a workspace)."""
    import zipfile
    if readDescription(folder) is None:
        return None
    root = os.path.basename(os.path.normpath(folder))
    with zipfile.ZipFile(path, "w") as archive:
        for entry in sorted(os.listdir(folder)):
            source = os.path.join(folder, entry)
            if not os.path.isfile(source):
                continue
            # the scene bundle and the preview are compressed already
            compression = zipfile.ZIP_STORED if entry.endswith((".mrb", ".png")) else zipfile.ZIP_DEFLATED
            archive.write(source, f"{root}/{entry}", compress_type=compression)
    return path


def deleteWorkspace(folder):
    if readDescription(folder) is None:
        # only folders of workspaces are removed
        return False
    shutil.rmtree(folder)
    return True


def dateGroup(isoDate, today=None):
    """Logbook group of a date: Today, This week, Earlier."""
    today = today or datetime.date.today()
    try:
        day = datetime.datetime.fromisoformat(isoDate).date()
    except (TypeError, ValueError):
        return "Earlier"
    if day == today:
        return "Today"
    if (today - day).days < 7:
        return "This week"
    return "Earlier"


# ----------------------------------------------------------------------
# Page of the drawer

def _value(attribute):
    """A Qt property (a PythonQt attribute) or a method."""
    return attribute() if callable(attribute) else attribute


class TitleEdit(qt.QLineEdit):
    """A name edited in place: elided ("…") to the width when not edited (the
    full name in the tooltip), the full name while edited; onFinished(text)
    when the editing ends (Enter or focus out). Escape cancels."""

    def __init__(self, text, onFinished, parent=None):
        qt.QLineEdit.__init__(self, parent)
        self.onFinished = onFinished
        self.fullText = text
        self.editing = False
        self.showElided()

    def setFullText(self, text):
        self.fullText = text
        if not self.editing:
            self.showElided()

    def showElided(self):
        width = max(20, _value(self.width) - 12)
        self.setText(self.fontMetrics().elidedText(self.fullText, qt.Qt.ElideRight, width))
        self.setCursorPosition(0)

    def focusInEvent(self, event):
        self.editing = True
        self.setText(self.fullText)
        qt.QLineEdit.focusInEvent(self, event)

    def focusOutEvent(self, event):
        qt.QLineEdit.focusOutEvent(self, event)
        if not self.editing:
            return
        self.editing = False
        text = self.text.strip()
        self.showElided()
        if self.onFinished is not None:
            self.onFinished(text)

    def keyPressEvent(self, event):
        if event.key() in (qt.Qt.Key_Return, qt.Qt.Key_Enter):
            self.clearFocus()
            return
        if event.key() == qt.Qt.Key_Escape:
            self.setText(self.fullText)
            self.clearFocus()
            return
        qt.QLineEdit.keyPressEvent(self, event)

    def resizeEvent(self, event):
        qt.QLineEdit.resizeEvent(self, event)
        if not self.editing:
            self.showElided()


class NotesEdit(qt.QPlainTextEdit):
    """Notes edited in place: a fixed height of LINES lines (a vertical
    scroll bar only when the text is longer), so the card does not move
    while typing; onFinished(text) when the editing ends (focus out)."""

    LINES = 4

    def __init__(self, text, onFinished, parent=None, lines=None):
        qt.QPlainTextEdit.__init__(self, parent)
        self.onFinished = onFinished
        self.lines = lines or self.LINES
        self.setProperty("astroRole", "inlineEdit")
        self.setFrameShape(qt.QFrame.NoFrame)
        self.setLineWrapMode(qt.QPlainTextEdit.WidgetWidth)
        self.setHorizontalScrollBarPolicy(qt.Qt.ScrollBarAlwaysOff)
        self.setVerticalScrollBarPolicy(qt.Qt.ScrollBarAsNeeded)
        self.setTabChangesFocus(True)
        self.document().setDocumentMargin(2)
        self.setPlainText(text)
        self.setSizePolicy(qt.QSizePolicy.Expanding, qt.QSizePolicy.Fixed)
        margins = self.contentsMargins()
        self.setFixedHeight(self.lines * self.fontMetrics().lineSpacing()
                            + 2 * int(_value(self.document().documentMargin))
                            + margins.top() + margins.bottom() + 6)

    def focusOutEvent(self, event):
        qt.QPlainTextEdit.focusOutEvent(self, event)
        if self.onFinished is not None:
            self.onFinished(self.toPlainText())


class WorkspaceEntry(qt.QFrame):
    """One entry of the logbook: wide preview, name, date, datasets and the
    actions."""

    def __init__(self, page, description, parent=None):
        qt.QFrame.__init__(self, parent)
        self.page = page
        self.description = description
        self.setProperty("astroRole", "workspaceEntry")
        self.setObjectName(f"Workspace_{os.path.basename(description['folder'])}")
        layout = qt.QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)
        preview = qt.QLabel()
        preview.setObjectName("WorkspacePreview")
        preview.setAlignment(qt.Qt.AlignCenter)
        preview.setMinimumHeight(60)
        pixmap = qt.QPixmap(description["preview"])
        if not pixmap.isNull():
            # a wide strip (16:9) of the views
            width = 236
            height = width * 9 // 16
            scaled = pixmap.scaled(width, height, qt.Qt.KeepAspectRatioByExpanding, qt.Qt.SmoothTransformation)
            preview.setPixmap(Sections.roundedPixmap(scaled.copy((scaled.width() - width) // 2,
                                                                 (scaled.height() - height) // 2, width, height)))
        else:
            preview.text = "no preview"
            preview.setProperty("astroRole", "label")
        preview.toolTip = "Double-click: open this workspace."
        layout.addWidget(preview)
        # name and notes edited in place (saved when the editing finishes)
        self.titleEdit = TitleEdit(description.get("name", ""), self.onTitleEdited)
        self.titleEdit.setObjectName("WorkspaceTitle")
        self.titleEdit.setProperty("astroRole", "inlineEdit")
        self.titleEdit.setProperty("inlineTitle", True)
        self.titleEdit.toolTip = (description.get("name", "") + "\nClick to rename the workspace (Enter: done, "
                                  "Escape: cancel).\n" + description["folder"])
        self.titleEdit.setAlignment(qt.Qt.AlignHCenter)
        layout.addWidget(self.titleEdit)
        modified = description.get("modified", "").replace("T", " ")
        datasets = ", ".join(description.get("datasets", []))
        meta = qt.QLabel(f"{modified} · {datasets}" if datasets else modified)
        meta.setProperty("astroRole", "monoLabel")
        meta.wordWrap = True
        objects = ", ".join(description.get("objects", []))
        meta.toolTip = f"Saved {modified}. Datasets: {datasets or '-'}" + (f". Objects: {objects}" if objects else "")
        layout.addWidget(meta)
        self.notesEdit = NotesEdit(description.get("notes", ""), lambda text: self.onNotesEdited())
        self.notesEdit.setObjectName("WorkspaceNotesEdit")
        self.notesEdit.placeholderText = "Add notes…"
        self.notesEdit.toolTip = "Notes of the workspace: click to write or edit them (several lines); saved when you leave the field."
        layout.addWidget(self.notesEdit)
        actions = qt.QHBoxLayout()
        actions.setSpacing(2)
        for text, icon, tip, slot, objectName in (
                ("Open", "folder", "Replace the current scene with this workspace.", self.open, "WorkspaceOpen"),
                ("Update", "download", "Save the current scene into this workspace (replaces it, new preview).",
                 self.saveOver, "WorkspaceSaveOver")):
            button = qt.QPushButton(text)
            button.setObjectName(objectName)
            button.setIcon(theme().icon(icon, "text"))
            button.toolTip = tip
            button.connect("clicked()", slot)
            actions.addWidget(button)
        actions.addStretch(1)
        for icon, tip, slot, objectName in (("archive", "Export this workspace as a zip file (scene, preview "
                                             "and description), e.g. to share it.", self.export, "WorkspaceExport"),
                                            ("trash", "Delete this workspace (its folder).", self.delete,
                                             "WorkspaceDelete")):
            button = qt.QToolButton()
            button.setObjectName(objectName)
            button.setProperty("astroRole", "toolbar")
            button.setAutoRaise(True)
            button.setIcon(theme().icon(icon, "muted"))
            button.toolTip = tip
            button.connect("clicked()", slot)
            actions.addWidget(button)
        layout.addLayout(actions)

    def mouseDoubleClickEvent(self, event):
        self.open()

    def open(self):
        if slicer.mrmlScene.GetNumberOfNodesByClass("vtkMRMLAstroVolumeNode") > 0:
            answer = qt.QMessageBox.question(slicer.util.mainWindow(), "Open workspace",
                                             f"Open {self.description.get('name')}? The current scene is closed "
                                             "(unsaved changes are lost).",
                                             qt.QMessageBox.Open | qt.QMessageBox.Cancel, qt.QMessageBox.Cancel)
            if answer != qt.QMessageBox.Open:
                return
        with slicer.util.tryWithErrorDisplay("The workspace could not be opened.", waitCursor=True):
            loadWorkspace(self.description["folder"])
        self.page.workbench.rail.buttons["data"].click()

    def saveOver(self):
        with slicer.util.tryWithErrorDisplay("The workspace could not be saved.", waitCursor=True):
            saveWorkspace(self.description.get("name", ""), self.description.get("notes", ""),
                          os.path.dirname(self.description["folder"]), overwrite=True)
        self.page.refresh()

    def onTitleEdited(self, name):
        """The name edited in the card: the workspace renamed."""
        name = (name or "").strip()
        if not name or name == self.description.get("name", ""):
            self.titleEdit.setFullText(self.description.get("name", ""))
            return
        try:
            self.description["folder"] = renameWorkspace(self.description["folder"], name)
            self.description["name"] = name
            self.titleEdit.setFullText(name)
        except Exception as error:
            self.titleEdit.setFullText(self.description.get("name", ""))
            self.page.workbench.statusBar.showMessage(f"The workspace could not be renamed: {error}")
            return
        # the logbook is rebuilt after this signal (it deletes this entry)
        qt.QTimer.singleShot(0, self.page.refresh)

    def onNotesEdited(self):
        notes = self.notesEdit.toPlainText().strip()
        if notes == self.description.get("notes", ""):
            return
        if setWorkspaceNotes(self.description["folder"], notes):
            self.description["notes"] = notes

    def export(self):
        name = folderName(self.description.get("name", "workspace"))
        defaultPath = os.path.join(os.path.expanduser("~"), name + ".zip")
        path = qt.QFileDialog.getSaveFileName(slicer.util.mainWindow(), "Export workspace", defaultPath,
                                              "Zip files (*.zip)")
        if not path:
            return
        if not path.lower().endswith(".zip"):
            path += ".zip"
        with slicer.util.tryWithErrorDisplay("The workspace could not be exported.", waitCursor=True):
            exportWorkspace(self.description["folder"], path)
            self.page.workbench.statusBar.showMessage(f"Workspace exported: {path}")

    def delete(self):
        answer = qt.QMessageBox.question(slicer.util.mainWindow(), "Delete workspace",
                                         f"Delete the workspace {self.description.get('name')} and its folder?",
                                         qt.QMessageBox.Yes | qt.QMessageBox.No, qt.QMessageBox.No)
        if answer == qt.QMessageBox.Yes:
            deleteWorkspace(self.description["folder"])
            self.page.refresh()


class WorkspacesPage(qt.QWidget):
    """Save the scene as a workspace; the saved workspaces as a logbook."""

    def __init__(self, workbench, parent=None):
        qt.QWidget.__init__(self, parent)
        self.workbench = workbench
        layout = qt.QVBoxLayout(self)
        layout.setContentsMargins(14, 14, 14, 10)
        layout.setSpacing(8)
        title = qt.QLabel("Workspaces")
        title.setProperty("astroRole", "title")
        layout.addWidget(title)
        saveRow = qt.QHBoxLayout()
        self.nameEdit = qt.QLineEdit()
        self.nameEdit.setObjectName("WorkspaceName")
        self.nameEdit.placeholderText = "Name of the workspace"
        self.nameEdit.toolTip = "Name of the new workspace (the scene, a screenshot of the views and the datasets)."
        saveRow.addWidget(self.nameEdit, 1)
        self.saveButton = qt.QPushButton("Save")
        self.saveButton.setObjectName("WorkspaceSave")
        self.saveButton.setProperty("astroRole", "primary")
        self.saveButton.setIcon(theme().icon("download", "on-accent"))
        self.saveButton.toolTip = "Save the current scene as a new workspace."
        self.saveButton.connect("clicked()", self.save)
        self.nameEdit.connect("returnPressed()", self.save)
        saveRow.addWidget(self.saveButton)
        layout.addLayout(saveRow)
        # as high as the other fields for one line
        self.notesEdit = NotesEdit("", None, lines=2)
        self.notesEdit.setProperty("astroRole", "")
        self.notesEdit.setObjectName("WorkspaceNotes")
        self.notesEdit.placeholderText = "Notes (optional)"
        self.notesEdit.toolTip = "Notes about the workspace (several lines), shown in the logbook."
        layout.addWidget(self.notesEdit)
        self.filterEdit = qt.QLineEdit()
        self.filterEdit.setObjectName("WorkspaceFilter")
        self.filterEdit.placeholderText = "Filter by name, dataset, notes"
        self.filterEdit.toolTip = "Show only the workspaces whose name, datasets or notes contain this text."
        self.filterEdit.addAction(theme().icon("search", "subtle"), qt.QLineEdit.LeadingPosition)
        self.filterEdit.connect("textChanged(QString)", lambda text: self.refresh())
        layout.addWidget(self.filterEdit)
        scroll = qt.QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(qt.QFrame.NoFrame)
        scroll.setHorizontalScrollBarPolicy(qt.Qt.ScrollBarAlwaysOff)
        slicer.qSlicerAstroScrollAreaFitter(scroll)
        self.entriesWidget = qt.QWidget()
        self.entriesLayout = qt.QVBoxLayout(self.entriesWidget)
        self.entriesLayout.setContentsMargins(0, 0, 4, 0)
        self.entriesLayout.setSpacing(8)
        scroll.setWidget(self.entriesWidget)
        layout.addWidget(scroll, 1)
        # the folder of the workspaces is shown in About (Folders) and in the
        # documentation, not here

    def save(self):
        name = self.nameEdit.text.strip() or datetime.datetime.now().strftime("Workspace %Y-%m-%d %H.%M")
        folder = os.path.join(workspacesDirectory(), folderName(name))
        overwrite = False
        if readDescription(folder) is not None:
            answer = qt.QMessageBox.question(slicer.util.mainWindow(), "Save workspace",
                                             f'Replace the workspace "{name}"?',
                                             qt.QMessageBox.Yes | qt.QMessageBox.No, qt.QMessageBox.No)
            if answer != qt.QMessageBox.Yes:
                return
            overwrite = True
        with slicer.util.tryWithErrorDisplay("The workspace could not be saved.", waitCursor=True):
            saveWorkspace(name, self.notesEdit.toPlainText().strip(), overwrite=overwrite)
            self.workbench.statusBar.showMessage(f"Workspace saved: {name}")
        self.nameEdit.text = ""
        self.notesEdit.setPlainText("")
        self.refresh()

    def refresh(self):
        while self.entriesLayout.count():
            item = self.entriesLayout.takeAt(0)
            if item.widget() is not None:
                item.widget().deleteLater()
        text = self.filterEdit.text.strip().lower()
        group = None
        count = 0
        for description in listWorkspaces():
            haystack = " ".join([description.get("name", ""), " ".join(description.get("datasets", [])),
                                 " ".join(description.get("objects", [])), description.get("notes", "")]).lower()
            if text and text not in haystack:
                continue
            entryGroup = dateGroup(description.get("modified"))
            if entryGroup != group:
                group = entryGroup
                label = qt.QLabel(group.upper())
                label.setProperty("astroRole", "overline")
                self.entriesLayout.addWidget(label)
            self.entriesLayout.addWidget(WorkspaceEntry(self, description))
            count += 1
        if count == 0:
            empty = qt.QLabel("No workspace yet: name the current scene and save it." if not text
                              else "No workspace matches the filter.")
            empty.setProperty("astroRole", "label")
            empty.wordWrap = True
            self.entriesLayout.addWidget(empty)
        self.entriesLayout.addStretch(1)
