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
#include <QTimer>
#include <QButtonGroup>
#include <QSignalBlocker>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QToolButton>
#include <QStringList>
#include <QVBoxLayout>

// VTK includes
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkWeakPointer.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>

// AstroSmoothing includes
#include "qSlicerAstroSmoothingModuleWidget.h"

// AstroVolume widgets includes
#include <qSlicerAstroDatasetSelector.h>
#include <qSlicerAstroFoldSection.h>
#include <qSlicerAstroMessageWidget.h>
#include <qSlicerAstroPanelHeader.h>
#include <qSlicerAstroResultCard.h>
#include <qSlicerAstroRunButton.h>
#include <qSlicerAstroSegmentedButton.h>
#include <qSlicerAstroTheme.h>
#include <qSlicerAstroToggleCard.h>
#include <qSlicerAstroValueField.h>
#include <qSlicerAstroVolumeModuleWidget.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroSmoothingLogic.h>

// qMRML includes
#include <qMRMLNodeComboBox.h>

// MRMLLogic includes
#include <vtkSlicerApplicationLogic.h>
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroSmoothingParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSelectionNode.h>
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
QLabel* Overline(const QString& text, QWidget* parent)
{
  QLabel* label = new QLabel(text, parent);
  qSlicerAstroTheme::setLabelRole(label, "overline");
  return label;
}

//----------------------------------------------------------------------------
QString Arcsec(double value)
{
  return QString::fromUtf8("%1″").arg(value, 0, 'f', 1);
}
} // end namespace

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroSmoothing
class qSlicerAstroSmoothingModuleWidgetPrivate
{
  Q_DECLARE_PUBLIC(qSlicerAstroSmoothingModuleWidget);
protected:
  qSlicerAstroSmoothingModuleWidget* const q_ptr;
public:

  qSlicerAstroSmoothingModuleWidgetPrivate(qSlicerAstroSmoothingModuleWidget& object);
  ~qSlicerAstroSmoothingModuleWidgetPrivate();

  void init();
  void cleanPointers();
  vtkSlicerAstroSmoothingLogic* logic() const;
  vtkMRMLAstroVolumeNode* inputVolume() const;
  /// The panel of the module (not a copy embedded elsewhere, e.g. in the
  /// Workbench, which arranges its own views)
  bool isModulePanel() const;
  void updateRunButton();
  void updateResult();
  /// Beam of the output (Gaussian), or why it is unchanged
  void updateBeam();
  /// Labels, units and visibility of the fields of the filter
  void updateFields(int filter);

  // Panel
  qSlicerAstroPanelHeader* Header{nullptr};
  qSlicerAstroDatasetSelector* InputVolumeNodeSelector{nullptr};
  qSlicerAstroMessageWidget* Message{nullptr};
  QButtonGroup* FilterGroup{nullptr};
  qSlicerAstroToggleCard* FilterCards[3]{nullptr, nullptr, nullptr};
  QLabel* KernelTitle{nullptr};
  qSlicerAstroValueField* ParameterFields[3]{nullptr, nullptr, nullptr};
  QCheckBox* LinkCheckBox{nullptr};
  QLabel* BeamLabel{nullptr};
  qSlicerAstroSegmentedButton* HardwareButton{nullptr};
  QCheckBox* GenerateMaskCheckBox{nullptr};
  qSlicerAstroFoldSection* AdvancedSection{nullptr};
  QWidget* RotationRow{nullptr};
  qSlicerAstroValueField* RotationFields[3]{nullptr, nullptr, nullptr};
  QLabel* AccuracyLabel{nullptr};
  qSlicerAstroValueField* AccuracyField{nullptr};
  QWidget* GradientRow{nullptr};
  qSlicerAstroValueField* KField{nullptr};
  qSlicerAstroValueField* TimeStepField{nullptr};
  QCheckBox* AutoRunCheckBox{nullptr};
  qMRMLNodeComboBox* OutputVolumeNodeSelector{nullptr};
  qMRMLNodeComboBox* ParametersNodeComboBox{nullptr};
  qSlicerAstroRunButton* RunButton{nullptr};
  qSlicerAstroResultCard* ResultCard{nullptr};

