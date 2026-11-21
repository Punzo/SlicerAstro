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
#include <QScrollArea>
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QAbstractButton>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPointer>
#include <QRegularExpression>

#include <cmath>
#include <QSettings>
#include <QStyle>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QVariant>
#include <QWidget>

#include "qSlicerAstroScrollAreaFitter.h"
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
// tokens (bars, slice backgrounds, plots of the views) follow the theme too.
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
  {"view", "#070A12", "#E6EAF1"},
  {"view-text", "#E7EBF3", "#121826"},
  {"view-subtle", "#8592AC", "#5F6B80"},
  {"view-data", "#4FD1E0", "#0B7C8C"},
  {"view-line", "#26304A", "#C9D0DC"},
  {"view-hover", "#202A42", "#D5DBE6"},
  {"view-groove", "#34405F", "#B9C2D3"},
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
namespace
{
//-----------------------------------------------------------------------------
/// Icons of the files and folders of file dialogs: the icons of the theme
/// (follow the light and dark modes) instead of the icons of the system
class qSlicerAstroFileIconProvider : public QFileIconProvider
{
public:
  explicit qSlicerAstroFileIconProvider(const qSlicerAstroTheme* theme) : Theme(theme) {}

  QIcon icon(IconType type) const override
  {
    switch (type)
      {
      case QFileIconProvider::Computer:
      case QFileIconProvider::Desktop:
        return this->Theme->icon("monitor", "muted");
      case QFileIconProvider::Trashcan:
        return this->Theme->icon("trash", "muted");
      case QFileIconProvider::Network:
        return this->Theme->icon("globe", "muted");
      case QFileIconProvider::Drive:
        return this->Theme->icon("drive", "muted");
      case QFileIconProvider::Folder:
        return this->Theme->icon("folder", "accent-ink");
      default:
        return this->Theme->icon("file", "muted");
      }
  }

