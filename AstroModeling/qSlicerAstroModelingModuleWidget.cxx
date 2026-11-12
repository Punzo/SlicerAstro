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
#include <QDebug>
#include <QSpinBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QFormLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QProgressBar>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QMutexLocker>
#include <QPushButton>
#include <ctkMessageBox.h>
#include <QSignalBlocker>
#include <ctkCollapsibleButton.h>
#include <QDateTime>
#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QStringList>
#include <QThread>
#include <QTimer>

// CTK includes
#include <ctkFlowLayout.h>

// VTK includes
#include <vtkCollection.h>
#include <vtkDoubleArray.h>
#include <vtkIdTypeArray.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkGeneralTransform.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPlot.h>
#include <vtkPointData.h>
#include <vtksys/SystemTools.hxx>
#include <vtkStringArray.h>
#include <vtkTable.h>
#include <vtkTransform.h>

// SlicerQt includesS
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>

// AstroModeling includes
#include "qSlicerAstroModelingModuleWidget.h"
#include "ui_qSlicerAstroModelingModuleWidget.h"
#include "qSlicerAstroModelingModuleWorker.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroModelingLogic.h>
#include <vtkSlicerMarkupsLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>

// qMRML includes
#include <qMRMLPlotView.h>
#include <qMRMLPlotWidget.h>
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSortFilterProxyModel.h>
#include <qMRMLNodeComboBox.h>

// MRMLLogic includes
#include <vtkSlicerApplicationLogic.h>
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLPlotSeriesNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLPlotViewNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#include <chrono>
#include <utility>
#include <sstream>
#include <iomanip>

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroModeling
class qSlicerAstroModelingModuleWidgetPrivate: public Ui_qSlicerAstroModelingModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroModelingModuleWidget);
protected:
  qSlicerAstroModelingModuleWidget* const q_ptr;
public:

  qSlicerAstroModelingModuleWidgetPrivate(qSlicerAstroModelingModuleWidget& object);
  ~qSlicerAstroModelingModuleWidgetPrivate();
  void init();
  void cleanPointers();
  // a 3DBarolo operation is running in the worker thread
  bool workerRunning() const;
  // segment of the contours segmentation of the module (data, model, region
  // of the model): the segmentation node if the segment exists
  vtkMRMLSegmentationNode* modelingSegment(vtkMRMLScene* scene, const char* segmentID) const;
  // Calculate and Visualize Model: progress below its button (the Fitting
  // parameters section, with the other progress bar, is often collapsed)
  QProgressBar* calculateProgressBar = nullptr;
  bool calculatingModel = false;
  // Output of 3DBarolo and of the operations (also in the terminal and in
  // the application log)
  ctkCollapsibleButton* outputLogCollapsibleButton = nullptr;
  QPlainTextEdit* outputLogTextEdit = nullptr;
  void appendOutputLog(const QString& text);
  void appendOperationHeader(int operation);

  vtkSlicerAstroModelingLogic* logic() const;
  qSlicerAstroVolumeModuleWidget* astroVolumeWidget;
  /// Beam (FWHM major and minor axes in arcsec, position angle in degrees)
  QLabel* BeamLabel{nullptr};
  QDoubleSpinBox* BeamMajorSpinBox{nullptr};
  QDoubleSpinBox* BeamMinorSpinBox{nullptr};
  QDoubleSpinBox* BeamPASpinBox{nullptr};

  /// 3DBarolo parameters of the Advanced section
  /// Initial parameters (and their estimate): Manual mode only
  QWidget* InitialParametersWidget{nullptr};
  QCheckBox* TwoStageCheckBox{nullptr};
  QComboBox* RegularizationComboBox{nullptr};
  QComboBox* SideComboBox{nullptr};
  QDoubleSpinBox* InstrumentalBroadeningSpinBox{nullptr};
  QDoubleSpinBox* RotationVelocityErrorSpinBox{nullptr};
  QDoubleSpinBox* MinimumDispersionSpinBox{nullptr};
  QDoubleSpinBox* MaximumDispersionSpinBox{nullptr};
  QCheckBox* EstimateErrorsCheckBox{nullptr};
  QSpinBox* StartRingSpinBox{nullptr};
  QDoubleSpinBox* BlankWeightSpinBox{nullptr};
  QCheckBox* NormalizeCubeCheckBox{nullptr};
  QDoubleSpinBox* RedshiftSpinBox{nullptr};
  QLineEdit* RestFrequenciesLineEdit{nullptr};
  QLineEdit* RelativeIntensitiesLineEdit{nullptr};
  QGroupBox* SearchGroupBox{nullptr};
  QDoubleSpinBox* SearchSNRCutSpinBox{nullptr};
  QDoubleSpinBox* SearchThresholdSpinBox{nullptr};
  QSpinBox* SearchMinChannelsSpinBox{nullptr};
  QDoubleSpinBox* SearchGrowthCutSpinBox{nullptr};
  vtkSmartPointer<vtkMRMLAstroModelingParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLTableNode> internalTableNode;
  vtkSmartPointer<vtkMRMLTableNode> astroTableNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeVRot;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeVRad;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeInc;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodePhi;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeVSys;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeVDisp;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeDens;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeZ0;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeXPos;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeYPos;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeVRot;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeVRad;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeInc;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodePhi;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeVSys;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeVDisp;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeDens;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeZ0;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeXPos;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeYPos;
  vtkSmartPointer<vtkMRMLMarkupsFiducialNode> fiducialNodeMajor;
  vtkSmartPointer<vtkMRMLMarkupsFiducialNode> fiducialNodeMinor;

  qSlicerAstroModelingModuleWorker *worker;
  QThread *thread;
  /// Parameters of the worker thread: a copy of the parameter node, outside
  /// the scene and without observers (the widgets and the views must not be
  /// updated from the worker thread). The results are copied to the
  /// parameter node in the GUI thread (onWorkFinished).
  vtkSmartPointer<vtkMRMLAstroModelingParametersNode> workerParametersNode;
  QTimer* workerProgressTimer{nullptr};
  /// The model and the data are shown again at the contour level when it
  /// stops changing
  QTimer* contourLevelTimer{nullptr};
  /// The input is converted to the optical velocity definition for 3DBarolo
  /// (in the GUI thread, the WCS is shared with the views) and restored
  bool restoreRadioVelocity{false};
  QAction *CopyAction;
  QAction *PasteAction;
  QAction *PlotAction;
};

