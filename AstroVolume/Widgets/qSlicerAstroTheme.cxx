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

// Qt includes
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QLabel>
#include <QList>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QStyle>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QVariant>
#include <QWidget>

#include "qSlicerAstroTheme.h"

namespace
{
//----------------------------------------------------------------------------
struct Token
{
  const char* Name;
  const char* Dark;
  const char* Light;
};

// Tokens of the design ("Observatory" dark, "Daylight" light). The view
// tokens are the same in both themes: the data stay on a dark background.
const Token Tokens[] =
{
  {"bg", "#0B0F1A", "#EEF1F6"},
  {"panel", "#111726", "#FFFFFF"},
  {"raised", "#182033", "#F5F7FA"},
  {"hover", "#202A42", "#E8ECF3"},
  {"line", "#26304A", "#D5DBE6"},
  {"line-strong", "#34405F", "#B9C2D3"},
  {"text", "#E7EBF3", "#121826"},
  {"muted", "#A9B3C8", "#3F4A5E"},
  {"subtle", "#8592AC", "#5F6B80"},
  {"accent", "#F2B24C", "#F2B24C"},
  {"accent-ink", "#F2B24C", "#9A5B00"},
  {"accent-hover", "#F6C26C", "#F6C26C"},
  {"accent-pressed", "#D99B38", "#D99B38"},
  {"on-accent", "#1B1306", "#1B1306"},
  {"accent-soft", "#24F2B24C", "#33F2B24C"},
  {"accent-soft-strong", "#47F2B24C", "#59F2B24C"},
  {"data", "#4FD1E0", "#0B7C8C"},
  {"data-soft", "#244FD1E0", "#1A0B7C8C"},
  {"data-soft-strong", "#404FD1E0", "#330B7C8C"},
  {"ok", "#5BD08A", "#1E8A4C"},
  {"warn", "#FF9F43", "#B85C00"},
  {"warn-soft", "#1AFF9F43", "#14B85C00"},
  {"err", "#FF6B6B", "#C62F2F"},
  {"err-soft", "#1AFF6B6B", "#14C62F2F"},
  {"view", "#070A12", "#070A12"},
  {"view-text", "#E7EBF3", "#E7EBF3"},
  {"view-subtle", "#8592AC", "#8592AC"},
  {"view-data", "#4FD1E0", "#4FD1E0"},
  {"view-line", "#26304A", "#26304A"},
};

const char* const SettingsKey = "SlicerAstro/Theme";

//----------------------------------------------------------------------------
/// Icon painted from the SVG with the current colour of a token
class qSlicerAstroIconEngine : public QIconEngine
{
public:
  qSlicerAstroIconEngine(const QString& name, const QString& token)
    : Name(name), Token(token)
    {
    }

  void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override
    {
    Q_UNUSED(state);
    qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
    QColor color = theme->color(this->Token);
    if (!color.isValid())
      {
      color = theme->color("muted");
      }
    if (mode == QIcon::Disabled)
      {
      color = theme->color("subtle");
      color.setAlphaF(0.55);
      }
    else if ((mode == QIcon::Active || mode == QIcon::Selected) && this->Token == "muted")
      {
      color = theme->color("text");
      }
    QSvgRenderer renderer(theme->iconSvg(this->Name, color));
    if (renderer.isValid())
      {
      renderer.render(painter, QRectF(rect));
      }
    }

  QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override
    {
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    this->paint(&painter, QRect(QPoint(0, 0), size), mode, state);
    painter.end();
    return QPixmap::fromImage(image);
    }

  QIconEngine* clone() const override
    {
    return new qSlicerAstroIconEngine(this->Name, this->Token);
    }

private:
  QString Name;
  QString Token;
};

} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroThemePrivate
{
public:
  QString Mode{"system"};
  bool AppliedToApplication{false};
  bool ApplyingToApplication{false};
  bool FontsRegistered{false};
  QString StyleSheetTemplate;
  QList<QPointer<QWidget> > Widgets;
  QTemporaryDir IconDir;
  QHash<QString, QString> IconFiles;
  QHash<QString, QByteArray> IconSources;

  void registerFonts();
  QString styleSheet(const qSlicerAstroTheme* theme);
  QString iconFile(const qSlicerAstroTheme* theme, const QString& name, const QString& token);
  void restyle(QWidget* widget, const qSlicerAstroTheme* theme);
};

