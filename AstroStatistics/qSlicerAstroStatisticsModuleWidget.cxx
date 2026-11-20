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
#include <QPointer>
#include <QAbstractItemView>
#include <QAction>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

// CTK includes
#include <ctkFlowLayout.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkTable.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <vtkSlicerApplicationLogic.h>

// MRML includes
#include <qMRMLNodeComboBox.h>
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSortFilterProxyModel.h>
#include <qMRMLTableView.h>
#include <vtkMRMLAstroStatisticsParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLTableNode.h>

// AstroVolume includes
#include <qSlicerAstroDatasetSelector.h>
#include <qSlicerAstroFoldSection.h>
#include <qSlicerAstroMessageWidget.h>
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroRunButton.h>
#include <qSlicerAstroSegmentedButton.h>
#include <qSlicerAstroTheme.h>
#include <vtkSlicerAstroVolumeLogic.h>

// Logic includes
#include <vtkSlicerAstroStatisticsLogic.h>

#include <cmath>
#include <vector>

#include "qSlicerAstroStatisticsModuleWidget.h"

namespace
{
/// The quantities: object name of the chip, column of the table, label
struct Quantity
{
  const char* Name;
  const char* Column;
  const char* Label;
  const char* ToolTip;
};

const Quantity Quantities[] = {
  {"TotalFlux", "TotalFlux", "Total flux", "Total flux (Jy): the sum of the intensities (Jy/beam) times the pixel "
                                           "area divided by the beam area (needs Jy/beam data with a beam)."},
  {"Mean", "Mean", "Mean", "Mean of the voxels."},
  {"Std", "Std", "Std", "Standard deviation of the voxels."},
  {"Median", "Median", "Median", "Median of the voxels (slower: the voxels are sorted)."},
  {"Min", "Min", "Min", "Minimum of the voxels."},
  {"Max", "Max", "Max", "Maximum of the voxels."},
  {"Sum", "Sum", "Sum", "Sum of the voxels."},
  {"Npixels", "Npixels", "N voxels", "Number of voxels (blank voxels are not counted)."}};
const int NumberOfQuantities = 8;

//----------------------------------------------------------------------------
bool QuantityOn(vtkMRMLAstroStatisticsParametersNode* node, int index)
{
  switch (index)
    {
    case 0: return node->GetTotalFlux();
    case 1: return node->GetMean();
    case 2: return node->GetStd();
    case 3: return node->GetMedian();
    case 4: return node->GetMin();
    case 5: return node->GetMax();
    case 6: return node->GetSum();
    default: return node->GetNpixels();
    }
}

//----------------------------------------------------------------------------
void SetQuantityOn(vtkMRMLAstroStatisticsParametersNode* node, int index, bool on)
{
  switch (index)
    {
    case 0: node->SetTotalFlux(on); break;
    case 1: node->SetMean(on); break;
    case 2: node->SetStd(on); break;
    case 3: node->SetMedian(on); break;
    case 4: node->SetMin(on); break;
    case 5: node->SetMax(on); break;
    case 6: node->SetSum(on); break;
    default: node->SetNpixels(on); break;
    }
}

//----------------------------------------------------------------------------
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}

//----------------------------------------------------------------------------
const char* ModeName(int region)
{
  return region == qSlicerAstroStatisticsModuleWidget::Box ? "ROI" :
         region == qSlicerAstroStatisticsModuleWidget::Mask ? "Segmentation" : "Volume";
}
} // end of anonymous namespace

//-----------------------------------------------------------------------------
class qSlicerAstroStatisticsModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroStatisticsModuleWidget);
protected:
  qSlicerAstroStatisticsModuleWidget* const q_ptr;
public:
  qSlicerAstroStatisticsModuleWidgetPrivate(qSlicerAstroStatisticsModuleWidget& object);
  void init();
  vtkSlicerAstroStatisticsLogic* logic() const;
  vtkMRMLAstroVolumeNode* inputVolume() const;
  static void fitStack(QStackedWidget* stack);

  qSlicerAstroPanelHeader* Header{nullptr};
  qSlicerAstroDatasetSelector* InputVolumeNodeSelector{nullptr};
  qSlicerAstroMessageWidget* Message{nullptr};
  qSlicerAstroSegmentedButton* RegionButton{nullptr};
  QStackedWidget* RegionStack{nullptr};
  qMRMLNodeComboBox* ROINodeComboBox{nullptr};
  QPushButton* ROIFitButton{nullptr};
  QPushButton* ROIVisibleButton{nullptr};
  qMRMLNodeComboBox* SegmentationNodeComboBox{nullptr};
  qMRMLSegmentsTableView* SegmentsTableView{nullptr};
  QToolButton* QuantityButtons[NumberOfQuantities] = {};
  QFrame* Results{nullptr};
  QGridLayout* ResultsLayout{nullptr};
  QLabel* ResultsTitle{nullptr};
  qSlicerAstroFoldSection* HistorySection{nullptr};
  qMRMLTableView* TableView{nullptr};
  qSlicerAstroFoldSection* AdvancedSection{nullptr};
  qMRMLNodeComboBox* TableNodeComboBox{nullptr};
  qMRMLNodeComboBox* ParametersNodeComboBox{nullptr};
  qSlicerAstroRunButton* RunButton{nullptr};

  vtkSmartPointer<vtkMRMLAstroStatisticsParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
};

