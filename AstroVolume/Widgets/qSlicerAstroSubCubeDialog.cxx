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
#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

// VTK includes
#include <vtkCollection.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkStringArray.h>
#include <vtksys/SystemInformation.hxx>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkFITSReader.h>

// Slicer includes
#include <qSlicerCoreApplication.h>
#include <qSlicerCoreIOManager.h>
#include <qSlicerIO.h>
#include <vtkMRMLNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkSlicerApplicationLogic.h>

#include <string>

#include "qSlicerAstroSubCubeDialog.h"
#include "qSlicerAstroTheme.h"

namespace
{
//-----------------------------------------------------------------------------
QString FormatBytes(double bytes)
{
  const char* units[5] = {"B", "KB", "MB", "GB", "TB"};
  int unit = 0;
  while (bytes >= 1024. && unit < 4)
    {
    bytes /= 1024.;
    unit++;
    }
  return QString("%1 %2").arg(bytes, 0, 'f', bytes < 10. && unit > 0 ? 1 : 0).arg(units[unit]);
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroSubCubeDialogPrivate
{
public:
  QString FileName;
  int HDUNumber{0};
  QLabel* ExtensionLabel{nullptr};
  QComboBox* Extension{nullptr};
  int Dimensions[3]{1, 1, 1};
  int BytesPerVoxel{4};
  vtkNew<vtkDoubleArray> SpectralValues;
  QString SpectralUnit;
  QLabel* Title{nullptr};
  QLabel* Whole{nullptr};
  QLabel* Part{nullptr};
  QLabel* Velocities{nullptr};
  QSpinBox* First[3]{nullptr, nullptr, nullptr};
  QSpinBox* Last[3]{nullptr, nullptr, nullptr};
  QLabel* AxisLabels[3]{nullptr, nullptr, nullptr};
  int Choice{qSlicerAstroSubCubeDialog::Cancelled};
};

//-----------------------------------------------------------------------------
qSlicerAstroSubCubeDialog::qSlicerAstroSubCubeDialog(QWidget* parent)
  : QDialog(parent)
  , d_ptr(new qSlicerAstroSubCubeDialogPrivate)
{
  Q_D(qSlicerAstroSubCubeDialog);
  this->setObjectName("AstroSubCubeDialog");
  this->setWindowTitle(tr("Load a part of the data"));
  this->setMinimumWidth(420);
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(10);
  d->Title = new QLabel(this);
  d->Title->setWordWrap(true);
  d->Title->setObjectName("SubCubeTitle");
  layout->addWidget(d->Title);
  // the image extensions of files with several (e.g. DATA and STAT of MUSE)
  QHBoxLayout* extensionRow = new QHBoxLayout();
  extensionRow->setSpacing(8);
  const QString extensionToolTip = tr("Image extension (HDU) of the file to load, e.g. the data (DATA) or the "
                                      "variance (STAT) of a MUSE cube.");
  d->ExtensionLabel = new QLabel(tr("Extension"), this);
  d->ExtensionLabel->setProperty("astroRole", "label");
  d->ExtensionLabel->setToolTip(extensionToolTip);
  d->Extension = new QComboBox(this);
  d->Extension->setObjectName("SubCubeExtension");
  d->Extension->setToolTip(extensionToolTip);
  QObject::connect(d->Extension, SIGNAL(currentIndexChanged(int)), this, SLOT(onExtensionChanged(int)));
  extensionRow->addWidget(d->ExtensionLabel);
  extensionRow->addWidget(d->Extension, 1);
  layout->addLayout(extensionRow);
  d->Whole = new QLabel(this);
  d->Whole->setProperty("astroRole", "monoLabel");
  d->Whole->setObjectName("SubCubeWhole");
  d->Whole->setWordWrap(true);
  layout->addWidget(d->Whole);

  QLabel* partTitle = new QLabel(tr("PART"), this);
  qSlicerAstroTheme::setLabelRole(partTitle, "overline");
  layout->addWidget(partTitle);
  QGridLayout* grid = new QGridLayout();
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  const QString axisNames[3] = {tr("x (pixels)"), tr("y (pixels)"), tr("Channels")};
  const QString axisToolTips[3] = {tr("First and last pixel along the first axis (right ascension)"),
                                   tr("First and last pixel along the second axis (declination)"),
                                   tr("First and last channel (spectral axis)")};
  for (int axis = 0; axis < 3; axis++)
    {
    d->AxisLabels[axis] = new QLabel(axisNames[axis], this);
    d->AxisLabels[axis]->setProperty("astroRole", "label");
    d->AxisLabels[axis]->setToolTip(axisToolTips[axis]);
    d->First[axis] = new QSpinBox(this);
    d->Last[axis] = new QSpinBox(this);
    d->First[axis]->setObjectName(QString("SubCubeFirst%1").arg(axis));
    d->Last[axis]->setObjectName(QString("SubCubeLast%1").arg(axis));
    for (QSpinBox* box : {d->First[axis], d->Last[axis]})
      {
      box->setToolTip(axisToolTips[axis]);
      box->setKeyboardTracking(false);
      QObject::connect(box, SIGNAL(valueChanged(int)), this, SLOT(updateSummary()));
      }
    QLabel* dash = new QLabel(QString::fromUtf8("–"), this);
    grid->addWidget(d->AxisLabels[axis], axis, 0);
    grid->addWidget(d->First[axis], axis, 1);
    grid->addWidget(dash, axis, 2);
    grid->addWidget(d->Last[axis], axis, 3);
    }
  grid->setColumnStretch(1, 1);
  grid->setColumnStretch(3, 1);
  layout->addLayout(grid);
  d->Velocities = new QLabel(this);
  d->Velocities->setProperty("astroRole", "monoLabel");
  d->Velocities->setObjectName("SubCubeVelocities");
  layout->addWidget(d->Velocities);
  d->Part = new QLabel(this);
  d->Part->setProperty("astroRole", "monoLabel");
  d->Part->setObjectName("SubCubePart");
  layout->addWidget(d->Part);

  QHBoxLayout* buttons = new QHBoxLayout();
  buttons->setSpacing(8);
  QPushButton* cancel = new QPushButton(tr("Cancel"), this);
  cancel->setObjectName("SubCubeCancel");
  cancel->setIcon(qSlicerAstroTheme::instance()->icon("close", "text"));
  QPushButton* whole = new QPushButton(tr("Whole data"), this);
  whole->setObjectName("SubCubeLoadWhole");
  whole->setIcon(qSlicerAstroTheme::instance()->icon("cube3d", "text"));
  whole->setToolTip(tr("Load all the data of the file"));
  QPushButton* part = new QPushButton(tr("Load the part"), this);
  part->setObjectName("SubCubeLoadPart");
  part->setProperty("astroRole", "primary");
  part->setIcon(qSlicerAstroTheme::instance()->icon("crop", "on-accent"));
  part->setToolTip(tr("Load only the chosen part (only these voxels are read from the file)"));
  part->setDefault(true);
  QObject::connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));
  QObject::connect(whole, SIGNAL(clicked()), this, SLOT(onWholeClicked()));
  QObject::connect(part, SIGNAL(clicked()), this, SLOT(onPartClicked()));
  buttons->addWidget(cancel);
  buttons->addStretch(1);
  buttons->addWidget(whole);
  buttons->addWidget(part);
  layout->addLayout(buttons);
  qSlicerAstroTheme::instance()->applyTo(this);
}

