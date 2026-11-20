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
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

// VTK includes
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkNew.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkTable.h>
#include <vtksys/SystemTools.hxx>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>

// AstroMasking includes
#include "qSlicerAstroMaskingModuleWidget.h"

// AstroVolume widgets includes
#include <qSlicerAstroDatasetSelector.h>
#include <qSlicerAstroFoldSection.h>
#include <qSlicerAstroMessageWidget.h>
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroResultCard.h>
#include <qSlicerAstroRunButton.h>
#include <qSlicerAstroSegmentedButton.h>
#include <qSlicerAstroTheme.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroMaskingLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>

// qMRML includes
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSortFilterProxyModel.h>
#include <qMRMLNodeComboBox.h>
#include <qMRMLSliceWidget.h>
#include <qMRMLTableView.h>

// MRMLLogic includes
#include <vtkSlicerApplicationLogic.h>
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLAstroMaskingParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#include <chrono>

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroMasking
class qSlicerAstroMaskingModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroMaskingModuleWidget);
protected:
  qSlicerAstroMaskingModuleWidget* const q_ptr;
public:

  qSlicerAstroMaskingModuleWidgetPrivate(qSlicerAstroMaskingModuleWidget& object);
  ~qSlicerAstroMaskingModuleWidgetPrivate();

  /// Initialization function of the widgets
  void init();

  /// clean pointers before importing a scene
  void cleanPointers();

  /// return the logic
  vtkSlicerAstroMaskingLogic* logic() const;

  /// The panel of the module (not a copy embedded elsewhere, e.g. in the
  /// Workbench, which arranges its own views)
  bool isModulePanel() const;
  void updateRunButton();
  void updateResult();
  /// The stacks have the height of the page shown
  static void fitStack(QStackedWidget* stack);

  // Panel
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
  qSlicerAstroSegmentedButton* OperationButton{nullptr};
  QWidget* BlankOptions{nullptr};
  qSlicerAstroSegmentedButton* BlankRegionButton{nullptr};
  QLineEdit* BlankValueLineEdit{nullptr};
  qSlicerAstroFoldSection* AdvancedSection{nullptr};
  qMRMLNodeComboBox* OutputVolumeNodeSelector{nullptr};
  qMRMLNodeComboBox* ParametersNodeComboBox{nullptr};
  qSlicerAstroRunButton* RunButton{nullptr};
  qSlicerAstroResultCard* ResultCard{nullptr};

  qSlicerAstroVolumeModuleWidget* astroVolumeWidget{nullptr};
  vtkSmartPointer<vtkMRMLAstroMaskingParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLMarkupsROINode> InputROINode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;

  bool FitROI = true;
  bool CancelRequested = false;
};

