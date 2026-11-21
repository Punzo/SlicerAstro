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
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QVariantList>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLStorageNode.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "qSlicerAstroInfoSection.h"
#include "qSlicerAstroSubCubeDialog.h"
#include <qSlicerCoreIOManager.h>
#include <qSlicerCoreApplication.h>
#include <vtkSlicerApplicationLogic.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkNew.h>
#include <vtkCollection.h>
#include "qSlicerAstroSegmentedButton.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroValueField.h"

namespace
{
const char* const FactNames[] = {"Object", "Telescope", "Dimensions", "Unit", "Beam", "Channel width",
                                 "Noise (σ)", "Spectral axis", "File"};
const char* const AttributePrefix = "SlicerAstro.";

//----------------------------------------------------------------------------
QString Attribute(vtkMRMLNode* node, const char* key)
{
  const char* value = node ? node->GetAttribute((std::string(AttributePrefix) + key).c_str()) : nullptr;
  const QString text = value ? QString::fromUtf8(value).trimmed() : QString();
  return text == "UNDEFINED" ? QString() : text;
}

//----------------------------------------------------------------------------
QString FormatAngle(double degrees)
{
  const double arcsec = degrees * 3600.;
  return arcsec >= 120. ? QString::fromUtf8("%1′").arg(arcsec / 60., 0, 'g', 3) :
    QString::fromUtf8("%1″").arg(arcsec, 0, 'g', 3);
}

//----------------------------------------------------------------------------
QLabel* RowLabel(const QString& text, const QString& toolTip, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  label->setProperty("astroRole", "label");
  label->setToolTip(toolTip);
  return label;
}
//-----------------------------------------------------------------------------
QToolButton* ConventionLabel(const QString& text, const QString& iconName, const QString& toolTip, QWidget* parent)
{
  // a tool button only to draw the themed icon beside the text (not
  // clickable: no hover style, see astro.qss)
  QToolButton* label = new QToolButton(parent);
  label->setText(text);
  label->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "muted"));
  label->setIconSize(QSize(16, 16));
  label->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  label->setAutoRaise(true);
  label->setFocusPolicy(Qt::NoFocus);
  label->setProperty("astroRole", "conventionLabel");
  label->setToolTip(toolTip);
  return label;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroInfoSectionPrivate
{
public:
  vtkWeakPointer<vtkMRMLAstroVolumeNode> Volume;
  QMap<QString, QLabel*> Facts;
  qSlicerAstroSegmentedButton* RightAscension{nullptr};
  qSlicerAstroSegmentedButton* Velocity{nullptr};
  QToolButton* VelocityLabel{nullptr};
  // the line of optical data in wavelengths
  QToolButton* LineLabel{nullptr};
  QWidget* LineRow{nullptr};
  QComboBox* Line{nullptr};
  QDoubleSpinBox* LineWavelength{nullptr};
  QPushButton* LineApply{nullptr};
  qSlicerAstroValueField* Beam[3] = {nullptr, nullptr, nullptr};
  QPushButton* EstimateButton{nullptr};
  QString EstimateToolTip;
  QPushButton* ApplyButton{nullptr};
  QLabel* BeamStatus{nullptr};
  QToolButton* HeaderToggle{nullptr};
  QLineEdit* Filter{nullptr};
  QTableWidget* Table{nullptr};
  QWidget* FileRow{nullptr};
  QLabel* ExtensionLabel{nullptr};
  QComboBox* Extension{nullptr};
};