//-----------------------------------------------------------------------------
qSlicerAstroSubCubeDialog::~qSlicerAstroSubCubeDialog() = default;

//-----------------------------------------------------------------------------
bool qSlicerAstroSubCubeDialog::setFile(const QString& fileName, int hduNumber)
{
  Q_D(qSlicerAstroSubCubeDialog);
  d->FileName = fileName;
  d->Title->setText(tr("Which part of %1 to load?").arg(QFileInfo(fileName).fileName()));
  QList<int> numbers;
  QStringList texts;
  qSlicerAstroSubCubeDialog::imageExtensions(fileName, numbers, texts);
  int current = -1;
  {
  const QSignalBlocker blocker(d->Extension);
  d->Extension->clear();
  for (int index = 0; index < numbers.size(); index++)
    {
    d->Extension->addItem(texts[index], numbers[index]);
    if (numbers[index] == hduNumber || (hduNumber <= 0 && index == 0))
      {
      current = index;
      }
    }
  d->Extension->setCurrentIndex(current);
  }
  QAbstractItemView* view = d->Extension->view();
  view->setTextElideMode(Qt::ElideNone);
  view->setMinimumWidth(view->sizeHintForColumn(0) + 2 * view->frameWidth() + 16);
  const bool several = numbers.size() > 1;
  d->Extension->setVisible(several);
  d->ExtensionLabel->setVisible(several);
  return this->readDescription(current >= 0 ? numbers[current] : hduNumber);
}

