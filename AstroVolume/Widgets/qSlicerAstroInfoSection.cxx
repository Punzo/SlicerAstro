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
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLStorageNode.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "qSlicerAstroInfoSection.h"
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
QToolButton* IconButton(const QString& iconName, const QString& toolTip, const QString& objectName, QWidget* parent)
{
  QToolButton* button = new QToolButton(parent);
  button->setProperty("astroRole", "toolbar");
  button->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "muted"));
  button->setIconSize(QSize(16, 16));
  button->setToolTip(toolTip);
  button->setAutoRaise(true);
  button->setObjectName(objectName);
  return button;
}

//----------------------------------------------------------------------------
QLabel* RowLabel(const QString& text, const QString& toolTip, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  label->setProperty("astroRole", "label");
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
  QLabel* VelocityLabel{nullptr};
  qSlicerAstroValueField* Beam[3] = {nullptr, nullptr, nullptr};
  QToolButton* EstimateButton{nullptr};
  QToolButton* ApplyButton{nullptr};
  QLabel* BeamStatus{nullptr};
  QToolButton* HeaderToggle{nullptr};
  QLineEdit* Filter{nullptr};
  QTableWidget* Table{nullptr};
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
  QGridLayout* facts = new QGridLayout();
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
  layout->addLayout(facts);

  // coordinate conventions of the readouts and axes
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
  conventions->addWidget(RowLabel(tr("RA"), d->RightAscension->toolTip(), this), 0, 0);
  conventions->addWidget(d->RightAscension, 0, 1);
  d->Velocity = new qSlicerAstroSegmentedButton(this);
  d->Velocity->setObjectName("InfoVelocity");
  d->Velocity->setToolTip(tr("Velocity definition of the spectral axis in the readouts and on the axes."));
  d->Velocity->addButton(tr("radio"), tr("Radio velocities: v = c (f0 − f) / f0."));
  d->Velocity->addButton(tr("optical"), tr("Optical velocities: v = c (f0 − f) / f."));
  QObject::connect(d->Velocity, SIGNAL(currentIndexChanged(int)), this, SLOT(onVelocityDefinitionChanged(int)));
  d->VelocityLabel = RowLabel(tr("Velocity"), d->Velocity->toolTip(), this);
  conventions->addWidget(d->VelocityLabel, 1, 0);
  conventions->addWidget(d->Velocity, 1, 1);
  conventions->setColumnStretch(2, 1);
  layout->addLayout(conventions);

  // beam
  QHBoxLayout* beamRow = new QHBoxLayout();
  beamRow->setSpacing(4);
  const QString beamToolTip = tr("Beam of the data (needed for fluxes in Jy and column densities; drawn in the "
                                 "slice views; saved in the FITS header).");
  beamRow->addWidget(RowLabel(tr("Beam"), beamToolTip, this));
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
  d->EstimateButton = IconButton("beam", tr("Estimate the beam from the noise (spatial autocorrelation of the "
                                            "first and last channels, about 10% accuracy); then apply it."),
                                 "InfoEstimateBeam", this);
  QObject::connect(d->EstimateButton, &QToolButton::clicked, this, [this]() { this->estimateBeam(); });
  d->ApplyButton = IconButton("check", tr("Apply the beam of the fields to the data (saved in the FITS header)."),
                              "InfoApplyBeam", this);
  QObject::connect(d->ApplyButton, &QToolButton::clicked, this, [this]() { this->applyBeam(); });
  beamRow->addWidget(d->EstimateButton);
  beamRow->addWidget(d->ApplyButton);
  layout->addLayout(beamRow);
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
  this->setHeaderVisible(false);
  this->setEnabled(false);
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
      const QString shown = spectralUnit.isEmpty() || spectralUnit.compare("m/s", Qt::CaseInsensitive) == 0 ||
        spectralUnit.compare("km/s", Qt::CaseInsensitive) == 0 ? QString("km/s") : spectralUnit;
      values["Channel width"] = QString("%1 %2").arg(width, 0, 'g', 3).arg(shown);
      }
    const double noise = volume->GetDisplayThreshold();
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
    vtkMRMLStorageNode* storage = volume->GetStorageNode();
    if (storage && storage->GetFileName())
      {
      values["File"] = QFileInfo(QString::fromUtf8(storage->GetFileName())).fileName();
      d->Facts["File"]->setToolTip(QString::fromUtf8(storage->GetFileName()));
      }
    }
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
  d->Velocity->setVisible(cube);
  d->VelocityLabel->setVisible(cube);

  // beam: the fields keep an edit (or an estimate) not applied yet
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
      d->BeamStatus->setText(tr("No beam: estimate it or enter it, then apply"));
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
  d->BeamStatus->setText(tr("Beam ESTIMATED from the noise (about 10% accuracy): check it, then apply it"));
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