//-----------------------------------------------------------------------------
qSlicerAstroStatisticsModuleWidgetPrivate::qSlicerAstroStatisticsModuleWidgetPrivate(
  qSlicerAstroStatisticsModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
vtkSlicerAstroStatisticsLogic* qSlicerAstroStatisticsModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroStatisticsModuleWidget);
  return vtkSlicerAstroStatisticsLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroStatisticsModuleWidgetPrivate::inputVolume() const
{
  Q_Q(const qSlicerAstroStatisticsModuleWidget);
  return (this->parametersNode && q->mrmlScene() && this->parametersNode->GetInputVolumeNodeID()) ?
    vtkMRMLAstroVolumeNode::SafeDownCast(q->mrmlScene()->GetNodeByID(this->parametersNode->GetInputVolumeNodeID())) :
    nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidgetPrivate::fitStack(QStackedWidget* stack)
{
  for (int page = 0; page < stack->count(); page++)
    {
    stack->widget(page)->setSizePolicy(QSizePolicy::Preferred,
      page == stack->currentIndex() ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    }
  stack->updateGeometry();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroStatisticsModuleWidget);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(qSlicerAstroStatisticsModuleWidget::tr("Statistics"));
  this->Header->setIconName("module-statistics");
  rootLayout->addWidget(this->Header);

  QWidget* body = new QWidget(q);
  QVBoxLayout* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(12, 12, 12, 14);
  bodyLayout->setSpacing(14);
  rootLayout->addWidget(body);

  this->InputVolumeNodeSelector = new qSlicerAstroDatasetSelector(body);
  this->InputVolumeNodeSelector->setObjectName("InputVolumeNodeSelector");
  this->InputVolumeNodeSelector->setNoneText(qSlicerAstroStatisticsModuleWidget::tr("Select data"));
  this->InputVolumeNodeSelector->setToolTip(qSlicerAstroStatisticsModuleWidget::tr(
    "Data of the statistics (also the active volume of the views)."));
  bodyLayout->addWidget(this->InputVolumeNodeSelector);

  this->Message = new qSlicerAstroMessageWidget(body);
  this->Message->setObjectName("MessageWidget");
  bodyLayout->addWidget(this->Message);

  // Where
  QWidget* region = new QWidget(body);
  QVBoxLayout* regionLayout = new QVBoxLayout(region);
  regionLayout->setContentsMargins(0, 0, 0, 0);
  regionLayout->setSpacing(8);
  regionLayout->addWidget(Overline(qSlicerAstroStatisticsModuleWidget::tr("Where"), region));
  this->RegionButton = new qSlicerAstroSegmentedButton(region);
  this->RegionButton->setObjectName("RegionButton");
  this->RegionButton->setToolTip(qSlicerAstroStatisticsModuleWidget::tr("Voxels of the statistics."));
  this->RegionButton->addButton(qSlicerAstroStatisticsModuleWidget::tr("Whole data"),
    qSlicerAstroStatisticsModuleWidget::tr("All the voxels of the data."));
  this->RegionButton->addButton(qSlicerAstroStatisticsModuleWidget::tr("Box (ROI)"),
    qSlicerAstroStatisticsModuleWidget::tr("The voxels in a box: drag its handles in the views."));
  this->RegionButton->addButton(qSlicerAstroStatisticsModuleWidget::tr("Mask"),
    qSlicerAstroStatisticsModuleWidget::tr("The voxels of a segment of a mask (e.g. a SoFiA source)."));
  regionLayout->addWidget(this->RegionButton);
  this->RegionStack = new QStackedWidget(region);
  this->RegionStack->addWidget(new QWidget(this->RegionStack));
  QWidget* roiPage = new QWidget(this->RegionStack);
  QVBoxLayout* roiLayout = new QVBoxLayout(roiPage);
  roiLayout->setContentsMargins(0, 0, 0, 0);
  roiLayout->setSpacing(6);
  this->ROINodeComboBox = new qMRMLNodeComboBox(roiPage);
  this->ROINodeComboBox->setObjectName("ROINodeComboBox");
  this->ROINodeComboBox->setNodeTypes(QStringList() << "vtkMRMLMarkupsROINode");
  this->ROINodeComboBox->setNoneEnabled(false);
  this->ROINodeComboBox->setAddEnabled(true);
  this->ROINodeComboBox->setRemoveEnabled(false);
  this->ROINodeComboBox->setEditEnabled(false);
  this->ROINodeComboBox->setRenameEnabled(true);
  this->ROINodeComboBox->setToolTip(qSlicerAstroStatisticsModuleWidget::tr("Box of the statistics."));
  roiLayout->addWidget(this->ROINodeComboBox);
  QHBoxLayout* roiButtons = new QHBoxLayout();
  roiButtons->setSpacing(6);
  this->ROIFitButton = new QPushButton(qSlicerAstroStatisticsModuleWidget::tr("Fit to the data"), roiPage);
  this->ROIFitButton->setObjectName("ROIFitButton");
  this->ROIFitButton->setIcon(theme->icon("roi", "text"));
  this->ROIFitButton->setToolTip(qSlicerAstroStatisticsModuleWidget::tr("The box around all the data, on its voxels."));
  this->ROIVisibleButton = new QPushButton(qSlicerAstroStatisticsModuleWidget::tr("Show box"), roiPage);
  this->ROIVisibleButton->setObjectName("ROIVisibleButton");
  this->ROIVisibleButton->setCheckable(true);
  this->ROIVisibleButton->setChecked(true);
  this->ROIVisibleButton->setIcon(theme->icon("eye", "text"));
  this->ROIVisibleButton->setToolTip(qSlicerAstroStatisticsModuleWidget::tr("Show or hide the box in the views."));
  roiButtons->addWidget(this->ROIFitButton, 1);
  roiButtons->addWidget(this->ROIVisibleButton, 1);
  roiLayout->addLayout(roiButtons);
  this->RegionStack->addWidget(roiPage);
  QWidget* maskPage = new QWidget(this->RegionStack);
  QVBoxLayout* maskLayout = new QVBoxLayout(maskPage);
  maskLayout->setContentsMargins(0, 0, 0, 0);
  maskLayout->setSpacing(6);
  this->SegmentationNodeComboBox = new qMRMLNodeComboBox(maskPage);
  this->SegmentationNodeComboBox->setObjectName("SegmentationNodeComboBox");
  this->SegmentationNodeComboBox->setNodeTypes(QStringList() << "vtkMRMLSegmentationNode");
  this->SegmentationNodeComboBox->setNoneEnabled(true);
  this->SegmentationNodeComboBox->setAddEnabled(false);
  this->SegmentationNodeComboBox->setRemoveEnabled(false);
  this->SegmentationNodeComboBox->setEditEnabled(false);
  this->SegmentationNodeComboBox->setRenameEnabled(true);
  this->SegmentationNodeComboBox->setToolTip(qSlicerAstroStatisticsModuleWidget::tr(
    "Mask with the segments. By default the mask of the data."));
  maskLayout->addWidget(this->SegmentationNodeComboBox);
  this->SegmentsTableView = new qMRMLSegmentsTableView(maskPage);
  this->SegmentsTableView->setObjectName("SegmentsTableView");
  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  this->SegmentsTableView->setMinimumHeight(120);
  this->SegmentsTableView->setMaximumHeight(170);
  this->SegmentsTableView->setToolTip(qSlicerAstroStatisticsModuleWidget::tr("The selected segment is the region."));
  maskLayout->addWidget(this->SegmentsTableView);
  this->RegionStack->addWidget(maskPage);
  regionLayout->addWidget(this->RegionStack);
  bodyLayout->addWidget(region);

  // Quantities
  QWidget* quantities = new QWidget(body);
  QVBoxLayout* quantitiesLayout = new QVBoxLayout(quantities);
  quantitiesLayout->setContentsMargins(0, 0, 0, 0);
  quantitiesLayout->setSpacing(8);
  quantitiesLayout->addWidget(Overline(qSlicerAstroStatisticsModuleWidget::tr("Quantities"), quantities));
  QWidget* chips = new QWidget(quantities);
  ctkFlowLayout* chipsLayout = new ctkFlowLayout(chips);
  chipsLayout->setContentsMargins(0, 0, 0, 0);
  chipsLayout->setHorizontalSpacing(6);
  chipsLayout->setVerticalSpacing(6);
  chipsLayout->setPreferredExpandingDirections(Qt::Vertical);
  chipsLayout->setAlignItems(false);
  for (int i = 0; i < NumberOfQuantities; i++)
    {
    QToolButton* chip = new QToolButton(chips);
    chip->setObjectName(QString("Quantity_%1").arg(Quantities[i].Name));
    chip->setProperty("astroRole", "actionChip");
    chip->setProperty("quantity", i);
    chip->setText(qSlicerAstroStatisticsModuleWidget::tr(Quantities[i].Label));
    chip->setToolTip(qSlicerAstroStatisticsModuleWidget::tr(Quantities[i].ToolTip));
    chip->setCheckable(true);
    chip->setCursor(Qt::PointingHandCursor);
    QObject::connect(chip, SIGNAL(toggled(bool)), q, SLOT(onQuantityToggled(bool)));
    chipsLayout->addWidget(chip);
    this->QuantityButtons[i] = chip;
    }
  quantitiesLayout->addWidget(chips);
  bodyLayout->addWidget(quantities);

  // Result of the last calculation
  QWidget* result = new QWidget(body);
  QVBoxLayout* resultLayout = new QVBoxLayout(result);
  resultLayout->setContentsMargins(0, 0, 0, 0);
  resultLayout->setSpacing(8);
  this->ResultsTitle = Overline(qSlicerAstroStatisticsModuleWidget::tr("Result"), result);
  resultLayout->addWidget(this->ResultsTitle);
  this->Results = new QFrame(result);
  this->Results->setObjectName("Results");
  this->Results->setProperty("astroRole", "card");
  this->Results->setToolTip(qSlicerAstroStatisticsModuleWidget::tr(
    "Results of the last calculation (all the calculations are rows of the table below)."));
  this->ResultsLayout = new QGridLayout(this->Results);
  this->ResultsLayout->setContentsMargins(10, 6, 10, 6);
  this->ResultsLayout->setHorizontalSpacing(10);
  this->ResultsLayout->setVerticalSpacing(6);
  resultLayout->addWidget(this->Results);
  bodyLayout->addWidget(result);

  // History: the table of all the calculations
  this->HistorySection = new qSlicerAstroFoldSection(body);
  this->HistorySection->setObjectName("HistorySection");
  this->HistorySection->setTitle(qSlicerAstroStatisticsModuleWidget::tr("History"));
  this->HistorySection->setSummary(qSlicerAstroStatisticsModuleWidget::tr("table of the calculations"));
  this->HistorySection->setSettingsKey("SlicerAstro/AstroStatistics/HistoryCollapsed");
  this->TableView = new qMRMLTableView(this->HistorySection->contentWidget());
  this->TableView->setObjectName("TableView");
  this->TableView->setMinimumHeight(160);
  this->TableView->setToolTip(qSlicerAstroStatisticsModuleWidget::tr(
    "A row per calculation (saved with the scene). Select columns to plot them."));
  this->HistorySection->contentLayout()->addWidget(this->TableView);
  QHBoxLayout* tableButtons = new QHBoxLayout();
  tableButtons->setSpacing(4);
  const char* actions[3][3] = {{"copy", "Copy", "Copy the selected cells (Ctrl+C)."},
                               {"paste", "Paste", "Paste in the selected cells (Ctrl+V)."},
                               {"plot", "Plot", "Plot the selected columns: the first is the X axis."}};
  for (int i = 0; i < 3; i++)
    {
    QPushButton* button = new QPushButton(qSlicerAstroStatisticsModuleWidget::tr(actions[i][1]),
                                          this->HistorySection->contentWidget());
    button->setObjectName(QString("Table%1Button").arg(actions[i][1]));
    button->setIcon(theme->icon(actions[i][0], "text"));
    button->setToolTip(qSlicerAstroStatisticsModuleWidget::tr(actions[i][2]));
    tableButtons->addWidget(button);
    const char* tableSlots[3] = {SLOT(copySelection()), SLOT(pasteSelection()), SLOT(plotSelection())};
    QObject::connect(button, SIGNAL(clicked()), this->TableView, tableSlots[i]);
    }
  tableButtons->addStretch(1);
  this->HistorySection->contentLayout()->addLayout(tableButtons);
  bodyLayout->addWidget(this->HistorySection);

  // Advanced
  this->AdvancedSection = new qSlicerAstroFoldSection(body);
  this->AdvancedSection->setObjectName("AdvancedSection");
  this->AdvancedSection->setTitle(qSlicerAstroStatisticsModuleWidget::tr("Advanced"));
  this->AdvancedSection->setSummary(qSlicerAstroStatisticsModuleWidget::tr("table, parameter set"));
  this->AdvancedSection->setSettingsKey("SlicerAstro/AstroStatistics/AdvancedCollapsed");
  QWidget* advancedParent = this->AdvancedSection->contentWidget();
  QFormLayout* advancedLayout = new QFormLayout();
  advancedLayout->setContentsMargins(0, 0, 0, 0);
  advancedLayout->setSpacing(8);
  this->TableNodeComboBox = new qMRMLNodeComboBox(advancedParent);
  this->TableNodeComboBox->setObjectName("TableNodeComboBox");
  this->TableNodeComboBox->setNodeTypes(QStringList() << "vtkMRMLTableNode");
  this->TableNodeComboBox->setNoneEnabled(false);
  this->TableNodeComboBox->setAddEnabled(false);
  this->TableNodeComboBox->setEditEnabled(false);
  this->TableNodeComboBox->setRenameEnabled(true);
  this->TableNodeComboBox->setToolTip(qSlicerAstroStatisticsModuleWidget::tr("Table of the calculations."));
  this->ParametersNodeComboBox = new qMRMLNodeComboBox(advancedParent);
  this->ParametersNodeComboBox->setObjectName("ParametersNodeComboBox");
  this->ParametersNodeComboBox->setNodeTypes(QStringList() << "vtkMRMLAstroStatisticsParametersNode");
  this->ParametersNodeComboBox->setShowHidden(true);
  this->ParametersNodeComboBox->setAddEnabled(false);
  this->ParametersNodeComboBox->setEditEnabled(false);
  this->ParametersNodeComboBox->setRenameEnabled(true);
  this->ParametersNodeComboBox->setToolTip(qSlicerAstroStatisticsModuleWidget::tr(
    "Parameters of the module (saved with the scene)."));
  for (qMRMLNodeComboBox* selector : {this->TableNodeComboBox, this->ParametersNodeComboBox})
    {
    selector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    selector->setMinimumWidth(80);
    }
  QLabel* tableLabel = new QLabel(qSlicerAstroStatisticsModuleWidget::tr("Table"), advancedParent);
  tableLabel->setProperty("astroRole", "label");
  QLabel* parametersLabel = new QLabel(qSlicerAstroStatisticsModuleWidget::tr("Parameter set"), advancedParent);
  parametersLabel->setProperty("astroRole", "label");
  advancedLayout->addRow(tableLabel, this->TableNodeComboBox);
  advancedLayout->addRow(parametersLabel, this->ParametersNodeComboBox);
  this->AdvancedSection->contentLayout()->addLayout(advancedLayout);
  bodyLayout->addWidget(this->AdvancedSection);

  // Footer
  QFrame* footer = new QFrame(q);
  footer->setProperty("astroRole", "panelFooter");
  QVBoxLayout* footerLayout = new QVBoxLayout(footer);
  footerLayout->setContentsMargins(12, 12, 12, 12);
  this->RunButton = new qSlicerAstroRunButton(footer);
  this->RunButton->setObjectName("RunButton");
  this->RunButton->pushButton()->setObjectName("ApplyButton");
  this->RunButton->setText(qSlicerAstroStatisticsModuleWidget::tr("Compute statistics"));
  this->RunButton->pushButton()->setToolTip(qSlicerAstroStatisticsModuleWidget::tr(
    "Compute the quantities in the region: a new row of the table."));
  footerLayout->addWidget(this->RunButton);
  rootLayout->addWidget(footer);
  rootLayout->addStretch(1);

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroStatisticsParametersNode(vtkMRMLNode*)));
  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));
  QObject::connect(this->ROINodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputROIChanged(vtkMRMLNode*)));
  QObject::connect(this->TableNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onTableNodeChanged(vtkMRMLNode*)));
  QObject::connect(this->SegmentationNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onMaskSegmentationChanged(vtkMRMLNode*)));
  for (QObject* selector : std::vector<QObject*>{this->InputVolumeNodeSelector, this->ParametersNodeComboBox,
         this->ROINodeComboBox, this->SegmentationNodeComboBox, this->SegmentsTableView, this->TableNodeComboBox,
         this->TableView})
    {
    QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), selector, SLOT(setMRMLScene(vtkMRMLScene*)));
    }
  QObject::connect(this->RegionButton, SIGNAL(currentIndexChanged(int)), q, SLOT(onRegionChanged(int)));
  QObject::connect(this->ROIFitButton, SIGNAL(clicked()), q, SLOT(onROIFit()));
  QObject::connect(this->ROIVisibleButton, SIGNAL(toggled(bool)), q, SLOT(onROIVisibilityChanged(bool)));
  QObject::connect(this->RunButton, SIGNAL(clicked()), q, SLOT(onCalculate()));
  QObject::connect(this->RunButton, SIGNAL(cancelRequested()), q, SLOT(onComputationCancelled()));
  this->fitStack(this->RegionStack);
  theme->applyTo(q);
}