//-----------------------------------------------------------------------------
void qSlicerAstroThemePrivate::registerFonts()
{
  if (this->FontsRegistered)
    {
    return;
    }
  this->FontsRegistered = true;
  const char* files[] =
    {
    "AtkinsonHyperlegibleNext-Regular.ttf", "AtkinsonHyperlegibleNext-Medium.ttf",
    "AtkinsonHyperlegibleNext-SemiBold.ttf", "AtkinsonHyperlegibleNext-Bold.ttf",
    "AtkinsonHyperlegibleMono-Regular.ttf", "AtkinsonHyperlegibleMono-Medium.ttf"
    };
  for (const char* file : files)
    {
    if (QFontDatabase::addApplicationFont(QString(":/AstroTheme/Fonts/%1").arg(file)) < 0)
      {
      qWarning() << "SlicerAstro theme: the font" << file << "could not be loaded";
      }
    }
}

//-----------------------------------------------------------------------------
QString qSlicerAstroThemePrivate::iconFile(const qSlicerAstroTheme* theme,
                                           const QString& name, const QString& token)
{
  const QColor color = theme->color(token);
  const QString key = name + "-" + color.name(QColor::HexArgb).mid(1);
  QHash<QString, QString>::const_iterator found = this->IconFiles.constFind(key);
  if (found != this->IconFiles.constEnd())
    {
    return found.value();
    }
  if (!this->IconDir.isValid())
    {
    return QString();
    }
  const QString path = QDir(this->IconDir.path()).filePath(key + ".svg");
  QFile file(path);
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
    file.write(theme->iconSvg(name, color));
    file.close();
    }
  this->IconFiles.insert(key, path);
  return path;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroThemePrivate::styleSheet(const qSlicerAstroTheme* theme)
{
  if (this->StyleSheetTemplate.isEmpty())
    {
    QFile file(":/AstroTheme/Theme/astro.qss");
    if (file.open(QIODevice::ReadOnly))
      {
      this->StyleSheetTemplate = QString::fromUtf8(file.readAll());
      }
    }
  QString sheet = this->StyleSheetTemplate;
  // comments are not needed in the applied sheet
  sheet.remove(QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption));
  // icons in a colour: @icon:name:token@
  QRegularExpression iconExpression("@icon:([A-Za-z0-9-]+):([a-z-]+)@");
  QRegularExpressionMatch match = iconExpression.match(sheet);
  while (match.hasMatch())
    {
    QString path = this->iconFile(theme, match.captured(1), match.captured(2));
    path.replace('\\', '/');
    sheet.replace(match.capturedStart(), match.capturedLength(), path);
    match = iconExpression.match(sheet);
    }
  sheet.replace("@body-family@", qSlicerAstroTheme::bodyFamily());
  sheet.replace("@mono-family@", qSlicerAstroTheme::monoFamily());
  for (const Token& token : Tokens)
    {
    sheet.replace(QString("@%1@").arg(token.Name), theme->color(token.Name).name(QColor::HexArgb));
    }
  return sheet;
}

//-----------------------------------------------------------------------------
void qSlicerAstroThemePrivate::restyle(QWidget* widget, const qSlicerAstroTheme* theme)
{
  if (!widget)
    {
    return;
    }
  widget->setAttribute(Qt::WA_StyledBackground, true);
  widget->setProperty("astroPanel", true);
  widget->setPalette(theme->palette());
  widget->setFont(theme->font("body"));
  widget->setStyleSheet(this->styleSheet(theme));
  widget->update();
}

//-----------------------------------------------------------------------------
qSlicerAstroTheme* qSlicerAstroTheme::instance()
{
  static QPointer<qSlicerAstroTheme> theme;
  if (!theme)
    {
    theme = new qSlicerAstroTheme(qApp);
    // for Python: slicer.app.property("SlicerAstroTheme")
    if (qApp)
      {
      qApp->setProperty("SlicerAstroTheme", QVariant::fromValue(static_cast<QObject*>(theme.data())));
      }
    }
  return theme;
}

//-----------------------------------------------------------------------------
qSlicerAstroTheme::qSlicerAstroTheme(QObject* parent)
  : QObject(parent)
  , d_ptr(new qSlicerAstroThemePrivate)
{
  Q_D(qSlicerAstroTheme);
  QSettings settings;
  const QString mode = settings.value(SettingsKey, QString("system")).toString();
  if (mode == "dark" || mode == "light" || mode == "system")
    {
    d->Mode = mode;
    }
  d->registerFonts();
  if (qApp)
    {
    qApp->installEventFilter(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
                     [this]()
      {
      if (this->mode() == "system")
        {
        this->setMode("system");
        }
      });
#endif
    }
}