  qSlicerAstroVolumeModuleWidget* astroVolumeWidget{nullptr};
  vtkSmartPointer<vtkMRMLAstroSmoothingParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
  vtkWeakPointer<vtkMRMLNode> observedInputVolume;
  bool CancelRequested{false};
  bool Updating{false};
};

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModuleWidgetPrivate::qSlicerAstroSmoothingModuleWidgetPrivate(qSlicerAstroSmoothingModuleWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModuleWidgetPrivate::~qSlicerAstroSmoothingModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroSmoothingModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* astroVolume = app && app->moduleManager() ?
    app->moduleManager()->module("AstroVolume") : nullptr;
  this->astroVolumeWidget = astroVolume ?
    dynamic_cast<qSlicerAstroVolumeModuleWidget*>(astroVolume->widgetRepresentation()) : nullptr;
  qSlicerAstroTheme* theme = qSlicerAstroTheme::instance();

  QVBoxLayout* rootLayout = new QVBoxLayout(q);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  this->Header = new qSlicerAstroPanelHeader(q);
  this->Header->setTitle(qSlicerAstroSmoothingModuleWidget::tr("Smoothing"));
  this->Header->setIconName("module-smoothing");
  rootLayout->addWidget(this->Header);

  QWidget* body = new QWidget(q);
  QVBoxLayout* bodyLayout = new QVBoxLayout(body);
  bodyLayout->setContentsMargins(12, 12, 12, 14);
  bodyLayout->setSpacing(14);
  rootLayout->addWidget(body);

  this->InputVolumeNodeSelector = new qSlicerAstroDatasetSelector(body);
  this->InputVolumeNodeSelector->setObjectName("InputVolumeNodeSelector");
  this->InputVolumeNodeSelector->setNoneText(qSlicerAstroSmoothingModuleWidget::tr("Select data"));
  this->InputVolumeNodeSelector->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
    "Data to smooth: a cube, an image or a spectrum (also the active volume of the views)."));
  bodyLayout->addWidget(this->InputVolumeNodeSelector);

  this->Message = new qSlicerAstroMessageWidget(body);
  this->Message->setObjectName("MessageWidget");
  bodyLayout->addWidget(this->Message);

  // Filter
  QWidget* filter = new QWidget(body);
  QVBoxLayout* filterLayout = new QVBoxLayout(filter);
  filterLayout->setContentsMargins(0, 0, 0, 0);
  filterLayout->setSpacing(8);
  filterLayout->addWidget(Overline(qSlicerAstroSmoothingModuleWidget::tr("Filter"), filter));
  QGridLayout* cardsLayout = new QGridLayout();
  cardsLayout->setSpacing(6);
  this->FilterGroup = new QButtonGroup(q);
  this->FilterGroup->setExclusive(true);
  const QString titles[3] = {qSlicerAstroSmoothingModuleWidget::tr("Box"),
                             qSlicerAstroSmoothingModuleWidget::tr("Gaussian"),
                             qSlicerAstroSmoothingModuleWidget::tr("Intensity")};
  const QString captions[3] = {qSlicerAstroSmoothingModuleWidget::tr("fastest"),
                               qSlicerAstroSmoothingModuleWidget::tr("rounder beam"),
                               qSlicerAstroSmoothingModuleWidget::tr("keeps edges")};
  const QString tips[3] = {
    qSlicerAstroSmoothingModuleWidget::tr("Box: the mean over a box of voxels. Fastest; the beam is not Gaussian any more."),
    qSlicerAstroSmoothingModuleWidget::tr("Gaussian: convolution with a Gaussian kernel. The beam becomes larger "
                                          "(and rounder): the new beam is written in the header, and Jy/beam data "
                                          "are scaled so that point sources keep their peak."),
    qSlicerAstroSmoothingModuleWidget::tr("Intensity-driven gradient smoothing (Punzo et al. 2016): smooths the "
                                          "noise and keeps the edges of the emission.")};
  const char* names[3] = {"BoxFilterCard", "GaussianFilterCard", "GradientFilterCard"};
  for (int i = 0; i < 3; i++)
    {
    this->FilterCards[i] = new qSlicerAstroToggleCard(titles[i], captions[i], filter);
    this->FilterCards[i]->setObjectName(names[i]);
    this->FilterCards[i]->setToolTip(tips[i]);
    this->FilterGroup->addButton(this->FilterCards[i], i);
    cardsLayout->addWidget(this->FilterCards[i], 0, i);
    }
  filterLayout->addLayout(cardsLayout);
  bodyLayout->addWidget(filter);

  // Kernel
  QWidget* kernel = new QWidget(body);
  QVBoxLayout* kernelLayout = new QVBoxLayout(kernel);
  kernelLayout->setContentsMargins(0, 0, 0, 0);
  kernelLayout->setSpacing(8);
  QHBoxLayout* kernelHeader = new QHBoxLayout();
  this->KernelTitle = Overline(qSlicerAstroSmoothingModuleWidget::tr("Kernel FWHM"), kernel);
  kernelHeader->addWidget(this->KernelTitle);
  kernelHeader->addStretch(1);
  this->LinkCheckBox = new QCheckBox(qSlicerAstroSmoothingModuleWidget::tr("Same on all axes"), kernel);
  this->LinkCheckBox->setObjectName("LinkCheckBox");
  this->LinkCheckBox->setToolTip(qSlicerAstroSmoothingModuleWidget::tr("Change the three values together."));
  kernelHeader->addWidget(this->LinkCheckBox);
  kernelLayout->addLayout(kernelHeader);
  QGridLayout* fieldsLayout = new QGridLayout();
  fieldsLayout->setSpacing(6);
  const char* axisNames[3] = {"x", "y", "z"};
  const char* fieldNames[3] = {"ParameterXField", "ParameterYField", "ParameterZField"};
  for (int i = 0; i < 3; i++)
    {
    this->ParameterFields[i] = new qSlicerAstroValueField(kernel);
    this->ParameterFields[i]->setObjectName(fieldNames[i]);
    this->ParameterFields[i]->setLeadingText(axisNames[i]);
    this->ParameterFields[i]->setDecimals(2);
    this->ParameterFields[i]->setRange(0., 1000.);
    this->ParameterFields[i]->setSingleStep(1.);
    fieldsLayout->addWidget(this->ParameterFields[i], 0, i);
    }
  kernelLayout->addLayout(fieldsLayout);
  this->BeamLabel = new QLabel(kernel);
  this->BeamLabel->setObjectName("BeamLabel");
  this->BeamLabel->setProperty("astroRole", "monoLabel");
  this->BeamLabel->setWordWrap(true);
  this->BeamLabel->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
    "Beam of the data before and after the smoothing (FWHM, position angle)."));
  kernelLayout->addWidget(this->BeamLabel);
  bodyLayout->addWidget(kernel);

  // Hardware and mask
  QWidget* run = new QWidget(body);
  QVBoxLayout* runLayout = new QVBoxLayout(run);
  runLayout->setContentsMargins(0, 0, 0, 0);
  runLayout->setSpacing(8);
  runLayout->addWidget(Overline(qSlicerAstroSmoothingModuleWidget::tr("Run on"), run));
  this->HardwareButton = new qSlicerAstroSegmentedButton(run);
  this->HardwareButton->setObjectName("HardwareButton");
  this->HardwareButton->setToolTip(qSlicerAstroSmoothingModuleWidget::tr("Processor of the smoothing."));
  this->HardwareButton->addButton(qSlicerAstroSmoothingModuleWidget::tr("CPU · %1 threads")
    .arg(QThread::idealThreadCount()),
    qSlicerAstroSmoothingModuleWidget::tr("All the cores of the processor."));
  const QString backend = QString::fromStdString(vtkSlicerAstroSmoothingLogic::GetGPUBackend());
  this->HardwareButton->addButton(backend.isEmpty() ? qSlicerAstroSmoothingModuleWidget::tr("GPU") :
                                  qSlicerAstroSmoothingModuleWidget::tr("GPU · %1").arg(backend),
    backend.isEmpty() ? qSlicerAstroSmoothingModuleWidget::tr("No GPU backend in this build.") :
    qSlicerAstroSmoothingModuleWidget::tr("The graphics card (%1); images and spectra are smoothed on the CPU.")
      .arg(QString::fromStdString(vtkSlicerAstroSmoothingLogic::GetGPUBackendDescription())));
  this->HardwareButton->button(1)->setEnabled(!backend.isEmpty());
  runLayout->addWidget(this->HardwareButton);
  this->GenerateMaskCheckBox = new QCheckBox(qSlicerAstroSmoothingModuleWidget::tr("Make a mask of the emission"), run);
  this->GenerateMaskCheckBox->setObjectName("GenerateMaskCheckBox");
  this->GenerateMaskCheckBox->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
    "A mask of the voxels above 3σ of the smoothed data, on the grid of the input (e.g. for moment maps, "
    "profiles and statistics of faint emission)."));
  runLayout->addWidget(this->GenerateMaskCheckBox);
  bodyLayout->addWidget(run);

  // Advanced
  this->AdvancedSection = new qSlicerAstroFoldSection(body);
  this->AdvancedSection->setObjectName("AdvancedSection");
  this->AdvancedSection->setTitle(qSlicerAstroSmoothingModuleWidget::tr("Advanced"));
  this->AdvancedSection->setSummary(qSlicerAstroSmoothingModuleWidget::tr("rotation, accuracy, run on change"));
  this->AdvancedSection->setSettingsKey("SlicerAstro/AstroSmoothing/AdvancedCollapsed");
  QWidget* advancedParent = this->AdvancedSection->contentWidget();
  this->RotationRow = new QWidget(advancedParent);
  QHBoxLayout* rotationLayout = new QHBoxLayout(this->RotationRow);
  rotationLayout->setContentsMargins(0, 0, 0, 0);
  rotationLayout->setSpacing(6);
  const char* rotationNames[3] = {"RxField", "RyField", "RzField"};
  const char* rotationLeads[3] = {"Rx", "Ry", "Rz"};
  for (int i = 0; i < 3; i++)
    {
    this->RotationFields[i] = new qSlicerAstroValueField(this->RotationRow);
    this->RotationFields[i]->setObjectName(rotationNames[i]);
    this->RotationFields[i]->setLeadingText(rotationLeads[i]);
    this->RotationFields[i]->setUnit(QString::fromUtf8("°"));
    this->RotationFields[i]->setDecimals(1);
    this->RotationFields[i]->setRange(-360., 360.);
    this->RotationFields[i]->setSingleStep(5.);
    this->RotationFields[i]->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
      "Rotation of the Gaussian kernel around the %1 axis (Punzo et al. 2016, Eq. 3).").arg(QString(rotationLeads[i]).mid(1)));
    rotationLayout->addWidget(this->RotationFields[i]);
    }
  this->AdvancedSection->contentLayout()->addWidget(this->RotationRow);
  QHBoxLayout* accuracyLayout = new QHBoxLayout();
  accuracyLayout->setSpacing(6);
  this->AccuracyLabel = new QLabel(advancedParent);
  this->AccuracyLabel->setProperty("astroRole", "label");
  accuracyLayout->addWidget(this->AccuracyLabel);
  this->AccuracyField = new qSlicerAstroValueField(advancedParent);
  this->AccuracyField->setObjectName("AccuracyField");
  this->AccuracyField->setDecimals(0);
  this->AccuracyField->setSingleStep(1.);
  accuracyLayout->addWidget(this->AccuracyField, 1);
  this->AdvancedSection->contentLayout()->addLayout(accuracyLayout);
  this->GradientRow = new QWidget(advancedParent);
  QHBoxLayout* gradientLayout = new QHBoxLayout(this->GradientRow);
  gradientLayout->setContentsMargins(0, 0, 0, 0);
  gradientLayout->setSpacing(6);
  this->KField = new qSlicerAstroValueField(this->GradientRow);
  this->KField->setObjectName("KField");
  this->KField->setLeadingText("K");
  this->KField->setDecimals(2);
  this->KField->setRange(0.01, 100.);
  this->KField->setSingleStep(0.1);
  this->KField->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
    "K: intensities above K times the noise are kept (edges of the emission); default 1.5."));
  this->TimeStepField = new qSlicerAstroValueField(this->GradientRow);
  this->TimeStepField->setObjectName("TimeStepField");
  this->TimeStepField->setLeadingText(QString::fromUtf8("τ"));
  this->TimeStepField->setDecimals(4);
  this->TimeStepField->setRange(0.0001, 1.);
  this->TimeStepField->setSingleStep(0.005);
  this->TimeStepField->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
    "Time step of each iteration of the diffusion; default 0.0325."));
  gradientLayout->addWidget(this->KField);
  gradientLayout->addWidget(this->TimeStepField);
  this->AdvancedSection->contentLayout()->addWidget(this->GradientRow);
  this->AutoRunCheckBox = new QCheckBox(qSlicerAstroSmoothingModuleWidget::tr("Run on change"), advancedParent);
  this->AutoRunCheckBox->setObjectName("AutoRunCheckBox");
  this->AutoRunCheckBox->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
    "Smooth again when a parameter changes (for quick tests on small data)."));
  this->AdvancedSection->contentLayout()->addWidget(this->AutoRunCheckBox);
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
  this->OutputVolumeNodeSelector->setNoneDisplay(qSlicerAstroSmoothingModuleWidget::tr("New data"));
  this->OutputVolumeNodeSelector->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
    "Data replaced by the next smoothing (new data if None)."));
  this->ParametersNodeComboBox = new qMRMLNodeComboBox(advancedParent);
  this->ParametersNodeComboBox->setObjectName("ParametersNodeComboBox");
  this->ParametersNodeComboBox->setNodeTypes(QStringList() << "vtkMRMLAstroSmoothingParametersNode");
  this->ParametersNodeComboBox->setShowHidden(true);
  this->ParametersNodeComboBox->setAddEnabled(false);
  this->ParametersNodeComboBox->setEditEnabled(false);
  this->ParametersNodeComboBox->setRenameEnabled(true);
  this->ParametersNodeComboBox->setToolTip(qSlicerAstroSmoothingModuleWidget::tr("Parameters of the module (saved with the scene)."));
  const QString formLabels[2] = {qSlicerAstroSmoothingModuleWidget::tr("Output"),
                                 qSlicerAstroSmoothingModuleWidget::tr("Parameter set")};
  qMRMLNodeComboBox* formFields[2] = {this->OutputVolumeNodeSelector, this->ParametersNodeComboBox};
  for (int i = 0; i < 2; i++)
    {
    QLabel* formLabel = new QLabel(formLabels[i], advancedParent);
    formLabel->setProperty("astroRole", "label");
    formLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    formFields[i]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    formFields[i]->setMinimumWidth(80);
    advancedLayout->addRow(formLabel, formFields[i]);
    // output and parameter set: chosen by the module, not by the user
    formLabel->hide();
    formFields[i]->hide();
    }
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
  this->RunButton->pushButton()->setToolTip(qSlicerAstroSmoothingModuleWidget::tr("Smooth the data (new data)."));
  footerLayout->addWidget(this->RunButton);
  this->ResultCard = new qSlicerAstroResultCard(footer);
  this->ResultCard->setObjectName("ResultCard");
  this->ResultCard->hide();
  footerLayout->addWidget(this->ResultCard);
  rootLayout->addWidget(footer);
  rootLayout->addStretch(1);

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroSmoothingParametersNode(vtkMRMLNode*)));
  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));
  QObject::connect(this->OutputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));
  for (QObject* selector : std::vector<QObject*>{this->InputVolumeNodeSelector, this->ParametersNodeComboBox,
         this->OutputVolumeNodeSelector})
    {
    QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), selector, SLOT(setMRMLScene(vtkMRMLScene*)));
    }
  QObject::connect(this->FilterGroup, &QButtonGroup::idClicked, q, &qSlicerAstroSmoothingModuleWidget::onFilterSelected);
  QObject::connect(this->HardwareButton, SIGNAL(currentIndexChanged(int)), q, SLOT(onHardwareChanged(int)));
  QObject::connect(this->GenerateMaskCheckBox, SIGNAL(toggled(bool)), q, SLOT(onGenerateMaskToggled(bool)));
  QObject::connect(this->LinkCheckBox, SIGNAL(toggled(bool)), q, SLOT(onLinkToggled(bool)));
  QObject::connect(this->AutoRunCheckBox, SIGNAL(toggled(bool)), q, SLOT(onAutoRunToggled(bool)));
  for (qSlicerAstroValueField* field : {this->ParameterFields[0], this->ParameterFields[1], this->ParameterFields[2],
                                        this->RotationFields[0], this->RotationFields[1], this->RotationFields[2],
                                        this->AccuracyField, this->KField, this->TimeStepField})
    {
    QObject::connect(field, SIGNAL(valueChanged(double)), q, SLOT(onParametersEdited()));
    }
  QObject::connect(this->RunButton, SIGNAL(clicked()), q, SLOT(onApply()));
  QObject::connect(this->RunButton, SIGNAL(cancelRequested()), q, SLOT(onComputationCancelled()));
  QObject::connect(this->ResultCard, SIGNAL(showRequested()), q, SLOT(onShowResult()));
  QObject::connect(this->InputVolumeNodeSelector, &qSlicerAstroDatasetSelector::currentNodeChanged,
                   q, [this](vtkMRMLNode*) { this->updateRunButton(); });

  theme->applyTo(q);
  this->updateFields(1);
  this->updateRunButton();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroSmoothingModuleWidget);

  if (!q->mrmlScene())
    {
    return;
    }
  // the segmentations and the Segment Editor node are shared with the other
  // modules (and are the user's data): only the references are dropped
  this->segmentEditorNode = nullptr;
  if (this->parametersNode)
    {
    q->mrmlScene()->RemoveNode(this->parametersNode);
    }
  this->parametersNode = nullptr;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic* qSlicerAstroSmoothingModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroSmoothingModuleWidget);
  return vtkSlicerAstroSmoothingLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