//-----------------------------------------------------------------------------
qSlicerAstroStatisticsModuleWidget::qSlicerAstroStatisticsModuleWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroStatisticsModuleWidgetPrivate(*this))
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroStatisticsModuleWidget::~qSlicerAstroStatisticsModuleWidget() = default;

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::setup()
{
  this->Superclass::setup();
}

//----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::enter()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  this->Superclass::enter();
  this->onMRMLAstroStatisticsParametersNodeModified();
  this->updateMaskSelector();
  this->onMaskSegmentationChanged(d->SegmentationNodeComboBox->currentNode());
}

//----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::exit()
{
  this->Superclass::exit();
  this->onROIVisibilityChanged(false);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  if (!scene)
    {
    return;
    }
  this->Superclass::setMRMLScene(scene);
  vtkSlicerApplicationLogic* appLogic = this->module() ? this->module()->appLogic() : nullptr;
  d->selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  if (!d->selectionNode)
    {
    return;
    }
  this->initializeParameterNode();
  this->initializeSegmentations();
  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent, this, SLOT(onEndCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::StartImportEvent, this, SLOT(onStartImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(onEndImportEvent()));
  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent, this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->onMRMLSelectionNodeModified(d->selectionNode);
  // masks added, removed or modified (e.g. the contours)
  this->qvtkReconnect(scene, vtkMRMLScene::NodeAddedEvent, this, SLOT(updateMaskSelector()));
  this->qvtkReconnect(scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(updateMaskSelector()));
  this->updateMaskSelector();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }
  vtkMRMLAstroStatisticsParametersNode* parametersNode = nullptr;
  const int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroStatisticsParametersNode");
  if (numNodes > 0 && !forceNew)
    {
    parametersNode = vtkMRMLAstroStatisticsParametersNode::SafeDownCast(
      this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroStatisticsParametersNode"));
    }
  else
    {
    parametersNode = vtkMRMLAstroStatisticsParametersNode::SafeDownCast(
      this->mrmlScene()->AddNewNodeByClass("vtkMRMLAstroStatisticsParametersNode"));
    if (parametersNode)
      {
      parametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
      parametersNode->SetMode("Volume");
      }
    }
  // the box and the table of the module (the box selector selects a box)
  if (parametersNode && d->logic())
    {
    d->logic()->EnsureStatisticsROI(parametersNode);
    d->logic()->EnsureStatisticsTable(parametersNode);
    }
  d->ParametersNodeComboBox->setCurrentNode(parametersNode);
  this->setMRMLAstroStatisticsParametersNode(parametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::initializeSegmentations()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene)
    {
    return;
    }
  vtkMRMLSegmentEditorNode* editorNode = vtkMRMLSegmentEditorNode::SafeDownCast(
    scene->GetSingletonNode("SegmentEditor", "vtkMRMLSegmentEditorNode"));
  if (!editorNode)
    {
    vtkNew<vtkMRMLSegmentEditorNode> newEditorNode;
    newEditorNode->SetSingletonTag("SegmentEditor");
    editorNode = vtkMRMLSegmentEditorNode::SafeDownCast(scene->AddNode(newEditorNode));
    }
  this->qvtkReconnect(d->segmentEditorNode, editorNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onSegmentEditorNodeModified(vtkObject*)));
  d->segmentEditorNode = editorNode;
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  // the nodes of the module are replaced by the ones of the scene
  this->qvtkDisconnect(d->parametersNode, vtkCommand::ModifiedEvent,
                       this, SLOT(onMRMLAstroStatisticsParametersNodeModified()));
  if (d->parametersNode && this->mrmlScene())
    {
    this->mrmlScene()->RemoveNode(d->parametersNode);
    }
  d->parametersNode = nullptr;
  d->SegmentsTableView->setSegmentationNode(nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onEndImportEvent()
{
  this->initializeParameterNode();
  this->initializeSegmentations();
  this->onMRMLAstroStatisticsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkSlicerApplicationLogic* appLogic = this->module() ? this->module()->appLogic() : nullptr;
  d->selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  this->initializeParameterNode(true);
  this->initializeSegmentations();
  this->onMRMLAstroStatisticsParametersNodeModified();
  d->Message->clearMessage();
}

//--------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::setMRMLAstroStatisticsParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkMRMLAstroStatisticsParametersNode* parametersNode = vtkMRMLAstroStatisticsParametersNode::SafeDownCast(mrmlNode);
  if (!parametersNode || parametersNode == d->parametersNode.GetPointer())
    {
    return;
    }
  this->qvtkReconnect(d->parametersNode, parametersNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroStatisticsParametersNodeModified()));
  d->parametersNode = parametersNode;
  this->onMRMLAstroStatisticsParametersNodeModified();
}