//-----------------------------------------------------------------------------
qSlicerAstroTheme::~qSlicerAstroTheme()
{
}

//-----------------------------------------------------------------------------
bool qSlicerAstroTheme::eventFilter(QObject* watched, QEvent* event)
{
  Q_D(qSlicerAstroTheme);
  // Slicer changes its colours (e.g. its Dark/Light style): "system" follows
  if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange &&
      d->Mode == "system" && !d->ApplyingToApplication && !d->AppliedToApplication)
    {
    this->setMode("system");
    }
  return QObject::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------
QString qSlicerAstroTheme::mode() const
{
  Q_D(const qSlicerAstroTheme);
  return d->Mode;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroTheme::isDark() const
{
  Q_D(const qSlicerAstroTheme);
  if (d->Mode == "dark")
    {
    return true;
    }
  if (d->Mode == "light")
    {
    return false;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
  if (d->AppliedToApplication && scheme != Qt::ColorScheme::Unknown)
    {
    return scheme == Qt::ColorScheme::Dark;
    }
#endif
  if (d->AppliedToApplication || !qApp)
    {
    return true;
    }
  // the extension follows the colours of Slicer
  return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

//-----------------------------------------------------------------------------
QColor qSlicerAstroTheme::tokenColor(const QString& token, bool dark) const
{
  for (const Token& entry : Tokens)
    {
    if (token == QLatin1String(entry.Name))
      {
      return QColor(QString::fromLatin1(dark ? entry.Dark : entry.Light));
      }
    }
  return QColor();
}

//-----------------------------------------------------------------------------
QColor qSlicerAstroTheme::color(const QString& token) const
{
  return this->tokenColor(token, this->isDark());
}

//-----------------------------------------------------------------------------
QString qSlicerAstroTheme::styleSheet() const
{
  Q_D(const qSlicerAstroTheme);
  return const_cast<qSlicerAstroThemePrivate*>(d)->styleSheet(this);
}

//-----------------------------------------------------------------------------
QPalette qSlicerAstroTheme::palette() const
{
  QPalette palette;
  const QColor text = this->color("text");
  const QColor subtle = this->color("subtle");
  palette.setColor(QPalette::Window, this->color("panel"));
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Base, this->color("raised"));
  palette.setColor(QPalette::AlternateBase, this->color("bg"));
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::Button, this->color("raised"));
  palette.setColor(QPalette::ButtonText, text);
  palette.setColor(QPalette::BrightText, this->color("accent"));
  palette.setColor(QPalette::Highlight, this->color("data"));
  palette.setColor(QPalette::HighlightedText, this->color("bg"));
  palette.setColor(QPalette::ToolTipBase, this->color("raised"));
  palette.setColor(QPalette::ToolTipText, text);
  palette.setColor(QPalette::PlaceholderText, subtle);
  palette.setColor(QPalette::Link, this->color("data"));
  palette.setColor(QPalette::LinkVisited, this->color("data"));
  palette.setColor(QPalette::Light, this->color("hover"));
  palette.setColor(QPalette::Midlight, this->color("line-strong"));
  palette.setColor(QPalette::Mid, this->color("line"));
  palette.setColor(QPalette::Dark, this->color("bg"));
  palette.setColor(QPalette::Shadow, this->isDark() ? QColor("#000000") : this->color("line-strong"));
  palette.setColor(QPalette::Disabled, QPalette::WindowText, subtle);
  palette.setColor(QPalette::Disabled, QPalette::Text, subtle);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, subtle);
  palette.setColor(QPalette::Disabled, QPalette::Base, this->color("panel"));
  palette.setColor(QPalette::Disabled, QPalette::Button, this->color("panel"));
  return palette;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroTheme::bodyFamily()
{
  return QString("Atkinson Hyperlegible Next");
}

//-----------------------------------------------------------------------------
QString qSlicerAstroTheme::monoFamily()
{
  return QString("Atkinson Hyperlegible Mono");
}

//-----------------------------------------------------------------------------
QFont qSlicerAstroTheme::font(const QString& role) const
{
  const bool mono = role.startsWith("mono");
  QFont font(mono ? monoFamily() : bodyFamily());
  font.setStyleHint(mono ? QFont::Monospace : QFont::SansSerif);
  int size = 13;
  QFont::Weight weight = QFont::Normal;
  if (role == "label")
    {
    size = 12;
    }
  else if (role == "overline")
    {
    size = 11;
    weight = QFont::Bold;
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.9);
    }
  else if (role == "section")
    {
    size = 15;
    weight = QFont::DemiBold;
    }
  else if (role == "title")
    {
    size = 22;
    weight = QFont::DemiBold;
    }
  else if (role == "monoSmall")
    {
    size = 11;
    }
  font.setPixelSize(size);
  font.setWeight(weight);
  return font;
}