  QIcon icon(const QFileInfo& info) const override
  {
    if (info.isRoot())
      {
      return this->icon(QFileIconProvider::Drive);
      }
    if (info.isDir())
      {
      return info.absoluteFilePath() == QDir::homePath() ? this->Theme->icon("home", "accent-ink") :
                                                           this->Theme->icon("folder", "accent-ink");
      }
    // astronomical data (FITS, also compressed) stand out
    const QString name = info.fileName().toLower();
    const char* suffixes[] = {".fits", ".fit", ".fts", ".fz", ".fits.gz", ".fit.gz", ".fts.gz"};
    for (const char* suffix : suffixes)
      {
      if (name.endsWith(QLatin1String(suffix)))
        {
        return this->Theme->icon("fileData", "text");
        }
      }
    return this->Theme->icon("file", "muted");
  }

private:
  const qSlicerAstroTheme* Theme;
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
  bool PopupsThemed{false};
  /// menus and dialogs themed while popups are themed
  QList<QPointer<QWidget> > Popups;
  QTemporaryDir IconDir;
  QHash<QString, QString> IconFiles;
  QHash<QString, QByteArray> IconSources;
  /// Icons of the files and folders of themed file dialogs
  QScopedPointer<QFileIconProvider> FileIconProvider;

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
  QWidget* widget = qobject_cast<QWidget*>(watched);
  // menus and dialogs (also Slicer's) while popups are themed
  if (d->PopupsThemed && widget && event->type() == QEvent::Show && widget->isWindow() &&
      (qobject_cast<QMenu*>(widget) || qobject_cast<QDialog*>(widget)) && !d->Popups.contains(widget))
    {
    d->Popups.append(widget);
    if (qobject_cast<QMenu*>(widget))
      {
      d->restyle(widget, this);
      widget->resize(widget->sizeHint());
      }
    else
      {
      this->applyToDialog(widget);
      }
    }
  // a themed panel shown in a scroll area
  if (widget && watched != qApp && event->type() == QEvent::Show && qSlicerAstroTheme::fitScrollAreaOf(widget))
    {
    widget->removeEventFilter(this);
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
  // the scroll area showing the panel (e.g. the module panel of Slicer) is
  // kept wide enough for the panel and its vertical scroll bar; the panel may
  // be placed in it later (shown)
  if (!qSlicerAstroTheme::fitScrollAreaOf(widget))
    {
    widget->installEventFilter(this);
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroTheme::fitScrollAreaOf(QWidget* widget)
{
  for (QWidget* parent = widget ? widget->parentWidget() : nullptr; parent; parent = parent->parentWidget())
    {
    QScrollArea* scrollArea = qobject_cast<QScrollArea*>(parent);
    if (scrollArea)
      {
      qSlicerAstroScrollAreaFitter::install(scrollArea);
      return true;
      }
    }
  return false;
}

//-----------------------------------------------------------------------------
void qSlicerAstroTheme::setPopupsThemed(bool themed)
{
  Q_D(qSlicerAstroTheme);
  if (themed == d->PopupsThemed)
    {
    return;
    }
  d->PopupsThemed = themed;
  if (!themed && !d->AppliedToApplication)
    {
    // Slicer's look back on the menus and dialogs that are kept
    const QList<QPointer<QWidget> > popups = d->Popups;
    d->Popups.clear();
    for (const QPointer<QWidget>& popup : popups)
      {
      this->removeFrom(popup);
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroTheme::removeFrom(QWidget* widget)
{
  Q_D(qSlicerAstroTheme);
  if (!widget)
    {
    return;
    }
  d->Widgets.removeAll(widget);
  widget->setProperty("astroPanel", QVariant());
  widget->setStyleSheet(QString());
  widget->setPalette(QApplication::palette());
  widget->setFont(QApplication::font());
  widget->update();
}

//-----------------------------------------------------------------------------
QList<QWidget*> qSlicerAstroTheme::suspendProgressDialogs(QWidget* except)
{
  QList<QWidget*> visible;
  for (QWidget* widget : QApplication::topLevelWidgets())
    {
    if (widget == except)
      {
      continue;
      }
    QProgressDialog* progressDialog = qobject_cast<QProgressDialog*>(widget);
    const bool shown = widget->isVisible();
    if (progressDialog)
      {
      // reset: its timer would show it during the question
      progressDialog->reset();
      progressDialog->hide();
      }
    else if (shown && qobject_cast<QDialog*>(widget) && widget->findChild<QProgressBar*>())
      {
      widget->hide();
      }
    else
      {
      continue;
      }
    if (shown)
      {
      visible << widget;
      }
    }
  return visible;
}

//-----------------------------------------------------------------------------
void qSlicerAstroTheme::resumeProgressDialogs(const QList<QWidget*>& dialogs)
{
  for (QWidget* widget : dialogs)
    {
    if (widget)
      {
      widget->show();
      }
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroTheme::popupsThemed() const
{
  Q_D(const qSlicerAstroTheme);
  return d->PopupsThemed;
}

//-----------------------------------------------------------------------------
void qSlicerAstroTheme::applyToDialog(QWidget* dialog)
{
  Q_D(qSlicerAstroTheme);
  if (!dialog)
    {
    return;
    }
  this->applyTo(dialog);
  // the buttons of the dialog: an icon of the theme by text, else by role
  struct TextIcon
    {
    const char* Text;
    const char* Icon;
    };
  const TextIcon textIcons[] = {{"replace", "reverse"}, {"merge", "layers"}, {"delete", "trash"},
                                {"remove", "trash"}, {"save", "download"}, {"export", "download"},
                                {"open", "folder"}, {"load", "folder"}, {"import", "folder"},
                                {"reset", "reset"}, {"help", "help"}, {"retry", "reset"}};
  for (QDialogButtonBox* box : dialog->findChildren<QDialogButtonBox*>())
    {
    for (QAbstractButton* button : box->buttons())
      {
      const QString text = button->text().remove('&').toLower();
      QString iconName;
      for (const TextIcon& textIcon : textIcons)
        {
        if (text.contains(QLatin1String(textIcon.Text)))
          {
          iconName = textIcon.Icon;
          break;
          }
        }
      if (iconName.isEmpty())
        {
        const QDialogButtonBox::ButtonRole role = box->buttonRole(button);
        if (role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::YesRole ||
            role == QDialogButtonBox::ApplyRole)
          {
          iconName = "check";
          }
        else if (role == QDialogButtonBox::RejectRole || role == QDialogButtonBox::NoRole)
          {
          iconName = "close";
          }
        else if (role == QDialogButtonBox::DestructiveRole)
          {
          iconName = "trash";
          }
        else if (role == QDialogButtonBox::ResetRole)
          {
          iconName = "reset";
          }
        else if (role == QDialogButtonBox::HelpRole)
          {
          iconName = "help";
          }
        }
      button->setIcon(iconName.isEmpty() ? QIcon() : this->icon(iconName, "text"));
      button->setIconSize(QSize(15, 15));
      }
    }
  // the other push buttons of Slicer's dialogs (e.g. Reset of "Add data")
  for (QPushButton* button : dialog->findChildren<QPushButton*>())
    {
    if (!qobject_cast<QDialogButtonBox*>(button->parentWidget()) && !button->icon().isNull())
      {
      const QString text = button->text().remove('&').toLower();
      button->setIcon(text.contains("reset") ? this->icon("reset", "text") : QIcon());
      }
    }
  // the tool buttons of a file dialog (Qt's names)
  struct NamedIcon
    {
    const char* Name;
    const char* Icon;
    };
  const NamedIcon fileDialogIcons[] = {{"backButton", "chevronLeft"}, {"forwardButton", "chevronRight"},
                                       {"toParentButton", "arrowUp"}, {"newFolderButton", "folderPlus"},
                                       {"listModeButton", "list"}, {"detailModeButton", "table"}};
  // (a named list: a range-for on a temporary built with << would dangle)
  QList<QFileDialog*> fileDialogs = dialog->findChildren<QFileDialog*>();
  fileDialogs.prepend(qobject_cast<QFileDialog*>(dialog));
  for (QFileDialog* fileDialog : fileDialogs)
    {
    if (!fileDialog)
      {
      continue;
      }
    // the files and folders (list, sidebar, "Look in")
    if (!d->FileIconProvider)
      {
      d->FileIconProvider.reset(new qSlicerAstroFileIconProvider(this));
      }
    fileDialog->setIconProvider(d->FileIconProvider.data());
    for (const NamedIcon& namedIcon : fileDialogIcons)
      {
      QToolButton* button = fileDialog->findChild<QToolButton*>(namedIcon.Name);
      if (button)
        {
        button->setIcon(this->icon(namedIcon.Icon, "muted"));
        button->setAutoRaise(true);
        }
      }
    }
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
QVector<double> qSlicerAstroTheme::niceTicks(double minimum, double maximum, int count)
{
  QVector<double> ticks;
  const double span = maximum - minimum;
  if (!(span > 0.) || !std::isfinite(span) || count < 1)
    {
    return ticks;
    }
  const double rough = span / count;
  const double magnitude = std::pow(10., std::floor(std::log10(rough)));
  const double fraction = rough / magnitude;
  const double step = (fraction < 1.5 ? 1. : fraction < 3. ? 2. : fraction < 7. ? 5. : 10.) * magnitude;
  for (double tick = std::ceil(minimum / step) * step; tick <= maximum + 1e-9 * step; tick += step)
    {
    ticks.append(std::abs(tick) < 1e-9 * step ? 0. : tick);
    }
  return ticks;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroTheme::tickLabel(double value)
{
  return QString::number(value, 'g', 4).replace('-', QChar(0x2212));
}

//-----------------------------------------------------------------------------
QString qSlicerAstroTheme::formatFlux(double value, const QString& unit)
{
  const QString text = qSlicerAstroTheme::displayUnit(unit);
  if (text.startsWith("Jy") && std::abs(value) < 0.1 && value != 0.)
    {
    return QString("%1 m%2").arg(value * 1000., 0, 'g', 3).arg(text);
    }
  return QString("%1 %2").arg(value, 0, 'g', 3).arg(text).trimmed();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroTheme::displayUnit(const QString& unit)
{
  QString text = unit.trimmed();
  if (text.compare("UNDEFINED", Qt::CaseInsensitive) == 0)
    {
    return QString();
    }
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
