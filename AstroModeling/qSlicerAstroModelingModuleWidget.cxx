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
#include <QMessageBox>
#include <QMutexLocker>
#include <QPushButton>
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

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLDoubleArrayNode.h>
#include <vtkMRMLChartNode.h>
#include <vtkMRMLChartViewNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLPlotDataNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#include <sys/time.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroModeling
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

  vtkSlicerAstroModelingLogic* logic() const;
  qSlicerAstroVolumeModuleWidget* astroVolumeWidget;
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
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodeVRot;
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodeVRad;
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodeInc;
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodePhi;
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodeVSys;
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodeVDisp;
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodeDens;
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodeZ0;
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodeXPos;
  vtkSmartPointer<vtkMRMLPlotDataNode> plotDataNodeYPos;
  vtkSmartPointer<vtkMRMLMarkupsFiducialNode> fiducialNodeMajor;
  vtkSmartPointer<vtkMRMLMarkupsFiducialNode> fiducialNodeMinor;

  qSlicerAstroModelingModuleWorker *worker;
  QThread *thread;
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
  this->internalTableNode = vtkSmartPointer<vtkMRMLTableNode>::New();
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
  this->plotDataNodeVRot = 0;
  this->plotDataNodeVRad = 0;
  this->plotDataNodeInc = 0;
  this->plotDataNodePhi = 0;
  this->plotDataNodeVSys = 0;
  this->plotDataNodeVDisp = 0;
  this->plotDataNodeDens = 0;
  this->plotDataNodeZ0 = 0;
  this->plotDataNodeXPos = 0;
  this->plotDataNodeYPos = 0;
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

  QObject::connect(ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroModelingParametersNode(vtkMRMLNode*)));

  QObject::connect(TableNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onTableNodeChanged(vtkMRMLNode*)));

  QObject::connect(InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(OutputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(ResidualVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onResidualVolumeChanged(vtkMRMLNode*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   SegmentsTableView, SLOT(setMRMLScene(vtkMRMLScene*)));

  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);

  QObject::connect(MaskCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onMaskActiveToggled(bool)));

  QObject::connect(ManualModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(AutomaticModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(RingsSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onNumberOfRingsChanged(double)));

  QObject::connect(RingWidthSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onRadSepChanged(double)));

  QObject::connect(XcenterSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onXCenterChanged(double)));

  QObject::connect(YcenterSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onYCenterChanged(double)));

  QObject::connect(SysVelSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onSystemicVelocityChanged(double)));

  QObject::connect(RotVelSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onRotationVelocityChanged(double)));

  QObject::connect(RadVelSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onRadialVelocityChanged(double)));

  QObject::connect(VelDispSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onVelocityDispersionChanged(double)));

  QObject::connect(InclinationSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onInclinationChanged(double)));

  QObject::connect(InclinationErrorSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onInclinationErrorChanged(double)));

  QObject::connect(PASliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onPositionAngleChanged(double)));

  QObject::connect(PAErrorSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onPositionAngleErrorChanged(double)));

  QObject::connect(SHSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onScaleHeightChanged(double)));

  QObject::connect(CDSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onColumnDensityChanged(double)));

  QObject::connect(DistanceSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onDistanceChanged(double)));

  QObject::connect(PARadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onPositionAngleFitChanged(bool)));

  QObject::connect(VROTRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onRotationVelocityFitChanged(bool)));

  QObject::connect(VRadRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onRadialVelocityFitChanged(bool)));

  QObject::connect(DISPRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onVelocityDispersionFitChanged(bool)));

  QObject::connect(INCRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onInclinationFitChanged(bool)));

  QObject::connect(XCenterRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onXCenterFitChanged(bool)));

  QObject::connect(YCenterRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onYCenterFitChanged(bool)));

  QObject::connect(VSYSRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onSystemicVelocityFitChanged(bool)));

  QObject::connect(SCRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onScaleHeightFitChanged(bool)));

  QObject::connect(LayerTypeComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onLayerTypeChanged(int)));

  QObject::connect(FittingFunctionComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onFittingFunctionChanged(int)));

  QObject::connect(WeightingFunctionComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onWeightingFunctionChanged(int)));

  QObject::connect(NumCloudsSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onNumberOfCloundsChanged(double)));

  QObject::connect(CloudCDSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onCloudsColumnDensityChanged(double)));

  QObject::connect(ContourSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onContourLevelChanged(double)));

  QObject::connect(CleanInitialParametersPushButton, SIGNAL(clicked()),
                   q, SLOT(onCleanInitialParameters()));

  QObject::connect(EstimateInitialParametersPushButton, SIGNAL(clicked()),
                   q, SLOT(onEstimateInitialParameters()));

  QObject::connect(NormalizeCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onNormalizeToggled(bool)));

  QObject::connect(FitPushButton, SIGNAL(clicked()),
                   q, SLOT(onFit()));

  QObject::connect(CreatePushButton, SIGNAL(clicked()),
                   q, SLOT(onCreate()));

  QObject::connect(CancelPushButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));

  QObject::connect(VisualizePushButton, SIGNAL(clicked()),
                   q, SLOT(onVisualize()));

  QObject::connect(CalculatePushButton, SIGNAL(clicked()),
                   q, SLOT(onCalculateAndVisualize()));

  QObject::connect(YellowSliceSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onYellowSliceRotated(double)));

  QObject::connect(GreenSliceSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onGreenSliceRotated(double)));

  InputSegmentCollapsibleButton->setCollapsed(true);
  FittingParametersCollapsibleButton->setCollapsed(false);
  OutputCollapsibleButton->setCollapsed(true);
  OutputCollapsibleButton_2->setCollapsed(true);

  progressBar->hide();
  progressBar->setMinimum(0);
  progressBar->setMaximum(100);
  CancelPushButton->hide();

  this->thread = new QThread();
  this->worker = new qSlicerAstroModelingModuleWorker();

  this->worker->moveToThread(thread);

  this->worker->SetAstroModelingLogic(this->logic());
  this->worker->SetAstroModelingParametersNode(this->parametersNode);
  this->worker->SetTableNode(this->internalTableNode);

  QObject::connect(this->worker, SIGNAL(workRequested()), this->thread, SLOT(start()));

  QObject::connect(this->thread, SIGNAL(started()), this->worker, SLOT(doWork()));

  QObject::connect(this->worker, SIGNAL(finished()), q, SLOT(onWorkFinished()));

  QObject::connect(this->worker, SIGNAL(finished()), this->thread, SLOT(quit()), Qt::DirectConnection);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidgetPrivate::cleanPointers()
{
  this->parametersNode = 0;
  this->astroTableNode = 0;
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
  this->plotDataNodeVRot = 0;
  this->plotDataNodeVRad = 0;
  this->plotDataNodeInc = 0;
  this->plotDataNodePhi = 0;
  this->plotDataNodeVSys = 0;
  this->plotDataNodeVDisp = 0;
  this->plotDataNodeDens = 0;
  this->plotDataNodeZ0 = 0;
  this->plotDataNodeXPos = 0;
  this->plotDataNodeYPos = 0;
  this->fiducialNodeMajor = 0;
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
}

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidget::~qSlicerAstroModelingModuleWidget()
{
}

void qSlicerAstroModelingModuleWidget::enter()
{
  this->onEnter();
  this->Superclass::enter();
}