vtkMRMLAstroVolumeNode* qSlicerAstroSmoothingModuleWidgetPrivate::inputVolume() const
{
  Q_Q(const qSlicerAstroSmoothingModuleWidget);
  if (!this->parametersNode || !q->mrmlScene() || !this->parametersNode->GetInputVolumeNodeID())
    {
    return nullptr;
    }
  return vtkMRMLAstroVolumeNode::SafeDownCast(q->mrmlScene()->GetNodeByID(this->parametersNode->GetInputVolumeNodeID()));
}

//-----------------------------------------------------------------------------
bool qSlicerAstroSmoothingModuleWidgetPrivate::isModulePanel() const
{
  Q_Q(const qSlicerAstroSmoothingModuleWidget);
  return !q->property("astroEmbedded").toBool();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidgetPrivate::updateRunButton()
{
  const bool input = this->InputVolumeNodeSelector->currentNode() != nullptr;
  this->RunButton->setText(input ? qSlicerAstroSmoothingModuleWidget::tr("Smooth") :
                                   qSlicerAstroSmoothingModuleWidget::tr("Select data"));
  this->RunButton->setEnabled(input);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidgetPrivate::updateResult()
{
  Q_Q(qSlicerAstroSmoothingModuleWidget);
  vtkMRMLAstroVolumeNode* output = (this->parametersNode && q->mrmlScene() &&
    this->parametersNode->GetOutputVolumeNodeID()) ? vtkMRMLAstroVolumeNode::SafeDownCast(
      q->mrmlScene()->GetNodeByID(this->parametersNode->GetOutputVolumeNodeID())) : nullptr;
  const bool show = output && output->GetImageData() && output->GetName() &&
    std::string(output->GetName()).find("_Filtered_") != std::string::npos;
  if (show)
    {
    this->ResultCard->setVolume(output);
    }
  qSlicerAstroResultCard::setCardsVisible(QList<qSlicerAstroResultCard*>() << this->ResultCard,
                                          QList<bool>() << show);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidgetPrivate::updateBeam()
{
  vtkMRMLAstroVolumeNode* input = this->inputVolume();
  if (!input || !this->parametersNode)
    {
    this->BeamLabel->clear();
    return;
    }
  const double bmaj = StringToDouble(input->GetAttribute("SlicerAstro.BMAJ")) * 3600.;
  const double bmin = StringToDouble(input->GetAttribute("SlicerAstro.BMIN")) * 3600.;
  if (!(bmaj > 0.) || !(bmin > 0.))
    {
    this->BeamLabel->setText(qSlicerAstroSmoothingModuleWidget::tr("no beam in the header"));
    return;
    }
  const QString before = std::abs(bmaj - bmin) < 0.01 * bmaj ? Arcsec(bmaj) :
    QString::fromUtf8("%1 × %2").arg(Arcsec(bmaj), Arcsec(bmin));
  double newBeam[3], kernel[3];
  if (vtkSlicerAstroSmoothingLogic::ComputeSmoothedBeam(input, this->parametersNode, newBeam, kernel))
    {
    this->BeamLabel->setText(QString::fromUtf8("beam %1 → %2 × %3, PA %4°").arg(before, Arcsec(newBeam[0]),
      Arcsec(newBeam[1])).arg(newBeam[2], 0, 'f', 0));
    }
  else
    {
    this->BeamLabel->setText(QString::fromUtf8("beam %1 · unchanged (not a Gaussian kernel)").arg(before));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidgetPrivate::updateFields(int filter)
{
  const QString titles[3] = {qSlicerAstroSmoothingModuleWidget::tr("Box size"),
                             qSlicerAstroSmoothingModuleWidget::tr("Kernel FWHM"),
                             qSlicerAstroSmoothingModuleWidget::tr("Conductance")};
  this->KernelTitle->setText(titles[std::max(0, std::min(filter, 2))].toUpper());
  for (int i = 0; i < 3; i++)
    {
    qSlicerAstroValueField* field = this->ParameterFields[i];
    field->setUnit(filter == 2 ? QString() : qSlicerAstroSmoothingModuleWidget::tr("px"));
    field->setDecimals(filter == 0 ? 0 : 2);
    field->setSingleStep(filter == 2 ? 0.1 : 1.);
    const QString axis = QString("xyz").mid(i, 1);
    field->setToolTip(filter == 0 ? qSlicerAstroSmoothingModuleWidget::tr("Size of the box along %1 (voxels, odd).").arg(axis) :
                      filter == 1 ? qSlicerAstroSmoothingModuleWidget::tr("FWHM of the kernel along %1 (voxels); 0: no "
                                                                         "smoothing along %1.").arg(axis) :
                      qSlicerAstroSmoothingModuleWidget::tr("Conductance of the diffusion along %1; 0: no smoothing "
                                                            "along %1.").arg(axis));
    }
  this->RotationRow->setVisible(filter == 1);
  this->GradientRow->setVisible(filter == 2);
  this->AccuracyLabel->setVisible(filter != 0);
  this->AccuracyField->setVisible(filter != 0);
  if (filter == 1)
    {
    this->AccuracyLabel->setText(qSlicerAstroSmoothingModuleWidget::tr("Kernel extent"));
    this->AccuracyField->setUnit(QString::fromUtf8("σ"));
    this->AccuracyField->setRange(1., 5.);
    this->AccuracyField->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
      "Extent of the Gaussian kernel on each side of its centre, in sigma (3: 99.73% of the kernel)."));
    }
  else
    {
    this->AccuracyLabel->setText(qSlicerAstroSmoothingModuleWidget::tr("Iterations"));
    this->AccuracyField->setUnit(QString());
    this->AccuracyField->setRange(1., 200.);
    this->AccuracyField->setToolTip(qSlicerAstroSmoothingModuleWidget::tr(
      "Number of iterations of the diffusion (default 20)."));
    }
  this->AccuracyLabel->setToolTip(this->AccuracyField->toolTip());
}

//-----------------------------------------------------------------------------
// qSlicerAstroSmoothingModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModuleWidget::qSlicerAstroSmoothingModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroSmoothingModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModuleWidget::~qSlicerAstroSmoothingModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::enter()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  this->Superclass::enter();
  // the module panel compares the input and the smoothed data in two 3D
  // views (the Workbench, which embeds the panel, arranges its own views)
  qSlicerApplication* app = qSlicerApplication::application();
  if (d->isModulePanel() && app && app->layoutManager() && app->layoutManager()->layoutLogic() &&
      app->layoutManager()->layoutLogic()->GetLayoutNode())
    {
    app->layoutManager()->layoutLogic()->GetLayoutNode()->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutDual3DView);
    }
  this->onMRMLAstroSmoothingParametersNodeModified();
}

//----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::exit()
{
  this->Superclass::exit();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }
  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::setMRMLScene : selectionNode not found!";
    return;
    }

  this->initializeNodes();

  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent, this, SLOT(onEndCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::StartImportEvent, this, SLOT(onStartImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(onEndImportEvent()));
  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceAddedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceAdded(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceRemovedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceRemoved(vtkObject*)));

  this->onMRMLSelectionNodeModified(d->selectionNode);
  this->onMRMLSelectionNodeReferenceAdded(d->selectionNode);

  // the parameters (and so the input) may have been selected when the
  // selectors received the scene (e.g. a second panel, embedded in the
  // Workbench): the input is set up now
  if (d->InputVolumeNodeSelector->currentNode())
    {
    this->onInputVolumeChanged(d->InputVolumeNodeSelector->currentNode());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  this->initializeParameterNode(forceNew);
  this->initializeSegmentations(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  d->selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::onEndCloseEvent : selectionNode not found!";
    return;
    }
  this->initializeNodes(true);
  this->onMRMLAstroSmoothingParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  d->selectionNode = appLogic ? appLogic->GetSelectionNode() : nullptr;
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::onEndImportEvent : selectionNode not found!";
    return;
    }
  this->initializeNodes();
  this->onMRMLAstroSmoothingParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroSmoothingParametersNode *astroParametersNode = nullptr;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroSmoothingParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroSmoothingParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroSmoothingParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroSmoothingParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroSmoothingParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  Q_UNUSED(forceNew);

  if (!this->mrmlScene())
    {
    return;
    }
  // the Segment Editor shows the mask made by the smoothing
  d->segmentEditorNode = vtkMRMLSegmentEditorNode::SafeDownCast(
    this->mrmlScene()->GetSingletonNode("SegmentEditor", "vtkMRMLSegmentEditorNode"));
  if (!d->segmentEditorNode)
    {
    vtkNew<vtkMRMLSegmentEditorNode> segmentEditorNode;
    segmentEditorNode->SetSingletonTag("SegmentEditor");
    d->segmentEditorNode = vtkMRMLSegmentEditorNode::SafeDownCast(this->mrmlScene()->AddNode(segmentEditorNode));
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  vtkMRMLSelectionNode *selectionNode = vtkMRMLSelectionNode::SafeDownCast(sender);
  if (!selectionNode || !d->parametersNode)
    {
    return;
    }
  if (d->parametersNode->GetInputVolumeNodeID() && selectionNode->GetActiveVolumeID() &&
      !strcmp(d->parametersNode->GetInputVolumeNodeID(), selectionNode->GetActiveVolumeID()))
    {
    return;
    }
  d->parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
}