//-----------------------------------------------------------------------------
// qSlicerAstroMaskingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModuleWidgetPrivate::qSlicerAstroMaskingModuleWidgetPrivate(qSlicerAstroMaskingModuleWidget& object)
  : q_ptr(&object)
{
  this->astroVolumeWidget = nullptr;
  this->parametersNode = nullptr;
  this->InputROINode = nullptr;
  this->selectionNode = nullptr;
  this->segmentEditorNode = nullptr;
}

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModuleWidgetPrivate::~qSlicerAstroMaskingModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroMaskingModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* astroVolume = app && app->moduleManager() ?
    app->moduleManager()->module("AstroVolume") : nullptr;
  this->astroVolumeWidget = astroVolume ?
    dynamic_cast<qSlicerAstroVolumeModuleWidget*>(astroVolume->widgetRepresentation()) : nullptr;

  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(qSlicerAstroMaskingModuleWidget::tr("Masking"));
  this->Header->setIconName("module-masking");
  rootLayout->addWidget(this->Header);

  QWidget* body = new QWidget(q);
  QVBoxLayout* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(12, 12, 12, 14);
  bodyLayout->setSpacing(14);
  rootLayout->addWidget(body);

  this->InputVolumeNodeSelector = new qSlicerAstroDatasetSelector(body);
  this->InputVolumeNodeSelector->setObjectName("InputVolumeNodeSelector");
  this->InputVolumeNodeSelector->setRequiredDimensions(3);
  this->InputVolumeNodeSelector->setNoneText(qSlicerAstroMaskingModuleWidget::tr("Select a data cube"));
  this->InputVolumeNodeSelector->setToolTip(qSlicerAstroMaskingModuleWidget::tr(
    "Data cube to crop or blank (also the active volume of the views)."));
  bodyLayout->addWidget(this->InputVolumeNodeSelector);

  this->Message = new qSlicerAstroMessageWidget(body);
  this->Message->setObjectName("MessageWidget");
  bodyLayout->addWidget(this->Message);

  // Region
  QWidget* region = new QWidget(body);
  QVBoxLayout* regionLayout = new QVBoxLayout(region);
  regionLayout->setContentsMargins(0, 0, 0, 0);
  regionLayout->setSpacing(8);
  QLabel* regionTitle = new QLabel(qSlicerAstroMaskingModuleWidget::tr("Region"), region);
  qSlicerAstroTheme::setLabelRole(regionTitle, "overline");
  regionLayout->addWidget(regionTitle);
  this->RegionButton = new qSlicerAstroSegmentedButton(region);
  this->RegionButton->setObjectName("RegionButton");
  this->RegionButton->setToolTip(qSlicerAstroMaskingModuleWidget::tr("Region of the operation: a box or a mask."));
  this->RegionButton->addButton(qSlicerAstroMaskingModuleWidget::tr("Box (ROI)"),
    qSlicerAstroMaskingModuleWidget::tr("A box on the data: drag its handles in the views."));
  this->RegionButton->addButton(qSlicerAstroMaskingModuleWidget::tr("Mask"),
    qSlicerAstroMaskingModuleWidget::tr("A segment of a mask (e.g. a SoFiA source)."));
  regionLayout->addWidget(this->RegionButton);
  this->RegionStack = new QStackedWidget(region);
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
  this->ROINodeComboBox->setToolTip(qSlicerAstroMaskingModuleWidget::tr("Box of the operation."));
  roiLayout->addWidget(this->ROINodeComboBox);
  QHBoxLayout* roiButtons = new QHBoxLayout();
  roiButtons->setSpacing(6);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  this->ROIFitButton = new QPushButton(qSlicerAstroMaskingModuleWidget::tr("Fit to the data"), roiPage);
  this->ROIFitButton->setObjectName("ROIFitButton");
  this->ROIFitButton->setIcon(theme->icon("roi", "text"));
  this->ROIFitButton->setToolTip(qSlicerAstroMaskingModuleWidget::tr("The box around the whole cube, on its voxels."));
  this->ROIVisibleButton = new QPushButton(qSlicerAstroMaskingModuleWidget::tr("Show box"), roiPage);
  this->ROIVisibleButton->setObjectName("ROIVisibleButton");
  this->ROIVisibleButton->setCheckable(true);
  this->ROIVisibleButton->setChecked(true);
  this->ROIVisibleButton->setIcon(theme->icon("eye", "text"));
  this->ROIVisibleButton->setToolTip(qSlicerAstroMaskingModuleWidget::tr("Show or hide the box in the views."));
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
  this->SegmentationNodeComboBox->setToolTip(qSlicerAstroMaskingModuleWidget::tr(
    "Mask with the segments. By default the mask of the input data cube."));
  maskLayout->addWidget(this->SegmentationNodeComboBox);
  this->SegmentsTableView = new qMRMLSegmentsTableView(maskPage);
  this->SegmentsTableView->setObjectName("SegmentsTableView");
  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  this->SegmentsTableView->setMinimumHeight(120);
  this->SegmentsTableView->setMaximumHeight(170);
  this->SegmentsTableView->setToolTip(qSlicerAstroMaskingModuleWidget::tr("The selected segment is the region."));
  maskLayout->addWidget(this->SegmentsTableView);
  this->RegionStack->addWidget(maskPage);
  regionLayout->addWidget(this->RegionStack);
  bodyLayout->addWidget(region);

  // Operation
  QWidget* operation = new QWidget(body);
  QVBoxLayout* operationLayout = new QVBoxLayout(operation);
  operationLayout->setContentsMargins(0, 0, 0, 0);
  operationLayout->setSpacing(8);
  QLabel* operationTitle = new QLabel(qSlicerAstroMaskingModuleWidget::tr("Operation"), operation);
  qSlicerAstroTheme::setLabelRole(operationTitle, "overline");
  operationLayout->addWidget(operationTitle);
  this->OperationButton = new qSlicerAstroSegmentedButton(operation);
  this->OperationButton->setObjectName("OperationButton");
  this->OperationButton->setToolTip(qSlicerAstroMaskingModuleWidget::tr("Crop the cube to the region, or blank voxels."));
  this->OperationButton->addButton(qSlicerAstroMaskingModuleWidget::tr("Crop"),
    qSlicerAstroMaskingModuleWidget::tr("A smaller cube: the voxels of the bounding box of the region (with its WCS)."));
  this->OperationButton->addButton(qSlicerAstroMaskingModuleWidget::tr("Blank"),
    qSlicerAstroMaskingModuleWidget::tr("The same cube with the voxels inside or outside the region set to a value."));
  operationLayout->addWidget(this->OperationButton);
  this->BlankOptions = new QWidget(operation);
  QHBoxLayout* blankLayout = new QHBoxLayout(this->BlankOptions);
  blankLayout->setContentsMargins(0, 0, 0, 0);
  blankLayout->setSpacing(6);
  this->BlankRegionButton = new qSlicerAstroSegmentedButton(this->BlankOptions);
  this->BlankRegionButton->setObjectName("BlankRegionButton");
  this->BlankRegionButton->setToolTip(qSlicerAstroMaskingModuleWidget::tr("Voxels to blank."));
  this->BlankRegionButton->addButton(qSlicerAstroMaskingModuleWidget::tr("Inside"),
    qSlicerAstroMaskingModuleWidget::tr("Blank the voxels inside the region."));
  this->BlankRegionButton->addButton(qSlicerAstroMaskingModuleWidget::tr("Outside"),
    qSlicerAstroMaskingModuleWidget::tr("Blank the voxels outside the region."));
  blankLayout->addWidget(this->BlankRegionButton, 2);
  QLabel* valueLabel = new QLabel(qSlicerAstroMaskingModuleWidget::tr("value"), this->BlankOptions);
  valueLabel->setProperty("astroRole", "label");
  blankLayout->addWidget(valueLabel);
  this->BlankValueLineEdit = new QLineEdit(this->BlankOptions);
  this->BlankValueLineEdit->setObjectName("BlankValueLineEdit");
  this->BlankValueLineEdit->setMaximumWidth(80);
  this->BlankValueLineEdit->setToolTip(qSlicerAstroMaskingModuleWidget::tr(
    "Value of the blanked voxels: a number, or NaN (blank voxels are ignored by the statistics, moment maps, "
    "profiles and smoothing)."));
  valueLabel->setToolTip(this->BlankValueLineEdit->toolTip());
  blankLayout->addWidget(this->BlankValueLineEdit, 1);
  operationLayout->addWidget(this->BlankOptions);
  bodyLayout->addWidget(operation);

  // Advanced
  this->AdvancedSection = new qSlicerAstroFoldSection(body);
  this->AdvancedSection->setObjectName("AdvancedSection");
  this->AdvancedSection->setTitle(qSlicerAstroMaskingModuleWidget::tr("Advanced"));
  this->AdvancedSection->setSummary(qSlicerAstroMaskingModuleWidget::tr("output, parameter set"));
  this->AdvancedSection->setSettingsKey("SlicerAstro/AstroMasking/AdvancedCollapsed");
  QWidget* advancedParent = this->AdvancedSection->contentWidget();
  QFormLayout* advancedLayout = new QFormLayout();
  advancedLayout->setContentsMargins(0, 0, 0, 0);
  advancedLayout->setSpacing(8);
  this->OutputVolumeNodeSelector = new qMRMLNodeComboBox(advancedParent);
  this->OutputVolumeNodeSelector->setObjectName("OutputVolumeNodeSelector");
  this->OutputVolumeNodeSelector->setNodeTypes(QStringList() << "vtkMRMLAstroVolumeNode");
  this->OutputVolumeNodeSelector->setNoneEnabled(true);
  this->OutputVolumeNodeSelector->setAddEnabled(false);
  this->OutputVolumeNodeSelector->setEditEnabled(false);
  this->OutputVolumeNodeSelector->setRenameEnabled(true);
  this->OutputVolumeNodeSelector->setSelectNodeUponCreation(false);
  this->OutputVolumeNodeSelector->setNoneDisplay(qSlicerAstroMaskingModuleWidget::tr("New cube"));
  this->OutputVolumeNodeSelector->setToolTip(qSlicerAstroMaskingModuleWidget::tr(
    "Cube replaced by the next operation (a new cube if None)."));
  this->ParametersNodeComboBox = new qMRMLNodeComboBox(advancedParent);
  this->ParametersNodeComboBox->setObjectName("ParametersNodeComboBox");
  this->ParametersNodeComboBox->setNodeTypes(QStringList() << "vtkMRMLAstroMaskingParametersNode");
  this->ParametersNodeComboBox->setShowHidden(true);
  this->ParametersNodeComboBox->setAddEnabled(false);
  this->ParametersNodeComboBox->setEditEnabled(false);
  this->ParametersNodeComboBox->setRenameEnabled(true);
  this->ParametersNodeComboBox->setToolTip(qSlicerAstroMaskingModuleWidget::tr("Parameters of the module (saved with the scene)."));
  for (QWidget* field : {static_cast<QWidget*>(this->OutputVolumeNodeSelector),
                         static_cast<QWidget*>(this->ParametersNodeComboBox)})
    {
    qMRMLNodeComboBox* selector = qobject_cast<qMRMLNodeComboBox*>(field);
    selector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    selector->setMinimumWidth(80);
    }
  QLabel* outputLabel = new QLabel(qSlicerAstroMaskingModuleWidget::tr("Output"), advancedParent);
  outputLabel->setProperty("astroRole", "label");
  outputLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  QLabel* parametersLabel = new QLabel(qSlicerAstroMaskingModuleWidget::tr("Parameter set"), advancedParent);
  parametersLabel->setProperty("astroRole", "label");
  parametersLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  advancedLayout->addRow(outputLabel, this->OutputVolumeNodeSelector);
  advancedLayout->addRow(parametersLabel, this->ParametersNodeComboBox);
  this->AdvancedSection->contentLayout()->addLayout(advancedLayout);
  bodyLayout->addWidget(this->AdvancedSection);

  // Footer
  QFrame* footer = new QFrame(q);
  footer->setProperty("astroRole", "panelFooter");
  QVBoxLayout* footerLayout = new QVBoxLayout(footer);
  footerLayout->setContentsMargins(12, 12, 12, 12);
  footerLayout->setSpacing(8);
  this->RunButton = new qSlicerAstroRunButton(footer);
  this->RunButton->setObjectName("RunButton");
  this->RunButton->pushButton()->setObjectName("ApplyButton");
  footerLayout->addWidget(this->RunButton);
  this->ResultCard = new qSlicerAstroResultCard(footer);
  this->ResultCard->setObjectName("ResultCard");
  this->ResultCard->hide();
  footerLayout->addWidget(this->ResultCard);
  rootLayout->addWidget(footer);
  rootLayout->addStretch(1);

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroMaskingParametersNode(vtkMRMLNode*)));
  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));
  QObject::connect(this->OutputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));
  QObject::connect(this->ROINodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputROIChanged(vtkMRMLNode*)));
  for (QObject* selector : std::vector<QObject*>{this->InputVolumeNodeSelector, this->ParametersNodeComboBox,
         this->OutputVolumeNodeSelector, this->ROINodeComboBox, this->SegmentationNodeComboBox,
         this->SegmentsTableView})
    {
    QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), selector, SLOT(setMRMLScene(vtkMRMLScene*)));
    }
  QObject::connect(this->RegionButton, SIGNAL(currentIndexChanged(int)), q, SLOT(onRegionChanged(int)));
  QObject::connect(this->OperationButton, SIGNAL(currentIndexChanged(int)), q, SLOT(onOperationChanged(int)));
  QObject::connect(this->BlankRegionButton, SIGNAL(currentIndexChanged(int)), q, SLOT(onBlankRegionChanged(int)));
  QObject::connect(this->BlankValueLineEdit, SIGNAL(editingFinished()), q, SLOT(onBlankValueChanged()));
  QObject::connect(this->ROIFitButton, SIGNAL(clicked()), q, SLOT(onROIFit()));
  QObject::connect(this->ROIVisibleButton, SIGNAL(toggled(bool)), q, SLOT(onROIVisibilityChanged(bool)));
  QObject::connect(this->RunButton, SIGNAL(clicked()), q, SLOT(onApply()));
  QObject::connect(this->RunButton, SIGNAL(cancelRequested()), q, SLOT(onComputationCancelled()));
  QObject::connect(this->ResultCard, SIGNAL(showRequested()), q, SLOT(onShowResult()));
  QObject::connect(this->InputVolumeNodeSelector, &qSlicerAstroDatasetSelector::currentNodeChanged,
                   q, [this](vtkMRMLNode*) { this->updateRunButton(); });

  theme->applyTo(q);
  this->updateRunButton();
}

