"""About SlicerAstro: version and libraries, the shortcuts of the Workbench
and of the views, credits (authors, funding, citations, libraries, fonts,
data)."""

import sys

import qt
import vtk

import slicer

DOCUMENTATION_URL = "https://punzo.github.io/SlicerAstro/"
ISSUES_URL = "https://github.com/Punzo/SlicerAstro/issues"
SOURCE_URL = "https://github.com/Punzo/SlicerAstro"

CITATIONS = [
    "Punzo, D., van der Hulst, J. M., Roerdink, J. B. T. M., Fillion-Robin, J. C., Yu, L. 2017, "
    "SlicerAstro: a 3-D interactive visual analytics tool for HI data, Astronomy and Computing, 19, 45 "
    "(arXiv:1703.06651)",
    "Punzo, D., van der Hulst, J. M., Roerdink, J. B. T. M., Oosterloo, T. A., Ramatsoku, M., "
    "Verheijen, M. A. W. 2015, The role of 3-D interactive visualization in blind surveys of HI in "
    "galaxies, Astronomy and Computing, 12, 86 (arXiv:1505.06976)",
]

# (keys, action); "Ctrl" is "Cmd" on macOS
SHORTCUTS = [
    ("Workbench", [
        ("Ctrl+Shift+W", "open or close the Workbench (full Slicer)"),
        ("Ctrl+Shift+T", "switch between the dark and light theme"),
        ("Space", "play or pause the channels"),
        ("F1", "this window"),
        ("Ctrl+3", "Python console (developer button)"),
        ("drag a dataset", "show it in one view (the 3D view renders it)"),
        ("click in a view", "make it the active view (the scrubber moves it)"),
    ]),
    ("Slice views", [
        ("mouse wheel · ← → · b f", "previous or next channel (slice)"),
        ("Ctrl+wheel · right-drag", "zoom"),
        ("middle-drag · Shift+left-drag", "pan"),
        ("Shift+move", "move the crosshair in all the views"),
        ("r", "reset zoom and pan"),
        ("[ ]", "previous or next data as background"),
        ("t", "show or hide the foreground"),
        ("double-click", "maximize the view or restore the layout"),
    ]),
    ("3D views", [
        ("left-drag · arrows", "rotate"),
        ("right-drag · Ctrl+wheel · + -", "zoom"),
        ("middle-drag · Shift+left-drag", "pan"),
        ("Home · End · Page Down", "look from an axis (with Shift: the opposite one)"),
        ("Insert · Keypad 0", "reset the camera"),
        ("Ctrl+B", "tilt lock"),
    ]),
]


def theme():
    return slicer.app.property("SlicerAstroTheme")


def overline(text):
    label = qt.QLabel(text.upper())
    label.setProperty("astroRole", "overline")
    font = label.font
    font.setLetterSpacing(qt.QFont.AbsoluteSpacing, 0.9)
    label.setFont(font)
    return label


def paragraph(text, role="label"):
    # links in the data colour of the theme (Qt's default blue is unreadable
    # on the dark theme)
    if "<a " in text:
        text = text.replace("<a ", f"<a style='color:{theme().color('data').name()}; text-decoration: none;' ")
    label = qt.QLabel(text)
    label.wordWrap = True
    label.setProperty("astroRole", role)
    label.setTextInteractionFlags(qt.Qt.TextBrowserInteraction)
    label.openExternalLinks = True
    return label


def versions():
    """(name, version) of SlicerAstro, 3D Slicer and the libraries."""
    logic = slicer.vtkSlicerAstroVolumeLogic
    result = [("SlicerAstro", logic.GetSlicerAstroVersion()),
              ("3D Slicer", f"{slicer.app.applicationVersion} ({slicer.app.repositoryRevision[:10]})"),
              ("Qt", qt.qVersion()), ("VTK", vtk.vtkVersion.GetVTKVersion()),
              ("Python", sys.version.split()[0])]
    for entry in logic.GetLibraryVersions().split(";"):
        name, _, version = entry.partition(" ")
        result.append((name, version))
    return result


