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
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include <vtkTable.h>
#include <vtkWeakPointer.h>

// SlicerQt includes
#include <qMRMLSegmentsTableView.h>
#include <qMRMLTableView.h>
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerLayoutManager.h>
#include <vtkSlicerApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkSegmentation.h>

// AstroVolume includes
#include <qSlicerAstroDatasetSelector.h>
#include <qSlicerAstroFoldSection.h>
#include <qSlicerAstroMessageWidget.h>
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroResultCard.h>
#include <qSlicerAstroRunButton.h>
#include <qSlicerAstroSegmentedButton.h>
#include <qSlicerAstroSegmentsTableStyler.h>
#include <qSlicerAstroTheme.h>
#include <qSlicerAstroValueField.h>
#include <vtkSlicerAstroVolumeLogic.h>

// Logic includes
#include <vtkSlicerAstroModelingLogic.h>

// AstroModeling includes
#include "qSlicerAstroModelingModuleWidget.h"
#include "qSlicerAstroRingPlot.h"

// STD includes
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace
{
typedef vtkMRMLAstroModelingParametersNode ParametersNode;

//----------------------------------------------------------------------------
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}

//----------------------------------------------------------------------------
QLabel* RowLabel(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  label->setProperty("astroRole", "label");
  return label;
}

//----------------------------------------------------------------------------
QToolButton* Chip(const QString& text, const QString& toolTip, const QString& objectName, QWidget* parent)
{
  QToolButton* button = new QToolButton(parent);
  button->setProperty("astroRole", "actionChip");
  button->setText(text);
  button->setCheckable(true);
  button->setToolTip(toolTip);
  button->setObjectName(objectName);
  button->setCursor(Qt::PointingHandCursor);
  return button;
}

//----------------------------------------------------------------------------
/// A numerical parameter of 3DBarolo in the parameters node, with its free
/// (fitted) flag if it has one
struct NumberParameter
{
  const char* Name;
  QString Label;
  QString Unit;
  double Minimum;
  double Maximum;
  double Step;
  int Decimals;
  /// -1: estimated by 3DBarolo ("auto")
  bool Auto;
  QString ToolTip;
  std::function<double(ParametersNode*)> Get;
  std::function<void(ParametersNode*, double)> Set;
  std::function<bool(ParametersNode*)> GetFree;
  std::function<void(ParametersNode*, bool)> SetFree;
};

//----------------------------------------------------------------------------
struct RingParameter
{
  const char* Column;
  const char* Label;
};

//----------------------------------------------------------------------------
const RingParameter RingParameters[] = {
  {"VRot", "Rotation velocity"}, {"VDisp", "Velocity dispersion"}, {"Inc", "Inclination"},
  {"Phi", "Position angle"}, {"VSys", "Systemic velocity"}, {"VRad", "Radial velocity"},
  {"Dens", "Column density"}, {"Z0", "Scale height"}, {"XPos", "X centre"}, {"YPos", "Y centre"}};
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroModelingModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroModelingModuleWidget);
protected:
  qSlicerAstroModelingModuleWidget* const q_ptr;
public:
  qSlicerAstroModelingModuleWidgetPrivate(qSlicerAstroModelingModuleWidget& object) : q_ptr(&object) {}
  void init();
  QWidget* buildSetup(QWidget* parent);
  QWidget* buildAdvanced(QWidget* parent);
  QWidget* buildResults(QWidget* parent);
  qSlicerAstroValueField* addNumberRow(const NumberParameter& parameter, QGridLayout* grid, QWidget* parent,
                                       bool withFree);
  vtkSlicerAstroModelingLogic* logic() const;
  vtkSlicerAstroVolumeLogic* astroVolumeLogic() const;
  vtkMRMLAstroVolumeNode* inputVolume() const;
  vtkMRMLAstroVolumeNode* volume(const char* id) const;
  vtkMRMLSegmentationNode* datasetMask() const;
  bool embedded() const;
  bool hasResults() const;
  void start(int operation);
  void appendLog(const QString& text);
  void updateFromNode();
  void updateBeam();
  void updateMask();
  void updateResults();
  void updateRingPlot();
  void updateRunning();
  void setSliceObservers(bool observe);

  qSlicerAstroPanelHeader* Header{nullptr};
  qSlicerAstroDatasetSelector* InputVolumeNodeSelector{nullptr};
  qSlicerAstroMessageWidget* Message{nullptr};
  QWidget* Setup{nullptr};
  qSlicerAstroSegmentedButton* RegionButton{nullptr};
  QWidget* SearchNote{nullptr};
  QWidget* MaskPage{nullptr};
  qMRMLSegmentsTableView* SegmentsTableView{nullptr};
  QLabel* MaskInfo{nullptr};
  qSlicerAstroSegmentedButton* ModeButton{nullptr};
  QPushButton* EstimateButton{nullptr};
  qSlicerAstroValueField* BeamFields[3]{nullptr, nullptr, nullptr};
  QLabel* BeamStatus{nullptr};
  QWidget* SearchGroup{nullptr};
  qSlicerAstroFoldSection* AdvancedFold{nullptr};
  QComboBox* LayerTypeCombo{nullptr};
  QComboBox* FittingFunctionCombo{nullptr};
  QComboBox* WeightingFunctionCombo{nullptr};
  qSlicerAstroSegmentedButton* NormalizeButton{nullptr};
  QCheckBox* ADRIFTCheckBox{nullptr};
  QCheckBox* TwoStageCheckBox{nullptr};
  QComboBox* RegularizationCombo{nullptr};
  QComboBox* SideCombo{nullptr};
  QCheckBox* NormalizeCubeCheckBox{nullptr};
  QCheckBox* EstimateErrorsCheckBox{nullptr};
  QLineEdit* RestFrequenciesEdit{nullptr};
  QLineEdit* RelativeIntensitiesEdit{nullptr};
  qSlicerAstroRunButton* CreateButton{nullptr};
  qSlicerAstroRunButton* FitButton{nullptr};
  qSlicerAstroFoldSection* LogFold{nullptr};
  QPlainTextEdit* LogEdit{nullptr};

  QWidget* Results{nullptr};
  QComboBox* RingParameterCombo{nullptr};
  qSlicerAstroRingPlot* RingPlot{nullptr};
  qMRMLTableView* TableView{nullptr};
  qSlicerAstroRunButton* UpdateButton{nullptr};
  QToolButton* DataContoursButton{nullptr};
  QToolButton* ModelContoursButton{nullptr};
  QToolButton* ModelRegionButton{nullptr};
  qSlicerAstroValueField* ContourLevelField{nullptr};
  qSlicerAstroValueField* MajorOffsetField{nullptr};
  qSlicerAstroValueField* MinorOffsetField{nullptr};
  qSlicerAstroResultCard* ModelResult{nullptr};
  qSlicerAstroResultCard* ResidualResult{nullptr};

  std::vector<std::pair<qSlicerAstroValueField*, NumberParameter> > Numbers;
  std::vector<std::pair<QToolButton*, NumberParameter> > FreeButtons;
  std::vector<qSlicerAstroValueField*> AutoFields;

  QTimer* ProgressTimer{nullptr};
  QTimer* MarkersTimer{nullptr};
  QToolButton* ActiveRunButton{nullptr};
  qSlicerAstroRunButton* RunningButton{nullptr};
  vtkSmartPointer<vtkMRMLAstroModelingParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkWeakPointer<vtkMRMLTableNode> ObservedTable;
  vtkWeakPointer<vtkMRMLSegmentationNode> ObservedMask;
  bool SliceObservers{false};
  bool Updating{false};
  QString RingColumn{"VRot"};
  QString EstimatedBeamVolumeID;
  double EstimatedBeam[3]{0., 0., 0.};
  bool EstimatedBeamValid{false};
};

