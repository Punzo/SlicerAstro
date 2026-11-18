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
#include <QAbstractItemView>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkWeakPointer.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>

// AstroMomentMaps includes
#include "qSlicerAstroMomentMapsModuleWidget.h"

// AstroVolume widgets includes
#include <qSlicerAstroDatasetSelector.h>
#include <qSlicerAstroFoldSection.h>
#include <qSlicerAstroMessageWidget.h>
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroResultCard.h>
#include <qSlicerAstroRunButton.h>
#include <qSlicerAstroSegmentedButton.h>
#include <qSlicerAstroSpectrumRangeWidget.h>
#include <qSlicerAstroTheme.h>
#include <qSlicerAstroThumbnails.h>
#include <qSlicerAstroToggleCard.h>
#include <qSlicerAstroValueField.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroMomentMapsLogic.h>

// qMRML includes
#include <qMRMLNodeComboBox.h>
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSortFilterProxyModel.h>

// MRMLLogic includes
#include <vtkSlicerApplicationLogic.h>
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <vector>

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << (num ? num : "");
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

//----------------------------------------------------------------------------
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}

//----------------------------------------------------------------------------
/// Flux with a readable prefix (e.g. 0.00062 Jy/beam -> 0.62 mJy/beam)
QString FormatWithUnit(double value, const QString& unit)
{
  const QString shortUnit = qSlicerAstroTheme::displayUnit(unit);
  if (shortUnit.startsWith("Jy") && std::abs(value) < 0.1 && value != 0.)
    {
    return QString("%1 m%2").arg(value * 1000., 0, 'g', 3).arg(shortUnit);
    }
  return QString("%1 %2").arg(value, 0, 'g', 3).arg(shortUnit);
}

//----------------------------------------------------------------------------
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}
} // end namespace

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroMomentMaps
class qSlicerAstroMomentMapsModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroMomentMapsModuleWidget);
protected:
  qSlicerAstroMomentMapsModuleWidget* const q_ptr;
public:

  qSlicerAstroMomentMapsModuleWidgetPrivate(qSlicerAstroMomentMapsModuleWidget& object);
  ~qSlicerAstroMomentMapsModuleWidgetPrivate();

  void init();
  void cleanPointers();

  vtkSlicerAstroMomentMapsLogic* logic() const;
  vtkMRMLAstroVolumeNode* inputVolume() const;
  /// Noise of the input (0 if unknown)
  double noise() const;
  /// Number of maps to compute (1 and 2 need 0, 2 needs 1)
  void updateRunButton();
  void updateResults();
  void showMap(const char* volumeID, const char* sliceNodeID, const char* compositeNodeID);
  /// Output volume of a moment map: the previous output of the same kind
  /// is replaced
  vtkMRMLAstroVolumeNode* createMapVolume(vtkMRMLAstroVolumeNode* input, const char* existingID,
                                          const char* suffix, const std::string& unit,
                                          const char* colorTableName, const char* datamodel, int serial);

  // Panel
  qSlicerAstroPanelHeader* Header{nullptr};
  qSlicerAstroDatasetSelector* InputVolumeNodeSelector{nullptr};
  qSlicerAstroMessageWidget* Message{nullptr};
  qSlicerAstroToggleCard* ZeroMomentToggle{nullptr};
  qSlicerAstroToggleCard* FirstMomentToggle{nullptr};
  qSlicerAstroToggleCard* SecondMomentToggle{nullptr};
  QPushButton* AutoRangeButton{nullptr};
  qSlicerAstroSpectrumRangeWidget* VelocityRangeWidget{nullptr};
  qSlicerAstroValueField* VelocityMinField{nullptr};
  qSlicerAstroValueField* VelocityMaxField{nullptr};
  QLabel* VelocityNote{nullptr};
  qSlicerAstroSegmentedButton* VoxelSelectionButton{nullptr};
  QStackedWidget* VoxelSelectionStack{nullptr};
  qSlicerAstroValueField* ThresholdField{nullptr};
  QLabel* ThresholdInfo{nullptr};
  qMRMLNodeComboBox* SegmentationNodeComboBox{nullptr};
  qMRMLSegmentsTableView* SegmentsTableView{nullptr};
  qSlicerAstroFoldSection* AdvancedSection{nullptr};
  qSlicerAstroValueField* IntensityMaxField{nullptr};
  qMRMLNodeComboBox* ZeroMomentVolumeNodeSelector{nullptr};
  qMRMLNodeComboBox* FirstMomentVolumeNodeSelector{nullptr};
  qMRMLNodeComboBox* SecondMomentVolumeNodeSelector{nullptr};
  qMRMLNodeComboBox* ParametersNodeComboBox{nullptr};
  qSlicerAstroRunButton* RunButton{nullptr};
  qSlicerAstroResultCard* ZeroMomentResult{nullptr};
  qSlicerAstroResultCard* FirstMomentResult{nullptr};
  qSlicerAstroResultCard* SecondMomentResult{nullptr};

  vtkSmartPointer<vtkMRMLAstroMomentMapsParametersNode> parametersNode;
  // input volume observed (Modified) and input volume of the selected ranges
  vtkWeakPointer<vtkMRMLNode> observedInputVolume;
  vtkWeakPointer<vtkMRMLNode> rangesInputVolume;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
  vtkSmartPointer<vtkMRMLUnitNode> unitNodeIntensity;
  vtkSmartPointer<vtkMRMLUnitNode> unitNodeVelocity;
  /// The user cancelled the current computation
  bool CancelRequested{false};
  /// Velocity (km/s) of each channel of the input
  QVector<double> channelVelocities;
};