//-----------------------------------------------------------------------------
qSlicerAstroInfoSection::qSlicerAstroInfoSection(QWidget* parent)
  : QWidget(parent)
  , d_ptr(new qSlicerAstroInfoSectionPrivate)
{
  Q_D(qSlicerAstroInfoSection);
  this->setObjectName("InfoSection");
  this->setToolTip(tr("Information about the data: facts, coordinate conventions, beam and FITS header."));
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  // facts
  QFrame* factsCard = new QFrame(this);
  factsCard->setObjectName("InfoFacts");
  factsCard->setProperty("astroRole", "card");
  QGridLayout* facts = new QGridLayout(factsCard);
  facts->setContentsMargins(12, 10, 12, 10);
  facts->setHorizontalSpacing(12);
  facts->setVerticalSpacing(4);
  const QMap<QString, QString> factToolTips = {
    {"Object", tr("Name of the object (OBJECT keyword, or the name of the data).")},
    {"Telescope", tr("Telescope (TELESCOP keyword).")},
    {"Dimensions", tr("Number of pixels along each axis (channels last).")},
    {"Unit", tr("Unit of the data values (BUNIT keyword).")},
    {"Beam", tr("Beam of the data: FWHM of the major and minor axes.")},
    {"Channel width", tr("Width of a channel at the centre of the field.")},
    {"Noise (σ)", tr("Noise of the data: used for the color functions, contours and thresholds.")},
    {"Spectral axis", tr("Type of the spectral axis and its reference frame (CTYPE3, SPECSYS).")},
    {"File", tr("File of the data.")}};
  int row = 0;
  for (const char* name : FactNames)
    {
    const QString key = QString::fromUtf8(name);
    QLabel* value = new QLabel(this);
    value->setProperty("astroRole", "mono");
    value->setObjectName(QString("Fact_%1").arg(key));
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setToolTip(factToolTips.value(key));
    value->setMinimumWidth(40);
    facts->addWidget(RowLabel(key, factToolTips.value(key), this), row, 0);
    facts->addWidget(value, row, 1);
    d->Facts.insert(key, value);
    row++;
    }
  facts->setColumnStretch(1, 1);
  layout->addWidget(factsCard);

  // the file: its image extensions (HDUs), a part of the data
  d->FileRow = new QWidget(this);
  QHBoxLayout* fileRow = new QHBoxLayout(d->FileRow);
  fileRow->setContentsMargins(0, 0, 0, 0);
  fileRow->setSpacing(8);
  const QString extensionToolTip = tr("Image extensions (HDUs) of the file. Choosing another one loads its data "
                                      "as a new dataset (or shows it if it is loaded), e.g. the variance of a "
                                      "MUSE cube.");
  d->ExtensionLabel = RowLabel(tr("Extension"), extensionToolTip, this);
  d->Extension = new QComboBox(this);
  d->Extension->setObjectName("InfoExtension");
  d->Extension->setToolTip(extensionToolTip);
  d->Extension->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->Extension->setMinimumContentsLength(8);
  QObject::connect(d->Extension, SIGNAL(activated(int)), this, SLOT(onExtensionActivated(int)));
  fileRow->addWidget(d->ExtensionLabel);
  fileRow->addWidget(d->Extension, 1);
  layout->addWidget(d->FileRow);

  // coordinate conventions of the readouts and axes: one row each, an icon
  // and a label, the choices across the width
  QGridLayout* conventions = new QGridLayout();
  conventions->setHorizontalSpacing(8);
  conventions->setVerticalSpacing(6);
  d->RightAscension = new qSlicerAstroSegmentedButton(this);
  d->RightAscension->setObjectName("InfoRightAscension");
  d->RightAscension->setToolTip(tr("Right ascension in the readouts and on the axes of the views."));
  d->RightAscension->addButton(tr("h m s"), tr("Right ascension in hours, minutes and seconds."));
  d->RightAscension->addButton(tr("degrees"), tr("Right ascension in degrees."));
  QObject::connect(d->RightAscension, SIGNAL(currentIndexChanged(int)), this,
                   SLOT(onRightAscensionFormatChanged(int)));
  d->Velocity = new qSlicerAstroSegmentedButton(this);
  d->Velocity->setObjectName("InfoVelocity");
  d->Velocity->setToolTip(tr("Velocity definition of the spectral axis in the readouts and on the axes."));
  d->Velocity->addButton(tr("radio"), tr("Radio velocities: v = c (f0 − f) / f0."));
  d->Velocity->addButton(tr("optical"), tr("Optical velocities: v = c (f0 − f) / f."));
  QObject::connect(d->Velocity, SIGNAL(currentIndexChanged(int)), this, SLOT(onVelocityDefinitionChanged(int)));
  // an icon on each choice: sexagesimal time (clock) or degrees; radio
  // (dish) or optical (telescope) definition
  const char* conventionIcons[4] = {"clock", "degree", "dish", "telescope"};
  qSlicerAstroSegmentedButton* conventionButtons[2] = {d->RightAscension, d->Velocity};
  for (int i = 0; i < 4; i++)
    {
    QToolButton* choice = conventionButtons[i / 2]->button(i % 2);
    if (choice)
      {
      choice->setIcon(qSlicerAstroTheme::instance()->icon(conventionIcons[i], "text"));
      choice->setIconSize(QSize(14, 14));
      }
    }
  QToolButton* rightAscensionLabel = ConventionLabel(tr("Right ascension"), "globe",
                                                     d->RightAscension->toolTip(), this);
  d->VelocityLabel = ConventionLabel(tr("Velocity"), "spectrum", d->Velocity->toolTip(), this);
  conventions->addWidget(rightAscensionLabel, 0, 0);
  conventions->addWidget(d->RightAscension, 0, 1);
  conventions->addWidget(d->VelocityLabel, 1, 0);
  conventions->addWidget(d->Velocity, 1, 1);
  for (qSlicerAstroSegmentedButton* segmented : {d->RightAscension, d->Velocity})
    {
    segmented->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    for (int index = 0; index < segmented->count(); index++)
      {
      segmented->button(index)->setMinimumWidth(48);
      }
    }
  // the line of optical data whose spectral axis is in wavelengths (no rest
  // wavelength in the file): its velocities once the line is set
  const QString lineToolTip = tr("Line of the data: the spectral axis is in wavelengths because the file has no "
                                 "rest wavelength. Set the line to see velocities (views, readouts, modules); it "
                                 "is saved in the file (RESTFRQ).");
  d->LineLabel = ConventionLabel(tr("Line"), "spectrum", lineToolTip, this);
  d->LineRow = new QWidget(this);
  QHBoxLayout* lineLayout = new QHBoxLayout(d->LineRow);
  lineLayout->setContentsMargins(0, 0, 0, 0);
  lineLayout->setSpacing(6);
  d->Line = new QComboBox(d->LineRow);
  d->Line->setObjectName("InfoLine");
  d->Line->setToolTip(tr("Emission line (rest wavelength in vacuum)."));
  for (int index = 0; index < vtkSlicerAstroVolumeLogic::GetNumberOfOpticalLines(); index++)
    {
    const double wavelength = vtkSlicerAstroVolumeLogic::GetOpticalLineWavelength(index);
    d->Line->addItem(QString("%1 · %2 %3").arg(QString::fromStdString(vtkSlicerAstroVolumeLogic::GetOpticalLineName(index)))
                       .arg(wavelength * 1.e10, 0, 'f', 2).arg(QString::fromUtf8("Å")), wavelength);
    }
  d->Line->addItem(tr("Other wavelength…"), 0.);
  d->Line->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->Line->setMinimumContentsLength(8);
  d->LineWavelength = new QDoubleSpinBox(d->LineRow);
  d->LineWavelength->setObjectName("InfoLineWavelength");
  d->LineWavelength->setToolTip(tr("Rest wavelength of the line in vacuum, Angstrom."));
  d->LineWavelength->setRange(100., 1.e6);
  d->LineWavelength->setDecimals(2);
  d->LineWavelength->setSuffix(QString::fromUtf8(" Å"));
  d->LineWavelength->setValue(6564.61);
  d->LineWavelength->hide();
  QObject::connect(d->Line, SIGNAL(currentIndexChanged(int)), this, SLOT(onLineChanged(int)));
  d->LineApply = new QPushButton(tr("Apply"), d->LineRow);
  d->LineApply->setObjectName("InfoApplyLine");
  d->LineApply->setIcon(qSlicerAstroTheme::instance()->icon("check", "text"));
  d->LineApply->setToolTip(tr("Set this line: the spectral axis becomes an optical velocity."));
  QObject::connect(d->LineApply, SIGNAL(clicked()), this, SLOT(applyLine()));
  lineLayout->addWidget(d->Line, 1);
  lineLayout->addWidget(d->LineWavelength, 1);
  lineLayout->addWidget(d->LineApply);
  conventions->addWidget(d->LineLabel, 2, 0);
  conventions->addWidget(d->LineRow, 2, 1);
  conventions->setColumnStretch(1, 1);
  layout->addLayout(conventions);

  // beam
  QHBoxLayout* beamRow = new QHBoxLayout();
  beamRow->setSpacing(4);
  const QString beamToolTip = tr("Beam of the data (needed for fluxes in Jy and column densities; drawn in the "
                                 "slice views; saved in the FITS header).");
  QLabel* beamTitle = new QLabel(tr("Beam"), this);
  qSlicerAstroTheme::setLabelRole(beamTitle, "overline");
  beamTitle->setToolTip(beamToolTip);
  layout->addSpacing(4);
  layout->addWidget(beamTitle);
  const char* beamNames[3] = {"BMAJ", "BMIN", "BPA"};
  const QString beamUnits[3] = {QString::fromUtf8("″"), QString::fromUtf8("″"), QString::fromUtf8("°")};
  const QString beamTips[3] = {tr("Major axis (FWHM) of the beam, arc seconds."),
                               tr("Minor axis (FWHM) of the beam, arc seconds."),
                               tr("Position angle of the major axis, degrees from North to East.")};
  for (int i = 0; i < 3; i++)
    {
    d->Beam[i] = new qSlicerAstroValueField(this);
    d->Beam[i]->setObjectName(QString("InfoBeam_%1").arg(beamNames[i]));
    d->Beam[i]->setDecimals(2);
    d->Beam[i]->setRange(i == 2 ? -180. : 0., i == 2 ? 180. : 36000.);
    d->Beam[i]->setUnit(beamUnits[i]);
    d->Beam[i]->setToolTip(beamTips[i]);
    d->Beam[i]->setMinimumWidth(50);
    QObject::connect(d->Beam[i], SIGNAL(valueChanged(double)), this, SLOT(onBeamEdited()));
    beamRow->addWidget(d->Beam[i], 1);
    }
  layout->addLayout(beamRow);
  QHBoxLayout* beamButtons = new QHBoxLayout();
  beamButtons->setSpacing(6);
  d->EstimateButton = new QPushButton(qSlicerAstroTheme::instance()->icon("beam", "text"), tr("Estimate from noise"),
                                      this);
  d->EstimateButton->setObjectName("InfoEstimateBeam");
  d->EstimateButton->setToolTip(tr("Estimate the beam from the noise (spatial autocorrelation of the first and last "
                                   "channels) into the fields; then apply it."));
  d->EstimateToolTip = d->EstimateButton->toolTip();
  QObject::connect(d->EstimateButton, &QPushButton::clicked, this, [this]() { this->estimateBeam(); });
  d->ApplyButton = new QPushButton(qSlicerAstroTheme::instance()->icon("check", "text"), tr("Apply"), this);
  d->ApplyButton->setObjectName("InfoApplyBeam");
  d->ApplyButton->setToolTip(tr("Apply the beam of the fields to the data (saved in the FITS header)."));
  QObject::connect(d->ApplyButton, &QPushButton::clicked, this, [this]() { this->applyBeam(); });
  beamButtons->addWidget(d->EstimateButton, 1);
  beamButtons->addWidget(d->ApplyButton, 1);
  layout->addLayout(beamButtons);
  d->BeamStatus = new QLabel(this);
  d->BeamStatus->setObjectName("InfoBeamStatus");
  d->BeamStatus->setProperty("astroRole", "monoLabel");
  d->BeamStatus->setWordWrap(true);
  d->BeamStatus->setToolTip(tr("State of the beam."));
  layout->addWidget(d->BeamStatus);

  // FITS header
  d->HeaderToggle = new QToolButton(this);
  d->HeaderToggle->setObjectName("InfoHeaderToggle");
  d->HeaderToggle->setCheckable(true);
  d->HeaderToggle->setAutoRaise(true);
  d->HeaderToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  d->HeaderToggle->setIcon(qSlicerAstroTheme::instance()->icon("table", "muted"));
  d->HeaderToggle->setToolTip(tr("Show or hide the FITS keywords of the data."));
  QObject::connect(d->HeaderToggle, SIGNAL(toggled(bool)), this, SLOT(setHeaderVisible(bool)));
  layout->addWidget(d->HeaderToggle, 0, Qt::AlignLeft);
  d->Filter = new QLineEdit(this);
  d->Filter->setObjectName("InfoHeaderFilter");
  d->Filter->setPlaceholderText(tr("Filter keywords"));
  d->Filter->setClearButtonEnabled(true);
  d->Filter->setToolTip(tr("Show only the keywords (or values) containing this text."));
  d->Filter->addAction(qSlicerAstroTheme::instance()->icon("search", "subtle"), QLineEdit::LeadingPosition);
  QObject::connect(d->Filter, SIGNAL(textChanged(QString)), this, SLOT(setKeywordFilter(QString)));
  layout->addWidget(d->Filter);
  d->Table = new QTableWidget(this);
  d->Table->setObjectName("InfoHeaderTable");
  d->Table->setColumnCount(2);
  d->Table->setHorizontalHeaderLabels(QStringList() << tr("Keyword") << tr("Value"));
  d->Table->horizontalHeader()->setStretchLastSection(true);
  d->Table->verticalHeader()->setVisible(false);
  d->Table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  d->Table->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->Table->setMinimumHeight(180);
  d->Table->setMaximumHeight(260);
  d->Table->setMinimumWidth(80);
  d->Table->setToolTip(tr("FITS keywords of the data (as read, with the values used by SlicerAstro)."));
  layout->addWidget(d->Table);
  layout->addStretch(1);
  this->setHeaderVisible(false);
  this->setEnabled(false);
}