//-----------------------------------------------------------------------------
QByteArray qSlicerAstroTheme::iconSvg(const QString& name, const QColor& color) const
{
  Q_D(const qSlicerAstroTheme);
  qSlicerAstroThemePrivate* p = const_cast<qSlicerAstroThemePrivate*>(d);
  QHash<QString, QByteArray>::const_iterator found = p->IconSources.constFind(name);
  QByteArray source;
  if (found != p->IconSources.constEnd())
    {
    source = found.value();
    }
  else
    {
    QFile file(QString(":/AstroTheme/Icons/%1.svg").arg(name));
    if (file.open(QIODevice::ReadOnly))
      {
      source = file.readAll();
      }
    p->IconSources.insert(name, source);
    }
  QByteArray svg = source;
  QColor opaque = color;
  opaque.setAlpha(255);
  svg.replace("currentColor", opaque.name(QColor::HexRgb).toLatin1());
  if (color.alpha() < 255)
    {
    svg.replace("<svg ", QString("<svg opacity=\"%1\" ").arg(color.alphaF()).toLatin1());
    }
  return svg;
}

//-----------------------------------------------------------------------------
QIcon qSlicerAstroTheme::icon(const QString& name, const QString& colorToken) const
{
  return QIcon(new qSlicerAstroIconEngine(name, colorToken.isEmpty() ? QString("muted") : colorToken));
}

//-----------------------------------------------------------------------------
void qSlicerAstroTheme::applyTo(QWidget* widget)
{
  Q_D(qSlicerAstroTheme);
  if (!widget)
    {
    return;
    }
  if (!d->Widgets.contains(widget))
    {
    d->Widgets.append(widget);
    }
  d->restyle(widget, this);
}

//-----------------------------------------------------------------------------
void qSlicerAstroTheme::applyToApplication()
{
  Q_D(qSlicerAstroTheme);
  if (!qApp)
    {
    return;
    }
  d->AppliedToApplication = true;
  d->ApplyingToApplication = true;
  QApplication::setPalette(this->palette());
  QApplication::setFont(this->font("body"));
  qApp->setStyleSheet(d->styleSheet(this));
  d->ApplyingToApplication = false;
}

//-----------------------------------------------------------------------------
void qSlicerAstroTheme::setMode(const QString& mode)
{
  Q_D(qSlicerAstroTheme);
  if (mode != "dark" && mode != "light" && mode != "system")
    {
    qWarning() << "SlicerAstro theme: unknown mode" << mode << "(dark, light or system)";
    return;
    }
  if (mode != d->Mode)
    {
    d->Mode = mode;
    QSettings settings;
    settings.setValue(SettingsKey, mode);
    }
  for (int i = d->Widgets.size() - 1; i >= 0; i--)
    {
    if (!d->Widgets[i])
      {
      d->Widgets.removeAt(i);
      continue;
      }
    d->restyle(d->Widgets[i], this);
    }
  if (d->AppliedToApplication)
    {
    this->applyToApplication();
    }
  emit this->themeChanged();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroTheme::displayUnit(const QString& unit)
{
  QString text = unit;
  text.replace(QRegularExpression("\\bJY\\b", QRegularExpression::CaseInsensitiveOption), "Jy");
  text.replace(QRegularExpression("\\bBEAM\\b", QRegularExpression::CaseInsensitiveOption), "beam");
  text.replace(QRegularExpression("\\bKM/S\\b", QRegularExpression::CaseInsensitiveOption), "km/s");
  return text;
}

//-----------------------------------------------------------------------------
void qSlicerAstroTheme::setLabelRole(QLabel* label, const QString& role)
{
  if (!label)
    {
    return;
    }
  label->setProperty("astroRole", role);
  if (role == "overline")
    {
    QFont font = label->font();
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.9);
    label->setFont(font);
    label->setText(label->text().toUpper());
    }
  label->style()->unpolish(label);
  label->style()->polish(label);
}
