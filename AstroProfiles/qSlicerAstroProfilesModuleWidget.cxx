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
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>

// STD includes
#include <cstring>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkTable.h>
#include <vtkWeakPointer.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>

// AstroProfiles includes
#include "qSlicerAstroProfilesModuleWidget.h"

// AstroVolume widgets includes
#include <qSlicerAstroDatasetSelector.h>
#include <qSlicerAstroFoldSection.h>
#include <qSlicerAstroMessageWidget.h>
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroRunButton.h>
#include <qSlicerAstroSegmentedButton.h>
#include <qSlicerAstroSpectrumPlot.h>
#include <qSlicerAstroSpectrumRangeWidget.h>
#include <qSlicerAstroThumbnails.h>
#include <qSlicerAstroTheme.h>
#include <qSlicerAstroValueField.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroProfilesLogic.h>

// qMRML includes
#include <qMRMLNodeComboBox.h>
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSortFilterProxyModel.h>

// MRMLLogic includes
#include <vtkSlicerApplicationLogic.h>
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroProfilesParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLPlotSeriesNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>
#include <vector>

namespace
{
/// Reference from a profile volume to the table of its velocities and fluxes
const char* ProfileTableReferenceRole = "SlicerAstro.ProfileTable";

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
QString FormatVelocity(double value)
{
  return std::isfinite(value) ?
    QString("%1 km/s").arg(value, 0, 'f', 1).replace('-', QChar(0x2212)) : QString::fromUtf8("—");
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
/// \ingroup SlicerAstro_QtModules_AstroProfiles
class qSlicerAstroProfilesModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroProfilesModuleWidget);
protected:
  qSlicerAstroProfilesModuleWidget* const q_ptr;
public:

  qSlicerAstroProfilesModuleWidgetPrivate(qSlicerAstroProfilesModuleWidget& object);
  ~qSlicerAstroProfilesModuleWidgetPrivate();

  void init();
  void cleanPointers();

  vtkSlicerAstroProfilesLogic* logic() const;
  vtkMRMLAstroVolumeNode* inputVolume() const;
  /// Noise of the input (0 if unknown)
  double noise() const;
  /// Table (velocity, flux) of the current profile
  vtkMRMLTableNode* profileTable() const;
  void updateRunButton();
  /// The current profile in the plot and its measurements
  void updateResults();
  /// Removes a profile volume with its table and plot series
  void removeProfile(vtkMRMLNode* profileVolume);

  // Panel
  qSlicerAstroPanelHeader* Header{nullptr};
  qSlicerAstroDatasetSelector* InputVolumeNodeSelector{nullptr};
  qSlicerAstroMessageWidget* Message{nullptr};
  qSlicerAstroSegmentedButton* RegionButton{nullptr};
  QStackedWidget* RegionStack{nullptr};
  qSlicerAstroValueField* ThresholdField{nullptr};
  QLabel* ThresholdInfo{nullptr};
  qMRMLNodeComboBox* SegmentationNodeComboBox{nullptr};
  qMRMLSegmentsTableView* SegmentsTableView{nullptr};
  QCheckBox* MaskApertureCheckBox{nullptr};
  QLabel* W50Label{nullptr};
  qSlicerAstroSpectrumPlot* SpectrumPlot{nullptr};
  QWidget* Measurements{nullptr};
  QLabel* IntegratedFluxValue{nullptr};
  QLabel* PeakFluxValue{nullptr};
  QLabel* SystemicVelocityValue{nullptr};
  QLabel* W20Value{nullptr};
  qSlicerAstroFoldSection* AdvancedSection{nullptr};
  qSlicerAstroValueField* VelocityMinField{nullptr};
  qSlicerAstroValueField* VelocityMaxField{nullptr};
  qSlicerAstroSpectrumRangeWidget* VelocityRangeWidget{nullptr};
  QPushButton* AutoRangeButton{nullptr};
  QLabel* VelocityNote{nullptr};
  qSlicerAstroValueField* IntensityMaxField{nullptr};
  qMRMLNodeComboBox* ProfileVolumeNodeSelector{nullptr};
  qMRMLNodeComboBox* ParametersNodeComboBox{nullptr};
  qSlicerAstroRunButton* RunButton{nullptr};
  QPushButton* PlotViewButton{nullptr};
  QPushButton* ExportButton{nullptr};

  vtkSmartPointer<vtkMRMLAstroProfilesParametersNode> parametersNode;
  // input volume observed (Modified) and input volume of the selected ranges
  vtkWeakPointer<vtkMRMLNode> observedInputVolume;
  vtkWeakPointer<vtkMRMLNode> rangesInputVolume;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeProfile;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
  vtkSmartPointer<vtkMRMLUnitNode> unitNodeIntensity;
  vtkSmartPointer<vtkMRMLUnitNode> unitNodeVelocity;
  /// The user cancelled the current computation
  bool CancelRequested{false};
};