//-----------------------------------------------------------------------------
QString qSlicerAstroInfoSection::fileName() const
{
  Q_D(const qSlicerAstroInfoSection);
  vtkMRMLStorageNode* storage = d->Volume ? d->Volume->GetStorageNode() : nullptr;
  const char* file = storage ? storage->GetFileName() : nullptr;
  return file && *file ? QString::fromUtf8(file) : QString();
}

//-----------------------------------------------------------------------------
int qSlicerAstroInfoSection::hduNumber() const
{
  Q_D(const qSlicerAstroInfoSection);
  vtkMRMLAstroVolumeStorageNode* storage = d->Volume ?
    vtkMRMLAstroVolumeStorageNode::SafeDownCast(d->Volume->GetStorageNode()) : nullptr;
  return storage ? storage->GetHDUNumber() : 0;
}

//-----------------------------------------------------------------------------
QStringList qSlicerAstroInfoSection::extensionNames() const
{
  Q_D(const qSlicerAstroInfoSection);
  QStringList names;
  for (int index = 0; index < d->Extension->count(); index++)
    {
    names << d->Extension->itemText(index);
    }
  return names;
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::updateExtensions()
{
  Q_D(qSlicerAstroInfoSection);
  const QString file = this->fileName();
  const QSignalBlocker blocker(d->Extension);
  d->Extension->clear();
  d->FileRow->setVisible(!file.isEmpty());
  if (file.isEmpty())
    {
    return;
    }
  // the image HDUs of the file (headers only)
  QList<int> numbers;
  QStringList texts;
  qSlicerAstroSubCubeDialog::imageExtensions(file, numbers, texts);
  const int current = this->hduNumber();
  for (int index = 0; index < numbers.size(); index++)
    {
    d->Extension->addItem(texts[index], numbers[index]);
    if (numbers[index] == current || (current <= 0 && index == 0))
      {
      d->Extension->setCurrentIndex(index);
      }
    }
  // the list shows the whole descriptions, also when the box is narrower
  QAbstractItemView* view = d->Extension->view();
  view->setTextElideMode(Qt::ElideNone);
  view->setMinimumWidth(view->sizeHintForColumn(0) + 2 * view->frameWidth() + 16);
  // the choice of the extension only for files with several images
  const bool several = numbers.size() > 1;
  d->Extension->setVisible(several);
  d->ExtensionLabel->setVisible(several);
}

//-----------------------------------------------------------------------------
vtkMRMLNode* qSlicerAstroInfoSection::loadedData(const QString& file, int hdu) const
{
  Q_D(const qSlicerAstroInfoSection);
  vtkMRMLScene* scene = d->Volume ? d->Volume->GetScene() : nullptr;
  if (!scene)
    {
    return nullptr;
    }
  std::vector<vtkMRMLNode*> volumes;
  scene->GetNodesByClass("vtkMRMLAstroVolumeNode", volumes);
  const int firstHDU = d->Extension->count() > 0 ? d->Extension->itemData(0).toInt() : 1;
  for (vtkMRMLNode* node : volumes)
    {
    vtkMRMLAstroVolumeStorageNode* storage = vtkMRMLAstroVolumeStorageNode::SafeDownCast(
      vtkMRMLStorableNode::SafeDownCast(node)->GetStorageNode());
    if (!storage || !storage->GetFileName() || storage->GetUseSubExtent() ||
        QString::fromUtf8(storage->GetFileName()) != file)
      {
      continue;
      }
    const int nodeHDU = storage->GetHDUNumber() > 0 ? storage->GetHDUNumber() : firstHDU;
    if (nodeHDU == (hdu > 0 ? hdu : firstHDU))
      {
      return node;
      }
    }
  return nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLNode* qSlicerAstroInfoSection::loadExtension(int hdu)
{
  const QString file = this->fileName();
  if (file.isEmpty())
    {
    return nullptr;
    }
  vtkMRMLNode* node = this->loadedData(file, hdu);
  if (!node)
    {
    qSlicerCoreApplication* app = qSlicerCoreApplication::application();
    vtkNew<vtkCollection> loaded;
    qSlicerIO::IOProperties properties;
    properties["fileName"] = file;
    properties["hdu"] = hdu;
    if (app && app->coreIOManager() &&
        app->coreIOManager()->loadNodes("AstroVolumeFile", properties, loaded))
      {
      node = vtkMRMLNode::SafeDownCast(loaded->GetItemAsObject(0));
      }
    }
  this->activate(node);
  return node;
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::onExtensionActivated(int index)
{
  Q_D(qSlicerAstroInfoSection);
  const int hdu = d->Extension->itemData(index).toInt();
  const int current = this->hduNumber() > 0 ? this->hduNumber() :
    (d->Extension->count() > 0 ? d->Extension->itemData(0).toInt() : 0);
  if (hdu != current)
    {
    this->loadExtension(hdu);
    }
  // the section shows its data: the choice back on its extension
  this->updateExtensions();
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::activate(vtkMRMLNode* node)
{
  qSlicerCoreApplication* app = qSlicerCoreApplication::application();
  vtkMRMLSelectionNode* selection = app && app->applicationLogic() ? app->applicationLogic()->GetSelectionNode() :
                                                                      nullptr;
  if (node && selection)
    {
    selection->SetActiveVolumeID(node->GetID());
    }
}

//-----------------------------------------------------------------------------
qSlicerAstroInfoSection::~qSlicerAstroInfoSection() = default;

//-----------------------------------------------------------------------------
QString qSlicerAstroInfoSection::fact(const QString& name) const
{
  Q_D(const qSlicerAstroInfoSection);
  QLabel* label = d->Facts.value(name);
  return label ? label->text() : QString();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroInfoSection::beamStatus() const
{
  Q_D(const qSlicerAstroInfoSection);
  return d->BeamStatus->text();
}

//-----------------------------------------------------------------------------
int qSlicerAstroInfoSection::keywordRowCount() const
{
  Q_D(const qSlicerAstroInfoSection);
  return d->Table->rowCount();
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::setVolumeNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroInfoSection);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (volume && !volume->GetScene())
    {
    // removed from the scene (e.g. while the scene is closed)
    volume = nullptr;
    }
  this->qvtkReconnect(d->Volume, volume, vtkCommand::ModifiedEvent, this, SLOT(updateFromVolume()));
  this->qvtkReconnect(d->Volume, volume, vtkMRMLAstroVolumeNode::DisplayThresholdModifiedEvent,
                      this, SLOT(updateFromVolume()));
  const bool changed = volume != d->Volume.GetPointer();
  d->Volume = volume;
  if (changed)
    {
    d->BeamStatus->clear();
    }
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::updateFromVolume()
{
  Q_D(qSlicerAstroInfoSection);
  vtkMRMLAstroVolumeNode* volume = d->Volume;
  this->setEnabled(volume != nullptr);
  QMap<QString, QString> values;
  if (volume)
    {
    const QString object = Attribute(volume, "OBJECT");
    values["Object"] = object.isEmpty() ? QString::fromUtf8(volume->GetName()) : object;
    values["Telescope"] = Attribute(volume, "TELESCOP");
    QStringList dims;
    const int naxis = Attribute(volume, "NAXIS").toInt();
    for (int axis = 1; axis <= std::max(naxis, 1); axis++)
      {
      const QString size = Attribute(volume, QString("NAXIS%1").arg(axis).toUtf8().constData());
      if (!size.isEmpty())
        {
        dims << size;
        }
      }
    values["Dimensions"] = dims.join(QString::fromUtf8("×"));
    const QString unit = Attribute(volume, "BUNIT");
    values["Unit"] = qSlicerAstroTheme::displayUnit(unit);
    double beam[3] = {0., 0., 0.};
    if (vtkSlicerAstroVolumeLogic::GetBeam(volume, beam))
      {
      values["Beam"] = std::fabs(beam[0] - beam[1]) > 0.01 * beam[0] ?
        QString::fromUtf8("%1 × %2, PA %3°").arg(FormatAngle(beam[0]), FormatAngle(beam[1])).arg(beam[2], 0, 'g', 3) :
        FormatAngle(beam[0]);
      }
    const double width = vtkSlicerAstroVolumeLogic::GetChannelWidth(volume);
    if (width > 0.)
      {
      const QString spectralUnit = Attribute(volume, "CUNIT3");
      if (spectralUnit == "m")
        {
        // wavelengths (optical data without a line): Angstrom
        values["Channel width"] = QString("%1 %2").arg(width * 1.e10, 0, 'g', 3).arg(QString::fromUtf8("Å"));
        }
      else
        {
        const QString shown = spectralUnit.isEmpty() || spectralUnit.compare("m/s", Qt::CaseInsensitive) == 0 ||
          spectralUnit.compare("km/s", Qt::CaseInsensitive) == 0 ? QString("km/s") : spectralUnit;
        values["Channel width"] = QString("%1 %2").arg(width, 0, 'g', 3).arg(shown);
        }
      }
    // maps of velocities (moment 1 and 2): no noise of the sky
    const double noise = vtkSlicerAstroVolumeLogic::HasNoise(volume) ? volume->GetDisplayThreshold() : 0.;
    values["Noise (σ)"] = noise > 0. ? qSlicerAstroTheme::formatFlux(noise, unit) : QString();
    if (naxis >= 3)
      {
      const QString ctype = Attribute(volume, "CTYPE3");
      const QString specsys = Attribute(volume, "SPECSYS");
      QStringList parts;
      if (!ctype.isEmpty())
        {
        parts << ctype.section('-', 0, 0);
        }
      if (!specsys.isEmpty())
        {
        parts << specsys;
        }
      values["Spectral axis"] = parts.join(QString::fromUtf8(" · "));
      }
    const QString file = this->fileName();
    if (!file.isEmpty())
      {
      values["File"] = QFileInfo(file).fileName();
      d->Facts["File"]->setToolTip(file);
      }
    }
  this->updateExtensions();
  for (QMap<QString, QLabel*>::const_iterator it = d->Facts.constBegin(); it != d->Facts.constEnd(); ++it)
    {
    const QString value = values.value(it.key());
    it.value()->setText(volume ? (value.isEmpty() ? QString::fromUtf8("—") : value) : QString());
    }

  // conventions
  const bool cube = volume && Attribute(volume, "NAXIS").toInt() >= 3;
    {
    const QSignalBlocker raBlocker(d->RightAscension);
    const QSignalBlocker velocityBlocker(d->Velocity);
    d->RightAscension->setCurrentIndex(vtkSlicerAstroVolumeLogic::IsRightAscensionSexagesimal(volume) ? 0 : 1);
    d->Velocity->setCurrentIndex(vtkSlicerAstroVolumeLogic::IsOpticalVelocityDefinition(volume) ? 1 : 0);
    }
  // data in wavelengths (no line): the line instead of the velocity definition
  const bool wavelengths = cube && vtkSlicerAstroVolumeLogic::IsWavelengthAxis(volume);
  d->Velocity->setVisible(cube && !wavelengths);
  d->VelocityLabel->setVisible(cube && !wavelengths);
  d->LineLabel->setVisible(wavelengths);
  d->LineRow->setVisible(wavelengths);
  if (wavelengths)
    {
    this->preselectLine();
    }

  // beam: the fields keep an edit (or an estimate) not applied yet. A
  // moment map has the beam of its cube: no estimate from its pixels (they
  // are not noise of the sky)
  const QString datamodel = Attribute(volume, "DATAMODEL");
  const bool momentMap = volume && datamodel.endsWith("MOMENTMAP");
  d->EstimateButton->setEnabled(volume && !momentMap);
  d->EstimateButton->setToolTip(momentMap ?
    tr("A moment map has the beam of its cube: estimate the beam on the cube.") : d->EstimateToolTip);
  const QString status = d->BeamStatus->text();
  const bool pending = status.startsWith(tr("Beam ESTIMATED")) || status.startsWith(tr("Beam edited"));
  double beam[3] = {0., 0., 0.};
  const bool defined = vtkSlicerAstroVolumeLogic::GetBeam(volume, beam);
  if (!pending)
    {
    for (int i = 0; i < 3; i++)
      {
      const QSignalBlocker blocker(d->Beam[i]);
      d->Beam[i]->setValue(i < 2 ? beam[i] * 3600. : beam[i]);
      }
    if (volume && !defined)
      {
      d->BeamStatus->setText(momentMap ?
        tr("No beam: estimate or enter it on the cube of this map (the maps of the cube take it)") :
        tr("No beam: estimate it or enter it, then apply"));
      }
    else if (status.startsWith(tr("No beam")))
      {
      d->BeamStatus->clear();
      }
    }
  this->fillKeywords();
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::onRightAscensionFormatChanged(int index)
{
  Q_D(qSlicerAstroInfoSection);
  vtkSlicerAstroVolumeLogic::SetRightAscensionSexagesimal(d->Volume, index == 0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::onVelocityDefinitionChanged(int index)
{
  Q_D(qSlicerAstroInfoSection);
  vtkSlicerAstroVolumeLogic::SetVelocityDefinition(d->Volume, index == 1);
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::onBeamEdited()
{
  Q_D(qSlicerAstroInfoSection);
  d->BeamStatus->setText(tr("Beam edited, not applied: apply it to set it as the beam of the data"));
}

//-----------------------------------------------------------------------------
bool qSlicerAstroInfoSection::estimateBeam()
{
  Q_D(qSlicerAstroInfoSection);
  double beam[3] = {0., 0., 0.};
  QApplication::setOverrideCursor(Qt::WaitCursor);
  const bool estimated = d->Volume && vtkSlicerAstroVolumeLogic::EstimateBeamFromNoise(d->Volume, beam);
  QApplication::restoreOverrideCursor();
  if (!estimated)
    {
    d->BeamStatus->setText(tr("The beam can not be estimated: the noise is not correlated (e.g. single dish or "
                              "optical data) or the image is too small; enter it"));
    return false;
    }
  const double values[3] = {beam[0] * 3600., beam[1] * 3600., beam[2] > 90. ? beam[2] - 180. : beam[2]};
  for (int i = 0; i < 3; i++)
    {
    const QSignalBlocker blocker(d->Beam[i]);
    d->Beam[i]->setValue(values[i]);
    }
  d->BeamStatus->setText(tr("Beam ESTIMATED from the noise: check it, then apply it"));
  return true;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroInfoSection::applyBeam()
{
  Q_D(qSlicerAstroInfoSection);
  if (!d->Volume)
    {
    return false;
    }
  // the beam of the header keeps its precision when it is not changed (the
  // fields show 2 decimals)
  double current[3] = {0., 0., 0.};
  bool unchanged = vtkSlicerAstroVolumeLogic::GetBeam(d->Volume, current);
  for (int i = 0; i < 3 && unchanged; i++)
    {
    const double value = i < 2 ? current[i] * 3600. : current[i];
    unchanged = std::fabs(QString::number(value, 'f', d->Beam[i]->decimals()).toDouble() - d->Beam[i]->value()) < 1e-9;
    }
  const double beam[3] = {d->Beam[0]->value() / 3600., d->Beam[1]->value() / 3600., d->Beam[2]->value()};
  if (!unchanged && !vtkSlicerAstroVolumeLogic::SetBeam(d->Volume, beam))
    {
    d->BeamStatus->setText(tr("Enter positive major and minor axes"));
    return false;
    }
  d->BeamStatus->setText(tr("Beam applied (saved in the FITS header)"));
  this->updateFromVolume();
  return true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::setKeywordFilter(const QString& text)
{
  Q_D(qSlicerAstroInfoSection);
  if (d->Filter->text() != text)
    {
    d->Filter->setText(text);
    return;
    }
  this->fillKeywords();
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::setHeaderVisible(bool visible)
{
  Q_D(qSlicerAstroInfoSection);
    {
    const QSignalBlocker blocker(d->HeaderToggle);
    d->HeaderToggle->setChecked(visible);
    }
  d->Filter->setVisible(visible);
  d->Table->setVisible(visible);
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::fillKeywords()
{
  Q_D(qSlicerAstroInfoSection);
  QList<QPair<QString, QString> > keywords;
  if (d->Volume)
    {
    const QString prefix = QString::fromUtf8(AttributePrefix);
    for (const std::string& name : d->Volume->GetAttributeNames())
      {
      const QString key = QString::fromStdString(name);
      if (key.startsWith(prefix))
        {
        const char* value = d->Volume->GetAttribute(name.c_str());
        keywords.append(qMakePair(key.mid(prefix.size()), value ? QString::fromUtf8(value) : QString()));
        }
      }
    }
  std::sort(keywords.begin(), keywords.end());
  d->HeaderToggle->setText(tr("FITS header (%1 keywords)").arg(keywords.size()));
  const QString filter = d->Filter->text().trimmed();
  QList<QPair<QString, QString> > shown;
  for (const QPair<QString, QString>& keyword : keywords)
    {
    if (filter.isEmpty() || keyword.first.contains(filter, Qt::CaseInsensitive) ||
        keyword.second.contains(filter, Qt::CaseInsensitive))
      {
      shown.append(keyword);
      }
    }
  d->Table->setRowCount(shown.size());
  for (int row = 0; row < shown.size(); row++)
    {
    QTableWidgetItem* keyItem = new QTableWidgetItem(shown[row].first);
    QTableWidgetItem* valueItem = new QTableWidgetItem(shown[row].second);
    valueItem->setToolTip(QString("%1 = %2").arg(shown[row].first, shown[row].second));
    d->Table->setItem(row, 0, keyItem);
    d->Table->setItem(row, 1, valueItem);
    }
  d->Table->resizeColumnToContents(0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::preselectLine()
{
  Q_D(qSlicerAstroInfoSection);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(d->Volume);
  vtkMRMLAstroVolumeDisplayNode* displayNode = volume ? volume->GetAstroVolumeDisplayNode() : nullptr;
  if (!displayNode)
    {
    return;
    }
  // wavelengths of the first and last channels (m)
  const int channels = Attribute(volume, "NAXIS3").toInt();
  double ijk[3] = {0., 0., 0.};
  double first[3] = {0., 0., 0.};
  double last[3] = {0., 0., 0.};
  displayNode->GetReferenceSpace(ijk, first);
  ijk[2] = std::max(0, channels - 1);
  displayNode->GetReferenceSpace(ijk, last);
  const double minimum = std::min(first[2], last[2]);
  const double maximum = std::max(first[2], last[2]);
  // the line seen in the data at a redshift up to 0.2: Halpha first, then
  // the first of the list
  int choice = -1;
  for (int index = 0; index < vtkSlicerAstroVolumeLogic::GetNumberOfOpticalLines(); index++)
    {
    const double wavelength = vtkSlicerAstroVolumeLogic::GetOpticalLineWavelength(index);
    if (wavelength <= maximum && wavelength * 1.2 >= minimum)
      {
      const bool halpha = std::fabs(wavelength - 6564.61e-10) < 1.e-12;
      if (choice < 0 || halpha)
        {
        choice = index;
        }
      }
    }
  if (choice >= 0)
    {
    const QSignalBlocker blocker(d->Line);
    d->Line->setCurrentIndex(choice);
    d->LineWavelength->hide();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroInfoSection::onLineChanged(int index)
{
  Q_D(qSlicerAstroInfoSection);
  // "Other wavelength…": a field for the wavelength
  d->LineWavelength->setVisible(d->Line->itemData(index).toDouble() <= 0.);
}

//-----------------------------------------------------------------------------
double qSlicerAstroInfoSection::selectedLineWavelength() const
{
  Q_D(const qSlicerAstroInfoSection);
  const double wavelength = d->Line->currentData().toDouble();
  return wavelength > 0. ? wavelength : d->LineWavelength->value() * 1.e-10;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroInfoSection::applyLine()
{
  Q_D(qSlicerAstroInfoSection);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(d->Volume);
  const bool applied = vtkSlicerAstroVolumeLogic::SetRestWavelength(volume, this->selectedLineWavelength());
  this->updateFromVolume();
  if (applied)
    {
    emit this->spectralAxisChanged();
    }
  return applied;
}