//-----------------------------------------------------------------------------
bool qSlicerAstroMaskingModuleWidgetPrivate::isModulePanel() const
{
  Q_Q(const qSlicerAstroMaskingModuleWidget);
  return !q->property("astroEmbedded").toBool();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidgetPrivate::fitStack(QStackedWidget* stack)
{
  for (int page = 0; page < stack->count(); page++)
    {
    stack->widget(page)->setSizePolicy(QSizePolicy::Preferred,
      page == stack->currentIndex() ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    }
  stack->updateGeometry();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidgetPrivate::updateRunButton()
{
  const bool input = this->InputVolumeNodeSelector->currentNode() != nullptr;
  const bool blank = this->OperationButton->currentIndex() == 1;
  this->RunButton->setText(!input ? qSlicerAstroMaskingModuleWidget::tr("Select a data cube") :
                           blank ? qSlicerAstroMaskingModuleWidget::tr("Blank") :
                                   qSlicerAstroMaskingModuleWidget::tr("Crop"));
  this->RunButton->setEnabled(input);
  this->RunButton->pushButton()->setToolTip(blank ?
    qSlicerAstroMaskingModuleWidget::tr("Blank the voxels of the region in a new cube.") :
    qSlicerAstroMaskingModuleWidget::tr("Crop the cube to the region in a new cube."));
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidgetPrivate::updateResult()
{
  Q_Q(qSlicerAstroMaskingModuleWidget);
  vtkMRMLAstroVolumeNode* output = (this->parametersNode && q->mrmlScene() &&
    this->parametersNode->GetOutputVolumeNodeID()) ? vtkMRMLAstroVolumeNode::SafeDownCast(
      q->mrmlScene()->GetNodeByID(this->parametersNode->GetOutputVolumeNodeID())) : nullptr;
  const std::string name = output && output->GetName() ? output->GetName() : "";
  const bool show = output && output->GetImageData() &&
    (name.find("_Blank_") != std::string::npos || name.find("_Crop_") != std::string::npos);
  if (show)
    {
    this->ResultCard->setVolume(output);
    }
  this->ResultCard->setVisible(show);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroMaskingModuleWidget);

  if (!q->mrmlScene())
    {
    return;
    }

  // the segmentations and the Segment Editor node are shared with the other
  // modules (and are the user's data): only the references are dropped
  this->segmentEditorNode = nullptr;
  if (this->SegmentsTableView)
    {
    this->SegmentsTableView->setSegmentationNode(nullptr);
    }

  if (this->parametersNode)
    {
    q->mrmlScene()->RemoveNode(this->parametersNode);
    }
  this->parametersNode = 0;

  if (this->InputROINode)
    {
    q->mrmlScene()->RemoveNode(this->InputROINode);
    }
  this->InputROINode = 0;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroMaskingLogic* qSlicerAstroMaskingModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroMaskingModuleWidget);
  return vtkSlicerAstroMaskingLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroMaskingModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModuleWidget::qSlicerAstroMaskingModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroMaskingModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroMaskingModuleWidget);
  d->init();

  QObject::connect(d->SegmentationNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this, SLOT(onMaskSegmentationChanged(vtkMRMLNode*)));
}

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModuleWidget::~qSlicerAstroMaskingModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::enter()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  this->Superclass::enter();

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager() || !app->layoutManager()->layoutLogic()
     || !app->layoutManager()->layoutLogic()->GetLayoutNode())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "qSlicerApplication not found.";
    return;
    }

  // the module panel shows the input and the output side by side in 3D
  if (d->isModulePanel())
    {
    app->layoutManager()->layoutLogic()->GetLayoutNode()->SetViewArrangement
            (vtkMRMLLayoutNode::SlicerLayoutDual3DView);
    }

  this->onMRMLAstroMaskingParametersNodeModified();

  this->updateMaskSelector();
  this->onMaskSegmentationChanged(d->SegmentationNodeComboBox->currentNode());
}