//-----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic* qSlicerAstroModelingModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroModelingModuleWidget);
  return vtkSlicerAstroModelingLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* qSlicerAstroModelingModuleWidgetPrivate::astroVolumeLogic() const
{
  vtkSlicerAstroModelingLogic* logic = this->logic();
  return logic ? logic->GetAstroVolumeLogic() : nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroModelingModuleWidgetPrivate::volume(const char* id) const
{
  Q_Q(const qSlicerAstroModelingModuleWidget);
  vtkMRMLAstroVolumeNode* node = (q->mrmlScene() && id) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(q->mrmlScene()->GetNodeByID(id)) : nullptr;
  return (node && node->GetImageData()) ? node : nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroModelingModuleWidgetPrivate::inputVolume() const
{
  return this->parametersNode ? this->volume(this->parametersNode->GetInputVolumeNodeID()) : nullptr;
}

//-----------------------------------------------------------------------------
vtkMRMLSegmentationNode* qSlicerAstroModelingModuleWidgetPrivate::datasetMask() const
{
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = this->astroVolumeLogic();
  vtkMRMLAstroVolumeNode* cube = this->inputVolume();
  return (astroVolumeLogic && cube) ? astroVolumeLogic->GetMaskSegmentationForVolume(cube, false) : nullptr;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroModelingModuleWidgetPrivate::embedded() const
{
  Q_Q(const qSlicerAstroModelingModuleWidget);
  return q->property("astroEmbedded").toBool();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroModelingModuleWidgetPrivate::hasResults() const
{
  vtkMRMLTableNode* table = this->parametersNode ? this->parametersNode->GetParamsTableNode() : nullptr;
  vtkMRMLAstroVolumeNode* model = this->parametersNode ? this->volume(this->parametersNode->GetOutputVolumeNodeID()) :
                                                         nullptr;
  return table && table->GetNumberOfRows() > 0 && model && model != this->inputVolume();
}

//-----------------------------------------------------------------------------
qSlicerAstroValueField* qSlicerAstroModelingModuleWidgetPrivate::addNumberRow(const NumberParameter& parameter,
                                                                            QGridLayout* grid, QWidget* parent,
                                                                            bool withFree)
{
  Q_Q(qSlicerAstroModelingModuleWidget);
  const int row = grid->rowCount();
  QLabel* label = RowLabel(parameter.Label, parent);
  label->setToolTip(parameter.ToolTip);
  grid->addWidget(label, row, 0);
  qSlicerAstroValueField* field = new qSlicerAstroValueField(parent);
  field->setObjectName(QString("%1Field").arg(parameter.Name));
  field->setUnit(parameter.Unit);
  field->setDecimals(parameter.Decimals);
  field->setRange(parameter.Minimum, parameter.Maximum);
  field->setSingleStep(parameter.Step);
  field->setToolTip(parameter.ToolTip + (parameter.Auto ?
    qSlicerAstroModelingModuleWidget::tr("\n\n\"auto\" (-1): estimated by 3DBarolo.") : QString()));
  if (parameter.Auto)
    {
    field->spinBox()->setSpecialValueText(qSlicerAstroModelingModuleWidget::tr("auto"));
    this->AutoFields.push_back(field);
    }
  grid->addWidget(field, row, 1);
  QObject::connect(field, &qSlicerAstroValueField::valueChanged, q, [this, parameter](double value)
    {
    if (!this->Updating && this->parametersNode)
      {
      parameter.Set(this->parametersNode, value);
      }
    });
  this->Numbers.push_back(std::make_pair(field, parameter));
  if (withFree && parameter.SetFree)
    {
    QToolButton* free = Chip(qSlicerAstroModelingModuleWidget::tr("free"),
      qSlicerAstroModelingModuleWidget::tr("Fitted by 3DBarolo (free); unchecked: fixed at the value."),
      QString("%1Free").arg(parameter.Name), parent);
    grid->addWidget(free, row, 2);
    QObject::connect(free, &QToolButton::toggled, q, [this, parameter](bool checked)
      {
      if (!this->Updating && this->parametersNode)
        {
        parameter.SetFree(this->parametersNode, checked);
        }
      });
    this->FreeButtons.push_back(std::make_pair(free, parameter));
    }
  return field;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroModelingModuleWidget);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(qSlicerAstroModelingModuleWidget::tr("Modelling"));
  this->Header->setIconName("module-modelling");
  QLabel* version = new QLabel(qSlicerAstroModelingModuleWidget::tr("3DBarolo 1.8"), this->Header);
  version->setProperty("astroRole", "chip");
  version->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "3D tilted-ring models of galaxies with 3DBarolo (Di Teodoro & Fraternali 2015)."));
  this->Header->addWidget(version);
  rootLayout->addWidget(this->Header);

  QWidget* body = new QWidget(q);
  QVBoxLayout* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(12, 12, 12, 14);
  bodyLayout->setSpacing(14);
  rootLayout->addWidget(body);

  this->InputVolumeNodeSelector = new qSlicerAstroDatasetSelector(body);
  this->InputVolumeNodeSelector->setObjectName("InputVolumeNodeSelector");
  this->InputVolumeNodeSelector->setRequiredDimensions(3);
  this->InputVolumeNodeSelector->setNoneText(qSlicerAstroModelingModuleWidget::tr("Select a data cube"));
  this->InputVolumeNodeSelector->setToolTip(qSlicerAstroModelingModuleWidget::tr("Data cube of the galaxy modelled."));
  bodyLayout->addWidget(this->InputVolumeNodeSelector);
  this->Message = new qSlicerAstroMessageWidget(body);
  this->Message->setObjectName("MessageWidget");
  bodyLayout->addWidget(this->Message);
  this->Setup = this->buildSetup(body);
  bodyLayout->addWidget(this->Setup);

  // operations and log
  QFrame* footer = new QFrame(q);
  footer->setProperty("astroRole", "panelFooter");
  QVBoxLayout* footerLayout = new QVBoxLayout(footer);
  footerLayout->setContentsMargins(12, 12, 12, 12);
  footerLayout->setSpacing(8);
  QHBoxLayout* runRow = new QHBoxLayout();
  runRow->setSpacing(6);
  this->CreateButton = new qSlicerAstroRunButton(footer);
  this->CreateButton->setObjectName("CreateButton");
  this->CreateButton->setText(qSlicerAstroModelingModuleWidget::tr("Create model"));
  this->CreateButton->pushButton()->setObjectName("CreatePushButton");
  this->CreateButton->pushButton()->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "The model of the parameters (no fit): model and residual cubes, rings table."));
  this->FitButton = new qSlicerAstroRunButton(footer);
  this->FitButton->setObjectName("FitButton");
  this->FitButton->setText(qSlicerAstroModelingModuleWidget::tr("Fit"));
  this->FitButton->pushButton()->setObjectName("FitPushButton");
  this->FitButton->pushButton()->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Fit the free parameters ring by ring (3DBarolo GALFIT): the rings table, the model and the residuals."));
  runRow->addWidget(this->CreateButton, 1);
  runRow->addWidget(this->FitButton, 1);
  footerLayout->addLayout(runRow);
  this->LogFold = new qSlicerAstroFoldSection(footer);
  this->LogFold->setObjectName("LogFold");
  this->LogFold->setTitle(qSlicerAstroModelingModuleWidget::tr("3DBarolo log"));
  this->LogFold->setSettingsKey("SlicerAstro/Modeling/LogFold");
  this->LogEdit = new QPlainTextEdit(this->LogFold);
  this->LogEdit->setObjectName("LogEdit");
  this->LogEdit->setReadOnly(true);
  this->LogEdit->setMaximumBlockCount(20000);
  this->LogEdit->setMinimumHeight(160);
  this->LogEdit->setProperty("astroRole", "mono");
  this->LogEdit->setToolTip(qSlicerAstroModelingModuleWidget::tr("What 3DBarolo prints during the operations."));
  this->LogFold->contentLayout()->addWidget(this->LogEdit);
  footerLayout->addWidget(this->LogFold);
  rootLayout->addWidget(footer);

  this->Results = this->buildResults(q);
  rootLayout->addWidget(this->Results);
  rootLayout->addStretch(1);

  this->ProgressTimer = new QTimer(q);
  this->ProgressTimer->setInterval(250);
  this->MarkersTimer = new QTimer(q);
  this->MarkersTimer->setSingleShot(true);
  this->MarkersTimer->setInterval(0);

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setInputVolume(vtkMRMLNode*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->InputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), this->TableView, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SegmentsTableView, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(this->CreateButton, SIGNAL(clicked()), q, SLOT(createModel()));
  QObject::connect(this->FitButton, SIGNAL(clicked()), q, SLOT(fit()));
  QObject::connect(this->UpdateButton, SIGNAL(clicked()), q, SLOT(updateModelFromTable()));
  for (qSlicerAstroRunButton* button : {this->CreateButton, this->FitButton, this->UpdateButton})
    {
    QObject::connect(button, SIGNAL(cancelRequested()), q, SLOT(cancelOperation()));
    }
  QObject::connect(this->ProgressTimer, SIGNAL(timeout()), q, SLOT(onProgressTimeout()));
  QObject::connect(this->MarkersTimer, &QTimer::timeout, q, [this]()
    {
    if (this->logic() && this->parametersNode && this->hasResults())
      {
      this->logic()->UpdateRingMarkers(this->parametersNode);
      }
    });
  theme->applyTo(q);
  this->updateFromNode();
}

