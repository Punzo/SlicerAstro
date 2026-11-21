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
#include <QComboBox>
#include <QGridLayout>

// CTK includes
#include <ctkFlowLayout.h>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLScene.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerModuleManager.h>

#include <cmath>
#include <limits>
#include <vector>

#include "qSlicerAstroDisplaySection.h"
#include "qSlicerAstroHistogramWidget.h"
#include "qSlicerAstroSegmentedButton.h"
#include "qSlicerAstroTheme.h"
#include "qSlicerAstroValueField.h"

namespace
{
const int HistogramBins = 180;

//----------------------------------------------------------------------------
QVector<double> ToVector(vtkDoubleArray* array)
{
  QVector<double> values(static_cast<int>(array->GetNumberOfValues()));
  for (int i = 0; i < values.size(); i++)
    {
    values[i] = array->GetValue(i);
    }
  return values;
}

const double Percentages[6] = {95., 98., 99., 99.5, 99.9, 100.};
const char* Options[3] = {"Reverse", "Inverse", "Log"};

//----------------------------------------------------------------------------
/// Pixmap of a colour table, from the lowest to the highest value
QPixmap ColourStrip(vtkMRMLColorNode* colorNode, int width = 48, int height = 12)
{
  QImage image(width, height, QImage::Format_RGB32);
  image.fill(Qt::transparent);
  vtkLookupTable* table = colorNode ? colorNode->GetLookupTable() : nullptr;
  if (table && table->GetNumberOfTableValues() > 0)
    {
    const vtkIdType count = table->GetNumberOfTableValues();
    for (int x = 0; x < width; x++)
      {
      double rgba[4];
      table->GetTableValue(std::min<vtkIdType>(count - 1, static_cast<vtkIdType>(x) * count / width), rgba);
      const QRgb colour = QColor::fromRgbF(rgba[0], rgba[1], rgba[2]).rgb();
      for (int y = 0; y < height; y++)
        {
        image.setPixel(x, y, colour);
        }
      }
    }
  return QPixmap::fromImage(image);
}

//----------------------------------------------------------------------------
QToolButton* OptionButton(const QString& text, const QString& toolTip, const QString& objectName, QWidget* parent,
                          const QString& iconName = QString())
{
  QToolButton* button = new QToolButton(parent);
  button->setProperty("astroRole", "actionChip");
  button->setText(text);
  if (!iconName.isEmpty())
    {
    button->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "muted"));
    button->setIconSize(QSize(14, 14));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
  button->setCheckable(true);
  button->setToolTip(toolTip);
  button->setObjectName(objectName);
  button->setCursor(Qt::PointingHandCursor);
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

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* AstroVolumeLogic()
{
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* module = app && app->moduleManager() ? app->moduleManager()->module("AstroVolume") : nullptr;
  return module ? vtkSlicerAstroVolumeLogic::SafeDownCast(module->logic()) : nullptr;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroDisplaySectionPrivate
{
public:
  vtkMRMLAstroVolumeDisplayNode* displayNode() const;
  double noise() const;

  vtkWeakPointer<vtkMRMLAstroVolumeNode> Volume;
  vtkWeakPointer<vtkMRMLAstroVolumeDisplayNode> ObservedDisplay;
  QComboBox* ColourTable{nullptr};
  QToolButton* OptionButtons[3]{nullptr, nullptr, nullptr};
  QToolButton* InterpolateButton{nullptr};
  QToolButton* FitSlicesButton{nullptr};
  qSlicerAstroHistogramWidget* Histogram{nullptr};
  qSlicerAstroValueField* MinimumField{nullptr};
  qSlicerAstroValueField* MaximumField{nullptr};
  qSlicerAstroValueField* NoiseField{nullptr};
  qSlicerAstroSegmentedButton* PercentageButton{nullptr};
  QLabel* WindowLabel{nullptr};
  double DataRange[2]{0., 1.};
  bool Updating{false};
  QTimer* NoiseTimer{nullptr};
  double PendingNoise{0.};
};

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode* qSlicerAstroDisplaySectionPrivate::displayNode() const
{
  return this->Volume ? this->Volume->GetAstroVolumeDisplayNode() : nullptr;
}

//-----------------------------------------------------------------------------
double qSlicerAstroDisplaySectionPrivate::noise() const
{
  // no noise for maps of velocities (moment 1 and 2)
  if (!vtkSlicerAstroVolumeLogic::HasNoise(this->Volume))
    {
    return 0.;
    }
  const double value = this->Volume->GetDisplayThreshold();
  return std::isfinite(value) && value > 0. ? value : 0.;
}

//-----------------------------------------------------------------------------
qSlicerAstroDisplaySection::qSlicerAstroDisplaySection(QWidget* parent)
  : QWidget(parent)
  , d_ptr(new qSlicerAstroDisplaySectionPrivate)
{
  Q_D(qSlicerAstroDisplaySection);
  this->setObjectName("DisplaySection");
  this->setToolTip(tr("Colours of the data in the 2D views, and the noise of the data."));
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  QGridLayout* top = new QGridLayout();
  top->setHorizontalSpacing(8);
  top->setVerticalSpacing(6);
  d->ColourTable = new QComboBox(this);
  d->ColourTable->setObjectName("DisplayColourTable");
  d->ColourTable->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->ColourTable->setMinimumContentsLength(8);
  d->ColourTable->setIconSize(QSize(48, 12));
  d->ColourTable->setToolTip(tr("Colour table of the 2D views (SlicerAstro tables and the custom ones)."));
  QObject::connect(d->ColourTable, SIGNAL(activated(int)), this, SLOT(onColorTableActivated(int)));
  top->addWidget(RowLabel(tr("Colours"), d->ColourTable->toolTip(), this), 0, 0);
  top->addWidget(d->ColourTable, 0, 1);
  // the options wrap to the width of the panel
  QWidget* optionsWidget = new QWidget(this);
  optionsWidget->setObjectName("DisplayOptions");
  ctkFlowLayout* options = new ctkFlowLayout(optionsWidget);
  options->setContentsMargins(0, 0, 0, 0);
  options->setHorizontalSpacing(4);
  options->setVerticalSpacing(4);
  options->setAlignItems(false);
  options->setPreferredExpandingDirections(Qt::Vertical);
  const QString texts[3] = {tr("Reverse"), tr("Invert"), tr("Log")};
  const QString tips[3] = {tr("Colours in reverse order (low values get the colours of the high ones)."),
                           tr("Complementary colours (e.g. black on white)."),
                           tr("Logarithmic scale of the colours (faint emission brighter).")};
  const char* names[3] = {"DisplayReverse", "DisplayInverse", "DisplayLog"};
  const char* icons[3] = {"reverse", "invert", "log"};
  for (int i = 0; i < 3; i++)
    {
    d->OptionButtons[i] = OptionButton(texts[i], tips[i], names[i], this, icons[i]);
    d->OptionButtons[i]->setProperty("colorOption", Options[i]);
    QObject::connect(d->OptionButtons[i], SIGNAL(toggled(bool)), this, SLOT(onOptionToggled(bool)));
    options->addWidget(d->OptionButtons[i]);
    }
  d->InterpolateButton = OptionButton(tr("Smooth"), tr("Interpolate between the pixels in the 2D views (off: "
                                                       "the pixels of the data)."), "DisplayInterpolate", this, "smoothing");
  d->FitSlicesButton = OptionButton(tr("Fit views"), tr("The 2D views fit their field of view to the grid of "
                                                         "the data (channel maps, position-velocity slices)."),
                                    "DisplayFitSlices", this, "expand");
  QObject::connect(d->InterpolateButton, SIGNAL(toggled(bool)), this, SLOT(onInterpolateToggled(bool)));
  QObject::connect(d->FitSlicesButton, SIGNAL(toggled(bool)), this, SLOT(onFitSlicesToggled(bool)));
  options->addWidget(d->InterpolateButton);
  options->addWidget(d->FitSlicesButton);
  top->addWidget(optionsWidget, 1, 0, 1, 2);
  top->setColumnStretch(1, 1);
  layout->addLayout(top);

  d->Histogram = new qSlicerAstroHistogramWidget(this);
  d->Histogram->setObjectName("DisplayHistogram");
  d->Histogram->setMinimumWidth(40);
  // the colour functions follow the lines while they are dragged (the 3D
  // one at most every 40 ms: it re-applies the 3D colour function)
  QObject::connect(d->Histogram, SIGNAL(windowChanging(double,double)),
                   this, SLOT(onHistogramWindowDragged(double,double)));
  QObject::connect(d->Histogram, SIGNAL(windowChanged(double,double)), this, SLOT(setWindow(double,double)));
  QObject::connect(d->Histogram, SIGNAL(noiseChanging(double)), this, SLOT(onHistogramNoiseDragged(double)));
  QObject::connect(d->Histogram, SIGNAL(noiseChanged(double)), this, SLOT(setNoise(double)));
  d->NoiseTimer = new QTimer(this);
  d->NoiseTimer->setSingleShot(true);
  d->NoiseTimer->setInterval(40);
  QObject::connect(d->NoiseTimer, SIGNAL(timeout()), this, SLOT(applyPendingNoise()));
  QObject::connect(d->Histogram, SIGNAL(viewRangeChanged(double,double)), this, SLOT(onViewRangeChanged(double,double)));
  layout->addWidget(d->Histogram);

  QGridLayout* fields = new QGridLayout();
  fields->setHorizontalSpacing(6);
  fields->setVerticalSpacing(6);
  d->MinimumField = new qSlicerAstroValueField(this);
  d->MinimumField->setObjectName("DisplayMinimum");
  d->MinimumField->setLeadingText("min");
  d->MinimumField->setToolTip(tr("Lowest value of the 2D colours (the first colour below it)."));
  d->MaximumField = new qSlicerAstroValueField(this);
  d->MaximumField->setObjectName("DisplayMaximum");
  d->MaximumField->setLeadingText("max");
  d->MaximumField->setToolTip(tr("Highest value of the 2D colours (the last colour above it)."));
  d->NoiseField = new qSlicerAstroValueField(this);
  d->NoiseField->setObjectName("DisplayNoise");
  d->NoiseField->setLeadingText(QString::fromUtf8("σ"));
  d->NoiseField->setToolTip(tr("Noise σ of the data: the 3D colour functions (surfaces at 3σ, 7σ, …), the "
                               "thresholds in σ and the contour levels in σ use it."));
  for (qSlicerAstroValueField* field : {d->MinimumField, d->MaximumField, d->NoiseField})
    {
    field->setDecimals(6);
    QObject::connect(field, SIGNAL(editingFinished()), this, SLOT(onFieldsEdited()));
    }
  fields->addWidget(d->MinimumField, 0, 0);
  fields->addWidget(d->MaximumField, 0, 1);
  fields->addWidget(d->NoiseField, 1, 0, 1, 2);
  layout->addLayout(fields);

  d->PercentageButton = new qSlicerAstroSegmentedButton(this);
  d->PercentageButton->setObjectName("DisplayPercentages");
  d->PercentageButton->setProperty("compact", true);
  d->PercentageButton->setToolTip(tr("Window of the 2D colours with a percentage of the data."));
  for (double percentage : Percentages)
    {
    d->PercentageButton->addButton(QString("%1%").arg(percentage),
      percentage < 100. ? tr("Window with %1% of the negative and of the positive values of the data (the "
                             "extreme values outside).").arg(percentage) :
                          tr("Window from the minimum to the maximum of the data."));
    }
  QObject::connect(d->PercentageButton, SIGNAL(currentIndexChanged(int)), this, SLOT(applyPercentage(int)));
  layout->addWidget(d->PercentageButton);
  d->WindowLabel = new QLabel(this);
  d->WindowLabel->setObjectName("DisplayWindowLabel");
  d->WindowLabel->setProperty("astroRole", "monoLabel");
  d->WindowLabel->setToolTip(tr("Window of the 2D colours in units of the noise."));
  layout->addWidget(d->WindowLabel);

  this->setEnabled(false);
}

//-----------------------------------------------------------------------------
qSlicerAstroDisplaySection::~qSlicerAstroDisplaySection() = default;

//-----------------------------------------------------------------------------
QList<double> qSlicerAstroDisplaySection::percentages() const
{
  QList<double> values;
  for (double percentage : Percentages)
    {
    values << percentage;
    }
  return values;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::setVolumeNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroDisplaySection);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (volume && !volume->GetScene())
    {
    // removed from the scene (e.g. while the scene is closed)
    volume = nullptr;
    }
  if (volume == d->Volume.GetPointer() && (!volume || volume->GetAstroVolumeDisplayNode() == d->ObservedDisplay.GetPointer()))
    {
    this->updateFromVolume();
    return;
    }
  this->qvtkReconnect(d->Volume, volume, vtkMRMLAstroVolumeNode::DisplayThresholdModifiedEvent,
                      this, SLOT(onVolumeModified(vtkObject*)));
  this->qvtkReconnect(d->Volume, volume, vtkCommand::ModifiedEvent, this, SLOT(onVolumeModified(vtkObject*)));
  vtkMRMLAstroVolumeDisplayNode* displayNode = volume ? volume->GetAstroVolumeDisplayNode() : nullptr;
  this->qvtkReconnect(d->ObservedDisplay, displayNode, vtkCommand::ModifiedEvent, this, SLOT(updateFromVolume()));
  d->Volume = volume;
  d->ObservedDisplay = displayNode;
  // the window of the data is not one chosen here (yet)
  d->PercentageButton->clearSelection();

  // histogram of the data (computed once per volume, and again for the
  // zoomed values)
  d->Histogram->clearHistogram();
  vtkNew<vtkDoubleArray> centres;
  vtkNew<vtkDoubleArray> counts;
  if (volume && vtkSlicerAstroVolumeLogic::ComputeHistogram(volume, HistogramBins, centres, counts))
    {
    d->Histogram->setHistogram(ToVector(centres), ToVector(counts));
    d->DataRange[0] = d->Histogram->minimumValue();
    d->DataRange[1] = d->Histogram->maximumValue();
    }
  this->setEnabled(volume != nullptr);
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onViewRangeChanged(double minimum, double maximum)
{
  Q_D(qSlicerAstroDisplaySection);
  vtkNew<vtkDoubleArray> centres;
  vtkNew<vtkDoubleArray> counts;
  const bool computed = d->Histogram->isZoomed() ?
    vtkSlicerAstroVolumeLogic::ComputeHistogramInRange(d->Volume, HistogramBins, minimum, maximum, centres, counts) :
    vtkSlicerAstroVolumeLogic::ComputeHistogram(d->Volume, HistogramBins, centres, counts);
  if (computed)
    {
    d->Histogram->setHistogram(ToVector(centres), ToVector(counts));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onVolumeModified(vtkObject* caller)
{
  Q_UNUSED(caller);
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::updateFromVolume()
{
  Q_D(qSlicerAstroDisplaySection);
  vtkMRMLAstroVolumeDisplayNode* displayNode = d->displayNode();
  if (!displayNode || d->Updating)
    {
    return;
    }
  d->Updating = true;
  const QString unit = QString::fromUtf8(d->Volume->GetAttribute("SlicerAstro.BUNIT"));
  const double noise = d->noise();
  d->Histogram->setUnit(unit);
  d->Histogram->setNoise(noise > 0. ? noise : std::numeric_limits<double>::quiet_NaN());
  const char* naxis = d->Volume->GetAttribute("SlicerAstro.NAXIS");
  const bool cube = naxis && QString(naxis).toInt() >= 3;
  d->Histogram->setNoiseEditable(cube);
  const double window = displayNode->GetWindow(), level = displayNode->GetLevel();
  const double low = level - window / 2., high = level + window / 2.;
  d->Histogram->setWindow(low, high);
  const double span = d->DataRange[1] - d->DataRange[0];
  const double values[3] = {low, high, noise};
  qSlicerAstroValueField* fields[3] = {d->MinimumField, d->MaximumField, d->NoiseField};
  for (int i = 0; i < 3; i++)
    {
    const QSignalBlocker blocker(fields[i]);
    fields[i]->setRange(d->DataRange[0] - span, d->DataRange[1] + span);
    fields[i]->setUnit(unit);
    fields[i]->setValue(values[i]);
    }
  d->NoiseField->setEnabled(cube);
  d->NoiseField->setVisible(noise > 0.);
  this->onWindowChanging(low, high);

  // colour tables with the SlicerAstro options (and the current one)
  vtkMRMLColorNode* colorNode = displayNode->GetColorNode();
  vtkMRMLScene* scene = d->Volume->GetScene();
    {
    const QSignalBlocker blocker(d->ColourTable);
    d->ColourTable->clear();
    std::vector<vtkMRMLNode*> tables;
    if (scene)
      {
      scene->GetNodesByClass("vtkMRMLColorTableNode", tables);
      }
    for (vtkMRMLNode* node : tables)
      {
      vtkMRMLColorNode* table = vtkMRMLColorNode::SafeDownCast(node);
      if (table && (vtkSlicerAstroVolumeLogic::HasColorFunctionOptions(table) || table == colorNode))
        {
        d->ColourTable->addItem(QIcon(ColourStrip(table)), QString::fromUtf8(table->GetName()),
                                QString::fromUtf8(table->GetID()));
        }
      }
    d->ColourTable->setCurrentIndex(colorNode ? d->ColourTable->findData(QString::fromUtf8(colorNode->GetID())) : -1);
    }
  const bool options = vtkSlicerAstroVolumeLogic::HasColorFunctionOptions(colorNode);
  for (int i = 0; i < 3; i++)
    {
    const QSignalBlocker blocker(d->OptionButtons[i]);
    d->OptionButtons[i]->setChecked(vtkSlicerAstroVolumeLogic::GetColorFunctionOption(colorNode, Options[i]));
    d->OptionButtons[i]->setEnabled(options);
    }
    {
    const QSignalBlocker blockInterpolate(d->InterpolateButton);
    d->InterpolateButton->setChecked(displayNode->GetInterpolate());
    const QSignalBlocker blockFit(d->FitSlicesButton);
    d->FitSlicesButton->setChecked(displayNode->GetFitSlices());
    }
  d->Updating = false;
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onWindowChanging(double minimum, double maximum)
{
  Q_D(qSlicerAstroDisplaySection);
  const double noise = d->noise();
  d->WindowLabel->setText(noise > 0. ? QString::fromUtf8("window %1σ … %2σ").arg(minimum / noise, 0, 'f', 1)
                                                                           .arg(maximum / noise, 0, 'f', 1)
                                     : QString());
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onHistogramWindowDragged(double minimum, double maximum)
{
  Q_D(qSlicerAstroDisplaySection);
  d->PercentageButton->clearSelection();
  this->onWindowChanging(minimum, maximum);
  this->setWindow(minimum, maximum);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onHistogramNoiseDragged(double noise)
{
  Q_D(qSlicerAstroDisplaySection);
  this->onNoiseChanging(noise);
  d->PendingNoise = noise;
  if (!d->NoiseTimer->isActive())
    {
    d->NoiseTimer->start();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::applyPendingNoise()
{
  Q_D(qSlicerAstroDisplaySection);
  this->setNoise(d->PendingNoise);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onNoiseChanging(double noise)
{
  Q_D(qSlicerAstroDisplaySection);
  const QSignalBlocker blocker(d->NoiseField);
  d->NoiseField->setValue(noise);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::setWindow(double minimum, double maximum)
{
  Q_D(qSlicerAstroDisplaySection);
  vtkMRMLAstroVolumeDisplayNode* displayNode = d->displayNode();
  if (!displayNode || !(maximum > minimum))
    {
    return;
    }
  int wasModifying = displayNode->StartModify();
  displayNode->SetAutoWindowLevel(0);
  displayNode->SetWindowLevel(maximum - minimum, 0.5 * (maximum + minimum));
  displayNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::setNoise(double noise)
{
  Q_D(qSlicerAstroDisplaySection);
  if (d->NoiseTimer && d->NoiseTimer->isActive() && noise != d->PendingNoise)
    {
    d->NoiseTimer->stop();
    }
  if (d->Volume && noise > 0. && std::abs(noise - d->noise()) > 1e-12 * std::max(1., noise))
    {
    d->Volume->SetDisplayThreshold(noise);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::applyPercentage(int index)
{
  Q_D(qSlicerAstroDisplaySection);
  if (!d->Volume || index < 0 || index >= 6)
    {
    return;
    }
  double window[2];
  if (vtkSlicerAstroVolumeLogic::ComputeClippedWindow(d->Volume, Percentages[index], window))
    {
    this->setWindow(window[0], window[1]);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onFieldsEdited()
{
  Q_D(qSlicerAstroDisplaySection);
  if (d->Updating)
    {
    return;
    }
  if (d->MaximumField->value() > d->MinimumField->value())
    {
    d->PercentageButton->clearSelection();
    this->setWindow(d->MinimumField->value(), d->MaximumField->value());
    }
  if (d->NoiseField->isEnabled())
    {
    this->setNoise(d->NoiseField->value());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onColorTableActivated(int index)
{
  Q_D(qSlicerAstroDisplaySection);
  vtkMRMLAstroVolumeDisplayNode* displayNode = d->displayNode();
  const QString nodeID = d->ColourTable->itemData(index).toString();
  if (displayNode && !nodeID.isEmpty())
    {
    displayNode->SetAndObserveColorNodeID(nodeID.toUtf8().constData());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onOptionToggled(bool on)
{
  Q_D(qSlicerAstroDisplaySection);
  vtkMRMLAstroVolumeDisplayNode* displayNode = d->displayNode();
  QObject* button = this->sender();
  if (!displayNode || !button)
    {
    return;
    }
  vtkSlicerAstroVolumeLogic::SetColorFunctionOption(displayNode->GetColorNode(),
    button->property("colorOption").toString().toUtf8().constData(), on);
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onInterpolateToggled(bool on)
{
  Q_D(qSlicerAstroDisplaySection);
  if (d->displayNode())
    {
    d->displayNode()->SetInterpolate(on);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroDisplaySection::onFitSlicesToggled(bool on)
{
  Q_D(qSlicerAstroDisplaySection);
  if (!d->displayNode())
    {
    return;
    }
  d->displayNode()->SetFitSlices(on);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic)
    {
    logic->FitSlicesToVolume(d->Volume);
    }
}
