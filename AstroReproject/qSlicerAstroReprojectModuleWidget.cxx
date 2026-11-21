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
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerModuleManager.h>
#include <vtkSlicerApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroReprojectParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>

// AstroVolume includes
#include <qSlicerAstroDatasetSelector.h>
#include <qSlicerAstroFoldSection.h>
#include <qSlicerAstroMessageWidget.h>
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroResultCard.h>
#include <qSlicerAstroRunButton.h>
#include <qSlicerAstroSegmentedButton.h>
#include <qSlicerAstroTheme.h>
#include <qSlicerAstroValueField.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <vtkSlicerAstroVolumeLogic.h>

// Logic includes
#include <vtkSlicerAstroReprojectLogic.h>

// AstroReproject includes
#include "qSlicerAstroReprojectModuleWidget.h"

// STD includes
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

typedef vtkMRMLAstroReprojectParametersNode Parameters;

namespace
{
//-----------------------------------------------------------------------------
QString Tr(const char* text)
{
  return qSlicerAstroReprojectModuleWidget::tr(text);
}

//-----------------------------------------------------------------------------
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}

//-----------------------------------------------------------------------------
QLabel* RowLabel(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  label->setProperty("astroRole", "label");
  return label;
}

//-----------------------------------------------------------------------------
/// A block of the panel: an overline and a grid of rows (label, field)
QGridLayout* Block(const QString& title, QWidget* parent, QVBoxLayout* into, QWidget** block = nullptr)
{
  QWidget* widget = new QWidget(parent);
  QGridLayout* grid = new QGridLayout(widget);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  grid->setColumnStretch(1, 1);
  if (!title.isEmpty())
    {
    grid->addWidget(Overline(title, widget), 0, 0, 1, 2);
    }
  into->addWidget(widget);
  if (block)
    {
    *block = widget;
    }
  return grid;
}

//-----------------------------------------------------------------------------
/// UI unit of a spectral type: (SI value per UI unit, unit)
void SpectralUnit(const QString& type, double& scale, QString& unit)
{
  if (type == "FREQ")
    {
    scale = 1.e6;
    unit = "MHz";
    }
  else if (type == "WAVE")
    {
    scale = 1.e-9;
    unit = "nm";
    }
  else
    {
    scale = 1.e3;
    unit = "km/s";
    }
}

//-----------------------------------------------------------------------------
QString VolumeDetail(vtkMRMLAstroVolumeNode* volume)
{
  if (!volume || !volume->GetImageData())
    {
    return QString();
    }
  int dims[3] = {0, 0, 0};
  volume->GetImageData()->GetDimensions(dims);
  QString text = dims[2] > 1 ? QString::fromUtf8("%1 × %2 × %3").arg(dims[0]).arg(dims[1]).arg(dims[2]) :
                               QString::fromUtf8("%1 × %2").arg(dims[0]).arg(dims[1]);
  const char* unit = volume->GetAttribute("SlicerAstro.BUNIT");
  if (unit && *unit)
    {
    text += QString::fromUtf8(" · ") + qSlicerAstroTheme::displayUnit(QString::fromUtf8(unit));
    }
  return text;
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroReproject
class qSlicerAstroReprojectModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroReprojectModuleWidget);
protected:
  qSlicerAstroReprojectModuleWidget* const q_ptr;
public:

  qSlicerAstroReprojectModuleWidgetPrivate(qSlicerAstroReprojectModuleWidget& object);
  void init();
  void buildGrid(QWidget* parent, QVBoxLayout* layout);
  void buildResampling(QWidget* parent, QVBoxLayout* layout);
  void buildResolution(QWidget* parent, QVBoxLayout* layout);
  void buildMosaic(QWidget* parent, QVBoxLayout* layout);
  void connectFields();
  vtkSlicerAstroReprojectLogic* logic() const;
  bool isModulePanel() const;
  vtkMRMLAstroVolumeNode* volume(const char* id) const;

  /// rows: a label and a field (hidden together)
  QLabel* addRow(QGridLayout* grid, const QString& label, QWidget* field, const QString& toolTip = QString());
  void setRowVisible(QWidget* field, bool visible);
  qSlicerAstroValueField* addField(QGridLayout* grid, QWidget* parent, const QString& label, const char* name,
                                   double minimum, double maximum, int decimals, const QString& unit,
                                   const QString& toolTip);
  /// A choice with the explanation of the selected option below it (inline)
  /// and the explanations of the options in their tooltips
  QComboBox* addChoice(QGridLayout* grid, QWidget* parent, const QString& label, const char* name,
                       const QStringList& items, const QStringList& data, const QString& toolTip,
                       const QStringList& explanations);
  qSlicerAstroSegmentedButton* addSegmentedChoice(QGridLayout* grid, QWidget* parent, const QString& label,
                                                  const char* name, const QStringList& items,
                                                  const QString& toolTip, const QStringList& explanations);
  QLabel* addExplanation(QGridLayout* grid, QWidget* parent, QWidget* choice, const QString& name);
  void updateExplanations();

  void updateFromNode();
  void updateVisibility();
  void updateInputs();
  void updateSummary();
  void updateResults();
  void updateRunButton();

  vtkSmartPointer<vtkMRMLAstroReprojectParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  bool Updating{false};
  bool Entered{false};
  /// inputs shown in the rows (rebuilt when they change)
  QString InputsSignature;

  QHash<QWidget*, QLabel*> RowLabels;
  QHash<QWidget*, QLabel*> Explanations;
  QHash<QWidget*, QStringList> ExplanationTexts;

  qSlicerAstroPanelHeader* Header{nullptr};
  qSlicerAstroMessageWidget* Message{nullptr};
  qSlicerAstroFoldSection* GuideFold{nullptr};
  // inputs
  qSlicerAstroDatasetSelector* InputVolumeNodeSelector{nullptr};
  QWidget* ExtraInputs{nullptr};
  QVBoxLayout* ExtraInputsLayout{nullptr};
  QToolButton* AddInputButton{nullptr};
  QMenu* AddInputMenu{nullptr};
  // sky grid
  QComboBox* SpatialGridModeComboBox{nullptr};
  qSlicerAstroDatasetSelector* ReferenceVolumeNodeSelector{nullptr};
  qSlicerAstroValueField* SpatialResampleFactorField{nullptr};
  QComboBox* CelestialFrameComboBox{nullptr};
  qSlicerAstroValueField* EquinoxField{nullptr};
  QComboBox* ProjectionComboBox{nullptr};
  qSlicerAstroValueField* CenterLongitudeField{nullptr};
  qSlicerAstroValueField* CenterLatitudeField{nullptr};
  qSlicerAstroValueField* PixelSizeXField{nullptr};
  qSlicerAstroValueField* PixelSizeYField{nullptr};
  qSlicerAstroValueField* RotationField{nullptr};
  qSlicerAstroValueField* SizeXField{nullptr};
  qSlicerAstroValueField* SizeYField{nullptr};
  QWidget* ManualGridButtons{nullptr};
  // channels
  QComboBox* SpectralGridModeComboBox{nullptr};
  qSlicerAstroValueField* SpectralResampleFactorField{nullptr};
  QComboBox* SpectralTypeComboBox{nullptr};
  QComboBox* SpectralFrameComboBox{nullptr};
  qSlicerAstroValueField* RestFrequencyField{nullptr};
  qSlicerAstroValueField* SpectralStartField{nullptr};
  qSlicerAstroValueField* SpectralStepField{nullptr};
  qSlicerAstroValueField* SpectralCountField{nullptr};
  QPushButton* SpectralFromInputButton{nullptr};
  qSlicerAstroSegmentedButton* SpectralMatchingButton{nullptr};
  QLabel* GridSummaryLabel{nullptr};
  // resampling
  QComboBox* SpatialMethodComboBox{nullptr};
  qSlicerAstroValueField* SupersamplingField{nullptr};
  QComboBox* SpectralMethodComboBox{nullptr};
  qSlicerAstroValueField* MinimumCoverageField{nullptr};
  // resolution
  qSlicerAstroFoldSection* ResolutionFold{nullptr};
  qSlicerAstroSegmentedButton* ResolutionModeButton{nullptr};
  QLabel* CommonBeamLabel{nullptr};
  qSlicerAstroValueField* TargetBeamMajorField{nullptr};
  qSlicerAstroValueField* TargetBeamMinorField{nullptr};
  qSlicerAstroValueField* TargetBeamPAField{nullptr};
  QPushButton* TargetFromCommonBeamButton{nullptr};
  // mosaic
  qSlicerAstroFoldSection* MosaicFold{nullptr};
  qSlicerAstroSegmentedButton* CombinationButton{nullptr};
  QComboBox* InputCorrectionComboBox{nullptr};
  qSlicerAstroSegmentedButton* PrimaryBeamModelButton{nullptr};
  qSlicerAstroValueField* DishDiameterField{nullptr};
  qSlicerAstroValueField* PrimaryBeamFactorField{nullptr};
  qSlicerAstroValueField* PrimaryBeamCutoffField{nullptr};
  QCheckBox* NoiseWeightingCheckBox{nullptr};
  QWidget* InputWeights{nullptr};
  QGridLayout* InputWeightsLayout{nullptr};
  QCheckBox* ComputeWeightCheckBox{nullptr};
  QCheckBox* ComputeSensitivityCheckBox{nullptr};
  QCheckBox* SaultImageCheckBox{nullptr};
  qSlicerAstroValueField* SaultFractionField{nullptr};
  // run and results
  qSlicerAstroRunButton* RunButton{nullptr};
  qSlicerAstroResultCard* ResultCard{nullptr};
  qSlicerAstroResultCard* WeightCard{nullptr};
  qSlicerAstroResultCard* SensitivityCard{nullptr};
  qSlicerAstroResultCard* SaultCard{nullptr};
};

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidgetPrivate::qSlicerAstroReprojectModuleWidgetPrivate(qSlicerAstroReprojectModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
vtkSlicerAstroReprojectLogic* qSlicerAstroReprojectModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroReprojectModuleWidget);
  return vtkSlicerAstroReprojectLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