//-----------------------------------------------------------------------------
vtkMRMLAstroStatisticsParametersNode* qSlicerAstroStatisticsModuleWidget::mrmlAstroStatisticsParametersNode() const
{
  Q_D(const qSlicerAstroStatisticsModuleWidget);
  return d->parametersNode;
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onMRMLAstroStatisticsParametersNodeModified()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }
  vtkMRMLAstroVolumeNode* inputVolume = d->inputVolume();
  {
  const QSignalBlocker inputBlocker(d->InputVolumeNodeSelector);
  const QSignalBlocker roiBlocker(d->ROINodeComboBox);
  const QSignalBlocker tableBlocker(d->TableNodeComboBox);
  const QSignalBlocker regionBlocker(d->RegionButton);
  d->InputVolumeNodeSelector->setCurrentNode(inputVolume);
  d->ROINodeComboBox->setCurrentNode(d->parametersNode->GetROINode());
  d->TableNodeComboBox->setCurrentNode(d->parametersNode->GetTableNode());
  const char* mode = d->parametersNode->GetMode() ? d->parametersNode->GetMode() : "ROI";
  const int region = !strcmp(mode, "Segmentation") ? Mask : !strcmp(mode, "Volume") ? WholeData : Box;
  d->RegionButton->setCurrentIndex(region);
  d->RegionStack->setCurrentIndex(region);
  d->fitStack(d->RegionStack);
  }
  d->TableView->setMRMLTableNode(d->parametersNode->GetTableNode());
  for (int i = 0; i < NumberOfQuantities; i++)
    {
    const QSignalBlocker blocker(d->QuantityButtons[i]);
    d->QuantityButtons[i]->setChecked(QuantityOn(d->parametersNode, i));
    }
  // total flux (Jy) only for data in Jy/beam with a beam
  const bool fluxDefined = vtkSlicerAstroVolumeLogic::GetJyBeamToJyFactor(inputVolume) > 0.;
  d->QuantityButtons[0]->setEnabled(fluxDefined);
  d->QuantityButtons[0]->setToolTip(fluxDefined ? tr(Quantities[0].ToolTip) :
    tr("Not available: the data have no beam (BMAJ, BMIN) or are not in Jy/beam, so the total flux in Jy can "
       "not be calculated."));
  const bool box = d->RegionButton->currentIndex() == Box;
  this->onROIVisibilityChanged(box && this->isEntered() && d->ROIVisibleButton->isChecked());
  d->RunButton->setEnabled(inputVolume != nullptr);
  d->RunButton->setText(inputVolume ? tr("Compute statistics") : tr("Select data"));
  const int status = d->parametersNode->GetStatus();
  d->RunButton->setRunning(status > 0 && status < 100);
  if (status > 0 && status < 100)
    {
    d->RunButton->setProgress(status);
    }
  this->updateResults();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::updateResults()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  while (QLayoutItem* item = d->ResultsLayout->takeAt(0))
    {
    delete item->widget();
    delete item;
    }
  vtkMRMLTableNode* tableNode = d->parametersNode ? d->parametersNode->GetTableNode() : nullptr;
  vtkTable* table = tableNode ? tableNode->GetTable() : nullptr;
  const int row = table ? static_cast<int>(table->GetNumberOfRows()) - 1 : -1;
  d->Results->setVisible(row >= 0);
  d->ResultsTitle->setVisible(row >= 0);
  if (row < 0)
    {
    return;
    }
  d->ResultsTitle->setText(QString::fromUtf8("RESULT · %1").arg(
    QString::fromStdString(tableNode->GetCellText(row, 0)).toUpper()));
  int line = 0;
  for (int i = 0; i < NumberOfQuantities; i++)
    {
    vtkDataArray* column = vtkDataArray::SafeDownCast(table->GetColumnByName(Quantities[i].Column));
    const double value = column && row < column->GetNumberOfTuples() ? column->GetTuple1(row) : NAN;
    if (!std::isfinite(value))
      {
      continue;
      }
    QLabel* name = new QLabel(tr(Quantities[i].Label), d->Results);
    name->setProperty("astroRole", "label");
    QLabel* number = new QLabel(i == NumberOfQuantities - 1 ? QString::number(value, 'f', 0) :
                                QString::number(value, 'g', 6).replace('-', QChar(0x2212)), d->Results);
    number->setProperty("astroRole", "mono");
    number->setObjectName(QString("Result_%1").arg(Quantities[i].Name));
    number->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QLabel* unit = new QLabel(qSlicerAstroTheme::displayUnit(QString::fromStdString(
      tableNode->GetColumnUnitLabel(Quantities[i].Column))), d->Results);
    unit->setProperty("astroRole", "unit");
    d->ResultsLayout->addWidget(name, line, 0);
    d->ResultsLayout->addWidget(number, line, 1, Qt::AlignRight);
    d->ResultsLayout->addWidget(unit, line, 2);
    line++;
    }
  d->ResultsLayout->setColumnStretch(0, 1);
}

