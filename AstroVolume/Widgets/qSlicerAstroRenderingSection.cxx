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
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

// CTK includes
#include <ctkDoubleSpinBox.h>
#include <ctkSliderWidget.h>
#include <ctkUtils.h>

// VTK includes
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLVolumePropertyNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerModuleManager.h>

#include <cmath>
#include <vector>

#include "qSlicerAstroRenderingSection.h"
#include "qSlicerAstroSegmentedButton.h"
#include "qSlicerAstroTheme.h"

namespace
{
//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* AstroVolumeLogic()
{
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* module = app && app->moduleManager() ? app->moduleManager()->module("AstroVolume") : nullptr;
  return module ? vtkSlicerAstroVolumeLogic::SafeDownCast(module->logic()) : nullptr;
}

//----------------------------------------------------------------------------
QPushButton* TextButton(const QString& text, const QString& iconName, const QString& toolTip,
                        const QString& objectName, QWidget* parent)
{
  QPushButton* button = new QPushButton(text, parent);
  button->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "text"));
  button->setToolTip(toolTip);
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

//----------------------------------------------------------------------------
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}

//----------------------------------------------------------------------------
ctkSliderWidget* Slider(const QString& toolTip, const QString& objectName, QWidget* parent)
{
  ctkSliderWidget* slider = new ctkSliderWidget(parent);
  slider->setObjectName(objectName);
  slider->setToolTip(toolTip);
  slider->setTracking(true);
  // compact values (with the unit of the data)
  slider->spinBox()->setFixedWidth(118);
  slider->spinBox()->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  return slider;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroRenderingSectionPrivate
{
public:
  vtkMRMLVolumePropertyNode* preset(int index) const;
  bool isCube() const;

  vtkWeakPointer<vtkMRMLAstroVolumeNode> Volume;
  QPushButton* VisibleButton{nullptr};
  QComboBox* Presets{nullptr};
  ctkSliderWidget* Shift{nullptr};
  ctkSliderWidget* Stretch{nullptr};
  ctkSliderWidget* Opacity{nullptr};
  QPushButton* BoxButton{nullptr};
  QPushButton* CropButton{nullptr};
  QPushButton* FitButton{nullptr};
  QPushButton* NoiseButton{nullptr};
  qSlicerAstroSegmentedButton* Quality{nullptr};
  QLabel* Status{nullptr};
  QLabel* Note{nullptr};
  QWidget* Controls{nullptr};
  QString LastResult;
};

//-----------------------------------------------------------------------------
vtkMRMLVolumePropertyNode* qSlicerAstroRenderingSectionPrivate::preset(int index) const
{
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  vtkMRMLScene* presets = logic ? logic->GetPresetsScene() : nullptr;
  if (!presets || index < 0)
    {
    return nullptr;
    }
  std::vector<vtkMRMLNode*> nodes;
  presets->GetNodesByClass("vtkMRMLVolumePropertyNode", nodes);
  return index < static_cast<int>(nodes.size()) ? vtkMRMLVolumePropertyNode::SafeDownCast(nodes[index]) : nullptr;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroRenderingSectionPrivate::isCube() const
{
  const char* naxis = this->Volume ? this->Volume->GetAttribute("SlicerAstro.NAXIS") : nullptr;
  return this->Volume && this->Volume->GetImageData() && naxis && QString(naxis).toInt() >= 3;
}

//-----------------------------------------------------------------------------
qSlicerAstroRenderingSection::qSlicerAstroRenderingSection(QWidget* parent)
  : QWidget(parent)
  , d_ptr(new qSlicerAstroRenderingSectionPrivate)
{
  Q_D(qSlicerAstroRenderingSection);
  this->setObjectName("RenderingSection");
  this->setToolTip(tr("Volume rendering of the cube in the 3D views."));
  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  d->Note = new QLabel(tr("3D rendering needs a cube"), this);
  d->Note->setObjectName("RenderingNote");
  d->Note->setProperty("astroRole", "label");
  d->Note->setToolTip(tr("Images and spectra are not rendered in 3D."));
  layout->addWidget(d->Note);

  d->Controls = new QWidget(this);
  QVBoxLayout* controls = new QVBoxLayout(d->Controls);
  controls->setContentsMargins(0, 0, 0, 0);
  controls->setSpacing(10);
  layout->addWidget(d->Controls);

  // colour function, shown or hidden in 3D
  QHBoxLayout* titleRow = new QHBoxLayout();
  titleRow->addWidget(Overline(tr("Colour function"), d->Controls), 1);
  d->VisibleButton = TextButton(tr("Shown in 3D"), "eye", tr("Show or hide the cube in the 3D views."),
                                "RenderingVisible", d->Controls);
  d->VisibleButton->setCheckable(true);
  QObject::connect(d->VisibleButton, SIGNAL(toggled(bool)), this, SLOT(setRenderingVisible(bool)));
  titleRow->addWidget(d->VisibleButton);
  controls->addLayout(titleRow);
  d->Presets = new QComboBox(d->Controls);
  d->Presets->setObjectName("RenderingPreset");
  d->Presets->setIconSize(QSize(44, 44));
  d->Presets->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->Presets->setMinimumContentsLength(8);
  d->Presets->setToolTip(tr("3D color function: surfaces at multiples of the noise σ, or emission ramps, scaled "
                            "to the noise of the data. Observatory is colour-blind safe."));
  QObject::connect(d->Presets, SIGNAL(activated(int)), this, SLOT(onPresetActivated(int)));
  controls->addWidget(d->Presets);

  QGridLayout* grid = new QGridLayout();
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(10);
  grid->setVerticalSpacing(4);
  d->Shift = Slider(tr("Shift the 3D color function along the data values."), "RenderingShift", d->Controls);
  d->Stretch = Slider(tr("Stretch the 3D color function: its points move away from (or towards) its centre."),
                      "RenderingStretch", d->Controls);
  d->Opacity = Slider(tr("Shift all the opacities of the 3D color function (transparent parts stay "
                         "transparent)."), "RenderingOpacity", d->Controls);
  d->Opacity->setRange(-1., 1.);
  d->Opacity->setSingleStep(0.05);
  d->Opacity->setDecimals(2);
  QObject::connect(d->Shift, SIGNAL(valueChanged(double)), this, SLOT(setShift(double)));
  QObject::connect(d->Stretch, SIGNAL(valueChanged(double)), this, SLOT(setStretch(double)));
  QObject::connect(d->Opacity, SIGNAL(valueChanged(double)), this, SLOT(setOpacityShift(double)));
  grid->addWidget(RowLabel(tr("Shift"), d->Shift->toolTip(), d->Controls), 0, 0);
  grid->addWidget(d->Shift, 0, 1);
  grid->addWidget(RowLabel(tr("Stretch"), d->Stretch->toolTip(), d->Controls), 1, 0);
  grid->addWidget(d->Stretch, 1, 1);
  grid->addWidget(RowLabel(tr("Opacity"), d->Opacity->toolTip(), d->Controls), 2, 0);
  grid->addWidget(d->Opacity, 2, 1);
  grid->setColumnStretch(1, 1);
  controls->addLayout(grid);

  // the box: shown, crop, fit, noise of its voxels
  controls->addSpacing(4);
  controls->addWidget(Overline(tr("Box"), d->Controls));
  // two by two: fits a narrow panel
  QGridLayout* boxRow = new QGridLayout();
  boxRow->setHorizontalSpacing(6);
  boxRow->setVerticalSpacing(6);
  d->BoxButton = TextButton(tr("Show"), "eye", tr("Show or hide the box (ROI) in the views; drag its handles to "
                                                  "move it."), "RenderingBoxVisible", d->Controls);
  d->BoxButton->setCheckable(true);
  QObject::connect(d->BoxButton, SIGNAL(toggled(bool)), this, SLOT(setBoxVisible(bool)));
  d->CropButton = TextButton(tr("Crop"), "crop", tr("Render only the cube inside the box."), "RenderingCrop",
                             d->Controls);
  d->CropButton->setCheckable(true);
  QObject::connect(d->CropButton, SIGNAL(toggled(bool)), this, SLOT(setCropping(bool)));
  d->FitButton = TextButton(tr("Fit"), "roi", tr("Fit the box to the whole cube."), "RenderingFitBox", d->Controls);
  QObject::connect(d->FitButton, SIGNAL(clicked()), this, SLOT(fitBox()));
  d->NoiseButton = TextButton(tr("Noise"), "histogram",
                              tr("Noise σ of the voxels in the box (show the box and place it where there is no "
                                 "emission first): it becomes the noise of the data (color functions, contours, "
                                 "thresholds)."), "RenderingNoiseInBox", d->Controls);
  QObject::connect(d->NoiseButton, &QPushButton::clicked, this, [this]() { this->computeNoiseInBox(); });
  boxRow->addWidget(d->BoxButton, 0, 0);
  boxRow->addWidget(d->CropButton, 0, 1);
  boxRow->addWidget(d->FitButton, 1, 0);
  boxRow->addWidget(d->NoiseButton, 1, 1);
  controls->addLayout(boxRow);

  // rendering quality
  controls->addSpacing(4);
  controls->addWidget(Overline(tr("Quality"), d->Controls));
  d->Quality = new qSlicerAstroSegmentedButton(d->Controls);
  d->Quality->setObjectName("RenderingQuality");
  d->Quality->setToolTip(tr("Rendering quality of the 3D views showing the cube."));
  d->Quality->addButton(tr("Adaptive"), tr("Lower quality while the view moves (fast)."));
  d->Quality->addButton(tr("Normal"), tr("Normal quality."));
  d->Quality->addButton(tr("Maximum"), tr("Maximum quality (slower)."));
  QObject::connect(d->Quality, SIGNAL(currentIndexChanged(int)), this, SLOT(setQuality(int)));
  controls->addWidget(d->Quality);

  d->Status = new QLabel(this);
  d->Status->setObjectName("RenderingStatus");
  d->Status->setProperty("astroRole", "monoLabel");
  d->Status->setToolTip(tr("Noise σ of the data."));
  layout->addWidget(d->Status);
  this->setEnabled(false);
}

//-----------------------------------------------------------------------------
qSlicerAstroRenderingSection::~qSlicerAstroRenderingSection() = default;

//-----------------------------------------------------------------------------
QStringList qSlicerAstroRenderingSection::presetNames() const
{
  Q_D(const qSlicerAstroRenderingSection);
  QStringList names;
  for (int index = 0; index < d->Presets->count(); index++)
    {
    names << d->Presets->itemText(index);
    }
  return names;
}

//-----------------------------------------------------------------------------
int qSlicerAstroRenderingSection::currentPreset() const
{
  Q_D(const qSlicerAstroRenderingSection);
  return d->Presets->currentIndex();
}

//-----------------------------------------------------------------------------
QString qSlicerAstroRenderingSection::status() const
{
  Q_D(const qSlicerAstroRenderingSection);
  return d->Status->text();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::setVolumeNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroRenderingSection);
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (volume && !volume->GetScene())
    {
    // removed from the scene (e.g. while the scene is closed)
    volume = nullptr;
    }
  if (volume != d->Volume.GetPointer())
    {
    d->LastResult.clear();
    }
  this->qvtkReconnect(d->Volume, volume, vtkMRMLAstroVolumeNode::DisplayThresholdModifiedEvent,
                      this, SLOT(onNoiseModified()));
  this->qvtkReconnect(d->Volume, volume, vtkMRMLDisplayableNode::DisplayModifiedEvent,
                      this, SLOT(updateFromVolume()));
  d->Volume = volume;

  // the presets (names and icons)
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  vtkMRMLScene* presets = logic ? logic->GetPresetsScene() : nullptr;
  if (d->Presets->count() == 0 && presets)
    {
    std::vector<vtkMRMLNode*> nodes;
    presets->GetNodesByClass("vtkMRMLVolumePropertyNode", nodes);
    for (vtkMRMLNode* preset : nodes)
      {
      const QString name = QString::fromUtf8(preset->GetName());
      d->Presets->addItem(QIcon(QString(":/presets/%1").arg(name)), name);
      }
    }
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::updateFromVolume()
{
  Q_D(qSlicerAstroRenderingSection);
  const bool cube = d->isCube();
  this->setEnabled(cube);
  d->Note->setVisible(d->Volume && !cube);
  d->Controls->setVisible(!d->Volume || cube);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (!cube || !logic)
    {
    d->Status->clear();
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode = logic->GetVolumeRenderingDisplayNode(d->Volume, false);
    {
    const QSignalBlocker visibleBlocker(d->VisibleButton);
    const QSignalBlocker cropBlocker(d->CropButton);
    const QSignalBlocker boxBlocker(d->BoxButton);
    d->VisibleButton->setChecked(displayNode && displayNode->GetVisibility());
    d->VisibleButton->setText(displayNode && displayNode->GetVisibility() ? tr("Shown in 3D") : tr("Hidden in 3D"));
    d->CropButton->setChecked(displayNode && displayNode->GetCroppingEnabled());
    vtkMRMLMarkupsROINode* box = logic->GetVolumeRenderingROINode(d->Volume, false);
    d->BoxButton->setChecked(box && box->GetDisplayVisibility());
    }

  // the color function of the volume
  vtkMRMLNode* presetNode = d->Volume->GetPresetNode();
  const std::string presetID = presetNode ? vtkMRMLAstroVolumeNode::GetPresetIdentifier(presetNode) : std::string();
  int presetIndex = -1;
  for (int index = 0; index < d->Presets->count() && !presetID.empty(); index++)
    {
    vtkMRMLVolumePropertyNode* preset = d->preset(index);
    if (preset && vtkMRMLAstroVolumeNode::GetPresetIdentifier(preset) == presetID)
      {
      presetIndex = index;
      }
    }
    {
    const QSignalBlocker blocker(d->Presets);
    d->Presets->setCurrentIndex(presetIndex);
    }

  // shift and stretch over the range of the data
  const double maximum = QString(d->Volume->GetAttribute("SlicerAstro.DATAMAX")).toDouble();
  const double minimum = QString(d->Volume->GetAttribute("SlicerAstro.DATAMIN")).toDouble();
  const double width = std::isfinite(maximum - minimum) && maximum > minimum ? maximum - minimum : 1.;
  const double step = ctk::closestPowerOfTen(width) / 100.;
  vtkMRMLAstroVolumeDisplayNode* volumeDisplay = d->Volume->GetAstroVolumeDisplayNode();
  const QString unit = qSlicerAstroTheme::displayUnit(QString::fromUtf8(d->Volume->GetAttribute("SlicerAstro.BUNIT")));
  for (ctkSliderWidget* slider : { d->Shift, d->Stretch })
    {
    const QSignalBlocker blocker(slider);
    slider->setSuffix(unit.isEmpty() ? QString() : QString(" ") + unit);
    slider->setRange(-width, width);
    slider->setSingleStep(step);
    slider->setPageStep(step);
    slider->setDecimals(std::max(0, -static_cast<int>(std::floor(std::log10(step)))));
    }
  if (volumeDisplay)
    {
    const QSignalBlocker shiftBlocker(d->Shift);
    const QSignalBlocker stretchBlocker(d->Stretch);
    const QSignalBlocker opacityBlocker(d->Opacity);
    d->Shift->setValue(volumeDisplay->GetPresetOffset());
    d->Stretch->setValue(volumeDisplay->GetPresetStretch());
    d->Opacity->setValue(volumeDisplay->GetOpacityShift());
    }
  const bool hasColorFunction = d->Volume->GetVolumePropertyNode() != nullptr;
  for (QWidget* widget : { static_cast<QWidget*>(d->Shift), static_cast<QWidget*>(d->Stretch),
                           static_cast<QWidget*>(d->Opacity) })
    {
    widget->setEnabled(hasColorFunction);
    }
  const int quality = logic->GetVolumeRenderingQuality(d->Volume);
    {
    const QSignalBlocker blocker(d->Quality);
    d->Quality->setCurrentIndex(quality >= 0 && quality < 3 ? quality : 0);
    }
  d->Quality->setEnabled(displayNode != nullptr);
  d->FitButton->setEnabled(displayNode != nullptr);

  const double noise = d->Volume->GetDisplayThreshold();
  const QString headerUnit = QString::fromUtf8(d->Volume->GetAttribute("SlicerAstro.BUNIT"));
  QString text = noise > 0. ? QString::fromUtf8("σ = %1").arg(qSlicerAstroTheme::formatFlux(noise, headerUnit)) :
    tr("σ unknown");
  if (!d->LastResult.isEmpty())
    {
    text += QString::fromUtf8(" · ") + d->LastResult;
    }
  d->Status->setText(text);
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::setRenderingVisible(bool visible)
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (!logic || !d->isCube())
    {
    return;
    }
  // shown the first time: the default color function, cameras on the cube
  const bool created = !logic->GetVolumeRenderingDisplayNode(d->Volume, false);
  logic->SetVolumeRenderingVisibility(d->Volume, visible, created);
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::onPresetActivated(int index)
{
  this->applyPreset(index);
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::applyPreset(int index)
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  vtkMRMLVolumePropertyNode* preset = d->preset(index);
  if (!logic || !preset || !d->isCube())
    {
    return;
    }
  logic->ApplyPreset(d->Volume, preset, true);
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::onNoiseModified()
{
  Q_D(qSlicerAstroRenderingSection);
  // the color function follows the noise (its shifts start again from 0)
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic && d->isCube() && d->Volume->GetVolumePropertyNode() && d->Presets->currentIndex() >= 0)
    {
    logic->ApplyPreset(d->Volume, d->preset(d->Presets->currentIndex()), true);
    }
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::setShift(double shift)
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic && d->Volume)
    {
    logic->SetColorFunctionShift(d->Volume, shift);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::setStretch(double stretch)
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic && d->Volume)
    {
    logic->SetColorFunctionStretch(d->Volume, stretch);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::setOpacityShift(double shift)
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic && d->Volume)
    {
    logic->SetColorFunctionOpacityShift(d->Volume, shift);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::setCropping(bool crop)
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic && d->isCube())
    {
    logic->SetVolumeRenderingCropping(d->Volume, crop);
    }
  this->updateFromVolume();
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::fitBox()
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic && d->isCube())
    {
    logic->FitVolumeRenderingROI(d->Volume);
    }
}

//-----------------------------------------------------------------------------
double qSlicerAstroRenderingSection::computeNoiseInBox()
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (!logic || !d->isCube())
    {
    return 0.;
    }
  vtkMRMLMarkupsROINode* roi = logic->GetVolumeRenderingROINode(d->Volume, true);
  if (!roi)
    {
    return 0.;
    }
  QApplication::setOverrideCursor(Qt::WaitCursor);
  const double noise = logic->CalculateDisplayThresholdInROI(roi, d->Volume);
  QApplication::restoreOverrideCursor();
  d->LastResult = noise > 0. ? tr("from the box") : tr("no data in the box");
  this->updateFromVolume();
  return noise;
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::setQuality(int quality)
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic && d->Volume)
    {
    logic->SetVolumeRenderingQuality(d->Volume, quality);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroRenderingSection::setBoxVisible(bool visible)
{
  Q_D(qSlicerAstroRenderingSection);
  vtkSlicerAstroVolumeLogic* logic = AstroVolumeLogic();
  if (logic && d->isCube())
    {
    logic->SetVolumeRenderingROIVisibility(d->Volume, visible);
    }
  this->updateFromVolume();
}