void qSlicerAstroModelingModuleWidget::exit()
{
  this->onExit();
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

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::setMRMLScene : selectionNode not found!";
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
  this->onInputVolumeChanged(this->mrmlScene()->GetNodeByID(d->selectionNode->GetActiveVolumeID()));
  this->onMRMLSelectionNodeReferenceAdded(d->selectionNode);
  this->onMRMLAstroModelingParametersNodeModified();

  vtkMRMLNode *activeVolume = this->mrmlScene()->GetNodeByID(d->selectionNode->GetActiveVolumeID());
  if (!activeVolume)
    {
    d->OutputVolumeNodeSelector->setEnabled(false);
    d->ParametersNodeComboBox->setEnabled(false);
    d->TableNodeComboBox->setEnabled(false);
    d->ResidualVolumeNodeSelector->setEnabled(false);
    }
  else
    {
    d->XcenterSliderWidget->setMaximum(StringToInt(activeVolume->GetAttribute("SlicerAstro.NAXIS1")));
    d->YcenterSliderWidget->setMaximum(StringToInt(activeVolume->GetAttribute("SlicerAstro.NAXIS2")));
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

  d->InputSegmentCollapsibleButton->setCollapsed(true);
  d->FittingParametersCollapsibleButton->setCollapsed(false);
  d->OutputCollapsibleButton->setCollapsed(true);
  d->OutputCollapsibleButton_2->setCollapsed(true);
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
  Q_D(qSlicerAstroModelingModuleWidget);

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

  vtkMRMLAstroModelingParametersNode *astroParametersNode = NULL;

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
      astroParametersNode->SetOutputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
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

  vtkSmartPointer<vtkMRMLNode> tableNode = NULL;

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
      if (d->selectionNode)
        {
        d->selectionNode->SetActiveTableID(d->astroTableNode->GetID());
        }
      return;
      }
    }

  vtkMRMLNode * foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLTableNode");
  tableNode.TakeReference(foo);
  std::string paramsTableNodeName = this->mrmlScene()->GenerateUniqueName("ModelingParamsTable");
  tableNode->SetName(paramsTableNodeName.c_str());
  this->mrmlScene()->AddNode(tableNode);

  d->astroTableNode = vtkMRMLTableNode::SafeDownCast(tableNode);
  int wasModifying = d->astroTableNode->StartModify();
  d->astroTableNode->RemoveAllColumns();
  d->astroTableNode->SetUseColumnNameAsColumnHeader(true);
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
  d->astroTableNode->SetColumnLongName("Radii", "Radius");

  vtkDoubleArray* VRot = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!VRot)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the VRot Column.";
    return;
    }
  VRot->SetName("VRot");
  d->astroTableNode->SetColumnUnitLabel("VRot", "km/s");
  d->astroTableNode->SetColumnLongName("VRot", "Rotational velocity");

  vtkDoubleArray* VRad = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!VRad)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the VRad Column.";
    return;
    }
  VRad->SetName("VRad");
  d->astroTableNode->SetColumnUnitLabel("VRad", "km/s");
  d->astroTableNode->SetColumnLongName("VRad", "Radial velocity");

  vtkDoubleArray* Inc = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Inc)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Inc Column.";
    return;
    }
  Inc->SetName("Inc");
  d->astroTableNode->SetColumnUnitLabel("Inc", "degree");
  d->astroTableNode->SetColumnLongName("Inc", "Inclination");

  vtkDoubleArray* Phi = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Phi)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Phi Column.";
    return;
    }
  Phi->SetName("Phi");
  d->astroTableNode->SetColumnUnitLabel("Phi", "degree");
  d->astroTableNode->SetColumnLongName("Phi", "Position angle");

  vtkDoubleArray* VSys = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!VSys)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the VSys Column.";
    return;
    }
  VSys->SetName("VSys");
  d->astroTableNode->SetColumnUnitLabel("VSys", "km/s (Velocity Definition: Optical)");
  d->astroTableNode->SetColumnLongName("VSys", "Systematic velocity");

  vtkDoubleArray* VDisp = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!VSys)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the VDisp Column.";
    return;
    }
  VDisp->SetName("VDisp");
  d->astroTableNode->SetColumnUnitLabel("VDisp", "km/s");
  d->astroTableNode->SetColumnLongName("VDisp", "Dispersion velocity");

  vtkDoubleArray* Dens = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Dens)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Dens Column.";
    return;
    }
  Dens->SetName("Dens");
  d->astroTableNode->SetColumnUnitLabel("Dens", "10^20 cm^-2");
  d->astroTableNode->SetColumnLongName("Dens", "Column density");

  vtkDoubleArray* Z0 = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Z0)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Z0 Column.";
    return;
    }
  Z0->SetName("Z0");
  d->astroTableNode->SetColumnUnitLabel("Z0", "Kpc");
  d->astroTableNode->SetColumnLongName("Z0", "Scale height");

  vtkDoubleArray* XPos = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!XPos)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the XPos Column.";
    return;
    }
  XPos->SetName("XPos");
  d->astroTableNode->SetColumnUnitLabel("XPos", "pixels");
  d->astroTableNode->SetColumnLongName("XPos", "X center");

  vtkDoubleArray* YPos = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!YPos)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the YPos Column.";
    return;
    }
  YPos->SetName("YPos");
  d->astroTableNode->SetColumnUnitLabel("YPos", "pixels");
  d->astroTableNode->SetColumnLongName("YPos", "Y center");
  d->astroTableNode->EndModify(wasModifying);

  d->parametersNode->SetParamsTableNode(d->astroTableNode);

  if (d->selectionNode)
    {
    d->selectionNode->SetActiveTableID(d->astroTableNode->GetID());
    }
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
  if (!d->plotDataNodeVRot)
    {
    vtkSmartPointer<vtkCollection> plotDataNodeVRotCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "VRot"));

    if (plotDataNodeVRotCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodeVRot.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodeVRot);
      d->plotDataNodeVRot->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodeVRot->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodeVRot->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnVRot));
      d->plotDataNodeVRot->SetName("VRot");
      }
    else
      {
      d->plotDataNodeVRot = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodeVRotCol->GetItemAsObject(plotDataNodeVRotCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodeVRot->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodeVRot);
    }

  if (!d->plotDataNodeVRad)
    {
    vtkSmartPointer<vtkCollection> plotDataNodeVRadCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "VRad"));

    if (plotDataNodeVRadCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodeVRad.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodeVRad);
      d->plotDataNodeVRad->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodeVRad->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodeVRad->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnVRad));
      d->plotDataNodeVRad->SetName("VRad");
      }
    else
      {
      d->plotDataNodeVRad = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodeVRadCol->GetItemAsObject(plotDataNodeVRadCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodeVRad->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodeVRad);
    }

  if (!d->plotDataNodeInc)
    {
    vtkSmartPointer<vtkCollection> plotDataNodeIncCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "Inc"));

    if (plotDataNodeIncCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodeInc.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodeInc);
      d->plotDataNodeInc->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodeInc->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodeInc->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnInc));
      d->plotDataNodeInc->SetName("Inc");
      }
    else
      {
      d->plotDataNodeInc = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodeIncCol->GetItemAsObject(plotDataNodeIncCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodeInc->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodeInc);
    }

  if (!d->plotDataNodePhi)
    {
    vtkSmartPointer<vtkCollection> plotDataNodePhiCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "Phi"));

    if (plotDataNodePhiCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodePhi.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodePhi);
      d->plotDataNodePhi->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodePhi->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodePhi->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnPhi));
      d->plotDataNodePhi->SetName("Phi");
      }
    else
      {
      d->plotDataNodePhi = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodePhiCol->GetItemAsObject(plotDataNodePhiCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodePhi->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodePhi);
    }

  if (!d->plotDataNodeVSys)
    {
    vtkSmartPointer<vtkCollection> plotDataNodeVSysCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "VSys"));

    if (plotDataNodeVSysCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodeVSys.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodeVSys);
      d->plotDataNodeVSys->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodeVSys->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodeVSys->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnVSys));
      d->plotDataNodeVSys->SetName("VSys");
      }
    else
      {
      d->plotDataNodeVSys = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodeVSysCol->GetItemAsObject(plotDataNodeVSysCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodeVSys->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodeVSys);
    }

  if (!d->plotDataNodeVDisp)
    {
    vtkSmartPointer<vtkCollection> plotDataNodeVDispCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "VDisp"));

    if (plotDataNodeVDispCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodeVDisp.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodeVDisp);
      d->plotDataNodeVDisp->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodeVDisp->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodeVDisp->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnVDisp));
      d->plotDataNodeVDisp->SetName("VDisp");
      }
    else
      {
      d->plotDataNodeVDisp = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodeVDispCol->GetItemAsObject(plotDataNodeVDispCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodeVDisp->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodeVDisp);
    }

  if (!d->plotDataNodeDens)
    {
    vtkSmartPointer<vtkCollection> plotDataNodeDensCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "Dens"));

    if (plotDataNodeDensCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodeDens.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodeDens);
      d->plotDataNodeDens->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodeDens->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodeDens->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnDens));
      d->plotDataNodeDens->SetName("Dens");
      }
    else
      {
      d->plotDataNodeDens = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodeDensCol->GetItemAsObject(plotDataNodeDensCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodeDens->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodeDens);
    }

  if (!d->plotDataNodeZ0)
    {
    vtkSmartPointer<vtkCollection> plotDataNodeZ0Col =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "Z0"));

    if (plotDataNodeZ0Col->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodeZ0.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodeZ0);
      d->plotDataNodeZ0->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodeZ0->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodeZ0->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnZ0));
      d->plotDataNodeZ0->SetName("Z0");
      }
    else
      {
      d->plotDataNodeZ0 = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodeZ0Col->GetItemAsObject(plotDataNodeZ0Col->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodeZ0->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodeZ0);
    }

  if (!d->plotDataNodeXPos)
    {
    vtkSmartPointer<vtkCollection> plotDataNodeXPosCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "XPos"));

    if (plotDataNodeXPosCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodeXPos.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodeXPos);
      d->plotDataNodeXPos->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodeXPos->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodeXPos->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnXPos));
      d->plotDataNodeXPos->SetName("XPos");
      }
    else
      {
      d->plotDataNodeXPos = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodeXPosCol->GetItemAsObject(plotDataNodeXPosCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodeXPos->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodeXPos);
    }

  if (!d->plotDataNodeYPos)
    {
    vtkSmartPointer<vtkCollection> plotDataNodeYPosCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotDataNode", "YPos"));

    if (plotDataNodeYPosCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotDataNodeYPos.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
      this->mrmlScene()->AddNode(d->plotDataNodeYPos);
      d->plotDataNodeYPos->SetAndObserveTableNodeID(tableNode->GetID());
      d->plotDataNodeYPos->SetXColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
      d->plotDataNodeYPos->SetYColumnName(tableNode->GetColumnName
        (vtkMRMLAstroModelingParametersNode::ParamsColumnYPos));
      d->plotDataNodeYPos->SetName("YPos");
      }
    else
      {
      d->plotDataNodeYPos = vtkMRMLPlotDataNode::SafeDownCast
        (plotDataNodeYPosCol->GetItemAsObject(plotDataNodeYPosCol->GetNumberOfItems() - 1));
      }
    }
  else if (!this->mrmlScene()->GetNodeByID(d->plotDataNodeYPos->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotDataNodeYPos);
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
      this->mrmlScene()->AddNode(d->plotChartNodeVRot);
      d->plotChartNodeVRot->SetName("VRotChart");
      d->plotChartNodeVRot->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodeVRot->SetAttribute("YAxisLabelName", "Rotational Velocity (km/s)");
      d->plotChartNodeVRot->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeVRot->SetAttribute("Type", "Line");
      d->plotChartNodeVRot->SetAttribute("Markers", "Circle");
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
      this->mrmlScene()->AddNode(d->plotChartNodeVRad);
      d->plotChartNodeVRad->SetName("VRadChart");
      d->plotChartNodeVRad->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodeVRad->SetAttribute("YAxisLabelName", "Radial Velocity (km/s)");
      d->plotChartNodeVRad->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeVRad->SetAttribute("Type", "Line");
      d->plotChartNodeVRad->SetAttribute("Markers", "Circle");
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
      this->mrmlScene()->AddNode(d->plotChartNodeInc);
      d->plotChartNodeInc->SetName("IncChart");
      d->plotChartNodeInc->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodeInc->SetAttribute("YAxisLabelName", "Inclination (degree)");
      d->plotChartNodeInc->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeInc->SetAttribute("Type", "Line");
      d->plotChartNodeInc->SetAttribute("Markers", "Circle");
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
      this->mrmlScene()->AddNode(d->plotChartNodePhi);
      d->plotChartNodePhi->SetName("PhiChart");
      d->plotChartNodePhi->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodePhi->SetAttribute("YAxisLabelName", "Orientation Angle (degree)");
      d->plotChartNodePhi->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodePhi->SetAttribute("Type", "Line");
      d->plotChartNodePhi->SetAttribute("Markers", "Circle");
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
      this->mrmlScene()->AddNode(d->plotChartNodeVSys);
      d->plotChartNodeVSys->SetName("VSysChart");
      d->plotChartNodeVSys->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodeVSys->SetAttribute("YAxisLabelName", "Systemic Velocity (km/s)");
      d->plotChartNodeVSys->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeVSys->SetAttribute("Type", "Line");
      d->plotChartNodeVSys->SetAttribute("Markers", "Circle");
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
      this->mrmlScene()->AddNode(d->plotChartNodeVDisp);
      d->plotChartNodeVDisp->SetName("VDispChart");
      d->plotChartNodeVDisp->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodeVDisp->SetAttribute("YAxisLabelName", "Dispersion Velocity (km/s)");
      d->plotChartNodeVDisp->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeVDisp->SetAttribute("Type", "Line");
      d->plotChartNodeVDisp->SetAttribute("Markers", "Circle");
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
      this->mrmlScene()->AddNode(d->plotChartNodeDens);
      d->plotChartNodeDens->SetName("DensChart");
      d->plotChartNodeDens->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodeDens->SetAttribute("YAxisLabelName", "Column Density (10^20 cm^-2)");
      d->plotChartNodeDens->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeDens->SetAttribute("Type", "Line");
      d->plotChartNodeDens->SetAttribute("Markers", "Circle");
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
      this->mrmlScene()->AddNode(d->plotChartNodeZ0);
      d->plotChartNodeZ0->SetName("Z0Chart");
      d->plotChartNodeZ0->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodeZ0->SetAttribute("YAxisLabelName", "Scale Heigth (Kpc)");
      d->plotChartNodeZ0->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeZ0->SetAttribute("Type", "Line");
      d->plotChartNodeZ0->SetAttribute("Markers", "Circle");
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
      this->mrmlScene()->AddNode(d->plotChartNodeXPos);
      d->plotChartNodeXPos->SetName("XPosChart");
      d->plotChartNodeXPos->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodeXPos->SetAttribute("YAxisLabelName", "X Center (pixels)");
      d->plotChartNodeXPos->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeXPos->SetAttribute("Type", "Line");
      d->plotChartNodeXPos->SetAttribute("Markers", "Circle");
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
      this->mrmlScene()->AddNode(d->plotChartNodeYPos);
      d->plotChartNodeYPos->SetName("YPosChart");
      d->plotChartNodeYPos->SetAttribute("XAxisLabelName", "Radii (arcsec)");
      d->plotChartNodeYPos->SetAttribute("YAxisLabelName", "Y Center (pixels)");
      d->plotChartNodeYPos->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeYPos->SetAttribute("Type", "Line");
      d->plotChartNodeYPos->SetAttribute("Markers", "Circle");
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

  // Add PlotDataNodes to PlotChartNodes
  d->plotChartNodeVRot->RemoveAllPlotDataNodeIDs();
  d->plotChartNodeVRad->RemoveAllPlotDataNodeIDs();
  d->plotChartNodeInc->RemoveAllPlotDataNodeIDs();
  d->plotChartNodePhi->RemoveAllPlotDataNodeIDs();
  d->plotChartNodeVSys->RemoveAllPlotDataNodeIDs();
  d->plotChartNodeVDisp->RemoveAllPlotDataNodeIDs();
  d->plotChartNodeDens->RemoveAllPlotDataNodeIDs();
  d->plotChartNodeZ0->RemoveAllPlotDataNodeIDs();
  d->plotChartNodeXPos->RemoveAllPlotDataNodeIDs();
  d->plotChartNodeYPos->RemoveAllPlotDataNodeIDs();

  d->plotChartNodeVRot->AddAndObservePlotDataNodeID(d->plotDataNodeVRot->GetID());
  d->plotChartNodeVRad->AddAndObservePlotDataNodeID(d->plotDataNodeVRad->GetID());
  d->plotChartNodeInc->AddAndObservePlotDataNodeID(d->plotDataNodeInc->GetID());
  d->plotChartNodePhi->AddAndObservePlotDataNodeID(d->plotDataNodePhi->GetID());
  d->plotChartNodeVSys->AddAndObservePlotDataNodeID(d->plotDataNodeVSys->GetID());
  d->plotChartNodeVDisp->AddAndObservePlotDataNodeID(d->plotDataNodeVDisp->GetID());
  d->plotChartNodeDens->AddAndObservePlotDataNodeID(d->plotDataNodeDens->GetID());
  d->plotChartNodeZ0->AddAndObservePlotDataNodeID(d->plotDataNodeZ0->GetID());
  d->plotChartNodeXPos->AddAndObservePlotDataNodeID(d->plotDataNodeXPos->GetID());
  d->plotChartNodeYPos->AddAndObservePlotDataNodeID(d->plotDataNodeYPos->GetID());

  //Select NULL Chart
  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID(NULL);
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
    vtkSmartPointer<vtkMRMLNode> segmentationNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLSegmentationNode");
    segmentationNode.TakeReference(foo);
    this->mrmlScene()->AddNode(segmentationNode);
    d->segmentEditorNode->SetAndObserveSegmentationNode
      (vtkMRMLSegmentationNode::SafeDownCast(segmentationNode));
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroModelingModuleWidget::convertSelectedSegmentToLabelMap()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->segmentEditorNode)
    {
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
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode)
    {
    QString message = QString("No segmentation node selected! Please create a segmentation or untoggle the input"
                              " mask option to perform automatic masking with 3DBarolo.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to select a mask"), message);
    return false;
    }

  // Export selected segments into a multi-label labelmap volume
  std::vector<std::string> segmentIDs;
  currentSegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIDs);

  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> labelMapNode =
    vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode>::New();

  QStringList selectedSegmentIDs = d->SegmentsTableView->selectedSegmentIDs();

  if (selectedSegmentIDs.size() < 1)
    {
    QString message = QString("No mask selected from teh segmentation node! Please provide a mask or untoggle the input"
                              " mask option to perform automatic masking with 3DBarolo.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to select a mask"), message);
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

  int Extents[6] = { 0, 0, 0, 0, 0, 0 };
  labelMapNode->GetImageData()->GetExtent(Extents);

  if (!vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode
       (currentSegmentationNode, segmentIDs, labelMapNode))
    {
    QString message = QString("Failed to export segments from segmentation '%1'' to representation node '%2!.").
                              arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    this->mrmlScene()->RemoveNode(labelMapNode);
    return false;
    }

  labelMapNode->GetAstroLabelMapVolumeDisplayNode()->
    SetAndObserveColorNodeID("vtkMRMLColorTableNodeFileGenericColors.txt");

  double storedOrigin[3] = { 0., 0., 0. };
  labelMapNode->GetOrigin(storedOrigin);

  // restore original Extents
  vtkNew<vtkImageReslice> reslice;
  reslice->SetOutputExtent(Extents);
  reslice->SetOutputOrigin(0., 0., 0.);
  reslice->SetOutputScalarType(VTK_SHORT);
  reslice->SetInputData(labelMapNode->GetImageData());

  reslice->Update();
  labelMapNode->GetImageData()->DeepCopy(reslice->GetOutput());

  // restore original Origins
  int *dims = labelMapNode->GetImageData()->GetDimensions();
  double dimsH[4];
  dimsH[0] = dims[0] - 1;
  dimsH[1] = dims[1] - 1;
  dimsH[2] = dims[2] - 1;
  dimsH[3] = 0.;

  vtkNew<vtkMatrix4x4> ijkToRAS;
  labelMapNode->GetIJKToRASMatrix(ijkToRAS.GetPointer());
  double rasCorner[4];
  ijkToRAS->MultiplyPoint(dimsH, rasCorner);

  double Origin[3] = { 0., 0., 0. };
  Origin[0] = -0.5 * rasCorner[0];
  Origin[1] = -0.5 * rasCorner[1];
  Origin[2] = -0.5 * rasCorner[2];

  labelMapNode->SetOrigin(Origin);

  // translate data to original location (linear translation supported only)
  storedOrigin[0] -= Origin[0];
  storedOrigin[1] -= Origin[1];
  storedOrigin[2] -= Origin[2];

  vtkNew<vtkImageData> tempVolumeData;
  tempVolumeData->Initialize();
  tempVolumeData->DeepCopy(labelMapNode->GetImageData());
  tempVolumeData->Modified();
  tempVolumeData->GetPointData()->GetScalars()->Modified();

  dims = labelMapNode->GetImageData()->GetDimensions();
  const int numElements = dims[0] * dims[1] * dims[2];
  const int numSlice = dims[0] * dims[1];
  int shiftX = (int) fabs(storedOrigin[0]);
  int shiftY = (int) fabs(storedOrigin[2]) * dims[0];
  int shiftZ = (int) fabs(storedOrigin[1]) * numSlice;
  short* tempVoxelPtr = static_cast<short*>(tempVolumeData->GetScalarPointer());
  short* voxelPtr = static_cast<short*>(labelMapNode->GetImageData()->GetScalarPointer());

  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    *(voxelPtr + elemCnt) = 0;
    }

  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int X = elemCnt + shiftX;
    int ref = (int) floor(elemCnt / dims[0]);
    ref *= dims[0];
    if(X < ref || X >= ref + dims[0])
      {
      continue;
      }

    int Y = elemCnt + shiftY;
    ref = (int) floor(elemCnt / numSlice);
    ref *= numSlice;
    if(Y < ref || Y >= ref + numSlice)
      {
      continue;
      }

    int Z = elemCnt + shiftZ;
    if(Z < 0 || Z >= numElements)
      {
      continue;
      }

    int shift = elemCnt + shiftX + shiftY + shiftZ;

    *(voxelPtr + shift) = *(tempVoxelPtr + elemCnt);
    }

  labelMapNode->UpdateRangeAttributes();

  d->parametersNode->SetMaskVolumeNodeID(labelMapNode->GetID());

  return true;
}