//-----------------------------------------------------------------------------
QWidget* qSlicerAstroModelingModuleWidgetPrivate::buildSetup(QWidget* parent)
{
  Q_Q(qSlicerAstroModelingModuleWidget);
  QWidget* setup = new QWidget(parent);
  QVBoxLayout* layout = new QVBoxLayout(setup);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(14);

  // the galaxy: found by 3DBarolo, or a segment of the mask of the cube
  QWidget* region = new QWidget(setup);
  QVBoxLayout* regionLayout = new QVBoxLayout(region);
  regionLayout->setContentsMargins(0, 0, 0, 0);
  regionLayout->setSpacing(8);
  regionLayout->addWidget(Overline(qSlicerAstroModelingModuleWidget::tr("Galaxy"), region));
  this->RegionButton = new qSlicerAstroSegmentedButton(region);
  this->RegionButton->setObjectName("RegionButton");
  this->RegionButton->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Voxels of the galaxy: found by the source finder of 3DBarolo, or a segment of the mask of the cube."));
  this->RegionButton->addButton(qSlicerAstroModelingModuleWidget::tr("Find source"),
    qSlicerAstroModelingModuleWidget::tr("3DBarolo searches the largest source in the cube (Advanced: source "
                                          "search)."));
  this->RegionButton->addButton(qSlicerAstroModelingModuleWidget::tr("Mask"),
    qSlicerAstroModelingModuleWidget::tr("A segment of the mask of the cube (e.g. a SoFiA source): the fit and "
                                          "the normalization of the model use its voxels."));
  regionLayout->addWidget(this->RegionButton);
  QLabel* searchNote = RowLabel(qSlicerAstroModelingModuleWidget::tr(
    "3DBarolo finds the galaxy (source search in Advanced)."), region);
  searchNote->setWordWrap(true);
  this->SearchNote = searchNote;
  regionLayout->addWidget(searchNote);
  QWidget* maskPage = new QWidget(region);
  this->MaskPage = maskPage;
  QVBoxLayout* maskLayout = new QVBoxLayout(maskPage);
  maskLayout->setContentsMargins(0, 0, 0, 0);
  maskLayout->setSpacing(6);
  this->SegmentsTableView = new qMRMLSegmentsTableView(maskPage);
  this->SegmentsTableView->setObjectName("SegmentsTableView");
  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  this->SegmentsTableView->setMinimumHeight(110);
  this->SegmentsTableView->setMaximumHeight(160);
  this->SegmentsTableView->setReadOnly(true);
  this->SegmentsTableView->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "The segment of the mask of the cube used for the fit (the first one if none is selected)."));
  qSlicerAstroSegmentsTableStyler::styleTable(this->SegmentsTableView);
  maskLayout->addWidget(this->SegmentsTableView);
  this->MaskInfo = RowLabel(QString(), maskPage);
  this->MaskInfo->setObjectName("MaskInfo");
  this->MaskInfo->setWordWrap(true);
  maskLayout->addWidget(this->MaskInfo);
  regionLayout->addWidget(maskPage);
  layout->addWidget(region);

  // mode and estimate
  QHBoxLayout* modeRow = new QHBoxLayout();
  modeRow->setSpacing(6);
  this->ModeButton = new qSlicerAstroSegmentedButton(setup);
  this->ModeButton->setObjectName("ModeButton");
  this->ModeButton->setToolTip(qSlicerAstroModelingModuleWidget::tr("Initial parameters of the rings."));
  this->ModeButton->addButton(qSlicerAstroModelingModuleWidget::tr("Automatic"),
    qSlicerAstroModelingModuleWidget::tr("3DBarolo estimates the initial parameters of the rings."));
  this->ModeButton->addButton(qSlicerAstroModelingModuleWidget::tr("Manual"),
    qSlicerAstroModelingModuleWidget::tr("The initial parameters given here (\"auto\" values are estimated)."));
  this->EstimateButton = new QPushButton(qSlicerAstroTheme::instance()->icon("target", "text"),
                                         qSlicerAstroModelingModuleWidget::tr("Estimate"), setup);
  this->EstimateButton->setObjectName("EstimateButton");
  this->EstimateButton->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Estimate the initial parameters with 3DBarolo and show them (Manual mode): edit them, then Create or Fit."));
  modeRow->addWidget(this->ModeButton, 1);
  modeRow->addWidget(this->EstimateButton);
  layout->addLayout(modeRow);

  // beam of the data
  QWidget* beam = new QWidget(setup);
  QVBoxLayout* beamLayout = new QVBoxLayout(beam);
  beamLayout->setContentsMargins(0, 0, 0, 0);
  beamLayout->setSpacing(6);
  beamLayout->addWidget(Overline(qSlicerAstroModelingModuleWidget::tr("Beam"), beam));
  QHBoxLayout* beamRow = new QHBoxLayout();
  beamRow->setSpacing(6);
  const char* beamNames[3] = {"BeamMajor", "BeamMinor", "BeamPA"};
  const QString beamLabels[3] = {qSlicerAstroModelingModuleWidget::tr("maj"), qSlicerAstroModelingModuleWidget::tr("min"),
                                 qSlicerAstroModelingModuleWidget::tr("PA")};
  const QString beamTips[3] = {
    qSlicerAstroModelingModuleWidget::tr("Major axis (FWHM) of the beam, arc seconds."),
    qSlicerAstroModelingModuleWidget::tr("Minor axis (FWHM) of the beam, arc seconds."),
    qSlicerAstroModelingModuleWidget::tr("Position angle of the major axis, degrees from North to East.")};
  for (int i = 0; i < 3; i++)
    {
    this->BeamFields[i] = new qSlicerAstroValueField(beam);
    this->BeamFields[i]->setObjectName(QString("%1Field").arg(beamNames[i]));
    this->BeamFields[i]->setLeadingText(beamLabels[i]);
    this->BeamFields[i]->setUnit(QString::fromUtf8(i < 2 ? "″" : "°"));
    this->BeamFields[i]->setDecimals(2);
    this->BeamFields[i]->setRange(i < 2 ? 0. : -180., i < 2 ? 36000. : 180.);
    this->BeamFields[i]->setToolTip(beamTips[i] + qSlicerAstroModelingModuleWidget::tr(
      "\n\nThe beam of the data (saved with it): from the FITS header, or estimated from the noise."));
    QObject::connect(this->BeamFields[i], SIGNAL(editingFinished()), q, SLOT(onBeamEdited()));
    beamRow->addWidget(this->BeamFields[i], 1);
    }
  beamLayout->addLayout(beamRow);
  this->BeamStatus = RowLabel(QString(), beam);
  this->BeamStatus->setObjectName("BeamStatusLabel");
  this->BeamStatus->setWordWrap(true);
  beamLayout->addWidget(this->BeamStatus);
  layout->addWidget(beam);

  // rings
  QWidget* rings = new QWidget(setup);
  QGridLayout* ringsGrid = new QGridLayout(rings);
  ringsGrid->setContentsMargins(0, 0, 0, 0);
  ringsGrid->setHorizontalSpacing(8);
  ringsGrid->setVerticalSpacing(6);
  ringsGrid->addWidget(Overline(qSlicerAstroModelingModuleWidget::tr("Rings"), rings), 0, 0, 1, 3);
  this->addNumberRow({"NumberOfRings", qSlicerAstroModelingModuleWidget::tr("Rings"), QString(), -1., 1000., 1., 0,
    true, qSlicerAstroModelingModuleWidget::tr("Number of rings (NRADII)."),
    [](ParametersNode* p) { return static_cast<double>(p->GetNumberOfRings()); },
    [](ParametersNode* p, double v) { p->SetNumberOfRings(static_cast<int>(std::lround(v))); }, nullptr, nullptr},
    ringsGrid, rings, false);
  this->addNumberRow({"RadSep", qSlicerAstroModelingModuleWidget::tr("Width"), QString::fromUtf8("″"), -1., 3600., 1.,
    2, true, qSlicerAstroModelingModuleWidget::tr("Width of the rings (RADSEP), arc seconds."),
    [](ParametersNode* p) { return p->GetRadSep(); }, [](ParametersNode* p, double v) { p->SetRadSep(v); },
    nullptr, nullptr}, ringsGrid, rings, false);
  layout->addWidget(rings);

  // geometry and kinematics, with the free parameters of the fit
  QWidget* geometry = new QWidget(setup);
  QGridLayout* grid = new QGridLayout(geometry);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  grid->setColumnStretch(1, 1);
  grid->addWidget(Overline(qSlicerAstroModelingModuleWidget::tr("Geometry and kinematics"), geometry), 0, 0, 1, 2);
  QLabel* freeTitle = RowLabel(qSlicerAstroModelingModuleWidget::tr("fit"), geometry);
  freeTitle->setToolTip(qSlicerAstroModelingModuleWidget::tr("The free parameters of the fit."));
  grid->addWidget(freeTitle, 0, 2, Qt::AlignCenter);
  const NumberParameter mainParameters[] = {
    {"XCenter", qSlicerAstroModelingModuleWidget::tr("Centre x"), qSlicerAstroModelingModuleWidget::tr("px"), -1., 100000.,
     1., 2, true, qSlicerAstroModelingModuleWidget::tr("Centre of the galaxy, pixel x (XPOS, from 0)."),
     [](ParametersNode* p) { return p->GetXCenter(); }, [](ParametersNode* p, double v) { p->SetXCenter(v); },
     [](ParametersNode* p) { return p->GetXCenterFit(); }, [](ParametersNode* p, bool f) { p->SetXCenterFit(f); }},
    {"YCenter", qSlicerAstroModelingModuleWidget::tr("Centre y"), qSlicerAstroModelingModuleWidget::tr("px"), -1., 100000.,
     1., 2, true, qSlicerAstroModelingModuleWidget::tr("Centre of the galaxy, pixel y (YPOS, from 0)."),
     [](ParametersNode* p) { return p->GetYCenter(); }, [](ParametersNode* p, double v) { p->SetYCenter(v); },
     [](ParametersNode* p) { return p->GetYCenterFit(); }, [](ParametersNode* p, bool f) { p->SetYCenterFit(f); }},
    {"SystemicVelocity", qSlicerAstroModelingModuleWidget::tr("Systemic velocity"), "km/s", -1., 1.e6, 5., 2, true,
     qSlicerAstroModelingModuleWidget::tr("Systemic velocity (VSYS), optical definition."),
     [](ParametersNode* p) { return p->GetSystemicVelocity(); },
     [](ParametersNode* p, double v) { p->SetSystemicVelocity(v); },
     [](ParametersNode* p) { return p->GetSystemicVelocityFit(); },
     [](ParametersNode* p, bool f) { p->SetSystemicVelocityFit(f); }},
    {"RotationVelocity", qSlicerAstroModelingModuleWidget::tr("Rotation velocity"), "km/s", -1., 1000., 5., 2, true,
     qSlicerAstroModelingModuleWidget::tr("Rotation velocity (VROT)."),
     [](ParametersNode* p) { return p->GetRotationVelocity(); },
     [](ParametersNode* p, double v) { p->SetRotationVelocity(v); },
     [](ParametersNode* p) { return p->GetRotationVelocityFit(); },
     [](ParametersNode* p, bool f) { p->SetRotationVelocityFit(f); }},
    {"VelocityDispersion", qSlicerAstroModelingModuleWidget::tr("Dispersion"), "km/s", -1., 500., 1., 2, true,
     qSlicerAstroModelingModuleWidget::tr("Velocity dispersion (VDISP)."),
     [](ParametersNode* p) { return p->GetVelocityDispersion(); },
     [](ParametersNode* p, double v) { p->SetVelocityDispersion(v); },
     [](ParametersNode* p) { return p->GetVelocityDispersionFit(); },
     [](ParametersNode* p, bool f) { p->SetVelocityDispersionFit(f); }},
    {"Inclination", qSlicerAstroModelingModuleWidget::tr("Inclination"), QString::fromUtf8("°"), -1., 90., 1., 2, true,
     qSlicerAstroModelingModuleWidget::tr("Inclination of the disk (INC)."),
     [](ParametersNode* p) { return p->GetInclination(); }, [](ParametersNode* p, double v) { p->SetInclination(v); },
     [](ParametersNode* p) { return p->GetInclinationFit(); }, [](ParametersNode* p, bool f) { p->SetInclinationFit(f); }},
    {"PositionAngle", qSlicerAstroModelingModuleWidget::tr("Position angle"), QString::fromUtf8("°"), -1., 360., 1., 2,
     true, qSlicerAstroModelingModuleWidget::tr("Position angle of the receding major axis, from North to East (PA)."),
     [](ParametersNode* p) { return p->GetPositionAngle(); }, [](ParametersNode* p, double v) { p->SetPositionAngle(v); },
     [](ParametersNode* p) { return p->GetPositionAngleFit(); },
     [](ParametersNode* p, bool f) { p->SetPositionAngleFit(f); }},
    {"ScaleHeight", qSlicerAstroModelingModuleWidget::tr("Scale height"), QString::fromUtf8("″"), -1., 1000., 1., 2, true,
     qSlicerAstroModelingModuleWidget::tr("Scale height of the disk (Z0), arc seconds."),
     [](ParametersNode* p) { return p->GetScaleHeight(); }, [](ParametersNode* p, double v) { p->SetScaleHeight(v); },
     [](ParametersNode* p) { return p->GetScaleHeightFit(); }, [](ParametersNode* p, bool f) { p->SetScaleHeightFit(f); }},
  };
  for (const NumberParameter& parameter : mainParameters)
    {
    this->addNumberRow(parameter, grid, geometry, true);
    }
  layout->addWidget(geometry);

  this->AdvancedFold = new qSlicerAstroFoldSection(setup);
  this->AdvancedFold->setObjectName("AdvancedFold");
  this->AdvancedFold->setTitle(qSlicerAstroModelingModuleWidget::tr("Advanced"));
  this->AdvancedFold->setSummary(qSlicerAstroModelingModuleWidget::tr("VRAD, VVERT, dV/dz, ZCYL, weights, 2nd stage"));
  this->AdvancedFold->setSettingsKey("SlicerAstro/Modeling/AdvancedFold");
  this->AdvancedFold->setCollapsed(true);
  this->AdvancedFold->contentLayout()->addWidget(this->buildAdvanced(this->AdvancedFold));
  layout->addWidget(this->AdvancedFold);

  QObject::connect(this->RegionButton, SIGNAL(currentIndexChanged(int)), q, SLOT(onRegionChanged(int)));
  QObject::connect(this->ModeButton, SIGNAL(currentIndexChanged(int)), q, SLOT(onModeChanged(int)));
  QObject::connect(this->EstimateButton, SIGNAL(clicked()), q, SLOT(estimate()));
  QObject::connect(this->SegmentsTableView, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                   q, SLOT(onMaskSegmentSelectionChanged()));
  return setup;
}

