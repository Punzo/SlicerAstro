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

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

// Qt includes
#include <QAction>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

// CTK includes
#include <ctkSettingsDialog.h>

// Slicer includes
#include "vtkSlicerConfigure.h"
#include "qSlicerApplication.h"
#include "qSlicerIO.h"
#include "qSlicerIOManager.h"
#include "qSlicerLayoutManager.h"
#include "qSlicerModuleManager.h"
#ifdef Slicer_USE_PYTHONQT
#include "qSlicerPythonManager.h"
#endif

// AstroVolume includes
#include "qSlicerAstroSegmentedButton.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroWelcomeWidget.h"

// STD includes
#include <algorithm>
#include <functional>

namespace
{
//-----------------------------------------------------------------------------
QString Tr(const char* text)
{
  return qSlicerAstroWelcomeWidget::tr(text);
}

//-----------------------------------------------------------------------------
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}

//-----------------------------------------------------------------------------
/// The image scaled into width x height (aspect kept) with rounded corners
QPixmap RoundedThumbnail(const QString& path, int width, int height, double radius, double ratio)
{
  QPixmap source(path);
  if (source.isNull())
    {
    return QPixmap();
    }
  const QPixmap scaled = source.scaled(static_cast<int>(width * ratio), static_cast<int>(height * ratio),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QPixmap rounded(scaled.size());
  rounded.fill(Qt::transparent);
  QPainter painter(&rounded);
  painter.setRenderHint(QPainter::Antialiasing);
  QPainterPath clip;
  clip.addRoundedRect(QRectF(rounded.rect()), radius * ratio, radius * ratio);
  painter.setClipPath(clip);
  painter.drawPixmap(0, 0, scaled);
  painter.end();
  rounded.setDevicePixelRatio(ratio);
  return rounded;
}

//-----------------------------------------------------------------------------
QVariant RunPython(const QString& code, bool evaluate)
{
#ifdef Slicer_USE_PYTHONQT
  qSlicerApplication* app = qSlicerApplication::application();
  if (app && app->pythonManager())
    {
    return app->pythonManager()->executeString(code, evaluate ? ctkAbstractPythonManager::EvalInput :
                                                                ctkAbstractPythonManager::FileInput);
    }
#else
  Q_UNUSED(code);
  Q_UNUSED(evaluate);
#endif
  return QVariant();
}

//-----------------------------------------------------------------------------
/// Remove the items of a layout without deleting their widgets
void ClearLayout(QLayout* layout)
{
  while (QLayoutItem* item = layout->takeAt(0))
    {
    delete item;
    }
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
/// A recently loaded file (File > Recent) or a recently saved or opened
/// workspace
struct RecentEntry
{
  bool Workspace{false};
  QString Name;
  QString Path;     ///< file, or folder of the workspace
  QString Preview;  ///< picture of the views of a workspace
  QString Detail;
  QString Time;     ///< ISO date and time (empty: unknown)
};

//-----------------------------------------------------------------------------
const char* LoadTimesKey = "SlicerAstro/Welcome/LoadTimes";
/// workspaces saved or opened before are not in the history any more
const char* HistoryClearedKey = "SlicerAstro/Welcome/HistoryCleared";

//-----------------------------------------------------------------------------
class qSlicerAstroWelcomeWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroWelcomeWidget);
protected:
  qSlicerAstroWelcomeWidget* const q_ptr;
public:
  qSlicerAstroWelcomeWidgetPrivate(qSlicerAstroWelcomeWidget& object);
  void init();
  QFrame* addCard(const QString& objectName, const QString& iconName, const QString& title, const QString& text,
                  const QString& toolTip, const std::function<void()>& action);
  void makeClickable(QFrame* card, const std::function<void()>& action);
  void arrange();
  void layoutSamples();
  void updateSamples();
  void updateRecent();
  void updateBackground();
  QMenu* recentMenu() const;
  QList<QAction*> recentActions() const;

  bool Wide{false};
  bool WorkbenchMode{false};
  QPointer<QWidget> OverlayTarget;
  QHash<QObject*, std::function<void()> > Actions;

  QVBoxLayout* RootLayout{nullptr};
  QScrollArea* ScrollArea{nullptr};
  QWidget* Content{nullptr};
  QVBoxLayout* ContentLayout{nullptr};
  QWidget* Top{nullptr};
  QHBoxLayout* TopLayout{nullptr};
  QWidget* Hero{nullptr};
  QLabel* Logo{nullptr};
  QLabel* Title{nullptr};
  QWidget* ThemeRow{nullptr};
  qSlicerAstroSegmentedButton* ThemeButton{nullptr};
  QWidget* Cards{nullptr};
  QGridLayout* CardsLayout{nullptr};
  QFrame* OpenDataCard{nullptr};
  QFrame* WorkbenchCard{nullptr};
  QFrame* WorkspacesCard{nullptr};
  QFrame* SkyCard{nullptr};
  QFrame* LearnCard{nullptr};
  QWidget* Columns{nullptr};
  QHBoxLayout* ColumnsLayout{nullptr};
  QWidget* LeftColumn{nullptr};
  QVBoxLayout* LeftColumnLayout{nullptr};
  QWidget* Recent{nullptr};
  QVBoxLayout* RecentLayout{nullptr};
  QLabel* RecentEmpty{nullptr};
  QPushButton* ClearRecentButton{nullptr};
  QWidget* SamplesSection{nullptr};
  QWidget* Samples{nullptr};
  QGridLayout* SamplesLayout{nullptr};
  QList<QFrame*> SampleCards;
  QList<QLabel*> SampleThumbnails;
  QStringList SampleNames;
  QStringList SampleThumbnailPaths;
  int SampleColumns{0};
  int SampleThumbnailWidth{0};
  QWidget* Footer{nullptr};
  QList<RecentEntry> RecentEntries;
};

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeWidgetPrivate::qSlicerAstroWelcomeWidgetPrivate(qSlicerAstroWelcomeWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidgetPrivate::makeClickable(QFrame* card, const std::function<void()>& action)
{
  Q_Q(qSlicerAstroWelcomeWidget);
  card->setProperty("astroRole", "actionCard");
  card->setCursor(Qt::PointingHandCursor);
  card->setFocusPolicy(Qt::StrongFocus);
  card->setAttribute(Qt::WA_Hover);
  for (QWidget* child : card->findChildren<QWidget*>())
    {
    child->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
  card->installEventFilter(q);
  this->Actions[card] = action;
}

//-----------------------------------------------------------------------------
QFrame* qSlicerAstroWelcomeWidgetPrivate::addCard(const QString& objectName, const QString& iconName,
  const QString& title, const QString& text, const QString& toolTip, const std::function<void()>& action)
{
  QFrame* card = new QFrame(this->Cards);
  card->setObjectName(objectName);
  card->setToolTip(toolTip);
  QVBoxLayout* layout = new QVBoxLayout(card);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(8);
  QLabel* icon = new QLabel(card);
  icon->setPixmap(qSlicerAstroTheme::instance()->icon(iconName, "accent").pixmap(24, 24));
  layout->addWidget(icon);
  QLabel* titleLabel = new QLabel(title, card);
  QFont font = titleLabel->font();
  font.setBold(true);
  titleLabel->setFont(font);
  layout->addWidget(titleLabel);
  QLabel* textLabel = new QLabel(text, card);
  textLabel->setProperty("astroRole", "label");
  textLabel->setWordWrap(true);
  layout->addWidget(textLabel);
  layout->addStretch(1);
  this->makeClickable(card, action);
  return card;
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidgetPrivate::init()
{
  Q_Q(qSlicerAstroWelcomeWidget);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  q->setObjectName("WelcomeWidget");
  this->RootLayout = new QVBoxLayout(q);
  this->RootLayout->setContentsMargins(0, 0, 0, 0);
  this->RootLayout->setSpacing(0);
  this->Content = new QWidget(q);
  this->Content->setObjectName("WelcomeContent");
  this->ContentLayout = new QVBoxLayout(this->Content);
  this->RootLayout->addWidget(this->Content);

  // wide: the hero and the theme on one row
  this->Top = new QWidget(this->Content);
  this->TopLayout = new QHBoxLayout(this->Top);
  this->TopLayout->setContentsMargins(0, 0, 0, 0);
  this->TopLayout->setSpacing(20);

  // --- what SlicerAstro is
  this->Hero = new QWidget(this->Content);
  QHBoxLayout* hero = new QHBoxLayout(this->Hero);
  hero->setContentsMargins(0, 0, 0, 0);
  hero->setSpacing(14);
  this->Logo = new QLabel(this->Hero);
  this->Logo->setObjectName("WelcomeLogo");
  this->Logo->setToolTip(Tr("SlicerAstro"));
  hero->addWidget(this->Logo, 0, Qt::AlignVCenter);
  QVBoxLayout* heroText = new QVBoxLayout();
  heroText->setSpacing(4);
  heroText->addStretch(1);
  this->Title = new QLabel(Tr("Welcome to SlicerAstro"), this->Hero);
  this->Title->setObjectName("WelcomeTitle");
  this->Title->setWordWrap(true);
  heroText->addWidget(this->Title);
  QLabel* subtitle = new QLabel(Tr("Visualise, analyse and model 3D spectral data cubes: HI, CO, optical IFU."),
                                this->Hero);
  subtitle->setObjectName("WelcomeSubtitle");
  subtitle->setProperty("astroRole", "label");
  subtitle->setWordWrap(true);
  heroText->addWidget(subtitle);
  heroText->addStretch(1);
  hero->addLayout(heroText, 1);

  // --- theme
  this->ThemeRow = new QWidget(this->Content);
  QHBoxLayout* themeRow = new QHBoxLayout(this->ThemeRow);
  themeRow->setContentsMargins(0, 0, 0, 0);
  themeRow->setSpacing(8);
  QLabel* themeLabel = new QLabel(Tr("Theme"), this->ThemeRow);
  themeLabel->setProperty("astroRole", "label");
  themeLabel->setToolTip(Tr("Colours of the SlicerAstro panels and of the AstroWorkbench."));
  themeRow->addWidget(themeLabel);
  this->ThemeButton = new qSlicerAstroSegmentedButton(this->ThemeRow);
  this->ThemeButton->setObjectName("ThemeButton");
  this->ThemeButton->setToolTip(themeLabel->toolTip());
  this->ThemeButton->addButton(Tr("Dark"), Tr("Observatory: dark, for long sessions with the data (default)."));
  this->ThemeButton->addButton(Tr("Light"), Tr("Daylight: light."));
  this->ThemeButton->addButton(Tr("System"), Tr("Dark or light as the operating system."));
  themeRow->addWidget(this->ThemeButton, 1);

  // --- quick actions
  this->Cards = new QWidget(this->Content);
  this->Cards->setObjectName("WelcomeCards");
  this->CardsLayout = new QGridLayout(this->Cards);
  this->CardsLayout->setContentsMargins(0, 0, 0, 0);
  this->CardsLayout->setSpacing(10);
  this->OpenDataCard = this->addCard("OpenDataCard", "layers", Tr("Open data"),
    Tr(".fits, .fits.gz and .fz; masks open as segmentations."),
    Tr("Open FITS cubes, images and masks, catalogues or a saved scene (Add data dialog)."),
    [q]() { q->openData(); });
  this->WorkbenchCard = this->addCard("WorkbenchCard", "module-workbench", Tr("Open the Workbench"),
    Tr("Datasets, views and the processing pipeline in one window."),
    Tr("The AstroWorkbench: a focused viewer for data cubes (Ctrl+Shift+W)."),
    [q]() { q->openWorkbench(); });
  this->WorkspacesCard = this->addCard("WorkspacesCard", "archive", Tr("Workspaces"),
    Tr("Continue where you left off: saved scenes with a picture of the views."),
    Tr("The saved workspaces (Spaces in the rail): open one, or save the current one."),
    [q]() { emit q->workspacesRequested(); });
  this->SkyCard = this->addCard("SkySearchCard", "globe", Tr("Search the sky"),
    Tr("Virtual Observatory archives: coming in a next version."),
    Tr("Find cubes and images in Virtual Observatory archives by object or position (not available yet)."),
    nullptr);
  this->SkyCard->setEnabled(false);
  this->SkyCard->setCursor(Qt::ArrowCursor);
  this->LearnCard = this->addCard("LearnCard", "help", Tr("Learn"),
    Tr("Guides for every module, methods, and how to cite."),
    Tr("The SlicerAstro documentation, in the web browser."),
    [q]() { q->openDocumentation(); });

  // --- recent files
  this->Recent = new QWidget(this->Content);
  this->Recent->setObjectName("RecentFiles");
  this->RecentLayout = new QVBoxLayout(this->Recent);
  this->RecentLayout->setContentsMargins(0, 0, 0, 0);
  this->RecentLayout->setSpacing(2);
  QHBoxLayout* recentTitle = new QHBoxLayout();
  recentTitle->addWidget(Overline(Tr("Recent"), this->Recent), 1);
  this->ClearRecentButton = new QPushButton(qSlicerAstroTheme::instance()->icon("clock", "muted"), Tr("Clear history"),
                                            this->Recent);
  this->ClearRecentButton->setObjectName("ClearRecentButton");
  this->ClearRecentButton->setProperty("astroRole", "ghost");
  this->ClearRecentButton->setToolTip(Tr("Clear the history of the recent files and workspaces: the list below and "
                                         "File > Recent are emptied. No file is deleted; the workspaces stay in Spaces."));
  QObject::connect(this->ClearRecentButton, SIGNAL(clicked()), q, SLOT(clearRecentFiles()));
  recentTitle->addWidget(this->ClearRecentButton);
  this->RecentLayout->addLayout(recentTitle);
  this->RecentEmpty = new QLabel(Tr("Files you open are listed here."), this->Recent);
  this->RecentEmpty->setProperty("astroRole", "label");
  this->RecentEmpty->setToolTip(Tr("The recently loaded files of Slicer (File > Recent)."));
  this->RecentLayout->addWidget(this->RecentEmpty);

  // --- sample data
  this->SamplesSection = new QWidget(this->Content);
  QVBoxLayout* samplesLayout = new QVBoxLayout(this->SamplesSection);
  samplesLayout->setContentsMargins(0, 0, 0, 0);
  samplesLayout->setSpacing(8);
  QHBoxLayout* samplesTitle = new QHBoxLayout();
  samplesTitle->addWidget(Overline(Tr("Sample data"), this->SamplesSection));
  QLabel* cached = new QLabel(Tr("downloaded once, then cached"), this->SamplesSection);
  cached->setProperty("astroRole", "label");
  cached->setToolTip(Tr("Public data sets hosted with SlicerAstro, with their references (About)."));
  samplesTitle->addWidget(cached, 1);
  samplesLayout->addLayout(samplesTitle);
  this->Samples = new QWidget(this->SamplesSection);
  this->Samples->setObjectName("Samples");
  this->Samples->installEventFilter(q);
  this->SamplesLayout = new QGridLayout(this->Samples);
  this->SamplesLayout->setContentsMargins(0, 0, 0, 0);
  this->SamplesLayout->setSpacing(10);
  samplesLayout->addWidget(this->Samples);
  samplesLayout->addStretch(1);

  // --- about and settings
  this->Footer = new QWidget(this->Content);
  QHBoxLayout* footer = new QHBoxLayout(this->Footer);
  footer->setContentsMargins(0, 0, 0, 0);
  footer->setSpacing(8);
  QPushButton* about = new QPushButton(theme->icon("info", "text"), Tr("About SlicerAstro"), this->Footer);
  about->setObjectName("AboutButton");
  about->setToolTip(Tr("Versions, shortcuts, credits and how to cite SlicerAstro (F1)."));
  QObject::connect(about, SIGNAL(clicked()), q, SLOT(showAbout()));
  QPushButton* settings = new QPushButton(theme->icon("settings", "text"), Tr("Settings"), this->Footer);
  settings->setObjectName("SettingsButton");
  settings->setToolTip(Tr("The application settings of Slicer."));
  QObject::connect(settings, SIGNAL(clicked()), q, SLOT(editApplicationSettings()));
  footer->addWidget(about, 1);
  footer->addWidget(settings, 1);

  // wide: recent files and footer beside the samples
  this->Columns = new QWidget(this->Content);
  this->ColumnsLayout = new QHBoxLayout(this->Columns);
  this->ColumnsLayout->setContentsMargins(0, 0, 0, 0);
  this->ColumnsLayout->setSpacing(28);
  this->LeftColumn = new QWidget(this->Columns);
  this->LeftColumnLayout = new QVBoxLayout(this->LeftColumn);
  this->LeftColumnLayout->setContentsMargins(0, 0, 0, 0);
  this->LeftColumnLayout->setSpacing(16);

  QObject::connect(this->ThemeButton, &qSlicerAstroSegmentedButton::currentIndexChanged, q, [q](int index)
    {
    const char* modes[3] = {"dark", "light", "system"};
    if (index >= 0 && index < 3)
      {
      q->setThemeMode(modes[index]);
      }
    });
  QObject::connect(theme, &qSlicerAstroTheme::themeChanged, q, [this, theme]()
    {
    const QString mode = theme->mode();
    const QSignalBlocker blocker(this->ThemeButton);
    this->ThemeButton->setCurrentIndex(mode == "light" ? 1 : (mode == "system" ? 2 : 0));
    this->updateBackground();
    });
  {
  const QString mode = theme->mode();
  const QSignalBlocker blocker(this->ThemeButton);
  this->ThemeButton->setCurrentIndex(mode == "light" ? 1 : (mode == "system" ? 2 : 0));
  }
  // the recent files change when a file is loaded
  qSlicerApplication* app = qSlicerApplication::application();
  if (app && app->ioManager())
    {
    QObject::connect(app->ioManager(), &qSlicerIOManager::newFileLoaded, q,
      [this, q](const qSlicerIO::IOProperties& properties)
      {
      // when each file was loaded (the order of the recent files and workspaces)
      const QString fileName = properties.value("fileName").toString();
      if (!fileName.isEmpty())
        {
        QSettings settings;
        QVariantMap times = settings.value(LoadTimesKey).toMap();
        times[fileName] = QDateTime::currentDateTime().toString(Qt::ISODate);
        while (times.size() > 50)
          {
          QVariantMap::iterator oldest = times.begin();
          for (QVariantMap::iterator it = times.begin(); it != times.end(); ++it)
            {
            if (it.value().toString() < oldest.value().toString())
              {
              oldest = it;
              }
            }
          times.erase(oldest);
          }
        settings.setValue(LoadTimesKey, times);
        }
      QTimer::singleShot(0, q, [this]() { this->updateRecent(); });
      if (q->isVisible())
        {
        emit q->dataLoaded();
        }
      });
    }
  this->arrange();
  theme->applyTo(q);
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidgetPrivate::arrange()
{
  Q_Q(qSlicerAstroWelcomeWidget);
  // the page in a scroll area of its own when wide (dashboard)
  if (this->Wide && !this->ScrollArea)
    {
    this->RootLayout->removeWidget(this->Content);
    this->ScrollArea = new QScrollArea(q);
    this->ScrollArea->setObjectName("WelcomeScrollArea");
    this->ScrollArea->setFrameShape(QFrame::NoFrame);
    this->ScrollArea->setWidgetResizable(true);
    this->ScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->ScrollArea->setWidget(this->Content);
    this->ScrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    this->RootLayout->addWidget(this->ScrollArea);
    }
  else if (!this->Wide && this->ScrollArea)
    {
    this->RootLayout->removeWidget(this->ScrollArea);
    this->ScrollArea->takeWidget();
    this->ScrollArea->deleteLater();
    this->ScrollArea = nullptr;
    this->Content->setParent(q);
    this->RootLayout->addWidget(this->Content);
    this->Content->show();
    }
  this->ContentLayout->setContentsMargins(this->Wide ? 32 : 14, this->Wide ? 32 : 16, this->Wide ? 32 : 14,
                                          this->Wide ? 36 : 16);
  this->ContentLayout->setSpacing(this->Wide ? 26 : 16);

  // hero: larger on the dashboard
  // (a style sheet of its own: the theme sets the font size of the labels)
  this->Title->setStyleSheet(QString("font-size: %1px; font-weight: 700;").arg(this->Wide ? 28 : 17));
  this->Logo->setPixmap(QIcon(":/AstroTheme/Logo/SlicerAstroLogo.svg").pixmap(this->Wide ? 84 : 56,
                                                                             this->Wide ? 84 : 56));

  // cards: one row (wide) or two columns
  ClearLayout(this->CardsLayout);
  QList<QFrame*> cards;
  cards << this->OpenDataCard << (this->WorkbenchMode ? this->WorkspacesCard : this->WorkbenchCard)
        << this->SkyCard << this->LearnCard;
  this->WorkbenchCard->setVisible(!this->WorkbenchMode);
  this->WorkspacesCard->setVisible(this->WorkbenchMode);
  const int cardColumns = this->Wide ? 4 : 2;
  for (int index = 0; index < cards.size(); index++)
    {
    this->CardsLayout->addWidget(cards[index], index / cardColumns, index % cardColumns);
    }
  for (int column = 0; column < 4; column++)
    {
    this->CardsLayout->setColumnStretch(column, column < cardColumns ? 1 : 0);
    }

  // sections
  ClearLayout(this->ContentLayout);
  ClearLayout(this->LeftColumnLayout);
  ClearLayout(this->ColumnsLayout);
  ClearLayout(this->TopLayout);
  if (this->Wide)
    {
    this->TopLayout->addWidget(this->Hero, 1);
    this->TopLayout->addWidget(this->ThemeRow, 0, Qt::AlignTop);
    this->ThemeRow->setMinimumWidth(300);
    this->ContentLayout->addWidget(this->Top);
    this->Top->show();
    this->ContentLayout->addWidget(this->Cards);
    this->LeftColumnLayout->addWidget(this->Recent);
    this->LeftColumnLayout->addStretch(1);
    this->LeftColumnLayout->addWidget(this->Footer);
    this->ColumnsLayout->addWidget(this->LeftColumn, 1);
    this->ColumnsLayout->addWidget(this->SamplesSection, 2);
    this->ContentLayout->addWidget(this->Columns, 1);
    this->Columns->show();
    }
  else
    {
    this->ThemeRow->setMinimumWidth(0);
    this->ContentLayout->addWidget(this->Hero);
    this->ContentLayout->addWidget(this->ThemeRow);
    this->ContentLayout->addWidget(this->Cards);
    this->ContentLayout->addWidget(this->Recent);
    this->ContentLayout->addWidget(this->SamplesSection);
    this->ContentLayout->addWidget(this->Footer);
    this->ContentLayout->addStretch(1);
    this->Columns->hide();
    this->Top->hide();
    }

  // samples: columns and thumbnails for the width (dashboard)
  this->layoutSamples();
  this->updateBackground();
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidgetPrivate::layoutSamples()
{
  Q_Q(qSlicerAstroWelcomeWidget);
  // dashboard: as many columns of about 260 px as the width allows (3 to 6),
  // thumbnails as wide as the cards; panel: two columns
  int columns = 2;
  int thumbnailWidth = 148;
  if (this->Wide)
    {
    const int width = std::max(600, this->Samples->width());
    columns = std::max(3, std::min(6, width / 260));
    thumbnailWidth = std::min(360, std::max(180, (width - 10 * (columns - 1)) / columns - 24));
    }
  const int thumbnailHeight = static_cast<int>(thumbnailWidth * 0.56);
  if (columns == this->SampleColumns && thumbnailWidth == this->SampleThumbnailWidth &&
      this->SamplesLayout->count() == this->SampleCards.size())
    {
    return;
    }
  this->SampleColumns = columns;
  this->SampleThumbnailWidth = thumbnailWidth;
  ClearLayout(this->SamplesLayout);
  for (int index = 0; index < this->SampleCards.size(); index++)
    {
    this->SamplesLayout->addWidget(this->SampleCards[index], index / columns, index % columns);
    this->SampleThumbnails[index]->setMinimumHeight(thumbnailHeight);
    this->SampleThumbnails[index]->setPixmap(RoundedThumbnail(this->SampleThumbnailPaths[index],
      thumbnailWidth, thumbnailHeight, 8., q->devicePixelRatioF()));
    }
  for (int column = 0; column < 6; column++)
    {
    this->SamplesLayout->setColumnStretch(column, column < columns ? 1 : 0);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidgetPrivate::updateBackground()
{
  Q_Q(qSlicerAstroWelcomeWidget);
  // an overlay (dashboard) is opaque, in the background colour of the theme
  const bool opaque = !this->OverlayTarget.isNull();
  q->setAutoFillBackground(opaque);
  QPalette palette = q->palette();
  palette.setColor(QPalette::Window, qSlicerAstroTheme::instance()->color("bg"));
  q->setPalette(palette);
  if (this->ScrollArea)
    {
    this->ScrollArea->viewport()->setAutoFillBackground(false);
    this->ScrollArea->setAutoFillBackground(false);
    this->ScrollArea->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget#WelcomeContent "
                                    "{ background: transparent; }");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidgetPrivate::updateSamples()
{
  Q_Q(qSlicerAstroWelcomeWidget);
  if (!this->SampleNames.isEmpty())
    {
    return;
    }
  const QVariant result = RunPython("__import__('AstroSampleData').samplesJSON()", true);
  const QJsonArray samples = QJsonDocument::fromJson(result.toString().toUtf8()).array();
  for (int index = 0; index < samples.size(); index++)
    {
    const QJsonObject sample = samples[index].toObject();
    const QString name = sample.value("name").toString();
    QFrame* card = new QFrame(this->Samples);
    card->setObjectName("Sample_" + name);
    card->setToolTip(sample.value("description").toString() + "\n\n" + Tr("Click to download (once) and load it."));
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 8, 8, 10);
    cardLayout->setSpacing(4);
    QLabel* thumbnail = new QLabel(card);
    thumbnail->setAlignment(Qt::AlignCenter);
    // the grid sets the size of the thumbnails (they may shrink with the page)
    thumbnail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    QLabel* nameLabel = new QLabel(name, card);
    QFont font = nameLabel->font();
    font.setBold(true);
    nameLabel->setFont(font);
    QLabel* titleLabel = new QLabel(sample.value("title").toString(), card);
    titleLabel->setProperty("astroRole", "label");
    titleLabel->setWordWrap(true);
    cardLayout->addWidget(thumbnail);
    cardLayout->addWidget(nameLabel);
    cardLayout->addWidget(titleLabel);
    cardLayout->addStretch(1);
    this->makeClickable(card, [q, name]() { q->loadSample(name); });
    this->SampleCards << card;
    this->SampleThumbnails << thumbnail;
    this->SampleThumbnailPaths << sample.value("thumbnail").toString();
    this->SampleNames << name;
    }
  this->arrange();
  qSlicerAstroTheme::instance()->applyTo(q);
}

//-----------------------------------------------------------------------------
QMenu* qSlicerAstroWelcomeWidgetPrivate::recentMenu() const
{
  qSlicerApplication* app = qSlicerApplication::application();
  QMainWindow* mainWindow = app ? app->mainWindow() : nullptr;
  return mainWindow ? mainWindow->findChild<QMenu*>("RecentlyLoadedMenu") : nullptr;
}

//-----------------------------------------------------------------------------
QList<QAction*> qSlicerAstroWelcomeWidgetPrivate::recentActions() const
{
  QList<QAction*> actions;
  QMenu* menu = this->recentMenu();
  if (!menu)
    {
    return actions;
    }
  for (QAction* action : menu->actions())
    {
    if (action->property("fileParameters").isValid() && action->isEnabled())
      {
      actions << action;
      }
    }
  return actions;
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidgetPrivate::updateRecent()
{
  Q_Q(qSlicerAstroWelcomeWidget);
  // rows after the title and the empty text
  while (this->RecentLayout->count() > 2)
    {
    QLayoutItem* item = this->RecentLayout->takeAt(2);
    if (item->widget())
      {
      item->widget()->hide();
      item->widget()->deleteLater();
      }
    delete item;
    }

  // the recent files (File > Recent, with the time they were loaded) and
  // the recent workspaces, newest first
  QList<RecentEntry> entries;
  const QVariantMap times = QSettings().value(LoadTimesKey).toMap();
  const QString cleared = QSettings().value(HistoryClearedKey).toString();
  for (QAction* action : this->recentActions())
    {
    RecentEntry entry;
    entry.Path = action->property("fileParameters").toMap().value("fileName").toString();
    const QFileInfo info(entry.Path);
    // the scene of a workspace: listed as the workspace
    if (QFileInfo::exists(info.absoluteDir().filePath("workspace.json")))
      {
      continue;
      }
    entry.Name = info.fileName();
    entry.Detail = QDir::toNativeSeparators(info.absolutePath());
    entry.Time = times.value(entry.Path).toString();
    entries << entry;
    }
  const QVariant workspaces = RunPython(
    "__import__('AstroWorkbenchLib.Workspaces', fromlist=['recentJSON']).recentJSON()", true);
  for (const QJsonValue& value : QJsonDocument::fromJson(workspaces.toString().toUtf8()).array())
    {
    const QJsonObject workspace = value.toObject();
    if (workspace.value("time").toString() <= cleared)
      {
      continue;
      }
    RecentEntry entry;
    entry.Workspace = true;
    entry.Name = workspace.value("name").toString();
    entry.Path = workspace.value("folder").toString();
    entry.Preview = workspace.value("preview").toString();
    entry.Time = workspace.value("time").toString();
    QStringList datasets;
    for (const QJsonValue& dataset : workspace.value("datasets").toArray())
      {
      datasets << dataset.toString();
      }
    entry.Detail = Tr("Workspace") + (datasets.isEmpty() ? QString() : QString::fromUtf8(" · ") + datasets.join(", "));
    entries << entry;
    }
  // known times first (newest first), then the files of unknown time in
  // the order of File > Recent
  std::stable_sort(entries.begin(), entries.end(), [](const RecentEntry& a, const RecentEntry& b)
    {
    if (a.Time.isEmpty() != b.Time.isEmpty())
      {
      return !a.Time.isEmpty();
      }
    return a.Time > b.Time;
    });
  while (entries.size() > 8)
    {
    entries.removeLast();
    }

  for (int index = 0; index < entries.size(); index++)
    {
    const RecentEntry& entry = entries[index];
    QToolButton* row = new QToolButton(this->Recent);
    row->setObjectName(QString("RecentEntry%1").arg(index));
    row->setProperty("recentKind", entry.Workspace ? "workspace" : "file");
    row->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (entry.Workspace)
      {
      const QPixmap preview = RoundedThumbnail(entry.Preview, 64, 36, 4., q->devicePixelRatioF());
      row->setIcon(preview.isNull() ? qSlicerAstroTheme::instance()->icon("archive", "muted") : QIcon(preview));
      row->setIconSize(QSize(64, 36));
      }
    else
      {
      row->setIcon(qSlicerAstroTheme::instance()->icon(QFileInfo(entry.Path).isDir() ? "folder" : "fileData", "muted"));
      row->setIconSize(QSize(18, 18));
      }
    QString when;
    const QDateTime time = QDateTime::fromString(entry.Time, Qt::ISODate);
    if (time.isValid())
      {
      when = QString::fromUtf8(" · ") + QLocale().toString(time, QLocale::ShortFormat);
      }
    const QString detail = row->fontMetrics().elidedText(entry.Detail + when, Qt::ElideMiddle, this->Wide ? 360 : 300);
    row->setText(entry.Name + "\n" + detail);
    row->setToolTip(entry.Workspace ? Tr("Open the workspace %1 (replaces the current scene)").arg(entry.Name) :
                                      Tr("Load %1").arg(QDir::toNativeSeparators(entry.Path)));
    row->setCursor(Qt::PointingHandCursor);
    QObject::connect(row, &QToolButton::clicked, q, [q, index]() { q->loadRecentFile(index); });
    this->RecentLayout->addWidget(row);
    }
  this->RecentEntries = entries;
  this->RecentEmpty->setText(Tr("Files and workspaces you open are listed here."));
  this->RecentEmpty->setVisible(entries.isEmpty());
  this->ClearRecentButton->setVisible(!entries.isEmpty());
  qSlicerAstroTheme::instance()->applyTo(this->Recent);
}

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeWidget::qSlicerAstroWelcomeWidget(QWidget* parent)
  : QWidget(parent)
  , d_ptr(new qSlicerAstroWelcomeWidgetPrivate(*this))
{
  Q_D(qSlicerAstroWelcomeWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroWelcomeWidget::~qSlicerAstroWelcomeWidget()
{
  Q_D(qSlicerAstroWelcomeWidget);
  if (d->OverlayTarget)
    {
    d->OverlayTarget->removeEventFilter(this);
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroWelcomeWidget::isWide() const
{
  Q_D(const qSlicerAstroWelcomeWidget);
  return d->Wide;
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::setWide(bool wide)
{
  Q_D(qSlicerAstroWelcomeWidget);
  if (d->Wide == wide)
    {
    return;
    }
  d->Wide = wide;
  d->arrange();
  qSlicerAstroTheme::instance()->applyTo(this);
}

//-----------------------------------------------------------------------------
bool qSlicerAstroWelcomeWidget::workbenchMode() const
{
  Q_D(const qSlicerAstroWelcomeWidget);
  return d->WorkbenchMode;
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::setWorkbenchMode(bool workbench)
{
  Q_D(qSlicerAstroWelcomeWidget);
  d->WorkbenchMode = workbench;
  d->arrange();
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::setOverlayTarget(QWidget* target)
{
  Q_D(qSlicerAstroWelcomeWidget);
  if (d->OverlayTarget)
    {
    d->OverlayTarget->removeEventFilter(this);
    }
  d->OverlayTarget = target;
  if (target)
    {
    this->setParent(target);
    target->installEventFilter(this);
    this->setGeometry(target->rect());
    this->raise();
    }
  d->updateBackground();
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::showEvent(QShowEvent* event)
{
  Q_D(qSlicerAstroWelcomeWidget);
  this->QWidget::showEvent(event);
  d->updateSamples();
  d->updateRecent();
  if (d->OverlayTarget)
    {
    this->setGeometry(d->OverlayTarget->rect());
    this->raise();
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroWelcomeWidget::eventFilter(QObject* watched, QEvent* event)
{
  Q_D(qSlicerAstroWelcomeWidget);
  if (watched == d->OverlayTarget && event->type() == QEvent::Resize)
    {
    this->setGeometry(d->OverlayTarget->rect());
    }
  if (watched == d->Samples && event->type() == QEvent::Resize && d->Wide)
    {
    // after the resize (not while the layout is being set)
    QTimer::singleShot(0, this, [d]() { d->layoutSamples(); });
    }
  if (d->Actions.contains(watched))
    {
    QWidget* card = qobject_cast<QWidget*>(watched);
    bool activate = false;
    if (event->type() == QEvent::MouseButtonRelease)
      {
      activate = true;
      }
    else if (event->type() == QEvent::KeyPress)
      {
      const int key = static_cast<QKeyEvent*>(event)->key();
      activate = key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space;
      }
    if (activate && card && card->isEnabled() && d->Actions[watched])
      {
      // run after the event: the action may change the page
      std::function<void()> action = d->Actions[watched];
      QTimer::singleShot(0, this, [action]() { action(); });
      return true;
      }
    }
  return this->QWidget::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------
int qSlicerAstroWelcomeWidget::recentFileCount() const
{
  Q_D(const qSlicerAstroWelcomeWidget);
  return d->RecentEntries.size();
}

//-----------------------------------------------------------------------------
int qSlicerAstroWelcomeWidget::sampleCount() const
{
  Q_D(const qSlicerAstroWelcomeWidget);
  return d->SampleNames.size();
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroWelcomeWidget::sampleNames() const
{
  Q_D(const qSlicerAstroWelcomeWidget);
  return d->SampleNames;
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::updateLists()
{
  Q_D(qSlicerAstroWelcomeWidget);
  d->updateSamples();
  d->updateRecent();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroWelcomeWidget::loadRecentFile(int index)
{
  Q_D(qSlicerAstroWelcomeWidget);
  if (index < 0 || index >= d->RecentEntries.size())
    {
    return false;
    }
  const RecentEntry entry = d->RecentEntries[index];
  bool loaded = false;
  if (entry.Workspace)
    {
    QString folder = entry.Path;
    folder.replace('\\', "\\\\").replace('\'', "\\'");
    loaded = RunPython(QString("__import__('AstroWorkbenchLib.Workspaces', fromlist=['loadWorkspace'])"
                               ".loadWorkspace('%1')").arg(folder), true).toBool();
    }
  else
    {
    // as File > Recent (the same loading options)
    for (QAction* action : d->recentActions())
      {
      if (action->property("fileParameters").toMap().value("fileName").toString() == entry.Path)
        {
        action->trigger();
        loaded = true;
        break;
        }
      }
    }
  if (loaded)
    {
    emit this->dataLoaded();
    }
  d->updateRecent();
  return loaded;
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroWelcomeWidget::recentNames() const
{
  Q_D(const qSlicerAstroWelcomeWidget);
  QStringList names;
  for (const RecentEntry& entry : d->RecentEntries)
    {
    names << entry.Name;
    }
  return names;
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::clearRecentFiles()
{
  Q_D(qSlicerAstroWelcomeWidget);
  QMenu* menu = d->recentMenu();
  if (menu)
    {
    for (QAction* action : menu->actions())
      {
      if (action->property("clearMenu").toBool())
        {
        action->trigger();
        break;
        }
      }
    }
  // the recent workspaces too (the workspaces themselves stay in Spaces)
  QSettings settings;
  settings.setValue(HistoryClearedKey, QDateTime::currentDateTime().toString(Qt::ISODate));
  settings.remove(LoadTimesKey);
  d->updateRecent();
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::openData()
{
  qSlicerApplication* app = qSlicerApplication::application();
  if (app && app->ioManager())
    {
    app->ioManager()->openAddDataDialog();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::openWorkbench()
{
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerModuleManager* moduleManager = app ? app->moduleManager() : nullptr;
  if (moduleManager && moduleManager->module("AstroWorkbench") && app->layoutManager())
    {
    app->layoutManager()->setCurrentModule("AstroWorkbench");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::openDocumentation()
{
  QDesktopServices::openUrl(QUrl("https://github.com/Punzo/SlicerAstro#readme"));
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::showAbout()
{
  // the About window of the AstroWorkbench (one window for both)
  RunPython("from AstroWorkbenchLib import About\nAbout.showAbout()", false);
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::editApplicationSettings()
{
  qSlicerApplication* app = qSlicerApplication::application();
  if (app && app->settingsDialog())
    {
    app->settingsDialog()->exec();
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroWelcomeWidget::loadSample(const QString& name)
{
  QString safeName = name;
  safeName.remove('\'').remove('\\');
  const bool loaded = RunPython(
    QString("__import__('AstroSampleData').loadSample('%1') is not None").arg(safeName), true).toBool();
  if (loaded)
    {
    emit this->dataLoaded();
    }
  return loaded;
}

//-----------------------------------------------------------------------------
void qSlicerAstroWelcomeWidget::setThemeMode(const QString& mode)
{
  qSlicerAstroTheme::instance()->setMode(mode);
}