void qSlicerAstroModelingModuleWidget::onEnter()
{
  if (!this->mrmlScene())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid scene";
    return;
    }

  this->qvtkReconnect(this->mrmlScene(), vtkMRMLScene::StartImportEvent,
                      this, SLOT(onMRMLSceneStartImportEvent()));
  this->qvtkReconnect(this->mrmlScene(), vtkMRMLScene::EndImportEvent,
                      this, SLOT(onMRMLSceneEndImportEvent()));
  this->qvtkReconnect(this->mrmlScene(), vtkMRMLScene::EndBatchProcessEvent,
                      this, SLOT(onMRMLSceneEndBatchProcessEvent()));
  this->qvtkReconnect(this->mrmlScene(), vtkMRMLScene::EndCloseEvent,
                      this, SLOT(onMRMLSceneEndCloseEvent()));
  this->qvtkReconnect(this->mrmlScene(), vtkMRMLScene::EndRestoreEvent,
                      this, SLOT(onMRMLSceneEndRestoreEvent()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onExit()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onCalculateAndVisualize()
{
  Q_D(qSlicerAstroModelingModuleWidget);

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

  char *activeVolumeNodeID = d->selectionNode->GetActiveVolumeID();
  char *secondaryVolumeNodeID = d->selectionNode->GetSecondaryVolumeID();

  if (!d->logic())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onCalculateAndVisualize :"
                  " logic not found!";
    return;
    }

  if (!d->logic()->UpdateModelFromTable(d->parametersNode))
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

  if (!sender || !this->mrmlScene() ||
      this->mrmlScene()->IsClosing() || this->mrmlScene()->IsBatchProcessing())
    {
    return;
    }

  vtkMRMLSelectionNode *selectionNode =
      vtkMRMLSelectionNode::SafeDownCast(sender);

  if (!d->parametersNode || !selectionNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
  d->parametersNode->SetOutputVolumeNodeID(selectionNode->GetSecondaryVolumeID());
  vtkMRMLTableNode* tableNode = vtkMRMLTableNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(selectionNode->GetActiveTableID()));
  d->astroTableNode = tableNode;
  this->qvtkReconnect(d->astroTableNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLTableNodeModified()));
  d->parametersNode->SetParamsTableNode(d->astroTableNode);
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

  int dims[3];
  inputVolume->GetImageData()->GetDimensions(dims);
  int Zcenter = dims[2] * 0.5;

  const double arcsec2deg = 1. / 3600.;
  const double deg2arcsec = 3600.;
  const double deg2rad = PI / 180.;
  const double kms2ms = 1000.;
  double  pidiv4 = PI / 4.;
  double  pidiv2 = PI / 2.;
  double SliceCenterWorld[3] = {0.,0.,0.}, ModelCenterWorld[3] = {0.,0.,0.},
         SliceCenterIJK[3] = {0.,0.,0.},   ModelCenterIJK[3] = {0.,0.,0.},
         SliceCenterRAS[3] = {0., 0., 0.}, //newSliceCenterRAS[3] = {0., 0., 0.},
         worldPositive[3] = {0.,0.,0.}, worldNegative[3] = {0.,0.,0.},
         IJK[3] = {0.,0.,0.}, RAS[3] = {0.,0.,0.};

  vtkNew<vtkGeneralTransform> IJKtoRASTransform;
  IJKtoRASTransform->Identity();
  IJKtoRASTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  inputVolume->GetIJKToRASMatrix(IJKtoRASMatrix.GetPointer());
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix.GetPointer());

  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  inputVolume->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  vtkNew<vtkGeneralTransform> EllipseTransform;
  EllipseTransform->Identity();
  EllipseTransform->PostMultiply();

  if (!d->parametersNode->GetParamsTableNode())
    {
    return;
    }

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

  if (!VRot || !VRad || !Inc || !VSys || !Radii)
    {
    return;
    }

  double *VRotPointer = static_cast<double*> (VRot->GetPointer(0));
  double *VRadPointer = static_cast<double*> (VRad->GetPointer(0));
  double *IncPointer = static_cast<double*> (Inc->GetPointer(0));
  double *VSysPointer = static_cast<double*> (VSys->GetPointer(0));
  double *RadiiPointer = static_cast<double*> (Radii->GetPointer(0));

  // Semi-major axes angle (ijk coordinates)
  double PVPhi = d->parametersNode->GetPVPhi();

  // Slice angle (ijk coordinates)
  double sliceAngle = 0.;
  if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeYellow"))
    {
    if (!d->fiducialNodeMajor ||
        d->fiducialNodeMajor->GetNumberOfFiducials() != Radii->GetNumberOfValues() * 2)
      {
      return;
      }

    sliceAngle = d->parametersNode->GetYellowRotValue();
    }
  else if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeGreen"))
    {
    if (!d->fiducialNodeMinor ||
        d->fiducialNodeMinor->GetNumberOfFiducials() != Radii->GetNumberOfValues() * 2)
      {
      return;
      }

    sliceAngle = d->parametersNode->GetGreenRotValue();
    }

  if (int(fabs(sliceAngle) < 1.E-6) ||
      int(fabs(fabs(sliceAngle) - 180.)) < 1.E-6)
    {
    sliceAngle += 0.01;
    }
  else if (int(fabs(fabs(sliceAngle) - 90.)) < 1.E-6 ||
           int(fabs(fabs(sliceAngle) - 270.)) < 1.E-6)
    {
    sliceAngle -= 0.01;
    }

  if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeYellow"))
    {
    sliceAngle += PVPhi;
    }
  else if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeGreen"))
    {
    sliceAngle += PVPhi +90.;
    }

  if (int(fabs(sliceAngle) < 1.E-6) ||
      int(fabs(fabs(sliceAngle) - 180.)) < 1.E-6)
    {
    sliceAngle += 0.01;
    }
  else if (int(fabs(fabs(sliceAngle) - 90.)) < 1.E-6 ||
           int(fabs(fabs(sliceAngle) - 270.)) < 1.E-6)
    {
    sliceAngle -= 0.01;
    }

  double factor = 0.;
  if ((sliceAngle - 90) > 1.E-6 && (sliceAngle - 270.) < 1.E-6)
    {
    factor = -180.;
    }
  else if ((sliceAngle - 270.) > 1.E-6)
    {
    factor = -360.;
    }
  else if ((sliceAngle + 90.) < 1.E-6 && (sliceAngle + 270.) > 1.E-6)
    {
    factor = +180.;
    }
  else if ((sliceAngle + 270.) < 1.E-6)
    {
    factor = +360.;
    }
  sliceAngle += factor;

  // Convert angles to rad
  PVPhi *= deg2rad;
  sliceAngle *= deg2rad;

  // Calculate WCS (sky) and fits header CDELT conversion factors
  ModelCenterIJK[0] = 0;
  ModelCenterIJK[1] = 0;
  ModelCenterIJK[2] = 0;
  inputVolumeDisplayNode->GetReferenceSpace(ModelCenterIJK, ModelCenterWorld);
  SliceCenterIJK[0] = 1;
  SliceCenterIJK[1] = 1;
  SliceCenterIJK[2] = 0;
  inputVolumeDisplayNode->GetReferenceSpace(SliceCenterIJK, SliceCenterWorld);
  double stepX = SliceCenterWorld[1] - ModelCenterWorld[1];
  double stepY = SliceCenterWorld[0] - ModelCenterWorld[0];
  double CDELTA1 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.CDELT1"));
  double CDELTA2 = StringToDouble(inputVolume->GetAttribute("SlicerAstro.CDELT2"));
  double factorX = fabs(stepX / CDELTA1);
  double factorY = fabs(stepY / CDELTA2);

  // Calculate WCS (sky) slice angle
  for (int ii = 0; ii < 3; ii++)
    {
    SliceCenterRAS[ii] = sliceToRAS->GetElement(ii, 3);
    }

  RAStoIJKTransform->TransformPoint(SliceCenterRAS, SliceCenterIJK);
  SliceCenterIJK[2] = Zcenter;
  inputVolumeDisplayNode->GetReferenceSpace(SliceCenterIJK, SliceCenterWorld);
  SliceCenterIJK[0] += (10 * cos(sliceAngle));
  SliceCenterIJK[1] += (10 * sin(sliceAngle));
  inputVolumeDisplayNode->GetReferenceSpace(SliceCenterIJK, ModelCenterWorld);

  double distX = (ModelCenterWorld[0] - SliceCenterWorld[0]);
  double distY = (ModelCenterWorld[1] - SliceCenterWorld[1]);
  double sliceAngleWorld = atan(distY / distX);

  double sliceAngleCos = cos(-sliceAngleWorld);
  double sliceAngleSin = sin(-sliceAngleWorld);

  // Calculate WCS (sky) semi-major axis angle
  /*ModelCenterIJK[0] = d->parametersNode->GetXPosCenterIJK();
  ModelCenterIJK[1] = d->parametersNode->GetYPosCenterIJK();
  ModelCenterIJK[2] = Zcenter;
  inputVolumeDisplayNode->GetReferenceSpace(ModelCenterIJK, ModelCenterWorld);
  ModelCenterIJK[0] += (10 * cos(PVPhi));
  ModelCenterIJK[1] += (10 * sin(PVPhi));
  inputVolumeDisplayNode->GetReferenceSpace(ModelCenterIJK, SliceCenterWorld);

  distX = (SliceCenterWorld[0] - ModelCenterWorld[0]);
  distY = (SliceCenterWorld[1] - ModelCenterWorld[1]);
  double PVPhiWorld = atan(distY / distX);

  double PVPhiWorldCos = cos(-PVPhiWorld);
  double PVPhiWorldSin = sin(-PVPhiWorld);*/

  // Calculate the semi-major axis (a) and semi-minor axis (b) lengths in ijk coordinates
  ModelCenterIJK[0] = d->parametersNode->GetXPosCenterIJK();
  ModelCenterIJK[1] = d->parametersNode->GetYPosCenterIJK();
  ModelCenterIJK[2] = Zcenter;
  /*inputVolumeDisplayNode->GetReferenceSpace(ModelCenterIJK, ModelCenterWorld);
  ModelCenterWorld[0] += *(RadiiPointer + Radii->GetNumberOfValues() - 1) * arcsec2deg * PVPhiWorldCos;
  ModelCenterWorld[1] += *(RadiiPointer + Radii->GetNumberOfValues() - 1) * arcsec2deg * PVPhiWorldSin;
  inputVolumeDisplayNode->GetIJKSpace(ModelCenterWorld, ModelCenterIJK);
  double a = sqrt(((ModelCenterIJK[0] - d->parametersNode->GetXPosCenterIJK()) *
                  (ModelCenterIJK[0] - d->parametersNode->GetXPosCenterIJK())) +
                  ((ModelCenterIJK[1] - d->parametersNode->GetYPosCenterIJK()) *
                  (ModelCenterIJK[1] - d->parametersNode->GetYPosCenterIJK())));
  double b = a * cos(*(IncPointer + Radii->GetNumberOfValues() - 1) * deg2rad);

  // Calculate m and b of the slice in ijk coordinates
  ModelCenterIJK[0] = d->parametersNode->GetXPosCenterIJK();
  ModelCenterIJK[1] = d->parametersNode->GetYPosCenterIJK();
  ModelCenterIJK[2] = Zcenter;*/
  RAStoIJKTransform->TransformPoint(SliceCenterRAS, SliceCenterIJK);

  /*EllipseTransform->Translate(-ModelCenterIJK[0],
                              -ModelCenterIJK[1],
                              -ModelCenterIJK[2]);
  EllipseTransform->RotateZ((PVPhi / deg2rad));
  EllipseTransform->TransformPoint(SliceCenterIJK, SliceCenterIJK);

  double shiftedPointX = SliceCenterIJK[0] + 100 * sin(sliceAngle - PVPhi + 90);
  double shiftedPointY = SliceCenterIJK[1] + 100 * cos(sliceAngle - PVPhi + 90);

  double m = (SliceCenterIJK[1] - shiftedPointY) / (SliceCenterIJK[0] - shiftedPointX);
  double c = shiftedPointY - (m * shiftedPointX);

  // Calculate the line (slope of the slice) and the 2D ellipse intersections
  double a2 = a * a;
  double b2 = b * b;
  double c2 = c * c;
  double m2 = m * m;

  double det = a2 * m2 + b2 - c2;
  if (det < 0.)
    {
    d->fiducialNodeMajor->GlobalWarningDisplayOff();
    int MajorWasModifying = d->fiducialNodeMajor->StartModify();
    for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues(); radiiIndex++)
      {
      int positiveIndex = radiiIndex * 2;
      int negativeIndex = (radiiIndex * 2) + 1;
      d->fiducialNodeMajor->SetNthFiducialPosition(positiveIndex, 0., 0., 0.);
      d->fiducialNodeMajor->SetNthFiducialPosition(negativeIndex, 0., 0., 0.);
      d->fiducialNodeMajor->SetNthFiducialVisibility(positiveIndex, false);
      d->fiducialNodeMajor->SetNthFiducialVisibility(negativeIndex, false);
      }
    d->fiducialNodeMajor->EndModify(MajorWasModifying);
    d->fiducialNodeMajor->GlobalWarningDisplayOn();
    return;
    }
  det = sqrt(det);

  double denom = a2 * m2 + b2;
  double x1 = ((-a2 * m * c) + (a * b * det)) / denom;
  double x2 = ((-a2 * m * c) - (a * b * det)) / denom;
  double y1 = ((-b2 * c) + (a * b * m * det)) / denom;
  double y2 = ((-b2 * c) - (a * b * m * det)) / denom;

  // Calculate the center of the slice respect to the
  // two intersections of slice on the the 2D ellipse
  SliceCenterIJK[0] = (x1 + x2) * 0.5;
  SliceCenterIJK[1] = -(y1 + y2) * 0.5;

  EllipseTransform->Identity();
  EllipseTransform->RotateZ(-(PVPhi / deg2rad));
  EllipseTransform->Translate(ModelCenterIJK[0],
                              ModelCenterIJK[1],
                              ModelCenterIJK[2]);
  EllipseTransform->TransformPoint(SliceCenterIJK, SliceCenterIJK);*/
  inputVolumeDisplayNode->GetReferenceSpace(SliceCenterIJK, SliceCenterWorld);

  // Update Slice RAS
  /*IJKtoRASTransform->TransformPoint(SliceCenterIJK, newSliceCenterRAS);
  for (int ii = 0; ii < 3; ii++)
    {
    sliceToRAS->SetElement(ii, 3, newSliceCenterRAS[ii]);
    }
  sliceNode->UpdateMatrices();*/

  // Calculate the center of the model in the WCS (sky) coordinates.
  inputVolumeDisplayNode->GetReferenceSpace(ModelCenterIJK, ModelCenterWorld);

  // Calculate the offsets (between slice/model centers)
  double worldOffsetX = (SliceCenterWorld[0] - ModelCenterWorld[0]) * factorX * deg2arcsec;
  double worldOffsetY = (SliceCenterWorld[1] - ModelCenterWorld[1]) * factorY * deg2arcsec;

  // Update Fiducials
  double alpha = PVPhi;
  double sina  = sin(alpha);
  double cosa  = cos(alpha);

  int MajorWasModifying = 0, MinorWasModifying= 0;
  if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeYellow"))
    {
    d->fiducialNodeMajor->GlobalWarningDisplayOff();
    MajorWasModifying = d->fiducialNodeMajor->StartModify();
    }
  else if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeGreen"))
    {
    d->fiducialNodeMinor->GlobalWarningDisplayOff();
    MinorWasModifying = d->fiducialNodeMinor->StartModify();
    }
  for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues(); radiiIndex++)
    {
    double m, b, p, q, r, x[2], y[2];
    int positiveIndex = radiiIndex * 2;
    int negativeIndex = (radiiIndex * 2) + 1;
    double sinInc = sin(*(IncPointer + radiiIndex) * deg2rad);
    double cosInc = cos(*(IncPointer + radiiIndex) * deg2rad);
    double cosIncCosInc = 1. / (cosInc * cosInc);

    double A = cosa * cosa + sina * sina * cosIncCosInc;
    double B = 2. * cosa * sina - 2. * sina * cosa * cosIncCosInc;
    double C = sina * sina + cosa * cosa * cosIncCosInc;


    bool danger = ((sliceAngle >= 1. * pidiv4 && sliceAngle <= 3. * pidiv4) ||
                   (sliceAngle >= 5. * pidiv4 && sliceAngle <= 7. * pidiv4));
    if (!danger)
      {
      // Intersect with a line Y = mX + b
      m = tan(sliceAngle);
      b = worldOffsetY - worldOffsetX * m;  // offsets are from g. to sl. center
      p = A + B * m + C * m * m;
      q = B * b + 2. * m * b * C;
      r = C * b * b - *(RadiiPointer + radiiIndex) * *(RadiiPointer + radiiIndex);
      }
    else
      {
      // Intersect with a line X = mY + b
      m = tan(pidiv2 - sliceAngle);
      b = worldOffsetX - worldOffsetY * m;
      p = A * m * m + B * m + C;
      q = B * b + 2. * m * b * A;
      r = A * b * b - *(RadiiPointer + radiiIndex) * *(RadiiPointer + radiiIndex);
      }

    double det = q * q - 4. * p * r;
    if (det < 0.)
      {
      if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeYellow"))
        {
        d->fiducialNodeMajor->SetNthFiducialPosition(positiveIndex, 0., 0., 0.);
        d->fiducialNodeMajor->SetNthFiducialPosition(negativeIndex, 0., 0., 0.);
        d->fiducialNodeMajor->SetNthFiducialVisibility(positiveIndex, false);
        d->fiducialNodeMajor->SetNthFiducialVisibility(negativeIndex, false);
        }
      else if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeGreen"))
        {
        d->fiducialNodeMinor->SetNthFiducialPosition(positiveIndex, 0., 0., 0.);
        d->fiducialNodeMinor->SetNthFiducialPosition(negativeIndex, 0., 0., 0.);
        d->fiducialNodeMinor->SetNthFiducialVisibility(positiveIndex, false);
        d->fiducialNodeMinor->SetNthFiducialVisibility(negativeIndex, false);
        }
      continue;
      }

    double sqrdet = sqrt(det);
    if (!danger)
      {
      x[0] = (-1. * q + sqrdet) / (2. * p);
      x[1] = (-1. * q - sqrdet) / (2. * p);
      y[0] = m * (x[0]) + b;
      y[1] = m * (x[1]) + b;
      }
    else
      {
      y[0] = (-1. * q + sqrdet) / (2. * p);
      y[1] = (-1. * q - sqrdet) / (2. * p);
      x[0] = m * (y[0]) + b;
      x[1] = m * (y[1]) + b;
      }

    // Project velocity
    double e1 = arctan(y[0], x[0]);
    double beta1 = putinrangerad(e1 - alpha);
    double theta1 = arctan(fabs(tan(beta1)), fabs(cosInc));
    double VelocitySin1 = (*(VRadPointer + radiiIndex) * sinInc * sin(theta1) +
                           *(VRotPointer + radiiIndex) * sinInc * cos(theta1)) * sign(cos(beta1));
    double VelocityPositive = *(VSysPointer + radiiIndex) + VelocitySin1;

    // Project radius
    double xt1 = x[0] - worldOffsetX;
    double yt1 = y[0] - worldOffsetY;
    double ProjectedRadius1 = (xt1 * cos(sliceAngle) / factorX + yt1 * sin(sliceAngle) / factorY)
                               * fabs(cos(theta1) / cos(beta1)) * arcsec2deg;

    // Update fiducials
    double ShiftX1 = ProjectedRadius1 * sliceAngleCos;
    double ShiftY1 = ProjectedRadius1 * sliceAngleSin;
    worldPositive[0] = SliceCenterWorld[0] + ShiftX1;
    worldPositive[1] = SliceCenterWorld[1] + ShiftY1;
    worldPositive[2] = VelocityPositive * kms2ms;
    inputVolumeDisplayNode->GetIJKSpace(worldPositive, IJK);
    IJKtoRASTransform->TransformPoint(IJK, RAS);
    if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeYellow"))
      {
      d->fiducialNodeMajor->SetNthFiducialPosition(positiveIndex, RAS[0], RAS[1], RAS[2]);
      d->fiducialNodeMajor->SetNthFiducialVisibility(positiveIndex, true);
      }
    else if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeGreen"))
      {
      d->fiducialNodeMinor->SetNthFiducialPosition(positiveIndex, RAS[0], RAS[1], RAS[2]);
      d->fiducialNodeMinor->SetNthFiducialVisibility(positiveIndex, true);
      }
    // Project velocity
    double e2 = arctan(y[1], x[1]);
    double beta2 = putinrangerad(e2 - alpha);
    double theta2 = arctan(fabs(tan(beta2)), fabs(cosInc));
    double VelocitySin2 = (*(VRadPointer + radiiIndex) * sinInc * sin(theta2) +
                           *(VRotPointer + radiiIndex) * sinInc * cos(theta2)) * sign(cos(beta2));
    double VelocityNegative = *(VSysPointer + radiiIndex) + VelocitySin2;

    // Project radius
    double xt2 = x[1] - worldOffsetX;
    double yt2 = y[1] - worldOffsetY;
    double ProjectedRadius2 = (xt2 * cos(sliceAngle) / factorX + yt2 * sin(sliceAngle) / factorY)
                               * fabs(cos(theta2) / cos(beta2)) * arcsec2deg;

    // Update fiducials
    double ShiftX2 = ProjectedRadius2 * sliceAngleCos;
    double ShiftY2 = ProjectedRadius2 * sliceAngleSin;
    worldNegative[0] = SliceCenterWorld[0] + ShiftX2;
    worldNegative[1] = SliceCenterWorld[1] + ShiftY2;
    worldNegative[2] = VelocityNegative * kms2ms;
    inputVolumeDisplayNode->GetIJKSpace(worldNegative, IJK);
    IJKtoRASTransform->TransformPoint(IJK, RAS);
    if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeYellow"))
      {
      d->fiducialNodeMajor->SetNthFiducialPosition(negativeIndex, RAS[0], RAS[1], RAS[2]);
      d->fiducialNodeMajor->SetNthFiducialVisibility(negativeIndex, true);
      }
    else if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeGreen"))
      {
      d->fiducialNodeMinor->SetNthFiducialPosition(negativeIndex, RAS[0], RAS[1], RAS[2]);
      d->fiducialNodeMinor->SetNthFiducialVisibility(negativeIndex, true);
      }
    }

  if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeYellow"))
    {
    d->fiducialNodeMajor->EndModify(MajorWasModifying);
    d->fiducialNodeMajor->GlobalWarningDisplayOn();
    }
  else if (!strcmp(sliceNode->GetID(), "vtkMRMLSliceNodeGreen"))
    {
    d->fiducialNodeMinor->EndModify(MinorWasModifying);
    d->fiducialNodeMinor->GlobalWarningDisplayOn();
    }

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
  Q_D(qSlicerAstroModelingModuleWidget);

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

  if (!d->parametersNode)
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