class AboutDialog(qt.QDialog):

    def __init__(self, parent=None):
        qt.QDialog.__init__(self, parent if parent is not None else slicer.util.mainWindow())
        self.setObjectName("AboutSlicerAstro")
        self.setWindowTitle("About SlicerAstro")
        self.resize(660, 640)
        layout = qt.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # header: logo, name, version
        header = qt.QFrame()
        header.setProperty("astroRole", "panelHeader")
        headerLayout = qt.QHBoxLayout(header)
        headerLayout.setContentsMargins(22, 20, 22, 16)
        headerLayout.setSpacing(16)
        logo = qt.QLabel()
        logo.setObjectName("AboutLogo")
        logo.setPixmap(qt.QIcon(":/AstroTheme/Logo/SlicerAstroLogo.svg").pixmap(114, 114))
        headerLayout.addWidget(logo)
        titles = qt.QVBoxLayout()
        titles.setSpacing(2)
        title = qt.QLabel("SlicerAstro")
        title.setStyleSheet("font-size: 24px; font-weight: 700;")
        titles.addWidget(title)
        version = qt.QLabel(f"Version {slicer.vtkSlicerAstroVolumeLogic.GetSlicerAstroVersion()} · "
                            f"3D Slicer {slicer.app.applicationVersion}")
        version.setObjectName("AboutVersion")
        version.setProperty("astroRole", "monoLabel")
        titles.addWidget(version)
        tagline = qt.QLabel("3D visualisation and analysis of astronomical data cubes")
        tagline.setProperty("astroRole", "label")
        titles.addWidget(tagline)
        headerLayout.addLayout(titles, 1)
        layout.addWidget(header)

        # sections
        tabsRow = qt.QHBoxLayout()
        tabsRow.setContentsMargins(22, 14, 22, 6)
        self.tabs = slicer.qSlicerAstroSegmentedButton()
        self.tabs.setObjectName("AboutTabs")
        for name in ("About", "Shortcuts", "Credits"):
            self.tabs.addButton(name)
        tabsRow.addWidget(self.tabs)
        tabsRow.addStretch(1)
        layout.addLayout(tabsRow)
        self.stack = qt.QStackedWidget()
        self.stack.addWidget(self.scroll(self.buildAbout()))
        self.stack.addWidget(self.scroll(self.buildShortcuts()))
        self.stack.addWidget(self.scroll(self.buildCredits()))
        self.tabs.connect("currentIndexChanged(int)", self.stack.setCurrentIndex)
        layout.addWidget(self.stack, 1)

        # footer
        footer = qt.QFrame()
        footer.setProperty("astroRole", "panelFooter")
        footerLayout = qt.QHBoxLayout(footer)
        footerLayout.setContentsMargins(22, 12, 22, 12)
        for text, url, icon in (("Documentation", DOCUMENTATION_URL, "help"), ("Report an issue", ISSUES_URL, "warning"),
                                ("Source", SOURCE_URL, "link")):
            button = qt.QPushButton(text)
            button.setIcon(theme().icon(icon, "text"))
            button.connect("clicked()", lambda u=url: qt.QDesktopServices.openUrl(qt.QUrl(u)))
            footerLayout.addWidget(button)
        footerLayout.addStretch(1)
        close = qt.QPushButton("Close")
        close.setObjectName("AboutClose")
        close.setProperty("astroRole", "primary")
        close.setMinimumWidth(96)
        close.connect("clicked()", self.accept)
        footerLayout.addWidget(close)
        layout.addWidget(footer)
        theme().applyTo(self)

    def scroll(self, widget):
        area = qt.QScrollArea()
        area.setWidgetResizable(True)
        area.setFrameShape(qt.QFrame.NoFrame)
        area.setHorizontalScrollBarPolicy(qt.Qt.ScrollBarAlwaysOff)
        area.setWidget(widget)
        return area

    def page(self):
        widget = qt.QWidget()
        layout = qt.QVBoxLayout(widget)
        layout.setContentsMargins(22, 10, 22, 18)
        layout.setSpacing(12)
        return widget, layout

    # ------------------------------------------------------------------
    def buildAbout(self):
        widget, layout = self.page()
        layout.addWidget(paragraph(
            "SlicerAstro is an extension of 3D Slicer for the interactive visualisation and analysis of "
            "three-dimensional astronomical data, above all HI (neutral hydrogen) data cubes. A cube is "
            "treated as a 3D object, two axes on the sky and one along the velocity, so that faint and "
            "complex structures, such as tidal tails, filaments and gas around galaxies, can be explored "
            "by linking 3D renderings, slices, plots and quantitative analysis in one place, always in "
            "sky and velocity coordinates.", role=""))
        layout.addWidget(overline("Versions"))
        grid = qt.QGridLayout()
        grid.setHorizontalSpacing(18)
        grid.setVerticalSpacing(5)
        self.versionText = []
        for row, (name, value) in enumerate(versions()):
            label = qt.QLabel(name)
            label.setProperty("astroRole", "label")
            text = qt.QLabel(value)
            text.setProperty("astroRole", "mono")
            text.setObjectName(f"AboutVersion_{name.replace(' ', '')}")
            text.setTextInteractionFlags(qt.Qt.TextSelectableByMouse)
            grid.addWidget(label, row, 0)
            grid.addWidget(text, row, 1)
            self.versionText.append(f"{name} {value}")
        grid.setColumnStretch(1, 1)
        layout.addLayout(grid)
        row = qt.QHBoxLayout()
        copy = qt.QPushButton("Copy the versions")
        copy.setObjectName("AboutCopyVersions")
        copy.setProperty("astroRole", "ghost")
        copy.setIcon(theme().icon("copy"))
        copy.toolTip = "For a bug report"
        copy.connect("clicked()", lambda: qt.QApplication.clipboard().setText("\n".join(self.versionText)))
        row.addWidget(copy)
        row.addStretch(1)
        layout.addLayout(row)
        layout.addWidget(paragraph(
            "SlicerAstro is free software under the BSD 3-Clause licence. Its packages include "
            "libraries under other licences (see Credits)."))
        layout.addStretch(1)
        return widget

    def buildShortcuts(self):
        widget, layout = self.page()
        mac = sys.platform == "darwin"
        for section, entries in SHORTCUTS:
            layout.addWidget(overline(section))
            grid = qt.QGridLayout()
            grid.setHorizontalSpacing(16)
            grid.setVerticalSpacing(6)
            for row, (keys, action) in enumerate(entries):
                if mac:
                    keys = keys.replace("Ctrl", "Cmd")
                key = qt.QLabel(keys)
                key.setProperty("astroRole", "chip")
                key.setStyleSheet("font-family: 'Atkinson Hyperlegible Mono'; font-size: 11px;")
                text = qt.QLabel(action)
                text.wordWrap = True
                grid.addWidget(key, row, 0, qt.Qt.AlignLeft | qt.Qt.AlignTop)
                grid.addWidget(text, row, 1)
            grid.setColumnStretch(1, 1)
            layout.addLayout(grid)
        layout.addWidget(paragraph("Keys act on the active view: click in a view first. "
                                   + ("" if mac else "On macOS, Ctrl is Cmd.")))
        layout.addStretch(1)
        return widget

    def buildCredits(self):
        widget, layout = self.page()
        layout.addWidget(overline("Authors"))
        layout.addWidget(paragraph(
            "Davide Punzo (<a href='mailto:punzodavide@hotmail.it'>punzodavide@hotmail.it</a>) and "
            "Thijs van der Hulst, Kapteyn Astronomical Institute, University of Groningen. SlicerAstro was "
            "supported by the European Research Council (grant nr. 291531) and the 3D Slicer community.",
            role=""))
        layout.addWidget(overline("Thanks"))
        layout.addWidget(paragraph(
            "Special thanks to Mpati Ramatsoku for the WEIN069 data cube, from her WSRT Zone of Avoidance "
            "Perseus-Pisces Filament survey (Ramatsoku et al. 2016, MNRAS, 460, 923), the sample data of "
            "SlicerAstro since its first version.", role=""))
        layout.addWidget(overline("Citing"))
        layout.addWidget(paragraph("If you use SlicerAstro in your research, please cite:"))
        for citation in CITATIONS:
            layout.addWidget(paragraph(citation, role="note"))
        row = qt.QHBoxLayout()
        copy = qt.QPushButton("Copy the citations")
        copy.setObjectName("AboutCopyCitations")
        copy.setProperty("astroRole", "ghost")
        copy.setIcon(theme().icon("copy"))
        copy.connect("clicked()", lambda: qt.QApplication.clipboard().setText("\n\n".join(CITATIONS)))
        row.addWidget(copy)
        row.addStretch(1)
        layout.addLayout(row)
        layout.addWidget(paragraph(
            "Please also cite 3D Slicer (Fedorov et al. 2012), 3DBarolo (Di Teodoro & Fraternali "
            "2015) for the kinematic models, and the surveys of your data."))
        layout.addWidget(overline("Built with"))
        layout.addWidget(paragraph(
            "<a href='https://www.slicer.org'>3D Slicer</a>, VTK, CTK and Qt · "
            "<a href='https://heasarc.gsfc.nasa.gov/fitsio/'>CFITSIO</a> (NASA) · "
            "<a href='https://www.atnf.csiro.au/people/mcalabre/WCS/'>WCSLIB</a> (M. Calabretta, LGPL-3) · "
            "<a href='https://github.com/liberfa/erfa'>ERFA</a> (BSD-3) · "
            "<a href='https://www.fftw.org'>FFTW</a> (GPL-2) · "
            "<a href='https://github.com/editeodoro/Bbarolo'>3DBarolo</a> (E. Di Teodoro, GPL-2) · "
            "the exact-overlap reprojection uses the spherical-polygon overlap code of "
            "<a href='https://github.com/astropy/reproject'>astropy reproject</a>, derived from "
            "<a href='http://montage.ipac.caltech.edu'>Montage</a> (Caltech/IPAC; both BSD-3). "
            "Because of FFTW and 3DBarolo the packages as a whole are under the GPL; the SlicerAstro "
            "sources are BSD."))
        layout.addWidget(overline("Design"))
        layout.addWidget(paragraph(
            "Fonts: Atkinson Hyperlegible Next and Mono (Braille Institute of America, SIL Open Font "
            "Licence 1.1). Logo, icons and the Observatory and Daylight themes: SlicerAstro."))
        layout.addWidget(overline("Sample data"))
        try:
            import AstroSampleData
            samples = AstroSampleData.SAMPLE_DATA_INFO
        except Exception:
            samples = {}
        for name, (short, details, credit) in samples.items():
            layout.addWidget(paragraph(f"<b>{name}</b> · {short}<br>{details}<br><i>{credit}</i>"))
        layout.addWidget(paragraph(
            "Provenance, preparation and the acknowledgements requested by each survey: release notes of "
            "<a href='https://github.com/Punzo/SlicerAstro/releases/tag/SampleData-v2'>SampleData-v2</a>."))
        layout.addStretch(1)
        return widget


def showAbout(section=0):
    dialog = AboutDialog()
    dialog.tabs.setCurrentIndex(section)
    dialog.exec()
    return dialog