//----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::exit()
{
  this->Superclass::exit();

  this->onROIVisibilityChanged(false);
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}

} // end namespace

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::setMRMLScene : selectionNode not found!";
    return;
    }

  this->initializeNodes();

  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent,
                      this, SLOT(onEndCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::StartImportEvent,
                      this, SLOT(onStartImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent,
                      this, SLOT(onEndImportEvent()));
  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceAddedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceAdded(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceRemovedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceRemoved(vtkObject*)));

  this->onMRMLSelectionNodeModified(d->selectionNode);
  this->onMRMLSelectionNodeReferenceAdded(d->selectionNode);

  // masks added, removed or modified (e.g. the contours)
  this->qvtkReconnect(scene, vtkMRMLScene::NodeAddedEvent, this, SLOT(updateMaskSelector()));
  this->qvtkReconnect(scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(updateMaskSelector()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(updateMaskSelector()));
  this->updateMaskSelector();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  this->initializeParameterNode(forceNew);

  this->initializeSegmentations(forceNew);

  this->initializeROINode(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::onEndCloseEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes(true);
  this->onMRMLAstroMaskingParametersNodeModified();

  if (!this->isEntered())
    {
    this->onROIVisibilityChanged(false);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::onEndImportEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroMaskingParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onInputROIChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLMarkupsROINode* roiNode = vtkMRMLMarkupsROINode::SafeDownCast(mrmlNode);
  if (d->InputROINode == roiNode)
    {
    return;
    }

  d->InputROINode = roiNode;
  d->parametersNode->SetROINode(roiNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroMaskingParametersNode *astroParametersNode = nullptr;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroMaskingParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroMaskingParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroMaskingParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroMaskingParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroMaskingParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::initializeROINode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkSmartPointer<vtkMRMLNode> roiNode = nullptr;

  if (!forceNew)
    {
    if (d->parametersNode->GetROINode())
      {
      roiNode = d->parametersNode->GetROINode();
      }
    else
      {
      vtkSmartPointer<vtkCollection> roiNodes = vtkSmartPointer<vtkCollection>::Take
          (this->mrmlScene()->GetNodesByClass("vtkMRMLMarkupsROINode"));

      for (int ii = roiNodes->GetNumberOfItems() - 1; ii >= 0 ; ii--)
        {
        vtkMRMLNode* tempROINode = vtkMRMLNode::SafeDownCast(roiNodes->GetItemAsObject(ii));
        if (!tempROINode)
          {
          continue;
          }
        std::string ROIName = tempROINode->GetName();
        std::size_t found = ROIName.find("MaskingROI");
        if (found != std::string::npos)
          {
          roiNode = tempROINode;
          break;
          }
        }
      }

    if (roiNode)
      {
      d->InputROINode = vtkMRMLMarkupsROINode::SafeDownCast(roiNode);
      d->parametersNode->SetROINode(d->InputROINode);
      return;
      }
    }

  vtkMRMLNode * foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLMarkupsROINode");
  roiNode.TakeReference(foo);
  std::string ROINodeName = this->mrmlScene()->GenerateUniqueName("MaskingROI");
  roiNode->SetName(ROINodeName.c_str());
  this->mrmlScene()->AddNode(roiNode);

  d->InputROINode = vtkMRMLMarkupsROINode::SafeDownCast(roiNode);
  d->parametersNode->SetROINode(d->InputROINode);
  int wasModifyingROI = d->InputROINode->StartModify();
  this->onROIFit();
  d->InputROINode->EndModify(wasModifyingROI);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  std::string segmentEditorSingletonTag = "SegmentEditor";
  vtkMRMLSegmentEditorNode *segmentEditorNodeSingleton = vtkMRMLSegmentEditorNode::SafeDownCast(
    this->mrmlScene()->GetSingletonNode(segmentEditorSingletonTag.c_str(), "vtkMRMLSegmentEditorNode"));

  if (!segmentEditorNodeSingleton)
    {
    d->segmentEditorNode = vtkSmartPointer<vtkMRMLSegmentEditorNode>::New();
    d->segmentEditorNode->SetSingletonTag(segmentEditorSingletonTag.c_str());
    d->segmentEditorNode = vtkMRMLSegmentEditorNode::SafeDownCast(
    this->mrmlScene()->AddNode(d->segmentEditorNode));
    }
  else
    {
    d->segmentEditorNode = segmentEditorNodeSingleton;
    }

  this->qvtkReconnect(d->segmentEditorNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onSegmentEditorNodeModified(vtkObject*)));

  this->onSegmentEditorNodeModified(d->segmentEditorNode);

  if (!d->segmentEditorNode->GetSegmentationNode() || forceNew)
    {
    // the mask of the active astronomical volume (no segmentation without data)
    vtkSlicerAstroVolumeLogic* astroVolumeLogic = d->logic() ? d->logic()->GetAstroVolumeLogic() : nullptr;
    vtkMRMLSelectionNode* activeSelectionNode = (this->module() && this->module()->appLogic()) ?
      this->module()->appLogic()->GetSelectionNode() : nullptr;
    vtkMRMLAstroVolumeNode* activeVolume = (activeSelectionNode && activeSelectionNode->GetActiveVolumeID()) ?
      vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->GetNodeByID(activeSelectionNode->GetActiveVolumeID())) : nullptr;
    vtkMRMLSegmentationNode* activeMask = (astroVolumeLogic && activeVolume) ?
      astroVolumeLogic->GetMaskSegmentationForVolume(activeVolume, true) : nullptr;
    if (activeMask)
      {
      d->segmentEditorNode->SetAndObserveSegmentationNode(activeMask);
      }
    }

  if (d->segmentEditorNode->GetSegmentationNode())
    {
    d->segmentEditorNode->GetSegmentationNode()->CreateDefaultDisplayNodes();
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroMaskingModuleWidget::setMaskSegmentFromSelection()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(
    d->SegmentationNodeComboBox->currentNode());
  if (!segmentationNode && d->segmentEditorNode)
    {
    segmentationNode = d->segmentEditorNode->GetSegmentationNode();
    }
  if (!d->parametersNode || !segmentationNode || !segmentationNode->GetSegmentation())
    {
    d->Message->showMessage("error", tr("No mask: select a segmentation with the segment of the region."));
    return false;
    }

  QStringList selectedSegmentIDs = d->SegmentsTableView->selectedSegmentIDs();
  if (selectedSegmentIDs.size() < 1)
    {
    d->Message->showMessage("error", tr("Select the segment of the region in the table."));
    return false;
    }

  // the logic reads the segment directly (no label map volume)
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetMaskSegmentationNodeID(segmentationNode->GetID());
  d->parametersNode->SetMaskSegmentID(selectedSegmentIDs[0].toUtf8().constData());
  d->parametersNode->EndModify(wasModifying);
  return true;
}

//--------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onBlankValueChanged()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetBlankValue(d->BlankValueLineEdit->text().toStdString().c_str());
}

//--------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!sender)
    {
    return;
    }

  vtkMRMLSelectionNode *selectionNode =
      vtkMRMLSelectionNode::SafeDownCast(sender);

  if (!selectionNode)
    {
    return;
    }

  vtkMRMLSegmentEditorNode* segmentEditorNode = vtkMRMLSegmentEditorNode::SafeDownCast(
      selectionNode->GetNodeReference("SegmentEditorNodeRef"));

  if (!segmentEditorNode)
    {
    return;
    }

  d->segmentEditorNode = segmentEditorNode;
}