//-----------------------------------------------------------------------------
// qSlicerAstroModelingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidgetPrivate::qSlicerAstroModelingModuleWidgetPrivate(qSlicerAstroModelingModuleWidget& object)
  : q_ptr(&object)
{
  this->astroVolumeWidget = 0;
  this->parametersNode = 0;
  this->internalTableNode = 0;
  this->astroTableNode = 0;
  this->selectionNode = 0;
  this->segmentEditorNode = 0;
  this->plotChartNodeVRot = 0;
  this->plotChartNodeVRad = 0;
  this->plotChartNodeInc = 0;
  this->plotChartNodePhi = 0;
  this->plotChartNodeVSys = 0;
  this->plotChartNodeVDisp = 0;
  this->plotChartNodeDens = 0;
  this->plotChartNodeZ0 = 0;
  this->plotChartNodeXPos = 0;
  this->plotChartNodeYPos = 0;
  this->PlotSeriesNodeVRot = 0;
  this->PlotSeriesNodeVRad = 0;
  this->PlotSeriesNodeInc = 0;
  this->PlotSeriesNodePhi = 0;
  this->PlotSeriesNodeVSys = 0;
  this->PlotSeriesNodeVDisp = 0;
  this->PlotSeriesNodeDens = 0;
  this->PlotSeriesNodeZ0 = 0;
  this->PlotSeriesNodeXPos = 0;
  this->PlotSeriesNodeYPos = 0;
  this->fiducialNodeMajor = 0;
  this->fiducialNodeMinor = 0;
  this->worker = 0;
  this->thread = 0;
  this->CopyAction = 0;
  this->PasteAction = 0;
  this->PlotAction = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidgetPrivate::~qSlicerAstroModelingModuleWidgetPrivate()
{
  if (this->astroVolumeWidget)
    {
    delete this->astroVolumeWidget;
    }

  if (this->worker)
    {
    this->worker->abort();
    delete this->worker;
    }

  if (this->thread)
    {
    this->thread->wait();
    delete this->thread;
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroModelingModuleWidget);

  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroModelingModuleWidgetPrivate::init(): could not find qSlicerApplication!";
    return;
    }

  qSlicerAbstractCoreModule* astroVolume = app->moduleManager()->module("AstroVolume");
  if (!astroVolume)
    {
    qCritical() << "qSlicerAstroModelingModuleWidgetPrivate::init(): could not find AstroVolume module!";
    return;
    }

  this->astroVolumeWidget = dynamic_cast<qSlicerAstroVolumeModuleWidget*>
    (astroVolume->widgetRepresentation());

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroModelingParametersNode(vtkMRMLNode*)));

  QObject::connect(this->TableNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onTableNodeChanged(vtkMRMLNode*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->OutputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->ResidualVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onResidualVolumeChanged(vtkMRMLNode*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SegmentsTableView, SLOT(setMRMLScene(vtkMRMLScene*)));

  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);

  QObject::connect(this->MaskCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onMaskActiveToggled(bool)));

  this->ManualModeRadioButton->setChecked(true);
  this->AutomaticModeRadioButton->setChecked(true);

  QObject::connect(this->ManualModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(this->AutomaticModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(this->RingsSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onNumberOfRingsChanged(double)));

  QObject::connect(this->RingWidthSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onRadSepChanged(double)));

  QObject::connect(this->XcenterSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onXCenterChanged(double)));

  QObject::connect(this->YcenterSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onYCenterChanged(double)));

  QObject::connect(this->SysVelSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onSystemicVelocityChanged(double)));

  QObject::connect(this->RotVelSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onRotationVelocityChanged(double)));

  QObject::connect(this->RadVelSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onRadialVelocityChanged(double)));

  QObject::connect(this->VerVelSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onVerticalVelocityChanged(double)));

  QObject::connect(this->DVDZSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onVerticalRotationalGradientChanged(double)));

  QObject::connect(this->ZCYLSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onVerticalRotationalGradientHeightChanged(double)));

  QObject::connect(this->VelDispSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onVelocityDispersionChanged(double)));

  QObject::connect(this->InclinationSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onInclinationChanged(double)));

  QObject::connect(this->InclinationErrorSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onInclinationErrorChanged(double)));

  QObject::connect(this->PASliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onPositionAngleChanged(double)));

  QObject::connect(this->PAErrorSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onPositionAngleErrorChanged(double)));

  QObject::connect(this->SHSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onScaleHeightChanged(double)));

  QObject::connect(this->CDSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onColumnDensityChanged(double)));

  QObject::connect(this->DistanceSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onDistanceChanged(double)));

  QObject::connect(this->PARadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onPositionAngleFitChanged(bool)));

  QObject::connect(this->VROTRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onRotationVelocityFitChanged(bool)));

  QObject::connect(this->VRadRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onRadialVelocityFitChanged(bool)));

  QObject::connect(this->DISPRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onVelocityDispersionFitChanged(bool)));

  QObject::connect(this->INCRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onInclinationFitChanged(bool)));

  QObject::connect(this->XCenterRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onXCenterFitChanged(bool)));

  QObject::connect(this->YCenterRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onYCenterFitChanged(bool)));

  QObject::connect(this->VSYSRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onSystemicVelocityFitChanged(bool)));

  QObject::connect(this->SCRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onScaleHeightFitChanged(bool)));

  QObject::connect(this->ADRIFTCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onADRIFTCorrectionChanged(bool)));

  QObject::connect(this->LayerTypeComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onLayerTypeChanged(int)));

  QObject::connect(this->FittingFunctionComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onFittingFunctionChanged(int)));

  QObject::connect(this->WeightingFunctionComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onWeightingFunctionChanged(int)));

  QObject::connect(this->NumCloudsSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onNumberOfCloundsChanged(double)));

  QObject::connect(this->CloudCDSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onCloudsColumnDensityChanged(double)));

  QObject::connect(this->TolleranceSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onTolleranceChanged(double)));

  QObject::connect(this->ContourSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onContourLevelChanged(double)));

  QObject::connect(this->CleanInitialParametersPushButton, SIGNAL(clicked()),
                   q, SLOT(onCleanInitialParameters()));

  QObject::connect(this->EstimateInitialParametersPushButton, SIGNAL(clicked()),
                   q, SLOT(onEstimateInitialParameters()));

  QObject::connect(this->NormalizeNonePushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onNormalizeNoneChanged(bool)));

  QObject::connect(this->NormalizeLocalPushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onNormalizeLocalChanged(bool)));

  QObject::connect(this->NormalizeAzimPushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onNormalizeAzimChanged(bool)));

  QObject::connect(this->FitPushButton, SIGNAL(clicked()),
                   q, SLOT(onFit()));

  QObject::connect(this->CreatePushButton, SIGNAL(clicked()),
                   q, SLOT(onCreate()));

  QObject::connect(this->CancelPushButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));

  QObject::connect(this->VisualizePushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onShowModelToggled(bool)));

  QObject::connect(this->VisualizeMaskPushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onShowModelMaskToggled(bool)));

  this->contourLevelTimer = new QTimer(q);
  this->contourLevelTimer->setSingleShot(true);
  this->contourLevelTimer->setInterval(300);
  QObject::connect(this->contourLevelTimer, SIGNAL(timeout()), q, SLOT(onVisualize()));

  QObject::connect(this->CalculatePushButton, SIGNAL(clicked()),
                   q, SLOT(onCalculateAndVisualize()));

  QObject::connect(this->YellowSliceSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onYellowSliceRotated(double)));

  QObject::connect(this->GreenSliceSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onGreenSliceRotated(double)));

  // Beam of the data (from the FITS header, or estimated from the noise)
  QWidget* beamWidget = new QWidget(this->FittingParametersCollapsibleButton);
  QHBoxLayout* beamLayout = new QHBoxLayout(beamWidget);
  beamLayout->setContentsMargins(0, 0, 0, 0);
  const QString beamToolTip = qSlicerAstroModelingModuleWidget::tr(
    "Beam of the data (BMAJ, BMIN, BPA): FWHM of the major and minor axes in arcsec, position "
    "angle of the major axis in degrees (from North to East). Read from the FITS header; if the "
    "header has no beam, it is estimated from the noise and can be edited.");
  this->BeamMajorSpinBox = new QDoubleSpinBox(beamWidget);
  this->BeamMinorSpinBox = new QDoubleSpinBox(beamWidget);
  this->BeamPASpinBox = new QDoubleSpinBox(beamWidget);
  QDoubleSpinBox* beamSpinBoxes[3] = {this->BeamMajorSpinBox, this->BeamMinorSpinBox, this->BeamPASpinBox};
  const char* beamPrefixes[3] = {"major ", "minor ", "PA "};
  for (int i = 0; i < 3; i++)
    {
    beamSpinBoxes[i]->setDecimals(2);
    beamSpinBoxes[i]->setRange(i < 2 ? 0. : -180., i < 2 ? 36000. : 180.);
    beamSpinBoxes[i]->setPrefix(QString::fromUtf8(beamPrefixes[i]));
    beamSpinBoxes[i]->setSuffix(i < 2 ? QString(" arcsec") : QString(" deg"));
    beamSpinBoxes[i]->setToolTip(beamToolTip);
    beamLayout->addWidget(beamSpinBoxes[i]);
    }
  this->BeamLabel = new QLabel(qSlicerAstroModelingModuleWidget::tr("Beam:"), this->FittingParametersCollapsibleButton);
  this->BeamLabel->setToolTip(beamToolTip);
  this->formLayout_2->insertRow(0, this->BeamLabel, beamWidget);

  // The initial parameters rows (from the Clean/Estimate buttons to the
  // column density) in a widget, hidden in Automatic mode without leaving
  // the spacing of the rows
  int firstRow = -1, lastRow = -1;
  QFormLayout::ItemRole role = QFormLayout::LabelRole;
  this->formLayout_2->getWidgetPosition(this->RingsLabel, &firstRow, &role);
  this->formLayout_2->getWidgetPosition(this->CDLabel, &lastRow, &role);
  if (firstRow > 0 && lastRow >= firstRow)
    {
    firstRow -= 1; // Clean and Estimate buttons
    this->InitialParametersWidget = new QWidget(this->FittingParametersCollapsibleButton);
    QFormLayout* initialLayout = new QFormLayout(this->InitialParametersWidget);
    initialLayout->setContentsMargins(0, 0, 0, 0);
    initialLayout->setFieldGrowthPolicy(this->formLayout_2->fieldGrowthPolicy());
    for (int row = firstRow; row <= lastRow; row++)
      {
      QFormLayout::TakeRowResult taken = this->formLayout_2->takeRow(firstRow);
      QWidget* label = taken.labelItem ? taken.labelItem->widget() : nullptr;
      QLayout* fieldLayout = taken.fieldItem ? taken.fieldItem->layout() : nullptr;
      QWidget* fieldWidget = (taken.fieldItem && !fieldLayout) ? taken.fieldItem->widget() : nullptr;
      if (label && fieldLayout)
        {
        initialLayout->addRow(label, fieldLayout);
        }
      else if (label && fieldWidget)
        {
        initialLayout->addRow(label, fieldWidget);
        }
      else if (fieldLayout)
        {
        initialLayout->addRow(fieldLayout);
        }
      else if (fieldWidget)
        {
        initialLayout->addRow(fieldWidget);
        }
      // the widget items are not needed (the layout items are the layouts)
      if (taken.labelItem && !taken.labelItem->layout())
        {
        delete taken.labelItem;
        }
      if (taken.fieldItem && !fieldLayout)
        {
        delete taken.fieldItem;
        }
      }
    this->formLayout_2->insertRow(firstRow, this->InitialParametersWidget);
    }

  // 3DBarolo parameters (Advanced section)
  QWidget* advanced = this->AdvancedCollapsibleButton;
  QLabel* fitTitle = new QLabel(qSlicerAstroModelingModuleWidget::tr("<b>3DBarolo fit</b>"), advanced);
  this->formLayout_3->addRow(fitTitle);

  QWidget* secondStageWidget = new QWidget(advanced);
  QHBoxLayout* secondStageLayout = new QHBoxLayout(secondStageWidget);
  secondStageLayout->setContentsMargins(0, 0, 0, 0);
  this->TwoStageCheckBox = new QCheckBox(qSlicerAstroModelingModuleWidget::tr("regularization:"), secondStageWidget);
  this->TwoStageCheckBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Second fit stage (TWOSTAGE): the inclination and position angle of the first stage are regularized "
    "and the other parameters are fitted again."));
  this->RegularizationComboBox = new QComboBox(secondStageWidget);
  this->RegularizationComboBox->setEditable(true);
  this->RegularizationComboBox->addItems(QStringList() << "auto" << "bezier" << "median" << "0" << "1" << "2" << "3");
  this->RegularizationComboBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Regularization of the second stage (REGTYPE): auto, bezier, median, or the degree of a polynomial."));
  secondStageLayout->addWidget(this->TwoStageCheckBox);
  secondStageLayout->addWidget(this->RegularizationComboBox, 1);
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("Second stage:"), secondStageWidget);

  this->SideComboBox = new QComboBox(advanced);
  this->SideComboBox->addItem(qSlicerAstroModelingModuleWidget::tr("Both"), "B");
  this->SideComboBox->addItem(qSlicerAstroModelingModuleWidget::tr("Approaching"), "A");
  this->SideComboBox->addItem(qSlicerAstroModelingModuleWidget::tr("Receding"), "R");
  this->SideComboBox->setToolTip(qSlicerAstroModelingModuleWidget::tr("Side of the galaxy fitted (SIDE)."));
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("Side:"), this->SideComboBox);

  this->InstrumentalBroadeningSpinBox = new QDoubleSpinBox(advanced);
  this->InstrumentalBroadeningSpinBox->setRange(-1., 100.);
  this->InstrumentalBroadeningSpinBox->setDecimals(2);
  this->InstrumentalBroadeningSpinBox->setSingleStep(0.1);
  this->InstrumentalBroadeningSpinBox->setSpecialValueText(qSlicerAstroModelingModuleWidget::tr("3DBarolo default"));
  this->InstrumentalBroadeningSpinBox->setSuffix(qSlicerAstroModelingModuleWidget::tr(" channels"));
  this->InstrumentalBroadeningSpinBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Instrumental spectral broadening (LINEAR), in channels: e.g. for Hanning smoothed data."));
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("Spectral resolution:"), this->InstrumentalBroadeningSpinBox);

  this->RotationVelocityErrorSpinBox = new QDoubleSpinBox(advanced);
  this->RotationVelocityErrorSpinBox->setRange(0., 10000.);
  this->RotationVelocityErrorSpinBox->setDecimals(1);
  this->RotationVelocityErrorSpinBox->setPrefix(QString::fromUtf8("\xC2\xB1 "));
  this->RotationVelocityErrorSpinBox->setSuffix(" km/s");
  this->RotationVelocityErrorSpinBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Maximum variation of the rotation velocity from the initial value in the fit (DELTAVROT)."));
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("Rotation velocity:"), this->RotationVelocityErrorSpinBox);

  QWidget* dispersionWidget = new QWidget(advanced);
  QHBoxLayout* dispersionLayout = new QHBoxLayout(dispersionWidget);
  dispersionLayout->setContentsMargins(0, 0, 0, 0);
  this->MinimumDispersionSpinBox = new QDoubleSpinBox(dispersionWidget);
  this->MaximumDispersionSpinBox = new QDoubleSpinBox(dispersionWidget);
  QDoubleSpinBox* dispersionSpinBoxes[2] = {this->MinimumDispersionSpinBox, this->MaximumDispersionSpinBox};
  for (QDoubleSpinBox* spinBox : dispersionSpinBoxes)
    {
    spinBox->setRange(0., 10000.);
    spinBox->setDecimals(1);
    spinBox->setSuffix(" km/s");
    dispersionLayout->addWidget(spinBox);
    }
  this->MinimumDispersionSpinBox->setPrefix(qSlicerAstroModelingModuleWidget::tr("min "));
  this->MaximumDispersionSpinBox->setPrefix(qSlicerAstroModelingModuleWidget::tr("max "));
  dispersionWidget->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Range of the velocity dispersion in the fit (MINVDISP, MAXVDISP)."));
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("Dispersion range:"), dispersionWidget);

  this->StartRingSpinBox = new QSpinBox(advanced);
  this->StartRingSpinBox->setRange(0, 10000);
  this->StartRingSpinBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "First ring fitted (STARTRAD, from 0): the inner rings keep their initial values."));
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("First ring:"), this->StartRingSpinBox);

  this->BlankWeightSpinBox = new QDoubleSpinBox(advanced);
  this->BlankWeightSpinBox->setRange(0., 10.);
  this->BlankWeightSpinBox->setDecimals(2);
  this->BlankWeightSpinBox->setSingleStep(0.1);
  this->BlankWeightSpinBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Power of the weighting function of the blank pixels in the fit (BWEIGHT)."));
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("Blank pixels weight:"), this->BlankWeightSpinBox);

  QWidget* optionsWidget = new QWidget(advanced);
  QHBoxLayout* optionsLayout = new QHBoxLayout(optionsWidget);
  optionsLayout->setContentsMargins(0, 0, 0, 0);
  this->NormalizeCubeCheckBox = new QCheckBox(qSlicerAstroModelingModuleWidget::tr("normalize the cube"), optionsWidget);
  this->NormalizeCubeCheckBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Normalize the flux values of the cube before the fit (NORMALCUBE)."));
  this->EstimateErrorsCheckBox = new QCheckBox(qSlicerAstroModelingModuleWidget::tr("estimate the errors (slow)"), optionsWidget);
  this->EstimateErrorsCheckBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Estimate the errors of the fitted parameters (FLAGERRORS): much slower fit."));
  optionsLayout->addWidget(this->NormalizeCubeCheckBox);
  optionsLayout->addWidget(this->EstimateErrorsCheckBox);
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("Options:"), optionsWidget);

  this->RedshiftSpinBox = new QDoubleSpinBox(advanced);
  this->RedshiftSpinBox->setRange(0., 20.);
  this->RedshiftSpinBox->setDecimals(6);
  this->RedshiftSpinBox->setSingleStep(0.001);
  this->RedshiftSpinBox->setToolTip(qSlicerAstroModelingModuleWidget::tr("Redshift of the galaxy (REDSHIFT)."));
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("Redshift:"), this->RedshiftSpinBox);

  QWidget* linesWidget = new QWidget(advanced);
  QHBoxLayout* linesLayout = new QHBoxLayout(linesWidget);
  linesLayout->setContentsMargins(0, 0, 0, 0);
  this->RestFrequenciesLineEdit = new QLineEdit(linesWidget);
  this->RestFrequenciesLineEdit->setPlaceholderText(qSlicerAstroModelingModuleWidget::tr("rest frequencies (Hz) from the header"));
  this->RestFrequenciesLineEdit->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Rest frequencies of the emission lines in Hz, comma separated (RESTFREQ). Empty: the rest frequency of the header."));
  this->RelativeIntensitiesLineEdit = new QLineEdit(linesWidget);
  this->RelativeIntensitiesLineEdit->setPlaceholderText(qSlicerAstroModelingModuleWidget::tr("relative intensities"));
  this->RelativeIntensitiesLineEdit->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Relative intensities of the emission lines, comma separated (RELINT; 1 for each line if empty)."));
  linesLayout->addWidget(this->RestFrequenciesLineEdit, 2);
  linesLayout->addWidget(this->RelativeIntensitiesLineEdit, 1);
  this->formLayout_3->addRow(qSlicerAstroModelingModuleWidget::tr("Emission lines:"), linesWidget);

  // source search: only without a mask
  this->SearchGroupBox = new QGroupBox(qSlicerAstroModelingModuleWidget::tr("Source search (without mask)"), advanced);
  this->SearchGroupBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Without a mask, 3DBarolo searches the galaxy in the cube with these parameters."));
  QFormLayout* searchLayout = new QFormLayout(this->SearchGroupBox);
  this->SearchSNRCutSpinBox = new QDoubleSpinBox(this->SearchGroupBox);
  this->SearchSNRCutSpinBox->setRange(0.5, 100.);
  this->SearchSNRCutSpinBox->setDecimals(1);
  this->SearchSNRCutSpinBox->setSuffix(QString::fromUtf8(" \xCF\x83"));
  this->SearchSNRCutSpinBox->setToolTip(qSlicerAstroModelingModuleWidget::tr("Signal to noise of the detections (SNRCUT)."));
  searchLayout->addRow(qSlicerAstroModelingModuleWidget::tr("Detection:"), this->SearchSNRCutSpinBox);
  this->SearchThresholdSpinBox = new QDoubleSpinBox(this->SearchGroupBox);
  this->SearchThresholdSpinBox->setRange(0., 1.E6);
  this->SearchThresholdSpinBox->setDecimals(6);
  this->SearchThresholdSpinBox->setSpecialValueText(qSlicerAstroModelingModuleWidget::tr("from the signal to noise"));
  this->SearchThresholdSpinBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Flux threshold of the detections in the units of the data (THRESHOLD); 0 to use the signal to noise."));
  searchLayout->addRow(qSlicerAstroModelingModuleWidget::tr("Threshold:"), this->SearchThresholdSpinBox);
  this->SearchMinChannelsSpinBox = new QSpinBox(this->SearchGroupBox);
  this->SearchMinChannelsSpinBox->setRange(1, 10000);
  this->SearchMinChannelsSpinBox->setToolTip(qSlicerAstroModelingModuleWidget::tr("Minimum channels of a source (MINCHANNELS)."));
  searchLayout->addRow(qSlicerAstroModelingModuleWidget::tr("Minimum channels:"), this->SearchMinChannelsSpinBox);
  this->SearchGrowthCutSpinBox = new QDoubleSpinBox(this->SearchGroupBox);
  this->SearchGrowthCutSpinBox->setRange(0., 100.);
  this->SearchGrowthCutSpinBox->setDecimals(1);
  this->SearchGrowthCutSpinBox->setSuffix(QString::fromUtf8(" \xCF\x83"));
  this->SearchGrowthCutSpinBox->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "Signal to noise down to which the detections are grown (GROWTHCUT)."));
  searchLayout->addRow(qSlicerAstroModelingModuleWidget::tr("Growth:"), this->SearchGrowthCutSpinBox);
  this->formLayout_3->addRow(this->SearchGroupBox);

  // any change: parameter node
  QObject::connect(this->TwoStageCheckBox, SIGNAL(toggled(bool)), q, SLOT(onBBaroloParameterChanged()));
  QObject::connect(this->RegularizationComboBox, SIGNAL(currentTextChanged(QString)), q, SLOT(onBBaroloParameterChanged()));
  QObject::connect(this->SideComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onBBaroloParameterChanged()));
  QObject::connect(this->NormalizeCubeCheckBox, SIGNAL(toggled(bool)), q, SLOT(onBBaroloParameterChanged()));
  QObject::connect(this->EstimateErrorsCheckBox, SIGNAL(toggled(bool)), q, SLOT(onBBaroloParameterChanged()));
  QObject::connect(this->RestFrequenciesLineEdit, SIGNAL(editingFinished()), q, SLOT(onBBaroloParameterChanged()));
  QObject::connect(this->RelativeIntensitiesLineEdit, SIGNAL(editingFinished()), q, SLOT(onBBaroloParameterChanged()));
  QObject::connect(this->StartRingSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onBBaroloParameterChanged()));
  QObject::connect(this->SearchMinChannelsSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onBBaroloParameterChanged()));
  QDoubleSpinBox* parameterSpinBoxes[] = {this->InstrumentalBroadeningSpinBox, this->RotationVelocityErrorSpinBox,
    this->MinimumDispersionSpinBox, this->MaximumDispersionSpinBox, this->BlankWeightSpinBox, this->RedshiftSpinBox,
    this->SearchSNRCutSpinBox, this->SearchThresholdSpinBox, this->SearchGrowthCutSpinBox};
  for (QDoubleSpinBox* spinBox : parameterSpinBoxes)
    {
    QObject::connect(spinBox, SIGNAL(valueChanged(double)), q, SLOT(onBBaroloParameterChanged()));
    }

  this->InputSegmentCollapsibleButton->setCollapsed(true);
  this->FittingParametersCollapsibleButton->setCollapsed(false);
  this->OutputCollapsibleButton->setCollapsed(true);
  this->OutputCollapsibleButton_2->setCollapsed(true);

  this->progressBar->hide();
  this->progressBar->setMinimum(0);
  this->progressBar->setMaximum(100);
  this->CancelPushButton->hide();

  this->calculateProgressBar = new QProgressBar(q);
  this->calculateProgressBar->setObjectName("CalculateProgressBar");
  this->calculateProgressBar->setMinimum(0);
  this->calculateProgressBar->setMaximum(100);
  this->calculateProgressBar->setTextVisible(true);
  this->calculateProgressBar->setMinimumHeight(35);
  this->calculateProgressBar->hide();
  QGridLayout* calculateLayout = qobject_cast<QGridLayout*>(this->CalculatePushButton->parentWidget()->layout());
  const int calculateIndex = calculateLayout ? calculateLayout->indexOf(this->CalculatePushButton) : -1;
  if (calculateIndex >= 0)
    {
    int row = 0, column = 0, rowSpan = 0, columnSpan = 0;
    calculateLayout->getItemPosition(calculateIndex, &row, &column, &rowSpan, &columnSpan);
    calculateLayout->addWidget(this->calculateProgressBar, row + 1, column, 1, columnSpan);
    }

  this->outputLogCollapsibleButton = new ctkCollapsibleButton(q);
  this->outputLogCollapsibleButton->setObjectName("OutputLogCollapsibleButton");
  this->outputLogCollapsibleButton->setText(qSlicerAstroModelingModuleWidget::tr("3DBarolo Output"));
  this->outputLogCollapsibleButton->setCollapsed(true);
  QVBoxLayout* outputLogLayout = new QVBoxLayout(this->outputLogCollapsibleButton);
  this->outputLogTextEdit = new QPlainTextEdit(this->outputLogCollapsibleButton);
  this->outputLogTextEdit->setObjectName("OutputLogTextEdit");
  this->outputLogTextEdit->setReadOnly(true);
  this->outputLogTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
  this->outputLogTextEdit->setMaximumBlockCount(20000);
  this->outputLogTextEdit->setMinimumHeight(200);
  this->outputLogTextEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  this->outputLogTextEdit->setToolTip(qSlicerAstroModelingModuleWidget::tr(
    "What 3DBarolo prints during the operations (also printed in the terminal), "
    "and the errors and warnings of the operations (also in the application log)."));
  outputLogLayout->addWidget(this->outputLogTextEdit);
  QPushButton* clearOutputLogButton = new QPushButton(qSlicerAstroModelingModuleWidget::tr("Clear"),
                                                      this->outputLogCollapsibleButton);
  clearOutputLogButton->setObjectName("ClearOutputLogPushButton");
  QObject::connect(clearOutputLogButton, SIGNAL(clicked()), this->outputLogTextEdit, SLOT(clear()));
  outputLogLayout->addWidget(clearOutputLogButton);
  QVBoxLayout* moduleLayout = qobject_cast<QVBoxLayout*>(q->layout());
  if (moduleLayout)
    {
    // before the final spacer of the module, if any
    int index = moduleLayout->count();
    if (index > 0 && moduleLayout->itemAt(index - 1)->spacerItem())
      {
      index--;
      }
    moduleLayout->insertWidget(index, this->outputLogCollapsibleButton);
    }

  this->thread = new QThread();
  this->worker = new qSlicerAstroModelingModuleWorker();

  this->worker->moveToThread(thread);

  this->worker->SetAstroModelingLogic(this->logic());
  this->worker->SetAstroModelingParametersNode(this->parametersNode);

  QObject::connect(this->worker, SIGNAL(workRequested()), this->thread, SLOT(start()));

  QObject::connect(this->thread, SIGNAL(started()), this->worker, SLOT(doWork()));

  QObject::connect(this->worker, SIGNAL(finished()), q, SLOT(onWorkFinished()));

  this->workerProgressTimer = new QTimer(q);
  this->workerProgressTimer->setInterval(250);
  QObject::connect(this->workerProgressTimer, SIGNAL(timeout()), q, SLOT(updateWorkerProgress()));

  QObject::connect(this->worker, SIGNAL(finished()), this->thread, SLOT(quit()), Qt::DirectConnection);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::appendOutputLog(const QString& text)
{
  if (!this->outputLogTextEdit || text.isEmpty())
    {
    return;
    }
  QScrollBar* scrollBar = this->outputLogTextEdit->verticalScrollBar();
  const bool atEnd = scrollBar->value() == scrollBar->maximum();
  QTextCursor cursor(this->outputLogTextEdit->document());
  cursor.movePosition(QTextCursor::End);
  // progress bars of 3DBarolo rewrite their line with carriage returns
  QString cleaned = text;
  cleaned.replace("\r\n", "\n");
  cleaned.replace('\r', '\n');
  cursor.insertText(cleaned);
  if (atEnd)
    {
    scrollBar->setValue(scrollBar->maximum());
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::appendOperationHeader(int operation)
{
  QString name;
  switch (operation)
    {
    case vtkMRMLAstroModelingParametersNode::ESTIMATE:
      name = qSlicerAstroModelingModuleWidget::tr("Estimate initial parameters");
      break;
    case vtkMRMLAstroModelingParametersNode::CREATE:
      name = qSlicerAstroModelingModuleWidget::tr("Create model");
      break;
    case vtkMRMLAstroModelingParametersNode::FIT:
      name = qSlicerAstroModelingModuleWidget::tr("Fit model");
      break;
    case vtkMRMLAstroModelingParametersNode::UPDATE:
      name = qSlicerAstroModelingModuleWidget::tr("Calculate and visualize model");
      break;
    default:
      name = qSlicerAstroModelingModuleWidget::tr("3DBarolo");
      break;
    }
  if (this->outputLogTextEdit && !this->outputLogTextEdit->document()->isEmpty())
    {
    this->appendOutputLog("\n");
    }
  this->appendOutputLog(QString("==== %1 (%2) ====\n")
    .arg(name, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
}

//-----------------------------------------------------------------------------
vtkMRMLSegmentationNode* qSlicerAstroModelingModuleWidgetPrivate::modelingSegment(vtkMRMLScene* scene,
                                                                                const char* segmentID) const
{
  vtkMRMLSegmentationNode* segmentationNode = scene ?
    vtkMRMLSegmentationNode::SafeDownCast(scene->GetFirstNodeByName("AstroModelingContoursSeg")) : nullptr;
  if (!segmentationNode || !segmentationNode->GetSegmentation() ||
      !segmentationNode->GetSegmentation()->GetSegment(segmentID))
    {
    return nullptr;
    }
  return segmentationNode;
}

//-----------------------------------------------------------------------------
bool qSlicerAstroModelingModuleWidgetPrivate::workerRunning() const
{
  return this->workerParametersNode != nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroModelingModuleWidget);

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

  this->internalTableNode = 0;

  if (this->astroTableNode)
    {
    q->mrmlScene()->RemoveNode(this->astroTableNode);
    }
  this->astroTableNode = 0;

  if (this->plotChartNodeVRot)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeVRot);
    }
  this->plotChartNodeVRot = 0;

  if (this->plotChartNodeVRad)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeVRad);
    }
  this->plotChartNodeVRad = 0;

  if (this->plotChartNodeInc)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeInc);
    }
  this->plotChartNodeInc = 0;

  if (this->plotChartNodePhi)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodePhi);
    }
  this->plotChartNodePhi = 0;

  if (this->plotChartNodeVSys)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeVSys);
    }
  this->plotChartNodeVSys = 0;

  if (this->plotChartNodeVDisp)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeVDisp);
    }
  this->plotChartNodeVDisp = 0;

  if (this->plotChartNodeDens)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeDens);
    }
  this->plotChartNodeDens = 0;

  if (this->plotChartNodeZ0)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeZ0);
    }
  this->plotChartNodeZ0 = 0;

  if (this->plotChartNodeXPos)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeXPos);
    }
  this->plotChartNodeXPos = 0;

  if (this->plotChartNodeYPos)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeYPos);
    }
  this->plotChartNodeYPos = 0;

  if (this->PlotSeriesNodeVRot)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeVRot);
    }
  this->PlotSeriesNodeVRot = 0;

  if (this->PlotSeriesNodeVRad)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeVRad);
    }
  this->PlotSeriesNodeVRad = 0;

  if (this->PlotSeriesNodeInc)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeInc);
    }
  this->PlotSeriesNodeInc = 0;

  if (this->PlotSeriesNodePhi)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodePhi);
    }
  this->PlotSeriesNodePhi = 0;

  if (this->PlotSeriesNodeVSys)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeVSys);
    }
  this->PlotSeriesNodeVSys = 0;

  if (this->PlotSeriesNodeVDisp)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeVDisp);
    }
  this->PlotSeriesNodeVDisp = 0;

  if (this->PlotSeriesNodeDens)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeDens);
    }
  this->PlotSeriesNodeDens = 0;

  if (this->PlotSeriesNodeZ0)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeZ0);
    }
  this->PlotSeriesNodeZ0 = 0;

  if (this->PlotSeriesNodeXPos)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeXPos);
    }
  this->PlotSeriesNodeXPos = 0;

  if (this->PlotSeriesNodeYPos)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeYPos);
    }
  this->PlotSeriesNodeYPos = 0;

  if (this->fiducialNodeMajor)
    {
    q->mrmlScene()->RemoveNode(this->fiducialNodeMajor);
    }
  this->fiducialNodeMajor = 0;

  if (this->fiducialNodeMinor)
    {
    q->mrmlScene()->RemoveNode(this->fiducialNodeMinor);
    }
  this->fiducialNodeMinor = 0;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroModelingLogic* qSlicerAstroModelingModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroModelingModuleWidget);
  return vtkSlicerAstroModelingLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroModelingModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidget::qSlicerAstroModelingModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroModelingModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->init();

  QObject::connect(d->SegmentationNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this, SLOT(onMaskSegmentationChanged(vtkMRMLNode*)));
}

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidget::~qSlicerAstroModelingModuleWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::enter()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  this->Superclass::enter();

  this->updateMaskSelector();
  this->onMaskSegmentationChanged(d->SegmentationNodeComboBox->currentNode());
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::exit()
{
  this->Superclass::exit();
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

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

//-----------------------------------------------------------------------------
double sign(double x)
{
  if (x < 0.)
    {
    return -1.;
    }

 return 1.;
}

//-----------------------------------------------------------------------------
double arctan(double y, double x)
{
  double r;

  r = atan2(y, x);

  if (r < 0.)
    {
    r += 2. * PI;
    }

  return r;
}

//-----------------------------------------------------------------------------
double putinrangerad(double angle)
{
  double twopi = 2. * PI;

  while (angle < 0.)
    {
    angle += twopi;
    }

  while (angle > twopi)
    {
    angle -= twopi;
    }

  return angle;
}

} // end namespace

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  this->qvtkReconnect(scene, vtkMRMLScene::StartImportEvent,
                      this, SLOT(onMRMLSceneStartImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent,
                      this, SLOT(onMRMLSceneEndImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndBatchProcessEvent,
                      this, SLOT(onMRMLSceneEndBatchProcessEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::StartCloseEvent,
                      this, SLOT(onMRMLSceneStartCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent,
                      this, SLOT(onMRMLSceneEndCloseEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndRestoreEvent,
                      this, SLOT(onMRMLSceneEndRestoreEvent()));

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::setMRMLScene : "
                   "appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::setMRMLScene : "
                   "selectionNode not found!";
    return;
    }

  this->initializeNodes();

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceAddedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceAdded(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceRemovedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceRemoved(vtkObject*)));

  this->onMRMLSelectionNodeModified(d->selectionNode);
  this->onMRMLSelectionNodeReferenceAdded(d->selectionNode);

  // Connect PlotWidget with ModelingWidget
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::setMRMLScene : "
                   "qSlicerApplication not found!";
    return;
    }

  qSlicerLayoutManager* layoutManager = app->layoutManager();

  if(!app)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::setMRMLScene : "
                   "layoutManager not found!";
    return;
    }

  vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(
    scene->GetFirstNodeByClass("vtkMRMLLayoutNode"));
  if (!layoutNode)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::setMRMLScene : "
                   "layoutNode not found!";
    return;
    }
  int viewArra = layoutNode->GetViewArrangement();
  if (viewArra != vtkMRMLLayoutNode::SlicerLayoutConventionalPlotView  &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutFourUpPlotView        &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutFourUpPlotTableView   &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutOneUpPlotView         &&
      viewArra != vtkMRMLLayoutNode::SlicerLayoutThreeOverThreePlotView)
    {
    layoutNode->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutConventionalPlotView);
    }

  qMRMLPlotWidget* plotWidget = layoutManager->plotWidget(0);

  if(!plotWidget)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::setMRMLScene : "
                   "plotWidget not found!";
    return;
    }

  qMRMLPlotView* plotView = plotWidget->plotView();
  if(!plotView)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::setMRMLScene : "
                   "plotView not found!";
    return;
    }

  QObject::connect(plotView, SIGNAL(dataSelected(vtkStringArray*, vtkCollection*)),
                   this, SLOT(onPlotSelectionChanged(vtkStringArray*, vtkCollection*)));

  vtkMRMLPlotViewNode* plotViewNode = plotView->mrmlPlotViewNode();
  if (plotViewNode)
    {
    plotViewNode->SetInteractionMode(vtkMRMLPlotViewNode::InteractionModeMovePoints);
    }

  vtkMRMLNode *activeVolume = scene->GetNodeByID(d->selectionNode->GetActiveVolumeID());
  if (activeVolume)
    {
    d->XcenterSliderWidget->setMaximum(StringToInt(activeVolume->GetAttribute("SlicerAstro.NAXIS1")));
    d->YcenterSliderWidget->setMaximum(StringToInt(activeVolume->GetAttribute("SlicerAstro.NAXIS2")));
    }

  d->InputSegmentCollapsibleButton->setCollapsed(true);
  d->FittingParametersCollapsibleButton->setCollapsed(false);
  d->OutputCollapsibleButton->setCollapsed(true);
  d->OutputCollapsibleButton_2->setCollapsed(true);

  // masks added, removed or modified (e.g. the contours)
  this->qvtkReconnect(scene, vtkMRMLScene::NodeAddedEvent, this, SLOT(updateMaskSelector()));
  this->qvtkReconnect(scene, vtkMRMLScene::NodeRemovedEvent, this, SLOT(updateMaskSelector()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent, this, SLOT(updateMaskSelector()));
  this->updateMaskSelector();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onFittingFunctionChanged(int value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetFittingFunction(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onGreenSliceRotated(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetGreenRotOldValue(d->parametersNode->GetGreenRotValue());
  d->parametersNode->SetGreenRotValue(value);
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onInclinationChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetInclination(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onInclinationErrorChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetInclinationError(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onInclinationFitChanged(bool flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetInclinationFit(flag);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  this->initializeParameterNode(forceNew);

  this->initializeSegmentations(forceNew);

  this->initializeTableNode(forceNew);

  this->initializePlotNodes(forceNew);

  this->initializeFiducialNodes(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::initializeFiducialNodes(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!this->mrmlScene() || !this->logic())
    {
    return;
    }

  vtkSlicerAstroModelingLogic* astroModelingLogic =
    vtkSlicerAstroModelingLogic::SafeDownCast(this->logic());
  if (!astroModelingLogic)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeFiducialNodes :"
                  " vtkSlicerAstroModelingLogic not found!";
    return;
    }
  vtkSlicerMarkupsLogic* MarkupsLogic =
    vtkSlicerMarkupsLogic::SafeDownCast(astroModelingLogic->GetMarkupsLogic());
  if (!MarkupsLogic)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeFiducialNodes :"
                  " vtkSlicerMarkupsLogic not found!";
    return;
    }

  int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLMarkupsFiducialNode");
  if(numNodes == 0 || forceNew)
    {
    std::string MarkupsFiducialMajorName("MarkupsFiducialsMajor");
    std::string ID = MarkupsLogic->AddNewFiducialNode
      (this->mrmlScene()->GenerateUniqueName(MarkupsFiducialMajorName).c_str());
    d->fiducialNodeMajor = vtkMRMLMarkupsFiducialNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(ID));
    }
  else
    {
    vtkSmartPointer<vtkCollection> fiducialMajorNodeCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClass("vtkMRMLMarkupsFiducialNode"));
    for (int ii = fiducialMajorNodeCol->GetNumberOfItems() - 1; ii >= 0 ; ii--)
      {
      vtkMRMLMarkupsFiducialNode* tempMarkupsFiducialMajor = vtkMRMLMarkupsFiducialNode::SafeDownCast
              (fiducialMajorNodeCol->GetItemAsObject(ii));
      if (!tempMarkupsFiducialMajor)
        {
        continue;
        }
      std::string MarkupsFiducialMajorName = tempMarkupsFiducialMajor->GetName();
      std::size_t found = MarkupsFiducialMajorName.find("MarkupsFiducialsMajor");
      if (found != std::string::npos)
        {
        d->fiducialNodeMajor = tempMarkupsFiducialMajor;
        }
      }
    }

  if(numNodes == 0 || forceNew)
    {
    std::string MarkupsFiducialMinorName("MarkupsFiducialsMinor");
    std::string ID = MarkupsLogic->AddNewFiducialNode
      (this->mrmlScene()->GenerateUniqueName(MarkupsFiducialMinorName).c_str());
    d->fiducialNodeMinor = vtkMRMLMarkupsFiducialNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(ID));
    }
  else
    {
    vtkSmartPointer<vtkCollection> fiducialMinorNodeCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClass("vtkMRMLMarkupsFiducialNode"));
    for (int ii = fiducialMinorNodeCol->GetNumberOfItems() - 1; ii >= 0 ; ii--)
      {
      vtkMRMLMarkupsFiducialNode* tempMarkupsFiducialMinor = vtkMRMLMarkupsFiducialNode::SafeDownCast
              (fiducialMinorNodeCol->GetItemAsObject(ii));
      if (!tempMarkupsFiducialMinor)
        {
        continue;
        }
      std::string MarkupsFiducialMinorName = tempMarkupsFiducialMinor->GetName();
      std::size_t found = MarkupsFiducialMinorName.find("MarkupsFiducialsMinor");
      if (found != std::string::npos)
        {
        d->fiducialNodeMinor = tempMarkupsFiducialMinor;
        }
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  vtkMRMLAstroModelingParametersNode *astroParametersNode = nullptr;

  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroModelingParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroModelingParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroModelingParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroModelingParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);
    astroParametersNode = vtkMRMLAstroModelingParametersNode::SafeDownCast(parametersNode);
    if (d->selectionNode)
      {
      int wasModifying = astroParametersNode->StartModify();
      astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
      astroParametersNode->EndModify(wasModifying);
      }
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::initializeTableNode(bool forceNew/* = false */)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  if (!d->internalTableNode)
    {
    d->internalTableNode = vtkSmartPointer<vtkMRMLTableNode>::New();
    }

  vtkSmartPointer<vtkMRMLNode> tableNode = nullptr;

  if (!forceNew)
    {
    if (d->parametersNode->GetParamsTableNode())
      {
      tableNode = d->parametersNode->GetParamsTableNode();
      }
    else
      {
      vtkSmartPointer<vtkCollection> TableNodes = vtkSmartPointer<vtkCollection>::Take
          (this->mrmlScene()->GetNodesByClass("vtkMRMLTableNode"));

      for (int ii = TableNodes->GetNumberOfItems() - 1; ii >= 0 ; ii--)
        {
        vtkMRMLNode* tempTableNode = vtkMRMLNode::SafeDownCast(TableNodes->GetItemAsObject(ii));
        if (!tempTableNode)
          {
          continue;
          }
        std::string TableName = tempTableNode->GetName();
        std::size_t found = TableName.find("ModelingParamsTable");
        if (found != std::string::npos)
          {
          tableNode = tempTableNode;
          break;
          }
        }
      }

    if (tableNode)
      {
      d->astroTableNode = vtkMRMLTableNode::SafeDownCast(tableNode);
      d->parametersNode->SetParamsTableNode(d->astroTableNode);
      return;
      }
    }

  vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLTableNode");
  tableNode.TakeReference(foo);
  std::string paramsTableNodeName = this->mrmlScene()->GenerateUniqueName("ModelingParamsTable");
  tableNode->SetName(paramsTableNodeName.c_str());
  this->mrmlScene()->AddNode(tableNode);

  d->astroTableNode = vtkMRMLTableNode::SafeDownCast(tableNode);
  int wasModifying = d->astroTableNode->StartModify();
  d->astroTableNode->RemoveAllColumns();
  d->astroTableNode->SetUseColumnTitleAsColumnHeader(true);
  d->astroTableNode->SetDefaultColumnType("double");

  vtkDoubleArray* Radii = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Radii)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Radii Column.";
    return;
    }
  Radii->SetName("Radii");
  d->astroTableNode->SetColumnUnitLabel("Radii", "arcsec");
  d->astroTableNode->SetColumnTitle("Radii", "Radius");

  vtkDoubleArray* VRot = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!VRot)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the VRot Column.";
    return;
    }
  VRot->SetName("VRot");
  d->astroTableNode->SetColumnUnitLabel("VRot", "km/s");
  d->astroTableNode->SetColumnTitle("VRot", "Rotational velocity");

  vtkDoubleArray* VRad = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!VRad)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the VRad Column.";
    return;
    }
  VRad->SetName("VRad");
  d->astroTableNode->SetColumnUnitLabel("VRad", "km/s");
  d->astroTableNode->SetColumnTitle("VRad", "Radial velocity");

  vtkDoubleArray* Inc = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Inc)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Inc Column.";
    return;
    }
  Inc->SetName("Inc");
  d->astroTableNode->SetColumnUnitLabel("Inc", "degree");
  d->astroTableNode->SetColumnTitle("Inc", "Inclination");

  vtkDoubleArray* Phi = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Phi)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Phi Column.";
    return;
    }
  Phi->SetName("Phi");
  d->astroTableNode->SetColumnUnitLabel("Phi", "degree");
  d->astroTableNode->SetColumnTitle("Phi", "Position angle");

  vtkDoubleArray* VSys = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!VSys)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the VSys Column.";
    return;
    }
  VSys->SetName("VSys");
  d->astroTableNode->SetColumnUnitLabel("VSys", "km/s (Velocity Definition: Optical)");
  d->astroTableNode->SetColumnTitle("VSys", "Systemic velocity");

  vtkDoubleArray* VDisp = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!VSys)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the VDisp Column.";
    return;
    }
  VDisp->SetName("VDisp");
  d->astroTableNode->SetColumnUnitLabel("VDisp", "km/s");
  d->astroTableNode->SetColumnTitle("VDisp", "Dispersion velocity");

  vtkDoubleArray* Dens = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Dens)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Dens Column.";
    return;
    }
  Dens->SetName("Dens");
  d->astroTableNode->SetColumnUnitLabel("Dens", "10^20 cm^-2");
  d->astroTableNode->SetColumnTitle("Dens", "Column density");

  vtkDoubleArray* Z0 = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Z0)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Z0 Column.";
    return;
    }
  Z0->SetName("Z0");
  d->astroTableNode->SetColumnUnitLabel("Z0", "Kpc");
  d->astroTableNode->SetColumnTitle("Z0", "Scale height");

  vtkDoubleArray* XPos = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!XPos)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the XPos Column.";
    return;
    }
  XPos->SetName("XPos");
  d->astroTableNode->SetColumnUnitLabel("XPos", "pixels");
  d->astroTableNode->SetColumnTitle("XPos", "X center");

  vtkDoubleArray* YPos = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!YPos)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the YPos Column.";
    return;
    }
  YPos->SetName("YPos");
  d->astroTableNode->SetColumnUnitLabel("YPos", "pixels");
  d->astroTableNode->SetColumnTitle("YPos", "Y center");
  d->astroTableNode->EndModify(wasModifying);

  d->parametersNode->SetParamsTableNode(d->astroTableNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::initializePlotNodes(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkMRMLTableNode *tableNode = d->parametersNode->GetParamsTableNode();

  if (!tableNode)
    {
    qWarning() <<"qSlicerAstroModelingModuleWidget::initializePlotNodes : "
                 "TableNode not found.";
    return;
    }

  // Check (and create) PlotData nodes
  if (!d->PlotSeriesNodeVRot)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeVRotCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "VRot"));

    if (PlotSeriesNodeVRotCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodeVRot.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeVRot);
      d->PlotSeriesNodeVRot->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeVRot->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeVRot->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeVRot->SetMarkerSize(9);
      d->PlotSeriesNodeVRot->SetLineWidth(3);
      d->PlotSeriesNodeVRot->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodeVRot->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodeVRot->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodeVRot->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnVRot));
      d->PlotSeriesNodeVRot->SetName("VRot");
      }
    else
      {
      d->PlotSeriesNodeVRot = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeVRotCol->GetItemAsObject(PlotSeriesNodeVRotCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodeVRot->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodeVRot);
    }

  if (!d->PlotSeriesNodeVRad)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeVRadCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "VRad"));

    if (PlotSeriesNodeVRadCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodeVRad.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeVRad);
      d->PlotSeriesNodeVRad->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeVRad->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeVRad->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeVRad->SetMarkerSize(9);
      d->PlotSeriesNodeVRad->SetLineWidth(3);
      d->PlotSeriesNodeVRad->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodeVRad->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodeVRad->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodeVRad->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnVRad));
      d->PlotSeriesNodeVRad->SetName("VRad");
      }
    else
      {
      d->PlotSeriesNodeVRad = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeVRadCol->GetItemAsObject(PlotSeriesNodeVRadCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodeVRad->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodeVRad);
    }

  if (!d->PlotSeriesNodeInc)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeIncCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "Inc"));

    if (PlotSeriesNodeIncCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodeInc.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeInc);
      d->PlotSeriesNodeInc->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeInc->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeInc->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeInc->SetMarkerSize(9);
      d->PlotSeriesNodeInc->SetLineWidth(3);
      d->PlotSeriesNodeInc->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodeInc->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodeInc->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodeInc->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnInc));
      d->PlotSeriesNodeInc->SetName("Inc");
      }
    else
      {
      d->PlotSeriesNodeInc = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeIncCol->GetItemAsObject(PlotSeriesNodeIncCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodeInc->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodeInc);
    }

  if (!d->PlotSeriesNodePhi)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodePhiCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "Phi"));

    if (PlotSeriesNodePhiCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodePhi.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodePhi);
      d->PlotSeriesNodePhi->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodePhi->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodePhi->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodePhi->SetMarkerSize(9);
      d->PlotSeriesNodePhi->SetLineWidth(3);
      d->PlotSeriesNodePhi->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodePhi->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodePhi->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodePhi->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnPhi));
      d->PlotSeriesNodePhi->SetName("Phi");
      }
    else
      {
      d->PlotSeriesNodePhi = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodePhiCol->GetItemAsObject(PlotSeriesNodePhiCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodePhi->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodePhi);
    }

  if (!d->PlotSeriesNodeVSys)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeVSysCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "VSys"));

    if (PlotSeriesNodeVSysCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodeVSys.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeVSys);
      d->PlotSeriesNodeVSys->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeVSys->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeVSys->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeVSys->SetMarkerSize(9);
      d->PlotSeriesNodeVSys->SetLineWidth(3);
      d->PlotSeriesNodeVSys->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodeVSys->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodeVSys->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodeVSys->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnVSys));
      d->PlotSeriesNodeVSys->SetName("VSys");
      }
    else
      {
      d->PlotSeriesNodeVSys = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeVSysCol->GetItemAsObject(PlotSeriesNodeVSysCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodeVSys->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodeVSys);
    }

  if (!d->PlotSeriesNodeVDisp)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeVDispCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "VDisp"));

    if (PlotSeriesNodeVDispCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodeVDisp.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeVDisp);
      d->PlotSeriesNodeVDisp->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeVDisp->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeVDisp->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeVDisp->SetMarkerSize(9);
      d->PlotSeriesNodeVDisp->SetLineWidth(3);
      d->PlotSeriesNodeVDisp->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodeVDisp->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodeVDisp->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodeVDisp->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnVDisp));
      d->PlotSeriesNodeVDisp->SetName("VDisp");
      }
    else
      {
      d->PlotSeriesNodeVDisp = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeVDispCol->GetItemAsObject(PlotSeriesNodeVDispCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodeVDisp->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodeVDisp);
    }

  if (!d->PlotSeriesNodeDens)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeDensCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "Dens"));

    if (PlotSeriesNodeDensCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodeDens.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeDens);
      d->PlotSeriesNodeDens->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeDens->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeDens->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeDens->SetMarkerSize(9);
      d->PlotSeriesNodeDens->SetLineWidth(3);
      d->PlotSeriesNodeDens->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodeDens->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodeDens->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodeDens->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnDens));
      d->PlotSeriesNodeDens->SetName("Dens");
      }
    else
      {
      d->PlotSeriesNodeDens = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeDensCol->GetItemAsObject(PlotSeriesNodeDensCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodeDens->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodeDens);
    }

  if (!d->PlotSeriesNodeZ0)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeZ0Col =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "Z0"));

    if (PlotSeriesNodeZ0Col->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodeZ0.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeZ0);
      d->PlotSeriesNodeZ0->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeZ0->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeZ0->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeZ0->SetMarkerSize(9);
      d->PlotSeriesNodeZ0->SetLineWidth(3);
      d->PlotSeriesNodeZ0->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodeZ0->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodeZ0->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodeZ0->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnZ0));
      d->PlotSeriesNodeZ0->SetName("Z0");
      }
    else
      {
      d->PlotSeriesNodeZ0 = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeZ0Col->GetItemAsObject(PlotSeriesNodeZ0Col->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodeZ0->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodeZ0);
    }

  if (!d->PlotSeriesNodeXPos)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeXPosCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "XPos"));

    if (PlotSeriesNodeXPosCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodeXPos.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeXPos);
      d->PlotSeriesNodeXPos->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeXPos->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeXPos->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeXPos->SetMarkerSize(9);
      d->PlotSeriesNodeXPos->SetLineWidth(3);
      d->PlotSeriesNodeXPos->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodeXPos->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodeXPos->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodeXPos->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnXPos));
      d->PlotSeriesNodeXPos->SetName("XPos");
      }
    else
      {
      d->PlotSeriesNodeXPos = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeXPosCol->GetItemAsObject(PlotSeriesNodeXPosCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodeXPos->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodeXPos);
    }

  if (!d->PlotSeriesNodeYPos)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeYPosCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "YPos"));

    if (PlotSeriesNodeYPosCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->PlotSeriesNodeYPos.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeYPos);
      d->PlotSeriesNodeYPos->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeYPos->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeYPos->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeYPos->SetMarkerSize(9);
      d->PlotSeriesNodeYPos->SetLineWidth(3);
      d->PlotSeriesNodeYPos->SetUniqueColor("vtkMRMLColorTableNodeFileDarkBrightChartColors.txt");
      d->PlotSeriesNodeYPos->SetAndObserveTableNodeID(tableNode->GetID());
      d->PlotSeriesNodeYPos->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->PlotSeriesNodeYPos->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnYPos));
      d->PlotSeriesNodeYPos->SetName("YPos");
      }
    else
      {
      d->PlotSeriesNodeYPos = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeYPosCol->GetItemAsObject(PlotSeriesNodeYPosCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->PlotSeriesNodeYPos->GetID()))
    {
    this->mrmlScene()->AddNode(d->PlotSeriesNodeYPos);
    }

  // Check (and create) PlotChart nodes
  if (!d->plotChartNodeVRot)
    {  
    vtkSmartPointer<vtkCollection> plotChartNodeVRotCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "VRotChart"));

    if (plotChartNodeVRotCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeVRot.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeVRot->SetName("VRotChart");
      d->plotChartNodeVRot->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodeVRot->SetYAxisTitle("Rotational Velocity (km/s)");
      d->plotChartNodeVRot->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodeVRot);
      }
    else
      {
      d->plotChartNodeVRot = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeVRotCol->GetItemAsObject(plotChartNodeVRotCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeVRot->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeVRot);
    }

  if (!d->plotChartNodeVRad)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeVRadCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "VRadChart"));

    if (plotChartNodeVRadCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeVRad.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeVRad->SetName("VRadChart");
      d->plotChartNodeVRad->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodeVRad->SetYAxisTitle("Radial Velocity (km/s)");
      d->plotChartNodeVRad->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodeVRad);
      }
    else
      {
      d->plotChartNodeVRad = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeVRadCol->GetItemAsObject(plotChartNodeVRadCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeVRad->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeVRad);
    }

  if (!d->plotChartNodeInc)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeIncCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "IncChart"));

    if (plotChartNodeIncCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeInc.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeInc->SetName("IncChart");
      d->plotChartNodeInc->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodeInc->SetYAxisTitle("Inclination (degree)");
      d->plotChartNodeInc->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodeInc);
      }
    else
      {
      d->plotChartNodeInc = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeIncCol->GetItemAsObject(plotChartNodeIncCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeInc->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeInc);
    }

  if (!d->plotChartNodePhi)
    {
    vtkSmartPointer<vtkCollection> plotChartNodePhiCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "PhiChart"));

    if (plotChartNodePhiCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodePhi.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodePhi->SetName("PhiChart");
      d->plotChartNodePhi->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodePhi->SetYAxisTitle("Orientation Angle (degree)");
      d->plotChartNodePhi->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodePhi);
      }
    else
      {
      d->plotChartNodePhi = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodePhiCol->GetItemAsObject(plotChartNodePhiCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodePhi->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodePhi);
    }

  if (!d->plotChartNodeVSys)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeVSysCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "VSysChart"));

    if (plotChartNodeVSysCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeVSys.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeVSys->SetName("VSysChart");
      d->plotChartNodeVSys->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodeVSys->SetYAxisTitle("Systemic Velocity (km/s)");
      d->plotChartNodeVSys->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodeVSys);
      }
    else
      {
      d->plotChartNodeVSys = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeVSysCol->GetItemAsObject(plotChartNodeVSysCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeVSys->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeVSys);
    }

  if (!d->plotChartNodeVDisp)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeVDispCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "VDispChart"));

    if (plotChartNodeVDispCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeVDisp.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeVDisp->SetName("VDispChart");
      d->plotChartNodeVDisp->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodeVDisp->SetYAxisTitle("Dispersion Velocity (km/s)");
      d->plotChartNodeVDisp->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodeVDisp);
      }
    else
      {
      d->plotChartNodeVDisp = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeVDispCol->GetItemAsObject(plotChartNodeVDispCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeVDisp->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeVDisp);
    }

  if (!d->plotChartNodeDens)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeDensCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "DensChart"));

    if (plotChartNodeDensCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeDens.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeDens->SetName("DensChart");
      d->plotChartNodeDens->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodeDens->SetYAxisTitle("Column Density (10^20 cm^-2)");
      d->plotChartNodeDens->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodeDens);
      }
    else
      {
      d->plotChartNodeDens = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeDensCol->GetItemAsObject(plotChartNodeDensCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeDens->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeDens);
    }

  if (!d->plotChartNodeZ0)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeZ0Col =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "Z0Chart"));

    if (plotChartNodeZ0Col->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeZ0.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeZ0->SetName("Z0Chart");
      d->plotChartNodeZ0->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodeZ0->SetYAxisTitle("Scale Heigth (Kpc)");
      d->plotChartNodeZ0->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodeZ0);
      }
    else
      {
      d->plotChartNodeZ0 = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeZ0Col->GetItemAsObject(plotChartNodeZ0Col->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeZ0->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeZ0);
    }

  if (!d->plotChartNodeXPos)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeXPosCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "XPosChart"));

    if (plotChartNodeXPosCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeXPos.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeXPos->SetName("XPosChart");
      d->plotChartNodeXPos->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodeXPos->SetYAxisTitle("X Center (pixels)");
      d->plotChartNodeXPos->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodeXPos);
      }
    else
      {
      d->plotChartNodeXPos = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeXPosCol->GetItemAsObject(plotChartNodeXPosCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeXPos->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeXPos);
    }

  if (!d->plotChartNodeYPos)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeYPosCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "YPosChart"));

    if (plotChartNodeYPosCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeYPos.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeYPos->SetName("YPosChart");
      d->plotChartNodeYPos->SetXAxisTitle("Radii (arcsec)");
      d->plotChartNodeYPos->SetYAxisTitle("Y Center (pixels)");
      d->plotChartNodeYPos->SetEnablePointMoveAlongX(false);
      this->mrmlScene()->AddNode(d->plotChartNodeYPos);
      }
    else
      {
      d->plotChartNodeYPos = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeYPosCol->GetItemAsObject(plotChartNodeYPosCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeYPos->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeYPos);
    }

  // Add PlotSeriesNodes to PlotChartNodes
  d->plotChartNodeVRot->RemoveAllPlotSeriesNodeIDs();
  d->plotChartNodeVRad->RemoveAllPlotSeriesNodeIDs();
  d->plotChartNodeInc->RemoveAllPlotSeriesNodeIDs();
  d->plotChartNodePhi->RemoveAllPlotSeriesNodeIDs();
  d->plotChartNodeVSys->RemoveAllPlotSeriesNodeIDs();
  d->plotChartNodeVDisp->RemoveAllPlotSeriesNodeIDs();
  d->plotChartNodeDens->RemoveAllPlotSeriesNodeIDs();
  d->plotChartNodeZ0->RemoveAllPlotSeriesNodeIDs();
  d->plotChartNodeXPos->RemoveAllPlotSeriesNodeIDs();
  d->plotChartNodeYPos->RemoveAllPlotSeriesNodeIDs();

  d->plotChartNodeVRot->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeVRot->GetID());
  d->plotChartNodeVRad->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeVRad->GetID());
  d->plotChartNodeInc->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeInc->GetID());
  d->plotChartNodePhi->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodePhi->GetID());
  d->plotChartNodeVSys->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeVSys->GetID());
  d->plotChartNodeVDisp->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeVDisp->GetID());
  d->plotChartNodeDens->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeDens->GetID());
  d->plotChartNodeZ0->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeZ0->GetID());
  d->plotChartNodeXPos->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeXPos->GetID());
  d->plotChartNodeYPos->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeYPos->GetID());

  //Select nullptr Chart
  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID(nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  std::string segmentEditorSingletonTag = "SegmentEditor";
  vtkMRMLSegmentEditorNode *segmentEditorNodeSingleton = vtkMRMLSegmentEditorNode::SafeDownCast(
    this->mrmlScene()->GetSingletonNode(segmentEditorSingletonTag.c_str(), "vtkMRMLSegmentEditorNode"));

  if (!segmentEditorNodeSingleton || forceNew)
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
bool qSlicerAstroModelingModuleWidget::convertSelectedSegmentToLabelMap()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!this->mrmlScene())
    {
    return false;
    }

  if (!d->segmentEditorNode)
    {
    qCritical() << Q_FUNC_INFO << ": segmentEditorNode not found.";
    return false;
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode || !currentSegmentationNode->GetSegmentation())
    {
    QString message = QString("No segmentation available! Please create a segmentation or untoggle the input"
                              " mask option to perform automatic masking with 3DBarolo.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(nullptr, tr("Failed to select a mask"), message);
    return false;
    }

  // Export selected segments into a multi-label labelmap volume
  std::vector<std::string> segmentIDs;
  currentSegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIDs);

  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> labelMapNode;

  QStringList selectedSegmentIDs = d->SegmentsTableView->selectedSegmentIDs();

  if (selectedSegmentIDs.size() < 1)
    {
    QString message = QString("No segment selected from the segmentation node! Please provide a mask or untoggle the input"
                              " mask option to perform automatic masking with 3DBarolo.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(nullptr, tr("Failed to select a mask"), message);
    return false;
    }

  segmentIDs.clear();
  segmentIDs.push_back(selectedSegmentIDs[0].toStdString());

  vtkMRMLAstroVolumeNode* activeVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(
     d->InputVolumeNodeSelector->currentNode());

  if (activeVolumeNode)
    {
    vtkSlicerAstroModelingLogic* astroModelinglogic =
      vtkSlicerAstroModelingLogic::SafeDownCast(this->logic());
    if (!astroModelinglogic)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::convertSelectedSegmentToLabelMap :"
                    " astroModelinglogic not found!";
      return false;
      }
    vtkSlicerAstroVolumeLogic* astroVolumelogic =
      vtkSlicerAstroVolumeLogic::SafeDownCast(astroModelinglogic->GetAstroVolumeLogic());
    if (!astroVolumelogic)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::convertSelectedSegmentToLabelMap :"
                    " vtkSlicerAstroVolumeLogic not found!";
      return false;
      }
    std::string name(activeVolumeNode->GetName());
    name += "Copy_mask" + IntToString(d->parametersNode->GetOutputSerial());
    labelMapNode = astroVolumelogic->CreateAndAddLabelVolume(this->mrmlScene(), activeVolumeNode, name.c_str());
    }
  else
    {
    qCritical() << Q_FUNC_INFO << ": converting current segmentation Node into labelMap Node (Mask),"
                                  " but the labelMap Node is invalid!";
    return false;
    }

  if (!vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode
       (currentSegmentationNode, segmentIDs, labelMapNode, activeVolumeNode))
    {
    QString message = QString("Failed to export segments from segmentation '%1'' to representation node '%2!.").
                              arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(nullptr, tr("Failed to export segment"), message);
    this->mrmlScene()->RemoveNode(labelMapNode);
    return false;
    }

  labelMapNode->UpdateRangeAttributes();

  d->parametersNode->SetMaskVolumeNodeID(labelMapNode->GetID());

  return true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onCalculateAndVisualize()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (d->workerRunning())
    {
    // one 3DBarolo operation at a time (e.g. a second click)
    return;
    }

  if (!d->parametersNode || !d->selectionNode ||
      !this->mrmlScene() || !d->astroVolumeWidget)
    {
    return;
    }

  if (!d->parametersNode->GetParamsTableNode() ||
      !d->parametersNode->GetParamsTableNode()->GetTable())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onCalculateAndVisualize : "
                  "Table not found!";
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onCalculateAndVisualize :"
                   " appLogic not found!";
    return;
    }


  if (!d->logic())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onCalculateAndVisualize :"
                  " logic not found!";
    return;
    }

  // the model is computed in the worker thread (progress, no blocked UI);
  // the visualization follows in onWorkFinished (finishCalculateAndVisualize)
  vtkMRMLAstroVolumeNode* inputVolume = vtkMRMLAstroVolumeNode::SafeDownCast(
    this->mrmlScene()->GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  vtkMRMLAstroVolumeDisplayNode* inputVolumeDisplayNode = inputVolume ? inputVolume->GetAstroVolumeDisplayNode() : nullptr;
  if (!inputVolumeDisplayNode)
    {
    qCritical() << Q_FUNC_INFO << ": input volume not found!";
    return;
    }
  d->restoreRadioVelocity = !strncmp(inputVolumeDisplayNode->GetVelocityDefinition().c_str(), "VRAD", 4);
  if (d->restoreRadioVelocity)
    {
    inputVolumeDisplayNode->SetOpticalVelocityDefinition();
    }
  d->calculatingModel = true;
  d->appendOperationHeader(vtkMRMLAstroModelingParametersNode::UPDATE);
  d->parametersNode->SetStatus(1);
  d->workerParametersNode = vtkSmartPointer<vtkMRMLAstroModelingParametersNode>::New();
  d->workerParametersNode->Copy(d->parametersNode);
  d->workerParametersNode->SetOperation(vtkMRMLAstroModelingParametersNode::UPDATE);
  d->worker->SetTableNode(d->parametersNode->GetParamsTableNode());
  d->worker->SetAstroModelingParametersNode(d->workerParametersNode);
  d->worker->SetAstroModelingLogic(d->logic());
  d->workerProgressTimer->start();
  d->worker->requestWork();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::finishCalculateAndVisualize(bool success)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode || !d->selectionNode || !this->mrmlScene() ||
      !d->astroVolumeWidget || !d->parametersNode->GetParamsTableNode())
    {
    return;
    }
  const char *activeVolumeNodeID = d->selectionNode->GetActiveVolumeID();
  const char *secondaryVolumeNodeID = d->selectionNode->GetSecondaryVolumeID();

  if (!success)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onCalculateAndVisualize :"
                  " UpdateModel error!";
    int wasModifying = d->parametersNode->StartModify();
    d->parametersNode->SetXPosCenterIJK(0.);
    d->parametersNode->SetYPosCenterIJK(0.);
    d->parametersNode->SetPVPhi(0.);
    d->parametersNode->EndModify(wasModifying);
    return;
    }

  vtkDoubleArray* Phi = vtkDoubleArray::SafeDownCast
    (d->parametersNode->GetParamsTableNode()
       ->GetTable()->GetColumnByName("Phi"));

  vtkDoubleArray* XPos = vtkDoubleArray::SafeDownCast
    (d->parametersNode->GetParamsTableNode()
       ->GetTable()->GetColumnByName("XPos"));

  vtkDoubleArray* YPos = vtkDoubleArray::SafeDownCast
    (d->parametersNode->GetParamsTableNode()
       ->GetTable()->GetColumnByName("YPos"));

  if (!Phi || !XPos || !YPos)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onCalculateAndVisualize : "
                  "arrays not found!";
    return;
    }

  double PhiMean = 0. , XPosMean = 0., YPosMean = 0.;

  for (int ii = 0; ii < Phi->GetNumberOfValues(); ii++)
    {
    PhiMean += Phi->GetValue(ii);
    XPosMean += XPos->GetValue(ii);
    YPosMean += YPos->GetValue(ii);
    }

  PhiMean /= Phi->GetNumberOfValues();
  XPosMean /= XPos->GetNumberOfValues();
  YPosMean /= YPos->GetNumberOfValues();

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetXPosCenterIJK(XPosMean);
  d->parametersNode->SetYPosCenterIJK(YPosMean);
  d->parametersNode->SetPVPhi(-(PhiMean - 90.));
  d->parametersNode->SetYellowRotOldValue(0.);
  d->parametersNode->SetYellowRotValue(0.);
  d->parametersNode->SetGreenRotOldValue(0.);
  d->parametersNode->SetGreenRotValue(0.);
  d->parametersNode->EndModify(wasModifying);

  vtkMRMLAstroVolumeNode *activeVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(activeVolumeNodeID));
  if (!activeVolume || !activeVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onCalculateAndVisualize : "
                  "activeVolume not found!";
    return;
    }

  int dims[3];
  activeVolume->GetImageData()->GetDimensions(dims);
  int Zcenter = dims[2] * 0.5;
  vtkNew<vtkGeneralTransform> IJKtoRASTransform;
  IJKtoRASTransform->Identity();
  IJKtoRASTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  activeVolume->GetIJKToRASMatrix(IJKtoRASMatrix.GetPointer());
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix.GetPointer());

  double ijk[3] = {0.,0.,0.}, RAS[3] = {0.,0.,0.};
  ijk[0] = d->parametersNode->GetXPosCenterIJK();
  ijk[1] = d->parametersNode->GetYPosCenterIJK();
  ijk[2] = Zcenter;
  IJKtoRASTransform->TransformPoint(ijk, RAS);
  wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetXPosCenterRAS(RAS[0]);
  d->parametersNode->SetYPosCenterRAS(RAS[1]);
  d->parametersNode->SetZPosCenterRAS(RAS[2]);
  d->parametersNode->EndModify(wasModifying);

  d->astroVolumeWidget->updateQuantitative3DView
        (activeVolumeNodeID,
         secondaryVolumeNodeID,
         d->parametersNode->GetContourLevel(),
         d->parametersNode->GetPVPhi(),
         d->parametersNode->GetPVPhi() + 90.,
         RAS, RAS, true);

  // the model and its region as chosen by the user (the redraw shows them)
  this->applyModelingSegmentVisibility();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setModelingSegmentVisibility(const char* segmentID, bool show)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  vtkMRMLSegmentationNode* segmentationNode = d->modelingSegment(this->mrmlScene(), segmentID);
  if (!segmentationNode)
    {
    return;
    }
  for (int ii = 0; ii < segmentationNode->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode* displayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetNthDisplayNode(ii));
    if (!displayNode)
      {
      continue;
      }
    displayNode->SetSegmentVisibility(segmentID, show);
    // the region of the model is shown in the 3D view only
    if (!strcmp(segmentID, qSlicerAstroVolumeModuleWidget::modelingModelMaskSegmentID()))
      {
      displayNode->SetSegmentVisibility3D(segmentID, show);
      }
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroModelingModuleWidget::isModelingSegmentVisible(const char* segmentID)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  vtkMRMLSegmentationNode* segmentationNode = d->modelingSegment(this->mrmlScene(), segmentID);
  vtkMRMLSegmentationDisplayNode* displayNode = segmentationNode ?
    vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetDisplayNode()) : nullptr;
  return displayNode && displayNode->GetSegmentVisibility(segmentID);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onShowModelToggled(bool show)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  // no contours of the model yet (or not for this model): draw them
  if (show && !d->modelingSegment(this->mrmlScene(),
                                  qSlicerAstroVolumeModuleWidget::modelingModelContoursSegmentID()))
    {
    this->onVisualize();
    return;
    }
  this->setModelingSegmentVisibility(qSlicerAstroVolumeModuleWidget::modelingModelContoursSegmentID(), show);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onShowModelMaskToggled(bool show)
{
  this->setModelingSegmentVisibility(qSlicerAstroVolumeModuleWidget::modelingModelMaskSegmentID(), show);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::applyModelingSegmentVisibility()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  // after a redraw of the contours (which shows them), as chosen by the user
  this->setModelingSegmentVisibility(qSlicerAstroVolumeModuleWidget::modelingModelContoursSegmentID(),
                                     d->VisualizePushButton->isChecked());
  this->setModelingSegmentVisibility(qSlicerAstroVolumeModuleWidget::modelingModelMaskSegmentID(),
                                     d->VisualizeMaskPushButton->isChecked());
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::updateModelingButtons()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  // after a new model (Create, Fit): the buttons show what is visible
  const QSignalBlocker modelBlocker(d->VisualizePushButton);
  d->VisualizePushButton->setChecked(
    this->isModelingSegmentVisible(qSlicerAstroVolumeModuleWidget::modelingModelContoursSegmentID()));
  const QSignalBlocker maskBlocker(d->VisualizeMaskPushButton);
  d->VisualizeMaskPushButton->setChecked(
    this->isModelingSegmentVisible(qSlicerAstroVolumeModuleWidget::modelingModelMaskSegmentID()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onCloudsColumnDensityChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetCloudsColumnDensity(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onColumnDensityChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetColumnDensity(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!sender || !this->mrmlScene())
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
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroModelingModuleWidget);

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
void qSlicerAstroModelingModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  Q_D(qSlicerAstroModelingModuleWidget);

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
void qSlicerAstroModelingModuleWidget::onMRMLSliceNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!sender || ! d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLSliceNode *sliceNode =
      vtkMRMLSliceNode::SafeDownCast(sender);
  if (!sliceNode)
    {
    return;
    }

  vtkMatrix4x4* sliceToRAS = sliceNode->GetSliceToRAS();
  if (!sliceToRAS)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* inputVolumeDisplayNode = inputVolume->GetAstroVolumeDisplayNode();
  if(!inputVolumeDisplayNode)
    {
    return;
    }

  vtkMRMLTableNode* paramsTableNode = d->parametersNode->GetParamsTableNode();
  if (!inputVolume->GetImageData() || !paramsTableNode || !paramsTableNode->GetTable())
    {
    return;
    }
  vtkTable* paramsTable = paramsTableNode->GetTable();
  vtkDoubleArray* Radii = vtkDoubleArray::SafeDownCast(paramsTable->GetColumnByName("Radii"));
  vtkDoubleArray* VRot = vtkDoubleArray::SafeDownCast(paramsTable->GetColumnByName("VRot"));
  vtkDoubleArray* VRad = vtkDoubleArray::SafeDownCast(paramsTable->GetColumnByName("VRad"));
  vtkDoubleArray* Inc = vtkDoubleArray::SafeDownCast(paramsTable->GetColumnByName("Inc"));
  vtkDoubleArray* Phi = vtkDoubleArray::SafeDownCast(paramsTable->GetColumnByName("Phi"));
  vtkDoubleArray* VSys = vtkDoubleArray::SafeDownCast(paramsTable->GetColumnByName("VSys"));
  vtkDoubleArray* XPos = vtkDoubleArray::SafeDownCast(paramsTable->GetColumnByName("XPos"));
  vtkDoubleArray* YPos = vtkDoubleArray::SafeDownCast(paramsTable->GetColumnByName("YPos"));
  if (!Radii || !VRot || !VRad || !Inc || !Phi || !VSys || !XPos || !YPos)
    {
    return;
    }
  const vtkIdType numberOfRings = Radii->GetNumberOfValues();
  if (VRot->GetNumberOfValues() < numberOfRings || VRad->GetNumberOfValues() < numberOfRings ||
      Inc->GetNumberOfValues() < numberOfRings || Phi->GetNumberOfValues() < numberOfRings ||
      VSys->GetNumberOfValues() < numberOfRings || XPos->GetNumberOfValues() < numberOfRings ||
      YPos->GetNumberOfValues() < numberOfRings)
    {
    return;
    }

  // points of the rotation curve on the PV slice: two points per ring
  vtkMRMLMarkupsFiducialNode* pointsNode = nullptr;
  if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeYellow"))
    {
    pointsNode = d->fiducialNodeMajor;
    }
  else if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeGreen"))
    {
    pointsNode = d->fiducialNodeMinor;
    }
  if (!pointsNode || pointsNode->GetNumberOfControlPoints() != numberOfRings * 2)
    {
    return;
    }

  // The PV slice in the pixel space (IJK, from 0) of the input: the line of
  // the slice through its center, along the spatial direction of the slice
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  inputVolume->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  inputVolume->GetIJKToRASMatrix(IJKtoRASMatrix.GetPointer());
  double centerRAS[4] = {sliceToRAS->GetElement(0, 3), sliceToRAS->GetElement(1, 3),
                         sliceToRAS->GetElement(2, 3), 1.};
  double centerIJK[4] = {0., 0., 0., 1.};
  RAStoIJKMatrix->MultiplyPoint(centerRAS, centerIJK);
  double direction[2] = {0., 0.};
  for (int axis = 0; axis < 2 && hypot(direction[0], direction[1]) < 1.E-9; axis++)
    {
    double axisRAS[4] = {centerRAS[0] + sliceToRAS->GetElement(0, axis),
                         centerRAS[1] + sliceToRAS->GetElement(1, axis),
                         centerRAS[2] + sliceToRAS->GetElement(2, axis), 1.};
    double axisIJK[4] = {0., 0., 0., 1.};
    RAStoIJKMatrix->MultiplyPoint(axisRAS, axisIJK);
    direction[0] = axisIJK[0] - centerIJK[0];
    direction[1] = axisIJK[1] - centerIJK[1];
    }
  const double directionNorm = hypot(direction[0], direction[1]);
  if (directionNorm < 1.E-9)
    {
    return;
    }
  direction[0] /= directionNorm;
  direction[1] /= directionNorm;

  // geometry of the rings as in the model of 3DBarolo (Galmod): pixel steps
  // in arcsec (signed), position angle plus the rotation of the image
  const double deg2rad = PI / 180.;
  const double cdelt1 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.CDELT1")) * 3600.;
  const double cdelt2 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.CDELT2")) * 3600.;
  const double crota = StringToDouble(inputVolume->GetAttribute("SlicerAstro.CROTA2")) * deg2rad;
  if (cdelt1 == 0. || cdelt2 == 0.)
    {
    return;
    }
  // the velocities of 3DBarolo are optical; the spectral axis of the input
  // may be displayed as radio velocity (same frequency: vrad = c vopt / (c + vopt))
  const bool radioVelocity = !strncmp(inputVolumeDisplayNode->GetVelocityDefinition().c_str(), "VRAD", 4);
  const double speedOfLight = 299792.458; // km/s

  pointsNode->GlobalWarningDisplayOff();
  const int wasModifying = pointsNode->StartModify();
  for (vtkIdType ring = 0; ring < numberOfRings; ring++)
    {
    const double radius = Radii->GetValue(ring);
    const double inc = Inc->GetValue(ring) * deg2rad;
    const double pa = Phi->GetValue(ring) * deg2rad + crota;
    const double spa = sin(pa);
    const double cpa = cos(pa);
    const double xpos = XPos->GetValue(ring);
    const double ypos = YPos->GetValue(ring);

    // Point of the ring at the azimuth az (from the receding major axis, in
    // the plane of the disk): x = R cos(az), y = R sin(az) cos(inc),
    // pixel = (xpos + (x spa - y cpa) / cdelt1, ypos + (x cpa + y spa) / cdelt2).
    // On the line of the slice: A + B cos(az) + E sin(az) = 0.
    const double ai = xpos - centerIJK[0];
    const double aj = ypos - centerIJK[1];
    const double bi = radius * spa / cdelt1;
    const double ei = -radius * cos(inc) * cpa / cdelt1;
    const double bj = radius * cpa / cdelt2;
    const double ej = radius * cos(inc) * spa / cdelt2;
    const double A = ai * direction[1] - aj * direction[0];
    const double B = bi * direction[1] - bj * direction[0];
    const double E = ei * direction[1] - ej * direction[0];
    const double rho = hypot(B, E);
    const int firstIndex = static_cast<int>(ring) * 2;
    if (radius <= 0. || rho < 1.E-12 || fabs(A) > rho)
      {
      // the ring does not cross the slice
      for (int side = 0; side < 2; side++)
        {
        pointsNode->SetNthControlPointVisibility(firstIndex + side, false);
        }
      continue;
      }
    const double base = atan2(E, B);
    const double delta = acos(-A / rho);
    for (int side = 0; side < 2; side++)
      {
      const double az = base + (side == 0 ? delta : -delta);
      const double x = radius * cos(az);
      const double y = radius * sin(az) * cos(inc);
      double ijk[3] = {xpos + (x * spa - y * cpa) / cdelt1,
                       ypos + (x * cpa + y * spa) / cdelt2,
                       0.};
      double velocity = VSys->GetValue(ring) +
        (VRot->GetValue(ring) * cos(az) + VRad->GetValue(ring) * sin(az)) * sin(inc);
      if (radioVelocity)
        {
        velocity = speedOfLight * velocity / (speedOfLight + velocity);
        }
      double world[3] = {0., 0., 0.}, velocityIJK[3] = {0., 0., 0.};
      inputVolumeDisplayNode->GetReferenceSpace(ijk, world);
      world[2] = velocity * 1000.;
      inputVolumeDisplayNode->GetIJKSpace(world, velocityIJK);
      double pointIJK[4] = {ijk[0], ijk[1], velocityIJK[2], 1.};
      double pointRAS[4] = {0., 0., 0., 1.};
      IJKtoRASMatrix->MultiplyPoint(pointIJK, pointRAS);
      pointsNode->SetNthControlPointPosition(firstIndex + side, pointRAS[0], pointRAS[1], pointRAS[2]);
      pointsNode->SetNthControlPointVisibility(firstIndex + side, true);
      }
    }
  pointsNode->EndModify(wasModifying);
  pointsNode->GlobalWarningDisplayOn();

   // This is required to update the table in the Markups Module.
   // However, it slows down a lot the performance.
   /*for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues(); radiiIndex++)
     {
     d->fiducialNodeMajor->InvokeCustomModifiedEvent(vtkMRMLMarkupsNode::PointModifiedEvent, (void*)&radiiIndex);
     d->fiducialNodeMinor->InvokeCustomModifiedEvent(vtkMRMLMarkupsNode::PointModifiedEvent, (void*)&radiiIndex);
     }*/
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLTableNodeModified()
{
  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode || !yellowSliceNode)
    {
    return;
    }

  this->onMRMLSliceNodeModified(yellowSliceNode);
  this->onMRMLSliceNodeModified(greenSliceNode);
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLYellowSliceRotated()
{
  Q_D(const qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    return;
    }

  vtkMatrix4x4* yellowSliceToRAS = yellowSliceNode->GetSliceToRAS();
  if (!yellowSliceToRAS)
    {
    return;
    }

  double RotY = d->parametersNode->GetYellowRotValue() -
                d->parametersNode->GetYellowRotOldValue();
  if (fabs(RotY) > 1.E-6)
    {
    if (fabs(yellowSliceToRAS->GetElement(0, 3) - d->parametersNode->GetXPosCenterRAS()) > 1.E-6 ||
        fabs(yellowSliceToRAS->GetElement(1, 3) - d->parametersNode->GetYPosCenterRAS()) > 1.E-6 ||
        fabs(yellowSliceToRAS->GetElement(2, 3) - d->parametersNode->GetZPosCenterRAS()) > 1.E-6)
      {
      yellowSliceToRAS->SetElement(0, 3, d->parametersNode->GetXPosCenterRAS());
      yellowSliceToRAS->SetElement(1, 3, d->parametersNode->GetYPosCenterRAS());
      yellowSliceToRAS->SetElement(2, 3, d->parametersNode->GetZPosCenterRAS());
      }

    vtkNew<vtkTransform> yellowTransform;
    yellowTransform->SetMatrix(yellowSliceToRAS);
    yellowTransform->RotateY(RotY);
    yellowSliceToRAS->DeepCopy(yellowTransform->GetMatrix());
    yellowSliceNode->UpdateMatrices();

    d->YellowSliceSliderWidget->blockSignals(true);
    d->YellowSliceSliderWidget->setValue(d->parametersNode->GetYellowRotValue());
    d->YellowSliceSliderWidget->blockSignals(false);
    }
  else
    {
    d->YellowSliceSliderWidget->blockSignals(true);
    d->YellowSliceSliderWidget->setValue(0.);
    d->YellowSliceSliderWidget->blockSignals(false);
  }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onNormalizeNoneChanged(bool toggled)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (toggled)
    {
    d->parametersNode->SetNormalize("NONE");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onNormalizeLocalChanged(bool toggled)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (toggled)
    {
    d->parametersNode->SetNormalize("LOCAL");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onNormalizeAzimChanged(bool toggled)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (toggled)
    {
    d->parametersNode->SetNormalize("AZIM");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onNumberOfCloundsChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetNumberOfClounds(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onNumberOfRingsChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetNumberOfRings(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setMRMLAstroModelingParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroModelingParametersNode* AstroModelingParaNode =
      vtkMRMLAstroModelingParametersNode::SafeDownCast(mrmlNode);

  if (!AstroModelingParaNode || d->parametersNode == AstroModelingParaNode)
    {
    return;
    }

  d->parametersNode = AstroModelingParaNode;

  this->qvtkReconnect(d->parametersNode, AstroModelingParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroModelingParametersNodeModified()));

  this->onMRMLAstroModelingParametersNodeModified();

  this->qvtkReconnect(d->parametersNode, AstroModelingParaNode,
                      vtkMRMLAstroModelingParametersNode::YellowRotationModifiedEvent,
                      this, SLOT(onMRMLYellowSliceRotated()));

  this->onMRMLYellowSliceRotated();

  this->qvtkReconnect(d->parametersNode, AstroModelingParaNode,
                      vtkMRMLAstroModelingParametersNode::GreenRotationModifiedEvent,
                      this, SLOT(onMRMLGreenSliceRotated()));

  this->onMRMLGreenSliceRotated();

  this->setEnabled(AstroModelingParaNode != 0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::centerPVOffset()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode || !yellowSliceNode->GetSliceToRAS() ||
      yellowSliceNode->GetOrientation().compare("PVMajor"))
    {
    return;
    }
  yellowSliceNode->GetSliceToRAS()->SetElement(0, 3, d->parametersNode->GetXPosCenterRAS());
  yellowSliceNode->GetSliceToRAS()->SetElement(1, 3, d->parametersNode->GetYPosCenterRAS());
  yellowSliceNode->GetSliceToRAS()->SetElement(2, 3, d->parametersNode->GetZPosCenterRAS());
  yellowSliceNode->UpdateMatrices();
  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode || !greenSliceNode->GetSliceToRAS() ||
      greenSliceNode->GetOrientation().compare("PVMinor"))
    {
    return;
    }
  greenSliceNode->GetSliceToRAS()->SetElement(0, 3, d->parametersNode->GetXPosCenterRAS());
  greenSliceNode->GetSliceToRAS()->SetElement(1, 3, d->parametersNode->GetYPosCenterRAS());
  greenSliceNode->GetSliceToRAS()->SetElement(2, 3, d->parametersNode->GetZPosCenterRAS());
  greenSliceNode->UpdateMatrices();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onADRIFTCorrectionChanged(bool toggled)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetADRIFTCorrection(toggled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode || !d->selectionNode)
    {
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    return;
    }

  if (mrmlNode)
    {
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    d->XcenterSliderWidget->setMaximum(StringToInt(mrmlNode->GetAttribute("SlicerAstro.NAXIS1")));
    d->YcenterSliderWidget->setMaximum(StringToInt(mrmlNode->GetAttribute("SlicerAstro.NAXIS2")));
    }
  else
    {
    d->selectionNode->SetActiveVolumeID(nullptr);
    d->selectionNode->SetActiveVolumeID(nullptr);
    }

  this->updateBeamFromVolume(vtkMRMLAstroVolumeNode::SafeDownCast(mrmlNode));
  this->updateMaskSelector();
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onLayerTypeChanged(int value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetLayerType(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMaskActiveToggled(bool active)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetMaskActive(active);
  if (d->SearchGroupBox)
    {
    d->SearchGroupBox->setVisible(!active);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onBBaroloParameterChanged()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode || !d->TwoStageCheckBox)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetTwoStage(d->TwoStageCheckBox->isChecked());
  d->parametersNode->SetRegularizationType(d->RegularizationComboBox->currentText().trimmed().toStdString());
  d->parametersNode->SetSide(d->SideComboBox->currentData().toString().toStdString());
  d->parametersNode->SetInstrumentalBroadening(d->InstrumentalBroadeningSpinBox->value());
  d->parametersNode->SetRotationVelocityError(d->RotationVelocityErrorSpinBox->value());
  d->parametersNode->SetMinimumVelocityDispersion(d->MinimumDispersionSpinBox->value());
  d->parametersNode->SetMaximumVelocityDispersion(d->MaximumDispersionSpinBox->value());
  d->parametersNode->SetEstimateErrors(d->EstimateErrorsCheckBox->isChecked());
  d->parametersNode->SetStartRing(d->StartRingSpinBox->value());
  d->parametersNode->SetBlankWeight(d->BlankWeightSpinBox->value());
  d->parametersNode->SetNormalizeCube(d->NormalizeCubeCheckBox->isChecked());
  d->parametersNode->SetRedshift(d->RedshiftSpinBox->value());
  d->parametersNode->SetRestFrequencies(d->RestFrequenciesLineEdit->text().trimmed().toStdString());
  d->parametersNode->SetRelativeIntensities(d->RelativeIntensitiesLineEdit->text().trimmed().toStdString());
  d->parametersNode->SetSearchSNRCut(d->SearchSNRCutSpinBox->value());
  d->parametersNode->SetSearchThreshold(d->SearchThresholdSpinBox->value());
  d->parametersNode->SetSearchMinChannels(d->SearchMinChannelsSpinBox->value());
  d->parametersNode->SetSearchGrowthCut(d->SearchGrowthCutSpinBox->value());
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::updateBBaroloParameterWidgets()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode || !d->TwoStageCheckBox)
    {
    return;
    }
  QWidget* widgets[] = {d->TwoStageCheckBox, d->RegularizationComboBox, d->SideComboBox,
    d->InstrumentalBroadeningSpinBox, d->RotationVelocityErrorSpinBox, d->MinimumDispersionSpinBox,
    d->MaximumDispersionSpinBox, d->EstimateErrorsCheckBox, d->StartRingSpinBox, d->BlankWeightSpinBox,
    d->NormalizeCubeCheckBox, d->RedshiftSpinBox, d->RestFrequenciesLineEdit, d->RelativeIntensitiesLineEdit,
    d->SearchSNRCutSpinBox, d->SearchThresholdSpinBox, d->SearchMinChannelsSpinBox, d->SearchGrowthCutSpinBox};
  std::vector<bool> blocked;
  for (QWidget* widget : widgets)
    {
    blocked.push_back(widget->blockSignals(true));
    }
  d->TwoStageCheckBox->setChecked(d->parametersNode->GetTwoStage());
  d->RegularizationComboBox->setEnabled(d->parametersNode->GetTwoStage());
  d->RegularizationComboBox->setCurrentText(QString::fromStdString(d->parametersNode->GetRegularizationType()));
  const int sideIndex = d->SideComboBox->findData(QString::fromStdString(d->parametersNode->GetSide()));
  d->SideComboBox->setCurrentIndex(sideIndex >= 0 ? sideIndex : 0);
  d->InstrumentalBroadeningSpinBox->setValue(d->parametersNode->GetInstrumentalBroadening());
  d->RotationVelocityErrorSpinBox->setValue(d->parametersNode->GetRotationVelocityError());
  d->MinimumDispersionSpinBox->setValue(d->parametersNode->GetMinimumVelocityDispersion());
  d->MaximumDispersionSpinBox->setValue(d->parametersNode->GetMaximumVelocityDispersion());
  d->EstimateErrorsCheckBox->setChecked(d->parametersNode->GetEstimateErrors());
  d->StartRingSpinBox->setValue(d->parametersNode->GetStartRing());
  d->BlankWeightSpinBox->setValue(d->parametersNode->GetBlankWeight());
  d->NormalizeCubeCheckBox->setChecked(d->parametersNode->GetNormalizeCube());
  d->RedshiftSpinBox->setValue(d->parametersNode->GetRedshift());
  d->RestFrequenciesLineEdit->setText(QString::fromStdString(d->parametersNode->GetRestFrequencies()));
  d->RelativeIntensitiesLineEdit->setText(QString::fromStdString(d->parametersNode->GetRelativeIntensities()));
  d->SearchSNRCutSpinBox->setValue(d->parametersNode->GetSearchSNRCut());
  d->SearchThresholdSpinBox->setValue(d->parametersNode->GetSearchThreshold());
  d->SearchMinChannelsSpinBox->setValue(d->parametersNode->GetSearchMinChannels());
  d->SearchGrowthCutSpinBox->setValue(d->parametersNode->GetSearchGrowthCut());
  d->SearchGroupBox->setVisible(!d->parametersNode->GetMaskActive());
  for (size_t index = 0; index < blocked.size(); index++)
    {
    widgets[index]->blockSignals(blocked[index]);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setInitialParametersVisible(bool visible)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  // the initial parameters (and their estimate) and the advanced parameters
  // are set in Manual mode only: in Automatic mode 3DBarolo estimates them
  if (d->InitialParametersWidget)
    {
    d->InitialParametersWidget->setVisible(visible);
    }
  d->AdvancedCollapsibleButton->setVisible(visible);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onModeChanged()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();

  if (d->ManualModeRadioButton->isChecked())
    {
    d->parametersNode->SetMode("Manual");
    }
  if (d->AutomaticModeRadioButton->isChecked())
    {
    d->parametersNode->SetMode("Automatic");
    d->parametersNode->SetNumberOfRings(-1);
    d->parametersNode->SetRadSep(-1);
    d->parametersNode->SetXCenter(-1);
    d->parametersNode->SetYCenter(-1.);
    d->parametersNode->SetSystemicVelocity(-1);
    d->parametersNode->SetRotationVelocity(-1);
    d->parametersNode->SetVelocityDispersion(-1);
    d->parametersNode->SetRadialVelocity(0.);
    d->parametersNode->SetVerticalVelocity(0.);
    d->parametersNode->SetVerticalRotationalGradient(0.);
    d->parametersNode->SetVerticalRotationalGradientHeight(0.);
    d->parametersNode->SetInclination(-1);
    d->parametersNode->SetInclinationError(5.);
    d->parametersNode->SetPositionAngle(-1);
    d->parametersNode->SetPositionAngleError(15.);
    d->parametersNode->SetScaleHeight(-1);
    d->parametersNode->SetColumnDensity(1.);
    d->parametersNode->SetDistance(-1);
    d->parametersNode->SetPositionAngleFit(true);
    d->parametersNode->SetRotationVelocityFit(true);
    d->parametersNode->SetRadialVelocityFit(false);
    d->parametersNode->SetVelocityDispersionFit(true);
    d->parametersNode->SetInclinationFit(true);
    d->parametersNode->SetXCenterFit(false);
    d->parametersNode->SetYCenterFit(false);
    d->parametersNode->SetSystemicVelocityFit(false);
    d->parametersNode->SetScaleHeightFit(false);
    d->parametersNode->SetADRIFTCorrection(false);
    d->parametersNode->SetLayerType(0);
    d->parametersNode->SetFittingFunction(1);
    d->parametersNode->SetWeightingFunction(1);
    d->parametersNode->SetNumberOfClounds(-1);
    d->parametersNode->SetCloudsColumnDensity(10.);
    d->parametersNode->SetTollerance(0.001);
    }

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroModelingModuleWidget);

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

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onPlotSelectionChanged(vtkStringArray* mrmlPlotDataIDs,
                                                              vtkCollection* selectionCol)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  if (!d->fiducialNodeMajor || !d->fiducialNodeMinor || !this->mrmlScene() ||
      !mrmlPlotDataIDs || !selectionCol)
    {
    return;
    }

  d->fiducialNodeMajor->GlobalWarningDisplayOff();
  d->fiducialNodeMinor->GlobalWarningDisplayOff();

  for (int fiducialIndex = 0; fiducialIndex < d->fiducialNodeMajor->GetNumberOfControlPoints(); fiducialIndex++)
    {
    d->fiducialNodeMajor->SetNthControlPointSelected(fiducialIndex, false);
    }

  for (int mrmlPlotDataIndex = 0; mrmlPlotDataIndex < mrmlPlotDataIDs->GetNumberOfValues(); mrmlPlotDataIndex++)
    {
    vtkMRMLPlotSeriesNode* PlotSeriesNode = vtkMRMLPlotSeriesNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(mrmlPlotDataIDs->GetValue(mrmlPlotDataIndex)));
    if (!PlotSeriesNode)
      {
      continue;
      }
    if (!strcmp(PlotSeriesNode->GetName(), "VRot") ||
        !strcmp(PlotSeriesNode->GetName(), "VRad") ||
        !strcmp(PlotSeriesNode->GetName(), "Inc") ||
        !strcmp(PlotSeriesNode->GetName(), "VSys"))
      {
      vtkIdTypeArray *selectionArray = vtkIdTypeArray::SafeDownCast
        (selectionCol->GetItemAsObject(mrmlPlotDataIndex));
      if (!selectionArray)
        {
        continue;
        }
      for (int selectionArrayIndex = 0; selectionArrayIndex < selectionArray->GetNumberOfValues(); selectionArrayIndex++)
        {
        int positiveIndex = selectionArray->GetValue(selectionArrayIndex) * 2;
        int negativeIndex = (selectionArray->GetValue(selectionArrayIndex) * 2) + 1;
        d->fiducialNodeMajor->SetNthControlPointSelected(positiveIndex, true);
        d->fiducialNodeMajor->SetNthControlPointSelected(negativeIndex, true);
        }
      }
    }

  for (int fiducialIndex = 0; fiducialIndex < d->fiducialNodeMinor->GetNumberOfControlPoints(); fiducialIndex++)
    {
    d->fiducialNodeMinor->SetNthControlPointSelected(fiducialIndex, false);
    }

  for (int mrmlPlotDataIndex = 0; mrmlPlotDataIndex < mrmlPlotDataIDs->GetNumberOfValues(); mrmlPlotDataIndex++)
    {
    vtkMRMLPlotSeriesNode* PlotSeriesNode = vtkMRMLPlotSeriesNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(mrmlPlotDataIDs->GetValue(mrmlPlotDataIndex)));
    if (!PlotSeriesNode)
      {
      continue;
      }
    if (!strcmp(PlotSeriesNode->GetName(), "VRot") ||
        !strcmp(PlotSeriesNode->GetName(), "VRad") ||
        !strcmp(PlotSeriesNode->GetName(), "Inc") ||
        !strcmp(PlotSeriesNode->GetName(), "Phi"))
      {
      vtkIdTypeArray *selectionArray = vtkIdTypeArray::SafeDownCast
        (selectionCol->GetItemAsObject(mrmlPlotDataIndex));
      if (!selectionArray)
        {
        continue;
        }
      for (int selectionArrayIndex = 0; selectionArrayIndex < selectionArray->GetNumberOfValues(); selectionArrayIndex++)
        {
        int positiveIndex = selectionArray->GetValue(selectionArrayIndex) * 2;
        int negativeIndex = (selectionArray->GetValue(selectionArrayIndex) * 2) + 1;
        d->fiducialNodeMinor->SetNthControlPointSelected(positiveIndex, true);
        d->fiducialNodeMinor->SetNthControlPointSelected(negativeIndex, true);
        }
      }
    }

  d->fiducialNodeMajor->GlobalWarningDisplayOn();
  d->fiducialNodeMinor->GlobalWarningDisplayOn();
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onPositionAngleChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetPositionAngle(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onPositionAngleErrorChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetPositionAngleError(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onPositionAngleFitChanged(bool flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetPositionAngleFit(flag);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onRadSepChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetRadSep(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onResidualVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetResidualVolumeNodeID(mrmlNode->GetID());
    }
  else
    {
    d->parametersNode->SetResidualVolumeNodeID(nullptr);
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onRadialVelocityChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetRadialVelocity(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onRadialVelocityFitChanged(bool flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetRadialVelocityFit(flag);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onRotationVelocityChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetRotationVelocity(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onRotationVelocityFitChanged(bool flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetRotationVelocityFit(flag);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onScaleHeightChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetScaleHeight(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onScaleHeightFitChanged(bool flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetScaleHeightFit(flag);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLAstroModelingParametersNodeModified()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode || !mrmlScene())
    {
    return;
    }

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
      qSlicerApplication::application()->processEvents();
      }
    return;
    }

  const char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  const char *outputVolumeNodeID = d->parametersNode->GetOutputVolumeNodeID();
  vtkMRMLAstroVolumeNode *outputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(outputVolumeNodeID));
  d->OutputVolumeNodeSelector->setCurrentNode(outputVolumeNode);

  const char *residualVolumeNodeID = d->parametersNode->GetResidualVolumeNodeID();
  vtkMRMLAstroVolumeNode *residualVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(residualVolumeNodeID));
  d->ResidualVolumeNodeSelector->setCurrentNode(residualVolumeNode);

  d->MaskCheckBox->setChecked(d->parametersNode->GetMaskActive());
  d->SegmentsTableView->setEnabled(d->parametersNode->GetMaskActive());

  this->setInitialParametersVisible(strcmp(d->parametersNode->GetMode(), "Automatic") != 0);
  if (!(strcmp(d->parametersNode->GetMode(), "Automatic")))
    {
    d->AutomaticModeRadioButton->setChecked(true);
    }
  else
    {
    d->ManualModeRadioButton->setChecked(true);
    }

  d->RingsSliderWidget->setValue(d->parametersNode->GetNumberOfRings());
  d->RingWidthSliderWidget->setValue(d->parametersNode->GetRadSep());
  d->XcenterSliderWidget->setValue(d->parametersNode->GetXCenter());
  d->YcenterSliderWidget->setValue(d->parametersNode->GetYCenter());
  d->SysVelSliderWidget->setValue(d->parametersNode->GetSystemicVelocity());
  d->RotVelSliderWidget->setValue(d->parametersNode->GetRotationVelocity());
  d->RadVelSliderWidget->setValue(d->parametersNode->GetRadialVelocity());
  d->VerVelSliderWidget->setValue(d->parametersNode->GetVerticalVelocity());
  d->DVDZSliderWidget->setValue(d->parametersNode->GetVerticalRotationalGradient());
  d->ZCYLSliderWidget->setValue(d->parametersNode->GetVerticalRotationalGradientHeight());
  d->VelDispSliderWidget->setValue(d->parametersNode->GetVelocityDispersion());
  d->InclinationSliderWidget->setValue(d->parametersNode->GetInclination());
  d->InclinationErrorSpinBox->setValue(d->parametersNode->GetInclinationError());
  d->PASliderWidget->setValue(d->parametersNode->GetPositionAngle());
  d->PAErrorSpinBox->setValue(d->parametersNode->GetPositionAngleError());
  d->SHSliderWidget->setValue(d->parametersNode->GetScaleHeight());
  d->CDSliderWidget->setValue(d->parametersNode->GetColumnDensity());
  d->DistanceSliderWidget->setValue(d->parametersNode->GetDistance());
  d->PARadioButton->setChecked(d->parametersNode->GetPositionAngleFit());
  d->DISPRadioButton->setChecked(d->parametersNode->GetVelocityDispersionFit());
  d->VROTRadioButton->setChecked(d->parametersNode->GetRotationVelocityFit());
  d->VRadRadioButton->setChecked(d->parametersNode->GetRadialVelocityFit());
  d->INCRadioButton->setChecked(d->parametersNode->GetInclinationFit());
  d->XCenterRadioButton->setChecked(d->parametersNode->GetXCenterFit());
  d->YCenterRadioButton->setChecked(d->parametersNode->GetYCenterFit());
  d->VSYSRadioButton->setChecked(d->parametersNode->GetSystemicVelocityFit());
  d->SCRadioButton->setChecked(d->parametersNode->GetScaleHeightFit());
  d->ADRIFTCheckBox->setChecked(d->parametersNode->GetADRIFTCorrection());
  d->LayerTypeComboBox->setCurrentIndex(d->parametersNode->GetLayerType());
  d->FittingFunctionComboBox->setCurrentIndex(d->parametersNode->GetFittingFunction());
  d->WeightingFunctionComboBox->setCurrentIndex(d->parametersNode->GetWeightingFunction());
  d->NumCloudsSliderWidget->setValue(d->parametersNode->GetNumberOfClounds());
  d->CloudCDSliderWidget->setValue(d->parametersNode->GetCloudsColumnDensity());
  d->TolleranceSliderWidget->setValue(d->parametersNode->GetTollerance());

  d->ContourSliderWidget->setValue(d->parametersNode->GetContourLevel());

  d->NormalizeNonePushButton->blockSignals(true);
  d->NormalizeLocalPushButton->blockSignals(true);
  d->NormalizeAzimPushButton->blockSignals(true);
  if (!strcmp(d->parametersNode->GetNormalize(), "NONE"))
    {
    d->NormalizeNonePushButton->setChecked(true);
    d->NormalizeLocalPushButton->setChecked(false);
    d->NormalizeAzimPushButton->setChecked(false);
    }
  else if (!strcmp(d->parametersNode->GetNormalize(), "LOCAL"))
    {
    d->NormalizeNonePushButton->setChecked(false);
    d->NormalizeLocalPushButton->setChecked(true);
    d->NormalizeAzimPushButton->setChecked(false);
    }
  else if (!strcmp(d->parametersNode->GetNormalize(), "AZIM"))
    {
    d->NormalizeNonePushButton->setChecked(false);
    d->NormalizeLocalPushButton->setChecked(false);
    d->NormalizeAzimPushButton->setChecked(true);
    } 
  d->NormalizeNonePushButton->blockSignals(false);
  d->NormalizeLocalPushButton->blockSignals(false);
  d->NormalizeAzimPushButton->blockSignals(false);

  d->TableView->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourSliderWidget->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourLabel->setEnabled(d->parametersNode->GetFitSuccess());
  d->VisualizePushButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->VisualizeMaskPushButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->CalculatePushButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->AstroModelingCopyButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->AstroModelingPasteButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->AstroModelingPlotButton->setEnabled(d->parametersNode->GetFitSuccess());

  d->OutputCollapsibleButton_2->setEnabled(d->parametersNode->GetFitSuccess());
  d->YellowSliceLabel->setEnabled(d->parametersNode->GetFitSuccess());
  d->YellowSliceSliderWidget->setEnabled(d->parametersNode->GetFitSuccess());
  d->GreenSliceLabel->setEnabled(d->parametersNode->GetFitSuccess());
  d->GreenSliceSliderWidget->setEnabled(d->parametersNode->GetFitSuccess());

  // Set params table to table view
  d->TableNodeComboBox->setCurrentNode(d->parametersNode->GetParamsTableNode());
  d->TableView->setMRMLTableNode(d->parametersNode->GetParamsTableNode());

  this->updateBBaroloParameterWidgets();
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLGreenSliceRotated()
{
  Q_D(const qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode)
    {
    return;
    }

  vtkMatrix4x4* greenSliceToRAS = greenSliceNode->GetSliceToRAS();
  if (!greenSliceToRAS)
    {
    return;
    }

  double RotY = d->parametersNode->GetGreenRotValue() -
                d->parametersNode->GetGreenRotOldValue();
  if (fabs(RotY) > 1.E-6)
    {
    if (fabs(greenSliceToRAS->GetElement(0, 3) - d->parametersNode->GetXPosCenterRAS()) > 1.E-6 ||
        fabs(greenSliceToRAS->GetElement(1, 3) - d->parametersNode->GetYPosCenterRAS()) > 1.E-6 ||
        fabs(greenSliceToRAS->GetElement(2, 3) - d->parametersNode->GetZPosCenterRAS()) > 1.E-6)
      {
      greenSliceToRAS->SetElement(0, 3, d->parametersNode->GetXPosCenterRAS());
      greenSliceToRAS->SetElement(1, 3, d->parametersNode->GetYPosCenterRAS());
      greenSliceToRAS->SetElement(2, 3, d->parametersNode->GetZPosCenterRAS());
      }

    vtkNew<vtkTransform> greenTransform;
    greenTransform->SetMatrix(greenSliceToRAS);
    greenTransform->RotateY(RotY);
    greenSliceToRAS->DeepCopy(greenTransform->GetMatrix());
    greenSliceNode->UpdateMatrices();
    d->GreenSliceSliderWidget->blockSignals(true);
    d->GreenSliceSliderWidget->setValue(d->parametersNode->GetGreenRotValue());
    d->GreenSliceSliderWidget->blockSignals(false);
    }
  else
    {
    d->GreenSliceSliderWidget->blockSignals(true);
    d->GreenSliceSliderWidget->setValue(0.);
    d->GreenSliceSliderWidget->blockSignals(false);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLSceneEndImportEvent()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onMRMLSceneEndImportEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onMRMLSceneEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();

  if (!d->parametersNode)
    {
    return;
    }

  if (!d->parametersNode->GetFitSuccess())
    {
    d->InputSegmentCollapsibleButton->setCollapsed(true);
    d->FittingParametersCollapsibleButton->setCollapsed(false);
    d->OutputCollapsibleButton->setCollapsed(true);
    d->OutputCollapsibleButton_2->setCollapsed(true);
    }
  else
    {
    d->InputSegmentCollapsibleButton->setCollapsed(true);
    d->FittingParametersCollapsibleButton->setCollapsed(true);
    d->OutputCollapsibleButton->setCollapsed(false);
    d->OutputCollapsibleButton_2->setCollapsed(false);
    this->onCalculateAndVisualize();

    vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
    if (!yellowSliceNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::onMRMLSceneEndImportEvent : "
                    "yellowSliceNode not found!";
      return;
      }
    vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
    if (!greenSliceNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::onMRMLSceneEndImportEvent : "
                    "greenSliceNode not found!";
      return;
      }

    this->qvtkConnect(yellowSliceNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSliceNodeModified(vtkObject*)));
    this->qvtkConnect(greenSliceNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSliceNodeModified(vtkObject*)));
    this->onMRMLSliceNodeModified(yellowSliceNode);
    this->onMRMLSliceNodeModified(greenSliceNode);
    }

  this->onMRMLAstroModelingParametersNodeModified();

  // Select VRot in the plotting window
  d->selectionNode->SetActivePlotChartID(d->plotChartNodeVRot->GetID());
  appLogic->PropagatePlotChartSelection();
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLSceneEndRestoreEvent()
{
  this->onMRMLAstroModelingParametersNodeModified();
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLSceneEndBatchProcessEvent()
{
  this->onMRMLAstroModelingParametersNodeModified();
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLSceneEndCloseEvent()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onMRMLSceneEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();

  if (!d->parametersNode)
    {
    return;
    }

  if (!d->parametersNode->GetFitSuccess())
    {
    d->InputSegmentCollapsibleButton->setCollapsed(true);
    d->FittingParametersCollapsibleButton->setCollapsed(false);
    d->OutputCollapsibleButton->setCollapsed(true);
    d->OutputCollapsibleButton_2->setCollapsed(true);
    }
  else
    {
    d->InputSegmentCollapsibleButton->setCollapsed(true);
    d->FittingParametersCollapsibleButton->setCollapsed(true);
    d->OutputCollapsibleButton->setCollapsed(false);
    d->OutputCollapsibleButton_2->setCollapsed(false);
    }

  this->onMRMLAstroModelingParametersNodeModified();
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLSceneStartCloseEvent()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  d->cleanPointers();
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLSceneStartImportEvent()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  d->cleanPointers();
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onEstimateInitialParameters()
{
  Q_D(const qSlicerAstroModelingModuleWidget);

  if (d->workerRunning())
    {
    // one 3DBarolo operation at a time (e.g. a second click)
    return;
    }

  if (!d->parametersNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onEstimateInitialParameters()"
                  " : parametersNode not found!";
    return;
    }

  d->parametersNode->SetOperation(vtkMRMLAstroModelingParametersNode::ESTIMATE);

  this->onApply();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onCreate()
{
  Q_D(const qSlicerAstroModelingModuleWidget);

  if (d->workerRunning())
    {
    // one 3DBarolo operation at a time (e.g. a second click)
    return;
    }

  if (!d->parametersNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onEstimateInitialParameters()"
                  " : parametersNode not found!";
    return;
    }

  d->parametersNode->SetOperation(vtkMRMLAstroModelingParametersNode::CREATE);

  this->onApply();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onFit()
{
  Q_D(const qSlicerAstroModelingModuleWidget);

  if (d->workerRunning())
    {
    // one 3DBarolo operation at a time (e.g. a second click)
    return;
    }

  if (!d->parametersNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onEstimateInitialParameters()"
                  " : parametersNode not found!";
    return;
    }

  d->parametersNode->SetOperation(vtkMRMLAstroModelingParametersNode::FIT);

  this->onApply();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::updateBeamFromVolume(vtkMRMLAstroVolumeNode* volumeNode)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->BeamMajorSpinBox)
    {
    return;
    }
  double beam[3] = {0., 0., 0.};
  QString source;
  const char* bmaj = volumeNode ? volumeNode->GetAttribute("SlicerAstro.BMAJ") : nullptr;
  const char* bmin = volumeNode ? volumeNode->GetAttribute("SlicerAstro.BMIN") : nullptr;
  const char* bpa = volumeNode ? volumeNode->GetAttribute("SlicerAstro.BPA") : nullptr;
  if (bmaj && bmin && strcmp(bmaj, "UNDEFINED") && strcmp(bmin, "UNDEFINED") &&
      StringToDouble(bmaj) > 0. && StringToDouble(bmin) > 0.)
    {
    beam[0] = StringToDouble(bmaj);
    beam[1] = StringToDouble(bmin);
    beam[2] = (bpa && strcmp(bpa, "UNDEFINED")) ? StringToDouble(bpa) : 0.;
    source = tr("from the FITS header");
    }
  else if (volumeNode && vtkSlicerAstroVolumeLogic::EstimateBeamFromNoise(volumeNode, beam))
    {
    source = tr("estimated from the noise (spatial autocorrelation of the first and last "
                "channels): check it");
    }
  else if (volumeNode)
    {
    source = tr("not in the FITS header and not estimated (the noise is not correlated): "
                "enter it");
    }
  d->BeamMajorSpinBox->setValue(beam[0] * 3600.);
  d->BeamMinorSpinBox->setValue(beam[1] * 3600.);
  d->BeamPASpinBox->setValue(beam[2]);
  d->BeamLabel->setText(source.startsWith(tr("from")) || source.isEmpty() ? tr("Beam:") : tr("Beam (!):"));
  d->BeamLabel->setToolTip(d->BeamMajorSpinBox->toolTip() + (source.isEmpty() ? QString() : "\n\n" + tr("Beam ") + source + "."));
}

//-----------------------------------------------------------------------------
bool qSlicerAstroModelingModuleWidget::applyBeamToInputVolume(vtkMRMLAstroVolumeNode* inputVolume)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!inputVolume || !d->BeamMajorSpinBox)
    {
    return false;
    }
  double bmaj = d->BeamMajorSpinBox->value() / 3600.;
  double bmin = d->BeamMinorSpinBox->value() / 3600.;
  if (bmaj <= 0. || bmin <= 0.)
    {
    return false;
    }
  if (bmin > bmaj)
    {
    std::swap(bmaj, bmin);
    }
  // the beam of the data (BBarolo, beam display, saved FITS header)
  std::ostringstream bmajString, bminString, bpaString;
  bmajString << std::setprecision(10) << bmaj;
  bminString << std::setprecision(10) << bmin;
  bpaString << std::setprecision(10) << d->BeamPASpinBox->value();
  inputVolume->SetAttribute("SlicerAstro.BMAJ", bmajString.str().c_str());
  inputVolume->SetAttribute("SlicerAstro.BMIN", bminString.str().c_str());
  inputVolume->SetAttribute("SlicerAstro.BPA", bpaString.str().c_str());
  return true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onApply()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (d->workerRunning())
    {
    // one 3DBarolo operation at a time (e.g. a second click)
    return;
    }

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onApply() : "
                   "parametersNode not found!";
    return;
    }

  vtkSlicerAstroModelingLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : "
                  "astroModelingLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!this->mrmlScene())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : "
                  "scene not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->parametersNode->GetParamsTableNode() || !d->internalTableNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onApply() : "
                   "TableNode not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->parametersNode->SetStatus(1);
  if (d->parametersNode->GetParamsTableNode()->GetNumberOfRows() > 0)
    {
    this->initializeTableNode(true);
    }

  d->internalTableNode->Copy(d->parametersNode->GetParamsTableNode());
  d->parametersNode->SetFitSuccess(false);

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : "
                  "scene not found!";
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : "
                  "inputVolume not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  // Check Input volume
  int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  if (n != 3)
    {
    QString message = QString("Model fitting is  available only"
                              " for datacube with dimensionality 3 (NAXIS = 3).");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(nullptr, tr("Failed to run 3DBarolo"), message);
    d->parametersNode->SetStatus(0);
    return;
    }

  // beam of the Fitting Parameters section (header, estimate or user)
  if (!this->applyBeamToInputVolume(inputVolume))
    {
    QString message = tr("The beam of the data is needed for the model (it is not in the FITS "
                         "header and could not be estimated from the noise). Enter it in the "
                         "Fitting Parameters section: FWHM of the major and minor axes in arcsec "
                         "and position angle in degrees.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(nullptr, tr("Failed to run 3DBarolo"), message);
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeDisplayNode *inputVolumeDisplayNode =
    inputVolume->GetAstroVolumeDisplayNode();
  if(!inputVolumeDisplayNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : "
                  "inputVolumeDisplay not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  // Create Output Volume
  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_model";

  int serial = d->parametersNode->GetOutputSerial();
  outSS<<"_"<< IntToString(serial);

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
    else if (name.find("_model_") != std::string::npos)
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
        scene->RemoveNode(volumeRenderingDisplay);
        }
      scene->RemoveNode(outputVolume);
      }
    }

  outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
     (logic->GetAstroVolumeLogic()->CloneVolume(scene, inputVolume, outSS.str().c_str()));

  d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());

  vtkMRMLNode* node = nullptr;
  outputVolume->SetPresetNode(node);

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

  // Create Residual Volume
  std::ostringstream residualSS;
  residualSS << inputVolume->GetName() << "_maskedByModel_"<<
             IntToString(serial);
  serial++;
  d->parametersNode->SetOutputSerial(serial);

  vtkMRMLAstroVolumeNode *residualVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetResidualVolumeNodeID()));

  if (residualVolume)
    {
    std::string name;
    name = residualVolume->GetName();
    if (name.find("_maskedByModel_") != std::string::npos)
      {
      vtkMRMLAstroVolumeStorageNode* astroStorage =
        vtkMRMLAstroVolumeStorageNode::SafeDownCast(residualVolume->GetStorageNode());
      scene->RemoveNode(astroStorage);
      scene->RemoveNode(residualVolume->GetAstroVolumeDisplayNode());

      vtkMRMLVolumeRenderingDisplayNode *volumeRenderingDisplay =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(residualVolume->GetDisplayNode());
      if (volumeRenderingDisplay)
        {
        scene->RemoveNode(volumeRenderingDisplay->GetROINode());
        scene->RemoveNode(volumeRenderingDisplay);
        }
      scene->RemoveNode(residualVolume);
      }
    }

  residualVolume = vtkMRMLAstroVolumeNode::SafeDownCast
     (logic->GetAstroVolumeLogic()->CloneVolume(scene, inputVolume, residualSS.str().c_str()));

  d->parametersNode->SetResidualVolumeNodeID(residualVolume->GetID());

  ndnodes = residualVolume->GetNumberOfDisplayNodes();
  for (int ii = 0; ii < ndnodes; ii++)
    {
    vtkMRMLVolumeRenderingDisplayNode *dnode =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
        residualVolume->GetNthDisplayNode(ii));
    if (dnode)
      {
      residualVolume->RemoveNthDisplayNodeID(ii);
      }
    }

  inputVolume->GetRASToIJKMatrix(transformationMatrix.GetPointer());
  residualVolume->SetRASToIJKMatrix(transformationMatrix.GetPointer());
  residualVolume->SetAndObserveTransformNodeID(inputVolume->GetTransformNodeID());

  // Check if there are segments and feed the mask to 3DBarolo
  if (d->parametersNode->GetMaskActive())
    {
    if (!this->convertSelectedSegmentToLabelMap())
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : "
                    "convertSelectedSegmentToLabelMap failed!";
      d->parametersNode->SetStatus(0);
      return;
      }
    }
  else if (!d->parametersNode->GetMaskActive() && d->parametersNode->GetNumberOfRings() == -1)
    {
    QString message = QString("No mask has been provided. 3DBarolo will search the "
                              "largest source in the datacube. The mask is necessary for fitting "
                              "and/or normalizing (see advanced options) the output model. "
                              "Do you wish to continue?");
    qWarning() << Q_FUNC_INFO << ": " << message;
    // no question in testing mode (the self-tests run without a mask); the
    // answer can be remembered ("Don't show this message again", stored in
    // the application settings)
    int reply = QMessageBox::Yes;
    qSlicerApplication* application = qSlicerApplication::application();
    if (!application || !application->testAttribute(qSlicerCoreApplication::AA_EnableTesting))
      {
      ctkMessageBox confirmation(this);
      confirmation.setWindowTitle(tr("3DBarolo"));
      confirmation.setIcon(QMessageBox::Question);
      confirmation.setText(message);
      confirmation.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      confirmation.setDefaultButton(QMessageBox::Yes);
      confirmation.setDontShowAgainVisible(true);
      // only "Yes" is remembered (ctkMessageBox remembers AcceptRole buttons
      // by default, and Yes has the YesRole)
      confirmation.setDontShowAgainButtonRoles(QList<QMessageBox::ButtonRole>() << QMessageBox::YesRole);
      confirmation.setDontShowAgainSettingsKey("SlicerAstro/Modeling/DontAskSearchWithoutMask");
      reply = confirmation.exec();
      }
    if (reply != QMessageBox::Yes)
      {
      d->parametersNode->SetStatus(0);
      return;
      }
    }

  // 3DBarolo works with the optical velocity definition: converted here in
  // the GUI thread, restored when the work is finished
  d->restoreRadioVelocity = !strncmp(inputVolumeDisplayNode->GetVelocityDefinition().c_str(), "VRAD", 4);
  if (d->restoreRadioVelocity)
    {
    inputVolumeDisplayNode->SetOpticalVelocityDefinition();
    }

  d->appendOperationHeader(d->parametersNode->GetOperation());
  // the worker thread works on a copy of the parameters
  d->workerParametersNode = vtkSmartPointer<vtkMRMLAstroModelingParametersNode>::New();
  d->workerParametersNode->Copy(d->parametersNode);

  d->worker->SetTableNode(d->internalTableNode);
  d->worker->SetAstroModelingParametersNode(d->workerParametersNode);
  d->worker->SetAstroModelingLogic(logic);
  d->workerProgressTimer->start();
  d->worker->requestWork();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->CancelPushButton->setEnabled(true);
  d->CancelPushButton->setText(tr("Cancel"));
  d->CancelPushButton->hide();
  d->progressBar->hide();
  d->calculateProgressBar->hide();
  d->calculatingModel = false;
  d->FitPushButton->show();
  d->CreatePushButton->show();
  d->EstimateInitialParametersPushButton->setEnabled(true);
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onContourLevelChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetContourLevel(value);
  // the model and the data at the new level (no button)
  if (d->parametersNode->GetFitSuccess())
    {
    d->contourLevelTimer->start();
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onDistanceChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetDistance(value);
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroModelingModuleWidget);

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
void qSlicerAstroModelingModuleWidget::onMaskSegmentationChanged(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroModelingModuleWidget);

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
void qSlicerAstroModelingModuleWidget::updateMaskSelector()
{
  Q_D(qSlicerAstroModelingModuleWidget);

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

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onSystemicVelocityChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetSystemicVelocity(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onSystemicVelocityFitChanged(bool flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetSystemicVelocityFit(flag);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onTolleranceChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetTollerance(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onTableNodeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLTableNode *tableNode = vtkMRMLTableNode::SafeDownCast(mrmlNode);
  if (d->astroTableNode == tableNode)
    {
    return;
    }

  d->astroTableNode = tableNode;
  this->qvtkReconnect(d->astroTableNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLTableNodeModified()));

  d->parametersNode->SetParamsTableNode(tableNode);

  d->PlotSeriesNodeVRot->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
  d->PlotSeriesNodeVRad->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
  d->PlotSeriesNodeInc->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
  d->PlotSeriesNodePhi->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
  d->PlotSeriesNodeVSys->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
  d->PlotSeriesNodeVDisp->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
  d->PlotSeriesNodeDens->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
  d->PlotSeriesNodeZ0->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
  d->PlotSeriesNodeXPos->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
  d->PlotSeriesNodeYPos->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onVelocityDispersionChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetVelocityDispersion(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onVelocityDispersionFitChanged(bool flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetVelocityDispersionFit(flag);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onVerticalVelocityChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetVerticalVelocity(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onVerticalRotationalGradientChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetVerticalRotationalGradient(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onVerticalRotationalGradientHeightChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetVerticalRotationalGradientHeight(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onVisualize()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (d->workerRunning())
    {
    // one 3DBarolo operation at a time (e.g. a second click)
    return;
    }

  if (!d->parametersNode || !d->selectionNode ||
      !this->mrmlScene() || !d->astroVolumeWidget)
    {
    return;
    }

  if (!d->parametersNode->GetParamsTableNode() ||
      !d->parametersNode->GetParamsTableNode()->GetTable())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onVisualize : "
                  "Table not found!";
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onVisualize : appLogic not found!";
    return;
    }

  const char *activeVolumeNodeID = d->selectionNode->GetActiveVolumeID();
  const char *secondaryVolumeNodeID = d->selectionNode->GetSecondaryVolumeID();

  vtkMRMLAstroVolumeNode *activeVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(activeVolumeNodeID));
  if (!activeVolume || !activeVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "activeVolume not found!";
    return;
    }

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!activeVolume || !activeVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "yellowSliceNode not found!";
    return;
    }

  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!activeVolume || !activeVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "greenSliceNode not found!";
    return;
    }

  double yellowRAS[3] = {0., 0., 0.}, greenRAS[3] = {0., 0., 0.};
  for (int ii = 0; ii < 3; ii++)
    {
    yellowRAS[ii] = yellowSliceNode->GetSliceToRAS()->GetElement(ii, 3);
    greenRAS[ii] = greenSliceNode->GetSliceToRAS()->GetElement(ii, 3);
    }

  d->astroVolumeWidget->updateQuantitative3DView
        (activeVolumeNodeID,
         secondaryVolumeNodeID,
         d->parametersNode->GetContourLevel(),
         d->parametersNode->GetPVPhi() + d->parametersNode->GetYellowRotValue(),
         d->parametersNode->GetPVPhi() + 90. + d->parametersNode->GetGreenRotValue(),
         yellowRAS, greenRAS, false);

  // the model and its region as chosen by the user (the redraw shows them)
  this->applyModelingSegmentVisibility();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setup()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  // Create shortcuts for copy/paste
  d->CopyAction = new QAction(this);
  d->CopyAction->setIcon(QIcon(":Icons/Medium/SlicerEditCopy.png"));
  d->CopyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  // set CTRL+C shortcut
  d->CopyAction->setShortcuts(QKeySequence::Copy);
  d->CopyAction->setToolTip(tr("Copy"));
  this->addAction(d->CopyAction);
  d->PasteAction = new QAction(this);
  d->PasteAction->setIcon(QIcon(":Icons/Medium/SlicerEditPaste.png"));
  d->PasteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  // set CTRL+V shortcut
  d->PasteAction->setShortcuts(QKeySequence::Paste);
  d->PasteAction->setToolTip(tr("Paste"));
  this->addAction(d->PasteAction);
  d->PlotAction = new QAction(this);
  d->PlotAction->setIcon(QIcon(":Icons/Medium/SlicerInteractivePlotting.png"));
  d->PlotAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  // set CTRL+P shortcut
  d->PlotAction->setShortcuts(QKeySequence::Print);
  d->PlotAction->setToolTip(tr("Generate an Interactive Plot based on user-selection of"
                                " the columns of the table. The First (from left to right)"
                                " Column will be used as X-Axis and each additional Column"
                                " will be plotted in the same Plot as Y-Axis. "
                                "If the selection is only one Column, the Column will be"
                                " used as Y-Axis and the X-Axis will be indexes."));
  this->addAction(d->PlotAction);

  // Connect copy, paste and plot actions
  d->AstroModelingCopyButton->setDefaultAction(d->CopyAction);
  this->connect(d->CopyAction, SIGNAL(triggered()), d->TableView, SLOT(copySelection()));
  d->AstroModelingPasteButton->setDefaultAction(d->PasteAction);
  this->connect(d->PasteAction, SIGNAL(triggered()), d->TableView, SLOT(pasteSelection()));
  d->AstroModelingPlotButton->setDefaultAction(d->PlotAction);
  this->connect(d->PlotAction, SIGNAL(triggered()), d->TableView, SLOT(plotSelection()));

  // Table View resize options
  d->TableView->resizeColumnsToContents();
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onWeightingFunctionChanged(int flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetWeightingFunction(flag);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onWorkFinished()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  // results of the worker thread (parameters and status) in the parameter
  // node, in the GUI thread
  d->workerProgressTimer->stop();
  if (d->logic())
    {
    d->appendOutputLog(QString::fromStdString(d->logic()->TakeLog()));
    }
  if (d->workerParametersNode && d->parametersNode)
    {
    d->parametersNode->Copy(d->workerParametersNode);
    }
  d->workerParametersNode = nullptr;

  // velocity definition of the input restored
  if (d->restoreRadioVelocity && d->parametersNode && this->mrmlScene())
    {
    vtkMRMLAstroVolumeNode* input = vtkMRMLAstroVolumeNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
    if (input && input->GetAstroVolumeDisplayNode())
      {
      input->GetAstroVolumeDisplayNode()->SetRadioVelocityDefinition();
      }
    }
  d->restoreRadioVelocity = false;

  // Calculate and Visualize Model: visualization of the model of the table
  if (d->parametersNode && d->parametersNode->GetOperation() == vtkMRMLAstroModelingParametersNode::UPDATE)
    {
    this->finishCalculateAndVisualize(d->worker->GetLastResult() != 0);
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->astroVolumeWidget)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "astroVolumeWidget not found!";
    d->TableView->resizeColumnsToContents();
    return;
    }

  if (!d->parametersNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "parametersNode not found!";
    d->TableView->resizeColumnsToContents();
    return;
    }

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "scene not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "inputVolume node not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkImageData* imageData = inputVolume->GetImageData();
  if (!imageData)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "imageData not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkPointData* pointData = imageData->GetPointData();
  if (!pointData)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "pointData not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkDataArray *dataArray = pointData->GetScalars();
  if (!dataArray)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "dataArray not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeDisplayNode *inputVolumeDisplayNode =
    inputVolume->GetAstroVolumeDisplayNode();
  if(!inputVolumeDisplayNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "display node not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));
  if(!outputVolume)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "outputVolume node not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeNode *residualVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetResidualVolumeNodeID()));
  if(!residualVolume)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "residualVolume node not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->parametersNode->GetParamsTableNode() ||
      !d->parametersNode->GetParamsTableNode()->GetTable())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "Table not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onWorkFinished : "
                  "yellowSliceNode not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }
  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onWorkFinished : "
                  "greenSliceNode not found!";
    d->TableView->resizeColumnsToContents();
    d->parametersNode->SetStatus(0);
    return;
    }

  if (d->parametersNode->GetFitSuccess())
    {
    int wasModifying = outputVolume->StartModify();
    outputVolume->UpdateRangeAttributes();
    outputVolume->UpdateDisplayThresholdAttributes();
    outputVolume->SetAttribute("SlicerAstro.DATAMODEL", "MODEL");
    outputVolume->EndModify(wasModifying);

    wasModifying = residualVolume->StartModify();
    residualVolume->UpdateRangeAttributes();
    residualVolume->UpdateDisplayThresholdAttributes();
    residualVolume->SetAttribute("SlicerAstro.DATAMODEL", "DATA");
    residualVolume->EndModify(wasModifying);

    if (!d->internalTableNode || !d->internalTableNode->GetTable())
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                    "tables not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    d->parametersNode->GetParamsTableNode()->GetTable()->DeepCopy(d->internalTableNode->GetTable());
    d->PlotSeriesNodeVRot->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
    d->PlotSeriesNodeVRad->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
    d->PlotSeriesNodeInc->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
    d->PlotSeriesNodePhi->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
    d->PlotSeriesNodeVSys->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
    d->PlotSeriesNodeVDisp->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
    d->PlotSeriesNodeDens->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
    d->PlotSeriesNodeZ0->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
    d->PlotSeriesNodeXPos->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());
    d->PlotSeriesNodeYPos->SetAndObserveTableNodeID(d->parametersNode->GetParamsTableNode()->GetID());

    vtkDoubleArray* Phi = vtkDoubleArray::SafeDownCast
      (d->parametersNode->GetParamsTableNode()
         ->GetTable()->GetColumnByName("Phi"));

    vtkDoubleArray* XPos = vtkDoubleArray::SafeDownCast
      (d->parametersNode->GetParamsTableNode()
         ->GetTable()->GetColumnByName("XPos"));

    vtkDoubleArray* YPos = vtkDoubleArray::SafeDownCast
      (d->parametersNode->GetParamsTableNode()
         ->GetTable()->GetColumnByName("YPos"));

    vtkDoubleArray* VRot = vtkDoubleArray::SafeDownCast
      (d->parametersNode->GetParamsTableNode()
         ->GetTable()->GetColumnByName("VRot"));

    vtkDoubleArray* VRad = vtkDoubleArray::SafeDownCast
      (d->parametersNode->GetParamsTableNode()
         ->GetTable()->GetColumnByName("VRad"));

    vtkDoubleArray* Inc = vtkDoubleArray::SafeDownCast
      (d->parametersNode->GetParamsTableNode()
         ->GetTable()->GetColumnByName("Inc"));

    vtkDoubleArray* VSys = vtkDoubleArray::SafeDownCast
      (d->parametersNode->GetParamsTableNode()
         ->GetTable()->GetColumnByName("VSys"));

    vtkDoubleArray* Radii = vtkDoubleArray::SafeDownCast
      (d->parametersNode->GetParamsTableNode()
         ->GetTable()->GetColumnByName("Radii"));

    if (!Phi || !XPos || !YPos || !VRot || !VRad || !Inc || !VSys || !Radii)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                    "arrays not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    double PhiMean = 0., XPosMean = 0., YPosMean = 0.;

    XPosMean = 0.;
    YPosMean = 0.;
    for (int ii = 0; ii < Phi->GetNumberOfValues(); ii++)
      {
      PhiMean += Phi->GetValue(ii);
      XPosMean += XPos->GetValue(ii);
      YPosMean += YPos->GetValue(ii);
      }

    PhiMean /=  Phi->GetNumberOfValues();
    XPosMean /=  XPos->GetNumberOfValues();
    YPosMean /=  YPos->GetNumberOfValues();

    wasModifying = d->parametersNode->StartModify();
    d->parametersNode->SetXPosCenterIJK(XPosMean);
    d->parametersNode->SetYPosCenterIJK(YPosMean);
    double PVPhi = -(PhiMean - 90.);
    d->parametersNode->SetPVPhi(PVPhi);
    d->parametersNode->SetYellowRotOldValue(0.);
    d->parametersNode->SetYellowRotValue(0.);
    d->parametersNode->SetGreenRotOldValue(0.);
    d->parametersNode->SetGreenRotValue(0.);
    d->parametersNode->EndModify(wasModifying);

    int *dims = imageData->GetDimensions();
    int Zcenter = dims[2] * 0.5;

    vtkNew<vtkGeneralTransform> IJKtoRASTransform;
    IJKtoRASTransform->Identity();
    IJKtoRASTransform->PostMultiply();
    vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
    inputVolume->GetIJKToRASMatrix(IJKtoRASMatrix.GetPointer());
    IJKtoRASTransform->Concatenate(IJKtoRASMatrix.GetPointer());

    double ijk[3] = {0.,0.,0.}, RAS[3] = {0.,0.,0.};
    ijk[0] = d->parametersNode->GetXPosCenterIJK();
    ijk[1] = d->parametersNode->GetYPosCenterIJK();
    ijk[2] = Zcenter;
    IJKtoRASTransform->TransformPoint(ijk, RAS);
    wasModifying = d->parametersNode->StartModify();
    d->parametersNode->SetXPosCenterRAS(RAS[0]);
    d->parametersNode->SetYPosCenterRAS(RAS[1]);
    d->parametersNode->SetZPosCenterRAS(RAS[2]);
    d->parametersNode->EndModify(wasModifying);

    d->astroVolumeWidget->setQuantitative3DView
        (inputVolume->GetID(), outputVolume->GetID(),
         residualVolume->GetID(), d->parametersNode->GetContourLevel(),
         PVPhi,
         PVPhi + 90.,
         RAS);

    this->updateModelingButtons();

    d->InputSegmentCollapsibleButton->setCollapsed(true);
    d->FittingParametersCollapsibleButton->setCollapsed(true);
    d->OutputCollapsibleButton->setCollapsed(false);
    d->OutputCollapsibleButton_2->setCollapsed(false);

    // Select VRot in the plotting window
    if (d->selectionNode)
      {
      d->selectionNode->SetActivePlotChartID(d->plotChartNodeVRot->GetID());
      }

    vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
    if (!appLogic)
      {
      qCritical() << "qSlicerAstroModelingModuleWidget::onWorkFinished"
                     " : appLogic not found!";
      return;
      }

    appLogic->PropagatePlotChartSelection();

    // Force again the offset of the PV
    // It will be good to understand why the active node
    // in the selection node takes so much time to be updated.
    // P.S.: FitAllSlice is called everytime the active volume is changed.

    QTimer::singleShot(2, this, SLOT(centerPVOffset()));

    // Add fiducials
    if (!d->fiducialNodeMajor || !d->fiducialNodeMinor)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                    "fiducialNodes not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    // Create fiducials
    vtkSlicerAstroModelingLogic* astroModelingLogic =
      vtkSlicerAstroModelingLogic::SafeDownCast(this->logic());
    if (!astroModelingLogic)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished :"
                    " vtkSlicerAstroModelingLogic not found!";
      return;
      }
    vtkSlicerMarkupsLogic* MarkupsLogic =
      vtkSlicerMarkupsLogic::SafeDownCast(astroModelingLogic->GetMarkupsLogic());
    if (!MarkupsLogic)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished :"
                    " vtkSlicerMarkupsLogic not found!";
      return;
      }

    d->fiducialNodeMajor->GlobalWarningDisplayOff();
    d->fiducialNodeMinor->GlobalWarningDisplayOff();
    d->fiducialNodeMajor->RemoveAllControlPoints();
    d->fiducialNodeMinor->RemoveAllControlPoints();

    MarkupsLogic->SetActiveList(d->fiducialNodeMinor);
    for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues() * 2; radiiIndex++)
      {
      MarkupsLogic->AddControlPoint(0., 0., 0.);
      }

    MarkupsLogic->SetActiveList(d->fiducialNodeMajor);
    for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues() * 2; radiiIndex++)
      {
      MarkupsLogic->AddControlPoint(0., 0., 0.);
      }

    int MajorWasModifying = d->fiducialNodeMajor->StartModify();
    int MinorWasModifying = d->fiducialNodeMinor->StartModify();

    for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues(); radiiIndex++)
      {
      int positiveIndex = radiiIndex * 2;
      int negativeIndex = (radiiIndex * 2) + 1;

      std::string fiducialLabelPositive = "RMajor";
      std::string fiducialLabelNegative = "-RMajor";
      fiducialLabelPositive += IntToString(radiiIndex);
      fiducialLabelNegative += IntToString(radiiIndex);
      d->fiducialNodeMajor->SetNthControlPointLabel(positiveIndex, fiducialLabelPositive);
      d->fiducialNodeMajor->SetNthControlPointSelected(positiveIndex, false);
      d->fiducialNodeMajor->SetNthControlPointLocked(positiveIndex, true);
      d->fiducialNodeMajor->SetNthControlPointLabel(negativeIndex, fiducialLabelNegative);
      d->fiducialNodeMajor->SetNthControlPointSelected(negativeIndex, false);
      d->fiducialNodeMajor->SetNthControlPointLocked(negativeIndex, true);

      fiducialLabelPositive = "RMinor";
      fiducialLabelNegative = "-RMinor";
      fiducialLabelPositive += IntToString(radiiIndex);
      fiducialLabelNegative += IntToString(radiiIndex);
      d->fiducialNodeMinor->SetNthControlPointLabel(positiveIndex, fiducialLabelPositive);
      d->fiducialNodeMinor->SetNthControlPointSelected(positiveIndex, false);
      d->fiducialNodeMinor->SetNthControlPointLocked(positiveIndex, true);
      d->fiducialNodeMinor->SetNthControlPointLabel(negativeIndex, fiducialLabelNegative);
      d->fiducialNodeMinor->SetNthControlPointSelected(negativeIndex, false);
      d->fiducialNodeMinor->SetNthControlPointLocked(negativeIndex, true);
      }

    d->fiducialNodeMajor->EndModify(MajorWasModifying);
    d->fiducialNodeMinor->EndModify(MinorWasModifying);

    // Change scale value for the display of the fiducials
    vtkMRMLMarkupsDisplayNode *fiducialsMajorDisplayNode =
      d->fiducialNodeMajor->GetMarkupsDisplayNode();
    if (!fiducialsMajorDisplayNode)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                    "fiducial display node not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    fiducialsMajorDisplayNode->SetGlyphScale(1.5);
    fiducialsMajorDisplayNode->SetTextScale(0.);
    fiducialsMajorDisplayNode->RemoveAllViewNodeIDs();
    fiducialsMajorDisplayNode->AddViewNodeID(yellowSliceNode->GetID());

    vtkMRMLMarkupsDisplayNode *fiducialsMinorDisplayNode =
      d->fiducialNodeMinor->GetMarkupsDisplayNode();
    if (!fiducialsMinorDisplayNode)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                    "fiducial display node not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    fiducialsMinorDisplayNode->SetGlyphScale(1.5);
    fiducialsMinorDisplayNode->SetTextScale(0.);
    fiducialsMinorDisplayNode->SetColor(1., 1., 0.44);
    fiducialsMinorDisplayNode->RemoveAllViewNodeIDs();
    fiducialsMinorDisplayNode->AddViewNodeID(greenSliceNode->GetID());

    d->fiducialNodeMajor->GlobalWarningDisplayOn();
    d->fiducialNodeMinor->GlobalWarningDisplayOn();

    // Connect slice nodes to slot to update fiducials
    this->qvtkConnect(yellowSliceNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSliceNodeModified(vtkObject*)));
    this->qvtkConnect(greenSliceNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSliceNodeModified(vtkObject*)));
    this->onMRMLSliceNodeModified(yellowSliceNode);
    this->onMRMLSliceNodeModified(greenSliceNode);
    }
   else
    {
    scene->RemoveNode(outputVolume);

    inputVolume->SetDisplayVisibility(1);

    scene->RemoveNode(residualVolume);

    // Disconnect slice nodes to slot to update fiducials
    this->qvtkDisconnect(yellowSliceNode, vtkCommand::ModifiedEvent,
                         this, SLOT(onMRMLSliceNodeModified(vtkObject*)));
    this->qvtkDisconnect(greenSliceNode, vtkCommand::ModifiedEvent,
                         this, SLOT(onMRMLSliceNodeModified(vtkObject*)));

    d->fiducialNodeMajor->GlobalWarningDisplayOff();
    d->fiducialNodeMinor->GlobalWarningDisplayOff();
    d->fiducialNodeMajor->RemoveAllControlPoints();
    d->fiducialNodeMinor->RemoveAllControlPoints();
    d->fiducialNodeMajor->GlobalWarningDisplayOn();
    d->fiducialNodeMinor->GlobalWarningDisplayOn();

    int wasModifying = d->parametersNode->StartModify();
    d->parametersNode->SetXPosCenterIJK(0.);
    d->parametersNode->SetYPosCenterIJK(0.);
    d->parametersNode->SetPVPhi(0.);
    d->parametersNode->SetYellowRotOldValue(0.);
    d->parametersNode->SetYellowRotValue(0.);
    d->parametersNode->SetGreenRotOldValue(0.);
    d->parametersNode->SetGreenRotValue(0.);
    d->parametersNode->EndModify(wasModifying);
    }

  vtkMRMLAstroLabelMapVolumeNode *maskVolume =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMaskVolumeNodeID()));
  if(maskVolume)
    {
    scene->RemoveNode(maskVolume);
    }

  d->TableView->resizeColumnsToContents();
  d->parametersNode->SetStatus(0);

  d->astroTableNode = d->parametersNode->GetParamsTableNode();
  this->qvtkReconnect(d->astroTableNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLTableNodeModified()));
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onXCenterChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetXCenter(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onXCenterFitChanged(bool flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetXCenterFit(flag);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onYCenterChanged(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetYCenter(value);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onYCenterFitChanged(bool flag)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetYCenterFit(flag);
}

//--------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onYellowSliceRotated(double value)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetYellowRotOldValue(d->parametersNode->GetYellowRotValue());
  d->parametersNode->SetYellowRotValue(value);
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  // the fit stops at the next iteration of the minimization, the other steps
  // (source search, estimate, model) at their end
  vtkSlicerAstroModelingLogic* logic = d->logic();
  if (logic)
    {
    logic->RequestAbort();
    }
  d->CancelPushButton->setEnabled(false);
  d->CancelPushButton->setText(tr("Cancelling..."));
  d->parametersNode->SetStatus(-1);
  if (d->workerParametersNode)
    {
    d->workerParametersNode->SetStatus(-1);
    }
  d->worker->abort();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::updateWorkerProgress()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  // output of 3DBarolo since the last update
  if (d->logic())
    {
    d->appendOutputLog(QString::fromStdString(d->logic()->TakeLog()));
    }

  // progress of the worker thread, shown by the parameter node (GUI thread)
  if (!d->workerParametersNode || !d->parametersNode)
    {
    return;
    }
  // cancel requested: kept in the copy of the worker
  if (d->parametersNode->GetStatus() == -1)
    {
    d->workerParametersNode->SetStatus(-1);
    return;
    }
  const int status = d->workerParametersNode->GetStatus();
  if (status > 0 && status < 100 && d->parametersNode->GetStatus() != status)
    {
    d->parametersNode->SetStatus(status);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->progressBar->setValue(value);
  d->calculateProgressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->CreatePushButton->hide();
  d->FitPushButton->hide();
  d->progressBar->show();
  d->CancelPushButton->show();
  if (d->calculatingModel)
    {
    d->calculateProgressBar->setValue(0);
    d->calculateProgressBar->show();
    }
  // no other operation (or edit of the rings) until the end of this one;
  // updateWidgetFromMRML enables them again at the end
  d->EstimateInitialParametersPushButton->setEnabled(false);
  d->CalculatePushButton->setEnabled(false);
  d->VisualizePushButton->setEnabled(false);
  d->VisualizeMaskPushButton->setEnabled(false);
  d->TableView->setEnabled(false);
  d->ContourSliderWidget->setEnabled(false);
  d->AstroModelingPasteButton->setEnabled(false);
}

//---------------------------------------------------------------------------
vtkMRMLAstroModelingParametersNode* qSlicerAstroModelingModuleWidget::
mrmlAstroModelingParametersNode()const
{
  Q_D(const qSlicerAstroModelingModuleWidget);
    return d->parametersNode;
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onCleanInitialParameters()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();

  d->parametersNode->SetNumberOfRings(-1);
  d->parametersNode->SetRadSep(-1);
  d->parametersNode->SetXCenter(-1);
  d->parametersNode->SetYCenter(-1.);
  d->parametersNode->SetSystemicVelocity(-1);
  d->parametersNode->SetRotationVelocity(-1);
  d->parametersNode->SetVelocityDispersion(-1);
  d->parametersNode->SetRadialVelocity(0.);
  d->parametersNode->SetVerticalVelocity(0.);
  d->parametersNode->SetVerticalRotationalGradient(0.);
  d->parametersNode->SetVerticalRotationalGradientHeight(0.);
  d->parametersNode->SetInclination(-1);
  d->parametersNode->SetInclinationError(5.);
  d->parametersNode->SetPositionAngle(-1);
  d->parametersNode->SetPositionAngleError(15.);
  d->parametersNode->SetScaleHeight(-1);
  d->parametersNode->SetColumnDensity(1.);
  d->parametersNode->SetDistance(-1);
  d->parametersNode->SetPositionAngleFit(true);
  d->parametersNode->SetRotationVelocityFit(true);
  d->parametersNode->SetRadialVelocityFit(false);
  d->parametersNode->SetVelocityDispersionFit(true);
  d->parametersNode->SetInclinationFit(true);
  d->parametersNode->SetXCenterFit(false);
  d->parametersNode->SetYCenterFit(false);
  d->parametersNode->SetSystemicVelocityFit(false);
  d->parametersNode->SetScaleHeightFit(false);
  d->parametersNode->SetADRIFTCorrection(false);
  d->parametersNode->SetLayerType(0);
  d->parametersNode->SetFittingFunction(1);
  d->parametersNode->SetWeightingFunction(1);
  d->parametersNode->SetNumberOfClounds(-1);
  d->parametersNode->SetCloudsColumnDensity(10.);
  d->parametersNode->SetTollerance(0.001);

  d->parametersNode->EndModify(wasModifying);
}