//-----------------------------------------------------------------------------
// qSlicerAstroProfilesModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModuleWidgetPrivate::qSlicerAstroProfilesModuleWidgetPrivate(qSlicerAstroProfilesModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModuleWidgetPrivate::~qSlicerAstroProfilesModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroProfilesModuleWidget);

  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(qSlicerAstroProfilesModuleWidget::tr("Profiles"));
  this->Header->setIconName("module-profiles");
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
  this->InputVolumeNodeSelector->setNoneText(qSlicerAstroProfilesModuleWidget::tr("Select a data cube"));
  this->InputVolumeNodeSelector->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Data cube of the profile (also the active volume of the views)."));
  bodyLayout->addWidget(this->InputVolumeNodeSelector);

  this->Message = new qSlicerAstroMessageWidget(body);
  this->Message->setObjectName("MessageWidget");
  bodyLayout->addWidget(this->Message);

  // Velocity range over the integrated spectrum (as in Moment maps)
  QWidget* velocity = new QWidget(body);
  QVBoxLayout* velocityLayout = new QVBoxLayout(velocity);
  velocityLayout->setContentsMargins(0, 0, 0, 0);
  velocityLayout->setSpacing(8);
  QHBoxLayout* velocityHeader = new QHBoxLayout();
  velocityHeader->addWidget(Overline(qSlicerAstroProfilesModuleWidget::tr("Velocity range"), velocity));
  velocityHeader->addStretch(1);
  this->AutoRangeButton = new QPushButton(qSlicerAstroProfilesModuleWidget::tr("Auto from line"), velocity);
  this->AutoRangeButton->setObjectName("AutoRangeButton");
  this->AutoRangeButton->setProperty("astroRole", "ghost");
  this->AutoRangeButton->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "The channels of the line: where the integrated spectrum is above its noise, around the peak."));
  velocityHeader->addWidget(this->AutoRangeButton);
  velocityLayout->addLayout(velocityHeader);
  this->VelocityRangeWidget = new qSlicerAstroSpectrumRangeWidget(velocity);
  this->VelocityRangeWidget->setObjectName("VelocityRangeWidget");
  this->VelocityRangeWidget->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Channels of the profile: drag over the integrated spectrum, or drag the edges of the range."));
  velocityLayout->addWidget(this->VelocityRangeWidget);
  QGridLayout* velocityFields = new QGridLayout();
  velocityFields->setSpacing(6);
  this->VelocityMinField = new qSlicerAstroValueField(velocity);
  this->VelocityMinField->setObjectName("VelocityMinField");
  this->VelocityMinField->setUnit("km/s");
  this->VelocityMinField->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Lower velocity of the channels of the profile (Cube only: a mask uses all the channels)."));
  this->VelocityMaxField = new qSlicerAstroValueField(velocity);
  this->VelocityMaxField->setObjectName("VelocityMaxField");
  this->VelocityMaxField->setUnit("km/s");
  this->VelocityMaxField->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Upper velocity of the channels of the profile (Cube only: a mask uses all the channels)."));
  velocityFields->addWidget(this->VelocityMinField, 0, 0);
  velocityFields->addWidget(this->VelocityMaxField, 0, 1);
  velocityLayout->addLayout(velocityFields);
  this->VelocityNote = new QLabel(qSlicerAstroProfilesModuleWidget::tr(
    "With a mask, the mask selects the voxels in all the channels."), velocity);
  this->VelocityNote->setProperty("astroRole", "note");
  this->VelocityNote->setWordWrap(true);
  this->VelocityNote->hide();
  velocityLayout->addWidget(this->VelocityNote);
  bodyLayout->addWidget(velocity);

  // Region
  QWidget* region = new QWidget(body);
  QVBoxLayout* regionLayout = new QVBoxLayout(region);
  regionLayout->setContentsMargins(0, 0, 0, 0);
  regionLayout->setSpacing(8);
  regionLayout->addWidget(Overline(qSlicerAstroProfilesModuleWidget::tr("Integrate over"), region));
  this->RegionButton = new qSlicerAstroSegmentedButton(region);
  this->RegionButton->setObjectName("RegionButton");
  this->RegionButton->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Voxels of the profile: the cube above a threshold, or a mask."));
  this->RegionButton->addButton(qSlicerAstroProfilesModuleWidget::tr("Cube"),
    qSlicerAstroProfilesModuleWidget::tr("In each channel of the velocity range, the voxels of the whole cube "
                                          "above a threshold."));
  this->RegionButton->addButton(qSlicerAstroProfilesModuleWidget::tr("Mask"),
    qSlicerAstroProfilesModuleWidget::tr("The voxels of a segment of a mask (e.g. a SoFiA source), "
                                          "or its sky area in all the channels."));
  regionLayout->addWidget(this->RegionButton);
  this->RegionStack = new QStackedWidget(region);
  QWidget* cubePage = new QWidget(this->RegionStack);
  QVBoxLayout* cubeLayout = new QVBoxLayout(cubePage);
  cubeLayout->setContentsMargins(0, 0, 0, 0);
  cubeLayout->setSpacing(4);
  this->ThresholdField = new qSlicerAstroValueField(cubePage);
  this->ThresholdField->setObjectName("ThresholdField");
  this->ThresholdField->setLeadingText(qSlicerAstroProfilesModuleWidget::tr("above"));
  this->ThresholdField->setUnit(QString::fromUtf8("σ"));
  this->ThresholdField->setDecimals(2);
  this->ThresholdField->setSingleStep(0.5);
  this->ThresholdField->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Lower threshold of the intensities, in units of the noise (σ) of the data."));
  cubeLayout->addWidget(this->ThresholdField);
  this->ThresholdInfo = new QLabel(cubePage);
  this->ThresholdInfo->setProperty("astroRole", "monoLabel");
  cubeLayout->addWidget(this->ThresholdInfo);
  this->RegionStack->addWidget(cubePage);
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
  this->SegmentationNodeComboBox->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Mask with the segments. By default the mask of the input data cube."));
  maskLayout->addWidget(this->SegmentationNodeComboBox);
  this->SegmentsTableView = new qMRMLSegmentsTableView(maskPage);
  this->SegmentsTableView->setObjectName("SegmentsTableView");
  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  this->SegmentsTableView->setMinimumHeight(120);
  this->SegmentsTableView->setMaximumHeight(170);
  this->SegmentsTableView->setToolTip(qSlicerAstroProfilesModuleWidget::tr("The selected segment is the mask."));
  maskLayout->addWidget(this->SegmentsTableView);
  // aperture: the sky area of the mask in all channels (the faint wings of a
  // source are included, the noise averages out)
  this->MaskApertureCheckBox = new QCheckBox(qSlicerAstroProfilesModuleWidget::tr(
    "Aperture: the sky area of the mask in all channels"), maskPage);
  this->MaskApertureCheckBox->setObjectName("MaskApertureCheckBox");
  this->MaskApertureCheckBox->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Checked: in each channel the sum of all the voxels in the sky area of the mask (its voxels in any channel), "
    "without threshold: the faint emission of the wings is included and the noise averages out (global profile). "
    "Unchecked: the sum of the voxels of the mask only."));
  maskLayout->addWidget(this->MaskApertureCheckBox);
  this->RegionStack->addWidget(maskPage);
  regionLayout->addWidget(this->RegionStack);
  bodyLayout->addWidget(region);

  // Spectrum and measurements
  QWidget* spectrum = new QWidget(body);
  QVBoxLayout* spectrumLayout = new QVBoxLayout(spectrum);
  spectrumLayout->setContentsMargins(0, 0, 0, 0);
  spectrumLayout->setSpacing(8);
  QHBoxLayout* spectrumHeader = new QHBoxLayout();
  spectrumHeader->addWidget(Overline(qSlicerAstroProfilesModuleWidget::tr("Spectrum"), spectrum));
  spectrumHeader->addStretch(1);
  this->W50Label = new QLabel(spectrum);
  this->W50Label->setObjectName("W50Label");
  this->W50Label->setProperty("astroRole", "monoLabel");
  this->W50Label->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Width of the line at 50% of the peak (amber band in the plot)."));
  spectrumHeader->addWidget(this->W50Label);
  spectrumLayout->addLayout(spectrumHeader);
  this->SpectrumPlot = new qSlicerAstroSpectrumPlot(spectrum);
  this->SpectrumPlot->setObjectName("SpectrumPlot");
  this->SpectrumPlot->setPlaceholderText(qSlicerAstroProfilesModuleWidget::tr("Compute the profile to see it here"));
  this->SpectrumPlot->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Profile: flux of each channel. Amber band: W50; dashed lines: W20."));
  spectrumLayout->addWidget(this->SpectrumPlot);
  this->Measurements = new QWidget(spectrum);
  QGridLayout* measurementsLayout = new QGridLayout(this->Measurements);
  measurementsLayout->setContentsMargins(0, 0, 0, 0);
  measurementsLayout->setHorizontalSpacing(12);
  measurementsLayout->setVerticalSpacing(4);
  QLabel** values[4] = {&this->IntegratedFluxValue, &this->PeakFluxValue, &this->SystemicVelocityValue, &this->W20Value};
  const char* valueNames[4] = {"IntegratedFluxValue", "PeakFluxValue", "SystemicVelocityValue", "W20Value"};
  const QString valueLabels[4] = {qSlicerAstroProfilesModuleWidget::tr("Integrated flux"),
                                  qSlicerAstroProfilesModuleWidget::tr("Peak"),
                                  qSlicerAstroProfilesModuleWidget::tr("Systemic (mid W50)"),
                                  qSlicerAstroProfilesModuleWidget::tr("W20")};
  const QString valueToolTips[4] = {
    qSlicerAstroProfilesModuleWidget::tr("Sum of the flux of each channel times the channel width."),
    qSlicerAstroProfilesModuleWidget::tr("Highest flux of the profile."),
    qSlicerAstroProfilesModuleWidget::tr("Middle of the edges of the line at 50% of the peak."),
    qSlicerAstroProfilesModuleWidget::tr("Width of the line at 20% of the peak (dashed lines in the plot).")};
  for (int i = 0; i < 4; i++)
    {
    QLabel* label = new QLabel(valueLabels[i], this->Measurements);
    label->setProperty("astroRole", "label");
    label->setToolTip(valueToolTips[i]);
    QLabel* value = new QLabel(this->Measurements);
    value->setObjectName(valueNames[i]);
    value->setProperty("astroRole", "mono");
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setToolTip(valueToolTips[i]);
    measurementsLayout->addWidget(label, i, 0);
    measurementsLayout->addWidget(value, i, 1);
    *values[i] = value;
    }
  measurementsLayout->setColumnStretch(1, 1);
  spectrumLayout->addWidget(this->Measurements);
  bodyLayout->addWidget(spectrum);

  // Advanced
  this->AdvancedSection = new qSlicerAstroFoldSection(body);
  this->AdvancedSection->setObjectName("AdvancedSection");
  this->AdvancedSection->setSummary(qSlicerAstroProfilesModuleWidget::tr("upper limit, output"));
  this->AdvancedSection->setSettingsKey("SlicerAstro/AstroProfiles/AdvancedCollapsed");
  QWidget* advancedParent = this->AdvancedSection->contentWidget();
  QFormLayout* advancedLayout = new QFormLayout();
  advancedLayout->setContentsMargins(0, 0, 0, 0);
  advancedLayout->setSpacing(8);
  advancedLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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
  this->IntensityMaxField = new qSlicerAstroValueField(advancedParent);
  this->IntensityMaxField->setObjectName("IntensityMaxField");
  this->IntensityMaxField->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Upper limit of the intensities (Cube only; by default the maximum of the data)."));
  addRow(qSlicerAstroProfilesModuleWidget::tr("Upper limit"), this->IntensityMaxField);
  this->ProfileVolumeNodeSelector = new qMRMLNodeComboBox(advancedParent);
  this->ProfileVolumeNodeSelector->setObjectName("ProfileVolumeNodeSelector");
  this->ProfileVolumeNodeSelector->setNodeTypes(QStringList() << "vtkMRMLAstroVolumeNode");
  this->ProfileVolumeNodeSelector->setNoneEnabled(true);
  this->ProfileVolumeNodeSelector->setAddEnabled(false);
  this->ProfileVolumeNodeSelector->setEditEnabled(false);
  this->ProfileVolumeNodeSelector->setRenameEnabled(true);
  this->ProfileVolumeNodeSelector->setSelectNodeUponCreation(false);
  this->ProfileVolumeNodeSelector->setNoneDisplay(qSlicerAstroProfilesModuleWidget::tr("New profile"));
  this->ProfileVolumeNodeSelector->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Profile replaced by the next computation (a new profile if None)."));
  addRow(qSlicerAstroProfilesModuleWidget::tr("Output"), this->ProfileVolumeNodeSelector);
  this->ParametersNodeComboBox = new qMRMLNodeComboBox(advancedParent);
  this->ParametersNodeComboBox->setObjectName("ParametersNodeComboBox");
  this->ParametersNodeComboBox->setNodeTypes(QStringList() << "vtkMRMLAstroProfilesParametersNode");
  this->ParametersNodeComboBox->setShowHidden(true);
  this->ParametersNodeComboBox->setAddEnabled(false);
  this->ParametersNodeComboBox->setEditEnabled(false);
  this->ParametersNodeComboBox->setRenameEnabled(true);
  addRow(qSlicerAstroProfilesModuleWidget::tr("Parameter set"), this->ParametersNodeComboBox);
  this->AdvancedSection->contentLayout()->addLayout(advancedLayout);
  bodyLayout->addWidget(this->AdvancedSection);

  // Footer: run, plot view and export
  QFrame* footer = new QFrame(q);
  footer->setProperty("astroRole", "panelFooter");
  QVBoxLayout* footerLayout = new QVBoxLayout(footer);
  footerLayout->setContentsMargins(12, 12, 12, 12);
  footerLayout->setSpacing(8);
  this->RunButton = new qSlicerAstroRunButton(footer);
  this->RunButton->setObjectName("RunButton");
  this->RunButton->pushButton()->setObjectName("ApplyButton");
  this->RunButton->pushButton()->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Compute the profile: the flux of each channel, with its widths and integrated flux."));
  footerLayout->addWidget(this->RunButton);
  QHBoxLayout* actionsLayout = new QHBoxLayout();
  actionsLayout->setSpacing(6);
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();
  this->PlotViewButton = new QPushButton(qSlicerAstroProfilesModuleWidget::tr("Open in plot view"), footer);
  this->PlotViewButton->setObjectName("PlotViewButton");
  this->PlotViewButton->setIcon(theme->icon("plot", "text"));
  this->PlotViewButton->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "The profile in Slicer's plot view (zoom, pan, compare profiles)."));
  this->ExportButton = new QPushButton(qSlicerAstroProfilesModuleWidget::tr("CSV"), footer);
  this->ExportButton->setObjectName("ExportButton");
  this->ExportButton->setIcon(theme->icon("download", "text"));
  this->ExportButton->setToolTip(qSlicerAstroProfilesModuleWidget::tr(
    "Save the velocity and the flux of each channel as a CSV file."));
  actionsLayout->addWidget(this->PlotViewButton, 1);
  actionsLayout->addWidget(this->ExportButton);
  footerLayout->addLayout(actionsLayout);
  rootLayout->addWidget(footer);
  rootLayout->addStretch(1);

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroProfilesParametersNode(vtkMRMLNode*)));
  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));
  QObject::connect(this->ProfileVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onProfileVolumeChanged(vtkMRMLNode*)));
  for (QObject* selector : std::vector<QObject*>{this->InputVolumeNodeSelector, this->ParametersNodeComboBox,
         this->ProfileVolumeNodeSelector, this->SegmentationNodeComboBox, this->SegmentsTableView})
    {
    QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), selector, SLOT(setMRMLScene(vtkMRMLScene*)));
    }
  QObject::connect(this->RegionButton, SIGNAL(currentIndexChanged(int)), q, SLOT(onRegionChanged(int)));
  QObject::connect(this->MaskApertureCheckBox, SIGNAL(toggled(bool)), q, SLOT(onMaskApertureToggled(bool)));
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
  QObject::connect(this->AutoRangeButton, &QPushButton::clicked, q, [this]()
    {
    const QString reason = this->VelocityRangeWidget->setRangeFromLine();
    if (reason.isEmpty())
      {
      this->Message->clearMessage();
      }
    else
      {
      this->Message->showMessage("info", reason);
      }
    });
  QObject::connect(this->VelocityMinField, SIGNAL(valueChanged(double)), q, SLOT(onVelocityFieldsChanged()));
  QObject::connect(this->VelocityMaxField, SIGNAL(valueChanged(double)), q, SLOT(onVelocityFieldsChanged()));
  QObject::connect(this->RunButton, SIGNAL(clicked()), q, SLOT(onCalculate()));
  QObject::connect(this->RunButton, SIGNAL(cancelRequested()), q, SLOT(onComputationCancelled()));
  QObject::connect(this->PlotViewButton, SIGNAL(clicked()), q, SLOT(onOpenPlotView()));
  QObject::connect(this->ExportButton, SIGNAL(clicked()), q, SLOT(onExportCSV()));
  QObject::connect(this->InputVolumeNodeSelector, &qSlicerAstroDatasetSelector::currentNodeChanged,
                   q, [this](vtkMRMLNode*) { this->updateRunButton(); });

  theme->applyTo(q);
  this->updateRunButton();
  this->updateResults();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroProfilesModuleWidget);

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
  this->parametersNode = nullptr;

  if (this->plotChartNodeProfile)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeProfile);
    }
  this->plotChartNodeProfile = nullptr;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroProfilesLogic* qSlicerAstroProfilesModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroProfilesModuleWidget);
  return vtkSlicerAstroProfilesLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroProfilesModuleWidgetPrivate::inputVolume() const
{
  Q_Q(const qSlicerAstroProfilesModuleWidget);
  if (!this->parametersNode || !q->mrmlScene() || !this->parametersNode->GetInputVolumeNodeID())
    {
    return nullptr;
    }
  return vtkMRMLAstroVolumeNode::SafeDownCast(
    q->mrmlScene()->GetNodeByID(this->parametersNode->GetInputVolumeNodeID()));
}

