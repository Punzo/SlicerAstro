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
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>

// Slicer includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerModuleManager.h>

// MRML includes
#include <vtkMRMLViewNode.h>

// STD includes
#include <algorithm>
#include <functional>

// AstroVolume includes
#include "qSlicerAstroSegmentedButton.h"
#include "qSlicerAstroSettingsDialog.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroValueField.h"
#include "vtkSlicerAstroVolumeLogic.h"

namespace
{
const char* SaveCompressedKey = "SlicerAstro/SaveCompressed";
const char* QualityKey = "SlicerAstro/VolumeRenderingQuality";
const char* AutoShow3DKey = "SlicerAstro/AutoShow3DMaximumMegabytes";
const char* LargeDataKey = "SlicerAstro/LargeDataFraction";
const char* WorkspacesKey = "SlicerAstro/WorkspacesDirectory";

//-----------------------------------------------------------------------------
QString Tr(const char* text)
{
  return qSlicerAstroSettingsDialog::tr(text);
}

//-----------------------------------------------------------------------------
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}

//-----------------------------------------------------------------------------
QLabel* Note(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  label->setProperty("astroRole", "label");
  label->setWordWrap(true);
  return label;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* AstroVolumeLogic()
{
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* module = app && app->moduleManager() ? app->moduleManager()->module("AstroVolume") :
                                                                    nullptr;
  return module ? vtkSlicerAstroVolumeLogic::SafeDownCast(module->logic()) : nullptr;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroSettingsDialogPrivate
{
public:
  qSlicerAstroSegmentedButton* ThemeButton{nullptr};
  QCheckBox* CompressedCheckBox{nullptr};
  QLineEdit* WorkspacesEdit{nullptr};
  qSlicerAstroSegmentedButton* QualityButton{nullptr};
  qSlicerAstroValueField* AutoShowField{nullptr};
  qSlicerAstroValueField* LargeDataField{nullptr};
};

//-----------------------------------------------------------------------------
qSlicerAstroSettingsDialog::qSlicerAstroSettingsDialog(QWidget* parent)
  : QDialog(parent)
  , d_ptr(new qSlicerAstroSettingsDialogPrivate)
{
  Q_D(qSlicerAstroSettingsDialog);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  this->setObjectName("SlicerAstroSettings");
  this->setWindowTitle(Tr("SlicerAstro settings"));
  this->setMinimumWidth(520);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 18, 20, 16);
  layout->setSpacing(16);

  QLabel* title = new QLabel(Tr("SlicerAstro settings"), this);
  title->setStyleSheet("font-size: 18px; font-weight: 700;");
  layout->addWidget(title);
  layout->addWidget(Note(Tr("Changes apply at once and are kept for the next sessions."), this));

  QGridLayout* grid = new QGridLayout();
  grid->setHorizontalSpacing(12);
  grid->setVerticalSpacing(8);
  grid->setColumnStretch(1, 1);
  int row = 0;
  std::function<void(const QString&, QWidget*, const QString&)> addRow =
    [this, grid, &row](const QString& label, QWidget* field, const QString& toolTip)
    {
    QLabel* rowLabel = new QLabel(label, this);
    rowLabel->setProperty("astroRole", "label");
    rowLabel->setToolTip(toolTip);
    field->setToolTip(toolTip);
    grid->addWidget(rowLabel, row, 0);
    grid->addWidget(field, row, 1);
    row++;
    };
  std::function<void(const QString&)> addSection = [this, grid, &row](const QString& text)
    {
    QLabel* overline = Overline(text, this);
    grid->addWidget(overline, row, 0, 1, 2);
    row++;
    };

  // appearance
  addSection(Tr("Appearance"));
  d->ThemeButton = new qSlicerAstroSegmentedButton(this);
  d->ThemeButton->setObjectName("SettingsThemeButton");
  d->ThemeButton->addButton(Tr("Dark"), Tr("Observatory: dark, for long sessions with the data (default)."));
  d->ThemeButton->addButton(Tr("Light"), Tr("Daylight: light."));
  d->ThemeButton->addButton(Tr("System"), Tr("Dark or light as the operating system."));
  addRow(Tr("Theme"), d->ThemeButton, Tr("Colours of the SlicerAstro panels and of the AstroWorkbench."));

  // saving
  addSection(Tr("Saving"));
  d->CompressedCheckBox = new QCheckBox(Tr("Save new data as compressed FITS (.fits.gz)"), this);
  d->CompressedCheckBox->setObjectName("SettingsCompressedCheckBox");
  addRow(Tr("FITS files"), d->CompressedCheckBox, Tr(
    "New data (e.g. smoothed cubes, moment maps, models) are saved as .fits.gz (smaller, slower to save and "
    "load) instead of .fits. Loaded files keep their own format."));
  QWidget* folder = new QWidget(this);
  QHBoxLayout* folderLayout = new QHBoxLayout(folder);
  folderLayout->setContentsMargins(0, 0, 0, 0);
  folderLayout->setSpacing(6);
  d->WorkspacesEdit = new QLineEdit(folder);
  d->WorkspacesEdit->setObjectName("SettingsWorkspacesEdit");
  QPushButton* browse = new QPushButton(theme->icon("folder", "text"), Tr("Choose..."), folder);
  browse->setObjectName("SettingsWorkspacesBrowse");
  browse->setToolTip(Tr("Choose the folder of the workspaces."));
  folderLayout->addWidget(d->WorkspacesEdit, 1);
  folderLayout->addWidget(browse);
  addRow(Tr("Workspaces folder"), folder, Tr(
    "Folder of the workspaces of the AstroWorkbench (Spaces): one folder per workspace with the scene, a "
    "picture of the views and its description."));

  // 3D views
  addSection(Tr("3D views"));
  d->QualityButton = new qSlicerAstroSegmentedButton(this);
  d->QualityButton->setObjectName("SettingsQualityButton");
  d->QualityButton->addButton(Tr("Adaptive"), Tr("Lower quality while the view moves, full quality at rest (fast)."));
  d->QualityButton->addButton(Tr("Normal"), Tr("The same quality while the view moves and at rest."));
  d->QualityButton->addButton(Tr("Maximum"), Tr("Finest sampling of the data (slow with large cubes)."));
  addRow(Tr("Rendering quality"), d->QualityButton, Tr("Quality of the volume rendering of the cubes in the 3D views."));
  d->AutoShowField = new qSlicerAstroValueField(this);
  d->AutoShowField->setObjectName("SettingsAutoShowField");
  d->AutoShowField->setUnit("MB");
  d->AutoShowField->setDecimals(0);
  d->AutoShowField->setRange(0., 1.e6);
  d->AutoShowField->setSingleStep(256.);
  addRow(Tr("Show in 3D up to"), d->AutoShowField, Tr(
    "Cubes up to this size are shown in 3D automatically when they become the active data; larger cubes "
    "(they go to the graphics memory) only on request. 0: never automatically."));

  // loading
  addSection(Tr("Loading"));
  d->LargeDataField = new qSlicerAstroValueField(this);
  d->LargeDataField->setObjectName("SettingsLargeDataField");
  d->LargeDataField->setUnit(Tr("% of memory"));
  d->LargeDataField->setDecimals(0);
  d->LargeDataField->setRange(1., 100.);
  d->LargeDataField->setSingleStep(5.);
  addRow(Tr("Offer a part above"), d->LargeDataField, Tr(
    "Files larger than this fraction of the memory of the computer: a dialog offers to load a part (a box on "
    "the sky and a range of channels) or an extension of the file."));
  layout->addLayout(grid);
  layout->addStretch(1);

  // buttons
  QHBoxLayout* buttons = new QHBoxLayout();
  QPushButton* close = new QPushButton(Tr("Close"), this);
  close->setObjectName("SettingsCloseButton");
  close->setProperty("astroRole", "primary");
  close->setDefault(true);
  close->setToolTip(Tr("Close the settings (the changes are already applied)."));
  buttons->addStretch(1);
  buttons->addWidget(close);
  layout->addLayout(buttons);

  this->updateFromSettings();
  QObject::connect(d->ThemeButton, &qSlicerAstroSegmentedButton::currentIndexChanged, this, [](int index)
    {
    const char* modes[3] = {"dark", "light", "system"};
    if (index >= 0 && index < 3)
      {
      qSlicerAstroTheme::instance()->setMode(modes[index]);
      }
    });
  QObject::connect(theme, &qSlicerAstroTheme::themeChanged, this, [this]()
    {
    this->updateFromSettings();
    qSlicerAstroTheme::instance()->applyToDialog(this);
    });
  QObject::connect(d->CompressedCheckBox, &QCheckBox::toggled, this, &qSlicerAstroSettingsDialog::setSaveCompressed);
  QObject::connect(d->QualityButton, &qSlicerAstroSegmentedButton::currentIndexChanged, this,
                   &qSlicerAstroSettingsDialog::setVolumeRenderingQuality);
  QObject::connect(d->AutoShowField, &qSlicerAstroValueField::valueChanged, this,
                   &qSlicerAstroSettingsDialog::setAutoShow3DMaximumMegabytes);
  QObject::connect(d->LargeDataField, &qSlicerAstroValueField::valueChanged, this,
                   &qSlicerAstroSettingsDialog::setLargeDataPercent);
  QObject::connect(d->WorkspacesEdit, &QLineEdit::editingFinished, this, [this, d]()
    {
    this->setWorkspacesDirectory(d->WorkspacesEdit->text());
    });
  QObject::connect(browse, &QPushButton::clicked, this, [this, d]()
    {
    const QString directory = QFileDialog::getExistingDirectory(this, Tr("Folder of the workspaces"),
                                                                d->WorkspacesEdit->text());
    if (!directory.isEmpty())
      {
      this->setWorkspacesDirectory(directory);
      }
    });
  QObject::connect(close, SIGNAL(clicked()), this, SLOT(accept()));
  theme->applyToDialog(this);
}

//-----------------------------------------------------------------------------
qSlicerAstroSettingsDialog::~qSlicerAstroSettingsDialog() = default;

//-----------------------------------------------------------------------------
bool qSlicerAstroSettingsDialog::saveCompressed()
{
  return QSettings().value(SaveCompressedKey, false).toBool();
}

//-----------------------------------------------------------------------------
int qSlicerAstroSettingsDialog::volumeRenderingQuality()
{
  return QSettings().value(QualityKey, vtkMRMLViewNode::Normal).toInt();
}

//-----------------------------------------------------------------------------
double qSlicerAstroSettingsDialog::autoShow3DMaximumMegabytes()
{
  return QSettings().value(AutoShow3DKey, 1024.).toDouble();
}

//-----------------------------------------------------------------------------
double qSlicerAstroSettingsDialog::largeDataPercent()
{
  return 100. * QSettings().value(LargeDataKey, 0.25).toDouble();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroSettingsDialog::workspacesDirectory()
{
  const QString defaultDirectory = QDir(QDir::homePath()).filePath("SlicerAstro/Workspaces");
  const QString directory = QSettings().value(WorkspacesKey, defaultDirectory).toString();
  return directory.isEmpty() ? defaultDirectory : directory;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSettingsDialog::applySettings(vtkSlicerAstroVolumeLogic* logic)
{
  if (!logic)
    {
    logic = AstroVolumeLogic();
    }
  if (!logic)
    {
    return;
    }
  logic->SetSaveCompressed(saveCompressed());
  logic->SetDefaultVolumeRenderingQuality(volumeRenderingQuality());
  logic->SetAutoShow3DMaximumMegabytes(autoShow3DMaximumMegabytes());
}

//-----------------------------------------------------------------------------
void qSlicerAstroSettingsDialog::showSettings(QWidget* parent)
{
  qSlicerApplication* app = qSlicerApplication::application();
  QWidget* window = parent ? parent : (app ? static_cast<QWidget*>(app->mainWindow()) : nullptr);
  qSlicerAstroSettingsDialog dialog(window);
  dialog.exec();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSettingsDialog::updateFromSettings()
{
  Q_D(qSlicerAstroSettingsDialog);
  const QString mode = qSlicerAstroTheme::instance()->mode();
  const QSignalBlocker themeBlocker(d->ThemeButton);
  d->ThemeButton->setCurrentIndex(mode == "light" ? 1 : (mode == "system" ? 2 : 0));
  const QSignalBlocker compressedBlocker(d->CompressedCheckBox);
  d->CompressedCheckBox->setChecked(saveCompressed());
  const QSignalBlocker qualityBlocker(d->QualityButton);
  d->QualityButton->setCurrentIndex(std::max(0, std::min(2, volumeRenderingQuality())));
  const QSignalBlocker autoShowBlocker(d->AutoShowField);
  d->AutoShowField->setValue(autoShow3DMaximumMegabytes());
  const QSignalBlocker largeBlocker(d->LargeDataField);
  d->LargeDataField->setValue(largeDataPercent());
  const QSignalBlocker workspacesBlocker(d->WorkspacesEdit);
  d->WorkspacesEdit->setText(QDir::toNativeSeparators(workspacesDirectory()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroSettingsDialog::setSaveCompressed(bool compressed)
{
  QSettings().setValue(SaveCompressedKey, compressed);
  applySettings();
  this->updateFromSettings();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSettingsDialog::setVolumeRenderingQuality(int quality)
{
  QSettings().setValue(QualityKey, quality);
  applySettings();
  this->updateFromSettings();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSettingsDialog::setAutoShow3DMaximumMegabytes(double megabytes)
{
  QSettings().setValue(AutoShow3DKey, std::max(0., megabytes));
  applySettings();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSettingsDialog::setLargeDataPercent(double percent)
{
  QSettings().setValue(LargeDataKey, std::max(0.01, std::min(1., percent / 100.)));
}

//-----------------------------------------------------------------------------
void qSlicerAstroSettingsDialog::setWorkspacesDirectory(const QString& directory)
{
  if (!directory.trimmed().isEmpty())
    {
    QSettings().setValue(WorkspacesKey, QDir::fromNativeSeparators(directory.trimmed()));
    }
  this->updateFromSettings();
}