bool qSlicerAstroReprojectModuleWidgetPrivate::isModulePanel() const
{
  Q_Q(const qSlicerAstroReprojectModuleWidget);
  return !q->property("astroEmbedded").toBool();
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroReprojectModuleWidgetPrivate::volume(const char* id) const
{
  Q_Q(const qSlicerAstroReprojectModuleWidget);
  if (!id || !*id || !q->mrmlScene())
    {
    return nullptr;
    }
  return vtkMRMLAstroVolumeNode::SafeDownCast(q->mrmlScene()->GetNodeByID(id));
}

//-----------------------------------------------------------------------------
QLabel* qSlicerAstroReprojectModuleWidgetPrivate::addRow(QGridLayout* grid, const QString& label, QWidget* field,
                                                         const QString& toolTip)
{
  const int row = grid->rowCount();
  if (label.isEmpty())
    {
    grid->addWidget(field, row, 0, 1, 2);
    return nullptr;
    }
  QLabel* rowLabel = RowLabel(label, field->parentWidget());
  rowLabel->setToolTip(toolTip.isEmpty() ? field->toolTip() : toolTip);
  grid->addWidget(rowLabel, row, 0);
  grid->addWidget(field, row, 1);
  this->RowLabels[field] = rowLabel;
  return rowLabel;
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::setRowVisible(QWidget* field, bool visible)
{
  field->setVisible(visible);
  if (this->RowLabels.contains(field))
    {
    this->RowLabels[field]->setVisible(visible);
    }
  if (this->Explanations.contains(field))
    {
    this->Explanations[field]->setVisible(visible);
    }
}

//-----------------------------------------------------------------------------
qSlicerAstroValueField* qSlicerAstroReprojectModuleWidgetPrivate::addField(QGridLayout* grid, QWidget* parent,
  const QString& label, const char* name, double minimum, double maximum, int decimals, const QString& unit,
  const QString& toolTip)
{
  qSlicerAstroValueField* field = new qSlicerAstroValueField(parent);
  field->setObjectName(name);
  field->setDecimals(decimals);
  field->setRange(minimum, maximum);
  field->setUnit(unit);
  field->setToolTip(toolTip);
  this->addRow(grid, label, field);
  return field;
}

//-----------------------------------------------------------------------------
QLabel* qSlicerAstroReprojectModuleWidgetPrivate::addExplanation(QGridLayout* grid, QWidget* parent, QWidget* choice,
                                                                 const QString& name)
{
  QLabel* explanation = RowLabel(QString(), parent);
  explanation->setObjectName(name + "Explanation");
  explanation->setWordWrap(true);
  explanation->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  explanation->setToolTip(Tr("Explanation of the selected option."));
  const int row = grid->rowCount();
  // below the choice: in the field column if the choice has a label
  if (this->RowLabels.contains(choice))
    {
    grid->addWidget(explanation, row, 1);
    }
  else
    {
    grid->addWidget(explanation, row, 0, 1, 2);
    }
  this->Explanations[choice] = explanation;
  return explanation;
}

//-----------------------------------------------------------------------------
QComboBox* qSlicerAstroReprojectModuleWidgetPrivate::addChoice(QGridLayout* grid, QWidget* parent,
  const QString& label, const char* name, const QStringList& items, const QStringList& data,
  const QString& toolTip, const QStringList& explanations)
{
  QComboBox* combo = new QComboBox(parent);
  combo->setObjectName(name);
  for (int index = 0; index < items.size(); index++)
    {
    combo->addItem(items[index], index < data.size() ? QVariant(data[index]) : QVariant(index));
    combo->setItemData(index, explanations.value(index), Qt::ToolTipRole);
    }
  combo->setToolTip(toolTip);
  this->addRow(grid, label, combo);
  QLabel* explanation = this->addExplanation(grid, parent, combo, name);
  this->ExplanationTexts[combo] = explanations;
  QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), explanation,
                   [explanation, explanations](int index) { explanation->setText(explanations.value(index)); });
  explanation->setText(explanations.value(combo->currentIndex()));
  return combo;
}