//-----------------------------------------------------------------------------
// qSlicerAstroMomentMapsModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModuleWidgetPrivate::qSlicerAstroMomentMapsModuleWidgetPrivate(qSlicerAstroMomentMapsModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModuleWidgetPrivate::~qSlicerAstroMomentMapsModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroMomentMapsModuleWidget);

  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(qSlicerAstroMomentMapsModuleWidget::tr("Moment maps"));
  this->Header->setIconName("module-moment-maps");
  rootLayout->addWidget(this->Header);

  QWidget* body = new QWidget(q);
  QVBoxLayout* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(12, 12, 12, 14);
  bodyLayout->setSpacing(14);
  rootLayout->addWidget(body);

  // Input data
  this->InputVolumeNodeSelector = new qSlicerAstroDatasetSelector(body);
  this->InputVolumeNodeSelector->setObjectName("InputVolumeNodeSelector");
  this->InputVolumeNodeSelector->setRequiredDimensions(3);
  this->InputVolumeNodeSelector->setNoneText(qSlicerAstroMomentMapsModuleWidget::tr("Select a data cube"));
  this->InputVolumeNodeSelector->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "Data cube of the moment maps (also the active volume of the views)."));
  bodyLayout->addWidget(this->InputVolumeNodeSelector);

  this->Message = new qSlicerAstroMessageWidget(body);
  this->Message->setObjectName("MessageWidget");
  bodyLayout->addWidget(this->Message);

  // Maps
  QWidget* maps = new QWidget(body);
  QVBoxLayout* mapsLayout = new QVBoxLayout(maps);
  mapsLayout->setContentsMargins(0, 0, 0, 0);
  mapsLayout->setSpacing(8);
  mapsLayout->addWidget(Overline(qSlicerAstroMomentMapsModuleWidget::tr("Maps"), maps));
  QGridLayout* togglesLayout = new QGridLayout();
  togglesLayout->setSpacing(6);
  this->ZeroMomentToggle = new qSlicerAstroToggleCard("0", qSlicerAstroMomentMapsModuleWidget::tr("flux"), maps);
  this->ZeroMomentToggle->setObjectName("ZeroMomentToggle");
  this->ZeroMomentToggle->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "Moment 0: integrated intensity (sum over the channels times the channel width)."));
  this->FirstMomentToggle = new qSlicerAstroToggleCard("1", qSlicerAstroMomentMapsModuleWidget::tr("velocity"), maps);
  this->FirstMomentToggle->setObjectName("FirstMomentToggle");
  this->FirstMomentToggle->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "Moment 1: intensity-weighted mean velocity (velocity field)."));
  this->SecondMomentToggle = new qSlicerAstroToggleCard("2", qSlicerAstroMomentMapsModuleWidget::tr("dispersion"), maps);
  this->SecondMomentToggle->setObjectName("SecondMomentToggle");
  this->SecondMomentToggle->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "Moment 2: intensity-weighted velocity dispersion."));
  togglesLayout->addWidget(this->ZeroMomentToggle, 0, 0);
  togglesLayout->addWidget(this->FirstMomentToggle, 0, 1);
  togglesLayout->addWidget(this->SecondMomentToggle, 0, 2);
  mapsLayout->addLayout(togglesLayout);
  bodyLayout->addWidget(maps);

  // Velocity range over the integrated spectrum
  QWidget* velocity = new QWidget(body);
  QVBoxLayout* velocityLayout = new QVBoxLayout(velocity);
  velocityLayout->setContentsMargins(0, 0, 0, 0);
  velocityLayout->setSpacing(8);
  QHBoxLayout* velocityHeader = new QHBoxLayout();
  velocityHeader->addWidget(Overline(qSlicerAstroMomentMapsModuleWidget::tr("Velocity range"), velocity));
  velocityHeader->addStretch(1);
  this->AutoRangeButton = new QPushButton(qSlicerAstroMomentMapsModuleWidget::tr("Auto from line"), velocity);
  this->AutoRangeButton->setObjectName("AutoRangeButton");
  this->AutoRangeButton->setProperty("astroRole", "ghost");
  this->AutoRangeButton->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "The channels of the line: where the integrated spectrum is above its noise, around the peak."));
  velocityHeader->addWidget(this->AutoRangeButton);
  velocityLayout->addLayout(velocityHeader);
  this->VelocityRangeWidget = new qSlicerAstroSpectrumRangeWidget(velocity);
  this->VelocityRangeWidget->setObjectName("VelocityRangeWidget");
  velocityLayout->addWidget(this->VelocityRangeWidget);
  QGridLayout* velocityFields = new QGridLayout();
  velocityFields->setSpacing(6);
  this->VelocityMinField = new qSlicerAstroValueField(velocity);
  this->VelocityMinField->setObjectName("VelocityMinField");
  this->VelocityMinField->setUnit("km/s");
  this->VelocityMinField->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr("Lower velocity of the range"));
  this->VelocityMaxField = new qSlicerAstroValueField(velocity);
  this->VelocityMaxField->setObjectName("VelocityMaxField");
  this->VelocityMaxField->setUnit("km/s");
  this->VelocityMaxField->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr("Upper velocity of the range"));
  velocityFields->addWidget(this->VelocityMinField, 0, 0);
  velocityFields->addWidget(this->VelocityMaxField, 0, 1);
  velocityLayout->addLayout(velocityFields);
  this->VelocityNote = new QLabel(qSlicerAstroMomentMapsModuleWidget::tr(
    "With a mask, the mask selects the voxels in all the channels."), velocity);
  this->VelocityNote->setProperty("astroRole", "note");
  this->VelocityNote->setWordWrap(true);
  this->VelocityNote->hide();
  velocityLayout->addWidget(this->VelocityNote);
  bodyLayout->addWidget(velocity);

  // Which voxels
  QWidget* voxels = new QWidget(body);
  QVBoxLayout* voxelsLayout = new QVBoxLayout(voxels);
  voxelsLayout->setContentsMargins(0, 0, 0, 0);
  voxelsLayout->setSpacing(8);
  voxelsLayout->addWidget(Overline(qSlicerAstroMomentMapsModuleWidget::tr("Which voxels"), voxels));
  this->VoxelSelectionButton = new qSlicerAstroSegmentedButton(voxels);
  this->VoxelSelectionButton->setObjectName("VoxelSelectionButton");
  this->VoxelSelectionButton->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "Voxels of the maps: above a threshold in the velocity range, or the voxels of a mask."));
  this->VoxelSelectionButton->addButton(qSlicerAstroMomentMapsModuleWidget::tr("Threshold"),
    qSlicerAstroMomentMapsModuleWidget::tr("The voxels above a threshold, in the velocity range."));
  this->VoxelSelectionButton->addButton(qSlicerAstroMomentMapsModuleWidget::tr("Mask"),
    qSlicerAstroMomentMapsModuleWidget::tr("The voxels of a segment of a mask (e.g. a SoFiA source)."));
  voxelsLayout->addWidget(this->VoxelSelectionButton);
  this->VoxelSelectionStack = new QStackedWidget(voxels);
  QWidget* thresholdPage = new QWidget(this->VoxelSelectionStack);
  QVBoxLayout* thresholdLayout = new QVBoxLayout(thresholdPage);
  thresholdLayout->setContentsMargins(0, 0, 0, 0);
  thresholdLayout->setSpacing(4);
  this->ThresholdField = new qSlicerAstroValueField(thresholdPage);
  this->ThresholdField->setObjectName("ThresholdField");
  this->ThresholdField->setLeadingText(qSlicerAstroMomentMapsModuleWidget::tr("above"));
  this->ThresholdField->setUnit(QString::fromUtf8("σ"));
  this->ThresholdField->setDecimals(2);
  this->ThresholdField->setSingleStep(0.5);
  this->ThresholdField->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "Lower threshold of the intensities, in units of the noise (σ) of the data."));
  thresholdLayout->addWidget(this->ThresholdField);
  this->ThresholdInfo = new QLabel(thresholdPage);
  this->ThresholdInfo->setProperty("astroRole", "monoLabel");
  thresholdLayout->addWidget(this->ThresholdInfo);
  this->VoxelSelectionStack->addWidget(thresholdPage);
  QWidget* maskPage = new QWidget(this->VoxelSelectionStack);
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
  this->SegmentationNodeComboBox->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "Mask with the segments. By default the mask of the input data cube."));
  maskLayout->addWidget(this->SegmentationNodeComboBox);
  this->SegmentsTableView = new qMRMLSegmentsTableView(maskPage);
  this->SegmentsTableView->setObjectName("SegmentsTableView");
  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  this->SegmentsTableView->setMinimumHeight(120);
  this->SegmentsTableView->setMaximumHeight(170);
  this->SegmentsTableView->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr("The selected segment is the mask."));
  maskLayout->addWidget(this->SegmentsTableView);
  this->VoxelSelectionStack->addWidget(maskPage);
  voxelsLayout->addWidget(this->VoxelSelectionStack);
  bodyLayout->addWidget(voxels);

  // Advanced
  this->AdvancedSection = new qSlicerAstroFoldSection(body);
  this->AdvancedSection->setObjectName("AdvancedSection");
  this->AdvancedSection->setSummary(qSlicerAstroMomentMapsModuleWidget::tr("upper limit, output maps"));
  this->AdvancedSection->setSettingsKey("SlicerAstro/AstroMomentMaps/AdvancedCollapsed");
  QFormLayout* advancedLayout = new QFormLayout();
  advancedLayout->setContentsMargins(0, 0, 0, 0);
  advancedLayout->setSpacing(8);
  advancedLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  this->IntensityMaxField = new qSlicerAstroValueField(this->AdvancedSection->contentWidget());
  this->IntensityMaxField->setObjectName("IntensityMaxField");
  this->IntensityMaxField->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "Upper limit of the intensities (by default the maximum of the data)."));
  QWidget* advancedParent = this->AdvancedSection->contentWidget();
  std::function<void(const QString&, QWidget*)> addRow =
    [advancedLayout, advancedParent](const QString& text, QWidget* field)
    {
    QLabel* label = new QLabel(text, advancedParent);
    label->setProperty("astroRole", "label");
    // the labels keep their text, the fields get narrower
    label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    qMRMLNodeComboBox* selector = qobject_cast<qMRMLNodeComboBox*>(field);
    if (selector)
      {
      selector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
      selector->setMinimumWidth(80);
      }
    advancedLayout->addRow(label, field);
    };
  addRow(qSlicerAstroMomentMapsModuleWidget::tr("Upper limit"), this->IntensityMaxField);
  qMRMLNodeComboBox** outputs[3] =
    {&this->ZeroMomentVolumeNodeSelector, &this->FirstMomentVolumeNodeSelector, &this->SecondMomentVolumeNodeSelector};
  const char* outputNames[3] =
    {"ZeroMomentVolumeNodeSelector", "FirstMomentVolumeNodeSelector", "SecondMomentVolumeNodeSelector"};
  const QString outputLabels[3] =
    {qSlicerAstroMomentMapsModuleWidget::tr("Moment 0 map"), qSlicerAstroMomentMapsModuleWidget::tr("Moment 1 map"),
     qSlicerAstroMomentMapsModuleWidget::tr("Moment 2 map")};
  for (int i = 0; i < 3; i++)
    {
    qMRMLNodeComboBox* selector = new qMRMLNodeComboBox(advancedParent);
    selector->setObjectName(outputNames[i]);
    selector->setNodeTypes(QStringList() << "vtkMRMLAstroVolumeNode");
    selector->setNoneEnabled(true);
    selector->setAddEnabled(false);
    selector->setEditEnabled(false);
    selector->setRenameEnabled(true);
    selector->setSelectNodeUponCreation(false);
    selector->setNoneDisplay(qSlicerAstroMomentMapsModuleWidget::tr("New map"));
    selector->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
      "Map replaced by the next computation (a new map if None)."));
    *outputs[i] = selector;
    addRow(outputLabels[i], selector);
    }
  this->ParametersNodeComboBox = new qMRMLNodeComboBox(advancedParent);
  this->ParametersNodeComboBox->setObjectName("ParametersNodeComboBox");
  this->ParametersNodeComboBox->setNodeTypes(QStringList() << "vtkMRMLAstroMomentMapsParametersNode");
  this->ParametersNodeComboBox->setShowHidden(true);
  this->ParametersNodeComboBox->setAddEnabled(false);
  this->ParametersNodeComboBox->setEditEnabled(false);
  this->ParametersNodeComboBox->setRenameEnabled(true);
  addRow(qSlicerAstroMomentMapsModuleWidget::tr("Parameter set"), this->ParametersNodeComboBox);
  this->AdvancedSection->contentLayout()->addLayout(advancedLayout);
  bodyLayout->addWidget(this->AdvancedSection);

  // Footer: run and results
  QFrame* footer = new QFrame(q);
  footer->setProperty("astroRole", "panelFooter");
  QVBoxLayout* footerLayout = new QVBoxLayout(footer);
  footerLayout->setContentsMargins(12, 12, 12, 12);
  footerLayout->setSpacing(8);
  this->RunButton = new qSlicerAstroRunButton(footer);
  this->RunButton->setObjectName("RunButton");
  this->RunButton->pushButton()->setObjectName("ApplyButton");
  this->RunButton->pushButton()->setToolTip(qSlicerAstroMomentMapsModuleWidget::tr(
    "Compute the selected moment maps (new maps, or the maps of the previous computation replaced)."));
  footerLayout->addWidget(this->RunButton);
  qSlicerAstroResultCard** results[3] = {&this->ZeroMomentResult, &this->FirstMomentResult, &this->SecondMomentResult};
  const char* resultNames[3] = {"ZeroMomentResult", "FirstMomentResult", "SecondMomentResult"};
  for (int i = 0; i < 3; i++)
    {
    qSlicerAstroResultCard* card = new qSlicerAstroResultCard(footer);
    card->setObjectName(resultNames[i]);
    card->hide();
    *results[i] = card;
    footerLayout->addWidget(card);
    }
  rootLayout->addWidget(footer);
  rootLayout->addStretch(1);

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroMomentMapsParametersNode(vtkMRMLNode*)));
  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));
  QObject::connect(this->ZeroMomentVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onZeroMomentVolumeChanged(vtkMRMLNode*)));
  QObject::connect(this->FirstMomentVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onFirstMomentVolumeChanged(vtkMRMLNode*)));
  QObject::connect(this->SecondMomentVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onSecondMomentVolumeChanged(vtkMRMLNode*)));
  for (QObject* selector : std::vector<QObject*>{this->InputVolumeNodeSelector, this->ParametersNodeComboBox,
         this->ZeroMomentVolumeNodeSelector, this->FirstMomentVolumeNodeSelector,
         this->SecondMomentVolumeNodeSelector, this->SegmentationNodeComboBox, this->SegmentsTableView})
    {
    QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), selector, SLOT(setMRMLScene(vtkMRMLScene*)));
    }
  QObject::connect(this->ZeroMomentToggle, SIGNAL(toggled(bool)), q, SLOT(onGenerateZeroToggled(bool)));
  QObject::connect(this->FirstMomentToggle, SIGNAL(toggled(bool)), q, SLOT(onGenerateFirstToggled(bool)));
  QObject::connect(this->SecondMomentToggle, SIGNAL(toggled(bool)), q, SLOT(onGenerateSecondToggled(bool)));
  QObject::connect(this->VoxelSelectionButton, SIGNAL(currentIndexChanged(int)), q, SLOT(onVoxelSelectionChanged(int)));
  QObject::connect(this->ThresholdField, SIGNAL(valueChanged(double)), q, SLOT(onThresholdChanged(double)));
  QObject::connect(this->IntensityMaxField, SIGNAL(valueChanged(double)), q, SLOT(onIntensityMaxChanged(double)));
  QObject::connect(this->VelocityRangeWidget, SIGNAL(rangeChanged(double,double)),
                   q, SLOT(onVelocityRangeChanged(double,double)));
  QObject::connect(this->VelocityRangeWidget, &qSlicerAstroSpectrumRangeWidget::rangeChanging,
                   q, [this](double minimum, double maximum)
    {
    // the fields follow the drag
    bool wasBlocked = this->VelocityMinField->blockSignals(true);
    this->VelocityMinField->setValue(minimum);
    this->VelocityMinField->blockSignals(wasBlocked);
    wasBlocked = this->VelocityMaxField->blockSignals(true);
    this->VelocityMaxField->setValue(maximum);
    this->VelocityMaxField->blockSignals(wasBlocked);
    });
  QObject::connect(this->VelocityMinField, SIGNAL(valueChanged(double)), q, SLOT(onVelocityFieldsChanged()));
  QObject::connect(this->VelocityMaxField, SIGNAL(valueChanged(double)), q, SLOT(onVelocityFieldsChanged()));
  QObject::connect(this->AutoRangeButton, SIGNAL(clicked()), q, SLOT(onAutoVelocityRange()));
  QObject::connect(this->RunButton, SIGNAL(clicked()), q, SLOT(onCalculate()));
  QObject::connect(this->RunButton, SIGNAL(cancelRequested()), q, SLOT(onComputationCancelled()));
  QObject::connect(this->ZeroMomentResult, SIGNAL(showRequested()), q, SLOT(onShowZeroMoment()));
  QObject::connect(this->FirstMomentResult, SIGNAL(showRequested()), q, SLOT(onShowFirstMoment()));
  QObject::connect(this->SecondMomentResult, SIGNAL(showRequested()), q, SLOT(onShowSecondMoment()));
  QObject::connect(this->InputVolumeNodeSelector, &qSlicerAstroDatasetSelector::currentNodeChanged,
                   q, [this](vtkMRMLNode*) { this->updateRunButton(); });

  qSlicerAstroTheme::instance()->applyTo(q);
  this->updateRunButton();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroMomentMapsModuleWidget);

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
}

