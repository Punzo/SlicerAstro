"""Contours of the Workbench: the datasets whose contours are drawn over a
dataset (chips of its card) and HI column density. Contours are drawn only
when the user asks (Contours section): never automatically."""

import vtk

import slicer

from . import Astro

def columnDensityFactor(volume):
    """cm^-2 per unit of a map of HI integrated intensity (0 if not one)."""
    if not Astro.isAstroVolume(volume) or Astro.dimensions(volume) != 2:
        return 0.0
    return slicer.vtkSlicerAstroVolumeLogic.GetHIColumnDensityFactor(volume)


def isHICube(volume):
    return Astro.isAstroVolume(volume) and Astro.dimensions(volume) >= 3 and "HI 21 cm" in Astro.chips(volume)


def isHIMap(volume):
    return columnDensityFactor(volume) > 0 and \
        Astro.attribute(volume, "DATAMODEL", "") in ("ZEROMOMENTMAP", "") and Astro.hasBeam(volume)


def contourSources(contours):
    """Datasets whose contours are drawn in a contours segmentation: {source
    ID: number of levels} (vtkSlicerAstroVolumeLogic.GetContourSources)."""
    sourceIDs = vtk.vtkStringArray()
    levels = vtk.vtkIntArray()
    slicer.vtkSlicerAstroVolumeLogic.GetContourSources(contours, sourceIDs, levels)
    return {sourceIDs.GetValue(index): levels.GetValue(index) for index in range(sourceIDs.GetNumberOfValues())}