//-----------------------------------------------------------------------------
double qSlicerAstroProfilesModuleWidgetPrivate::noise() const
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
vtkMRMLTableNode* qSlicerAstroProfilesModuleWidgetPrivate::profileTable() const
{
  Q_Q(const qSlicerAstroProfilesModuleWidget);
  if (!this->parametersNode || !q->mrmlScene() || !this->parametersNode->GetProfileVolumeNodeID())
    {
    return nullptr;
    }
  vtkMRMLNode* profile = q->mrmlScene()->GetNodeByID(this->parametersNode->GetProfileVolumeNodeID());
  return profile ? vtkMRMLTableNode::SafeDownCast(profile->GetNodeReference(ProfileTableReferenceRole)) : nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidgetPrivate::updateRunButton()
{
  const bool input = this->InputVolumeNodeSelector->currentNode() != nullptr;
  this->RunButton->setText(input ? qSlicerAstroProfilesModuleWidget::tr("Compute profile") :
                                   qSlicerAstroProfilesModuleWidget::tr("Select a data cube"));
  this->RunButton->setEnabled(input);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidgetPrivate::updateResults()
{
  vtkMRMLTableNode* table = this->profileTable();
  vtkTable* data = table ? table->GetTable() : nullptr;
  const bool profile = data && data->GetNumberOfColumns() >= 2 && data->GetNumberOfRows() >= 2;
  this->PlotViewButton->setEnabled(profile);
  this->ExportButton->setEnabled(profile);
  this->Measurements->setVisible(profile);
  if (!profile)
    {
    this->SpectrumPlot->clearSpectrum();
    this->W50Label->clear();
    return;
    }

  vtkNew<vtkDoubleArray> velocity;
  vtkNew<vtkDoubleArray> flux;
  const vtkIdType rows = data->GetNumberOfRows();
  velocity->SetNumberOfValues(rows);
  flux->SetNumberOfValues(rows);
  QVector<double> x(static_cast<int>(rows)), y(static_cast<int>(rows));
  for (vtkIdType row = 0; row < rows; row++)
    {
    velocity->SetValue(row, data->GetValue(row, 0).ToDouble());
    flux->SetValue(row, data->GetValue(row, 1).ToDouble());
    x[static_cast<int>(row)] = velocity->GetValue(row);
    y[static_cast<int>(row)] = flux->GetValue(row);
    }
  const char* fluxColumn = data->GetColumnName(1);
  const std::string unitLabel = fluxColumn ? table->GetColumnUnitLabel(fluxColumn) : std::string();
  const QString unit = qSlicerAstroTheme::displayUnit(QString::fromStdString(unitLabel));
  this->SpectrumPlot->setSpectrum(x, y);
  this->SpectrumPlot->setUnits("km/s", unit);

  double measurements[9];
  const bool measured = vtkSlicerAstroProfilesLogic::MeasureProfile(velocity, flux, measurements);
  if (measured)
    {
    this->SpectrumPlot->setWidthMarkers(measurements[5], measurements[6], measurements[7], measurements[8]);
    this->W50Label->setText(QString("W50 %1").arg(FormatVelocity(measurements[2])));
    }
  else
    {
    this->SpectrumPlot->clearWidthMarkers();
    this->W50Label->setText(qSlicerAstroProfilesModuleWidget::tr("no emission"));
    }
  this->IntegratedFluxValue->setText(std::isfinite(measurements[0]) ?
    QString("%1 %2 km/s").arg(measurements[0], 0, 'g', 4).arg(unit).replace('-', QChar(0x2212)) :
    QString::fromUtf8("—"));
  this->PeakFluxValue->setText(measured ? FormatWithUnit(measurements[1], unit) : QString::fromUtf8("—"));
  this->SystemicVelocityValue->setText(FormatVelocity(measurements[4]));
  this->W20Value->setText(FormatVelocity(measurements[3]));
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidgetPrivate::removeProfile(vtkMRMLNode* node)
{
  Q_Q(qSlicerAstroProfilesModuleWidget);
  vtkMRMLScene* scene = q->mrmlScene();
  vtkMRMLAstroVolumeNode* profile = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (!scene || !profile)
    {
    return;
    }
  vtkMRMLNode* table = profile->GetNodeReference(ProfileTableReferenceRole);
  if (table)
    {
    std::vector<vtkMRMLNode*> series;
    scene->GetNodesByClass("vtkMRMLPlotSeriesNode", series);
    for (vtkMRMLNode* seriesNode : series)
      {
      vtkMRMLPlotSeriesNode* plotSeries = vtkMRMLPlotSeriesNode::SafeDownCast(seriesNode);
      if (plotSeries && plotSeries->GetTableNodeID() && table->GetID() &&
          !strcmp(plotSeries->GetTableNodeID(), table->GetID()))
        {
        scene->RemoveNode(plotSeries);
        }
      }
    scene->RemoveNode(table);
    }
  scene->RemoveNode(profile->GetStorageNode());
  scene->RemoveNode(profile->GetAstroVolumeDisplayNode());
  vtkMRMLVolumeRenderingDisplayNode* volumeRenderingDisplay =
    vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(profile->GetDisplayNode());
  if (volumeRenderingDisplay)
    {
    scene->RemoveNode(volumeRenderingDisplay->GetROINode());
    scene->RemoveNode(volumeRenderingDisplay);
    }
  scene->RemoveNode(profile);
}

//-----------------------------------------------------------------------------
// qSlicerAstroProfilesModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModuleWidget::qSlicerAstroProfilesModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroProfilesModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->init();

  QObject::connect(d->SegmentationNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this, SLOT(onMaskSegmentationChanged(vtkMRMLNode*)));
}

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModuleWidget::~qSlicerAstroProfilesModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::enter()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  this->Superclass::enter();

  this->updateMaskSelector();
  this->onMaskSegmentationChanged(d->SegmentationNodeComboBox->currentNode());
  d->updateResults();
}

//----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::exit()
{
  this->Superclass::exit();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::setMRMLScene : selectionNode not found!";
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
  this->qvtkReconnect(scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(onSceneNodeRemoved()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(updateMaskSelector()));
  this->updateMaskSelector();

  // the parameters (and so the input) may have been selected when the
  // selectors received the scene, before the selection node was known (e.g.
  // a second panel, embedded in the Workbench): the input is set up now
  if (d->InputVolumeNodeSelector->currentNode())
    {
    this->onInputVolumeChanged(d->InputVolumeNodeSelector->currentNode());
    }
  d->updateResults();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::initializeNodes(bool forceNew  /*= false*/)
{
  this->initializeParameterNode(forceNew);

  this->initializeSegmentations(forceNew);

  this->initializePlotNodes(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onEndCloseEvent : "
                   "appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes(true);
  this->onMRMLAstroProfilesParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onEndImportEvent : "
                   "appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroProfilesParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroProfilesParametersNode *astroParametersNode = nullptr;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroProfilesParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroProfilesParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroProfilesParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroProfilesParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroProfilesParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->SetMaskActive(false);
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::initializePlotNodes(bool forceNew  /*= false*/)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  // Check (and create) PlotChart node
  if (!d->plotChartNodeProfile || forceNew)
    {
    vtkMRMLPlotChartNode* existing = vtkMRMLPlotChartNode::SafeDownCast(
      this->mrmlScene()->GetFirstNode("ProfileChart", "vtkMRMLPlotChartNode"));
    if (!existing || forceNew)
      {
      d->plotChartNodeProfile.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeProfile->SetName("ProfileChart");
      d->plotChartNodeProfile->SetTitle("Profile");
      d->plotChartNodeProfile->SetXAxisTitle("Velocity (km/s)");
      d->plotChartNodeProfile->SetYAxisTitle("Flux per channel (Jy)");
      d->plotChartNodeProfile->SetEnablePointMoveAlongX(false);
      d->plotChartNodeProfile->SetEnablePointMoveAlongY(false);
      this->mrmlScene()->AddNode(d->plotChartNodeProfile);
      }
    else
      {
      d->plotChartNodeProfile = existing;
      }
    }

  // Select nullptr Chart
  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID(nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
bool qSlicerAstroProfilesModuleWidget::setMaskSegmentFromSelection()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  this->onMRMLSelectionNodeReferenceAdded(sender);
}

//--------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::setMRMLAstroProfilesParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroProfilesParametersNode* AstroProfilesParaNode =
      vtkMRMLAstroProfilesParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroProfilesParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroProfilesParametersNodeModified()));

  d->parametersNode = AstroProfilesParaNode;

  this->onMRMLAstroProfilesParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
    }

  this->updateMaskSelector();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onInputVolumeModified()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
    qCritical() << "qSlicerAstroProfilesModuleWidget::onInputVolumeModified :"
                   " WCS not found!";
    return;
    }
  const double velocityFactor = !strcmp(WCS->cunit[2], "m/s") ? 0.001 : 1.;

  // the limits of the data: edges of the first and last channels
  const int channels = StringToInt(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS3"));
  double ijk[3], worldOne[3], worldTwo[3];
  ijk[0] = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS1")) * 0.5;
  ijk[1] = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS2")) * 0.5;
  ijk[2] = 0.;
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldOne);
  ijk[2] = channels < 2 ? channels + 1 : channels;
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldTwo);
  const double Vmin = std::min(worldOne[2], worldTwo[2]) * velocityFactor;
  const double Vmax = std::max(worldOne[2], worldTwo[2]) * velocityFactor;
  double channelWidth = 0.;
  if (channels > 1)
    {
    double worldNext[3];
    ijk[2] = 1.;
    astroMrmlDisplayNode->GetReferenceSpace(ijk, worldNext);
    channelWidth = std::abs(worldNext[2] - worldOne[2]) * velocityFactor;
    }
  for (qSlicerAstroValueField* field : {d->VelocityMinField, d->VelocityMaxField})
    {
    bool wasBlocked = field->blockSignals(true);
    field->setRange(Vmin, Vmax);
    field->setSingleStep(channelWidth > 0. ? channelWidth : (Vmax - Vmin) / 200.);
    field->blockSignals(wasBlocked);
    }
  // the integrated spectrum under the velocity range (velocity of each
  // channel at the centre of the field)
  QVector<double> channelVelocities(std::max(channels, 0));
  for (int k = 0; k < channels; k++)
    {
    double channelIJK[3] = {ijk[0], ijk[1], static_cast<double>(k)};
    double channelWorld[3];
    astroMrmlDisplayNode->GetReferenceSpace(channelIJK, channelWorld);
    channelVelocities[k] = channelWorld[2] * velocityFactor;
    }
  d->VelocityRangeWidget->setSpectrum(channelVelocities,
                                      qSlicerAstroThumbnails::instance()->integratedSpectrum(astroMrmlNode));

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
  if (channelWidth > 0.)
    {
    summary << QString("%1 km/s").arg(channelWidth, 0, 'g', 3);
    }
  summary << tr("%1 channels").arg(channels);
  d->InputVolumeNodeSelector->setSubtitle(summary.join(QString::fromUtf8(" · ")));
  this->onMRMLAstroProfilesParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onMaskApertureToggled(bool aperture)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetMaskAperture(aperture);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onMaskActiveToggled(bool active)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetMaskActive(active);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onRegionChanged(int index)
{
  this->onMaskActiveToggled(index == 1);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onThresholdChanged(double threshold)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  const double noise = d->noise();
  d->parametersNode->SetIntensityMin(noise > 0. ? threshold * noise : threshold);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onIntensityMaxChanged(double maximum)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetIntensityMax(maximum);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onMRMLAstroProfilesParametersNodeModified()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolumeNode = d->inputVolume();
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  const char *profileVolumeNodeID = d->parametersNode->GetProfileVolumeNodeID();
  d->ProfileVolumeNodeSelector->setCurrentNode(profileVolumeNodeID ?
    this->mrmlScene()->GetNodeByID(profileVolumeNodeID) : nullptr);

  const bool maskActive = d->parametersNode->GetMaskActive();
  d->RegionButton->setCurrentIndex(maskActive ? 1 : 0);
  // the stack has the height of the page shown (not of the tallest page)
  d->RegionStack->setCurrentIndex(maskActive ? 1 : 0);
  for (int page = 0; page < d->RegionStack->count(); page++)
    {
    d->RegionStack->widget(page)->setSizePolicy(QSizePolicy::Preferred,
      page == d->RegionStack->currentIndex() ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    }
  d->RegionStack->updateGeometry();
  d->SegmentsTableView->setEnabled(maskActive);
  bool wasBlocked = d->MaskApertureCheckBox->blockSignals(true);
  d->MaskApertureCheckBox->setChecked(d->parametersNode->GetMaskAperture());
  d->MaskApertureCheckBox->setEnabled(maskActive);
  d->MaskApertureCheckBox->blockSignals(wasBlocked);
  // with a mask the logic uses all the channels and no threshold
  d->VelocityMinField->setEnabled(!maskActive);
  d->VelocityMaxField->setEnabled(!maskActive);
  d->VelocityRangeWidget->setEnabled(!maskActive);
  d->AutoRangeButton->setEnabled(!maskActive);
  d->VelocityNote->setVisible(maskActive);
  d->VelocityRangeWidget->setRange(d->parametersNode->GetVelocityMin(), d->parametersNode->GetVelocityMax());
  d->IntensityMaxField->setEnabled(!maskActive);

  wasBlocked = d->VelocityMinField->blockSignals(true);
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

  d->updateRunButton();

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
void qSlicerAstroProfilesModuleWidget::onMRMLSelectionNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onCalculate()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onCalculate"
                   " : parametersNode not found!";
    return;
    }

  vtkSlicerAstroProfilesLogic *logic = d->logic();
  vtkMRMLScene *scene = this->mrmlScene();
  if (!logic || !scene)
    {
    qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate"
                  " : logic or scene not found!";
    return;
    }

  d->Message->clearMessage();
  vtkMRMLAstroVolumeNode *inputVolume = d->inputVolume();
  if(!inputVolume || !inputVolume->GetImageData())
    {
    d->Message->showMessage("error", tr("Select the data cube of the profile."));
    return;
    }

  // Check Input volume
  if (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS")) != 3)
    {
    d->Message->showMessage("error", tr("Profiles need a data cube (3 axes: two on the sky and the "
                                        "spectral axis)."));
    return;
    }
  vtkMRMLAstroVolumeDisplayNode* astroDisplay = inputVolume->GetAstroVolumeDisplayNode();
  struct wcsprm* WCS = astroDisplay ? astroDisplay->GetWCSStruct() : nullptr;
  if (!WCS)
    {
    d->Message->showMessage("error", tr("The data cube has no world coordinates (WCS)."));
    return;
    }

  if (d->parametersNode->GetMaskActive() && !this->setMaskSegmentFromSelection())
    {
    return;
    }

  d->CancelRequested = false;
  d->parametersNode->SetStatus(1);
  const int serial = d->parametersNode->GetOutputSerial();

  // the previous profile is replaced (never the input)
  vtkMRMLNode* previousProfile = d->parametersNode->GetProfileVolumeNodeID() ?
    scene->GetNodeByID(d->parametersNode->GetProfileVolumeNodeID()) : nullptr;
  if (previousProfile && previousProfile != inputVolume &&
      std::string(previousProfile->GetName() ? previousProfile->GetName() : "").find("_Profile_") != std::string::npos)
    {
    d->removeProfile(previousProfile);
    }

  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_Profile_" << serial;
  const std::string name = outSS.str();

  // the profile: a 1D astro volume along the spectral axis
  const int N1 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3"));
  vtkNew<vtkImageData> imageDataTemp;
  imageDataTemp->SetDimensions(N1, 1, 1);
  imageDataTemp->SetSpacing(1.,1.,1.);
  imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

  vtkMRMLAstroVolumeNode* ProfileVolume = vtkMRMLAstroVolumeNode::SafeDownCast
     (logic->GetAstroVolumeLogic()->CloneVolumeWithoutImageData(scene, inputVolume, name.c_str()));
  if (!ProfileVolume)
    {
    d->parametersNode->SetStatus(0);
    d->Message->showMessage("error", tr("The profile volume could not be created."));
    return;
    }

  // Modify fits attributes
  ProfileVolume->SetAttribute("SlicerAstro.NAXIS", "1");
  ProfileVolume->GetAstroVolumeDisplayNode()->SetAttribute("SlicerAstro.NAXIS", "1");
  ProfileVolume->GetAstroVolumeDisplayNode()->CopyWCS(inputVolume->GetAstroVolumeDisplayNode());
  std::string Bunit = ProfileVolume->GetAttribute("SlicerAstro.BUNIT") ? ProfileVolume->GetAttribute("SlicerAstro.BUNIT") : "";
  Bunit += " km/s";
  ProfileVolume->SetAttribute("SlicerAstro.BUNIT", Bunit.c_str());
  std::string Btype = inputVolume->GetAstroVolumeDisplayNode()->AddVelocityInfoToDisplayStringZ(std::string());
  ProfileVolume->SetAttribute("SlicerAstro.BTYPE", Btype.c_str());
  const char* spectralToFirst[] = {"NAXIS", "CROTA", "CRPIX", "CRVAL", "CTYPE", "CUNIT", "DTYPE", "DRVAL", "DUNIT"};
  for (const char* key : spectralToFirst)
    {
    const std::string third = std::string("SlicerAstro.") + key + "3";
    ProfileVolume->SetAttribute((std::string("SlicerAstro.") + key + "1").c_str(), inputVolume->GetAttribute(third.c_str()));
    }
  const char* removedKeys[] = {"NAXIS", "CDELT", "CROTA", "CRPIX", "CRVAL", "CTYPE", "CUNIT", "DTYPE", "DRVAL", "DUNIT"};
  for (int axis = 2; axis <= 3; axis++)
    {
    for (const char* key : removedKeys)
      {
      ProfileVolume->RemoveAttribute((std::string("SlicerAstro.") + key + std::to_string(axis)).c_str());
      }
    }
  for (int i = 1; i <= 3; i++)
    {
    for (int j = 1; j <= 3; j++)
      {
      ProfileVolume->RemoveAttribute(("SlicerAstro.PC" + std::to_string(i) + "_" + std::to_string(j)).c_str());
      }
    }

  ProfileVolume->SetAndObserveImageData(imageDataTemp.GetPointer());
  double Origin[3];
  inputVolume->GetOrigin(Origin);
  Origin[1] = 0.;
  ProfileVolume->SetOrigin(Origin);
  ProfileVolume->SetName(name.c_str());
  vtkMRMLNode* noPreset = nullptr;
  ProfileVolume->SetPresetNode(noPreset);
  // profiles have no volume rendering
  for (int ii = ProfileVolume->GetNumberOfDisplayNodes() - 1; ii >= 0; ii--)
    {
    if (vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(ProfileVolume->GetNthDisplayNode(ii)))
      {
      ProfileVolume->RemoveNthDisplayNodeID(ii);
      }
    }
  ProfileVolume->SetAttribute("SlicerAstro.DATAMODEL", "PROFILE");
  d->parametersNode->SetProfileVolumeNodeID(ProfileVolume->GetID());
  d->parametersNode->SetOutputSerial(serial + 1);

  // Run computation
  if (!logic->CalculateProfile(d->parametersNode))
    {
    const bool cancelled = d->CancelRequested;
    d->removeProfile(ProfileVolume);
    d->parametersNode->SetProfileVolumeNodeID(nullptr);
    d->parametersNode->SetStatus(0);
    if (cancelled)
      {
      d->Message->showMessage("info", tr("Computation cancelled."));
      }
    else
      {
      d->Message->showMessage("error", tr("The profile could not be computed (see the error log)."));
      }
    return;
    }

  // table (velocity, flux) for the plot and the export: flux in Jy with a
  // beam, otherwise the sum of the intensities in the units of the data
  const char* bunit = inputVolume->GetAttribute("SlicerAstro.BUNIT");
  const bool jansky = vtkSlicerAstroVolumeLogic::GetJyBeamToJyFactor(inputVolume) > 0.;
  const std::string fluxUnit = jansky ? "Jy" : (bunit && *bunit ? bunit : "");

  vtkNew<vtkTable> table;
  vtkNew<vtkMRMLTableNode> tableNode;
  int wasModifying = tableNode->StartModify();
  tableNode->SetName((name + "Table").c_str());
  tableNode->SetAndObserveTable(table.GetPointer());
  tableNode->RemoveAllColumns();
  tableNode->SetUseColumnTitleAsColumnHeader(true);
  tableNode->SetDefaultColumnType("double");
  vtkDoubleArray* velocityColumn = vtkDoubleArray::SafeDownCast(tableNode->AddColumn());
  vtkDoubleArray* fluxColumn = vtkDoubleArray::SafeDownCast(tableNode->AddColumn());
  if (!velocityColumn || !fluxColumn)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onCalculate : the table columns could not be created";
    tableNode->EndModify(wasModifying);
    d->parametersNode->SetStatus(0);
    return;
    }
  velocityColumn->SetName("Velocity");
  tableNode->SetColumnUnitLabel("Velocity", "km/s");
  tableNode->SetColumnTitle("Velocity", "Velocity");
  fluxColumn->SetName(name.c_str());
  tableNode->SetColumnUnitLabel(name.c_str(), fluxUnit.c_str());
  tableNode->SetColumnTitle(name.c_str(), "Flux");
  table->SetNumberOfRows(N1);
  const double velocityFactor = !strcmp(WCS->cunit[2], "m/s") ? 0.001 : 1.;
  double ijk[3], world[3];
  ijk[0] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) * 0.5;
  ijk[1] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) * 0.5;
  for (int ii = 0; ii < N1; ii++)
    {
    ijk[2] = ii;
    astroDisplay->GetReferenceSpace(ijk, world);
    table->SetValue(ii, 0, world[2] * velocityFactor);
    table->SetValue(ii, 1, ProfileVolume->GetImageData()->GetScalarComponentAsDouble(ii, 0, 0, 0));
    }
  tableNode->EndModify(wasModifying);
  scene->AddNode(tableNode.GetPointer());
  ProfileVolume->SetNodeReferenceID(ProfileTableReferenceRole, tableNode->GetID());

  vtkNew<vtkMRMLPlotSeriesNode> plotSeriesNode;
  plotSeriesNode->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
  plotSeriesNode->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleNone);
  plotSeriesNode->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
  plotSeriesNode->SetLineWidth(3);
  plotSeriesNode->SetName(name.c_str());
  plotSeriesNode->SetAndObserveTableNodeID(tableNode->GetID());
  plotSeriesNode->SetXColumnName(tableNode->GetColumnName(0));
  plotSeriesNode->SetYColumnName(tableNode->GetColumnName(1));
  // colour-blind safe colours (Okabe-Ito, without black: visible on the
  // dark plots of SlicerAstro and on the light ones of Slicer), one per
  // profile of the chart
  const double profileColors[7][3] = {{230., 159., 0.}, {86., 180., 233.}, {0., 158., 115.}, {204., 121., 167.},
                                      {213., 94., 0.}, {0., 114., 178.}, {240., 228., 66.}};
  const int colorIndex = d->plotChartNodeProfile ? d->plotChartNodeProfile->GetNumberOfPlotSeriesNodes() % 7 : 0;
  plotSeriesNode->SetColor(profileColors[colorIndex][0] / 255., profileColors[colorIndex][1] / 255.,
                           profileColors[colorIndex][2] / 255.);
  scene->AddNode(plotSeriesNode.GetPointer());

  if (d->plotChartNodeProfile)
    {
    QString unit = qSlicerAstroTheme::displayUnit(QString::fromUtf8(bunit ? bunit : ""));
    if (unit.isEmpty())
      {
      unit = tr("no unit");
      }
    d->plotChartNodeProfile->SetYAxisTitle(jansky ? "Flux per channel (Jy)" :
      (std::string("Sum per channel (") + unit.toStdString() + ")").c_str());
    d->plotChartNodeProfile->AddAndObservePlotSeriesNodeID(plotSeriesNode->GetID());
    }

  d->parametersNode->SetStatus(0);
  d->updateResults();
  d->Message->showMessage("success", tr("Profile computed"));
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onOpenPlotView()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();
  vtkMRMLTableNode* table = d->profileTable();
  if (!app || !app->layoutManager() || !d->plotChartNodeProfile || !table)
    {
    return;
    }
  // a layout with a plot view, unless the current layout shows one (plot
  // widgets of previous layouts stay alive, hidden: plotViewCount() does not
  // tell whether one is shown)
  vtkMRMLLayoutNode* layoutNode = app->layoutManager()->layoutLogic()->GetLayoutNode();
  const char* description = layoutNode ? layoutNode->GetCurrentLayoutDescription() : nullptr;
  if (layoutNode && (!description || !strstr(description, "vtkMRMLPlotViewNode")))
    {
    // channel map, 3D view and plot (the layout with a plot of SlicerAstro)
    layoutNode->SetViewArrangement(vtkSlicerAstroVolumeLogic::AddPlotLayout(layoutNode));
    }
  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID(d->plotChartNodeProfile->GetID());
    d->selectionNode->SetActiveTableID(table->GetID());
    }
  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (appLogic)
    {
    appLogic->PropagatePlotChartSelection();
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroProfilesModuleWidget::exportCSV(const QString& fileName)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  vtkMRMLTableNode* tableNode = d->profileTable();
  vtkTable* table = tableNode ? tableNode->GetTable() : nullptr;
  if (!table || table->GetNumberOfColumns() < 2 || fileName.isEmpty())
    {
    return false;
    }
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
    d->Message->showMessage("error", tr("The file %1 could not be written.").arg(fileName));
    return false;
    }
  const char* fluxColumn = table->GetColumnName(1);
  QString unit = fluxColumn ? QString::fromStdString(tableNode->GetColumnUnitLabel(fluxColumn)) : QString();
  unit = unit.isEmpty() ? QString("flux") : QString("flux (%1)").arg(unit);
  QTextStream stream(&file);
  stream << "velocity (km/s)," << unit << "\n";
  for (vtkIdType row = 0; row < table->GetNumberOfRows(); row++)
    {
    const double velocity = table->GetValue(row, 0).ToDouble();
    const double flux = table->GetValue(row, 1).ToDouble();
    stream << QString::number(velocity, 'g', 10) << ","
           << (std::isfinite(flux) ? QString::number(flux, 'g', 10) : QString("nan")) << "\n";
    }
  file.close();
  d->Message->showMessage("success", tr("Profile saved to %1").arg(fileName));
  return true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onExportCSV()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  vtkMRMLTableNode* table = d->profileTable();
  if (!table)
    {
    return;
    }
  QString defaultName = QString::fromUtf8(table->GetName() ? table->GetName() : "Profile");
  defaultName.replace(QRegularExpression("Table$"), QString());
  const QString fileName = QFileDialog::getSaveFileName(this, tr("Save the profile"),
    defaultName + ".csv", tr("CSV files (*.csv)"));
  if (!fileName.isEmpty())
    {
    this->exportCSV(fileName);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->RunButton->setRunning(false);
}