//--------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!sender)
    {
    return;
    }

  vtkMRMLSelectionNode *selectionNode =
      vtkMRMLSelectionNode::SafeDownCast(sender);

  if (!selectionNode)
    {
    return;
    }

  vtkMRMLSegmentEditorNode* segmentEditorNode = vtkMRMLSegmentEditorNode::SafeDownCast(
      selectionNode->GetNodeReference("SegmentEditorNodeRef"));

  if (!segmentEditorNode)
    {
    return;
    }

  d->segmentEditorNode = segmentEditorNode;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onOperationChanged(int index)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  d->updateRunButton();
  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetOperation(index == 1 ? "Blank" : "Crop");
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onRegionChanged(int index)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetMode(index == 1 ? "Segmentation" : "ROI");
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onBlankRegionChanged(int index)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetBlankRegion(index == 1 ? "Outside" : "Inside");
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onShowResult()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  vtkSlicerApplicationLogic* appLogic = this->module() ? this->module()->appLogic() : nullptr;
  if (!d->parametersNode || !d->selectionNode || !appLogic || !d->parametersNode->GetOutputVolumeNodeID())
    {
    return;
    }
  d->selectionNode->SetActiveVolumeID(d->parametersNode->GetOutputVolumeNodeID());
  appLogic->PropagateVolumeSelection(0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetOutputVolumeNodeID(mrmlNode->GetID());
    }
  else
    {
    d->parametersNode->SetOutputVolumeNodeID(nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onROIFit()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  d->logic()->SnapROIToVoxelGrid(d->parametersNode);
  d->logic()->FitROIToInputVolume(d->parametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onROIVisibilityChanged(bool visible)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLMarkupsROINode *ROINode = d->parametersNode->GetROINode();
  if(!ROINode)
    {
    return;
    }

  ROINode->SetDisplayVisibility(visible);
}

//--------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::setMRMLAstroMaskingParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroMaskingParametersNode* AstroMaskingParaNode =
      vtkMRMLAstroMaskingParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroMaskingParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroMaskingParametersNodeModified()));

  d->parametersNode = AstroMaskingParaNode;

  this->onMRMLAstroMaskingParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode || !d->selectionNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    }
  else
    {
    d->selectionNode->SetActiveVolumeID(nullptr);
    d->selectionNode->SetActiveVolumeID(nullptr);
    }

  this->updateMaskSelector();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onMRMLAstroMaskingParametersNodeModified()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  const char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNodeID ? this->mrmlScene()->GetNodeByID(inputVolumeNodeID) : nullptr);
  const char *outputVolumeNodeID = d->parametersNode->GetOutputVolumeNodeID();
  d->OutputVolumeNodeSelector->setCurrentNode(outputVolumeNodeID ? this->mrmlScene()->GetNodeByID(outputVolumeNodeID) : nullptr);
  d->ROINodeComboBox->setCurrentNode(d->parametersNode->GetROINode());

  const bool maskMode = d->parametersNode->GetMode() && !strcmp(d->parametersNode->GetMode(), "Segmentation");
  d->RegionButton->setCurrentIndex(maskMode ? 1 : 0);
  d->RegionStack->setCurrentIndex(maskMode ? 1 : 0);
  qSlicerAstroMaskingModuleWidgetPrivate::fitStack(d->RegionStack);
  // the box is shown while the panel is in use, in box mode
  this->onROIVisibilityChanged(!maskMode && this->isEntered() && d->ROIVisibleButton->isChecked());
  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode ?
    d->segmentEditorNode->GetSegmentationNode() : nullptr;
  if (currentSegmentationNode && this->isEntered())
    {
    for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
      {
      vtkMRMLSegmentationDisplayNode *segmentationDisplayNode =
        vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
      if (segmentationDisplayNode)
        {
        segmentationDisplayNode->SetAllSegmentsVisibility(maskMode);
        }
      }
    }

  const bool blank = d->parametersNode->GetOperation() && !strcmp(d->parametersNode->GetOperation(), "Blank");
  d->OperationButton->setCurrentIndex(blank ? 1 : 0);
  d->BlankOptions->setVisible(blank);
  const bool outside = d->parametersNode->GetBlankRegion() && !strcmp(d->parametersNode->GetBlankRegion(), "Outside");
  d->BlankRegionButton->setCurrentIndex(outside ? 1 : 0);
  d->BlankValueLineEdit->setText(d->parametersNode->GetBlankValue());
  d->updateRunButton();
  d->updateResult();

  int status = d->parametersNode->GetStatus();
  if(status == 0)
    {
    this->onComputationFinished();
    }
  else
    {
    if(status == 1)
      {
      this->onComputationStarted();
      }
    if(status != -1)
      {
      this->updateProgress(status);
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onMRMLSelectionNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!sender)
    {
    return;
    }

  vtkMRMLSelectionNode *selectionNode =
      vtkMRMLSelectionNode::SafeDownCast(sender);

  if (!selectionNode || !d->parametersNode)
    {
    return;
    }

  if (d->parametersNode->GetInputVolumeNodeID() && selectionNode->GetActiveVolumeID())
    {
    if(!strcmp(d->parametersNode->GetInputVolumeNodeID(), selectionNode->GetActiveVolumeID()))
      {
      return;
      }
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
  d->parametersNode->EndModify(wasModifying);

  if (d->FitROI)
    {
    this->onROIFit();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onApply()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::on Apply : "
                   "parametersNode not found!";
    return;
    }

  vtkSlicerAstroMaskingLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroMaskingModuleWidget::on Apply : "
                  "vtkSlicerAstroMaskingLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->segmentEditorNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::on Apply : "
                   "d->segmentEditorNode not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->Message->clearMessage();
  d->CancelRequested = false;
  d->FitROI = false;
  d->parametersNode->SetStatus(1);

  if (!(strcmp(d->parametersNode->GetMode(), "Segmentation")))
    {
    if (!this->setMaskSegmentFromSelection())
      {
      qCritical() <<"qSlicerAstroMaskingModuleWidget::on Apply : "
                    "setMaskSegmentFromSelection failed!";
      d->parametersNode->SetStatus(0);
      return;
      }
    }

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroMaskingModuleWidget::onApply"
                  " : scene not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    d->Message->showMessage("error", tr("Select the data cube to crop or blank."));
    d->parametersNode->SetStatus(0);
    d->FitROI = true;
    return;
    }

  // Create output volume node
  std::ostringstream outSS;
  outSS << inputVolume->GetName();

  if (!(strcmp(d->parametersNode->GetOperation(), "Blank")))
    {
    outSS<<"_Blank";
    }
  else if (!(strcmp(d->parametersNode->GetOperation(), "Crop")))
    {
    outSS<<"_Crop";
    }

  int serial = d->parametersNode->GetOutputSerial();
  outSS<<"_"<< IntToString(serial);
  serial++;
  d->parametersNode->SetOutputSerial(serial);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  if (outputVolume)
    {
    std::string name;
    name = outputVolume->GetName();
    if (!name.compare(inputVolume->GetName()))
      {
      outputVolume = nullptr;
      }
    else if (name.find("_Blank_") != std::string::npos ||
             name.find("_Crop_") != std::string::npos )
      {
      vtkMRMLAstroVolumeStorageNode* astroStorage =
        vtkMRMLAstroVolumeStorageNode::SafeDownCast(outputVolume->GetStorageNode());
      scene->RemoveNode(astroStorage);
      scene->RemoveNode(outputVolume->GetAstroVolumeDisplayNode());
      vtkMRMLVolumeRenderingDisplayNode *volumeRenderingDisplay =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(outputVolume->GetDisplayNode());
      if (volumeRenderingDisplay)
        {
        scene->RemoveNode(volumeRenderingDisplay->GetROINode());
        }
      scene->RemoveNode(outputVolume);
      }
    }

  // Create Astro Volume for the output
  outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (logic->GetAstroVolumeLogic()->CloneVolume(scene, inputVolume, outSS.str().c_str()));

  d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());

  vtkMRMLNode* node = nullptr;
  outputVolume->SetPresetNode(node);

  // Remove old rendering Display
  int ndnodes = outputVolume->GetNumberOfDisplayNodes();
  for (int ii = 0; ii < ndnodes; ii++)
    {
    vtkMRMLVolumeRenderingDisplayNode *dnode =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
        outputVolume->GetNthDisplayNode(ii));
    if (dnode)
      {
      outputVolume->RemoveNthDisplayNodeID(ii);
      }
    }

  vtkNew<vtkMatrix4x4> transformationMatrix;
  inputVolume->GetRASToIJKMatrix(transformationMatrix.GetPointer());
  outputVolume->SetRASToIJKMatrix(transformationMatrix.GetPointer());
  outputVolume->SetAndObserveTransformNodeID(inputVolume->GetTransformNodeID());

  // Run the operation without blocking the user interface (progress in the
  // run button); the result is shown when it is done
  QPointer<qSlicerAstroMaskingModuleWidget> self(this);
  vtkSmartPointer<vtkMRMLAstroVolumeNode> inputHold = inputVolume;
  vtkSmartPointer<vtkMRMLAstroVolumeNode> outputHold = outputVolume;
  const bool started = logic->StartApplyMask(d->parametersNode, [self, inputHold, outputHold](bool success)
    {
    if (self)
      {
      self->finishOperation(success, inputHold, outputHold);
      }
    });
  if (!started)
    {
    this->finishOperation(false, inputVolume, outputVolume);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::finishOperation(bool success, vtkMRMLAstroVolumeNode* inputVolume,
                                                     vtkMRMLAstroVolumeNode* outputVolume)
{
  Q_D(qSlicerAstroMaskingModuleWidget);
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene || !d->parametersNode || !inputVolume || !outputVolume)
    {
    return;
    }
  const bool blank = d->parametersNode->GetOperation() && !strcmp(d->parametersNode->GetOperation(), "Blank");
  if (success)
    {
    // the module panel compares the input and the output in 3D (the
    // Workbench lists the output with the products of the input)
    if (d->isModulePanel() && d->astroVolumeWidget)
      {
      d->astroVolumeWidget->setComparative3DViews(inputVolume->GetID(), outputVolume->GetID(), false, blank);
      }
    d->Message->showMessage("success", blank ? tr("Blanked cube created") : tr("Cropped cube created"));
    }
  else
    {
    const bool cancelled = d->CancelRequested;
    scene->RemoveNode(outputVolume);
    d->parametersNode->SetOutputVolumeNodeID(nullptr);
    d->Message->showMessage(cancelled ? "info" : "error", cancelled ? tr("Computation cancelled.") :
      tr("The operation failed (see the error log)."));
    }

  d->parametersNode->SetStatus(0);
  d->FitROI = true;

  if (!(strcmp(d->parametersNode->GetMode(), "Segmentation")))
    {
    if (d->segmentEditorNode)
      {
      vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
      if (currentSegmentationNode && this->isEntered())
        {
        for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
          {
          vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
            vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
          if (!SegmentationDisplayNode)
            {
            continue;
            }
          SegmentationDisplayNode->SetAllSegmentsVisibility(false);
          }
        }
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroMaskingModuleWidget);
  d->RunButton->setRunning(false);
  d->updateResult();
}

//---------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!sender)
    {
    return;
    }

  vtkMRMLSegmentEditorNode *segmentEditorNode =
    vtkMRMLSegmentEditorNode::SafeDownCast(sender);
  if (!segmentEditorNode)
    {
    return;
    }

  vtkMRMLSegmentationNode* segmentationNode =
    segmentEditorNode->GetSegmentationNode();
  if (!segmentationNode)
    {
    return;
    }

  // the mask selector follows the Segment Editor (masks only, not the
  // contours); the table shows the selected mask (onMaskSegmentationChanged)
  if (this->isEntered() && d->SegmentationNodeComboBox->currentNode() != segmentationNode &&
      vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode))
    {
    d->SegmentationNodeComboBox->setCurrentNode(segmentationNode);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onMaskSegmentationChanged(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

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
void qSlicerAstroMaskingModuleWidget::updateMaskSelector()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene || !d->SegmentationNodeComboBox || scene->IsBatchProcessing())
    {
    return;
    }

  QStringList hiddenNodeIDs;
  std::vector<vtkMRMLNode*> nodes;
  scene->GetNodesByClass("vtkMRMLSegmentationNode", nodes);
  for (vtkMRMLNode* node : nodes)
    {
    vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
    if (segmentationNode && segmentationNode->GetID() &&
        !vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode))
      {
      hiddenNodeIDs << QString::fromUtf8(segmentationNode->GetID());
      }
    }
  d->SegmentationNodeComboBox->sortFilterProxyModel()->setHiddenNodeIDs(hiddenNodeIDs);

  // by default the mask of the input volume
  vtkMRMLSegmentationNode* currentNode =
    vtkMRMLSegmentationNode::SafeDownCast(d->SegmentationNodeComboBox->currentNode());
  vtkMRMLSegmentationNode* editorNode = d->segmentEditorNode ? d->segmentEditorNode->GetSegmentationNode() : nullptr;
  vtkMRMLVolumeNode* inputVolume = (d->parametersNode && d->parametersNode->GetInputVolumeNodeID()) ?
    vtkMRMLVolumeNode::SafeDownCast(scene->GetNodeByID(d->parametersNode->GetInputVolumeNodeID())) : nullptr;
  vtkMRMLSegmentationNode* mask = vtkSlicerAstroVolumeLogic::GetMaskForVolume(scene, inputVolume,
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

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroMaskingModuleWidget);
  d->CancelRequested = true;
  if (d->parametersNode)
    {
    d->parametersNode->SetStatus(-1);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroMaskingModuleWidget);
  d->RunButton->setProgress(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroMaskingModuleWidget);
  d->RunButton->setProgressText(d->OperationButton->currentIndex() == 1 ? tr("Blanking") : tr("Cropping"));
  d->RunButton->setRunning(true);
}

//---------------------------------------------------------------------------
vtkMRMLAstroMaskingParametersNode* qSlicerAstroMaskingModuleWidget::
mrmlAstroMaskingParametersNode()const
{
  Q_D(const qSlicerAstroMaskingModuleWidget);

  return d->parametersNode;
}