//-----------------------------------------------------------------------------
int qSlicerAstroSubCubeDialog::hduNumber() const
{
  Q_D(const qSlicerAstroSubCubeDialog);
  return d->HDUNumber;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSubCubeDialog::onExtensionChanged(int index)
{
  Q_D(qSlicerAstroSubCubeDialog);
  if (index >= 0)
    {
    this->readDescription(d->Extension->itemData(index).toInt());
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroSubCubeDialog::readDescription(int hduNumber)
{
  Q_D(qSlicerAstroSubCubeDialog);
  std::string unit;
  if (!vtkSlicerAstroVolumeLogic::GetFITSDataDescription(d->FileName.toUtf8().constData(), hduNumber, d->Dimensions,
                                                         &d->BytesPerVoxel, d->SpectralValues, &unit))
    {
    return false;
    }
  d->HDUNumber = hduNumber;
  d->SpectralUnit = QString::fromStdString(unit);
  for (int axis = 0; axis < 3; axis++)
    {
    const bool used = d->Dimensions[axis] > 1 || axis < 2;
    for (QWidget* widget : {static_cast<QWidget*>(d->First[axis]), static_cast<QWidget*>(d->Last[axis]),
                            static_cast<QWidget*>(d->AxisLabels[axis])})
      {
      widget->setVisible(used);
      }
    // shown 1-based (FITS pixels and channels)
    const QSignalBlocker firstBlocker(d->First[axis]);
    const QSignalBlocker lastBlocker(d->Last[axis]);
    d->First[axis]->setRange(1, d->Dimensions[axis]);
    d->Last[axis]->setRange(1, d->Dimensions[axis]);
    d->First[axis]->setValue(1);
    d->Last[axis]->setValue(d->Dimensions[axis]);
    }
  this->updateSummary();
  return true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSubCubeDialog::setSubExtent(int i0, int i1, int j0, int j1, int k0, int k1)
{
  Q_D(qSlicerAstroSubCubeDialog);
  const int extent[6] = {i0, i1, j0, j1, k0, k1};
  for (int axis = 0; axis < 3; axis++)
    {
    d->First[axis]->setValue(extent[2 * axis] + 1);
    d->Last[axis]->setValue(extent[2 * axis + 1] + 1);
    }
}

//-----------------------------------------------------------------------------
QList<int> qSlicerAstroSubCubeDialog::subExtent() const
{
  Q_D(const qSlicerAstroSubCubeDialog);
  QList<int> extent;
  for (int axis = 0; axis < 3; axis++)
    {
    const int first = std::min(d->First[axis]->value(), d->Last[axis]->value());
    const int last = std::max(d->First[axis]->value(), d->Last[axis]->value());
    extent << first - 1 << last - 1;
    }
  return extent;
}

//-----------------------------------------------------------------------------
double qSlicerAstroSubCubeDialog::wholeBytes() const
{
  Q_D(const qSlicerAstroSubCubeDialog);
  return static_cast<double>(d->Dimensions[0]) * d->Dimensions[1] * d->Dimensions[2] * d->BytesPerVoxel;
}

//-----------------------------------------------------------------------------
double qSlicerAstroSubCubeDialog::subCubeBytes() const
{
  Q_D(const qSlicerAstroSubCubeDialog);
  const QList<int> extent = this->subExtent();
  return static_cast<double>(extent[1] - extent[0] + 1) * (extent[3] - extent[2] + 1) *
         (extent[5] - extent[4] + 1) * d->BytesPerVoxel;
}

//-----------------------------------------------------------------------------
int qSlicerAstroSubCubeDialog::choice() const
{
  Q_D(const qSlicerAstroSubCubeDialog);
  return d->Choice;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSubCubeDialog::updateSummary()
{
  Q_D(qSlicerAstroSubCubeDialog);
  const QString times = QString::fromUtf8("×");
  const QString dims = d->Dimensions[2] > 1 ?
    QString("%1%2%3%4%5").arg(d->Dimensions[0]).arg(times).arg(d->Dimensions[1]).arg(times).arg(d->Dimensions[2]) :
    QString("%1%2%3").arg(d->Dimensions[0]).arg(times).arg(d->Dimensions[1]);
  const double memory = qSlicerAstroSubCubeDialog::physicalMemory();
  d->Whole->setText(tr("Whole data: %1 · %2%3").arg(dims, FormatBytes(this->wholeBytes()),
    memory > 0. ? tr(" (memory of the computer: %1)").arg(FormatBytes(memory)) : QString()));
  const QList<int> extent = this->subExtent();
  QString part = QString("%1%2%3").arg(extent[1] - extent[0] + 1).arg(times).arg(extent[3] - extent[2] + 1);
  if (d->Dimensions[2] > 1)
    {
    part += times + QString::number(extent[5] - extent[4] + 1);
    }
  d->Part->setText(tr("Part: %1 · %2").arg(part, FormatBytes(this->subCubeBytes())));
  // the spectral range of the channels
  const vtkIdType channels = d->SpectralValues->GetNumberOfValues();
  if (channels > 0 && extent[4] < channels && extent[5] < channels)
    {
    d->Velocities->setText(tr("Channels %1–%2: %3 to %4 %5")
                             .arg(extent[4] + 1).arg(extent[5] + 1)
                             .arg(d->SpectralValues->GetValue(extent[4]), 0, 'f', 1)
                             .arg(d->SpectralValues->GetValue(extent[5]), 0, 'f', 1)
                             .arg(d->SpectralUnit));
    d->Velocities->show();
    }
  else
    {
    d->Velocities->hide();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSubCubeDialog::onWholeClicked()
{
  Q_D(qSlicerAstroSubCubeDialog);
  d->Choice = WholeData;
  this->accept();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSubCubeDialog::onPartClicked()
{
  Q_D(qSlicerAstroSubCubeDialog);
  d->Choice = SubCube;
  this->accept();
}

//-----------------------------------------------------------------------------
double qSlicerAstroSubCubeDialog::physicalMemory()
{
  vtksys::SystemInformation information;
  information.RunMemoryCheck();
  // MiB
  return static_cast<double>(information.GetTotalPhysicalMemory()) * 1024. * 1024.;
}

//-----------------------------------------------------------------------------
double qSlicerAstroSubCubeDialog::dataBytes(const QString& fileName, int hduNumber)
{
  int dimensions[3] = {1, 1, 1};
  int bytesPerVoxel = 4;
  if (!vtkSlicerAstroVolumeLogic::GetFITSDataDescription(fileName.toUtf8().constData(), hduNumber, dimensions,
                                                         &bytesPerVoxel, nullptr, nullptr))
    {
    return 0.;
    }
  return static_cast<double>(dimensions[0]) * dimensions[1] * dimensions[2] * bytesPerVoxel;
}

//-----------------------------------------------------------------------------
int qSlicerAstroSubCubeDialog::ask(const QString& fileName, int* hduNumber, QWidget* parent, int subExtent[6])
{
  qSlicerAstroSubCubeDialog dialog(parent);
  if (!dialog.setFile(fileName, hduNumber ? *hduNumber : 0))
    {
    return WholeData;
    }
  if (dialog.exec() != QDialog::Accepted)
    {
    return Cancelled;
    }
  const QList<int> extent = dialog.subExtent();
  for (int index = 0; index < 6 && subExtent; index++)
    {
    subExtent[index] = extent[index];
    }
  if (hduNumber)
    {
    *hduNumber = dialog.hduNumber();
    }
  return dialog.choice();
}

//-----------------------------------------------------------------------------
vtkMRMLNode* qSlicerAstroSubCubeDialog::load()
{
  Q_D(qSlicerAstroSubCubeDialog);
  qSlicerCoreApplication* app = qSlicerCoreApplication::application();
  if (d->FileName.isEmpty() || d->Choice == Cancelled || !app || !app->coreIOManager())
    {
    return nullptr;
    }
  qSlicerIO::IOProperties properties;
  properties["fileName"] = d->FileName;
  properties["hdu"] = d->HDUNumber;
  if (d->Choice == SubCube)
    {
    QVariantList extent;
    const QList<int> values = this->subExtent();
    for (int index = 0; index < values.size(); index++)
      {
      extent << values[index];
      }
    properties["subExtent"] = extent;
    }
  else
    {
    // the whole data: already chosen here
    properties["askLargeData"] = false;
    }
  vtkNew<vtkCollection> loaded;
  if (!app->coreIOManager()->loadNodes("AstroVolumeFile", properties, loaded))
    {
    return nullptr;
    }
  vtkMRMLNode* node = vtkMRMLNode::SafeDownCast(loaded->GetItemAsObject(0));
  vtkMRMLSelectionNode* selection = app->applicationLogic() ? app->applicationLogic()->GetSelectionNode() : nullptr;
  if (node && selection)
    {
    selection->SetActiveVolumeID(node->GetID());
    }
  return node;
}

//-----------------------------------------------------------------------------
void qSlicerAstroSubCubeDialog::imageExtensions(const QString& fileName, QList<int>& numbers, QStringList& texts)
{
  struct CachedExtensions
    {
    QDateTime Modified;
    QList<int> Numbers;
    QStringList Texts;
    };
  static QHash<QString, CachedExtensions> cache;
  const QDateTime modified = QFileInfo(fileName).lastModified();
  if (!cache.contains(fileName) || cache[fileName].Modified != modified)
    {
    vtkNew<vtkIntArray> hdus;
    vtkNew<vtkStringArray> names;
    vtkNew<vtkStringArray> descriptions;
    vtkFITSReader::GetImageHDUs(fileName.toUtf8().constData(), hdus, names, descriptions);
    CachedExtensions entry;
    entry.Modified = modified;
    for (vtkIdType index = 0; index < hdus->GetNumberOfValues(); index++)
      {
      entry.Numbers << hdus->GetValue(index);
      // "134 x 70 x 83 · JY/BEAM" written as the facts: "134×70×83 · Jy/beam"
      QStringList parts = QString::fromStdString(descriptions->GetValue(index)).split(" · ");
      parts[0].replace(" x ", QString::fromUtf8("×"));
      if (parts.size() > 1)
        {
        parts[1] = qSlicerAstroTheme::displayUnit(parts[1]);
        }
      parts.prepend(QString::fromStdString(names->GetValue(index)));
      entry.Texts << parts.join(" · ");
      }
    cache.insert(fileName, entry);
    }
  numbers = cache[fileName].Numbers;
  texts = cache[fileName].Texts;
}