//--------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  vtkMRMLSelectionNode *selectionNode = vtkMRMLSelectionNode::SafeDownCast(sender);
  vtkMRMLSegmentEditorNode* segmentEditorNode = selectionNode ? vtkMRMLSegmentEditorNode::SafeDownCast(
    selectionNode->GetNodeReference("SegmentEditorNodeRef")) : nullptr;
  if (segmentEditorNode)
    {
    d->segmentEditorNode = segmentEditorNode;
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  this->onMRMLSelectionNodeReferenceAdded(sender);
}

//--------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::setMRMLAstroSmoothingParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  vtkMRMLAstroSmoothingParametersNode* parametersNode = vtkMRMLAstroSmoothingParametersNode::SafeDownCast(mrmlNode);
  if (!parametersNode)
    {
    return;
    }
  this->qvtkReconnect(d->parametersNode, parametersNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroSmoothingParametersNodeModified()));
  d->parametersNode = parametersNode;
  this->onMRMLAstroSmoothingParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode || !d->selectionNode)
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
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onInputVolumeModified()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  vtkMRMLAstroVolumeNode* input = d->inputVolume();
  if (!input || !this->mrmlScene() || this->mrmlScene()->IsClosing() || this->mrmlScene()->IsImporting())
    {
    return;
    }
  // the dataset line: beam and noise
  QStringList summary;
  const double bmaj = StringToDouble(input->GetAttribute("SlicerAstro.BMAJ")) * 3600.;
  if (bmaj > 0.)
    {
    summary << QString::fromUtf8("beam %1").arg(Arcsec(bmaj));
    }
  const double noise = input->GetDisplayThreshold();
  if (std::isfinite(noise) && noise > 0.)
    {
    const QString unit = qSlicerAstroTheme::displayUnit(QString::fromUtf8(input->GetAttribute("SlicerAstro.BUNIT")));
    summary << (unit.startsWith("Jy") && noise < 0.1 ?
      QString::fromUtf8("σ %1 m%2").arg(noise * 1000., 0, 'g', 3).arg(unit) :
      QString::fromUtf8("σ %1 %2").arg(noise, 0, 'g', 3).arg(unit));
    }
  d->InputVolumeNodeSelector->setSubtitle(summary.join(QString::fromUtf8(" · ")));
  d->updateBeam();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetOutputVolumeNodeID(mrmlNode ? mrmlNode->GetID() : nullptr);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onMRMLAstroSmoothingParametersNodeModified()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode || !this->mrmlScene() || d->Updating)
    {
    return;
    }
  d->Updating = true;
  vtkMRMLAstroSmoothingParametersNode* p = d->parametersNode;

  const char* inputID = p->GetInputVolumeNodeID();
  d->InputVolumeNodeSelector->setCurrentNode(inputID ? this->mrmlScene()->GetNodeByID(inputID) : nullptr);
  const char* outputID = p->GetOutputVolumeNodeID();
  d->OutputVolumeNodeSelector->setCurrentNode(outputID ? this->mrmlScene()->GetNodeByID(outputID) : nullptr);

  // the widgets show the parameters (signals blocked: the parameters are
  // never changed in response to the node)
  const int filter = std::max(0, std::min(p->GetFilter(), 2));
  if (d->FilterGroup->button(filter))
    {
    const QSignalBlocker blocker(d->FilterGroup);
    d->FilterGroup->button(filter)->setChecked(true);
    }
  d->updateFields(filter);
  const double parameters[3] = {p->GetParameterX(), p->GetParameterY(), p->GetParameterZ()};
  const double rotations[3] = {p->GetRx(), p->GetRy(), p->GetRz()};
  for (int i = 0; i < 3; i++)
    {
    const QSignalBlocker blockParameter(d->ParameterFields[i]);
    d->ParameterFields[i]->setValue(parameters[i]);
    const QSignalBlocker blockRotation(d->RotationFields[i]);
    d->RotationFields[i]->setValue(rotations[i]);
    }
    {
    const QSignalBlocker blockAccuracy(d->AccuracyField);
    d->AccuracyField->setValue(p->GetAccuracy());
    const QSignalBlocker blockK(d->KField);
    d->KField->setValue(p->GetK());
    const QSignalBlocker blockTimeStep(d->TimeStepField);
    d->TimeStepField->setValue(p->GetTimeStep());
    const QSignalBlocker blockLink(d->LinkCheckBox);
    d->LinkCheckBox->setChecked(p->GetLink());
    const QSignalBlocker blockAutoRun(d->AutoRunCheckBox);
    d->AutoRunCheckBox->setChecked(p->GetAutoRun());
    const QSignalBlocker blockMask(d->GenerateMaskCheckBox);
    d->GenerateMaskCheckBox->setChecked(p->GetMasksCommand() && !strcmp(p->GetMasksCommand(), "Generate"));
    }
  d->HardwareButton->setCurrentIndex(p->GetHardware() != 0 && d->HardwareButton->button(1)->isEnabled() ? 1 : 0);
  d->updateBeam();
  d->updateRunButton();
  d->updateResult();
  d->Updating = false;

  const int status = p->GetStatus();
  if (status == 0)
    {
    this->onComputationFinished();
    }
  else
    {
    if (status == 1)
      {
      this->onComputationStarted();
      }
    if (status != -1)
      {
      this->updateProgress(status);
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onFilterSelected(int index)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  // the default parameters of the filter chosen by the user
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetAutoRun(false);
  if (index == 0)
    {
    d->parametersNode->SetKernelLengthX(5);
    d->parametersNode->SetKernelLengthY(5);
    d->parametersNode->SetKernelLengthZ(5);
    }
  else if (index == 1)
    {
    d->parametersNode->SetAccuracy(3);
    d->parametersNode->SetRx(0.);
    d->parametersNode->SetRy(0.);
    d->parametersNode->SetRz(0.);
    }
  else
    {
    // Punzo et al. 2016: K = 1.5, tau = 0.0325, n = 20, Cx = Cy = Cz = 5
    d->parametersNode->SetAccuracy(20);
    d->parametersNode->SetTimeStep(0.0325);
    d->parametersNode->SetK(1.5);
    }
  d->parametersNode->SetParameterX(5);
  d->parametersNode->SetParameterY(5);
  d->parametersNode->SetParameterZ(5);
  d->parametersNode->SetFilter(index);
  d->parametersNode->SetGaussianKernels();
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onParametersEdited()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode || d->Updating)
    {
    return;
    }
  vtkMRMLAstroSmoothingParametersNode* p = d->parametersNode;
  int wasModifying = p->StartModify();
  double values[3] = {d->ParameterFields[0]->value(), d->ParameterFields[1]->value(), d->ParameterFields[2]->value()};
  if (p->GetLink())
    {
    // the field changed by the user sets the three values
    QObject* source = this->sender();
    for (int i = 0; i < 3; i++)
      {
      if (source == d->ParameterFields[i])
        {
        values[0] = values[1] = values[2] = d->ParameterFields[i]->value();
        }
      }
    }
  p->SetParameterX(values[0]);
  p->SetParameterY(values[1]);
  p->SetParameterZ(values[2]);
  if (p->GetFilter() == 0)
    {
    p->SetKernelLengthX(static_cast<int>(values[0]));
    p->SetKernelLengthY(static_cast<int>(values[1]));
    p->SetKernelLengthZ(static_cast<int>(values[2]));
    }
  p->SetRx(d->RotationFields[0]->value());
  p->SetRy(d->RotationFields[1]->value());
  p->SetRz(d->RotationFields[2]->value());
  p->SetAccuracy(static_cast<int>(d->AccuracyField->value()));
  p->SetK(d->KField->value());
  p->SetTimeStep(d->TimeStepField->value());
  p->SetGaussianKernels();
  if (p->GetAutoRun() && p->GetStatus() > 1)
    {
    p->SetStatus(-1);
    }
  p->EndModify(wasModifying);

  if (p->GetAutoRun() && p->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onHardwareChanged(int index)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (d->parametersNode && !d->Updating)
    {
    d->parametersNode->SetHardware(index);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onGenerateMaskToggled(bool generate)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (d->parametersNode)
    {
    d->parametersNode->SetMasksCommand(generate ? "Generate" : "Skip");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onLinkToggled(bool link)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (d->parametersNode)
    {
    d->parametersNode->SetLink(link);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onAutoRunToggled(bool autoRun)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (d->parametersNode)
    {
    d->parametersNode->SetAutoRun(autoRun);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onShowResult()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  // the result in place of its input in the views (the input stays active);
  // clicked again: the input back
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = d->logic() ? d->logic()->GetAstroVolumeLogic() : nullptr;
  vtkMRMLScene* scene = this->mrmlScene();
  if (!d->parametersNode || !astroVolumeLogic || !scene || !d->parametersNode->GetOutputVolumeNodeID() ||
      !d->parametersNode->GetInputVolumeNodeID())
    {
    return;
    }
  vtkMRMLAstroVolumeNode* output = vtkMRMLAstroVolumeNode::SafeDownCast(
    scene->GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));
  vtkMRMLAstroVolumeNode* input = vtkMRMLAstroVolumeNode::SafeDownCast(
    scene->GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  astroVolumeLogic->ToggleProductInViews(output, input);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onApply()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  // one smoothing at a time (e.g. a parameter changed while the OpenGL
  // filters run in this thread would apply the smoothing again)
  if (d->parametersNode->GetStatus() != 0)
    {
    return;
    }
  vtkSlicerAstroSmoothingLogic *logic = d->logic();
  vtkMRMLScene *scene = this->mrmlScene();
  if (!logic || !scene)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::onApply : logic or scene not found!";
    return;
    }

  d->Message->clearMessage();
  vtkMRMLAstroVolumeNode *inputVolume = d->inputVolume();
  if (!inputVolume || !inputVolume->GetImageData())
    {
    d->Message->showMessage("error", tr("Select the data to smooth."));
    return;
    }
  const int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  if (n < 1 || n > 3)
    {
    d->Message->showMessage("error", tr("Smoothing is available for data with 1, 2 or 3 axes."));
    return;
    }

  d->CancelRequested = false;
  d->parametersNode->SetStatus(1);
  const char* filterNames[3] = {"Box", "Gaussian", "Gradient"};
  const int filter = std::max(0, std::min(d->parametersNode->GetFilter(), 2));
  const int serial = d->parametersNode->GetOutputSerial();
  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_Filtered_" << filterNames[filter] << "_" << serial;
  d->parametersNode->SetOutputSerial(serial + 1);

  // the previous output is replaced (never the input)
  vtkMRMLAstroVolumeNode *outputVolume = d->parametersNode->GetOutputVolumeNodeID() ?
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->GetNodeByID(d->parametersNode->GetOutputVolumeNodeID())) : nullptr;
  if (outputVolume && outputVolume != inputVolume && outputVolume->GetName() &&
      std::string(outputVolume->GetName()).find("_Filtered_") != std::string::npos)
    {
    scene->RemoveNode(outputVolume->GetStorageNode());
    vtkMRMLVolumeRenderingDisplayNode *volumeRenderingDisplay =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(outputVolume->GetDisplayNode());
    if (volumeRenderingDisplay)
      {
      scene->RemoveNode(volumeRenderingDisplay->GetROINode());
      scene->RemoveNode(volumeRenderingDisplay);
      }
    scene->RemoveNode(outputVolume->GetDisplayNode());
    scene->RemoveNode(outputVolume);
    }

  outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast(logic->GetAstroVolumeLogic()->CloneVolume(
    scene, inputVolume, outSS.str().c_str(), /* cloneImageData = */ false));
  if (!outputVolume)
    {
    d->parametersNode->SetStatus(0);
    d->Message->showMessage("error", tr("The output data could not be created."));
    return;
    }
  d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());
  vtkMRMLNode* noPreset = nullptr;
  outputVolume->SetPresetNode(noPreset);
  for (int ii = outputVolume->GetNumberOfDisplayNodes() - 1; ii >= 0; ii--)
    {
    if (vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(outputVolume->GetNthDisplayNode(ii)))
      {
      outputVolume->RemoveNthDisplayNodeID(ii);
      }
    }
  vtkNew<vtkMatrix4x4> transformationMatrix;
  inputVolume->GetRASToIJKMatrix(transformationMatrix.GetPointer());
  outputVolume->SetRASToIJKMatrix(transformationMatrix.GetPointer());
  outputVolume->SetAndObserveTransformNodeID(inputVolume->GetTransformNodeID());

  // the OpenGL filters create their own offscreen OpenGL context: views
  // created and not rendered yet (e.g. the 3D views of the layout set when
  // the panel was entered, by a script) render first, with their contexts
  if (d->parametersNode->GetHardware() != 0 && qSlicerApplication::application())
    {
    qSlicerApplication::application()->processEvents();
    }
  // without blocking the user interface (progress in the run button); the
  // result is shown when the smoothing is done
  QPointer<qSlicerAstroSmoothingModuleWidget> self(this);
  vtkSmartPointer<vtkMRMLAstroVolumeNode> inputHold = inputVolume;
  vtkSmartPointer<vtkMRMLAstroVolumeNode> outputHold = outputVolume;
  const bool started = logic->StartApply(d->parametersNode, nullptr, [self, inputHold, outputHold](bool success)
    {
    // next turn of the event loop: the OpenGL filters call this before
    // releasing their offscreen context, and the views render in the finish
    QTimer::singleShot(0, self, [self, inputHold, outputHold, success]()
      {
      if (self)
        {
        self->finishSmoothing(success, inputHold, outputHold);
        }
      });
    });
  if (!started)
    {
    this->finishSmoothing(false, inputVolume, outputVolume);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::finishSmoothing(bool success, vtkMRMLAstroVolumeNode* inputVolume,
                                                       vtkMRMLAstroVolumeNode* outputVolume)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  vtkMRMLScene* scene = this->mrmlScene();
  vtkSlicerAstroSmoothingLogic* logic = d->logic();
  if (!scene || !logic || !d->parametersNode || !inputVolume || !outputVolume)
    {
    return;
    }
  if (!success)
    {
    const bool cancelled = d->CancelRequested;
    scene->RemoveNode(outputVolume);
    d->parametersNode->SetOutputVolumeNodeID(nullptr);
    inputVolume->SetDisplayVisibility(1);
    d->parametersNode->SetStatus(0);
    d->Message->showMessage(cancelled ? "info" : "error", cancelled ? tr("Computation cancelled.") :
      tr("The smoothing failed (see the error log)."));
    return;
    }

  const bool generateMask = d->parametersNode->GetMasksCommand() &&
    !strcmp(d->parametersNode->GetMasksCommand(), "Generate");
  QString maskName;
  if (d->isModulePanel() && d->astroVolumeWidget)
    {
    // the module panel compares the input and the output in 3D (and makes
    // the mask)
    d->astroVolumeWidget->setComparative3DViews(inputVolume->GetID(), outputVolume->GetID(), generateMask);
    }
  else if (generateMask)
    {
    // embedded (e.g. Workbench): the mask only, the views are not changed
    const double maskColor[3] = {0.31, 0.82, 0.88};
    vtkMRMLSegmentationNode* mask = logic->GetAstroVolumeLogic()->CreateThresholdMask(inputVolume, outputVolume,
      3. * outputVolume->GetDisplayThreshold(), (std::string(outputVolume->GetName()) + "_3RMS").c_str(), maskColor);
    if (mask && d->segmentEditorNode)
      {
      d->segmentEditorNode->SetAndObserveSegmentationNode(mask);
      }
    }
  d->parametersNode->SetStatus(0);
  d->Message->showMessage("success", generateMask ? tr("Smoothed data and mask created") : tr("Smoothed data created"));
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->RunButton->setRunning(false);
  d->updateResult();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->CancelRequested = true;
  if (d->parametersNode)
    {
    d->parametersNode->SetStatus(-1);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->RunButton->setProgress(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->RunButton->setProgressText(tr("Smoothing"));
  d->RunButton->setRunning(true);
}

//---------------------------------------------------------------------------
vtkMRMLAstroSmoothingParametersNode* qSlicerAstroSmoothingModuleWidget::
mrmlAstroSmoothingParametersNode()const
{
  Q_D(const qSlicerAstroSmoothingModuleWidget);
  return d->parametersNode;
}