void qSlicerAstroModelingModuleWidget::onNormalizeToggled(bool toggled)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetNormalize(toggled);
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
    d->selectionNode->SetReferenceActiveVolumeID(mrmlNode->GetID());
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    d->parametersNode->SetInputVolumeNodeID(mrmlNode->GetID());
    d->XcenterSliderWidget->setMaximum(StringToInt(mrmlNode->GetAttribute("SlicerAstro.NAXIS1")));
    d->YcenterSliderWidget->setMaximum(StringToInt(mrmlNode->GetAttribute("SlicerAstro.NAXIS2")));
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(NULL);
    d->selectionNode->SetActiveVolumeID(NULL);
    }
  appLogic->PropagateVolumeSelection();
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
    d->parametersNode->SetNumberOfRings(0);
    d->parametersNode->SetRadSep(0.);
    d->parametersNode->SetXCenter(0.);
    d->parametersNode->SetYCenter(0.);
    d->parametersNode->SetSystemicVelocity(0.);
    d->parametersNode->SetRotationVelocity(0.);
    d->parametersNode->SetVelocityDispersion(0.);
    d->parametersNode->SetInclination(0.);
    d->parametersNode->SetInclinationError(5.);
    d->parametersNode->SetPositionAngle(0.);
    d->parametersNode->SetPositionAngleError(15.);
    d->parametersNode->SetScaleHeight(0.);
    d->parametersNode->SetColumnDensity(1.);
    d->parametersNode->SetDistance(0.);
    d->parametersNode->SetPositionAngleFit(true);
    d->parametersNode->SetRotationVelocityFit(true);
    d->parametersNode->SetRadialVelocityFit(false);
    d->parametersNode->SetVelocityDispersionFit(true);
    d->parametersNode->SetInclinationFit(true);
    d->parametersNode->SetXCenterFit(false);
    d->parametersNode->SetYCenterFit(false);
    d->parametersNode->SetSystemicVelocityFit(false);
    d->parametersNode->SetScaleHeightFit(false);
    d->parametersNode->SetLayerType(0);
    d->parametersNode->SetFittingFunction(1);
    d->parametersNode->SetWeightingFunction(1);
    d->parametersNode->SetNumberOfClounds(0);
    d->parametersNode->SetCloudsColumnDensity(10.);
    }

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
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
    d->selectionNode->SetReferenceSecondaryVolumeID(mrmlNode->GetID());
    d->selectionNode->SetSecondaryVolumeID(mrmlNode->GetID());
    d->parametersNode->SetOutputVolumeNodeID(mrmlNode->GetID());
    }
  else
    {
    d->selectionNode->SetReferenceSecondaryVolumeID(NULL);
    d->selectionNode->SetSecondaryVolumeID(NULL);
    }
  appLogic->PropagateVolumeSelection();
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

  for (int fiducialIndex = 0; fiducialIndex < d->fiducialNodeMajor->GetNumberOfFiducials(); fiducialIndex++)
    {
    d->fiducialNodeMajor->SetNthFiducialSelected(fiducialIndex, false);
    }

  for (int mrmlPlotDataIndex = 0; mrmlPlotDataIndex < mrmlPlotDataIDs->GetNumberOfValues(); mrmlPlotDataIndex++)
    {
    vtkMRMLPlotDataNode* plotDataNode = vtkMRMLPlotDataNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(mrmlPlotDataIDs->GetValue(mrmlPlotDataIndex)));
    if (!plotDataNode)
      {
      continue;
      }
    if (!strcmp(plotDataNode->GetName(), "VRot") ||
        !strcmp(plotDataNode->GetName(), "VRad") ||
        !strcmp(plotDataNode->GetName(), "Inc") ||
        !strcmp(plotDataNode->GetName(), "Phi"))
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
        d->fiducialNodeMajor->SetNthFiducialSelected(positiveIndex, true);
        d->fiducialNodeMajor->SetNthFiducialSelected(negativeIndex, true);
        }
      }
    }

  for (int fiducialIndex = 0; fiducialIndex < d->fiducialNodeMinor->GetNumberOfFiducials(); fiducialIndex++)
    {
    d->fiducialNodeMinor->SetNthFiducialSelected(fiducialIndex, false);
    }

  for (int mrmlPlotDataIndex = 0; mrmlPlotDataIndex < mrmlPlotDataIDs->GetNumberOfValues(); mrmlPlotDataIndex++)
    {
    vtkMRMLPlotDataNode* plotDataNode = vtkMRMLPlotDataNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(mrmlPlotDataIDs->GetValue(mrmlPlotDataIndex)));
    if (!plotDataNode)
      {
      continue;
      }
    if (!strcmp(plotDataNode->GetName(), "VRot") ||
        !strcmp(plotDataNode->GetName(), "VRad") ||
        !strcmp(plotDataNode->GetName(), "Inc") ||
        !strcmp(plotDataNode->GetName(), "Phi"))
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
        d->fiducialNodeMinor->SetNthFiducialSelected(positiveIndex, true);
        d->fiducialNodeMinor->SetNthFiducialSelected(negativeIndex, true);
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

  if (!d->parametersNode || !mrmlNode)
    {
    return;
    }

  d->parametersNode->SetResidualVolumeNodeID(mrmlNode->GetID());
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

  char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  if (inputVolumeNode)
    {
    d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);
    }

  char *outputVolumeNodeID = d->parametersNode->GetOutputVolumeNodeID();
  vtkMRMLAstroVolumeNode *outputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(outputVolumeNodeID));
  if (outputVolumeNode)
    {
    d->OutputVolumeNodeSelector->setCurrentNode(outputVolumeNode);
    }

  char *residualVolumeNodeID = d->parametersNode->GetResidualVolumeNodeID();
  vtkMRMLAstroVolumeNode *residualVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(residualVolumeNodeID));
  if (residualVolumeNode)
    {
    d->ResidualVolumeNodeSelector->setCurrentNode(residualVolumeNode);
    }

  d->MaskCheckBox->setChecked(d->parametersNode->GetMaskActive());
  d->SegmentsTableView->setEnabled(d->parametersNode->GetMaskActive());

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
  d->VelDispSliderWidget->setValue(d->parametersNode->GetVelocityDispersion());
  d->InclinationSliderWidget->setValue(d->parametersNode->GetInclination());
  d->InclinationErrorSpinBox->setValue(d->parametersNode->GetInclinationError());
  d->PASliderWidget->setValue(d->parametersNode->GetPositionAngle());
  d->PAErrorSpinBox->setValue(d->parametersNode->GetPositionAngleError());
  d->SHSliderWidget->setValue(d->parametersNode->GetScaleHeight());
  d->CDSliderWidget->setValue(d->parametersNode->GetColumnDensity());
  d->DistanceSliderWidget->setValue(d->parametersNode->GetDistance());
  d->PARadioButton->setChecked(d->parametersNode->GetPositionAngleFit());
  d->DISPRadioButton->setChecked(d->parametersNode->GetRotationVelocityFit());
  d->VROTRadioButton->setChecked(d->parametersNode->GetVelocityDispersionFit());
  d->VRadRadioButton->setChecked(d->parametersNode->GetRadialVelocityFit());
  d->INCRadioButton->setChecked(d->parametersNode->GetInclinationFit());
  d->XCenterRadioButton->setChecked(d->parametersNode->GetXCenterFit());
  d->YCenterRadioButton->setChecked(d->parametersNode->GetYCenterFit());
  d->VSYSRadioButton->setChecked(d->parametersNode->GetSystemicVelocityFit());
  d->SCRadioButton->setChecked(d->parametersNode->GetScaleHeightFit());
  d->LayerTypeComboBox->setCurrentIndex(d->parametersNode->GetLayerType());
  d->FittingFunctionComboBox->setCurrentIndex(d->parametersNode->GetFittingFunction());
  d->WeightingFunctionComboBox->setCurrentIndex(d->parametersNode->GetWeightingFunction());
  d->NumCloudsSliderWidget->setValue(d->parametersNode->GetNumberOfClounds());
  d->CloudCDSliderWidget->setValue(d->parametersNode->GetCloudsColumnDensity());

  d->ContourSliderWidget->setValue(d->parametersNode->GetContourLevel());

  d->NormalizeCheckBox->setChecked(d->parametersNode->GetNormalize());

  d->TableView->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourSliderWidget->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourLabel->setEnabled(d->parametersNode->GetFitSuccess());
  d->VisualizePushButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->CalculatePushButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->CopyButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->PasteButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->PlotButton->setEnabled(d->parametersNode->GetFitSuccess());

  d->OutputCollapsibleButton_2->setEnabled(d->parametersNode->GetFitSuccess());
  d->YellowSliceLabel->setEnabled(d->parametersNode->GetFitSuccess());
  d->YellowSliceSliderWidget->setEnabled(d->parametersNode->GetFitSuccess());
  d->GreenSliceLabel->setEnabled(d->parametersNode->GetFitSuccess());
  d->GreenSliceSliderWidget->setEnabled(d->parametersNode->GetFitSuccess());

  // Set params table to table view
  d->TableNodeComboBox->setCurrentNode(d->parametersNode->GetParamsTableNode());
  d->TableView->setMRMLTableNode(d->parametersNode->GetParamsTableNode());
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onMRMLGreenSliceRotated()
{
  Q_D(const qSlicerAstroModelingModuleWidget);

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
    qCritical() << "qSlicerAstroSmoothingModuleWidget::setMRMLScene : appLogic not found!";
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

  // Select VRot in the plotting window
  d->selectionNode->SetActivePlotChartID(d->plotChartNodeVRot->GetID());
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
    qCritical() << "qSlicerAstroSmoothingModuleWidget::setMRMLScene : appLogic not found!";
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
void qSlicerAstroModelingModuleWidget::onMRMLSceneStartImportEvent()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  if (d->parametersNode)
    {
    this->mrmlScene()->RemoveNode(d->parametersNode);
    }

  if (d->astroTableNode)
    {
    this->mrmlScene()->RemoveNode(d->astroTableNode);
    }

  if (d->plotChartNodeVRot)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodeVRot);
    }

  if (d->plotChartNodeVRad)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodeVRad);
    }

  if (d->plotChartNodeInc)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodeInc);
    }

  if (d->plotChartNodePhi)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodePhi);
    }

  if (d->plotChartNodeVSys)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodeVSys);
    }

  if (d->plotChartNodeVDisp)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodeVDisp);
    }

  if (d->plotChartNodeDens)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodeDens);
    }

  if (d->plotChartNodeZ0)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodeZ0);
    }

  if (d->plotChartNodeXPos)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodeXPos);
    }

  if (d->plotChartNodeYPos)
    {
    this->mrmlScene()->RemoveNode(d->plotChartNodeYPos);
    }

  if (d->plotDataNodeVRot)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodeVRot);
    }

  if (d->plotDataNodeVRad)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodeVRad);
    }

  if (d->plotDataNodeInc)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodeInc);
    }

  if (d->plotDataNodePhi)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodePhi);
    }

  if (d->plotDataNodeVSys)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodeVSys);
    }

  if (d->plotDataNodeVDisp)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodeVDisp);
    }

  if (d->plotDataNodeDens)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodeDens);
    }

  if (d->plotDataNodeZ0)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodeZ0);
    }

  if (d->plotDataNodeXPos)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodeXPos);
    }

  if (d->plotDataNodeYPos)
    {
    this->mrmlScene()->RemoveNode(d->plotDataNodeYPos);
    }

  if (d->fiducialNodeMajor)
    {
    this->mrmlScene()->RemoveNode(d->fiducialNodeMajor);
    }

  if (d->fiducialNodeMinor)
    {
    this->mrmlScene()->RemoveNode(d->fiducialNodeMinor);
    }

  if (d->segmentEditorNode)
    {
    if (d->segmentEditorNode->GetSegmentationNode())
      {
      this->mrmlScene()->RemoveNode(d->segmentEditorNode->GetSegmentationNode());
      d->segmentEditorNode->SetAndObserveSegmentationNode(NULL);
      }
    }
  if (d->SegmentsTableView)
    {
    if (d->SegmentsTableView->segmentationNode())
      {
      this->mrmlScene()->RemoveNode(d->SegmentsTableView->segmentationNode());
      d->SegmentsTableView->setSegmentationNode(NULL);
      }
    }

  d->cleanPointers();
}