//-----------------------------------------------------------------------------
vtkSlicerAstroMomentMapsLogic* qSlicerAstroMomentMapsModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroMomentMapsModuleWidget);
  return vtkSlicerAstroMomentMapsLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroMomentMapsModuleWidgetPrivate::inputVolume() const
{
  Q_Q(const qSlicerAstroMomentMapsModuleWidget);
  if (!this->parametersNode || !q->mrmlScene() || !this->parametersNode->GetInputVolumeNodeID())
    {
    return nullptr;
    }
  return vtkMRMLAstroVolumeNode::SafeDownCast(
    q->mrmlScene()->GetNodeByID(this->parametersNode->GetInputVolumeNodeID()));
}

//-----------------------------------------------------------------------------
double qSlicerAstroMomentMapsModuleWidgetPrivate::noise() const
{
  vtkMRMLAstroVolumeNode* input = this->inputVolume();
  if (!input)
    {
    return 0.;
    }
  const double noise = input->GetDisplayThreshold();
  return (std::isfinite(noise) && noise > 0.) ? noise : 0.;
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidgetPrivate::updateRunButton()
{
  int maps = 0;
  for (qSlicerAstroToggleCard* toggle : {this->ZeroMomentToggle, this->FirstMomentToggle, this->SecondMomentToggle})
    {
    maps += toggle->isChecked() ? 1 : 0;
    }
  const bool input = this->InputVolumeNodeSelector->currentNode() != nullptr;
  if (!input)
    {
    this->RunButton->setText(qSlicerAstroMomentMapsModuleWidget::tr("Select a data cube"));
    }
  else if (maps == 0)
    {
    this->RunButton->setText(qSlicerAstroMomentMapsModuleWidget::tr("Select a map"));
    }
  else
    {
    this->RunButton->setText(maps == 1 ? qSlicerAstroMomentMapsModuleWidget::tr("Compute 1 map") :
                             qSlicerAstroMomentMapsModuleWidget::tr("Compute %1 maps").arg(maps));
    }
  this->RunButton->setEnabled(input && maps > 0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidgetPrivate::updateResults()
{
  Q_Q(qSlicerAstroMomentMapsModuleWidget);
  if (!this->parametersNode || !q->mrmlScene())
    {
    return;
    }
  const char* ids[3] = {this->parametersNode->GetZeroMomentVolumeNodeID(),
                        this->parametersNode->GetFirstMomentVolumeNodeID(),
                        this->parametersNode->GetSecondMomentVolumeNodeID()};
  qSlicerAstroResultCard* cards[3] = {this->ZeroMomentResult, this->FirstMomentResult, this->SecondMomentResult};
  vtkMRMLAstroVolumeNode* input = this->inputVolume();
  for (int i = 0; i < 3; i++)
    {
    vtkMRMLAstroVolumeNode* map = ids[i] ?
      vtkMRMLAstroVolumeNode::SafeDownCast(q->mrmlScene()->GetNodeByID(ids[i])) : nullptr;
    // only the maps of the current input (with data)
    const bool show = map && map != input && map->GetImageData() &&
                      StringToInt(map->GetAttribute("SlicerAstro.NAXIS")) == 2;
    if (show)
      {
      cards[i]->setVolume(map);
      }
    cards[i]->setVisible(show);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidgetPrivate::showMap(const char* volumeID, const char* sliceNodeID,
                                                       const char* compositeNodeID)
{
  Q_Q(qSlicerAstroMomentMapsModuleWidget);
  vtkMRMLScene* scene = q->mrmlScene();
  qSlicerApplication* app = qSlicerApplication::application();
  if (!scene || !volumeID || !app || !app->applicationLogic())
    {
    return;
    }
  vtkMRMLSliceCompositeNode* composite = vtkMRMLSliceCompositeNode::SafeDownCast(scene->GetNodeByID(compositeNodeID));
  vtkMRMLSliceNode* slice = vtkMRMLSliceNode::SafeDownCast(scene->GetNodeByID(sliceNodeID));
  if (!composite || !slice)
    {
    return;
    }
  composite->SetLabelVolumeID("");
  composite->SetForegroundVolumeID("");
  composite->SetForegroundOpacity(0.);
  composite->SetBackgroundVolumeID(volumeID);
  // setting to the XZ orientation is needed in order to force the refresh
  slice->SetOrientation("XZ");
  slice->SetOrientation("XY");
  slice->SetSliceOffset(0.);
  vtkMRMLSliceLogic* sliceLogic = app->applicationLogic()->GetSliceLogic(slice);
  if (sliceLogic)
    {
    int* dims = slice->GetDimensions();
    if (dims)
      {
      sliceLogic->FitSliceToAll(dims[0], dims[1]);
      }
    sliceLogic->SnapSliceOffsetToIJK();
    }
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroMomentMapsModuleWidgetPrivate::createMapVolume(
  vtkMRMLAstroVolumeNode* input, const char* existingID, const char* suffix, const std::string& unit,
  const char* colorTableName, const char* datamodel, int serial)
{
  Q_Q(qSlicerAstroMomentMapsModuleWidget);
  vtkMRMLScene* scene = q->mrmlScene();
  std::ostringstream outSS;
  outSS << input->GetName() << "_" << suffix << "_" << serial;

  // the previous map of the same kind is replaced (never the input)
  vtkMRMLAstroVolumeNode* existing = existingID ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(existingID)) : nullptr;
  if (existing && existing != input &&
      std::string(existing->GetName()).find(std::string("_") + suffix + "_") != std::string::npos)
    {
    scene->RemoveNode(existing->GetStorageNode());
    scene->RemoveNode(existing->GetAstroVolumeDisplayNode());
    vtkMRMLVolumeRenderingDisplayNode* volumeRenderingDisplay =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(existing->GetDisplayNode());
    if (volumeRenderingDisplay)
      {
      scene->RemoveNode(volumeRenderingDisplay->GetROINode());
      }
    scene->RemoveNode(existing);
    }

  return this->logic()->CreateMomentMapVolume(input, outSS.str().c_str(), unit.c_str(), colorTableName, datamodel);
}

//-----------------------------------------------------------------------------
// qSlicerAstroMomentMapsModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModuleWidget::qSlicerAstroMomentMapsModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroMomentMapsModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->init();

  QObject::connect(d->SegmentationNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this, SLOT(onMaskSegmentationChanged(vtkMRMLNode*)));
}

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModuleWidget::~qSlicerAstroMomentMapsModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::enter()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  this->Superclass::enter();

  this->updateMaskSelector();
  this->onMaskSegmentationChanged(d->SegmentationNodeComboBox->currentNode());
}

//----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::exit()
{
  this->Superclass::exit();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::setMRMLScene : selectionNode not found!";
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

  d->unitNodeIntensity = d->selectionNode->GetUnitNode("intensity");
  this->qvtkReconnect(d->unitNodeIntensity, vtkCommand::ModifiedEvent,
                      this, SLOT(onUnitNodeIntensityChanged(vtkObject*)));
  this->onUnitNodeIntensityChanged(d->unitNodeIntensity);

  d->unitNodeVelocity = d->selectionNode->GetUnitNode("velocity");
  this->qvtkReconnect(d->unitNodeVelocity, vtkCommand::ModifiedEvent,
                      this, SLOT(onUnitNodeVelocityChanged(vtkObject*)));
  this->onUnitNodeVelocityChanged(d->unitNodeVelocity);

  // masks added, removed or modified (e.g. the contours)
  this->qvtkReconnect(scene, vtkMRMLScene::NodeAddedEvent, this, SLOT(updateMaskSelector()));
  this->qvtkReconnect(scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(updateMaskSelector()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(updateMaskSelector()));
  this->updateMaskSelector();

  // the parameters (and so the input) may have been selected when the
  // selectors received the scene, before the selection node was known (e.g.
  // a second panel, embedded in the Workbench): the input is set up now
  if (d->InputVolumeNodeSelector->currentNode())
    {
    this->onInputVolumeChanged(d->InputVolumeNodeSelector->currentNode());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::initializeNodes(bool forceNew)
{
  this->initializeParameterNode(forceNew);

  this->initializeSegmentations(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onEndCloseEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes(true);
  this->onMRMLAstroMomentMapsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onEndImportEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroMomentMapsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroMomentMapsParametersNode *astroParametersNode = nullptr;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroMomentMapsParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroMomentMapsParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroMomentMapsParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroMomentMapsParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroMomentMapsParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->SetMaskActive(false);
    astroParametersNode->SetGenerateZero(true);
    astroParametersNode->SetGenerateFirst(true);
    astroParametersNode->SetGenerateSecond(true);
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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
bool qSlicerAstroMomentMapsModuleWidget::setMaskSegmentFromSelection()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(
    d->SegmentationNodeComboBox->currentNode());
  if (!segmentationNode && d->segmentEditorNode)
    {
    segmentationNode = d->segmentEditorNode->GetSegmentationNode();
    }
  if (!d->parametersNode || !segmentationNode || !segmentationNode->GetSegmentation())
    {
    d->Message->showMessage("error", tr("No mask: select a segmentation with the segment of the mask."));
    return false;
    }

  QStringList selectedSegmentIDs = d->SegmentsTableView->selectedSegmentIDs();
  if (selectedSegmentIDs.size() < 1)
    {
    d->Message->showMessage("error", tr("Select the segment of the mask in the table."));
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
void qSlicerAstroMomentMapsModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkMRMLSelectionNode *selectionNode = vtkMRMLSelectionNode::SafeDownCast(sender);
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
void qSlicerAstroMomentMapsModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  this->onMRMLSelectionNodeReferenceAdded(sender);
}

//--------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onSecondMomentVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetSecondMomentVolumeNodeID(mrmlNode ? mrmlNode->GetID() : nullptr);
}

//--------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::setMRMLAstroMomentMapsParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroMomentMapsParametersNode* AstroMomentMapsParaNode =
      vtkMRMLAstroMomentMapsParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroMomentMapsParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroMomentMapsParametersNodeModified()));

  d->parametersNode = AstroMomentMapsParaNode;

  this->onMRMLAstroMomentMapsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode || !d->selectionNode)
    {
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    return;
    }

  d->Message->clearMessage();
  if (mrmlNode)
    {
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    d->parametersNode->SetInputVolumeNodeID(mrmlNode->GetID());

    // only the current input data are observed
    this->qvtkReconnect(d->observedInputVolume, mrmlNode, vtkCommand::ModifiedEvent,
                        this, SLOT(onInputVolumeModified()));
    d->observedInputVolume = mrmlNode;

    this->onInputVolumeModified();
    }
  else
    {
    d->selectionNode->SetActiveVolumeID(nullptr);
    d->VelocityRangeWidget->clearSpectrum();
    d->channelVelocities.clear();
    }

  this->updateMaskSelector();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onInputVolumeModified()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode || !this->mrmlScene() ||
     this->mrmlScene()->IsClosing() || this->mrmlScene()->IsBatchProcessing() ||
     this->mrmlScene()->IsImporting())
    {
    return;
    }

  vtkMRMLAstroVolumeNode* astroMrmlNode = d->inputVolume();
  if (!astroMrmlNode)
    {
    return;
    }
  vtkMRMLAstroVolumeDisplayNode * astroMrmlDisplayNode = astroMrmlNode->GetAstroVolumeDisplayNode();
  if (!astroMrmlDisplayNode)
    {
    return;
    }
  struct wcsprm* WCS = astroMrmlDisplayNode->GetWCSStruct();
  if (!WCS)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onInputVolumeModified :"
                   " WCS not found!";
    return;
    }
  const double velocityFactor = !strcmp(WCS->cunit[2], "m/s") ? 0.001 : 1.;

  // velocity of each channel (at the centre of the field) and the limits
  // of the data (edges of the first and last channels)
  const int channels = StringToInt(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS3"));
  double ijk[3], world[3];
  ijk[0] = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS1")) * 0.5;
  ijk[1] = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS2")) * 0.5;
  d->channelVelocities.resize(std::max(channels, 0));
  for (int k = 0; k < channels; k++)
    {
    ijk[2] = k;
    astroMrmlDisplayNode->GetReferenceSpace(ijk, world);
    d->channelVelocities[k] = world[2] * velocityFactor;
    }
  ijk[2] = 0.;
  double worldOne[3], worldTwo[3];
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldOne);
  ijk[2] = channels < 2 ? channels + 1 : channels;
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldTwo);
  const double Vmin = std::min(worldOne[2], worldTwo[2]) * velocityFactor;
  const double Vmax = std::max(worldOne[2], worldTwo[2]) * velocityFactor;

  d->VelocityRangeWidget->setSpectrum(d->channelVelocities,
                                      qSlicerAstroThumbnails::instance()->integratedSpectrum(astroMrmlNode));
  for (qSlicerAstroValueField* field : {d->VelocityMinField, d->VelocityMaxField})
    {
    bool wasBlocked = field->blockSignals(true);
    field->setRange(Vmin, Vmax);
    field->setSingleStep((Vmax - Vmin) / 200.);
    field->blockSignals(wasBlocked);
    }

  double min = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.DATAMIN"));
  double max = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.DATAMAX"));
  bool wasBlocked = d->IntensityMaxField->blockSignals(true);
  d->IntensityMaxField->setRange(min, max);
  d->IntensityMaxField->setSingleStep((max - min) / 200.);
  d->IntensityMaxField->setUnit(QString::fromUtf8(astroMrmlNode->GetAttribute("SlicerAstro.BUNIT")));
  d->IntensityMaxField->blockSignals(wasBlocked);

  // the limits of the data; the selected ranges are reset only for new input
  // data (not at every modification of the volume)
  const bool newInput = d->rangesInputVolume.GetPointer() != astroMrmlNode;
  d->rangesInputVolume = astroMrmlNode;
  if (newInput)
    {
    // default threshold: 3 times the noise
    const double noise = d->noise();
    int wasModifying = d->parametersNode->StartModify();
    d->parametersNode->SetVelocityMin(Vmin);
    d->parametersNode->SetVelocityMax(Vmax);
    d->parametersNode->SetIntensityMin(noise > 0. ? 3. * noise : min);
    d->parametersNode->SetIntensityMax(max);
    d->parametersNode->EndModify(wasModifying);
    }

  // the dataset line: noise and channel width
  const QString unit = QString::fromUtf8(astroMrmlNode->GetAttribute("SlicerAstro.BUNIT"));
  const double noise = d->noise();
  QStringList summary;
  if (noise > 0.)
    {
    summary << QString::fromUtf8("σ %1").arg(FormatWithUnit(noise, unit));
    }
  if (d->channelVelocities.size() > 1)
    {
    summary << QString("%1 km/s").arg(std::abs(d->channelVelocities[1] - d->channelVelocities[0]), 0, 'g', 3);
    }
  summary << tr("%1 channels").arg(channels);
  d->InputVolumeNodeSelector->setSubtitle(summary.join(QString::fromUtf8(" · ")));
  this->onMRMLAstroMomentMapsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onFirstMomentVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetFirstMomentVolumeNodeID(mrmlNode ? mrmlNode->GetID() : nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onGenerateFirstToggled(bool generate)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  d->updateRunButton();
  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetGenerateFirst(generate);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onGenerateSecondToggled(bool generate)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  d->updateRunButton();
  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetGenerateSecond(generate);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onGenerateZeroToggled(bool generate)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  d->updateRunButton();
  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetGenerateZero(generate);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onMaskActiveToggled(bool active)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetMaskActive(active);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onVoxelSelectionChanged(int index)
{
  this->onMaskActiveToggled(index == 1);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onThresholdChanged(double threshold)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  const double noise = d->noise();
  d->parametersNode->SetIntensityMin(noise > 0. ? threshold * noise : threshold);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onIntensityMaxChanged(double maximum)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetIntensityMax(maximum);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onMRMLAstroMomentMapsParametersNodeModified()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolumeNode = d->inputVolume();
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  const char* outputIDs[3] = {d->parametersNode->GetZeroMomentVolumeNodeID(),
                              d->parametersNode->GetFirstMomentVolumeNodeID(),
                              d->parametersNode->GetSecondMomentVolumeNodeID()};
  qMRMLNodeComboBox* outputs[3] = {d->ZeroMomentVolumeNodeSelector, d->FirstMomentVolumeNodeSelector,
                                   d->SecondMomentVolumeNodeSelector};
  for (int i = 0; i < 3; i++)
    {
    outputs[i]->setCurrentNode(outputIDs[i] ? this->mrmlScene()->GetNodeByID(outputIDs[i]) : nullptr);
    }

  const bool maskActive = d->parametersNode->GetMaskActive();
  d->VoxelSelectionButton->setCurrentIndex(maskActive ? 1 : 0);
  // the stack has the height of the page shown (not of the tallest page)
  d->VoxelSelectionStack->setCurrentIndex(maskActive ? 1 : 0);
  for (int page = 0; page < d->VoxelSelectionStack->count(); page++)
    {
    d->VoxelSelectionStack->widget(page)->setSizePolicy(QSizePolicy::Preferred,
      page == d->VoxelSelectionStack->currentIndex() ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    }
  d->VoxelSelectionStack->adjustSize();
  d->SegmentsTableView->setEnabled(maskActive);
  // with a mask the logic uses all the channels
  d->VelocityRangeWidget->setEnabled(!maskActive);
  d->VelocityMinField->setEnabled(!maskActive);
  d->VelocityMaxField->setEnabled(!maskActive);
  d->AutoRangeButton->setEnabled(!maskActive);
  d->VelocityNote->setVisible(maskActive);

  for (qSlicerAstroToggleCard* toggle : {d->ZeroMomentToggle, d->FirstMomentToggle, d->SecondMomentToggle})
    {
    toggle->blockSignals(true);
    }
  d->ZeroMomentToggle->setChecked(d->parametersNode->GetGenerateZero());
  d->FirstMomentToggle->setChecked(d->parametersNode->GetGenerateFirst());
  d->SecondMomentToggle->setChecked(d->parametersNode->GetGenerateSecond());
  for (qSlicerAstroToggleCard* toggle : {d->ZeroMomentToggle, d->FirstMomentToggle, d->SecondMomentToggle})
    {
    toggle->blockSignals(false);
    }
  d->updateRunButton();

  d->VelocityRangeWidget->setRange(d->parametersNode->GetVelocityMin(), d->parametersNode->GetVelocityMax());
  bool wasBlocked = d->VelocityMinField->blockSignals(true);
  d->VelocityMinField->setValue(d->parametersNode->GetVelocityMin());
  d->VelocityMinField->blockSignals(wasBlocked);
  wasBlocked = d->VelocityMaxField->blockSignals(true);
  d->VelocityMaxField->setValue(d->parametersNode->GetVelocityMax());
  d->VelocityMaxField->blockSignals(wasBlocked);

  // threshold in units of the noise (in the units of the data without noise)
  const double noise = d->noise();
  const QString unit = inputVolumeNode ?
    QString::fromUtf8(inputVolumeNode->GetAttribute("SlicerAstro.BUNIT")) : QString();
  wasBlocked = d->ThresholdField->blockSignals(true);
  d->ThresholdField->setUnit(noise > 0. ? QString::fromUtf8("σ") : unit);
  d->ThresholdField->setValue(noise > 0. ? d->parametersNode->GetIntensityMin() / noise :
                              d->parametersNode->GetIntensityMin());
  d->ThresholdField->blockSignals(wasBlocked);
  d->ThresholdInfo->setText(noise > 0. ?
    QString::fromUtf8("= %1 · σ = %2").arg(FormatWithUnit(d->parametersNode->GetIntensityMin(), unit))
                                      .arg(FormatWithUnit(noise, unit)) : QString());
  wasBlocked = d->IntensityMaxField->blockSignals(true);
  d->IntensityMaxField->setValue(d->parametersNode->GetIntensityMax());
  d->IntensityMaxField->blockSignals(wasBlocked);

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
void qSlicerAstroMomentMapsModuleWidget::onMRMLSelectionNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkMRMLSelectionNode *selectionNode = vtkMRMLSelectionNode::SafeDownCast(sender);
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
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onCalculate()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onCalculate :"
                   " parametersNode not found!";
    return;
    }

  vtkSlicerAstroMomentMapsLogic *logic = d->logic();
  vtkMRMLScene *scene = this->mrmlScene();
  if (!logic || !scene)
    {
    qCritical() <<"qSlicerAstroMomentMapsModuleWidget::onCalculate :"
                  " logic or scene not found!";
    return;
    }

  d->Message->clearMessage();
  vtkMRMLAstroVolumeNode *inputVolume = d->inputVolume();
  if(!inputVolume || !inputVolume->GetImageData())
    {
    d->Message->showMessage("error", tr("Select the data cube of the moment maps."));
    return;
    }

  // Check Input volume
  if (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS")) != 3)
    {
    d->Message->showMessage("error", tr("Moment maps need a data cube (3 axes: two on the sky and the "
                                        "spectral axis)."));
    return;
    }

  if (d->parametersNode->GetMaskActive() && !this->setMaskSegmentFromSelection())
    {
    return;
    }

  d->CancelRequested = false;
  d->parametersNode->SetStatus(1);
  const int serial = d->parametersNode->GetOutputSerial();
  const bool generateZero = d->parametersNode->GetGenerateZero();
  const bool generateFirst = d->parametersNode->GetGenerateFirst();
  const bool generateSecond = d->parametersNode->GetGenerateSecond();

  // moment 1 needs moment 0, moment 2 needs moment 1
  vtkMRMLAstroVolumeNode* zeroMomentVolume = nullptr;
  vtkMRMLAstroVolumeNode* firstMomentVolume = nullptr;
  vtkMRMLAstroVolumeNode* secondMomentVolume = nullptr;
  if (generateZero || generateFirst || generateSecond)
    {
    std::string unit = inputVolume->GetAttribute("SlicerAstro.BUNIT") ? inputVolume->GetAttribute("SlicerAstro.BUNIT") : "";
    unit += " km/s";
    zeroMomentVolume = d->createMapVolume(inputVolume, d->parametersNode->GetZeroMomentVolumeNodeID(),
                                          "mom0th", unit, nullptr, "ZEROMOMENTMAP", serial);
    d->parametersNode->SetZeroMomentVolumeNodeID(zeroMomentVolume ? zeroMomentVolume->GetID() : nullptr);
    }
  if (generateFirst || generateSecond)
    {
    firstMomentVolume = d->createMapVolume(inputVolume, d->parametersNode->GetFirstMomentVolumeNodeID(),
                                           "mom1st", "km/s", "VelocityField", "FIRSTMOMENTMAP", serial);
    d->parametersNode->SetFirstMomentVolumeNodeID(firstMomentVolume ? firstMomentVolume->GetID() : nullptr);
    }
  if (generateSecond)
    {
    secondMomentVolume = d->createMapVolume(inputVolume, d->parametersNode->GetSecondMomentVolumeNodeID(),
                                            "mom2nd", "km/s", "Rainbow", "SECONDMOMENTMAP", serial);
    d->parametersNode->SetSecondMomentVolumeNodeID(secondMomentVolume ? secondMomentVolume->GetID() : nullptr);
    }

  d->parametersNode->SetOutputSerial(serial + 1);

  // Run computation
  if (!logic->CalculateMomentMaps(d->parametersNode))
    {
    const bool cancelled = d->CancelRequested;
    for (vtkMRMLAstroVolumeNode* map : {zeroMomentVolume, firstMomentVolume, secondMomentVolume})
      {
      if (map)
        {
        scene->RemoveNode(map);
        }
      }
    d->parametersNode->SetStatus(0);
    if (cancelled)
      {
      d->Message->showMessage("info", tr("Computation cancelled."));
      }
    else
      {
      d->Message->showMessage("error", tr("The moment maps could not be computed (see the error log)."));
      }
    return;
    }

  // maps computed only as an intermediate step are removed
  if (!generateZero && zeroMomentVolume)
    {
    scene->RemoveNode(zeroMomentVolume);
    d->parametersNode->SetZeroMomentVolumeNodeID(nullptr);
    }
  if (!generateFirst && firstMomentVolume)
    {
    scene->RemoveNode(firstMomentVolume);
    d->parametersNode->SetFirstMomentVolumeNodeID(nullptr);
    }

  // the maps in the views (the module panel; the Workbench, which embeds
  // the panel, arranges its own views and lists the maps as products)
  const bool modulePanel = !this->property("astroEmbedded").toBool();
  qSlicerApplication* app = qSlicerApplication::application();
  if (modulePanel && app && app->layoutManager())
    {
    app->layoutManager()->layoutLogic()->GetLayoutNode()->SetViewArrangement(2);
    }
  if (modulePanel && generateZero)
    {
    d->showMap(d->parametersNode->GetZeroMomentVolumeNodeID(), "vtkMRMLSliceNodeRed", "vtkMRMLSliceCompositeNodeRed");
    }
  if (modulePanel && generateFirst)
    {
    d->showMap(d->parametersNode->GetFirstMomentVolumeNodeID(), "vtkMRMLSliceNodeYellow",
               "vtkMRMLSliceCompositeNodeYellow");
    }
  if (modulePanel && generateSecond)
    {
    d->showMap(d->parametersNode->GetSecondMomentVolumeNodeID(), "vtkMRMLSliceNodeGreen",
               "vtkMRMLSliceCompositeNodeGreen");
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode ?
    d->segmentEditorNode->GetSegmentationNode() : nullptr;
  if (currentSegmentationNode)
    {
    for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
      {
      vtkMRMLSegmentationDisplayNode *segmentationDisplayNode =
        vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
      if (segmentationDisplayNode)
        {
        segmentationDisplayNode->SetAllSegmentsVisibility(false);
        }
      }
    }

  d->parametersNode->SetStatus(0);
  const int maps = (generateZero ? 1 : 0) + (generateFirst ? 1 : 0) + (generateSecond ? 1 : 0);
  d->Message->showMessage("success", maps == 1 ? tr("1 map created") : tr("%1 maps created").arg(maps));
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->RunButton->setRunning(false);
  d->updateResults();
}

//---------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkMRMLSegmentEditorNode *segmentEditorNode = vtkMRMLSegmentEditorNode::SafeDownCast(sender);
  if (!segmentEditorNode)
    {
    return;
    }

  vtkMRMLSegmentationNode* segmentationNode = segmentEditorNode->GetSegmentationNode();
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
void qSlicerAstroMomentMapsModuleWidget::onMaskSegmentationChanged(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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
void qSlicerAstroMomentMapsModuleWidget::updateMaskSelector()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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
  vtkMRMLVolumeNode* inputVolume = d->inputVolume();
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
void qSlicerAstroMomentMapsModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onUnitNodeIntensityChanged(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkMRMLUnitNode* unitNodeIntensity = vtkMRMLUnitNode::SafeDownCast(sender);
  if (!unitNodeIntensity)
    {
    return;
    }
  d->IntensityMaxField->setDecimals(unitNodeIntensity->GetPrecision());
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onUnitNodeVelocityChanged(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkMRMLUnitNode* unitNodeVelocity = vtkMRMLUnitNode::SafeDownCast(sender);
  if (!unitNodeVelocity)
    {
    return;
    }
  d->VelocityMinField->setDecimals(unitNodeVelocity->GetPrecision());
  d->VelocityMaxField->setDecimals(unitNodeVelocity->GetPrecision());
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onVelocityRangeChanged(double min, double max)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetVelocityMin(min);
  d->parametersNode->SetVelocityMax(max);
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onVelocityFieldsChanged()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  this->onVelocityRangeChanged(std::min(d->VelocityMinField->value(), d->VelocityMaxField->value()),
                               std::max(d->VelocityMinField->value(), d->VelocityMaxField->value()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onAutoVelocityRange()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  const QVector<double> spectrum = d->VelocityRangeWidget->spectrumY();
  const QVector<double> velocities = d->VelocityRangeWidget->spectrumX();
  std::vector<double> values;
  for (double value : spectrum)
    {
    if (std::isfinite(value))
      {
      values.push_back(value);
      }
    }
  if (values.size() < 5 || velocities.size() != spectrum.size())
    {
    d->Message->showMessage("info", tr("The cube has too few channels to find the line."));
    return;
    }
  // baseline and noise of the spectrum: median and median absolute deviation
  std::vector<double> sorted = values;
  std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
  const double baseline = sorted[sorted.size() / 2];
  std::vector<double> deviations;
  for (double value : values)
    {
    deviations.push_back(std::abs(value - baseline));
    }
  std::nth_element(deviations.begin(), deviations.begin() + deviations.size() / 2, deviations.end());
  const double sigma = 1.4826 * deviations[deviations.size() / 2];
  int peak = -1;
  for (int k = 0; k < spectrum.size(); k++)
    {
    if (std::isfinite(spectrum[k]) && (peak < 0 || spectrum[k] > spectrum[peak]))
      {
      peak = k;
      }
    }
  if (peak < 0 || spectrum[peak] - baseline < 5. * sigma)
    {
    d->Message->showMessage("info", tr("No line above the noise of the integrated spectrum: "
                                       "the range is unchanged."));
    return;
    }
  // the channels around the peak above 1.5 times the noise, plus two
  int first = peak, last = peak;
  const double limit = baseline + 1.5 * sigma;
  while (first > 0 && (!std::isfinite(spectrum[first - 1]) || spectrum[first - 1] > limit))
    {
    first--;
    }
  while (last < spectrum.size() - 1 && (!std::isfinite(spectrum[last + 1]) || spectrum[last + 1] > limit))
    {
    last++;
    }
  first = std::max(0, first - 2);
  last = std::min(static_cast<int>(spectrum.size()) - 1, last + 2);
  const double halfChannel = velocities.size() > 1 ? 0.5 * std::abs(velocities[1] - velocities[0]) : 0.;
  const double v1 = std::min(velocities[first], velocities[last]) - halfChannel;
  const double v2 = std::max(velocities[first], velocities[last]) + halfChannel;
  d->Message->clearMessage();
  this->onVelocityRangeChanged(std::max(v1, d->VelocityRangeWidget->minimumX()),
                               std::min(v2, d->VelocityRangeWidget->maximumX()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onShowZeroMoment()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  if (d->parametersNode)
    {
    d->showMap(d->parametersNode->GetZeroMomentVolumeNodeID(), "vtkMRMLSliceNodeRed", "vtkMRMLSliceCompositeNodeRed");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onShowFirstMoment()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  if (d->parametersNode)
    {
    d->showMap(d->parametersNode->GetFirstMomentVolumeNodeID(), "vtkMRMLSliceNodeRed", "vtkMRMLSliceCompositeNodeRed");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onShowSecondMoment()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  if (d->parametersNode)
    {
    d->showMap(d->parametersNode->GetSecondMomentVolumeNodeID(), "vtkMRMLSliceNodeRed", "vtkMRMLSliceCompositeNodeRed");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onZeroMomentVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetZeroMomentVolumeNodeID(mrmlNode ? mrmlNode->GetID() : nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->CancelRequested = true;
  if (d->parametersNode)
    {
    d->parametersNode->SetStatus(-1);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->RunButton->setProgress(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->RunButton->setProgressText(tr("Computing the moment maps"));
  d->RunButton->setRunning(true);
}

//---------------------------------------------------------------------------
vtkMRMLAstroMomentMapsParametersNode* qSlicerAstroMomentMapsModuleWidget::
mrmlAstroMomentMapsParametersNode()const
{
  Q_D(const qSlicerAstroMomentMapsModuleWidget);
  return d->parametersNode;
}