//-----------------------------------------------------------------------------
QWidget* qSlicerAstroModelingModuleWidgetPrivate::buildAdvanced(QWidget* parent)
{
  Q_Q(qSlicerAstroModelingModuleWidget);
  QWidget* advanced = new QWidget(parent);
  QVBoxLayout* layout = new QVBoxLayout(advanced);
  layout->setContentsMargins(0, 4, 0, 0);
  layout->setSpacing(12);

  // more parameters of the rings
  QWidget* rings = new QWidget(advanced);
  QGridLayout* grid = new QGridLayout(rings);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  grid->setColumnStretch(1, 1);
  grid->addWidget(Overline(qSlicerAstroModelingModuleWidget::tr("Rings"), rings), 0, 0, 1, 3);
  const NumberParameter ringParameters[] = {
    {"RadialVelocity", qSlicerAstroModelingModuleWidget::tr("Radial velocity"), "km/s", -1000., 1000., 1., 2, false,
     qSlicerAstroModelingModuleWidget::tr("Radial velocity (VRAD); not estimated by 3DBarolo."),
     [](ParametersNode* p) { return p->GetRadialVelocity(); }, [](ParametersNode* p, double v) { p->SetRadialVelocity(v); },
     [](ParametersNode* p) { return p->GetRadialVelocityFit(); },
     [](ParametersNode* p, bool f) { p->SetRadialVelocityFit(f); }},
    {"VerticalVelocity", qSlicerAstroModelingModuleWidget::tr("Vertical velocity"), "km/s", -1000., 1000., 1., 2, false,
     qSlicerAstroModelingModuleWidget::tr("Vertical velocity (VVERT); not estimated by 3DBarolo."),
     [](ParametersNode* p) { return p->GetVerticalVelocity(); },
     [](ParametersNode* p, double v) { p->SetVerticalVelocity(v); }, nullptr, nullptr},
    {"VerticalRotationalGradient", qSlicerAstroModelingModuleWidget::tr("dV/dz"), QString::fromUtf8("km/s/″"), -1000.,
     1000., 1., 2, false, qSlicerAstroModelingModuleWidget::tr("Vertical gradient of the rotation velocity (DVDZ)."),
     [](ParametersNode* p) { return p->GetVerticalRotationalGradient(); },
     [](ParametersNode* p, double v) { p->SetVerticalRotationalGradient(v); }, nullptr, nullptr},
    {"VerticalRotationalGradientHeight", qSlicerAstroModelingModuleWidget::tr("ZCYL"), QString::fromUtf8("″"), -1000.,
     1000., 1., 2, false,
     qSlicerAstroModelingModuleWidget::tr("Height where the vertical gradient of the rotation starts (ZCYL)."),
     [](ParametersNode* p) { return p->GetVerticalRotationalGradientHeight(); },
     [](ParametersNode* p, double v) { p->SetVerticalRotationalGradientHeight(v); }, nullptr, nullptr},
    {"ColumnDensity", qSlicerAstroModelingModuleWidget::tr("Column density"),
     QString::fromUtf8("10²⁰ cm⁻²"), 0., 1.e5, 1., 2, false,
     qSlicerAstroModelingModuleWidget::tr("Column density of the rings (DENS); not fitted by 3DBarolo."),
     [](ParametersNode* p) { return p->GetColumnDensity(); }, [](ParametersNode* p, double v) { p->SetColumnDensity(v); },
     nullptr, nullptr},
    {"Distance", qSlicerAstroModelingModuleWidget::tr("Distance"), "Mpc", -1., 1.e5, 1., 2, true,
     qSlicerAstroModelingModuleWidget::tr("Distance of the galaxy (DISTANCE)."),
     [](ParametersNode* p) { return p->GetDistance(); }, [](ParametersNode* p, double v) { p->SetDistance(v); },
     nullptr, nullptr},
    {"InclinationError", qSlicerAstroModelingModuleWidget::tr("Inclination range"), QString::fromUtf8("±°"), 0., 90., 1.,
     2, false, qSlicerAstroModelingModuleWidget::tr("Maximum change of the inclination in the fit (DELTAINC)."),
     [](ParametersNode* p) { return p->GetInclinationError(); },
     [](ParametersNode* p, double v) { p->SetInclinationError(v); }, nullptr, nullptr},
    {"PositionAngleError", qSlicerAstroModelingModuleWidget::tr("Position angle range"), QString::fromUtf8("±°"), 0.,
     360., 1., 2, false, qSlicerAstroModelingModuleWidget::tr("Maximum change of the position angle in the fit (DELTAPHI)."),
     [](ParametersNode* p) { return p->GetPositionAngleError(); },
     [](ParametersNode* p, double v) { p->SetPositionAngleError(v); }, nullptr, nullptr},
    {"RotationVelocityError", qSlicerAstroModelingModuleWidget::tr("Rotation range"), QString::fromUtf8("±km/s"), 0.,
     10000., 1., 1, false,
     qSlicerAstroModelingModuleWidget::tr("Maximum change of the rotation velocity in the fit (DELTAVROT)."),
     [](ParametersNode* p) { return p->GetRotationVelocityError(); },
     [](ParametersNode* p, double v) { p->SetRotationVelocityError(v); }, nullptr, nullptr},
    {"MinimumVelocityDispersion", qSlicerAstroModelingModuleWidget::tr("Dispersion min"), "km/s", 0., 10000., 1., 1,
     false, qSlicerAstroModelingModuleWidget::tr("Minimum velocity dispersion in the fit (MINVDISP)."),
     [](ParametersNode* p) { return p->GetMinimumVelocityDispersion(); },
     [](ParametersNode* p, double v) { p->SetMinimumVelocityDispersion(v); }, nullptr, nullptr},
    {"MaximumVelocityDispersion", qSlicerAstroModelingModuleWidget::tr("Dispersion max"), "km/s", 0., 10000., 1., 1,
     false, qSlicerAstroModelingModuleWidget::tr("Maximum velocity dispersion in the fit (MAXVDISP)."),
     [](ParametersNode* p) { return p->GetMaximumVelocityDispersion(); },
     [](ParametersNode* p, double v) { p->SetMaximumVelocityDispersion(v); }, nullptr, nullptr},
  };
  for (const NumberParameter& parameter : ringParameters)
    {
    this->addNumberRow(parameter, grid, rings, true);
    }
  layout->addWidget(rings);

  // the model and the fit
  QWidget* fit = new QWidget(advanced);
  QGridLayout* fitGrid = new QGridLayout(fit);
  fitGrid->setContentsMargins(0, 0, 0, 0);
  fitGrid->setHorizontalSpacing(8);
  fitGrid->setVerticalSpacing(6);
  fitGrid->setColumnStretch(1, 1);
  fitGrid->addWidget(Overline(qSlicerAstroModelingModuleWidget::tr("Model and fit"), fit), 0, 0, 1, 2);
  std::function<void(const QString&, QComboBox*, const QString&)> addComboRow =
    [fit, fitGrid](const QString& label, QComboBox* combo, const QString& toolTip)
    {
    const int row = fitGrid->rowCount();
    QLabel* rowLabel = RowLabel(label, fit);
    rowLabel->setToolTip(toolTip);
    combo->setToolTip(toolTip);
    fitGrid->addWidget(rowLabel, row, 0);
    fitGrid->addWidget(combo, row, 1);
    };
  this->LayerTypeCombo = new QComboBox(fit);
  this->LayerTypeCombo->setObjectName("LayerTypeCombo");
  this->LayerTypeCombo->addItems(QStringList() << qSlicerAstroModelingModuleWidget::tr("Gaussian")
    << qSlicerAstroModelingModuleWidget::tr("Sech²") << qSlicerAstroModelingModuleWidget::tr("Exponential")
    << qSlicerAstroModelingModuleWidget::tr("Lorentzian") << qSlicerAstroModelingModuleWidget::tr("Box"));
  addComboRow(qSlicerAstroModelingModuleWidget::tr("Layer"), this->LayerTypeCombo,
              qSlicerAstroModelingModuleWidget::tr("Vertical density profile of the disk (LTYPE)."));
  const NumberParameter modelParameters[] = {
    {"NumberOfClounds", qSlicerAstroModelingModuleWidget::tr("Clouds"), QString(), -1., 1.e6, 1., 0, true,
     qSlicerAstroModelingModuleWidget::tr("Number of clouds per ring of the model (NV)."),
     [](ParametersNode* p) { return static_cast<double>(p->GetNumberOfClounds()); },
     [](ParametersNode* p, double v) { p->SetNumberOfClounds(static_cast<int>(std::lround(v))); }, nullptr, nullptr},
    {"CloudsColumnDensity", qSlicerAstroModelingModuleWidget::tr("Cloud density"), QString::fromUtf8("10²⁰ cm⁻²"), 0.,
     1.e5, 1., 2, false, qSlicerAstroModelingModuleWidget::tr("Column density of a cloud (CDENS); 0: estimated."),
     [](ParametersNode* p) { return p->GetCloudsColumnDensity(); },
     [](ParametersNode* p, double v) { p->SetCloudsColumnDensity(v); }, nullptr, nullptr},
    {"Tollerance", qSlicerAstroModelingModuleWidget::tr("Tolerance"), QString(), 0.000001, 0.1, 0.001, 6, false,
     qSlicerAstroModelingModuleWidget::tr("Tolerance of the minimization (TOL)."),
     [](ParametersNode* p) { return p->GetTollerance(); }, [](ParametersNode* p, double v) { p->SetTollerance(v); },
     nullptr, nullptr},
    {"InstrumentalBroadening", qSlicerAstroModelingModuleWidget::tr("Spectral resolution"),
     qSlicerAstroModelingModuleWidget::tr("channels"), -1., 100., 0.1, 2, true,
     qSlicerAstroModelingModuleWidget::tr("Instrumental spectral broadening (LINEAR), e.g. for Hanning smoothed data."),
     [](ParametersNode* p) { return p->GetInstrumentalBroadening(); },
     [](ParametersNode* p, double v) { p->SetInstrumentalBroadening(v); }, nullptr, nullptr},
    {"StartRing", qSlicerAstroModelingModuleWidget::tr("First ring"), QString(), 0., 10000., 1., 0, false,
     qSlicerAstroModelingModuleWidget::tr("First ring fitted (STARTRAD, from 0): the inner rings keep their values."),
     [](ParametersNode* p) { return static_cast<double>(p->GetStartRing()); },
     [](ParametersNode* p, double v) { p->SetStartRing(static_cast<int>(std::lround(v))); }, nullptr, nullptr},
    {"BlankWeight", qSlicerAstroModelingModuleWidget::tr("Blank weight"), QString(), 0., 10., 0.1, 2, false,
     qSlicerAstroModelingModuleWidget::tr("Power of the weight of the blank pixels in the fit (BWEIGHT)."),
     [](ParametersNode* p) { return p->GetBlankWeight(); }, [](ParametersNode* p, double v) { p->SetBlankWeight(v); },
     nullptr, nullptr},
    {"Redshift", qSlicerAstroModelingModuleWidget::tr("Redshift"), QString(), 0., 20., 0.001, 6, false,
     qSlicerAstroModelingModuleWidget::tr("Redshift of the galaxy (REDSHIFT)."),
     [](ParametersNode* p) { return p->GetRedshift(); }, [](ParametersNode* p, double v) { p->SetRedshift(v); },
     nullptr, nullptr},
  };
  for (const NumberParameter& parameter : modelParameters)
    {
    this->addNumberRow(parameter, fitGrid, fit, false);
    }
  this->FittingFunctionCombo = new QComboBox(fit);
  this->FittingFunctionCombo->setObjectName("FittingFunctionCombo");
  this->FittingFunctionCombo->addItems(QStringList() << QString::fromUtf8("χ²") << "|m - o|" << "|m - o| / (m + o)");
  addComboRow(qSlicerAstroModelingModuleWidget::tr("Residuals"), this->FittingFunctionCombo,
              qSlicerAstroModelingModuleWidget::tr("Function minimized by the fit (FTYPE)."));
  this->WeightingFunctionCombo = new QComboBox(fit);
  this->WeightingFunctionCombo->setObjectName("WeightingFunctionCombo");
  this->WeightingFunctionCombo->addItems(QStringList() << qSlicerAstroModelingModuleWidget::tr("Uniform")
    << qSlicerAstroModelingModuleWidget::tr("Cosine") << qSlicerAstroModelingModuleWidget::tr("Cosine²"));
  addComboRow(qSlicerAstroModelingModuleWidget::tr("Azimuthal weight"), this->WeightingFunctionCombo,
              qSlicerAstroModelingModuleWidget::tr("Weight of the pixels along the rings (WFUNC)."));
  this->SideCombo = new QComboBox(fit);
  this->SideCombo->setObjectName("SideCombo");
  this->SideCombo->addItem(qSlicerAstroModelingModuleWidget::tr("Both"), "B");
  this->SideCombo->addItem(qSlicerAstroModelingModuleWidget::tr("Approaching"), "A");
  this->SideCombo->addItem(qSlicerAstroModelingModuleWidget::tr("Receding"), "R");
  addComboRow(qSlicerAstroModelingModuleWidget::tr("Side"), this->SideCombo,
              qSlicerAstroModelingModuleWidget::tr("Side of the galaxy fitted (SIDE)."));
  QLabel* normalizeLabel = RowLabel(qSlicerAstroModelingModuleWidget::tr("Normalization"), fit);
  const QString normalizeTip = qSlicerAstroModelingModuleWidget::tr(
    "Normalization of the model to the data (NORM): none, pixel by pixel (local) or along the rings (azimuthal). "
    "Local and azimuthal need the mask of the galaxy.");
  normalizeLabel->setToolTip(normalizeTip);
  this->NormalizeButton = new qSlicerAstroSegmentedButton(fit);
  this->NormalizeButton->setObjectName("NormalizeButton");
  this->NormalizeButton->setToolTip(normalizeTip);
  this->NormalizeButton->addButton(qSlicerAstroModelingModuleWidget::tr("None"));
  this->NormalizeButton->addButton(qSlicerAstroModelingModuleWidget::tr("Local"));
  this->NormalizeButton->addButton(qSlicerAstroModelingModuleWidget::tr("Azimuthal"));
  const int normalizeRow = fitGrid->rowCount();
  fitGrid->addWidget(normalizeLabel, normalizeRow, 0);
  fitGrid->addWidget(this->NormalizeButton, normalizeRow, 1);

  QHBoxLayout* secondStage = new QHBoxLayout();
  secondStage->setSpacing(6);
  this->TwoStageCheckBox = new QCheckBox(qSlicerAstroModelingModuleWidget::tr("Second stage"), fit);
  this->TwoStageCheckBox->setObjectName("TwoStageCheckBox");
  this->TwoStageCheckBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Second fit stage (TWOSTAGE): the inclination and position angle of the first stage are regularized and "
    "the other parameters are fitted again."));
  this->RegularizationCombo = new QComboBox(fit);
  this->RegularizationCombo->setObjectName("RegularizationCombo");
  this->RegularizationCombo->setEditable(true);
  this->RegularizationCombo->addItems(QStringList() << "auto" << "bezier" << "median" << "0" << "1" << "2" << "3");
  this->RegularizationCombo->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Regularization of the second stage (REGTYPE): auto, bezier, median, or the degree of a polynomial."));
  secondStage->addWidget(this->TwoStageCheckBox);
  secondStage->addWidget(this->RegularizationCombo, 1);
  fitGrid->addLayout(secondStage, fitGrid->rowCount(), 0, 1, 2);
  this->ADRIFTCheckBox = new QCheckBox(qSlicerAstroModelingModuleWidget::tr("Asymmetric drift correction"), fit);
  this->ADRIFTCheckBox->setObjectName("ADRIFTCheckBox");
  this->ADRIFTCheckBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Correct the rotation curve for the asymmetric drift (ADRIFT)."));
  fitGrid->addWidget(this->ADRIFTCheckBox, fitGrid->rowCount(), 0, 1, 2);
  this->NormalizeCubeCheckBox = new QCheckBox(qSlicerAstroModelingModuleWidget::tr("Normalize the cube"), fit);
  this->NormalizeCubeCheckBox->setObjectName("NormalizeCubeCheckBox");
  this->NormalizeCubeCheckBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Normalize the flux values of the cube before the fit (NORMALCUBE)."));
  fitGrid->addWidget(this->NormalizeCubeCheckBox, fitGrid->rowCount(), 0, 1, 2);
  this->EstimateErrorsCheckBox = new QCheckBox(qSlicerAstroModelingModuleWidget::tr("Estimate the errors (slow)"), fit);
  this->EstimateErrorsCheckBox->setObjectName("EstimateErrorsCheckBox");
  this->EstimateErrorsCheckBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Estimate the errors of the fitted parameters (FLAGERRORS): much slower fit."));
  fitGrid->addWidget(this->EstimateErrorsCheckBox, fitGrid->rowCount(), 0, 1, 2);
  QLabel* linesLabel = RowLabel(qSlicerAstroModelingModuleWidget::tr("Emission lines"), fit);
  this->RestFrequenciesEdit = new QLineEdit(fit);
  this->RestFrequenciesEdit->setObjectName("RestFrequenciesEdit");
  this->RestFrequenciesEdit->setPlaceholderText(qSlicerAstroModelingModuleWidget::tr("rest frequencies (Hz)"));
  this->RestFrequenciesEdit->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Rest frequencies of the emission lines in Hz, comma separated (RESTFREQ). Empty: the rest frequency of the "
    "header."));
  this->RelativeIntensitiesEdit = new QLineEdit(fit);
  this->RelativeIntensitiesEdit->setObjectName("RelativeIntensitiesEdit");
  this->RelativeIntensitiesEdit->setPlaceholderText(qSlicerAstroModelingModuleWidget::tr("relative intensities"));
  this->RelativeIntensitiesEdit->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Relative intensities of the emission lines, comma separated (RELINT; 1 for each line if empty)."));
  const int linesRow = fitGrid->rowCount();
  fitGrid->addWidget(linesLabel, linesRow, 0);
  fitGrid->addWidget(this->RestFrequenciesEdit, linesRow, 1);
  fitGrid->addWidget(this->RelativeIntensitiesEdit, linesRow + 1, 1);
  layout->addWidget(fit);

  // source search (without a mask)
  this->SearchGroup = new QWidget(advanced);
  this->SearchGroup->setObjectName("SearchGroup");
  this->SearchGroup->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Without a mask, 3DBarolo searches the galaxy in the cube with these parameters."));
  QGridLayout* searchGrid = new QGridLayout(this->SearchGroup);
  searchGrid->setContentsMargins(0, 0, 0, 0);
  searchGrid->setHorizontalSpacing(8);
  searchGrid->setVerticalSpacing(6);
  searchGrid->setColumnStretch(1, 1);
  searchGrid->addWidget(Overline(qSlicerAstroModelingModuleWidget::tr("Source search"), this->SearchGroup), 0, 0, 1, 2);
  const NumberParameter searchParameters[] = {
    {"SearchSNRCut", qSlicerAstroModelingModuleWidget::tr("Detection"), QString::fromUtf8("σ"), 0.5, 100., 0.5, 1, false,
     qSlicerAstroModelingModuleWidget::tr("Signal to noise of the detections (SNRCUT)."),
     [](ParametersNode* p) { return p->GetSearchSNRCut(); }, [](ParametersNode* p, double v) { p->SetSearchSNRCut(v); },
     nullptr, nullptr},
    {"SearchThreshold", qSlicerAstroModelingModuleWidget::tr("Threshold"), QString(), 0., 1.e6, 0.001, 6, false,
     qSlicerAstroModelingModuleWidget::tr("Flux threshold of the detections in the units of the data (THRESHOLD); "
                                          "0: from the signal to noise."),
     [](ParametersNode* p) { return p->GetSearchThreshold(); },
     [](ParametersNode* p, double v) { p->SetSearchThreshold(v); }, nullptr, nullptr},
    {"SearchMinChannels", qSlicerAstroModelingModuleWidget::tr("Minimum channels"), QString(), 1., 10000., 1., 0, false,
     qSlicerAstroModelingModuleWidget::tr("Minimum channels of a source (MINCHANNELS)."),
     [](ParametersNode* p) { return static_cast<double>(p->GetSearchMinChannels()); },
     [](ParametersNode* p, double v) { p->SetSearchMinChannels(static_cast<int>(std::lround(v))); }, nullptr, nullptr},
    {"SearchGrowthCut", qSlicerAstroModelingModuleWidget::tr("Growth"), QString::fromUtf8("σ"), 0., 100., 0.5, 1, false,
     qSlicerAstroModelingModuleWidget::tr("Signal to noise down to which the detections are grown (GROWTHCUT)."),
     [](ParametersNode* p) { return p->GetSearchGrowthCut(); },
     [](ParametersNode* p, double v) { p->SetSearchGrowthCut(v); }, nullptr, nullptr},
  };
  for (const NumberParameter& parameter : searchParameters)
    {
    this->addNumberRow(parameter, searchGrid, this->SearchGroup, false);
    }
  layout->addWidget(this->SearchGroup);

  // the controls of the parameters node that are not numbers
  QObject::connect(this->LayerTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this](int index)
    {
    if (!this->Updating && this->parametersNode)
      {
      this->parametersNode->SetLayerType(index);
      }
    });
  QObject::connect(this->FittingFunctionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this](int index)
    {
    if (!this->Updating && this->parametersNode)
      {
      this->parametersNode->SetFittingFunction(index + 1);
      }
    });
  QObject::connect(this->WeightingFunctionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q,
                   [this](int index)
    {
    if (!this->Updating && this->parametersNode)
      {
      this->parametersNode->SetWeightingFunction(index);
      }
    });
  QObject::connect(this->SideCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this](int index)
    {
    if (!this->Updating && this->parametersNode)
      {
      this->parametersNode->SetSide(this->SideCombo->itemData(index).toString().toStdString());
      }
    });
  QObject::connect(this->NormalizeButton, &qSlicerAstroSegmentedButton::currentIndexChanged, q, [this](int index)
    {
    if (!this->Updating && this->parametersNode)
      {
      const char* values[3] = {"NONE", "LOCAL", "AZIM"};
      this->parametersNode->SetNormalize(values[std::max(0, std::min(2, index))]);
      }
    });
  QObject::connect(this->RegularizationCombo, &QComboBox::currentTextChanged, q, [this](const QString& text)
    {
    if (!this->Updating && this->parametersNode)
      {
      this->parametersNode->SetRegularizationType(text.toStdString());
      }
    });
  QCheckBox* checkBoxes[4] = {this->TwoStageCheckBox, this->ADRIFTCheckBox, this->NormalizeCubeCheckBox,
                              this->EstimateErrorsCheckBox};
  for (QCheckBox* checkBox : checkBoxes)
    {
    QObject::connect(checkBox, &QCheckBox::toggled, q, [this, checkBox](bool checked)
      {
      if (this->Updating || !this->parametersNode)
        {
        return;
        }
      if (checkBox == this->TwoStageCheckBox)
        {
        this->parametersNode->SetTwoStage(checked);
        }
      else if (checkBox == this->ADRIFTCheckBox)
        {
        this->parametersNode->SetADRIFTCorrection(checked);
        }
      else if (checkBox == this->NormalizeCubeCheckBox)
        {
        this->parametersNode->SetNormalizeCube(checked);
        }
      else
        {
        this->parametersNode->SetEstimateErrors(checked);
        }
      });
    }
  QObject::connect(this->RestFrequenciesEdit, &QLineEdit::editingFinished, q, [this]()
    {
    if (!this->Updating && this->parametersNode)
      {
      this->parametersNode->SetRestFrequencies(this->RestFrequenciesEdit->text().trimmed().toStdString());
      }
    });
  QObject::connect(this->RelativeIntensitiesEdit, &QLineEdit::editingFinished, q, [this]()
    {
    if (!this->Updating && this->parametersNode)
      {
      this->parametersNode->SetRelativeIntensities(this->RelativeIntensitiesEdit->text().trimmed().toStdString());
      }
    });
  return advanced;
}

