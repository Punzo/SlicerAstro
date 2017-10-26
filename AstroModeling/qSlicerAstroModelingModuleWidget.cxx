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
  vtkSmartPointer<vtkMRMLMarkupsFiducialNode> fiducialNodeMajor;
  vtkSmartPointer<vtkMRMLMarkupsFiducialNode> fiducialNodeMinor;

  qSlicerAstroModelingModuleWorker *worker;
  QThread *thread;
  QAction *CopyAction;
  QAction *PasteAction;
  QAction *PlotAction;

  double XPosMean;
  double YPosMean;
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


  InputSegmentCollapsibleButton->setCollapsed(true);
  FittingParametersCollapsibleButton->setCollapsed(false);
  OutputCollapsibleButton->setCollapsed(true);

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

double sign(double x)
{
   if (x < 0.0)
      return( -1.0 );
   return( 1.0 );
}

} // end namespace

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  this->Superclass::setMRMLScene(scene);
  if (scene == NULL)
    {
    return;
    }

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

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));

  this->initializeParameterNode(scene);

  // observe close event so can re-add a parameters node if necessary
  this->qvtkReconnect(this->mrmlScene(), vtkMRMLScene::EndCloseEvent,
                      this, SLOT(onEndCloseEvent()));

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceAddedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceAdded(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceRemovedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceRemoved(vtkObject*)));
  this->onMRMLSelectionNodeModified(d->selectionNode);
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

  d->parametersNode->SetMaskActive(false);

  d->InputSegmentCollapsibleButton->setCollapsed(true);
  d->FittingParametersCollapsibleButton->setCollapsed(false);
  d->OutputCollapsibleButton->setCollapsed(true);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (d->fiducialNodeMajor)
    {
    d->fiducialNodeMajor->RemoveAllMarkups();
    d->fiducialNodeMajor = 0;
    }

  if (d->fiducialNodeMinor)
    {
    d->fiducialNodeMinor->RemoveAllMarkups();
    d->fiducialNodeMinor = 0;
    }

  this->initializeParameterNode(this->mrmlScene());
  this->onMRMLAstroModelingParametersNodeModified();

  if (d->plotChartNodeVRot)
    {
    d->plotChartNodeVRot->RemoveAllPlotDataNodeIDs();
    }
  if (d->plotChartNodeVRad)
    {
    d->plotChartNodeVRad->RemoveAllPlotDataNodeIDs();
    }
  if (d->plotChartNodeInc)
    {
    d->plotChartNodeInc->RemoveAllPlotDataNodeIDs();
    }
  if (d->plotChartNodePhi)
    {
    d->plotChartNodePhi->RemoveAllPlotDataNodeIDs();
    }
  if (d->plotChartNodeVSys)
    {
    d->plotChartNodeVSys->RemoveAllPlotDataNodeIDs();
    }
  if (d->plotChartNodeVDisp)
    {
    d->plotChartNodeVDisp->RemoveAllPlotDataNodeIDs();
    }
  if (d->plotChartNodeDens)
    {
    d->plotChartNodeDens->RemoveAllPlotDataNodeIDs();
    }
  if (d->plotChartNodeZ0)
    {
    d->plotChartNodeZ0->RemoveAllPlotDataNodeIDs();
    }
  if (d->plotChartNodeXPos)
    {
    d->plotChartNodeXPos->RemoveAllPlotDataNodeIDs();
    }
  if (d->plotChartNodeYPos)
    {
    d->plotChartNodeYPos->RemoveAllPlotDataNodeIDs();
    }

  d->InputSegmentCollapsibleButton->setCollapsed(true);
  d->FittingParametersCollapsibleButton->setCollapsed(false);
  d->OutputCollapsibleButton->setCollapsed(true);
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
void qSlicerAstroModelingModuleWidget::initializeParameterNode(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!scene || !d->selectionNode ||
      scene->IsClosing() || scene->IsBatchProcessing())
    {
    return;
    }

  vtkSmartPointer<vtkMRMLNode> parametersNode;
  unsigned int numNodes = scene->GetNumberOfNodesByClass("vtkMRMLAstroModelingParametersNode");
  if(numNodes > 0)
    {
    parametersNode = scene->GetNthNodeByClass(0, "vtkMRMLAstroModelingParametersNode");
    }
  else
    {
    vtkMRMLNode * foo = scene->CreateNodeByClass("vtkMRMLAstroModelingParametersNode");
    parametersNode.TakeReference(foo);
    scene->AddNode(parametersNode);
    }
  vtkMRMLAstroModelingParametersNode *astroParametersNode =
    vtkMRMLAstroModelingParametersNode::SafeDownCast(parametersNode);
  astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
  astroParametersNode->SetOutputVolumeNodeID(d->selectionNode->GetSecondaryVolumeID());
  astroParametersNode->SetMaskActive(false);

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);

  this->initializeTableNode(scene);

  if (!d->fiducialNodeMajor)
    {
    vtkMRMLNode *foo = scene->CreateNodeByClass("vtkMRMLMarkupsFiducialNode");
    d->fiducialNodeMajor.TakeReference(vtkMRMLMarkupsFiducialNode::SafeDownCast(foo));
    scene->AddNode(d->fiducialNodeMajor);
    d->fiducialNodeMajor->SetName("MarkupsFiducialsMajor");
    }

  if (!d->fiducialNodeMinor)
    {
    vtkMRMLNode *foo = scene->CreateNodeByClass("vtkMRMLMarkupsFiducialNode");
    d->fiducialNodeMinor.TakeReference(vtkMRMLMarkupsFiducialNode::SafeDownCast(foo));
    scene->AddNode(d->fiducialNodeMinor);
    d->fiducialNodeMinor->SetName("MarkupsFiducialsMinor");
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::initializeTableNode(vtkMRMLScene *scene, bool forceNew/* = false */)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!scene || !d->parametersNode ||
      scene->IsClosing() || scene->IsBatchProcessing())
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
    vtkSmartPointer<vtkCollection> TableNodes = vtkSmartPointer<vtkCollection>::Take
        (scene->GetNodesByClass("vtkMRMLTableNode"));

    for (int ii = 0; ii < TableNodes->GetNumberOfItems(); ii++)
      {
      vtkMRMLTableNode* tempTableNode = vtkMRMLTableNode::SafeDownCast(TableNodes->GetItemAsObject(ii));
      if (!tempTableNode)
        {
        continue;
        }
      std::string TableName = tempTableNode->GetName();
      std::size_t found = TableName.find("ModelingParamsTable");
      if (found != std::string::npos)
        {
        tableNode = tempTableNode;
        }
      }
    }
  if (!tableNode)
    {
    vtkMRMLNode * foo = scene->CreateNodeByClass("vtkMRMLTableNode");
    tableNode.TakeReference(foo);
    std::string paramsTableNodeName = scene->GenerateUniqueName("ModelingParamsTable");
    tableNode->SetName(paramsTableNodeName.c_str());
    scene->AddNode(tableNode);
    }

  d->astroTableNode = vtkMRMLTableNode::SafeDownCast(tableNode);
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
  d->astroTableNode->SetColumnUnitLabel("VSys", "km/s");
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

  this->qvtkReconnect(d->astroTableNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLTableNodeModified()));

  d->parametersNode->SetParamsTableNode(d->astroTableNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::createPlots()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode ||
      this->mrmlScene()->IsClosing() || this->mrmlScene()->IsBatchProcessing())
    {
    return;
    }

  vtkMRMLTableNode *tableNode = d->parametersNode->GetParamsTableNode();

 if (!tableNode)
   {
   qWarning() <<"qSlicerAstroModelingModuleWidget::createPlots : "
                "Unable to find the table.";
   return;
   }

  // Create plotDataNodes
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

  plotDataNodeVRot.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
  plotDataNodeVRad.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
  plotDataNodeInc.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
  plotDataNodePhi.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
  plotDataNodeVSys.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
  plotDataNodeVDisp.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
  plotDataNodeDens.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
  plotDataNodeZ0.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
  plotDataNodeXPos.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));
  plotDataNodeYPos.TakeReference(vtkMRMLPlotDataNode::SafeDownCast
    (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotDataNode")));

  this->mrmlScene()->AddNode(plotDataNodeVRot);
  this->mrmlScene()->AddNode(plotDataNodeVRad);
  this->mrmlScene()->AddNode(plotDataNodeInc);
  this->mrmlScene()->AddNode(plotDataNodePhi);
  this->mrmlScene()->AddNode(plotDataNodeVSys);
  this->mrmlScene()->AddNode(plotDataNodeVDisp);
  this->mrmlScene()->AddNode(plotDataNodeDens);
  this->mrmlScene()->AddNode(plotDataNodeZ0);
  this->mrmlScene()->AddNode(plotDataNodeXPos);
  this->mrmlScene()->AddNode(plotDataNodeYPos);

  // Set Properties of PlotDataNodes
  plotDataNodeVRot->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodeVRot->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodeVRot->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnVRot));
  plotDataNodeVRot->SetName("VRot");

  plotDataNodeVRad->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodeVRad->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodeVRad->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnVRad));
  plotDataNodeVRad->SetName("VRad");

  plotDataNodeInc->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodeInc->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodeInc->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnInc));
  plotDataNodeInc->SetName("Inc");

  plotDataNodePhi->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodePhi->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodePhi->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnPhi));
  plotDataNodePhi->SetName("Phi");

  plotDataNodeVSys->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodeVSys->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodeVSys->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnVSys));
  plotDataNodeVSys->SetName("VSys");

  plotDataNodeVDisp->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodeVDisp->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodeVDisp->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnVDisp));
  plotDataNodeVDisp->SetName("VDisp");

  plotDataNodeDens->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodeDens->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodeDens->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnDens));
  plotDataNodeDens->SetName("Dens");

  plotDataNodeZ0->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodeZ0->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodeZ0->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnZ0));
  plotDataNodeZ0->SetName("Z0");

  plotDataNodeXPos->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodeXPos->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodeXPos->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnXPos));
  plotDataNodeXPos->SetName("XPos");

  plotDataNodeYPos->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNodeYPos->SetXColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnRadii));
  plotDataNodeYPos->SetYColumnName(tableNode->GetColumnName
    (vtkMRMLAstroModelingParametersNode::ParamsColumnYPos));
  plotDataNodeYPos->SetName("YPos");

  // Check (and create) PlotChart nodes
  if (!d->plotChartNodeVRot)
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
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeVRot->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeVRot);
    }

  if (!d->plotChartNodeVRad)
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
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeVRad->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeVRad);
    }

  if (!d->plotChartNodeInc)
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
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeInc->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeInc);
    }

  if (!d->plotChartNodePhi)
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
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodePhi->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodePhi);
    }

  if (!d->plotChartNodeVSys)
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
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeVSys->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeVSys);
    }

  if (!d->plotChartNodeVDisp)
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
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeVDisp->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeVDisp);
    }

  if (!d->plotChartNodeDens)
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
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeDens->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeDens);
    }

  if (!d->plotChartNodeZ0)
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
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeZ0->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeZ0);
    }

  if (!d->plotChartNodeXPos)
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
  else if (!this->mrmlScene()->GetNodeByID(d->plotChartNodeXPos->GetID()))
    {
    this->mrmlScene()->AddNode(d->plotChartNodeXPos);
    }

  if (!d->plotChartNodeYPos)
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

  d->plotChartNodeVRot->AddAndObservePlotDataNodeID(plotDataNodeVRot->GetID());
  d->plotChartNodeVRad->AddAndObservePlotDataNodeID(plotDataNodeVRad->GetID());
  d->plotChartNodeInc->AddAndObservePlotDataNodeID(plotDataNodeInc->GetID());
  d->plotChartNodePhi->AddAndObservePlotDataNodeID(plotDataNodePhi->GetID());
  d->plotChartNodeVSys->AddAndObservePlotDataNodeID(plotDataNodeVSys->GetID());
  d->plotChartNodeVDisp->AddAndObservePlotDataNodeID(plotDataNodeVDisp->GetID());
  d->plotChartNodeDens->AddAndObservePlotDataNodeID(plotDataNodeDens->GetID());
  d->plotChartNodeZ0->AddAndObservePlotDataNodeID(plotDataNodeZ0->GetID());
  d->plotChartNodeXPos->AddAndObservePlotDataNodeID(plotDataNodeXPos->GetID());
  d->plotChartNodeYPos->AddAndObservePlotDataNodeID(plotDataNodeYPos->GetID());

  //Select VRot
  d->selectionNode->SetActivePlotChartID(d->plotChartNodeVRot->GetID());
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


  this->qvtkConnect(this->mrmlScene(), vtkMRMLScene::EndImportEvent,
                    this, SLOT(onMRMLSceneEndImportEvent()));
  this->qvtkConnect(this->mrmlScene(), vtkMRMLScene::EndBatchProcessEvent,
                    this, SLOT(onMRMLSceneEndBatchProcessEvent()));
  this->qvtkConnect(this->mrmlScene(), vtkMRMLScene::EndCloseEvent,
                    this, SLOT(onMRMLSceneEndCloseEvent()));
  this->qvtkConnect(this->mrmlScene(), vtkMRMLScene::EndRestoreEvent,
                    this, SLOT(onMRMLSceneEndRestoreEvent()));
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onCalculateAndVisualize()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
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

  vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
  if (!selectionNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onCalculateAndVisualize :"
                  " selectionNode not found!";
    return;
    }

  char *activeVolumeNodeID = selectionNode->GetActiveVolumeID();
  char *secondaryVolumeNodeID = selectionNode->GetSecondaryVolumeID();

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
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "arrays not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  double PhiMean = 0., XPosMean = 0., YPosMean = 0.;

  for (int ii = 0; ii < Phi->GetNumberOfValues(); ii++)
    {
    PhiMean += Phi->GetValue(ii);
    XPosMean += XPos->GetValue(ii);
    YPosMean += YPos->GetValue(ii);
    }

  PhiMean /=  Phi->GetNumberOfValues();
  XPosMean /=  XPos->GetNumberOfValues();
  YPosMean /=  YPos->GetNumberOfValues();

  double PVPhi = -(PhiMean - 90.);

  vtkMRMLAstroVolumeNode *activeVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(activeVolumeNodeID));
  if (!activeVolume || !activeVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
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
  activeVolume->GetIJKToRASMatrix(IJKtoRASMatrix);
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix);

  double ijk[3] = {0.,0.,0.}, RAS[3] = {0.,0.,0.};
  ijk[0] = XPosMean;
  ijk[1] = YPosMean;
  ijk[2] = Zcenter;
  IJKtoRASTransform->TransformPoint(ijk, RAS);

  d->astroVolumeWidget->updateQuantitative3DView
        (activeVolumeNodeID,
         secondaryVolumeNodeID,
         d->parametersNode->GetContourLevel(),
         PVPhi, RAS, true);
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

  if (!sender)
    {
    return;
    }

  vtkMRMLSelectionNode *selectionNode =
      vtkMRMLSelectionNode::SafeDownCast(sender);

  if (!d->parametersNode || !selectionNode)
    {
    return;
    }

  unsigned int numNodes = this->mrmlScene()->
      GetNumberOfNodesByClass("vtkMRMLAstroModelingParametersNode");
  if(numNodes == 0)
    {
    this->initializeParameterNode(selectionNode->GetScene());
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
  d->parametersNode->SetOutputVolumeNodeID(selectionNode->GetSecondaryVolumeID());
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
void qSlicerAstroModelingModuleWidget::onMRMLTableNodeModified()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->fiducialNodeMajor || !d->fiducialNodeMinor || !d->parametersNode)
    {
    return;
    }

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    return;
    }

  vtkImageData* imageData = inputVolume->GetImageData();
  if (!imageData)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode *inputVolumeDisplayNode =
    inputVolume->GetAstroVolumeDisplayNode();
  if(!inputVolumeDisplayNode)
    {
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
    return;
    }

  if (d->fiducialNodeMajor->GetNumberOfFiducials() != Radii->GetNumberOfValues() * 2 ||
      d->fiducialNodeMinor->GetNumberOfFiducials() != Radii->GetNumberOfValues() * 2)
    {
    return;
    }

  double *VRotPointer = static_cast<double*> (VRot->GetPointer(0));
  double *PhiPointer = static_cast<double*> (Phi->GetPointer(0));
  double *XPosPointer = static_cast<double*> (XPos->GetPointer(0));
  double *YPosPointer = static_cast<double*> (YPos->GetPointer(0));
  double *VRadPointer = static_cast<double*> (VRad->GetPointer(0));
  double *IncPointer = static_cast<double*> (Inc->GetPointer(0));
  double *VSysPointer = static_cast<double*> (VSys->GetPointer(0));
  double *RadiiPointer = static_cast<double*> (Radii->GetPointer(0));

  double PhiMean = 0., XMean = 0., YMean = 0.;
  for (int ii = 0; ii < Phi->GetNumberOfValues(); ii++)
    {
    PhiMean += *(PhiPointer + ii);
    XMean += *(XPosPointer + ii);
    YMean += *(YPosPointer + ii);
    }
  PhiMean /=  Phi->GetNumberOfValues();
  XMean /=  XPos->GetNumberOfValues();
  YMean /=  YPos->GetNumberOfValues();
  double PVPhi = -(PhiMean - 90.);

  const double arcsec2deg = 1. / 3600.;
  double ijk[3] = {0.,0.,0.}, RAS[3] = {0.,0.,0.}, worldPositive[3] = {0.,0.,0.},
         worldNegative[3] = {0.,0.,0.}, worldCenter[3] = {0.,0.,0.};
  int *dims = imageData->GetDimensions();
  int Zcenter = dims[2] * 0.5;

  const double deg2rad = PI / 180.;
  double factor = 0.;
  if (PVPhi > 0.001 && PVPhi - 90. > 0.001)
    {
    factor = -180.;
    }
  else if (PVPhi < 0.001 && PVPhi + 90. < 0.001)
    {
    factor = +180.;
    }
  PVPhi += factor;
  ijk[0] = XMean + (10 * cos(PVPhi * deg2rad));
  ijk[1] = YMean + (10 * sin(PVPhi * deg2rad));
  ijk[2] = Zcenter;
  inputVolumeDisplayNode->GetReferenceSpace(ijk, worldPositive);
  ijk[0] = XMean + (10 * cos((PVPhi + 90.) * deg2rad));
  ijk[1] = YMean + (10 * sin((PVPhi + 90.) * deg2rad));
  ijk[2] = Zcenter;
  inputVolumeDisplayNode->GetReferenceSpace(ijk, worldNegative);
  ijk[0] = XMean;
  ijk[1] = YMean;
  ijk[2] = Zcenter;
  inputVolumeDisplayNode->GetReferenceSpace(ijk, worldCenter);

  double distX = (worldPositive[0] - worldCenter[0]);
  double distY = (worldPositive[1] - worldCenter[1]);
  double PVPhiMajor = atan(distY / distX);
  PVPhiMajor += factor * deg2rad;

  distX = (worldNegative[0] - worldCenter[0]);
  distY = (worldNegative[1] - worldCenter[1]);
  double PVPhiMinor = atan(distY / distX);
  PVPhiMinor += factor * deg2rad;

  double PVPhiMajorCos = cos(-PVPhiMajor);
  double PVPhiMajorSin = sin(-PVPhiMajor);

  double PVPhiMinorCos = cos(-PVPhiMinor);
  double PVPhiMinorSin = sin(-PVPhiMinor);

  vtkNew<vtkGeneralTransform> IJKtoRASTransform;
  IJKtoRASTransform->Identity();
  IJKtoRASTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  inputVolume->GetIJKToRASMatrix(IJKtoRASMatrix);
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix);

  d->fiducialNodeMajor->GlobalWarningDisplayOff();
  d->fiducialNodeMinor->GlobalWarningDisplayOff();
  int MajorWasModifying = d->fiducialNodeMajor->StartModify();
  int MinorWasModifying = d->fiducialNodeMinor->StartModify();

  for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues(); radiiIndex++)
    {
    int positiveIndex = radiiIndex * 2;
    int negativeIndex = (radiiIndex * 2) + 1;
    // Update fiducials on Major axis
    double VelocitySin = *(VRotPointer + radiiIndex) * sin(*(IncPointer + radiiIndex) * deg2rad);
    double VelocityMajorPositive = - VelocitySin + *(VSysPointer + radiiIndex);
    double ShiftX = *(RadiiPointer + radiiIndex) * arcsec2deg * PVPhiMajorCos;
    double ShiftY = *(RadiiPointer + radiiIndex) * arcsec2deg * PVPhiMajorSin;
    worldPositive[0] = worldCenter[0] - ShiftX;
    worldPositive[1] = worldCenter[1] - ShiftY;
    worldPositive[2] = VelocityMajorPositive * 1000;
    inputVolumeDisplayNode->GetIJKSpace(worldPositive, ijk);
    IJKtoRASTransform->TransformPoint(ijk, RAS);
    d->fiducialNodeMajor->SetNthFiducialPosition(positiveIndex, RAS[0], RAS[1], RAS[2]);

    double VelocityMajorNegative = VelocitySin + *(VSysPointer + radiiIndex);
    worldNegative[0] = worldCenter[0] + ShiftX;
    worldNegative[1] = worldCenter[1] + ShiftY;
    worldNegative[2] = VelocityMajorNegative * 1000;
    inputVolumeDisplayNode->GetIJKSpace(worldNegative, ijk);
    IJKtoRASTransform->TransformPoint(ijk, RAS);
    d->fiducialNodeMajor->SetNthFiducialPosition(negativeIndex, RAS[0], RAS[1], RAS[2]);

    // Update fiducials on the Minor axis
    VelocitySin = *(VRadPointer + radiiIndex) * sin(*(IncPointer + radiiIndex) * deg2rad);
    double VelocityMinorPositive = VelocitySin + VSys->GetValue(radiiIndex);
    ShiftX = Radii->GetValue(radiiIndex) * arcsec2deg * PVPhiMinorCos;
    ShiftY = Radii->GetValue(radiiIndex) * arcsec2deg * PVPhiMinorSin;
    worldPositive[0] = worldCenter[0] - ShiftX;
    worldPositive[1] = worldCenter[1] - ShiftY;
    worldPositive[2] = VelocityMinorPositive * 1000;
    inputVolumeDisplayNode->GetIJKSpace(worldPositive, ijk);
    IJKtoRASTransform->TransformPoint(ijk, RAS);
    d->fiducialNodeMinor->SetNthFiducialPosition(positiveIndex, RAS[0], RAS[1], RAS[2]);

    double VelocityMinorNegative = - VelocitySin + VSys->GetValue(radiiIndex);
    worldNegative[0] = worldCenter[0] + ShiftX;
    worldNegative[1] = worldCenter[1] + ShiftY;
    worldNegative[2] = VelocityMinorNegative * 1000;
    inputVolumeDisplayNode->GetIJKSpace(worldNegative, ijk);
    IJKtoRASTransform->TransformPoint(ijk, RAS);
    d->fiducialNodeMinor->SetNthFiducialPosition(negativeIndex, RAS[0], RAS[1], RAS[2]);
    }

  d->fiducialNodeMajor->EndModify(MajorWasModifying);
  d->fiducialNodeMinor->EndModify(MinorWasModifying);
  d->fiducialNodeMajor->GlobalWarningDisplayOn();
  d->fiducialNodeMinor->GlobalWarningDisplayOn();
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

  if (!mrmlNode || !this->mrmlScene() ||
      this->mrmlScene()->IsClosing() || this->mrmlScene()->IsBatchProcessing())
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

  d->parametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
  d->parametersNode->SetOutputVolumeNodeID(d->selectionNode->GetSecondaryVolumeID());
  d->parametersNode->SetMaskActive(false);


  if (!d->parametersNode->GetParamsTableNode())
    {
    this->initializeTableNode(this->mrmlScene());
    }

  this->qvtkReconnect(d->parametersNode, AstroModelingParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroModelingParametersNodeModified()));

  this->onMRMLAstroModelingParametersNodeModified();

  this->setEnabled(AstroModelingParaNode != 0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::setPVOffset()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode || yellowSliceNode->GetOrientation().compare("PVMajor"))
    {
    return;
    }
  yellowSliceNode->GetSliceToRAS()->SetElement(0, 3, d->XPosMean);
  yellowSliceNode->GetSliceToRAS()->SetElement(1, 3, 0.);
  yellowSliceNode->GetSliceToRAS()->SetElement(2, 3, d->YPosMean);
  yellowSliceNode->UpdateMatrices();
  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode || greenSliceNode->GetOrientation().compare("PVMinor"))
    {
    return;
    }
  greenSliceNode->GetSliceToRAS()->SetElement(0, 3, d->XPosMean);
  greenSliceNode->GetSliceToRAS()->SetElement(1, 3, 0.);
  greenSliceNode->GetSliceToRAS()->SetElement(2, 3, d->YPosMean);
  greenSliceNode->UpdateMatrices();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode || !this->mrmlScene() ||
      this->mrmlScene()->IsClosing() || this->mrmlScene()->IsBatchProcessing())
    {
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    return;
    }
  vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
  if (!selectionNode)
    {
    return;
    }

  if (mrmlNode)
    {
    selectionNode->SetReferenceActiveVolumeID(mrmlNode->GetID());
    selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    d->XcenterSliderWidget->setMaximum(StringToInt(mrmlNode->GetAttribute("SlicerAstro.NAXIS1")));
    d->YcenterSliderWidget->setMaximum(StringToInt(mrmlNode->GetAttribute("SlicerAstro.NAXIS2")));
    }
  else
    {
    selectionNode->SetReferenceActiveVolumeID(NULL);
    selectionNode->SetActiveVolumeID(NULL);
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

  if (!d->parametersNode || !this->mrmlScene() ||
      this->mrmlScene()->IsClosing() || this->mrmlScene()->IsBatchProcessing())
    {
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    return;
    }
  vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
  if (!selectionNode)
    {
    return;
    }

  if (mrmlNode)
    {
    selectionNode->SetReferenceSecondaryVolumeID(mrmlNode->GetID());
    selectionNode->SetSecondaryVolumeID(mrmlNode->GetID());
    }
  else
    {
    selectionNode->SetReferenceSecondaryVolumeID(NULL);
    selectionNode->SetSecondaryVolumeID(NULL);
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
  d->fiducialNodeMajor->GlobalWarningDisplayOn();

  d->fiducialNodeMinor->GlobalWarningDisplayOff();
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

  if (!d->parametersNode)
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

  d->TableView->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourSliderWidget->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourLabel->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourLabelUnit->setEnabled(d->parametersNode->GetFitSuccess());
  d->VisualizePushButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->CalculatePushButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->CopyButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->PasteButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->PlotButton->setEnabled(d->parametersNode->GetFitSuccess());

  // Set params table to table view
  if (d->TableView->mrmlTableNode() != d->parametersNode->GetParamsTableNode())
    {
    d->TableView->setMRMLTableNode(d->parametersNode->GetParamsTableNode());
    }

  d->TableNodeComboBox->setCurrentNode(d->parametersNode->GetParamsTableNode());
}

void qSlicerAstroModelingModuleWidget::onMRMLSceneEndImportEvent()
{
  //this->onMRMLAstroModelingParametersNodeModified();
}

void qSlicerAstroModelingModuleWidget::onMRMLSceneEndRestoreEvent()
{
  //this->onMRMLAstroModelingParametersNodeModified();
}

void qSlicerAstroModelingModuleWidget::onMRMLSceneEndBatchProcessEvent()
{
  //this->onMRMLAstroModelingParametersNodeModified();
}

void qSlicerAstroModelingModuleWidget::onMRMLSceneEndCloseEvent()
{
  //this->onMRMLAstroModelingParametersNodeModified();
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
    this->initializeTableNode(this->mrmlScene(), true);
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

  wcsprm* wcs = inputVolumeDisplayNode->GetWCSStruct();
  if(!wcs)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : "
                  "WCS not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (strncmp(wcs->ctype[2], "VRAD", 4))
    {
    QString message = QString("The input datacube 3rd axes is an optical velocity. "
                              "In order to proceed with the modeling, it is required to transform the "
                              "3rd axes into a radio velocity. A conversion tool is "
                              "available in the AstroVolume module. ");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to run 3DBarolo"), message);

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

  vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
  if (!selectionNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : selectionNode not found!";
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

  if (!d->fiducialNodeMajor || !d->fiducialNodeMinor)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onApply() : FiducialNodes not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->fiducialNodeMajor->RemoveAllMarkups();
  d->fiducialNodeMinor->RemoveAllMarkups();

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
  if (!d->parametersNode)
    {
    return;
    }

  d->astroTableNode = vtkMRMLTableNode::SafeDownCast(mrmlNode);
  this->qvtkReconnect(d->astroTableNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLTableNodeModified()));
  d->parametersNode->SetParamsTableNode(d->astroTableNode);
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

  if (!d->parametersNode || !this->mrmlScene())
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

  vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
  if (!selectionNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onVisualize : selectionNode not found!";
    return;
    }

  char *activeVolumeNodeID = selectionNode->GetActiveVolumeID();
  char *secondaryVolumeNodeID = selectionNode->GetSecondaryVolumeID();

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
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "arrays not found!";
    return;
    }

  double PhiMean = 0., XPosMean = 0., YPosMean = 0.;

  for (int ii = 0; ii < Phi->GetNumberOfValues(); ii++)
    {
    PhiMean += Phi->GetValue(ii);
    XPosMean += XPos->GetValue(ii);
    YPosMean += YPos->GetValue(ii);
    }

  PhiMean /=  Phi->GetNumberOfValues();
  XPosMean /=  XPos->GetNumberOfValues();
  YPosMean /=  YPos->GetNumberOfValues();

  double PVPhi = -(PhiMean - 90.);

  vtkMRMLAstroVolumeNode *activeVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(activeVolumeNodeID));
  if (!activeVolume || !activeVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
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
  activeVolume->GetIJKToRASMatrix(IJKtoRASMatrix);
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix);

  double ijk[3] = {0.,0.,0.}, RAS[3] = {0.,0.,0.};
  ijk[0] = XPosMean;
  ijk[1] = YPosMean;
  ijk[2] = Zcenter;
  IJKtoRASTransform->TransformPoint(ijk, RAS);

  d->astroVolumeWidget->updateQuantitative3DView
        (activeVolumeNodeID,
         secondaryVolumeNodeID,
         d->parametersNode->GetContourLevel(),
         PVPhi, RAS, false);
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

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "scene not found!";
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "inputVolume node not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkImageData* imageData = inputVolume->GetImageData();
  if (!imageData)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "imageData not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkPointData* pointData = imageData->GetPointData();
  if (!pointData)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "pointData not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkDataArray *dataArray = pointData->GetScalars();
  if (!dataArray)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "dataArray not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeDisplayNode *inputVolumeDisplayNode =
    inputVolume->GetAstroVolumeDisplayNode();
  if(!inputVolumeDisplayNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "display node not found!";
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
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->parametersNode->GetParamsTableNode() ||
      !d->parametersNode->GetParamsTableNode()->GetTable())
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                  "Table not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (d->parametersNode->GetFitSuccess())
    {
    d->parametersNode->GetParamsTableNode()->Copy(d->internalTableNode);
    this->createPlots();

    outputVolume->UpdateNoiseAttributes();
    outputVolume->UpdateRangeAttributes();
    outputVolume->SetAttribute("SlicerAstro.DATAMODEL", "MODEL");

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
      d->parametersNode->SetStatus(0);
      return;
      }

    double PhiMean = 0.;

    d->XPosMean = 0.;
    d->YPosMean = 0.;
    for (int ii = 0; ii < Phi->GetNumberOfValues(); ii++)
      {
      PhiMean += Phi->GetValue(ii);
      d->XPosMean += XPos->GetValue(ii);
      d->YPosMean += YPos->GetValue(ii);
      }

    PhiMean /=  Phi->GetNumberOfValues();
    d->XPosMean /=  XPos->GetNumberOfValues();
    d->YPosMean /=  YPos->GetNumberOfValues();

    double PVPhi = -(PhiMean - 90.);

    int *dims = imageData->GetDimensions();
    int Zcenter = dims[2] * 0.5;
    double XMean = d->XPosMean;
    double YMean = d->YPosMean;

    vtkNew<vtkGeneralTransform> IJKtoRASTransform;
    IJKtoRASTransform->Identity();
    IJKtoRASTransform->PostMultiply();
    vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
    inputVolume->GetIJKToRASMatrix(IJKtoRASMatrix);
    IJKtoRASTransform->Concatenate(IJKtoRASMatrix);

    double ijk[3] = {0.,0.,0.}, RAS[3] = {0.,0.,0.};
    ijk[0] = XMean;
    ijk[1] = YMean;
    ijk[2] = Zcenter;
    IJKtoRASTransform->TransformPoint(ijk, RAS);
    d->XPosMean = RAS[0];
    d->YPosMean = RAS[2];

    d->astroVolumeWidget->setQuantitative3DView
        (inputVolume->GetID(), outputVolume->GetID(),
         residualVolume->GetID(), d->parametersNode->GetContourLevel(),
         PVPhi, RAS);

    d->InputSegmentCollapsibleButton->setCollapsed(true);
    d->FittingParametersCollapsibleButton->setCollapsed(true);
    d->OutputCollapsibleButton->setCollapsed(false);

    // Force again the offset of the PV
    // It will be good to understand why the active node
    // in the selection node takes so much time to be updated.
    // P.S.: FitAllSlice is called everytime the active volume is changed.

    QTimer::singleShot(10, this, SLOT(setPVOffset()));

    // Connect PlotWidget with ModelingWidget
    // Setting the Layout for the Output
    qSlicerApplication* app = qSlicerApplication::application();

    if(!app)
      {
      qCritical() << "qSlicerAstroMomentMapsModuleWidget::onWorkFinished : "
                     "qSlicerApplication not found!";
      d->parametersNode->SetStatus(0);
      return;
      }

    qSlicerLayoutManager* layoutManager = app->layoutManager();

    if(!app)
      {
      qCritical() << "qSlicerAstroMomentMapsModuleWidget::onWorkFinished : "
                     "layoutManager not found!";
      d->parametersNode->SetStatus(0);
      return;
      }

    qMRMLPlotWidget* plotWidget = layoutManager->plotWidget(0);

    if(!plotWidget)
      {
      qCritical() << "qSlicerAstroMomentMapsModuleWidget::onWorkFinished : "
                     "plotWidget not found!";
      d->parametersNode->SetStatus(0);
      return;
      }

    qMRMLPlotView* plotView = plotWidget->plotView();
    if(!plotWidget)
      {
      qCritical() << "qSlicerAstroMomentMapsModuleWidget::onWorkFinished : "
                     "plotView not found!";
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
      d->parametersNode->SetStatus(0);
      return;
      }

    double *VRotPointer = static_cast<double*> (VRot->GetPointer(0));
    double *VRadPointer = static_cast<double*> (VRad->GetPointer(0));
    double *IncPointer = static_cast<double*> (Inc->GetPointer(0));
    double *VSysPointer = static_cast<double*> (VSys->GetPointer(0));
    double *RadiiPointer = static_cast<double*> (Radii->GetPointer(0));

    const double arcsec2deg = 1. / 3600.;
    double worldPositive[3] = {0.,0.,0.}, worldNegative[3] = {0.,0.,0.},
           worldCenter[3] = {0.,0.,0.};

    const double deg2rad = PI / 180.;
    double factor = 0.;
    if (PVPhi > 0.001 && PVPhi - 90. > 0.001)
      {
      factor = -180.;
      }
    else if (PVPhi < 0.001 && PVPhi + 90. < 0.001)
      {
      factor = +180.;
      }
    PVPhi += factor;
    ijk[0] = XMean + (10 * cos(PVPhi * deg2rad));
    ijk[1] = YMean + (10 * sin(PVPhi * deg2rad));
    ijk[2] = Zcenter;
    inputVolumeDisplayNode->GetReferenceSpace(ijk, worldPositive);
    ijk[0] = XMean + (10 * cos((PVPhi + 90.) * deg2rad));
    ijk[1] = YMean + (10 * sin((PVPhi + 90.) * deg2rad));
    ijk[2] = Zcenter;
    inputVolumeDisplayNode->GetReferenceSpace(ijk, worldNegative);
    ijk[0] = XMean;
    ijk[1] = YMean;
    ijk[2] = Zcenter;
    inputVolumeDisplayNode->GetReferenceSpace(ijk, worldCenter);

    double distX = (worldPositive[0] - worldCenter[0]);
    double distY = (worldPositive[1] - worldCenter[1]);
    double PVPhiMajor = atan(distY / distX);
    PVPhiMajor += factor * deg2rad;

    distX = (worldNegative[0] - worldCenter[0]);
    distY = (worldNegative[1] - worldCenter[1]);
    double PVPhiMinor = atan(distY / distX);
    PVPhiMinor += factor * deg2rad;

    double PVPhiMajorCos = cos(-PVPhiMajor);
    double PVPhiMajorSin = sin(-PVPhiMajor);

    double PVPhiMinorCos = cos(-PVPhiMinor);
    double PVPhiMinorSin = sin(-PVPhiMinor);

    double MinorSign = sign(cos((PVPhi + 30.) * deg2rad));

    // Create fiducials
    for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues() * 2; radiiIndex++)
      {
      d->fiducialNodeMajor->AddFiducial(0., 0., 0.);
      d->fiducialNodeMinor->AddFiducial(0., 0., 0.);
      }

    d->fiducialNodeMajor->GlobalWarningDisplayOff();
    d->fiducialNodeMinor->GlobalWarningDisplayOff();
    int MajorWasModifying = d->fiducialNodeMajor->StartModify();
    int MinorWasModifying = d->fiducialNodeMinor->StartModify();

    for (int radiiIndex = 0; radiiIndex < Radii->GetNumberOfValues(); radiiIndex++)
      {
      int positiveIndex = radiiIndex * 2;
      int negativeIndex = (radiiIndex * 2) + 1;

      // Update fiducials on the Major axes:
      // Center of PV is in (X0,Y0), this means that the angle beta is equal
      // to the angle of the slice. Therefore, the angle theta is zero on the semi-major axes.
      // Project velocity (theta zero)
      double VelocitySin = *(VRotPointer + radiiIndex) * sin(*(IncPointer + radiiIndex) * deg2rad);
      double VelocityMajorPositive = - VelocitySin + *(VSysPointer + radiiIndex);

      // Project radius (beta equal to PVPhiMajor)
      double ProjectedRadius = *(RadiiPointer + radiiIndex) * arcsec2deg;
      double ShiftX = ProjectedRadius * PVPhiMajorCos;
      double ShiftY = ProjectedRadius * PVPhiMajorSin;

      worldPositive[0] = worldCenter[0] - ShiftX;
      worldPositive[1] = worldCenter[1] - ShiftY;
      worldPositive[2] = VelocityMajorPositive * 1000;
      inputVolumeDisplayNode->GetIJKSpace(worldPositive, ijk);
      IJKtoRASTransform->TransformPoint(ijk, RAS);
      d->fiducialNodeMajor->SetNthFiducialPosition(positiveIndex, RAS[0], RAS[1], RAS[2]);

      double VelocityMajorNegative = VelocitySin + *(VSysPointer + radiiIndex);
      worldNegative[0] = worldCenter[0] + ShiftX;
      worldNegative[1] = worldCenter[1] + ShiftY;
      worldNegative[2] = VelocityMajorNegative * 1000;
      inputVolumeDisplayNode->GetIJKSpace(worldNegative, ijk);
      IJKtoRASTransform->TransformPoint(ijk, RAS);
      d->fiducialNodeMajor->SetNthFiducialPosition(negativeIndex, RAS[0], RAS[1], RAS[2]);

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

      // Update fiducials on the Minor axes
      // Project velocity
      double beta = 90 * deg2rad;
      double theta = atan2(tan(beta), cos(*(IncPointer + radiiIndex) * deg2rad));
      VelocitySin = *(VRadPointer + radiiIndex) * sin(*(IncPointer + radiiIndex) * deg2rad) * sin(theta) +
                    *(VRotPointer + radiiIndex) * sin(*(IncPointer + radiiIndex) * deg2rad) * cos(theta);
      double VelocityMinorPositive = VelocitySin + VSys->GetValue(radiiIndex);

      // Project radius
      ProjectedRadius = Radii->GetValue(radiiIndex) * arcsec2deg * cos(theta) / cos(beta);
      ShiftX = ProjectedRadius * PVPhiMinorCos;
      ShiftY = ProjectedRadius * PVPhiMinorSin;
      worldPositive[0] = worldCenter[0] + ShiftX * MinorSign;
      worldPositive[1] = worldCenter[1] + ShiftY * MinorSign;
      worldPositive[2] = VelocityMinorPositive * 1000;
      inputVolumeDisplayNode->GetIJKSpace(worldPositive, ijk);
      IJKtoRASTransform->TransformPoint(ijk, RAS);
      d->fiducialNodeMinor->SetNthFiducialPosition(positiveIndex, RAS[0], RAS[1], RAS[2]);

      double VelocityMinorNegative = - VelocitySin + VSys->GetValue(radiiIndex);
      worldNegative[0] = worldCenter[0] - ShiftX * MinorSign;
      worldNegative[1] = worldCenter[1] - ShiftY * MinorSign;
      worldNegative[2] = VelocityMinorNegative * 1000;
      inputVolumeDisplayNode->GetIJKSpace(worldNegative, ijk);
      IJKtoRASTransform->TransformPoint(ijk, RAS);
      d->fiducialNodeMinor->SetNthFiducialPosition(negativeIndex, RAS[0], RAS[1], RAS[2]);

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
    d->fiducialNodeMajor->GlobalWarningDisplayOn();
    d->fiducialNodeMinor->GlobalWarningDisplayOn();

    // Change scale value for the display of the fiducials
    vtkMRMLMarkupsDisplayNode *fiducialsMajorDisplayNode =
      d->fiducialNodeMajor->GetMarkupsDisplayNode();
    if (!fiducialsMajorDisplayNode)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                    "fiducial display node not found!";
      d->parametersNode->SetStatus(0);
      return;
      }

    fiducialsMajorDisplayNode->SetGlyphScale(1.5);
    fiducialsMajorDisplayNode->SetTextScale(0.);
    fiducialsMajorDisplayNode->RemoveAllViewNodeIDs();
    vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
    if (!yellowSliceNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::onWorkFinished : "
                    "yellowSliceNode not found!";
      return;
      }
    fiducialsMajorDisplayNode->AddViewNodeID(yellowSliceNode->GetID());

    vtkMRMLMarkupsDisplayNode *fiducialsMinorDisplayNode =
      d->fiducialNodeMinor->GetMarkupsDisplayNode();
    if (!fiducialsMinorDisplayNode)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                    "fiducial display node not found!";
      d->parametersNode->SetStatus(0);
      return;
      }

    fiducialsMinorDisplayNode->SetGlyphScale(1.5);
    fiducialsMinorDisplayNode->SetTextScale(0.);
    fiducialsMinorDisplayNode->SetColor(1., 1., 0.44);
    fiducialsMinorDisplayNode->RemoveAllViewNodeIDs();
    vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
    if (!greenSliceNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::onWorkFinished : "
                    "greenSliceNode not found!";
      return;
      }
    fiducialsMinorDisplayNode->AddViewNodeID(greenSliceNode->GetID());
    }
   else
    {
    scene->RemoveNode(outputVolume);

    inputVolume->SetDisplayVisibility(1);

    scene->RemoveNode(residualVolume);

    if (!d->parametersNode->GetMaskActive())
      {
      d->parametersNode->SetStatus(0);
      return;
      }

    vtkMRMLAstroLabelMapVolumeNode *maskVolume =
      vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMaskVolumeNodeID()));

    if(!maskVolume)
      {
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