//-----------------------------------------------------------------------------
qSlicerAstroSegmentedButton* qSlicerAstroReprojectModuleWidgetPrivate::addSegmentedChoice(QGridLayout* grid,
  QWidget* parent, const QString& label, const char* name, const QStringList& items, const QString& toolTip,
  const QStringList& explanations)
{
  qSlicerAstroSegmentedButton* button = new qSlicerAstroSegmentedButton(parent);
  button->setObjectName(name);
  for (int index = 0; index < items.size(); index++)
    {
    button->addButton(items[index], explanations.value(index));
    }
  button->setToolTip(toolTip);
  button->setCurrentIndex(0);
  this->addRow(grid, label, button);
  QLabel* explanation = this->addExplanation(grid, parent, button, name);
  this->ExplanationTexts[button] = explanations;
  QObject::connect(button, &qSlicerAstroSegmentedButton::currentIndexChanged, explanation,
                   [explanation, explanations](int index) { explanation->setText(explanations.value(index)); });
  explanation->setText(explanations.value(0));
  return button;
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateExplanations()
{
  for (QHash<QWidget*, QLabel*>::const_iterator it = this->Explanations.constBegin();
       it != this->Explanations.constEnd(); ++it)
    {
    int index = 0;
    QComboBox* combo = qobject_cast<QComboBox*>(it.key());
    qSlicerAstroSegmentedButton* segmented = qobject_cast<qSlicerAstroSegmentedButton*>(it.key());
    if (combo)
      {
      index = combo->currentIndex();
      }
    else if (segmented)
      {
      index = segmented->currentIndex();
      }
    it.value()->setText(this->ExplanationTexts.value(it.key()).value(index));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroReprojectModuleWidget);
  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(Tr("Reproject"));
  this->Header->setIconName("module-reproject");
  this->Header->setToolTip(Tr("Reprojection, regridding, resampling and mosaics of data cubes and images."));
  rootLayout->addWidget(this->Header);

  QWidget* body = new QWidget(q);
  QVBoxLayout* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(12, 12, 12, 14);
  bodyLayout->setSpacing(14);
  rootLayout->addWidget(body);

  // --- quick guide
  this->GuideFold = new qSlicerAstroFoldSection(body);
  this->GuideFold->setObjectName("GuideFold");
  this->GuideFold->setTitle(Tr("Quick guide"));
  this->GuideFold->setSummary(Tr("common tasks, step by step"));
  this->GuideFold->setSettingsKey("SlicerAstro/Reproject/GuideFold");
  this->GuideFold->setCollapsed(true);
  this->GuideFold->setToolTip(Tr("How reprojection, mosaics and regridding work, step by step."));
  QLabel* guide = new QLabel(Tr(
    "<p>The module puts the <b>input data</b> on a new grid of <b>sky pixels</b> and <b>channels</b> and creates "
    "new data. The values of the output pixels are computed from the input pixels at the same sky position and "
    "frequency/velocity, after the conversion of the celestial frames (e.g. FK4 to ICRS, Galactic) and of the "
    "spectral frames (e.g. topocentric to LSRK). The summary below the channels shows the grid that will be "
    "created.</p>"
    "<p><b>Common tasks</b></p><ul>"
    "<li><b>Overlay or compare two data sets</b> (e.g. HI contours on an optical image): input = the data to move, "
    "Sky grid = <i>Grid of a reference</i>, Reference = the other data.</li>"
    "<li><b>Larger or smaller pixels</b>: Sky grid = <i>Input grid, other pixel size</i>, factor 2 = pixels twice "
    "as large. For larger pixels use <i>Area average</i> or <i>Exact overlap</i> in Resampling (flux conserved).</li>"
    "<li><b>North up, other projection or celestial frame</b>: Sky grid = <i>Manual</i>, press "
    "<i>From input, north up</i>, then change projection, frame, centre or size.</li>"
    "<li><b>Wider channels, frequency to velocity, other spectral frame</b> (e.g. topocentric to LSRK): "
    "Channels = <i>Input channels, other width</i> or <i>Manual</i>.</li>"
    "<li><b>Same resolution for several data</b>: Resolution, Beam = <i>Common</i>.</li>"
    "<li><b>Mosaic</b> of several pointings or fields: add them to the inputs (+), Sky grid = "
    "<i>Covering all the inputs</i>, Channels = <i>Covering all the inputs</i>, then the primary beam options in "
    "<i>Mosaic</i>.</li>"
    "<li><b>Different lines</b> (e.g. CO on the channels of HI): Channels = <i>Channels of the reference</i>, "
    "Match channels by = <i>Velocity</i>.</li></ul>"
    "<p>Every option has a tooltip, and the text under each choice explains the selected one.</p>"), this->GuideFold);
  guide->setObjectName("GuideLabel");
  guide->setWordWrap(true);
  guide->setTextFormat(Qt::RichText);
  guide->setProperty("astroRole", "label");
  this->GuideFold->contentLayout()->addWidget(guide);
  bodyLayout->addWidget(this->GuideFold);

  // --- inputs
  QWidget* inputs = new QWidget(body);
  QVBoxLayout* inputsLayout = new QVBoxLayout(inputs);
  inputsLayout->setContentsMargins(0, 0, 0, 0);
  inputsLayout->setSpacing(6);
  QHBoxLayout* inputsTitle = new QHBoxLayout();
  inputsTitle->setSpacing(6);
  inputsTitle->addWidget(Overline(Tr("Inputs"), inputs), 1);
  this->AddInputButton = new QToolButton(inputs);
  this->AddInputButton->setObjectName("AddInputButton");
  this->AddInputButton->setIcon(qSlicerAstroTheme::instance()->icon("plus", "text"));
  this->AddInputButton->setToolTip(Tr(
    "Add data to the inputs: several inputs are combined into one (mosaic, see Mosaic)."));
  this->AddInputButton->setPopupMode(QToolButton::InstantPopup);
  this->AddInputMenu = new QMenu(this->AddInputButton);
  this->AddInputMenu->setObjectName("AddInputMenu");
  this->AddInputButton->setMenu(this->AddInputMenu);
  this->AddInputButton->setCursor(Qt::PointingHandCursor);
  inputsTitle->addWidget(this->AddInputButton);
  inputsLayout->addLayout(inputsTitle);
  this->InputVolumeNodeSelector = new qSlicerAstroDatasetSelector(inputs);
  this->InputVolumeNodeSelector->setObjectName("InputVolumeNodeSelector");
  this->InputVolumeNodeSelector->setNoneText(Tr("Select the data to reproject"));
  this->InputVolumeNodeSelector->setToolTip(Tr(
    "The data put on the new grid ('the input' of the options below; its grid is 'the grid of the input')."));
  inputsLayout->addWidget(this->InputVolumeNodeSelector);
  this->ExtraInputs = new QWidget(inputs);
  this->ExtraInputs->setObjectName("ExtraInputs");
  this->ExtraInputsLayout = new QVBoxLayout(this->ExtraInputs);
  this->ExtraInputsLayout->setContentsMargins(0, 0, 0, 0);
  this->ExtraInputsLayout->setSpacing(4);
  inputsLayout->addWidget(this->ExtraInputs);
  bodyLayout->addWidget(inputs);

  this->Message = new qSlicerAstroMessageWidget(body);
  this->Message->setObjectName("MessageWidget");
  bodyLayout->addWidget(this->Message);

  this->buildGrid(body, bodyLayout);
  this->buildResampling(body, bodyLayout);
  this->buildResolution(body, bodyLayout);
  this->buildMosaic(body, bodyLayout);

  // --- run and results
  QFrame* footer = new QFrame(q);
  footer->setProperty("astroRole", "panelFooter");
  QVBoxLayout* footerLayout = new QVBoxLayout(footer);
  footerLayout->setContentsMargins(12, 12, 12, 12);
  footerLayout->setSpacing(8);
  this->RunButton = new qSlicerAstroRunButton(footer);
  this->RunButton->setObjectName("RunButton");
  this->RunButton->setText(Tr("Reproject"));
  this->RunButton->pushButton()->setObjectName("ApplyButton");
  this->RunButton->pushButton()->setIcon(qSlicerAstroTheme::instance()->icon("run", "on-accent"));
  this->RunButton->pushButton()->setToolTip(Tr(
    "Reproject the inputs on the output grid (and combine them for a mosaic): new data. The views and the "
    "other panels stay usable meanwhile."));
  footerLayout->addWidget(this->RunButton);
  this->ResultCard = new qSlicerAstroResultCard(footer);
  this->ResultCard->setObjectName("ResultCard");
  this->WeightCard = new qSlicerAstroResultCard(footer);
  this->WeightCard->setObjectName("WeightCard");
  this->WeightCard->setToolTip(Tr("Sum of the weights of the inputs at each pixel."));
  this->SensitivityCard = new qSlicerAstroResultCard(footer);
  this->SensitivityCard->setObjectName("SensitivityCard");
  this->SensitivityCard->setToolTip(Tr("Expected noise of the result at each pixel."));
  this->SaultCard = new qSlicerAstroResultCard(footer);
  this->SaultCard->setObjectName("SaultCard");
  this->SaultCard->setToolTip(Tr("Display version of the mosaic: flux correct above the threshold, tapered below."));
  for (qSlicerAstroResultCard* card : {this->ResultCard, this->WeightCard, this->SensitivityCard, this->SaultCard})
    {
    card->hide();
    footerLayout->addWidget(card);
    }
  rootLayout->addWidget(footer);
  rootLayout->addStretch(1);

  // --- connections
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->InputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->ReferenceVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setInputVolume(vtkMRMLNode*)));
  QObject::connect(this->AddInputMenu, &QMenu::aboutToShow, q, [this, q]()
    {
    // the data not yet inputs
    this->AddInputMenu->clear();
    const std::vector<vtkMRMLAstroVolumeNode*> current = this->logic() && this->parametersNode ?
      this->logic()->GetInputVolumes(this->parametersNode) : std::vector<vtkMRMLAstroVolumeNode*>();
    for (vtkMRMLNode* node : this->InputVolumeNodeSelector->nodes())
      {
      vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
      if (!volume || std::find(current.begin(), current.end(), volume) != current.end())
        {
        continue;
        }
      QAction* action = this->AddInputMenu->addAction(QString::fromUtf8(volume->GetName()));
      action->setToolTip(VolumeDetail(volume));
      const QString id = QString::fromUtf8(volume->GetID());
      QObject::connect(action, &QAction::triggered, q, [q, id]()
        {
        vtkMRMLScene* scene = q->mrmlScene();
        q->addInputVolume(scene ? scene->GetNodeByID(id.toUtf8().constData()) : nullptr);
        });
      }
    if (this->AddInputMenu->isEmpty())
      {
      QAction* none = this->AddInputMenu->addAction(Tr("No other data"));
      none->setEnabled(false);
      }
    });
  QObject::connect(this->RunButton, SIGNAL(clicked()), q, SLOT(onApply()));
  for (qSlicerAstroResultCard* card : {this->ResultCard, this->WeightCard, this->SensitivityCard, this->SaultCard})
    {
    QObject::connect(card, &qSlicerAstroResultCard::showRequested, q, [this, q, card]()
      {
      vtkSlicerAstroVolumeLogic* astroVolumeLogic = this->logic() ? this->logic()->GetAstroVolumeLogic() : nullptr;
      vtkMRMLAstroVolumeNode* product = vtkMRMLAstroVolumeNode::SafeDownCast(card->volume());
      vtkMRMLAstroVolumeNode* dataset = this->parametersNode ? this->volume(this->parametersNode->GetInputVolumeNodeID()) :
        nullptr;
      if (astroVolumeLogic && product)
        {
        astroVolumeLogic->ToggleProductInViews(product, dataset);
        }
      Q_UNUSED(q);
      });
    QObject::connect(card, &qSlicerAstroResultCard::deleted, q, [this]() { this->updateResults(); });
    }
  this->connectFields();
  qSlicerAstroTheme::instance()->applyTo(q);
  this->updateFromNode();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::buildGrid(QWidget* parent, QVBoxLayout* layout)
{
  Q_Q(qSlicerAstroReprojectModuleWidget);
  // --- sky grid
  QWidget* skyBlock = nullptr;
  QGridLayout* sky = Block(Tr("Output sky grid"), parent, layout, &skyBlock);
  this->SpatialGridModeComboBox = this->addChoice(sky, skyBlock, QString(), "SpatialGridModeComboBox",
    QStringList() << Tr("Grid of a reference") << Tr("Grid of the input") << Tr("Input grid, other pixel size")
                  << Tr("Covering all the inputs (mosaic)") << Tr("Area common to all the inputs") << Tr("Manual"),
    QStringList(), Tr("Pixels on the sky of the output: projection, celestial frame, centre, pixel size, rotation "
                      "and size."),
    QStringList()
    << Tr("The output has exactly the sky pixels of the Reference (projection, frame, centre, pixel size, "
          "rotation, size): use it to overlay or compare the inputs with the reference pixel by pixel.")
    << Tr("The sky pixels of the (first) input are kept: use it to change only the channels or the resolution.")
    << Tr("The sky grid of the input with pixels 'Pixel size factor' times larger (factor > 1: fewer, larger "
          "pixels) or smaller (factor < 1: finer sampling); the field of view stays the same.")
    << Tr("A new grid that contains all the inputs (their union on the sky), with the frame, projection, pixel "
          "size and rotation below: use it to mosaic several pointings or fields.")
    << Tr("A new grid that covers only the sky area where all the inputs overlap (their intersection), with "
          "the frame, projection, pixel size and rotation below.")
    << Tr("A grid defined by you: frame, projection, centre, pixel size, rotation and number of pixels. "
          "'From input' fills the fields with the grid of the first input, 'north up' also removes its rotation."));
  this->ReferenceVolumeNodeSelector = new qSlicerAstroDatasetSelector(skyBlock);
  this->ReferenceVolumeNodeSelector->setObjectName("ReferenceVolumeNodeSelector");
  this->ReferenceVolumeNodeSelector->setNoneText(Tr("Select the reference"));
  this->ReferenceVolumeNodeSelector->setToolTip(Tr(
    "Data whose grid (sky pixels and/or channels) is used for the output. Its data are not used or modified."));
  this->addRow(sky, Tr("Reference"), this->ReferenceVolumeNodeSelector);
  this->SpatialResampleFactorField = this->addField(sky, skyBlock, Tr("Pixel size factor"),
    "SpatialResampleFactorField", 0.01, 100., 3, QString::fromUtf8("×"), Tr(
    "Output pixel size / input pixel size. 2: pixels twice as large (half the pixels per axis, lower sampling; "
    "use Area average or Exact overlap). 0.5: pixels twice as small (finer sampling, no new information: the "
    "resolution is set by the beam)."));
  this->CelestialFrameComboBox = this->addChoice(sky, skyBlock, Tr("Frame"), "CelestialFrameComboBox",
    QStringList() << "ICRS" << Tr("FK5 (equatorial)") << Tr("FK4 (equatorial)") << Tr("Galactic") << Tr("Supergalactic"),
    QStringList() << "ICRS" << "FK5" << "FK4" << "GALACTIC" << "SUPERGALACTIC",
    Tr("Celestial coordinate system of the output. The inputs are converted (ERFA, as astropy)."),
    QStringList()
    << Tr("ICRS: the modern equatorial system (RA, Dec), within 0.02\" of FK5 J2000.")
    << Tr("FK5: equatorial RA, Dec at the equinox below (J2000 for most data).")
    << Tr("FK4: equatorial RA, Dec of older data (B1950), with the elliptic aberration terms.")
    << Tr("Galactic longitude and latitude (l, b).")
    << Tr("Supergalactic longitude and latitude (SGL, SGB)."));
  this->EquinoxField = this->addField(sky, skyBlock, Tr("Equinox"), "EquinoxField", 1800., 2200., 1, QString(),
    Tr("Equinox of the FK5 (Julian, e.g. 2000) or FK4 (Besselian, e.g. 1950) frame."));
  this->ProjectionComboBox = this->addChoice(sky, skyBlock, Tr("Projection"), "ProjectionComboBox",
    QStringList() << Tr("SIN (orthographic)") << Tr("TAN (gnomonic)") << Tr("ARC (zenithal equidistant)")
                  << Tr("ZEA (zenithal equal area)") << Tr("STG (stereographic)") << Tr("CAR (plate carree)")
                  << Tr("SFL (Sanson-Flamsteed)") << Tr("AIT (Hammer-Aitoff)") << Tr("MER (Mercator)"),
    QStringList() << "SIN" << "TAN" << "ARC" << "ZEA" << "STG" << "CAR" << "SFL" << "AIT" << "MER",
    Tr("Map projection of the sky on the pixels (FITS WCS, Calabretta & Greisen 2002). For fields of a few "
       "degrees or less all projections are almost identical."),
    QStringList()
    << Tr("SIN: usual for radio interferometers (and SlicerAstro default).")
    << Tr("TAN: usual for optical and infrared images.")
    << Tr("ARC: distances from the centre are preserved (e.g. single-dish surveys).")
    << Tr("ZEA: areas preserved; good for large fields.")
    << Tr("STG: angles preserved (conformal).")
    << Tr("CAR: longitude and latitude on a regular grid; all-sky and Galactic plane surveys.")
    << Tr("SFL: areas preserved, all-sky.")
    << Tr("AIT: areas preserved, all-sky maps.")
    << Tr("MER: conformal cylindrical projection."));
  this->CenterLongitudeField = this->addField(sky, skyBlock, Tr("Centre longitude"), "CenterLongitudeField",
    -360., 360., 7, QString::fromUtf8("°"),
    Tr("Sky position of the centre of the grid (RA or longitude, degrees, in the frame above)."));
  this->CenterLatitudeField = this->addField(sky, skyBlock, Tr("Centre latitude"), "CenterLatitudeField",
    -90., 90., 7, QString::fromUtf8("°"),
    Tr("Sky position of the centre of the grid (Dec or latitude, degrees, in the frame above)."));
  this->PixelSizeXField = this->addField(sky, skyBlock, Tr("Pixel size"), "PixelSizeXField", 0., 1.e6, 4,
    QString::fromUtf8("″"), Tr("Size of the output pixels on the sky (arcsec). 0: the smallest pixel of the "
                              "inputs. Sample the beam with at least 3-5 pixels across its FWHM."));
  this->PixelSizeXField->spinBox()->setSpecialValueText(Tr("smallest of the inputs"));
  this->PixelSizeYField = this->addField(sky, skyBlock, Tr("Pixel size (y)"), "PixelSizeYField", 0., 1.e6, 4,
    QString::fromUtf8("″"), Tr("Size of the output pixels along y (arcsec); usually equal to the x size."));
  this->RotationField = this->addField(sky, skyBlock, Tr("Rotation"), "RotationField", -360., 360., 4,
    QString::fromUtf8("°"), Tr("Rotation of the pixel grid on the sky (FITS CROTA2). 0: north up and east to the left."));
  this->SizeXField = this->addField(sky, skyBlock, Tr("Pixels (x)"), "SizeXField", 1., 100000., 0, Tr("px"),
    Tr("Number of pixels along x (the field of view is pixels x pixel size)."));
  this->SizeYField = this->addField(sky, skyBlock, Tr("Pixels (y)"), "SizeYField", 1., 100000., 0, Tr("px"),
    Tr("Number of pixels along y."));
  this->ManualGridButtons = new QWidget(skyBlock);
  {
  QHBoxLayout* buttons = new QHBoxLayout(this->ManualGridButtons);
  buttons->setContentsMargins(0, 0, 0, 0);
  buttons->setSpacing(6);
  QPushButton* fromInput = new QPushButton(Tr("From input"), this->ManualGridButtons);
  fromInput->setObjectName("ManualGridFromInputButton");
  fromInput->setToolTip(Tr("Fill the fields with the sky grid of the first input (starting point to edit)."));
  QPushButton* northUp = new QPushButton(qSlicerAstroTheme::instance()->icon("compass", "text"),
                                         Tr("From input, north up"), this->ManualGridButtons);
  northUp->setObjectName("ManualGridNorthUpButton");
  northUp->setToolTip(Tr("As 'From input', without rotation: north up, the grid enlarged to contain the rotated data."));
  buttons->addWidget(fromInput, 1);
  buttons->addWidget(northUp, 1);
  QObject::connect(fromInput, SIGNAL(clicked()), q, SLOT(setManualGridFromInput()));
  QObject::connect(northUp, SIGNAL(clicked()), q, SLOT(setManualGridFromInputNorthUp()));
  }
  this->addRow(sky, QString(), this->ManualGridButtons);

  // --- channels
  QWidget* spectralBlock = nullptr;
  QGridLayout* spectral = Block(Tr("Spectral axis"), parent, layout, &spectralBlock);
  this->SpectralGridModeComboBox = this->addChoice(spectral, spectralBlock, QString(), "SpectralGridModeComboBox",
    QStringList() << Tr("Channels of the input") << Tr("Channels of the reference")
                  << Tr("Input channels, other width") << Tr("Covering all the inputs")
                  << Tr("Range common to all the inputs") << Tr("Manual"),
    QStringList(), Tr("Spectral axis (channels) of the output. Ignored for 2D images."),
    QStringList()
    << Tr("The channels of the (first) input are kept (no spectral resampling).")
    << Tr("The output has the channels of the Reference: the inputs are resampled on them, after the conversion "
          "of the spectral frames (and of the lines, see 'Match channels by').")
    << Tr("The channels of the input 'Channel width factor' times wider (factor > 1: fewer, wider channels; "
          "noise averaged down with Channel overlap) or narrower.")
    << Tr("New channels covering the spectral ranges of all the inputs, with the spectral axis, frame and width "
          "below (width 0: the narrowest channel of the inputs).")
    << Tr("New channels covering only the spectral range common to all the inputs.")
    << Tr("Channels defined by you: spectral axis, frame, rest frequency, first channel, width and number. "
          "'Channels from input' fills the fields from the first input."));
  this->SpectralResampleFactorField = this->addField(spectral, spectralBlock, Tr("Channel width factor"),
    "SpectralResampleFactorField", 0.01, 1000., 3, QString::fromUtf8("×"), Tr(
    "Output channel width / input channel width. 2: channels twice as wide (half the channels, noise lower by "
    "sqrt(2) with Channel overlap). 0.5: twice as narrow."));
  this->SpectralTypeComboBox = this->addChoice(spectral, spectralBlock, Tr("Spectral axis"), "SpectralTypeComboBox",
    QStringList() << Tr("Radio velocity") << Tr("Optical velocity") << Tr("Relativistic velocity")
                  << Tr("Frequency") << Tr("Wavelength"),
    QStringList() << "VRAD" << "VOPT" << "VELO" << "FREQ" << "WAVE",
    Tr("Quantity that is linear along the output channels. SlicerAstro shows frequency axes as radio velocities "
       "and wavelength axes as optical velocities; the file keeps the chosen axis."),
    QStringList()
    << Tr("Radio velocity v = c (1 - f / f0): radio astronomy (e.g. HI, CO); channels equally spaced in frequency.")
    << Tr("Optical velocity v = c (f0 / f - 1) = cz: optical spectra; channels equally spaced in wavelength.")
    << Tr("Relativistic (apparent radial) velocity.")
    << Tr("Frequency (no rest frequency needed for the axis itself).")
    << Tr("Wavelength (vacuum)."));
  this->SpectralFrameComboBox = this->addChoice(spectral, spectralBlock, Tr("Spectral frame"), "SpectralFrameComboBox",
    QStringList() << Tr("Barycentric") << Tr("LSR kinematic") << Tr("LSR dynamic") << Tr("Topocentric")
                  << Tr("Geocentric") << Tr("Galactocentric") << Tr("Local Group") << Tr("CMB dipole"),
    QStringList() << "BARYCENT" << "LSRK" << "LSRD" << "TOPOCENT" << "GEOCENTR" << "GALACTOC" << "LOCALGRP" << "CMBDIPOL",
    Tr("Rest frame of the frequencies/velocities of the output (FITS SPECSYS). The inputs are converted from "
       "their SPECSYS (casacore conventions)."),
    QStringList()
    << Tr("Barycentric: the solar system barycentre (optical and extragalactic data; ~heliocentric).")
    << Tr("LSRK: kinematic local standard of rest (20 km/s towards RA 18h, Dec +30 deg, B1900); radio "
          "observatories and Galactic data.")
    << Tr("LSRD: dynamical local standard of rest (9, 12, 7 km/s in Galactic coordinates).")
    << Tr("Topocentric: the observatory (needs DATE-OBS/MJD-OBS and OBSGEO in the headers).")
    << Tr("Geocentric: the centre of the Earth (needs the date of the observation).")
    << Tr("Galactocentric: the centre of the Galaxy (LSRD + 220 km/s of Galactic rotation).")
    << Tr("Local Group barycentre (308 km/s).")
    << Tr("Frame of the cosmic microwave background (dipole, 369.5 km/s)."));
  this->RestFrequencyField = this->addField(spectral, spectralBlock, Tr("Rest frequency"), "RestFrequencyField",
    0., 1.e9, 6, "MHz", Tr("Rest frequency of the line, for velocity axes: HI 1420.405752 MHz, CO(1-0) "
                           "115271.2018 MHz, CO(2-1) 230538.0 MHz, CO(3-2) 345795.99 MHz."));
  this->SpectralStartField = this->addField(spectral, spectralBlock, Tr("First channel"), "SpectralStartField",
    -1.e12, 1.e12, 6, QString(), Tr("Value of the first output channel (centre), in the spectral axis and frame above."));
  this->SpectralStepField = this->addField(spectral, spectralBlock, Tr("Channel width"), "SpectralStepField",
    -1.e12, 1.e12, 6, QString(), Tr("Width of the output channels (negative: decreasing values). For 'Covering all "
                                    "the inputs': 0 = the narrowest channel of the inputs."));
  this->SpectralCountField = this->addField(spectral, spectralBlock, Tr("Channels"), "SpectralCountField",
    1., 1000000., 0, QString(), Tr("Number of output channels."));
  this->SpectralFromInputButton = new QPushButton(Tr("Channels from input"), spectralBlock);
  this->SpectralFromInputButton->setObjectName("SpectralFromInputButton");
  this->SpectralFromInputButton->setToolTip(Tr("Fill the channel fields with the spectral axis of the first input."));
  QObject::connect(this->SpectralFromInputButton, SIGNAL(clicked()), q, SLOT(setManualSpectralGridFromInput()));
  this->addRow(spectral, QString(), this->SpectralFromInputButton);
  this->SpectralMatchingButton = this->addSegmentedChoice(spectral, spectralBlock, Tr("Match by"),
    "SpectralMatchingButton", QStringList() << Tr("Frequency") << Tr("Velocity"),
    Tr("How the input channels are matched to the output channels."),
    QStringList()
    << Tr("By observed frequency: the usual choice for data of the same line.")
    << Tr("By velocity: each input is converted to velocity with its own rest frequency (as CASA imregrid). "
          "Use it to align different lines, e.g. CO on the channels of HI."));
  this->GridSummaryLabel = new QLabel(spectralBlock);
  this->GridSummaryLabel->setObjectName("GridSummaryLabel");
  this->GridSummaryLabel->setWordWrap(true);
  this->GridSummaryLabel->setProperty("astroRole", "monoLabel");
  this->GridSummaryLabel->setToolTip(Tr("The grid that will be created with the current settings."));
  this->addRow(spectral, QString(), this->GridSummaryLabel);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::buildResampling(QWidget* parent, QVBoxLayout* layout)
{
  QWidget* block = nullptr;
  QGridLayout* grid = Block(Tr("Resampling"), parent, layout, &block);
  block->setToolTip(Tr("Resampling of the values: nearest, bilinear, bicubic, flux-conserving area or exact overlap."));
  this->SpatialMethodComboBox = this->addChoice(grid, block, Tr("Sky"), "SpatialMethodComboBox",
    QStringList() << Tr("Nearest neighbour") << Tr("Bilinear") << Tr("Bicubic") << Tr("Lanczos-3")
                  << Tr("Area average (flux conserving)") << Tr("Exact overlap (flux conserving, slow)"),
    QStringList(), Tr("How the value of an output pixel is computed from the input pixels around the same sky position."),
    QStringList()
    << Tr("Value of the closest input pixel: keeps the original values (masks, labels), blocky.")
    << Tr("Linear interpolation of the 4 closest pixels: good default when the output pixels are similar to or "
          "smaller than the input pixels.")
    << Tr("Cubic convolution of the 16 closest pixels (as CASA): smoother and more accurate than bilinear for "
          "well-sampled data.")
    << Tr("Lanczos kernel on 6 x 6 pixels: the sharpest interpolation; may ring around bright compact sources.")
    << Tr("Average of the input sampled on a sub-grid of each output pixel: conserves the flux; use it when the "
          "output pixels are larger than the input pixels.")
    << Tr("Average of the input pixels weighted by their exact overlap with the output pixel on the sphere (as "
          "Montage and astropy reproject 'exact'): exact flux conservation, slowest."));
  this->SupersamplingField = this->addField(grid, block, Tr("Subpixels"), "SupersamplingField", 0., 32., 0, QString(),
    Tr("Area average: sub-grid points per axis in each output pixel (automatic: from the ratio of the pixel sizes)."));
  this->SupersamplingField->spinBox()->setSpecialValueText(Tr("automatic"));
  this->SpectralMethodComboBox = this->addChoice(grid, block, Tr("Channels"), "SpectralMethodComboBox",
    QStringList() << Tr("Nearest channel") << Tr("Linear") << Tr("Cubic") << Tr("Channel overlap (flux conserving)")
                  << Tr("Automatic (overlap for wider channels)"),
    QStringList(), Tr("How the value of an output channel is computed from the input channels."),
    QStringList()
    << Tr("Value of the closest input channel.")
    << Tr("Linear interpolation of the 2 closest channels.")
    << Tr("Cubic interpolation of the 4 closest channels.")
    << Tr("Average of the input channels weighted by their overlap with the output channel: conserves the "
          "integrated flux and averages the noise down (CASA does not average channels).")
    << Tr("Channel overlap when the output channels are wider than the input channels, cubic interpolation "
          "otherwise (recommended)."));
  this->MinimumCoverageField = this->addField(grid, block, Tr("Minimum coverage"), "MinimumCoverageField",
    0., 1., 2, QString(), Tr("Area average, exact overlap and channel overlap: output pixels/channels covered by "
                             "valid (not blank) data for less than this fraction are blank."));
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::buildResolution(QWidget* parent, QVBoxLayout* layout)
{
  Q_Q(qSlicerAstroReprojectModuleWidget);
  this->ResolutionFold = new qSlicerAstroFoldSection(parent);
  this->ResolutionFold->setObjectName("ResolutionFold");
  this->ResolutionFold->setTitle(Tr("Resolution"));
  this->ResolutionFold->setSettingsKey("SlicerAstro/Reproject/ResolutionFold");
  this->ResolutionFold->setCollapsed(true);
  this->ResolutionFold->setToolTip(Tr("Beams: convolve the inputs to a common beam or to a target beam."));
  QWidget* content = new QWidget(this->ResolutionFold);
  QGridLayout* grid = new QGridLayout(content);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  grid->setColumnStretch(1, 1);
  this->ResolutionModeButton = this->addSegmentedChoice(grid, content, Tr("Beam"), "ResolutionModeButton",
    QStringList() << Tr("Keep") << Tr("Common") << Tr("Target"),
    Tr("Angular resolution (beam) of the output. The inputs are convolved with Gaussian kernels before the "
       "resampling; Jy/beam data keep the peak of point sources, Jy/pixel data become Jy/beam."),
    QStringList()
    << Tr("No smoothing: each input keeps its beam (with several inputs of different beams the resolution of the "
          "result varies across it).")
    << Tr("Every input is smoothed to the smallest beam all the beams fit in (CASA algorithm), so the result has "
          "one resolution: needed to combine or compare data with different beams.")
    << Tr("Every input is smoothed to the beam given below, which must be larger than the beams of the data in "
          "every direction."));
  this->CommonBeamLabel = new QLabel(content);
  this->CommonBeamLabel->setObjectName("CommonBeamLabel");
  this->CommonBeamLabel->setProperty("astroRole", "monoLabel");
  this->CommonBeamLabel->setWordWrap(true);
  this->CommonBeamLabel->setToolTip(Tr("Common beam of the inputs: FWHM major x minor axis, position angle."));
  this->addRow(grid, Tr("Common beam"), this->CommonBeamLabel);
  this->TargetBeamMajorField = this->addField(grid, content, Tr("Major axis"), "TargetBeamMajorField", 0., 1.e6, 3,
    QString::fromUtf8("″"), Tr("FWHM of the major axis of the target beam (arcsec)."));
  this->TargetBeamMinorField = this->addField(grid, content, Tr("Minor axis"), "TargetBeamMinorField", 0., 1.e6, 3,
    QString::fromUtf8("″"), Tr("FWHM of the minor axis of the target beam (arcsec)."));
  this->TargetBeamPAField = this->addField(grid, content, Tr("Position angle"), "TargetBeamPAField", -180., 180., 2,
    QString::fromUtf8("°"), Tr("Position angle of the major axis, from north through east (degrees)."));
  this->TargetFromCommonBeamButton = new QPushButton(qSlicerAstroTheme::instance()->icon("beam", "text"),
                                                     Tr("Use the common beam"), content);
  this->TargetFromCommonBeamButton->setObjectName("TargetFromCommonBeamButton");
  this->TargetFromCommonBeamButton->setToolTip(Tr("Copy the common beam of the inputs into the target beam (e.g. to round it up)."));
  QObject::connect(this->TargetFromCommonBeamButton, SIGNAL(clicked()), q, SLOT(setTargetBeamFromCommonBeam()));
  this->addRow(grid, QString(), this->TargetFromCommonBeamButton);
  this->ResolutionFold->contentLayout()->addWidget(content);
  layout->addWidget(this->ResolutionFold);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::buildMosaic(QWidget* parent, QVBoxLayout* layout)
{
  this->MosaicFold = new qSlicerAstroFoldSection(parent);
  this->MosaicFold->setObjectName("MosaicFold");
  this->MosaicFold->setTitle(Tr("Mosaic"));
  this->MosaicFold->setSettingsKey("SlicerAstro/Reproject/MosaicFold");
  this->MosaicFold->setCollapsed(true);
  this->MosaicFold->setToolTip(Tr("Combination of several inputs: weights (noise, primary beam), optimal or "
                                  "primary-beam weighted mosaic, weight and coverage maps."));
  QWidget* content = new QWidget(this->MosaicFold);
  QGridLayout* grid = new QGridLayout(content);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  grid->setColumnStretch(1, 1);
  QLabel* note = new QLabel(Tr(
    "Used with several inputs: where they overlap, the output is a weighted combination (linear mosaic, Sault, "
    "Staveley-Smith & Brouw 1996; CASA linearmosaic, Miriad linmos)."), content);
  note->setWordWrap(true);
  note->setProperty("astroRole", "note");
  note->setToolTip(Tr("How the inputs are combined."));
  this->addRow(grid, QString(), note);
  this->CombinationButton = this->addSegmentedChoice(grid, content, Tr("Weights"), "CombinationButton",
    QStringList() << Tr("Mean") << Tr("Optimal") << Tr("PB weighted"),
    Tr("Weights of the inputs where they overlap. A = primary beam, w = 1 or 1 / noise^2."),
    QStringList()
    << Tr("Plain average of the inputs covering each pixel (no primary beam): for data already on a common flux "
          "scale with uniform noise, e.g. adjacent fields.")
    << Tr("sum(A I w) / sum(A^2 w): the lowest noise for interferometer pointings (Sault et al. 1996).")
    << Tr("sum(I w) / sum(A w): weights proportional to the primary beam."));
  this->InputCorrectionComboBox = this->addChoice(grid, content, Tr("The inputs are"), "InputCorrectionComboBox",
    QStringList() << Tr("Primary beam corrected") << Tr("Not corrected (flat noise)") << Tr("Multiplied by the primary beam"),
    QStringList(), Tr("Primary beam state of the input images."),
    QStringList()
    << Tr("Divided by the primary beam (flux correct, noise rising towards the edges): e.g. CASA .pbcor images.")
    << Tr("Not divided by the primary beam (uniform noise, sources attenuated away from the centre): e.g. CASA "
          ".image products.")
    << Tr("Already multiplied by the primary beam (A I)."));
  this->PrimaryBeamModelButton = this->addSegmentedChoice(grid, content, Tr("Primary beam"), "PrimaryBeamModelButton",
    QStringList() << Tr("None") << Tr("Gaussian") << Tr("Airy") << Tr("Data"),
    Tr("Response of the antennas around each pointing centre (OBSRA/OBSDEC of the header, otherwise the reference "
       "position CRVAL), at the frequency of each channel."),
    QStringList()
    << Tr("No primary beam (A = 1): only the mean combination is meaningful.")
    << Tr("Gaussian with FWHM = factor x wavelength / dish diameter.")
    << Tr("Airy pattern of a uniformly illuminated dish of the given diameter.")
    << Tr("Primary beam data (values 0-1) chosen for each input below."));
  this->DishDiameterField = this->addField(grid, content, Tr("Dish diameter"), "DishDiameterField", 0., 1000., 2, "m",
    Tr("Diameter of the antennas: VLA and WSRT 25 m, ALMA 12 m, ATCA 22 m, MeerKAT 13.5 m."));
  this->PrimaryBeamFactorField = this->addField(grid, content, Tr("FWHM"), "PrimaryBeamFactorField", 0.1, 5., 3,
    QString::fromUtf8("λ/D"), Tr("FWHM of the Gaussian primary beam in units of wavelength / dish diameter (1.13 for "
                               "ALMA, about 1.02-1.2 for other arrays, depending on the illumination)."));
  this->PrimaryBeamCutoffField = this->addField(grid, content, Tr("Cut below PB"), "PrimaryBeamCutoffField", 0., 1., 3,
    QString(), Tr("An input is not used where its primary beam is below this level (e.g. 0.2: 20% of the peak response)."));
  this->NoiseWeightingCheckBox = new QCheckBox(Tr("Weights 1 / noise² (Miriad linmos)"), content);
  this->NoiseWeightingCheckBox->setObjectName("NoiseWeightingCheckBox");
  this->NoiseWeightingCheckBox->setToolTip(Tr(
    "Weight each input by 1 / noise^2 (the noise of each input below): noisier inputs count less."));
  this->addRow(grid, QString(), this->NoiseWeightingCheckBox);
  this->InputWeights = new QWidget(content);
  this->InputWeights->setObjectName("InputWeights");
  this->InputWeights->setToolTip(Tr(
    "Per input: the noise (data units; auto: the noise estimated by SlicerAstro when the data were loaded) used by "
    "the noise weights and the sensitivity output, and the primary beam data (with Primary beam = Data)."));
  this->InputWeightsLayout = new QGridLayout(this->InputWeights);
  this->InputWeightsLayout->setContentsMargins(0, 0, 0, 0);
  this->InputWeightsLayout->setHorizontalSpacing(6);
  this->InputWeightsLayout->setVerticalSpacing(4);
  this->InputWeightsLayout->setColumnStretch(0, 1);
  this->addRow(grid, QString(), this->InputWeights);
  QLabel* outputs = Overline(Tr("Outputs"), content);
  outputs->setToolTip(Tr("Volumes made with the mosaic."));
  this->addRow(grid, QString(), outputs);
  this->ComputeWeightCheckBox = new QCheckBox(Tr("Weight map"), content);
  this->ComputeWeightCheckBox->setObjectName("ComputeWeightCheckBox");
  this->ComputeWeightCheckBox->setToolTip(Tr(
    "Also create a volume with the sum of the weights at each pixel (where and how much the inputs contribute)."));
  this->addRow(grid, QString(), this->ComputeWeightCheckBox);
  this->ComputeSensitivityCheckBox = new QCheckBox(Tr("Coverage (sensitivity)"), content);
  this->ComputeSensitivityCheckBox->setObjectName("ComputeSensitivityCheckBox");
  this->ComputeSensitivityCheckBox->setToolTip(Tr(
    "Also create a volume with the expected noise of the result at each pixel (needs the noise of every input)."));
  this->addRow(grid, QString(), this->ComputeSensitivityCheckBox);
  this->SaultImageCheckBox = new QCheckBox(Tr("Sault-weighted"), content);
  this->SaultImageCheckBox->setObjectName("SaultImageCheckBox");
  this->SaultImageCheckBox->setToolTip(Tr(
    "Also create a display version of the mosaic: flux correct where the weight is above the threshold, tapered "
    "towards the edges (uniform noise) below it (Sault et al. 1996)."));
  this->addRow(grid, QString(), this->SaultImageCheckBox);
  this->SaultFractionField = this->addField(grid, content, Tr("Sault threshold"), "SaultFractionField", 0., 1., 3,
    QString(), Tr("Fraction of the peak weight above which the Sault volume is flux correct."));
  this->MosaicFold->contentLayout()->addWidget(content);
  layout->addWidget(this->MosaicFold);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::connectFields()
{
  Q_Q(qSlicerAstroReprojectModuleWidget);
  std::function<bool()> ready = [this]() { return this->parametersNode && !this->Updating; };
  typedef void (Parameters::*IntSetter)(int);
  typedef void (Parameters::*DoubleSetter)(double);
  typedef void (Parameters::*StringSetter)(const char*);
  std::function<void(QComboBox*, IntSetter)> connectIndex = [this, q, ready](QComboBox* combo, IntSetter setter)
    {
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this, ready, combo, setter](int)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(combo->currentData().toInt()); } });
    };
  std::function<void(QComboBox*, StringSetter)> connectString = [this, q, ready](QComboBox* combo, StringSetter setter)
    {
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this, ready, combo, setter](int)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(combo->currentData().toString().toUtf8().constData()); } });
    };
  std::function<void(qSlicerAstroSegmentedButton*, IntSetter)> connectSegmented =
    [this, q, ready](qSlicerAstroSegmentedButton* button, IntSetter setter)
    {
    QObject::connect(button, &qSlicerAstroSegmentedButton::currentIndexChanged, q, [this, ready, setter](int index)
      { if (ready() && index >= 0) { (this->parametersNode.GetPointer()->*setter)(index); } });
    };
  std::function<void(qSlicerAstroValueField*, DoubleSetter, std::function<double()>)> connectDouble =
    [this, q, ready](qSlicerAstroValueField* field, DoubleSetter setter, std::function<double()> scale)
    {
    QObject::connect(field, &qSlicerAstroValueField::valueChanged, q, [this, ready, setter, scale](double value)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(value * (scale ? scale() : 1.)); } });
    };
  std::function<void(qSlicerAstroValueField*, IntSetter)> connectInt =
    [this, q, ready](qSlicerAstroValueField* field, IntSetter setter)
    {
    QObject::connect(field, &qSlicerAstroValueField::valueChanged, q, [this, ready, setter](double value)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(static_cast<int>(std::lround(value))); } });
    };
  std::function<void(QCheckBox*, IntSetter)> connectCheck = [this, q, ready](QCheckBox* box, IntSetter setter)
    {
    QObject::connect(box, &QCheckBox::toggled, q, [this, ready, setter](bool checked)
      { if (ready()) { (this->parametersNode.GetPointer()->*setter)(checked ? 1 : 0); } });
    };
  // SI value per UI unit of the spectral fields
  std::function<double()> spectralScale = [this]()
    {
    double scale = 1.;
    QString unit;
    SpectralUnit(this->SpectralTypeComboBox->currentData().toString(), scale, unit);
    return scale;
    };
  QObject::connect(this->ReferenceVolumeNodeSelector, &qSlicerAstroDatasetSelector::currentNodeChanged, q,
    [this, ready](vtkMRMLNode* node)
    {
    if (ready())
      {
      this->parametersNode->SetReferenceVolumeNodeID(node ? node->GetID() : nullptr);
      }
    });
  connectIndex(this->SpatialGridModeComboBox, &Parameters::SetSpatialGridMode);
  connectDouble(this->SpatialResampleFactorField, &Parameters::SetSpatialResampleFactor, nullptr);
  connectString(this->CelestialFrameComboBox, &Parameters::SetCelestialFrame);
  connectDouble(this->EquinoxField, &Parameters::SetEquinox, nullptr);
  connectString(this->ProjectionComboBox, &Parameters::SetProjection);
  connectDouble(this->CenterLongitudeField, &Parameters::SetCenterLongitude, nullptr);
  connectDouble(this->CenterLatitudeField, &Parameters::SetCenterLatitude, nullptr);
  connectDouble(this->PixelSizeXField, &Parameters::SetPixelSizeX, nullptr);
  connectDouble(this->PixelSizeYField, &Parameters::SetPixelSizeY, nullptr);
  connectDouble(this->RotationField, &Parameters::SetRotation, nullptr);
  connectInt(this->SizeXField, &Parameters::SetSizeX);
  connectInt(this->SizeYField, &Parameters::SetSizeY);
  connectIndex(this->SpectralGridModeComboBox, &Parameters::SetSpectralGridMode);
  // switching to a manual grid: the fields start from the grid of the first input
  QObject::connect(this->SpatialGridModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), q,
    [this, ready](int)
    {
    if (ready() && this->parametersNode->GetSpatialGridMode() == Parameters::SpatialManual)
      {
      this->logic()->SetManualGridFromInput(this->parametersNode, true, false, false);
      }
    });
  QObject::connect(this->SpectralGridModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), q,
    [this, ready](int)
    {
    if (ready() && this->parametersNode->GetSpectralGridMode() == Parameters::SpectralManual)
      {
      this->logic()->SetManualGridFromInput(this->parametersNode, false, true, false);
      }
    });
  connectDouble(this->SpectralResampleFactorField, &Parameters::SetSpectralResampleFactor, nullptr);
  connectString(this->SpectralTypeComboBox, &Parameters::SetSpectralType);
  connectString(this->SpectralFrameComboBox, &Parameters::SetSpectralFrame);
  connectDouble(this->RestFrequencyField, &Parameters::SetRestFrequency, []() { return 1.e6; });
  connectDouble(this->SpectralStartField, &Parameters::SetSpectralStart, spectralScale);
  connectDouble(this->SpectralStepField, &Parameters::SetSpectralStep, spectralScale);
  connectInt(this->SpectralCountField, &Parameters::SetSpectralCount);
  connectSegmented(this->SpectralMatchingButton, &Parameters::SetSpectralMatching);
  connectIndex(this->SpatialMethodComboBox, &Parameters::SetInterpolationOrder);
  connectInt(this->SupersamplingField, &Parameters::SetSupersampling);
  connectIndex(this->SpectralMethodComboBox, &Parameters::SetSpectralMethod);
  connectDouble(this->MinimumCoverageField, &Parameters::SetMinimumCoverage, nullptr);
  connectSegmented(this->ResolutionModeButton, &Parameters::SetResolutionMode);
  connectDouble(this->TargetBeamMajorField, &Parameters::SetTargetBeamMajor, nullptr);
  connectDouble(this->TargetBeamMinorField, &Parameters::SetTargetBeamMinor, nullptr);
  connectDouble(this->TargetBeamPAField, &Parameters::SetTargetBeamPA, nullptr);
  connectSegmented(this->CombinationButton, &Parameters::SetCombination);
  connectIndex(this->InputCorrectionComboBox, &Parameters::SetInputCorrection);
  connectSegmented(this->PrimaryBeamModelButton, &Parameters::SetPrimaryBeamModel);
  connectDouble(this->DishDiameterField, &Parameters::SetDishDiameter, nullptr);
  connectDouble(this->PrimaryBeamFactorField, &Parameters::SetPrimaryBeamFactor, nullptr);
  connectDouble(this->PrimaryBeamCutoffField, &Parameters::SetPrimaryBeamCutoff, nullptr);
  connectCheck(this->NoiseWeightingCheckBox, &Parameters::SetNoiseWeighting);
  connectCheck(this->ComputeWeightCheckBox, &Parameters::SetComputeWeight);
  connectCheck(this->ComputeSensitivityCheckBox, &Parameters::SetComputeSensitivity);
  connectCheck(this->SaultImageCheckBox, &Parameters::SetSaultImage);
  connectDouble(this->SaultFractionField, &Parameters::SetSaultFraction, nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateFromNode()
{
  Parameters* p = this->parametersNode;
  QWidget* body = this->GuideFold->parentWidget();
  body->setEnabled(p != nullptr);
  if (!p)
    {
    this->updateRunButton();
    return;
    }
  this->Updating = true;
  std::function<void(QComboBox*, int)> setIndex = [](QComboBox* combo, int value)
    {
    const QSignalBlocker blocker(combo);
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    };
  std::function<void(QComboBox*, const char*)> setText = [](QComboBox* combo, const char* value)
    {
    const QSignalBlocker blocker(combo);
    const int index = combo->findData(QString(value ? value : ""));
    combo->setCurrentIndex(index >= 0 ? index : 0);
    };
  std::function<void(qSlicerAstroSegmentedButton*, int)> setSegmented = [](qSlicerAstroSegmentedButton* button, int value)
    {
    const QSignalBlocker blocker(button);
    button->setCurrentIndex(value);
    };
  std::function<void(qSlicerAstroValueField*, double)> setValue = [](qSlicerAstroValueField* field, double value)
    {
    const QSignalBlocker blocker(field);
    field->setValue(value);
    };
  std::function<void(QCheckBox*, bool)> setChecked = [](QCheckBox* box, bool value)
    {
    const QSignalBlocker blocker(box);
    box->setChecked(value);
    };
  {
  const QSignalBlocker blocker(this->InputVolumeNodeSelector);
  this->InputVolumeNodeSelector->setCurrentNodeID(QString::fromUtf8(p->GetInputVolumeNodeID() ? p->GetInputVolumeNodeID() : ""));
  }
  {
  const QSignalBlocker blocker(this->ReferenceVolumeNodeSelector);
  this->ReferenceVolumeNodeSelector->setCurrentNodeID(
    QString::fromUtf8(p->GetReferenceVolumeNodeID() ? p->GetReferenceVolumeNodeID() : ""));
  }
  setIndex(this->SpatialGridModeComboBox, p->GetSpatialGridMode());
  setValue(this->SpatialResampleFactorField, p->GetSpatialResampleFactor());
  setText(this->CelestialFrameComboBox, p->GetCelestialFrame());
  setValue(this->EquinoxField, p->GetEquinox());
  setText(this->ProjectionComboBox, p->GetProjection());
  setValue(this->CenterLongitudeField, p->GetCenterLongitude());
  setValue(this->CenterLatitudeField, p->GetCenterLatitude());
  setValue(this->PixelSizeXField, p->GetPixelSizeX());
  setValue(this->PixelSizeYField, p->GetPixelSizeY());
  setValue(this->RotationField, p->GetRotation());
  setValue(this->SizeXField, p->GetSizeX());
  setValue(this->SizeYField, p->GetSizeY());
  setIndex(this->SpectralGridModeComboBox, p->GetSpectralGridMode());
  setValue(this->SpectralResampleFactorField, p->GetSpectralResampleFactor());
  setText(this->SpectralTypeComboBox, p->GetSpectralType());
  setText(this->SpectralFrameComboBox, p->GetSpectralFrame());
  setValue(this->RestFrequencyField, p->GetRestFrequency() / 1.e6);
  double scale = 1.;
  QString unit;
  SpectralUnit(QString(p->GetSpectralType()), scale, unit);
  this->SpectralStartField->setUnit(unit);
  this->SpectralStepField->setUnit(unit);
  setValue(this->SpectralStartField, p->GetSpectralStart() / scale);
  setValue(this->SpectralStepField, p->GetSpectralStep() / scale);
  setValue(this->SpectralCountField, p->GetSpectralCount());
  setSegmented(this->SpectralMatchingButton, p->GetSpectralMatching());
  setIndex(this->SpatialMethodComboBox, p->GetInterpolationOrder());
  setValue(this->SupersamplingField, p->GetSupersampling());
  setIndex(this->SpectralMethodComboBox, p->GetSpectralMethod());
  setValue(this->MinimumCoverageField, p->GetMinimumCoverage());
  setSegmented(this->ResolutionModeButton, p->GetResolutionMode());
  setValue(this->TargetBeamMajorField, p->GetTargetBeamMajor());
  setValue(this->TargetBeamMinorField, p->GetTargetBeamMinor());
  setValue(this->TargetBeamPAField, p->GetTargetBeamPA());
  setSegmented(this->CombinationButton, p->GetCombination());
  setIndex(this->InputCorrectionComboBox, p->GetInputCorrection());
  setSegmented(this->PrimaryBeamModelButton, p->GetPrimaryBeamModel());
  setValue(this->DishDiameterField, p->GetDishDiameter());
  setValue(this->PrimaryBeamFactorField, p->GetPrimaryBeamFactor());
  setValue(this->PrimaryBeamCutoffField, p->GetPrimaryBeamCutoff());
  setChecked(this->NoiseWeightingCheckBox, p->GetNoiseWeighting() != 0);
  setChecked(this->ComputeWeightCheckBox, p->GetComputeWeight() != 0);
  setChecked(this->ComputeSensitivityCheckBox, p->GetComputeSensitivity() != 0);
  setChecked(this->SaultImageCheckBox, p->GetSaultImage() != 0);
  setValue(this->SaultFractionField, p->GetSaultFraction());
  this->updateInputs();
  this->updateExplanations();
  this->updateVisibility();
  this->Updating = false;
  this->updateSummary();
  this->updateResults();
  this->updateRunButton();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateVisibility()
{
  Parameters* p = this->parametersNode;
  if (!p)
    {
    return;
    }
  const int spatial = p->GetSpatialGridMode();
  const int spectral = p->GetSpectralGridMode();
  const bool manual = spatial == Parameters::SpatialManual;
  const bool fitted = spatial == Parameters::SpatialUnion || spatial == Parameters::SpatialIntersection;
  this->setRowVisible(this->ReferenceVolumeNodeSelector,
                      spatial == Parameters::SpatialReference || spectral == Parameters::SpectralReference);
  this->setRowVisible(this->SpatialResampleFactorField, spatial == Parameters::SpatialResample);
  const std::string frame = p->GetCelestialFrame();
  for (QWidget* field : std::vector<QWidget*>{this->CelestialFrameComboBox, this->ProjectionComboBox,
                                              this->PixelSizeXField, this->RotationField})
    {
    this->setRowVisible(field, manual || fitted);
    }
  this->setRowVisible(this->EquinoxField, (manual || fitted) && (frame == "FK5" || frame == "FK4"));
  for (QWidget* field : std::vector<QWidget*>{this->CenterLongitudeField, this->CenterLatitudeField,
                                              this->PixelSizeYField, this->SizeXField, this->SizeYField,
                                              this->ManualGridButtons})
    {
    this->setRowVisible(field, manual);
    }
  this->PixelSizeXField->spinBox()->setMinimum(manual ? 1.e-4 : 0.);
  const bool spectralFitted = spectral == Parameters::SpectralUnion || spectral == Parameters::SpectralIntersection;
  const bool spectralManual = spectral == Parameters::SpectralManual;
  this->setRowVisible(this->SpectralResampleFactorField, spectral == Parameters::SpectralResample);
  for (QWidget* field : std::vector<QWidget*>{this->SpectralTypeComboBox, this->SpectralFrameComboBox,
                                              this->RestFrequencyField, this->SpectralStepField})
    {
    this->setRowVisible(field, spectralFitted || spectralManual);
    }
  for (QWidget* field : std::vector<QWidget*>{this->SpectralStartField, this->SpectralCountField,
                                              this->SpectralFromInputButton})
    {
    this->setRowVisible(field, spectralManual);
    }
  this->SpectralStepField->spinBox()->setSpecialValueText(spectralFitted ? Tr("narrowest of the inputs") : QString());
  this->setRowVisible(this->SupersamplingField, p->GetInterpolationOrder() == Parameters::AreaAverage);
  const int resolution = p->GetResolutionMode();
  this->setRowVisible(this->CommonBeamLabel, resolution == Parameters::ResolutionCommonBeam);
  for (QWidget* field : std::vector<QWidget*>{this->TargetBeamMajorField, this->TargetBeamMinorField,
                                              this->TargetBeamPAField})
    {
    this->setRowVisible(field, resolution == Parameters::ResolutionTarget);
    }
  const int model = p->GetPrimaryBeamModel();
  const bool combined = p->GetCombination() != Parameters::CombineMean;
  this->setRowVisible(this->InputCorrectionComboBox, combined);
  this->setRowVisible(this->PrimaryBeamModelButton, combined);
  this->setRowVisible(this->DishDiameterField, combined &&
                      (model == Parameters::GaussianPrimaryBeam || model == Parameters::AiryPrimaryBeam));
  this->setRowVisible(this->PrimaryBeamFactorField, combined && model == Parameters::GaussianPrimaryBeam);
  this->setRowVisible(this->PrimaryBeamCutoffField, combined && model != Parameters::NoPrimaryBeam);
  this->setRowVisible(this->SaultFractionField, p->GetSaultImage() != 0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateInputs()
{
  Q_Q(qSlicerAstroReprojectModuleWidget);
  const std::vector<vtkMRMLAstroVolumeNode*> volumes = this->logic() && this->parametersNode ?
    this->logic()->GetInputVolumes(this->parametersNode) : std::vector<vtkMRMLAstroVolumeNode*>();
  const bool beams = this->parametersNode &&
    this->parametersNode->GetCombination() != Parameters::CombineMean &&
    this->parametersNode->GetPrimaryBeamModel() == Parameters::VolumePrimaryBeam;
  QString signature = beams ? "beams" : "";
  for (std::size_t index = 0; index < volumes.size(); index++)
    {
    signature += QString(";%1:%2").arg(QString::fromUtf8(volumes[index]->GetID()))
      .arg(QString::fromUtf8(volumes[index]->GetName()));
    if (beams)
      {
      signature += "/" + QString::fromStdString(
        this->logic()->GetInputPrimaryBeam(this->parametersNode, static_cast<int>(index)));
      }
    }
  if (signature == this->InputsSignature)
    {
    // the same rows: only the noise values
    for (std::size_t index = 0; index < volumes.size(); index++)
      {
      qSlicerAstroValueField* noise = this->InputWeights->findChild<qSlicerAstroValueField*>(
        QString("InputNoiseField%1").arg(index));
      if (noise)
        {
        const QSignalBlocker blocker(noise);
        noise->setValue(this->logic()->GetInputNoise(this->parametersNode, static_cast<int>(index)));
        }
      }
    return;
    }
  this->InputsSignature = signature;
  // the other inputs (mosaic): one row each, with a remove button
  while (QLayoutItem* item = this->ExtraInputsLayout->takeAt(0))
    {
    if (item->widget())
      {
      item->widget()->deleteLater();
      }
    delete item;
    }
  while (QLayoutItem* item = this->InputWeightsLayout->takeAt(0))
    {
    if (item->widget())
      {
      item->widget()->deleteLater();
      }
    delete item;
    }
  for (std::size_t index = 1; index < volumes.size(); index++)
    {
    vtkMRMLAstroVolumeNode* volume = volumes[index];
    QFrame* row = new QFrame(this->ExtraInputs);
    row->setObjectName(QString("ExtraInput%1").arg(index));
    row->setProperty("astroRole", "card");
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(10, 6, 6, 6);
    rowLayout->setSpacing(6);
    QLabel* name = new QLabel(QString::fromUtf8(volume->GetName()), row);
    name->setToolTip(Tr("Input combined with the others (mosaic)."));
    QLabel* detail = RowLabel(VolumeDetail(volume), row);
    detail->setToolTip(Tr("Size and unit of the data."));
    QToolButton* remove = new QToolButton(row);
    remove->setObjectName("RemoveInputButton");
    remove->setIcon(qSlicerAstroTheme::instance()->icon("close", "muted"));
    remove->setToolTip(Tr("Remove this input (the data are not deleted)."));
    remove->setCursor(Qt::PointingHandCursor);
    const QString id = QString::fromUtf8(volume->GetID());
    QObject::connect(remove, &QToolButton::clicked, q, [q, id]() { q->removeInputVolume(id); });
    rowLayout->addWidget(name, 1);
    rowLayout->addWidget(detail);
    rowLayout->addWidget(remove);
    this->ExtraInputsLayout->addWidget(row);
    }
  this->ExtraInputs->setVisible(volumes.size() > 1);

  // noise and primary beam of each input (mosaic)
  const QString title = Tr("Noise per input");
  QLabel* header = Overline(beams ? Tr("Noise and primary beam per input") : title, this->InputWeights);
  this->InputWeightsLayout->addWidget(header, 0, 0, 1, 2);
  for (std::size_t index = 0; index < volumes.size(); index++)
    {
    vtkMRMLAstroVolumeNode* volume = volumes[index];
    const int row = 1 + 2 * static_cast<int>(index);
    QLabel* name = RowLabel(QString::fromUtf8(volume->GetName()), this->InputWeights);
    name->setToolTip(Tr("Input %1.").arg(index + 1));
    this->InputWeightsLayout->addWidget(name, row, 0);
    qSlicerAstroValueField* noise = new qSlicerAstroValueField(this->InputWeights);
    noise->setObjectName(QString("InputNoiseField%1").arg(index));
    noise->setDecimals(8);
    noise->setRange(0., 1.e12);
    const char* unit = volume->GetAttribute("SlicerAstro.BUNIT");
    noise->setUnit(unit ? qSlicerAstroTheme::displayUnit(QString::fromUtf8(unit)) : QString());
    noise->spinBox()->setSpecialValueText(Tr("auto"));
    noise->setValue(this->logic()->GetInputNoise(this->parametersNode, static_cast<int>(index)));
    noise->setToolTip(Tr("rms noise of %1 (data units). auto: the noise estimated by SlicerAstro when the data were "
                         "loaded.").arg(QString::fromUtf8(volume->GetName())));
    const int inputIndex = static_cast<int>(index);
    QObject::connect(noise, &qSlicerAstroValueField::valueChanged, q, [this, inputIndex](double value)
      {
      if (!this->Updating && this->parametersNode)
        {
        this->logic()->SetInputNoise(this->parametersNode, inputIndex, value);
        }
      });
    this->InputWeightsLayout->addWidget(noise, row, 1);
    if (beams)
      {
      qSlicerAstroDatasetSelector* beam = new qSlicerAstroDatasetSelector(this->InputWeights);
      beam->setObjectName(QString("InputPrimaryBeamSelector%1").arg(index));
      beam->setNoneText(Tr("Primary beam data"));
      beam->setToolTip(Tr("Data with the primary beam response of %1 (values 0-1).")
                         .arg(QString::fromUtf8(volume->GetName())));
      beam->setMRMLScene(q->mrmlScene());
      beam->setCurrentNodeID(QString::fromStdString(this->logic()->GetInputPrimaryBeam(this->parametersNode, inputIndex)));
      QObject::connect(beam, &qSlicerAstroDatasetSelector::currentNodeChanged, q, [this, inputIndex](vtkMRMLNode* node)
        {
        if (!this->Updating && this->parametersNode)
          {
          this->logic()->SetInputPrimaryBeam(this->parametersNode, inputIndex, node ? node->GetID() : "");
          }
        });
      this->InputWeightsLayout->addWidget(beam, row + 1, 0, 1, 2);
      }
    }
  qSlicerAstroTheme::instance()->applyTo(this->ExtraInputs);
  qSlicerAstroTheme::instance()->applyTo(this->InputWeights);
  this->setRowVisible(this->InputWeights, volumes.size() > 1 || beams);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateSummary()
{
  if (!this->parametersNode || !this->logic())
    {
    this->GridSummaryLabel->clear();
    return;
    }
  if (this->logic()->GetInputVolumes(this->parametersNode).empty())
    {
    this->GridSummaryLabel->setText(Tr("Select the data to reproject."));
    return;
    }
  this->GridSummaryLabel->setText(QString::fromStdString(this->logic()->DescribeOutputGrid(this->parametersNode)));
  if (this->parametersNode->GetResolutionMode() == Parameters::ResolutionCommonBeam)
    {
    double beam[3];
    if (this->logic()->GetCommonBeam(this->parametersNode, beam))
      {
      this->CommonBeamLabel->setText(QString::fromUtf8("%1″ × %2″, PA %3°").arg(beam[0], 0, 'f', 2)
                                     .arg(beam[1], 0, 'f', 2).arg(beam[2], 0, 'f', 1));
      }
    else
      {
      this->CommonBeamLabel->setText(QString::fromStdString(this->logic()->GetLastError()));
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateResults()
{
  QList<qSlicerAstroResultCard*> cards;
  QList<bool> visible;
  Parameters* p = this->parametersNode;
  const char* ids[4] = {p ? p->GetOutputVolumeNodeID() : nullptr, p ? p->GetWeightVolumeNodeID() : nullptr,
                        p ? p->GetSensitivityVolumeNodeID() : nullptr, p ? p->GetSaultVolumeNodeID() : nullptr};
  qSlicerAstroResultCard* resultCards[4] = {this->ResultCard, this->WeightCard, this->SensitivityCard, this->SaultCard};
  for (int index = 0; index < 4; index++)
    {
    vtkMRMLAstroVolumeNode* output = this->volume(ids[index]);
    const bool show = output && output->GetImageData();
    if (show && resultCards[index]->volume() != output)
      {
      resultCards[index]->setVolume(output);
      }
    cards << resultCards[index];
    visible << show;
    }
  qSlicerAstroResultCard::setCardsVisible(cards, visible);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidgetPrivate::updateRunButton()
{
  const std::size_t inputs = this->logic() && this->parametersNode ?
    this->logic()->GetInputVolumes(this->parametersNode).size() : 0;
  if (this->RunButton->isRunning())
    {
    return;
    }
  this->RunButton->setText(inputs == 0 ? Tr("Select data") : (inputs > 1 ? Tr("Make the mosaic") : Tr("Reproject")));
  this->RunButton->setEnabled(inputs > 0);
}

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidget::qSlicerAstroReprojectModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroReprojectModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroReprojectModuleWidget::~qSlicerAstroReprojectModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::enter()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  this->Superclass::enter();
  d->Entered = true;
  this->onMRMLSelectionNodeModified(d->selectionNode);
  this->onMRMLAstroReprojectParametersNodeModified();
}

//----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::exit()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  d->Entered = false;
  this->Superclass::exit();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!scene)
    {
    return;
    }
  this->Superclass::setMRMLScene(scene);
  vtkSlicerApplicationLogic* appLogic = this->module() ? this->module()->appLogic() : nullptr;
  d->selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  this->initializeParameterNode();
  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent, this, SLOT(onEndCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::StartImportEvent, this, SLOT(onStartImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(onEndImportEvent()));
  if (d->selectionNode)
    {
    this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent, this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onEndCloseEvent()
{
  this->initializeParameterNode(true);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onEndImportEvent()
{
  this->initializeParameterNode();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  // the parameters node of the module (created when the scene was closed) is
  // replaced by the one of the imported scene, as in the other modules
  if (d->parametersNode && this->mrmlScene())
    {
    vtkMRMLNode* parametersNode = d->parametersNode;
    this->setMRMLAstroReprojectParametersNode(nullptr);
    this->mrmlScene()->RemoveNode(parametersNode);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene)
    {
    return;
    }
  vtkMRMLAstroReprojectParametersNode* parametersNode = nullptr;
  const int numNodes = scene->GetNumberOfNodesByClass("vtkMRMLAstroReprojectParametersNode");
  if (numNodes > 0 && !forceNew)
    {
    parametersNode = vtkMRMLAstroReprojectParametersNode::SafeDownCast(
      scene->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroReprojectParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> node;
    node.TakeReference(scene->CreateNodeByClass("vtkMRMLAstroReprojectParametersNode"));
    scene->AddNode(node);
    parametersNode = vtkMRMLAstroReprojectParametersNode::SafeDownCast(node);
    if (d->selectionNode && d->selectionNode->GetActiveVolumeID() && d->logic())
      {
      d->logic()->SetSingleInputVolume(parametersNode, d->selectionNode->GetActiveVolumeID());
      }
    }
  this->setMRMLAstroReprojectParametersNode(parametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  vtkMRMLSelectionNode* selectionNode = vtkMRMLSelectionNode::SafeDownCast(sender);
  if (!d->Entered || !selectionNode || !d->parametersNode || !d->logic() || d->RunButton->isRunning() ||
      !selectionNode->GetActiveVolumeID())
    {
    return;
    }
  // the panel follows the active data, unless several inputs (mosaic) were chosen
  if (d->logic()->GetInputVolumes(d->parametersNode).size() <= 1 &&
      vtkMRMLAstroVolumeNode::SafeDownCast(d->volume(selectionNode->GetActiveVolumeID())))
    {
    d->logic()->SetFirstInputVolume(d->parametersNode, selectionNode->GetActiveVolumeID());
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setMRMLAstroReprojectParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  vtkMRMLAstroReprojectParametersNode* node = vtkMRMLAstroReprojectParametersNode::SafeDownCast(mrmlNode);
  this->qvtkReconnect(d->parametersNode, node, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroReprojectParametersNodeModified()));
  d->parametersNode = node;
  this->onMRMLAstroReprojectParametersNodeModified();
}

//--------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onMRMLAstroReprojectParametersNodeModified()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (this->mrmlScene() && this->mrmlScene()->IsClosing())
    {
    // scene closing: the selectors may still list removed nodes
    return;
    }
  d->updateFromNode();
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setInputVolume(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!d->parametersNode || !d->logic() || d->Updating)
    {
    return;
    }
  d->logic()->SetFirstInputVolume(d->parametersNode, node ? node->GetID() : nullptr);
  // the chosen data become the active data (the views and other panels follow)
  if (node && d->selectionNode && d->Entered)
    {
    d->selectionNode->SetActiveVolumeID(node->GetID());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::addInputVolume(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!d->parametersNode || !d->logic() || !node)
    {
    return;
    }
  d->logic()->AddInputVolume(d->parametersNode, node->GetID());
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::removeInputVolume(const QString& nodeID)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!d->parametersNode || !d->logic())
    {
    return;
    }
  d->logic()->RemoveInputVolume(d->parametersNode, nodeID.toUtf8().constData());
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setManualGridFromInput()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (d->parametersNode && !d->logic()->SetManualGridFromInput(d->parametersNode, true, false, false))
    {
    d->Message->showMessage("warning", QString::fromStdString(d->logic()->GetLastError()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setManualGridFromInputNorthUp()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (d->parametersNode && !d->logic()->SetManualGridFromInput(d->parametersNode, true, false, true))
    {
    d->Message->showMessage("warning", QString::fromStdString(d->logic()->GetLastError()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setManualSpectralGridFromInput()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (d->parametersNode && !d->logic()->SetManualGridFromInput(d->parametersNode, false, true, false))
    {
    d->Message->showMessage("warning", QString::fromStdString(d->logic()->GetLastError()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::setTargetBeamFromCommonBeam()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  double beam[3];
  if (!d->parametersNode)
    {
    return;
    }
  if (!d->logic()->GetCommonBeam(d->parametersNode, beam))
    {
    d->Message->showMessage("warning", QString::fromStdString(d->logic()->GetLastError()));
    return;
    }
  const int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetTargetBeamMajor(beam[0]);
  d->parametersNode->SetTargetBeamMinor(beam[1]);
  d->parametersNode->SetTargetBeamPA(beam[2]);
  d->parametersNode->SetResolutionMode(Parameters::ResolutionTarget);
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::onApply()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (!d->parametersNode || !d->logic() || d->RunButton->isRunning())
    {
    return;
    }
  d->Message->clearMessage();
  // in the background, without blocking the user interface: the progress
  // and Cancel are in the run button (a click of the button, or a script)
  QPointer<qSlicerAstroReprojectModuleWidget> self(this);
  bool started = false;
  d->RunButton->runAsActive([d, self, &started]()
    {
    started = d->logic()->StartReproject(d->parametersNode, [self](bool success)
      {
      if (self)
        {
        self->finishReprojection(success);
        }
      });
    });
  if (!started)
    {
    this->finishReprojection(false);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::finishReprojection(bool success)
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  d->updateRunButton();
  if (!d->parametersNode || !d->logic())
    {
    return;
    }
  const QString warnings = QString::fromStdString(d->logic()->GetLastWarnings()).trimmed();
  if (!success)
    {
    const QString error = QString::fromStdString(d->logic()->GetLastError());
    if (error == "cancelled")
      {
      d->Message->showMessage("info", tr("Reprojection cancelled."));
      }
    else
      {
      d->Message->showMessage("error", error + (warnings.isEmpty() ? QString() : "\n" + warnings));
      }
    return;
    }
  d->Message->showMessage(warnings.isEmpty() ? "success" : "warning",
                          warnings.isEmpty() ? tr("Reprojected data created.") : warnings);
  d->updateResults();
  // the module panel compares the input and the output (classic Slicer); in
  // the Workbench the views are not changed (the output is a product of the
  // input there)
  const std::vector<vtkMRMLAstroVolumeNode*> inputs = d->logic()->GetInputVolumes(d->parametersNode);
  const char* outputID = d->parametersNode->GetOutputVolumeNodeID();
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* astroVolume = app ? app->moduleManager()->module("AstroVolume") : nullptr;
  qSlicerAstroVolumeModuleWidget* astroVolumeWidget = astroVolume ?
    dynamic_cast<qSlicerAstroVolumeModuleWidget*>(astroVolume->widgetRepresentation()) : nullptr;
  if (d->isModulePanel() && astroVolumeWidget && !inputs.empty() && outputID)
    {
    astroVolumeWidget->setThreeComparativeView(inputs[0]->GetID(), outputID, outputID);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroReprojectModuleWidget::showResult()
{
  Q_D(qSlicerAstroReprojectModuleWidget);
  if (d->ResultCard->volume())
    {
    emit d->ResultCard->showRequested();
    }
}

//---------------------------------------------------------------------------
vtkMRMLAstroReprojectParametersNode* qSlicerAstroReprojectModuleWidget::mrmlAstroReprojectParametersNode()const
{
  Q_D(const qSlicerAstroReprojectModuleWidget);
  return d->parametersNode;
}
