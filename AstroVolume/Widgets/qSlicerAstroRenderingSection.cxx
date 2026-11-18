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
#include <ctkSliderWidget.h>
#include <ctkUtils.h>

// VTK includes
#include <vtkWeakPointer.h>

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLViewNode.h>
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
QToolButton* IconButton(const QString& iconName, const QString& toolTip, const QString& objectName, bool checkable,
                        QWidget* parent)
{
  QToolButton* button = new QToolButton(parent);
  button->setProperty("astroRole", "toolbar");
  button->setIcon(qSlicerAstroTheme::instance()->icon(iconName, "muted"));
  button->setIconSize(QSize(16, 16));
  button->setToolTip(toolTip);
  button->setCheckable(checkable);
  button->setAutoRaise(true);
  button->setObjectName(objectName);
  return button;
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
ctkSliderWidget* Slider(const QString& toolTip, const QString& objectName, QWidget* parent)
{
  ctkSliderWidget* slider = new ctkSliderWidget(parent);
  slider->setObjectName(objectName);
  slider->setToolTip(toolTip);
  slider->setTracking(true);
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
  QToolButton* VisibleButton{nullptr};
  QComboBox* Presets{nullptr};
  ctkSliderWidget* Shift{nullptr};
  ctkSliderWidget* Stretch{nullptr};
  ctkSliderWidget* Opacity{nullptr};
  QPushButton* CropButton{nullptr};
  QToolButton* FitButton{nullptr};
  QPushButton* NoiseButton{nullptr};
  QComboBox* Quality{nullptr};
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
  QGridLayout* grid = new QGridLayout(d->Controls);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  layout->addWidget(d->Controls);

  d->VisibleButton = IconButton("eye", tr("Show or hide the cube in the 3D views."), "RenderingVisible", true,
                                d->Controls);
  QObject::connect(d->VisibleButton, SIGNAL(toggled(bool)), this, SLOT(setRenderingVisible(bool)));
  d->Presets = new QComboBox(d->Controls);
  d->Presets->setObjectName("RenderingPreset");
  d->Presets->setIconSize(QSize(40, 40));
  d->Presets->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  d->Presets->setMinimumContentsLength(8);
  d->Presets->setToolTip(tr("3D color function: surfaces at multiples of the noise σ, or emission ramps, scaled "
                            "to the noise of the data. Observatory is colour-blind safe."));
  QObject::connect(d->Presets, SIGNAL(activated(int)), this, SLOT(onPresetActivated(int)));
  grid->addWidget(RowLabel(tr("Colours"), d->Presets->toolTip(), d->Controls), 0, 0);
  QHBoxLayout* presetRow = new QHBoxLayout();
  presetRow->setSpacing(4);
  presetRow->addWidget(d->Presets, 1);
  presetRow->addWidget(d->VisibleButton);
  grid->addLayout(presetRow, 0, 1);

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
  grid->addWidget(RowLabel(tr("Shift"), d->Shift->toolTip(), d->Controls), 1, 0);
  grid->addWidget(d->Shift, 1, 1);
  grid->addWidget(RowLabel(tr("Stretch"), d->Stretch->toolTip(), d->Controls), 2, 0);
  grid->addWidget(d->Stretch, 2, 1);
  grid->addWidget(RowLabel(tr("Opacity"), d->Opacity->toolTip(), d->Controls), 3, 0);
  grid->addWidget(d->Opacity, 3, 1);

  d->Quality = new QComboBox(d->Controls);
  d->Quality->setObjectName("RenderingQuality");
  d->Quality->addItem(tr("Adaptive"));
  d->Quality->addItem(tr("Normal"));
  d->Quality->addItem(tr("Maximum"));
  d->Quality->setToolTip(tr("Rendering quality of the 3D views showing the cube: adaptive lowers it while the "
                            "view moves."));
  QObject::connect(d->Quality, SIGNAL(activated(int)), this, SLOT(setQuality(int)));
  grid->addWidget(RowLabel(tr("Quality"), d->Quality->toolTip(), d->Controls), 4, 0);
  grid->addWidget(d->Quality, 4, 1);
  grid->setColumnStretch(1, 1);

  QHBoxLayout* buttons = new QHBoxLayout();
  buttons->setSpacing(4);
  d->CropButton = TextButton(tr("Crop"), "crop", tr("Render only the cube inside a box; drag its handles in the "
                                                    "views."), "RenderingCrop", this);
  d->CropButton->setCheckable(true);
  QObject::connect(d->CropButton, SIGNAL(toggled(bool)), this, SLOT(setCropping(bool)));
  d->FitButton = IconButton("roi", tr("Fit the box to the whole cube."), "RenderingFitBox", false, this);
  QObject::connect(d->FitButton, SIGNAL(clicked()), this, SLOT(fitBox()));
  d->NoiseButton = TextButton(tr("Noise in box"), "histogram",
                              tr("Noise σ of the voxels in the box (choose a box without emission): it becomes "
                                 "the noise of the data (color functions, contours, thresholds)."),
                              "RenderingNoiseInBox", this);
  QObject::connect(d->NoiseButton, &QPushButton::clicked, this, [this]() { this->computeNoiseInBox(); });
  buttons->addWidget(d->CropButton, 1);
  buttons->addWidget(d->FitButton);
  buttons->addWidget(d->NoiseButton, 1);
  layout->addLayout(buttons);

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
  d->CropButton->setVisible(!d->Volume || cube);
  d->FitButton->setVisible(!d->Volume || cube);
  d->NoiseButton->setVisible(!d->Volume || cube);
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
    d->VisibleButton->setChecked(displayNode && displayNode->GetVisibility());
    d->CropButton->setChecked(displayNode && displayNode->GetCroppingEnabled());
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
  for (ctkSliderWidget* slider : { d->Shift, d->Stretch })
    {
    const QSignalBlocker blocker(slider);
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
    d->Quality->setCurrentIndex(quality >= 0 && quality < d->Quality->count() ? quality : 0);
    }
  d->Quality->setEnabled(displayNode != nullptr);
  d->FitButton->setEnabled(displayNode != nullptr);

  const double noise = d->Volume->GetDisplayThreshold();
  const QString unit = QString::fromUtf8(d->Volume->GetAttribute("SlicerAstro.BUNIT"));
  QString text = noise > 0. ? QString::fromUtf8("σ = %1").arg(qSlicerAstroTheme::formatFlux(noise, unit)) :
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
  const bool created = !logic->GetVolumeRenderingDisplayNode(d->Volume, false);
  vtkMRMLVolumeRenderingDisplayNode* displayNode = logic->GetVolumeRenderingDisplayNode(d->Volume, visible);
  if (!displayNode)
    {
    return;
    }
  if (visible && !d->Volume->GetVolumePropertyNode())
    {
    // the default color function, scaled to the noise
    logic->ApplyPreset(d->Volume, d->preset(vtkMRMLAstroVolumeNode::DefaultPreset), true);
    }
  displayNode->SetVisibility(visible);
  if (visible && created && d->Volume->GetScene())
    {
    // looking along the velocity axis, north up, centered on the cube
    const double directionOfProjection[3] = {0., -1., 0.};
    const double viewUp[3] = {0., 0., 1.};
    std::vector<vtkMRMLNode*> cameras;
    d->Volume->GetScene()->GetNodesByClass("vtkMRMLCameraNode", cameras);
    for (vtkMRMLNode* node : cameras)
      {
      vtkMRMLCameraNode* camera = vtkMRMLCameraNode::SafeDownCast(node);
      const char* viewID = camera ? camera->GetLayoutName() : nullptr;
      vtkMRMLViewNode* view = (viewID && d->Volume->GetScene()) ? vtkMRMLViewNode::SafeDownCast(
        d->Volume->GetScene()->GetSingletonNode(viewID, "vtkMRMLViewNode")) : nullptr;
      if (camera && (!view || displayNode->IsDisplayableInView(view->GetID())))
        {
        vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(camera, d->Volume, directionOfProjection, viewUp);
        }
      }
    }
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
  // the box is shown: the user sees (and adjusts) the region of the noise
  roi->SetDisplayVisibility(true);
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