//---------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onEstimateInitialParameters()
{
  Q_D(const qSlicerAstroModelingModuleWidget);

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
void qSlicerAstroModelingModuleWidget::onApply()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  vtkSlicerAstroModelingLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : astroModelingLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!this->mrmlScene())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : scene not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onApply() : parametersNode not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->parametersNode->GetParamsTableNode())
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onApply() : TableNode not found!";
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
  if(!inputVolume)
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
    QMessageBox::warning(NULL, tr("Failed to run 3DBarolo"), message);
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!strcmp(inputVolume->GetAttribute("SlicerAstro.BMAJ"), "UNDEFINED") ||
      !strcmp(inputVolume->GetAttribute("SlicerAstro.BMIN"), "UNDEFINED") ||
      !strcmp(inputVolume->GetAttribute("SlicerAstro.BPA"), "UNDEFINED") )
    {
    QString message = QString("Beam information (BMAJ, BMIN and/or BPA) not available."
                              " It is not possible to procede with the model fitting.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to run 3DBarolo"), message);
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
  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  if (!outputVolume)
    {
    outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast(scene->
            GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
    }

  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_model";

  int serial = d->parametersNode->GetOutputSerial();
  outSS<<"_"<< IntToString(serial);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : appLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  // Check Output volume
  if (!strcmp(inputVolume->GetID(), outputVolume->GetID()) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) !=
      StringToInt(outputVolume->GetAttribute("SlicerAstro.NAXIS1"))) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) !=
      StringToInt(outputVolume->GetAttribute("SlicerAstro.NAXIS2"))) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3")) !=
      StringToInt(outputVolume->GetAttribute("SlicerAstro.NAXIS3"))))
    {

    outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
       (logic->GetAstroVolumeLogic()->CloneVolume(scene, inputVolume, outSS.str().c_str()));

    outputVolume->SetName(outSS.str().c_str());
    d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());

    int ndnodes = outputVolume->GetNumberOfDisplayNodes();
    for (int i=0; i<ndnodes; i++)
      {
      vtkMRMLVolumeRenderingDisplayNode *dnode =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
          outputVolume->GetNthDisplayNode(i));
      if (dnode)
        {
        outputVolume->RemoveNthDisplayNodeID(i);
        }
      } 
    }
  else
    {
    outputVolume->SetName(outSS.str().c_str());
    d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());
    }

  vtkNew<vtkMatrix4x4> transformationMatrix;
  inputVolume->GetRASToIJKMatrix(transformationMatrix.GetPointer());
  outputVolume->SetRASToIJKMatrix(transformationMatrix.GetPointer());
  outputVolume->SetAndObserveTransformNodeID(inputVolume->GetTransformNodeID());

  // Create Residual Volume
  vtkMRMLAstroVolumeNode *residualVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetResidualVolumeNodeID()));

  if (!residualVolume)
    {
    residualVolume = vtkMRMLAstroVolumeNode::SafeDownCast(scene->
            GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
    }

  std::ostringstream residualSS;
  residualSS << inputVolume->GetName() << "_maskedByModel_"<<
             IntToString(serial);
  serial++;
  d->parametersNode->SetOutputSerial(serial);

  // Check residual volume
  if (!strcmp(inputVolume->GetID(), residualVolume->GetID()) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) !=
      StringToInt(residualVolume->GetAttribute("SlicerAstro.NAXIS1"))) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) !=
      StringToInt(residualVolume->GetAttribute("SlicerAstro.NAXIS2"))) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3")) !=
      StringToInt(residualVolume->GetAttribute("SlicerAstro.NAXIS3"))))
    {

    residualVolume = vtkMRMLAstroVolumeNode::SafeDownCast
       (logic->GetAstroVolumeLogic()->CloneVolume(scene, inputVolume, residualSS.str().c_str()));

    residualVolume->SetName(residualSS.str().c_str());
    d->parametersNode->SetResidualVolumeNodeID(residualVolume->GetID());

    int ndnodes = residualVolume->GetNumberOfDisplayNodes();
    for (int i=0; i<ndnodes; i++)
      {
      vtkMRMLVolumeRenderingDisplayNode *dnode =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
          residualVolume->GetNthDisplayNode(i));
      if (dnode)
        {
        residualVolume->RemoveNthDisplayNodeID(i);
        }
      }
    }
  else
    {
    residualVolume->SetName(residualSS.str().c_str());
    d->parametersNode->SetResidualVolumeNodeID(residualVolume->GetID());
    }

  inputVolume->GetRASToIJKMatrix(transformationMatrix.GetPointer());
  residualVolume->SetRASToIJKMatrix(transformationMatrix.GetPointer());
  residualVolume->SetAndObserveTransformNodeID(inputVolume->GetTransformNodeID());

  // Check if there are segment and feed the mask to 3DBarolo
  if (d->parametersNode->GetMaskActive())
    {
    if (!this->convertSelectedSegmentToLabelMap())
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : convertSelectedSegmentToLabelMap failed!";
      d->parametersNode->SetStatus(0);
      return;
      }
    }
  else if (!d->parametersNode->GetMaskActive() && d->parametersNode->GetNumberOfRings() == 0)
    {
    QString message = QString("No mask has been provided. 3DBarolo will search and fit the"
                              " largest source in the datacube.");
    qWarning() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("3DBarolo"), message);
    }

  d->worker->SetTableNode(d->internalTableNode);
  d->worker->SetAstroModelingParametersNode(d->parametersNode);
  d->worker->SetAstroModelingLogic(logic);
  d->worker->requestWork();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->CancelPushButton->hide();
  d->progressBar->hide();
  d->FitPushButton->show();
  d->CreatePushButton->show();
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

  vtkMRMLSegmentationNode* segmentationNodeTable = vtkMRMLSegmentationNode::SafeDownCast(
    d->SegmentsTableView->segmentationNode());
  if (!segmentationNodeTable)
    {
    d->SegmentsTableView->setSegmentationNode(segmentationNode);
    return;
    }

  if (segmentationNode != segmentationNodeTable)
    {
    d->SegmentsTableView->setSegmentationNode(segmentationNode);
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
void qSlicerAstroModelingModuleWidget::onTableNodeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  vtkMRMLTableNode *tableNode = vtkMRMLTableNode::SafeDownCast(mrmlNode);
  if (d->astroTableNode == tableNode)
    {
    return;
    }

  d->astroTableNode = tableNode;

  if (!d->selectionNode)
    {
    return;
    }

  if(d->astroTableNode)
    {
    d->selectionNode->SetActiveTableID(d->astroTableNode->GetID());
    }
  else
    {
    d->selectionNode->SetActiveTableID(NULL);
    }
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
void qSlicerAstroModelingModuleWidget::onVisualize()
{
  Q_D(qSlicerAstroModelingModuleWidget);

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

  char *activeVolumeNodeID = d->selectionNode->GetActiveVolumeID();
  char *secondaryVolumeNodeID = d->selectionNode->GetSecondaryVolumeID();

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
  d->PlotAction->setToolTip(tr("Generate an Interactive Plot based on user-selection"
                               " of the columns of the table."));
  this->addAction(d->PlotAction);

  // Connect copy, paste and plot actions
  d->CopyButton->setDefaultAction(d->CopyAction);
  this->connect(d->CopyAction, SIGNAL(triggered()), d->TableView, SLOT(copySelection()));
  d->PasteButton->setDefaultAction(d->PasteAction);
  this->connect(d->PasteAction, SIGNAL(triggered()), d->TableView, SLOT(pasteSelection()));
  d->PlotButton->setDefaultAction(d->PlotAction);
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
    outputVolume->UpdateNoiseAttributes();
    outputVolume->UpdateRangeAttributes();
    outputVolume->SetAttribute("SlicerAstro.DATAMODEL", "MODEL");

    residualVolume->UpdateNoiseAttributes();
    residualVolume->UpdateRangeAttributes();
    outputVolume->SetAttribute("SlicerAstro.DATAMODEL", "DATA");

    d->parametersNode->GetParamsTableNode()->Copy(d->internalTableNode);

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

    int wasModifying = d->parametersNode->StartModify();
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

    d->InputSegmentCollapsibleButton->setCollapsed(true);
    d->FittingParametersCollapsibleButton->setCollapsed(true);
    d->OutputCollapsibleButton->setCollapsed(false);
    d->OutputCollapsibleButton_2->setCollapsed(false);

    // Select VRot in the plotting window
    if (d->selectionNode)
      {
      d->selectionNode->SetActivePlotChartID(d->plotChartNodeVRot->GetID());
      }

    // Force again the offset of the PV
    // It will be good to understand why the active node
    // in the selection node takes so much time to be updated.
    // P.S.: FitAllSlice is called everytime the active volume is changed.

    QTimer::singleShot(2, this, SLOT(centerPVOffset()));


    // Connect PlotWidget with ModelingWidget
    // Setting the Layout for the Output
    qSlicerApplication* app = qSlicerApplication::application();

    if(!app)
      {
      qCritical() << "qSlicerAstroMomentMapsModuleWidget::onWorkFinished : "
                     "qSlicerApplication not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    qSlicerLayoutManager* layoutManager = app->layoutManager();

    if(!app)
      {
      qCritical() << "qSlicerAstroMomentMapsModuleWidget::onWorkFinished : "
                     "layoutManager not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    qMRMLPlotWidget* plotWidget = layoutManager->plotWidget(0);

    if(!plotWidget)
      {
      qCritical() << "qSlicerAstroMomentMapsModuleWidget::onWorkFinished : "
                     "plotWidget not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    qMRMLPlotView* plotView = plotWidget->plotView();
    if(!plotWidget)
      {
      qCritical() << "qSlicerAstroMomentMapsModuleWidget::onWorkFinished : "
                     "plotView not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    QObject::connect(plotView, SIGNAL(dataSelected(vtkStringArray*, vtkCollection*)),
                     this, SLOT(onPlotSelectionChanged(vtkStringArray*, vtkCollection*)));

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
    d->fiducialNodeMajor->RemoveAllMarkups();
    d->fiducialNodeMinor->RemoveAllMarkups();

    MarkupsLogic->SetActiveListID(d->fiducialNodeMinor);
    for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues() * 2; radiiIndex++)
      {
      MarkupsLogic->AddFiducial(0., 0., 0.);
      }

    MarkupsLogic->SetActiveListID(d->fiducialNodeMajor);
    for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues() * 2; radiiIndex++)
      {
      MarkupsLogic->AddFiducial(0., 0., 0.);
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
      d->fiducialNodeMajor->SetNthFiducialLabel(positiveIndex, fiducialLabelPositive);
      d->fiducialNodeMajor->SetNthFiducialSelected(positiveIndex, false);
      d->fiducialNodeMajor->SetNthMarkupLocked(positiveIndex, true);
      d->fiducialNodeMajor->SetNthFiducialLabel(negativeIndex, fiducialLabelNegative);
      d->fiducialNodeMajor->SetNthFiducialSelected(negativeIndex, false);
      d->fiducialNodeMajor->SetNthMarkupLocked(negativeIndex, true);

      fiducialLabelPositive = "RMinor";
      fiducialLabelNegative = "-RMinor";
      fiducialLabelPositive += IntToString(radiiIndex);
      fiducialLabelNegative += IntToString(radiiIndex);
      d->fiducialNodeMinor->SetNthFiducialLabel(positiveIndex, fiducialLabelPositive);
      d->fiducialNodeMinor->SetNthFiducialSelected(positiveIndex, false);
      d->fiducialNodeMinor->SetNthMarkupLocked(positiveIndex, true);
      d->fiducialNodeMinor->SetNthFiducialLabel(negativeIndex, fiducialLabelNegative);
      d->fiducialNodeMinor->SetNthFiducialSelected(negativeIndex, false);
      d->fiducialNodeMinor->SetNthMarkupLocked(negativeIndex, true);
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
    d->fiducialNodeMajor->RemoveAllMarkups();
    d->fiducialNodeMinor->RemoveAllMarkups();
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

    if (!d->parametersNode->GetMaskActive())
      {
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    vtkMRMLAstroLabelMapVolumeNode *maskVolume =
      vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMaskVolumeNodeID()));

    if(!maskVolume)
      {
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    scene->RemoveNode(maskVolume);
    }

  d->parametersNode->SetStatus(0);
  d->TableView->resizeColumnsToContents();
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
  d->parametersNode->SetStatus(-1);
  d->worker->abort();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->progressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->CreatePushButton->hide();
  d->FitPushButton->hide();
  d->progressBar->show();
  d->CancelPushButton->show();
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

  d->parametersNode->SetNumberOfRings(0);
  d->parametersNode->SetRadSep(0.);
  d->parametersNode->SetXCenter(0.);
  d->parametersNode->SetYCenter(0.);
  d->parametersNode->SetSystemicVelocity(0.);
  d->parametersNode->SetRotationVelocity(0.);
  d->parametersNode->SetVelocityDispersion(0.);
  d->parametersNode->SetInclination(0.);
  d->parametersNode->SetInclinationError(5.);
  d->parametersNode->SetPositionAngle(0.);
  d->parametersNode->SetPositionAngleError(15.);
  d->parametersNode->SetScaleHeight(0.);
  d->parametersNode->SetColumnDensity(1.);
  d->parametersNode->SetDistance(0.);
  d->parametersNode->SetPositionAngleFit(true);
  d->parametersNode->SetRotationVelocityFit(true);
  d->parametersNode->SetRadialVelocityFit(false);
  d->parametersNode->SetVelocityDispersionFit(true);
  d->parametersNode->SetInclinationFit(true);
  d->parametersNode->SetXCenterFit(false);
  d->parametersNode->SetYCenterFit(false);
  d->parametersNode->SetSystemicVelocityFit(false);
  d->parametersNode->SetScaleHeightFit(false);
  d->parametersNode->SetLayerType(0);
  d->parametersNode->SetFittingFunction(1);
  d->parametersNode->SetWeightingFunction(1);
  d->parametersNode->SetNumberOfClounds(0);
  d->parametersNode->SetCloudsColumnDensity(10.);

  d->parametersNode->EndModify(wasModifying);
}
