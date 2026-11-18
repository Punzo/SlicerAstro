"""Helpers of the Inspect tab of the Workbench (fold sections, buttons, colour
strips). The sections themselves are the reusable C++ sections of
AstroVolume (Info, Display, Contours, Masks, Sources, 3D)."""

import qt
import vtk

import slicer

from . import Astro


def theme():
    return slicer.app.property("SlicerAstroTheme")


def iconButton(iconName, toolTip, checkable=False, objectName=None):
    button = qt.QToolButton()
    button.setProperty("astroRole", "toolbar")
    button.setIcon(theme().icon(iconName, "muted"))
    button.setIconSize(qt.QSize(16, 16))
    button.toolTip = toolTip
    button.checkable = checkable
    button.setAutoRaise(True)
    if objectName:
        button.setObjectName(objectName)
    return button


def label(text, toolTip=""):
    widget = qt.QLabel(text)
    widget.setProperty("astroRole", "label")
    widget.toolTip = toolTip
    return widget


def foldSection(title, summary, key, collapsed=True):
    section = slicer.qSlicerAstroFoldSection()
    section.setObjectName(f"InspectSection_{title.replace(' ', '')}")
    section.title = title
    section.summary = summary
    section.settingsKey = f"SlicerAstro/AstroWorkbench/{key}Collapsed"
    section.setCollapsed(collapsed)
    return section


def astroVolumeLogic():
    return slicer.modules.astrovolume.logic()


def colourStrip(colorNode, width=48, height=12):
    """Pixmap of a colour table, from the lowest to the highest value."""
    pixmap = qt.QPixmap(width, height)
    table = colorNode.GetLookupTable() if colorNode is not None else None
    if table is None:
        pixmap.fill(qt.QColor(0, 0, 0, 0))
        return pixmap
    image = qt.QImage(width, height, qt.QImage.Format_RGB32)
    count = table.GetNumberOfTableValues()
    rgba = [0.0, 0.0, 0.0, 1.0]
    for x in range(width):
        table.GetTableValue(min(count - 1, int(x * count / width)), rgba)
        colour = qt.QColor.fromRgbF(rgba[0], rgba[1], rgba[2]).rgb()
        for y in range(height):
            image.setPixel(x, y, colour)
    return qt.QPixmap.fromImage(image)


def optionButton(text, toolTip, objectName):
    button = qt.QToolButton()
    button.setProperty("astroRole", "actionChip")
    button.text = text
    button.checkable = True
    button.toolTip = toolTip
    button.setObjectName(objectName)
    button.setCursor(qt.Qt.PointingHandCursor)
    return button