//-----------------------------------------------------------------------------
QWidget* qSlicerAstroModelingModuleWidgetPrivate::buildResults(QWidget* parent)
{
  Q_Q(qSlicerAstroModelingModuleWidget);
  QWidget* results = new QWidget(parent);
  results->setObjectName("Results");
  QVBoxLayout* layout = new QVBoxLayout(results);
  layout->setContentsMargins(12, 14, 12, 14);
  layout->setSpacing(10);

  // the rings: plot and table
  QHBoxLayout* plotRow = new QHBoxLayout();
  plotRow->setSpacing(6);
  plotRow->addWidget(Overline(qSlicerAstroModelingModuleWidget::tr("Rings"), results));
  plotRow->addStretch(1);
  this->RingParameterCombo = new QComboBox(results);
  this->RingParameterCombo->setObjectName("RingParameterCombo");
  this->RingParameterCombo->setToolTip(qSlicerAstroModelingModuleWidget::tr("Parameter of the rings in the plot."));
  for (const RingParameter& parameter : RingParameters)
    {
    this->RingParameterCombo->addItem(qSlicerAstroModelingModuleWidget::tr(parameter.Label), QString(parameter.Column));
    }
  plotRow->addWidget(this->RingParameterCombo);
  QToolButton* plotView = new QToolButton(results);
  plotView->setObjectName("PlotViewButton");
  plotView->setProperty("astroRole", "iconButton");
  plotView->setIcon(qSlicerAstroTheme::instance()->icon("plot", "muted"));
  plotView->setToolTip(qSlicerAstroModelingModuleWidget::tr("Open the plot of the rings in a plot view of the layout."));
  plotRow->addWidget(plotView);
  layout->addLayout(plotRow);
  this->RingPlot = new qSlicerAstroRingPlot(results);
  this->RingPlot->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "The parameter of the rings against the radius; move the mouse over a ring for its values."));
  layout->addWidget(this->RingPlot);

  this->TableView = new qMRMLTableView(results);
  this->TableView->setObjectName("TableView");
  this->TableView->setMinimumHeight(140);
  this->TableView->setMaximumHeight(220);
  this->TableView->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Parameters of the rings: edit them (or paste, Ctrl+V), then Calculate model."));
  layout->addWidget(this->TableView);
  QHBoxLayout* tableRow = new QHBoxLayout();
  tableRow->setSpacing(6);
  QToolButton* copy = new QToolButton(results);
  copy->setObjectName("CopyButton");
  copy->setProperty("astroRole", "iconButton");
  copy->setIcon(qSlicerAstroTheme::instance()->icon("copy", "muted"));
  copy->setToolTip(qSlicerAstroModelingModuleWidget::tr("Copy the selected cells (Ctrl+C), e.g. to a spreadsheet."));
  QToolButton* paste = new QToolButton(results);
  paste->setObjectName("PasteButton");
  paste->setProperty("astroRole", "iconButton");
  paste->setIcon(qSlicerAstroTheme::instance()->icon("paste", "muted"));
  paste->setToolTip(qSlicerAstroModelingModuleWidget::tr("Paste values into the selected cells (Ctrl+V)."));
  QToolButton* exportButton = new QToolButton(results);
  exportButton->setObjectName("ExportButton");
  exportButton->setProperty("astroRole", "iconButton");
  exportButton->setIcon(qSlicerAstroTheme::instance()->icon("download", "muted"));
  exportButton->setToolTip(qSlicerAstroModelingModuleWidget::tr("Save the table of the rings as CSV."));
  this->UpdateButton = new qSlicerAstroRunButton(results);
  this->UpdateButton->setObjectName("UpdateButton");
  this->UpdateButton->setText(qSlicerAstroModelingModuleWidget::tr("Calculate model"));
  this->UpdateButton->pushButton()->setObjectName("CalculatePushButton");
  this->UpdateButton->pushButton()->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "The model of the rings of the table (e.g. edited rings): model, residuals and contours updated."));
  tableRow->addWidget(copy);
  tableRow->addWidget(paste);
  tableRow->addWidget(exportButton);
  tableRow->addWidget(this->UpdateButton, 1);
  layout->addLayout(tableRow);

  // display of the results
  layout->addWidget(Overline(qSlicerAstroModelingModuleWidget::tr("Display"), results));
  QHBoxLayout* contoursRow = new QHBoxLayout();
  contoursRow->setSpacing(4);
  this->DataContoursButton = Chip(qSlicerAstroModelingModuleWidget::tr("Data"),
    qSlicerAstroModelingModuleWidget::tr("Contours of the data at the contour level (2D views)."),
    "DataContoursButton", results);
  this->ModelContoursButton = Chip(qSlicerAstroModelingModuleWidget::tr("Model"),
    qSlicerAstroModelingModuleWidget::tr("Contours of the model at the contour level (2D views) and the model in 3D."),
    "ModelContoursButton", results);
  this->ModelRegionButton = Chip(qSlicerAstroModelingModuleWidget::tr("Model region"),
    qSlicerAstroModelingModuleWidget::tr("The voxels of the model (the mask of 3DBarolo), 3D views."),
    "ModelRegionButton", results);
  contoursRow->addWidget(this->DataContoursButton);
  contoursRow->addWidget(this->ModelContoursButton);
  contoursRow->addWidget(this->ModelRegionButton);
  contoursRow->addStretch(1);
  layout->addLayout(contoursRow);
  QGridLayout* displayGrid = new QGridLayout();
  displayGrid->setHorizontalSpacing(8);
  displayGrid->setVerticalSpacing(6);
  this->ContourLevelField = new qSlicerAstroValueField(results);
  this->ContourLevelField->setObjectName("ContourLevelField");
  this->ContourLevelField->setLeadingText(qSlicerAstroModelingModuleWidget::tr("Contours"));
  this->ContourLevelField->setUnit(QString::fromUtf8("σ"));
  this->ContourLevelField->setDecimals(2);
  this->ContourLevelField->setRange(-10., 50.);
  this->ContourLevelField->setSingleStep(0.5);
  this->ContourLevelField->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Contour level of the data and of the model, in units of the noise of the data (above 1.5 recommended)."));
  this->MajorOffsetField = new qSlicerAstroValueField(results);
  this->MajorOffsetField->setObjectName("MajorOffsetField");
  this->MajorOffsetField->setLeadingText(qSlicerAstroModelingModuleWidget::tr("Major axis"));
  this->MajorOffsetField->setUnit(QString::fromUtf8("°"));
  this->MajorOffsetField->setDecimals(1);
  this->MajorOffsetField->setRange(-180., 180.);
  this->MajorOffsetField->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Rotation of the PV slice along the major axis (yellow view) from the mean position angle of the rings."));
  this->MinorOffsetField = new qSlicerAstroValueField(results);
  this->MinorOffsetField->setObjectName("MinorOffsetField");
  this->MinorOffsetField->setLeadingText(qSlicerAstroModelingModuleWidget::tr("Minor axis"));
  this->MinorOffsetField->setUnit(QString::fromUtf8("°"));
  this->MinorOffsetField->setDecimals(1);
  this->MinorOffsetField->setRange(-180., 180.);
  this->MinorOffsetField->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Rotation of the PV slice along the minor axis (green view) from the mean position angle of the rings + 90°."));
  displayGrid->addWidget(this->ContourLevelField, 0, 0, 1, 2);
  displayGrid->addWidget(this->MajorOffsetField, 1, 0);
  displayGrid->addWidget(this->MinorOffsetField, 1, 1);
  layout->addLayout(displayGrid);
  QPushButton* showButton = new QPushButton(qSlicerAstroTheme::instance()->icon("viewFit", "text"),
                                            qSlicerAstroModelingModuleWidget::tr("Show in views"), results);
  showButton->setObjectName("ShowResultsButton");
  showButton->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "The results in the views: the cube in 3D, the PV slices along the axes with the rotation curve, the "
    "contours."));
  layout->addWidget(showButton);

  // the model and residual cubes
  this->ModelResult = new qSlicerAstroResultCard(results);
  this->ModelResult->setObjectName("ModelResult");
  this->ResidualResult = new qSlicerAstroResultCard(results);
  this->ResidualResult->setObjectName("ResidualResult");
  layout->addWidget(this->ModelResult);
  layout->addWidget(this->ResidualResult);

  QObject::connect(this->RingParameterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this](int index)
    {
    Q_Q(qSlicerAstroModelingModuleWidget);
    q->setRingParameter(this->RingParameterCombo->itemData(index).toString());
    });
  QObject::connect(plotView, SIGNAL(clicked()), q, SLOT(openPlotView()));
  QObject::connect(copy, SIGNAL(clicked()), this->TableView, SLOT(copySelection()));
  QObject::connect(paste, SIGNAL(clicked()), this->TableView, SLOT(pasteSelection()));
  QObject::connect(exportButton, &QToolButton::clicked, q, [q]() { q->exportTable(); });
  QObject::connect(showButton, SIGNAL(clicked()), q, SLOT(showResults()));
  const std::pair<QToolButton*, const char*> contourButtons[3] = {
    {this->DataContoursButton, vtkSlicerAstroModelingLogic::GetDataContoursSegmentID()},
    {this->ModelContoursButton, vtkSlicerAstroModelingLogic::GetModelContoursSegmentID()},
    {this->ModelRegionButton, vtkSlicerAstroModelingLogic::GetModelMaskSegmentID()}};
  for (const std::pair<QToolButton*, const char*>& entry : contourButtons)
    {
    const char* segmentID = entry.second;
    QObject::connect(entry.first, &QToolButton::toggled, q, [this, segmentID](bool checked)
      {
      if (!this->Updating && this->logic())
        {
        this->logic()->SetResultContoursVisibility(segmentID, checked);
        }
      });
    }
  QObject::connect(this->ContourLevelField, &qSlicerAstroValueField::editingFinished, q, [this]()
    {
    if (this->Updating || !this->parametersNode || !this->logic())
      {
      return;
      }
    this->parametersNode->SetContourLevel(this->ContourLevelField->value());
    this->logic()->UpdateResultContours(this->parametersNode);
    });
  for (qSlicerAstroValueField* field : {this->MajorOffsetField, this->MinorOffsetField})
    {
    QObject::connect(field, &qSlicerAstroValueField::editingFinished, q, [this, field]()
      {
      if (this->Updating || !this->parametersNode || !this->logic())
        {
        return;
        }
      if (field == this->MajorOffsetField)
        {
        this->parametersNode->SetYellowRotValue(field->value());
        }
      else
        {
        this->parametersNode->SetGreenRotValue(field->value());
        }
      this->logic()->SetPVSlices(this->parametersNode);
      });
    }
  for (qSlicerAstroResultCard* card : {this->ModelResult, this->ResidualResult})
    {
    QObject::connect(card, &qSlicerAstroResultCard::showRequested, q, [this, card]()
      {
      vtkSlicerAstroVolumeLogic* astroVolumeLogic = this->astroVolumeLogic();
      vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(card->volume());
      if (astroVolumeLogic && volume)
        {
        astroVolumeLogic->ToggleProductInViews(volume, this->inputVolume());
        card->updateShowButton();
        }
      });
    QObject::connect(card, &qSlicerAstroResultCard::deleted, q, [this]() { this->updateResults(); });
    }
  results->hide();
  return results;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::appendLog(const QString& text)
{
  if (text.isEmpty())
    {
    return;
    }
  QScrollBar* scrollBar = this->LogEdit->verticalScrollBar();
  const bool atEnd = !scrollBar || scrollBar->value() >= scrollBar->maximum() - 4;
  this->LogEdit->moveCursor(QTextCursor::End);
  this->LogEdit->insertPlainText(text);
  if (atEnd && scrollBar)
    {
    scrollBar->setValue(scrollBar->maximum());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::start(int operation)
{
  vtkSlicerAstroModelingLogic* logic = this->logic();
  if (!logic || !this->parametersNode || logic->IsOperationRunning())
    {
    return;
    }
  this->Message->clearMessage();
  // the button of the operation shows its progress and Cancel
  this->RunningButton = operation == ParametersNode::CREATE ? this->CreateButton :
    (operation == ParametersNode::FIT || operation == ParametersNode::ESTIMATE) ? this->FitButton : this->UpdateButton;
  const char* names[4] = {"estimate of the initial parameters", "create model", "fit", "calculate model"};
  this->appendLog(qSlicerAstroModelingModuleWidget::tr("\n== 3DBarolo: %1 ==\n")
                    .arg(names[std::max(0, std::min(3, operation))]));
  this->RunningButton->setRunning(true);
  this->updateRunning();
  this->ProgressTimer->start();
  if (!logic->StartOperation(this->parametersNode, operation, false))
    {
    this->ProgressTimer->stop();
    this->RunningButton->setRunning(false);
    this->RunningButton = nullptr;
    this->appendLog(QString::fromUtf8(logic->GetLastError()) + "\n");
    this->Message->showMessage("error", QString::fromUtf8(logic->GetLastError()));
    this->updateRunning();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::updateRunning()
{
  vtkSlicerAstroModelingLogic* logic = this->logic();
  const bool running = logic && logic->IsOperationRunning();
  const bool ready = this->parametersNode && this->inputVolume();
  this->Setup->setEnabled(ready && !running);
  this->EstimateButton->setEnabled(ready && !running);
  for (qSlicerAstroRunButton* button : {this->CreateButton, this->FitButton, this->UpdateButton})
    {
    if (button != this->RunningButton)
      {
      button->setEnabled(ready && !running);
      }
    }
  this->TableView->setEnabled(!running);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::updateBeam()
{
  vtkMRMLAstroVolumeNode* cube = this->inputVolume();
  double beam[3] = {0., 0., 0.};
  const bool known = cube && vtkSlicerAstroVolumeLogic::GetBeam(cube, beam);
  // no beam in the header: the estimate from the noise used by the
  // operations (computed once per cube)
  bool estimated = false;
  if (cube && !known)
    {
    if (this->EstimatedBeamVolumeID != QString::fromUtf8(cube->GetID()))
      {
      this->EstimatedBeamVolumeID = QString::fromUtf8(cube->GetID());
      this->EstimatedBeamValid = vtkSlicerAstroVolumeLogic::EstimateBeamFromNoise(cube, this->EstimatedBeam);
      }
    estimated = this->EstimatedBeamValid;
    if (estimated)
      {
      std::copy(this->EstimatedBeam, this->EstimatedBeam + 3, beam);
      }
    }
  for (int i = 0; i < 3; i++)
    {
    const QSignalBlocker blocker(this->BeamFields[i]);
    this->BeamFields[i]->setValue(i < 2 ? beam[i] * 3600. : beam[i]);
    }
  if (!cube)
    {
    this->BeamStatus->clear();
    }
  else if (known)
    {
    this->BeamStatus->setText(qSlicerAstroModelingModuleWidget::tr("The beam of the data (saved in its FITS header)."));
    }
  else if (estimated)
    {
    this->BeamStatus->setText(qSlicerAstroModelingModuleWidget::tr(
      "Beam ESTIMATED from the noise (the FITS header has none): used for the model and saved with the data. "
      "Check it, or enter the beam of the observation."));
    }
  else
    {
    this->BeamStatus->setText(qSlicerAstroModelingModuleWidget::tr(
      "No beam: the FITS header has none and the noise is not correlated. Enter the beam of the observation."));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::updateMask()
{
  Q_Q(qSlicerAstroModelingModuleWidget);
  vtkMRMLSegmentationNode* mask = this->datasetMask();
  if (mask != this->ObservedMask.GetPointer())
    {
    q->qvtkReconnect(this->ObservedMask, mask, vtkSegmentation::SegmentAdded, q, SLOT(onMaskModified()));
    q->qvtkReconnect(this->ObservedMask, mask, vtkSegmentation::SegmentRemoved, q, SLOT(onMaskModified()));
    this->ObservedMask = mask;
    }
  if (this->SegmentsTableView->segmentationNode() != mask)
    {
    this->SegmentsTableView->setSegmentationNode(mask);
    }
  const int segments = (mask && mask->GetSegmentation()) ? mask->GetSegmentation()->GetNumberOfSegments() : 0;
  this->MaskInfo->setText(segments > 0 ? QString() : qSlicerAstroModelingModuleWidget::tr(
    "The mask of the cube has no segments: draw it in the Mask tab (or import a SoFiA mask)."));
  this->MaskInfo->setVisible(segments < 1);
  // the segment of the parameters node selected in the table
  const char* segmentID = this->parametersNode ? this->parametersNode->GetMaskSegmentID() : nullptr;
  if (segmentID && mask && mask->GetSegmentation()->GetSegment(segmentID) &&
      !this->SegmentsTableView->selectedSegmentIDs().contains(QString::fromUtf8(segmentID)))
    {
    const QSignalBlocker blocker(this->SegmentsTableView);
    this->SegmentsTableView->setSelectedSegmentIDs(QStringList() << QString::fromUtf8(segmentID));
    }
  this->SearchGroup->setVisible(!this->parametersNode || !this->parametersNode->GetMaskActive());
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::updateFromNode()
{
  Q_Q(qSlicerAstroModelingModuleWidget);
  ParametersNode* p = this->parametersNode;
  this->Updating = true;
  {
  const QSignalBlocker blocker(this->InputVolumeNodeSelector);
  this->InputVolumeNodeSelector->setCurrentNode(this->inputVolume());
  }
  if (p)
    {
    for (std::pair<qSlicerAstroValueField*, NumberParameter>& entry : this->Numbers)
      {
      const QSignalBlocker blocker(entry.first);
      entry.first->setValue(entry.second.Get(p));
      }
    for (std::pair<QToolButton*, NumberParameter>& entry : this->FreeButtons)
      {
      const QSignalBlocker blocker(entry.first);
      entry.first->setChecked(entry.second.GetFree(p));
      }
    const bool manual = p->GetMode() && !strcmp(p->GetMode(), "Manual");
    {
    const QSignalBlocker blockMode(this->ModeButton);
    this->ModeButton->setCurrentIndex(manual ? 1 : 0);
    const QSignalBlocker blockRegion(this->RegionButton);
    this->RegionButton->setCurrentIndex(p->GetMaskActive() ? 1 : 0);
    }
    this->SearchNote->setVisible(!p->GetMaskActive());
    this->MaskPage->setVisible(p->GetMaskActive());
    // Automatic: the parameters are estimated by 3DBarolo
    for (qSlicerAstroValueField* field : this->AutoFields)
      {
      field->setEnabled(manual || field->objectName().startsWith("Search") ||
                        field->objectName() == "InstrumentalBroadeningField" ||
                        field->objectName() == "NumberOfCloundsField");
      }
    this->LayerTypeCombo->setCurrentIndex(p->GetLayerType());
    this->FittingFunctionCombo->setCurrentIndex(std::max(0, p->GetFittingFunction() - 1));
    this->WeightingFunctionCombo->setCurrentIndex(p->GetWeightingFunction());
    const int side = this->SideCombo->findData(QString::fromStdString(p->GetSide()));
    this->SideCombo->setCurrentIndex(side >= 0 ? side : 0);
    const QString normalize = QString::fromUtf8(p->GetNormalize() ? p->GetNormalize() : "NONE");
    this->NormalizeButton->setCurrentIndex(normalize == "LOCAL" ? 1 : (normalize == "AZIM" ? 2 : 0));
    this->TwoStageCheckBox->setChecked(p->GetTwoStage());
    this->RegularizationCombo->setCurrentText(QString::fromStdString(p->GetRegularizationType()));
    this->ADRIFTCheckBox->setChecked(p->GetADRIFTCorrection());
    this->NormalizeCubeCheckBox->setChecked(p->GetNormalizeCube());
    this->EstimateErrorsCheckBox->setChecked(p->GetEstimateErrors());
    if (!this->RestFrequenciesEdit->hasFocus())
      {
      this->RestFrequenciesEdit->setText(QString::fromStdString(p->GetRestFrequencies()));
      }
    if (!this->RelativeIntensitiesEdit->hasFocus())
      {
      this->RelativeIntensitiesEdit->setText(QString::fromStdString(p->GetRelativeIntensities()));
      }
    this->ContourLevelField->setValue(p->GetContourLevel());
    this->MajorOffsetField->setValue(p->GetYellowRotValue());
    this->MinorOffsetField->setValue(p->GetGreenRotValue());
    }
  this->Updating = false;
  // the table of the rings
  vtkMRMLTableNode* table = p ? p->GetParamsTableNode() : nullptr;
  if (table != this->ObservedTable.GetPointer())
    {
    q->qvtkReconnect(this->ObservedTable, table, vtkCommand::ModifiedEvent, q, SLOT(onTableModified()));
    this->ObservedTable = table;
    }
  if (this->TableView->mrmlTableNode() != table)
    {
    this->TableView->setMRMLTableNode(table);
    }
  this->updateBeam();
  this->updateMask();
  this->updateResults();
  this->updateRunning();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::updateResults()
{
  const bool results = this->hasResults();
  this->Results->setVisible(results);
  if (!results)
    {
    return;
    }
  vtkSlicerAstroModelingLogic* logic = this->logic();
  {
  const QSignalBlocker blockData(this->DataContoursButton);
  const QSignalBlocker blockModel(this->ModelContoursButton);
  const QSignalBlocker blockRegion(this->ModelRegionButton);
  this->DataContoursButton->setChecked(logic &&
    logic->GetResultContoursVisibility(vtkSlicerAstroModelingLogic::GetDataContoursSegmentID()));
  this->ModelContoursButton->setChecked(logic &&
    logic->GetResultContoursVisibility(vtkSlicerAstroModelingLogic::GetModelContoursSegmentID()));
  this->ModelRegionButton->setChecked(logic &&
    logic->GetResultContoursVisibility(vtkSlicerAstroModelingLogic::GetModelMaskSegmentID()));
  }
  vtkMRMLAstroVolumeNode* model = this->volume(this->parametersNode->GetOutputVolumeNodeID());
  vtkMRMLAstroVolumeNode* residual = this->volume(this->parametersNode->GetResidualVolumeNodeID());
  if (model)
    {
    this->ModelResult->setVolume(model);
    this->ModelResult->setSubtitle(qSlicerAstroModelingModuleWidget::tr("model cube"));
    }
  if (residual)
    {
    this->ResidualResult->setVolume(residual);
    this->ResidualResult->setSubtitle(qSlicerAstroModelingModuleWidget::tr("data in the region of the model"));
    }
  qSlicerAstroResultCard::setCardsVisible(QList<qSlicerAstroResultCard*>() << this->ModelResult << this->ResidualResult,
                                          QList<bool>() << (model != nullptr) << (residual != nullptr));
  this->updateRingPlot();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::updateRingPlot()
{
  vtkMRMLTableNode* table = this->parametersNode ? this->parametersNode->GetParamsTableNode() : nullptr;
  vtkTable* data = table ? table->GetTable() : nullptr;
  vtkDataArray* radii = data ? vtkDataArray::SafeDownCast(data->GetColumnByName("Radii")) : nullptr;
  vtkDataArray* values = data ? vtkDataArray::SafeDownCast(data->GetColumnByName(this->RingColumn.toUtf8().constData())) :
                                nullptr;
  if (!radii || !values)
    {
    this->RingPlot->clearRings();
    return;
    }
  QVector<double> x, y;
  for (vtkIdType row = 0; row < radii->GetNumberOfTuples() && row < values->GetNumberOfTuples(); row++)
    {
    x << radii->GetTuple1(row);
    y << values->GetTuple1(row);
    }
  const char* unit = table->GetColumnUnitLabel(this->RingColumn.toUtf8().constData()).c_str();
  QString unitText = QString::fromUtf8(unit).section(' ', 0, 0);
  this->RingPlot->setRings(x, y, unitText);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::setSliceObservers(bool observe)
{
  Q_Q(qSlicerAstroModelingModuleWidget);
  vtkMRMLScene* scene = q->mrmlScene();
  if (observe == this->SliceObservers || !scene)
    {
    return;
    }
  // the points of the rotation curve follow the PV slices moved in the views
  for (const char* id : {"vtkMRMLSliceNodeYellow", "vtkMRMLSliceNodeGreen"})
    {
    vtkMRMLNode* sliceNode = scene->GetNodeByID(id);
    if (observe)
      {
      q->qvtkConnect(sliceNode, vtkCommand::ModifiedEvent, q, SLOT(onSliceNodeModified()));
      }
    else
      {
      q->qvtkDisconnect(sliceNode, vtkCommand::ModifiedEvent, q, SLOT(onSliceNodeModified()));
      }
    }
  this->SliceObservers = observe;
}

//-----------------------------------------------------------------------------
// qSlicerAstroModelingModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidget::qSlicerAstroModelingModuleWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroModelingModuleWidgetPrivate(*this))
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidget::~qSlicerAstroModelingModuleWidget() = default;

//-----------------------------------------------------------------------------
vtkMRMLAstroModelingParametersNode* qSlicerAstroModelingModuleWidget::mrmlAstroModelingParametersNode() const
{
  Q_D(const qSlicerAstroModelingModuleWidget);
  return d->parametersNode;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroModelingModuleWidget::outputLog() const
{
  Q_D(const qSlicerAstroModelingModuleWidget);
  return d->LogEdit->toPlainText();
}

//-----------------------------------------------------------------------------
qSlicerAstroRingPlot* qSlicerAstroModelingModuleWidget::ringPlot() const
{
  Q_D(const qSlicerAstroModelingModuleWidget);
  return d->RingPlot;
}

//-----------------------------------------------------------------------------
QString qSlicerAstroModelingModuleWidget::ringParameter() const
{
  Q_D(const qSlicerAstroModelingModuleWidget);
  return d->RingColumn;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  this->Superclass::setMRMLScene(scene);
  if (!scene)
    {
    return;
    }
  vtkSlicerApplicationLogic* appLogic = this->module() ? this->module()->appLogic() : nullptr;
  d->selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  d->SliceObservers = false;
  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent, this, SLOT(onEndCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(onEndImportEvent()));
  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent, this, SLOT(onSelectionNodeModified()));
  this->qvtkReconnect(d->logic(), vtkSlicerAstroModelingLogic::OperationFinishedEvent,
                      this, SLOT(onOperationFinished()));
  this->onEndImportEvent();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  // a new parameters node for the new scene
  d->parametersNode = nullptr;
  d->SliceObservers = false;
  d->LogEdit->clear();
  this->onEndImportEvent();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene)
    {
    return;
    }
  // the parameters of a loaded scene (the last one with a cube) rather than
  // the empty node made when the previous scene was closed, which is removed
  std::vector<vtkMRMLNode*> nodes;
  scene->GetNodesByClass("vtkMRMLAstroModelingParametersNode", nodes);
  vtkMRMLNode* parametersNode = nullptr;
  for (vtkMRMLNode* node : nodes)
    {
    vtkMRMLAstroModelingParametersNode* candidate = vtkMRMLAstroModelingParametersNode::SafeDownCast(node);
    if (candidate && (candidate->GetInputVolumeNodeID() || !parametersNode))
      {
      parametersNode = candidate;
      }
    }
  for (vtkMRMLNode* node : nodes)
    {
    vtkMRMLAstroModelingParametersNode* other = vtkMRMLAstroModelingParametersNode::SafeDownCast(node);
    if (other && other != parametersNode && !other->GetInputVolumeNodeID())
      {
      if (other == d->parametersNode.GetPointer())
        {
        this->setMRMLAstroModelingParametersNode(nullptr);
        }
      scene->RemoveNode(other);
      }
    }
  if (!parametersNode)
    {
    vtkSmartPointer<vtkMRMLNode> node = vtkSmartPointer<vtkMRMLNode>::Take(
      scene->CreateNodeByClass("vtkMRMLAstroModelingParametersNode"));
    parametersNode = scene->AddNode(node);
    }
  this->setMRMLAstroModelingParametersNode(parametersNode);
  // a loaded scene with results: the PV slices along the axes of its rings
  // (absolute orientations) and the points of the rotation curve
  vtkSlicerAstroModelingLogic* logic = d->logic();
  if (logic && d->parametersNode && d->hasResults())
    {
    logic->SetPVSlices(d->parametersNode);
    }
  this->onSelectionNodeModified();
  d->updateFromNode();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setMRMLAstroModelingParametersNode(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  vtkMRMLAstroModelingParametersNode* parametersNode = vtkMRMLAstroModelingParametersNode::SafeDownCast(node);
  this->qvtkReconnect(d->parametersNode, parametersNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onParametersNodeModified()));
  d->parametersNode = parametersNode;
  d->updateFromNode();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onParametersNodeModified()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  if (!d->Updating)
    {
    d->updateFromNode();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onSelectionNodeModified()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  // the module follows the active cube while it is shown (not during an
  // operation: the cube in use is kept)
  vtkSlicerAstroModelingLogic* logic = d->logic();
  if (!this->isEntered() || !d->selectionNode || (logic && logic->IsOperationRunning()))
    {
    return;
    }
  const char* activeID = d->selectionNode->GetActiveVolumeID();
  vtkMRMLAstroVolumeNode* active = d->volume(activeID);
  if (!active || active->GetImageData()->GetDimensions()[2] < 2 || active == d->inputVolume())
    {
    return;
    }
  // the model and residual cubes of the module are not new inputs
  const char* dataModel = active->GetAttribute("SlicerAstro.DATAMODEL");
  const std::string name = active->GetName() ? active->GetName() : "";
  if ((dataModel && !strcmp(dataModel, "MODEL")) || name.find("_maskedByModel_") != std::string::npos)
    {
    return;
    }
  this->setInputVolume(active);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setInputVolume(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  vtkSlicerAstroModelingLogic* logic = d->logic();
  if (logic && logic->IsOperationRunning())
    {
    // the cube in use is kept until the end of the operation
    d->updateFromNode();
    return;
    }
  const char* id = node ? node->GetID() : nullptr;
  const char* current = d->parametersNode->GetInputVolumeNodeID();
  const bool changed = (id && current) ? strcmp(id, current) != 0 : id != current;
  if (changed)
    {
    d->parametersNode->SetInputVolumeNodeID(id);
    }
  // the chosen cube becomes the active one (the module follows the active
  // volume)
  if (id && d->selectionNode && (!d->selectionNode->GetActiveVolumeID() ||
                                 strcmp(d->selectionNode->GetActiveVolumeID(), id)))
    {
    d->selectionNode->SetActiveVolumeID(id);
    }
  d->updateFromNode();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onModeChanged(int mode)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  const int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetMode(mode == 1 ? "Manual" : "Automatic");
  if (mode == 0)
    {
    // Automatic: the values of 3DBarolo
    vtkSlicerAstroModelingLogic::ResetFitParameters(d->parametersNode);
    }
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onRegionChanged(int region)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  if (!d->parametersNode || d->Updating)
    {
    return;
    }
  d->parametersNode->SetMaskActive(region == 1);
  if (region == 1 && d->logic())
    {
    // the selected segment, or the first one
    d->logic()->SelectMaskSegment(d->parametersNode);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMaskSegmentSelectionChanged()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  vtkMRMLSegmentationNode* mask = d->datasetMask();
  const QStringList selected = d->SegmentsTableView->selectedSegmentIDs();
  if (!d->parametersNode || !mask || selected.isEmpty())
    {
    return;
    }
  const int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetMaskSegmentationNodeID(mask->GetID());
  d->parametersNode->SetMaskSegmentID(selected.first().toUtf8().constData());
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMaskModified()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->updateMask();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onBeamEdited()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  vtkMRMLAstroVolumeNode* cube = d->inputVolume();
  if (!cube)
    {
    return;
    }
  // the beam of the data (3DBarolo, beam display, saved FITS header); the
  // beam of the header keeps its precision unless the user changed it (the
  // fields show 2 decimals)
  double current[3] = {0., 0., 0.};
  const bool known = vtkSlicerAstroVolumeLogic::GetBeam(cube, current);
  if (!known && d->EstimatedBeamValid && d->EstimatedBeamVolumeID == QString::fromUtf8(cube->GetID()))
    {
    std::copy(d->EstimatedBeam, d->EstimatedBeam + 3, current);
    }
  const double edited[3] = {d->BeamFields[0]->value() / 3600., d->BeamFields[1]->value() / 3600.,
                            d->BeamFields[2]->value()};
  bool changed = !known;
  for (int i = 0; i < 3 && !changed; i++)
    {
    const double shown = QString::number(i < 2 ? current[i] * 3600. : current[i], 'f', 2).toDouble();
    changed = std::fabs(shown - (i < 2 ? edited[i] * 3600. : edited[i])) > 1e-9;
    }
  if (changed && edited[0] > 0. && edited[1] > 0.)
    {
    vtkSlicerAstroVolumeLogic::SetBeam(cube, edited);
    }
  d->updateBeam();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::estimate()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->start(vtkMRMLAstroModelingParametersNode::ESTIMATE);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::createModel()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->start(vtkMRMLAstroModelingParametersNode::CREATE);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::fit()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->start(vtkMRMLAstroModelingParametersNode::FIT);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::updateModelFromTable()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->start(vtkMRMLAstroModelingParametersNode::UPDATE);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::cancelOperation()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  if (d->logic())
    {
    d->logic()->CancelOperation();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onProgressTimeout()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  vtkSlicerAstroModelingLogic* logic = d->logic();
  if (!logic)
    {
    return;
    }
  d->appendLog(QString::fromStdString(logic->TakeLog()));
  if (d->RunningButton)
    {
    d->RunningButton->setProgress(logic->GetOperationProgress());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onOperationFinished()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  vtkSlicerAstroModelingLogic* logic = d->logic();
  d->ProgressTimer->stop();
  if (d->RunningButton)
    {
    d->RunningButton->setRunning(false);
    d->RunningButton = nullptr;
    }
  if (!logic)
    {
    return;
    }
  d->appendLog(QString::fromStdString(logic->TakeLog()));
  const int result = logic->GetLastOperationResult();
  const int operation = d->parametersNode ? d->parametersNode->GetOperation() : -1;
  if (result == -1)
    {
    d->appendLog(tr("== cancelled ==\n"));
    }
  else if (result == 0)
    {
    d->Message->showMessage("error", tr("3DBarolo failed: see the 3DBarolo log."));
    }
  else if (operation == vtkMRMLAstroModelingParametersNode::ESTIMATE && d->parametersNode)
    {
    // the estimated parameters, shown to be edited
    d->parametersNode->SetMode("Manual");
    }
  d->updateFromNode();
  if (result == 1 && operation != vtkMRMLAstroModelingParametersNode::ESTIMATE && this->isEntered() &&
      d->hasResults())
    {
    logic->ShowResults(d->parametersNode, !d->embedded());
    logic->UpdateRingPlot(d->parametersNode, d->RingColumn.toUtf8().constData());
    vtkSlicerApplicationLogic* appLogic = this->module() ? this->module()->appLogic() : nullptr;
    if (appLogic)
      {
      appLogic->PropagatePlotChartSelection();
      }
    d->updateResults();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onTableModified()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->updateRingPlot();
  if (d->Results->isHidden() && d->hasResults())
    {
    d->updateResults();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onSliceNodeModified()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->MarkersTimer->start();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setRingParameter(const QString& column)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  const int index = d->RingParameterCombo->findData(column);
  if (index < 0)
    {
    return;
    }
  d->RingColumn = column;
  {
  const QSignalBlocker blocker(d->RingParameterCombo);
  d->RingParameterCombo->setCurrentIndex(index);
  }
  d->updateRingPlot();
  vtkSlicerAstroModelingLogic* logic = d->logic();
  vtkMRMLScene* scene = this->mrmlScene();
  // the plot view, if it shows the rings
  vtkMRMLPlotChartNode* chart = scene ? vtkMRMLPlotChartNode::SafeDownCast(
    scene->GetFirstNode("ModelingRingsChart", "vtkMRMLPlotChartNode")) : nullptr;
  if (logic && chart && d->parametersNode)
    {
    logic->UpdateRingPlot(d->parametersNode, column.toUtf8().constData());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::openPlotView()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  vtkSlicerAstroModelingLogic* logic = d->logic();
  qSlicerApplication* app = qSlicerApplication::application();
  if (!logic || !d->parametersNode || !logic->UpdateRingPlot(d->parametersNode, d->RingColumn.toUtf8().constData()))
    {
    return;
    }
  // a layout with a plot view, unless the current layout shows one
  vtkMRMLLayoutNode* layoutNode = (app && app->layoutManager() && app->layoutManager()->layoutLogic()) ?
    app->layoutManager()->layoutLogic()->GetLayoutNode() : nullptr;
  const char* description = layoutNode ? layoutNode->GetCurrentLayoutDescription() : nullptr;
  if (layoutNode && (!description || !strstr(description, "vtkMRMLPlotViewNode")))
    {
    layoutNode->SetViewArrangement(vtkSlicerAstroVolumeLogic::AddPlotLayout(layoutNode));
    }
  vtkSlicerApplicationLogic* appLogic = this->module() ? this->module()->appLogic() : nullptr;
  if (appLogic)
    {
    appLogic->PropagatePlotChartSelection();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::showResults()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  vtkSlicerAstroModelingLogic* logic = d->logic();
  if (logic && d->parametersNode && d->hasResults())
    {
    logic->ShowResults(d->parametersNode, !d->embedded());
    d->updateResults();
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroModelingModuleWidget::exportTable(const QString& fileName)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  vtkMRMLTableNode* table = d->parametersNode ? d->parametersNode->GetParamsTableNode() : nullptr;
  vtkTable* data = table ? table->GetTable() : nullptr;
  if (!data || data->GetNumberOfRows() < 1)
    {
    return false;
    }
  QString path = fileName;
  if (path.isEmpty())
    {
    vtkMRMLAstroVolumeNode* cube = d->inputVolume();
    const QString name = QString::fromUtf8(cube && cube->GetName() ? cube->GetName() : "rings") + "_rings.csv";
    path = QFileDialog::getSaveFileName(this, tr("Save the rings"), name, tr("CSV (*.csv)"));
    if (path.isEmpty())
      {
      return false;
      }
    }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
    d->Message->showMessage("error", tr("The table could not be saved in %1.").arg(path));
    return false;
    }
  QTextStream stream(&file);
  QStringList header;
  for (vtkIdType column = 0; column < data->GetNumberOfColumns(); column++)
    {
    const char* name = data->GetColumnName(column);
    const std::string unit = table->GetColumnUnitLabel(name);
    header << (unit.empty() ? QString::fromUtf8(name) : QString("%1 [%2]").arg(QString::fromUtf8(name),
                                                                               QString::fromStdString(unit)));
    }
  stream << header.join(",") << "\n";
  for (vtkIdType row = 0; row < data->GetNumberOfRows(); row++)
    {
    QStringList values;
    for (vtkIdType column = 0; column < data->GetNumberOfColumns(); column++)
      {
      values << QString::number(data->GetValue(row, column).ToDouble(), 'g', 10);
      }
    stream << values.join(",") << "\n";
    }
  return true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::enter()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  this->Superclass::enter();
  this->onSelectionNodeModified();
  d->setSliceObservers(true);
  vtkSlicerAstroModelingLogic* logic = d->logic();
  // the results of the cube in the views (the layout of classic Slicer; the
  // Workbench keeps its own)
  if (logic && d->parametersNode && d->hasResults() && !logic->IsOperationRunning())
    {
    logic->ShowResults(d->parametersNode, !d->embedded());
    }
  d->updateFromNode();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::exit()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->setSliceObservers(false);
  this->Superclass::exit();
}