//-----------------------------------------------------------------------------
QString qSlicerAstroStatisticsModuleWidget::resultsText() const
{
  Q_D(const qSlicerAstroStatisticsModuleWidget);
  QStringList lines;
  for (int row = 0; row < d->ResultsLayout->rowCount(); row++)
    {
    QStringList parts;
    for (int column = 0; column < 3; column++)
      {
      QLayoutItem* item = d->ResultsLayout->itemAtPosition(row, column);
      QLabel* label = item ? qobject_cast<QLabel*>(item->widget()) : nullptr;
      parts << (label ? label->text() : QString());
      }
    if (!parts[0].isEmpty())
      {
      lines << QString("%1 = %2 %3").arg(parts[0], parts[1], parts[2]).trimmed();
      }
    }
  return lines.join("\n");
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkMRMLSelectionNode* selectionNode = vtkMRMLSelectionNode::SafeDownCast(sender);
  if (!selectionNode || !d->parametersNode)
    {
    return;
    }
  const char* activeID = selectionNode->GetActiveVolumeID();
  const char* inputID = d->parametersNode->GetInputVolumeNodeID();
  if ((!activeID && !inputID) || (activeID && inputID && !strcmp(activeID, inputID)))
    {
    return;
    }
  d->parametersNode->SetInputVolumeNodeID(activeID);
  this->onROIFit();
  this->updateMaskSelector();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onInputVolumeChanged(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  if (d->selectionNode)
    {
    d->selectionNode->SetActiveVolumeID(mrmlNode ? mrmlNode->GetID() : nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onInputROIChanged(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkMRMLMarkupsROINode* roiNode = vtkMRMLMarkupsROINode::SafeDownCast(node);
  if (d->parametersNode && d->parametersNode->GetROINode() != roiNode)
    {
    d->parametersNode->SetROINode(roiNode);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onTableNodeChanged(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkMRMLTableNode* tableNode = vtkMRMLTableNode::SafeDownCast(mrmlNode);
  if (d->parametersNode && d->parametersNode->GetTableNode() != tableNode)
    {
    d->parametersNode->SetTableNode(tableNode);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::setRegion(int region)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  d->RegionButton->setCurrentIndex(region);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onRegionChanged(int index)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  d->RegionStack->setCurrentIndex(index);
  d->fitStack(d->RegionStack);
  if (!d->parametersNode)
    {
    return;
    }
  if (index == Box && d->logic())
    {
    d->logic()->EnsureStatisticsROI(d->parametersNode);
    }
  d->parametersNode->SetMode(ModeName(index));
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onQuantityToggled(bool on)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  QObject* chip = this->sender();
  if (chip && d->parametersNode)
    {
    SetQuantityOn(d->parametersNode, chip->property("quantity").toInt(), on);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onROIFit()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  if (d->logic() && d->parametersNode && d->parametersNode->GetROINode())
    {
    d->logic()->SnapROIToVoxelGrid(d->parametersNode);
    d->logic()->FitROIToInputVolume(d->parametersNode);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onROIVisibilityChanged(bool visible)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkMRMLMarkupsROINode* roiNode = d->parametersNode ? d->parametersNode->GetROINode() : nullptr;
  if (roiNode)
    {
    roiNode->SetDisplayVisibility(visible);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onCalculate()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkSlicerAstroStatisticsLogic* logic = d->logic();
  if (!d->parametersNode || !logic || !this->mrmlScene())
    {
    return;
    }
  d->Message->clearMessage();
  // the region: the selected segment of the mask
  if (d->RegionButton->currentIndex() == Mask)
    {
    vtkMRMLSegmentationNode* mask = vtkMRMLSegmentationNode::SafeDownCast(d->SegmentationNodeComboBox->currentNode());
    const QStringList selected = d->SegmentsTableView->selectedSegmentIDs();
    if (!mask || selected.isEmpty())
      {
      d->Message->showMessage("warning", tr("Select a segment of the mask."));
      return;
      }
    const int wasModifying = d->parametersNode->StartModify();
    d->parametersNode->SetMaskSegmentationNodeID(mask->GetID());
    d->parametersNode->SetMaskSegmentID(selected[0].toUtf8().constData());
    d->parametersNode->EndModify(wasModifying);
    }
  else if (d->RegionButton->currentIndex() == Box)
    {
    logic->EnsureStatisticsROI(d->parametersNode);
    }
  logic->EnsureStatisticsTable(d->parametersNode);
  d->parametersNode->SetStatus(1);
  // without blocking the user interface (progress in the run button); the
  // results are shown when the computation is done
  QPointer<qSlicerAstroStatisticsModuleWidget> self(this);
  const bool started = logic->StartCalculateStatistics(d->parametersNode, [self](bool computed)
    {
    if (self)
      {
      self->finishCalculation(computed);
      }
    });
  if (!started)
    {
    this->finishCalculation(false);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::finishCalculation(bool computed)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkSlicerAstroStatisticsLogic* logic = d->logic();
  if (!d->parametersNode || !logic)
    {
    return;
    }
  d->parametersNode->SetStatus(0);
  const QString message = QString::fromStdString(logic->GetLastError());
  if (!computed)
    {
    d->Message->showMessage("error", message.isEmpty() ? tr("The statistics could not be computed.") : message);
    }
  else if (!message.isEmpty())
    {
    d->Message->showMessage("warning", message);
    }
  this->updateResults();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  if (d->parametersNode)
    {
    d->parametersNode->SetStatus(-1);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onSegmentEditorNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkMRMLSegmentEditorNode* segmentEditorNode = vtkMRMLSegmentEditorNode::SafeDownCast(sender);
  vtkMRMLSegmentationNode* segmentationNode = segmentEditorNode ? segmentEditorNode->GetSegmentationNode() : nullptr;
  // the mask selector follows the Segment Editor (masks only, not the contours)
  if (segmentationNode && this->isEntered() && d->SegmentationNodeComboBox->currentNode() != segmentationNode &&
      vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode))
    {
    d->SegmentationNodeComboBox->setCurrentNode(segmentationNode);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onMaskSegmentationChanged(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
  d->SegmentsTableView->setSegmentationNode(segmentationNode);
  // the Segment Editor node is shared: only the module in use sets it
  if (this->isEntered() && segmentationNode && d->segmentEditorNode &&
      d->segmentEditorNode->GetSegmentationNode() != segmentationNode)
    {
    d->segmentEditorNode->SetAndObserveSegmentationNode(segmentationNode);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::updateMaskSelector()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene || scene->IsBatchProcessing())
    {
    return;
    }
  QStringList hiddenNodeIDs;
  std::vector<vtkMRMLNode*> nodes;
  scene->GetNodesByClass("vtkMRMLSegmentationNode", nodes);
  for (vtkMRMLNode* node : nodes)
    {
    vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
    if (segmentationNode && segmentationNode->GetID() && !vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode))
      {
      hiddenNodeIDs << QString::fromUtf8(segmentationNode->GetID());
      }
    }
  d->SegmentationNodeComboBox->sortFilterProxyModel()->setHiddenNodeIDs(hiddenNodeIDs);

  // by default the mask of the input data
  vtkMRMLSegmentationNode* currentNode = vtkMRMLSegmentationNode::SafeDownCast(d->SegmentationNodeComboBox->currentNode());
  vtkMRMLSegmentationNode* editorNode = d->segmentEditorNode ? d->segmentEditorNode->GetSegmentationNode() : nullptr;
  vtkMRMLSegmentationNode* mask = vtkSlicerAstroVolumeLogic::GetMaskForVolume(scene, d->inputVolume(),
    currentNode ? currentNode : editorNode);
  if (!mask && !currentNode && vtkSlicerAstroVolumeLogic::IsMaskSegmentation(editorNode))
    {
    mask = editorNode;
    }
  if (mask && mask != currentNode)
    {
    d->SegmentationNodeComboBox->setCurrentNode(mask);
    }
  else if (currentNode && !vtkSlicerAstroVolumeLogic::IsMaskSegmentation(currentNode))
    {
    d->SegmentationNodeComboBox->setCurrentNode(nullptr);
    }
}
