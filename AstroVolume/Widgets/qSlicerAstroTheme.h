/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#ifndef __qSlicerAstroTheme_h
#define __qSlicerAstroTheme_h

// Qt includes
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QVector>
#include <QPalette>
#include <QScopedPointer>

#include "qSlicerAstroVolumeModuleWidgetsExport.h"

class QLabel;
class qSlicerAstroThemePrivate;

/// \ingroup SlicerAstro_QtModules_AstroVolume_Widgets
/// The "Observatory" (dark) and "Daylight" (light) themes of SlicerAstro.
///
/// The colours are named tokens (bg, panel, raised, hover, line, line-strong,
/// text, muted, subtle, accent, accent-ink, on-accent, data, ok, warn, err,
/// view, ...): the style sheet and the palette are generated from them. The
/// fonts (Atkinson Hyperlegible Next and Mono, OFL) are bundled.
///
/// In the extension the theme is applied only to the SlicerAstro panels
/// (applyTo); SlicerAstroApp applies it to the whole application
/// (applyToApplication). The mode (dark, light, system) is stored in the
/// settings ("SlicerAstro/Theme"); system follows the colours of Slicer (or
/// of the desktop for the application).
///
/// Icons are SVG files drawn in the text colour ("currentColor"), tinted
/// with a token when painted: they follow a change of theme.
class vtkSegmentation;

class Q_SLICERASTRO_QTMODULES_ASTROVOLUME_WIDGETS_EXPORT qSlicerAstroTheme
  : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY themeChanged)
  Q_PROPERTY(bool dark READ isDark NOTIFY themeChanged)

public:
  /// The theme shared by all the panels: use instance() (the constructor is
  /// public for the Python wrapping only). In Python:
  /// slicer.app.property("SlicerAstroTheme")
  static qSlicerAstroTheme* instance();
  qSlicerAstroTheme(QObject* parent = nullptr);
  ~qSlicerAstroTheme() override;

  /// "dark", "light" or "system"
  QString mode() const;
  /// Resolved theme
  bool isDark() const;

  /// Colour of a token of the current theme (invalid colour if unknown)
  Q_INVOKABLE QColor color(const QString& token) const;
  /// Colour of a token in the dark or light theme
  Q_INVOKABLE QColor tokenColor(const QString& token, bool dark) const;

  /// Colours of new segments (masks), in order: the theme hues (amber, coral,
  /// violet, green, pink, sky, orange, cyan), clear of the colour maps of the
  /// data and the same in the dark and light themes (they are saved with the
  /// segments)
  Q_INVOKABLE QList<QColor> segmentColors() const;
  /// The colour of a new segment of a segmentation: the first colour of
  /// segmentColors() that no segment has, after the last one the cycle again
  static QColor nextSegmentColor(vtkSegmentation* segmentation);

  /// Style sheet of the current theme
  Q_INVOKABLE QString styleSheet() const;
  QPalette palette() const;

  /// Fonts: "body" (13), "label" (12), "overline" (11 bold, spaced),
  /// "section" (15 semibold), "title" (22 semibold), "mono" (13),
  /// "monoSmall" (11.5)
  Q_INVOKABLE QFont font(const QString& role) const;
  static QString bodyFamily();
  static QString monoFamily();

  /// Icon of the SlicerAstro set (name of the SVG file without extension,
  /// e.g. "run", "module-moment-maps") drawn in the colour of a token
  Q_INVOKABLE QIcon icon(const QString& name, const QString& colorToken = QString("muted")) const;
  /// SVG of an icon with its stroke colour
  QByteArray iconSvg(const QString& name, const QColor& color) const;

  /// Style and palette of the theme for a panel and its children; the
  /// widget is restyled when the theme changes
  Q_INVOKABLE void applyTo(QWidget* widget);
  /// The theme for the whole application (SlicerAstroApp)
  Q_INVOKABLE void applyToApplication();
  /// The theme on every menu and dialog shown (e.g. Slicer's view context
  /// menus, file dialogs) while on: set by the Workbench while it is active
  /// (the menus and dialogs get Slicer's look back when it is turned off),
  /// and for the whole application by SlicerAstroApp
  Q_INVOKABLE void setPopupsThemed(bool themed);
  Q_INVOKABLE bool popupsThemed() const;
  /// The theme on a dialog, with the icons of the theme on its buttons
  /// (OK, Cancel, Yes, No, Replace, Merge, Delete, Save, Open...) and on the
  /// tool buttons of a file dialog
  Q_INVOKABLE void applyToDialog(QWidget* dialog);
  /// Slicer's look back on a widget themed with applyTo (style sheet,
  /// palette and font of the application); it no longer follows the theme
  Q_INVOKABLE void removeFrom(QWidget* widget);
  /// Slicer's "Loading data" progress windows (shown after a delay by their
  /// own timer) hidden and stopped while a question is asked during a load;
  /// returns the visible ones, to show again with resumeProgressDialogs
  static QList<QWidget*> suspendProgressDialogs(QWidget* except = nullptr);
  static void resumeProgressDialogs(const QList<QWidget*>& dialogs);

  /// A label with a text role: "overline", "label", "unit", "mono",
  /// "section", "title" (font and colour of the role)
  Q_INVOKABLE static void setLabelRole(QLabel* label, const QString& role);

  /// A unit of a FITS header as it is written in text (e.g. JY/BEAM ->
  /// Jy/beam, KM/S -> km/s)
  Q_INVOKABLE static QString displayUnit(const QString& unit);
  /// A value of the data with its unit, 3 significant digits (small Jy
  /// values in mJy, e.g. 0.00052 JY/BEAM -> 0.52 mJy/beam)
  Q_INVOKABLE static QString formatFlux(double value, const QString& unit);

  /// About `count` round values (1, 2 or 5 times a power of ten) in
  /// [minimum, maximum], for the ticks of plots
  static QVector<double> niceTicks(double minimum, double maximum, int count);
  /// A tick label: 4 significant digits, typographic minus
  static QString tickLabel(double value);

public slots:
  void setMode(const QString& mode);

signals:
  void themeChanged();

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  /// qSlicerAstroScrollAreaFitter on the scroll area showing the widget
  /// (false if the widget is in no scroll area)
  static bool fitScrollAreaOf(QWidget* widget);

  QScopedPointer<qSlicerAstroThemePrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerAstroTheme);
  Q_DISABLE_COPY(qSlicerAstroTheme);
};

#endif