//---------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onMaskSegmentationChanged(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::updateMaskSelector()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onUnitNodeIntensityChanged(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  vtkMRMLUnitNode* unitNodeIntensity = vtkMRMLUnitNode::SafeDownCast(sender);
  if (!unitNodeIntensity)
    {
    return;
    }
  d->IntensityMaxField->setDecimals(unitNodeIntensity->GetPrecision());
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onUnitNodeVelocityChanged(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  vtkMRMLUnitNode* unitNodeVelocity = vtkMRMLUnitNode::SafeDownCast(sender);
  if (!unitNodeVelocity)
    {
    return;
    }
  d->VelocityMinField->setDecimals(unitNodeVelocity->GetPrecision());
  d->VelocityMaxField->setDecimals(unitNodeVelocity->GetPrecision());
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onVelocityRangeChanged(double min, double max)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onVelocityFieldsChanged()
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  this->onVelocityRangeChanged(std::min(d->VelocityMinField->value(), d->VelocityMaxField->value()),
                               std::max(d->VelocityMinField->value(), d->VelocityMaxField->value()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onProfileVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetProfileVolumeNodeID(mrmlNode ? mrmlNode->GetID() : nullptr);
  d->updateResults();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->CancelRequested = true;
  if (d->parametersNode)
    {
    d->parametersNode->SetStatus(-1);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->RunButton->setProgress(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->RunButton->setProgressText(tr("Computing the profile"));
  d->RunButton->setRunning(true);
}

//---------------------------------------------------------------------------
vtkMRMLAstroProfilesParametersNode* qSlicerAstroProfilesModuleWidget::
mrmlAstroProfilesParametersNode()const
{
  Q_D(const qSlicerAstroProfilesModuleWidget);
  return d->parametersNode;
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onSceneNodeRemoved()
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->updateResults();
}
