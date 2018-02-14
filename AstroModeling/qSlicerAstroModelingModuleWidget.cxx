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

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroModelingParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLDoubleArrayNode.h>
#include <vtkMRMLChartNode.h>
#include <vtkMRMLChartViewNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLPlotSeriesNode.h>
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

  QObject::connect(this->VisualizePushButton, SIGNAL(clicked()),
                   q, SLOT(onVisualize()));

  QObject::connect(this->CalculatePushButton, SIGNAL(clicked()),
                   q, SLOT(onCalculateAndVisualize()));

  QObject::connect(this->YellowSliceSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onYellowSliceRotated(double)));

  QObject::connect(this->GreenSliceSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onGreenSliceRotated(double)));

  this->InputSegmentCollapsibleButton->setCollapsed(true);
  this->FittingParametersCollapsibleButton->setCollapsed(false);
  this->OutputCollapsibleButton->setCollapsed(true);
  this->OutputCollapsibleButton_2->setCollapsed(true);

  this->progressBar->hide();
  this->progressBar->setMinimum(0);
  this->progressBar->setMaximum(100);
  this->CancelPushButton->hide();

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
  Q_Q(const qSlicerAstroModelingModuleWidget);

  if (!q->mrmlScene())
    {
    return;
    }

  if (this->segmentEditorNode)
    {
    if (this->segmentEditorNode->GetSegmentationNode())
      {
      q->mrmlScene()->RemoveNode(this->segmentEditorNode->GetSegmentationNode());
      this->segmentEditorNode->SetAndObserveSegmentationNode(NULL);
      }
    q->mrmlScene()->RemoveNode(this->segmentEditorNode);
    }
  if (this->SegmentsTableView)
    {
    if (this->SegmentsTableView->segmentationNode())
      {
      q->mrmlScene()->RemoveNode(this->SegmentsTableView->segmentationNode());
      this->SegmentsTableView->setSegmentationNode(NULL);
      }
    }

  if (this->parametersNode)
    {
    q->mrmlScene()->RemoveNode(this->parametersNode);
    }
  this->parametersNode = 0;

  if (this->internalTableNode)
    {
    q->mrmlScene()->RemoveNode(this->internalTableNode);
    }
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
}

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidget::~qSlicerAstroModelingModuleWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::enter()
{
  this->Superclass::enter();
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

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceAddedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceAdded(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceRemovedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceRemoved(vtkObject*)));

  this->onMRMLSelectionNodeModified(d->selectionNode);
  this->initializeNodes();
  this->onInputVolumeChanged(scene->GetNodeByID(d->selectionNode->GetActiveVolumeID()));
  this->onMRMLSelectionNodeReferenceAdded(d->selectionNode);
  this->onMRMLAstroModelingParametersNodeModified();

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

  vtkMRMLNode *activeVolume = scene->GetNodeByID(d->selectionNode->GetActiveVolumeID());
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
    qCritical() << Q_FUNC_INFO << ": segmentEditorNode not found.";
    return false;
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

  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> labelMapNode;

  QStringList selectedSegmentIDs = d->SegmentsTableView->selectedSegmentIDs();

  if (selectedSegmentIDs.size() < 1)
    {
    QString message = QString("No segment selected from the segmentation node! Please provide a mask or untoggle the input"
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

  if (!vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode
       (currentSegmentationNode, segmentIDs, labelMapNode, activeVolumeNode))
    {
    QString message = QString("Failed to export segments from segmentation '%1'' to representation node '%2!.").
                              arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
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

  if (!sender || !this->mrmlScene())
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
    d->XcenterSliderWidget->setMaximum(StringToInt(mrmlNode->GetAttribute("SlicerAstro.NAXIS1")));
    d->YcenterSliderWidget->setMaximum(StringToInt(mrmlNode->GetAttribute("SlicerAstro.NAXIS2")));
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(NULL);
    d->selectionNode->SetActiveVolumeID(NULL);
    }
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

  if (!d->parametersNode || !mrmlNode)
    {
    return;
    }

  d->parametersNode->SetOutputVolumeNodeID(mrmlNode->GetID());
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
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

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
  d->DISPRadioButton->setChecked(d->parametersNode->GetVelocityDispersionFit());
  d->VROTRadioButton->setChecked(d->parametersNode->GetRotationVelocityFit());
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
void qSlicerAstroModelingModuleWidget::onMRMLSceneStartImportEvent()
{
  Q_D(qSlicerAstroModelingModuleWidget);

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

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onApply() : parametersNode not found!";
    return;
    }

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
  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_model";

  int serial = d->parametersNode->GetOutputSerial();
  outSS<<"_"<< IntToString(serial);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  if (outputVolume && strcmp(inputVolume->GetID(), outputVolume->GetID()))
    {
    vtkMRMLAstroVolumeStorageNode* astroStorage =
      vtkMRMLAstroVolumeStorageNode::SafeDownCast(outputVolume->GetStorageNode());
    scene->RemoveNode(astroStorage);
    scene->RemoveNode(outputVolume->GetDisplayNode());

    vtkMRMLVolumeRenderingDisplayNode *volumeRenderingDisplay =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(outputVolume->GetDisplayNode());
    if (volumeRenderingDisplay)
      {
      scene->RemoveNode(volumeRenderingDisplay->GetROINode());
      scene->RemoveNode(volumeRenderingDisplay);
      }
    scene->RemoveNode(outputVolume->GetVolumePropertyNode());
    scene->RemoveNode(outputVolume);
    }

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
      qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : "
                    "convertSelectedSegmentToLabelMap failed!";
      d->parametersNode->SetStatus(0);
      return;
      }
    }
  else if (!d->parametersNode->GetMaskActive() && d->parametersNode->GetNumberOfRings() == 0)
    {
    QString message = QString("No mask has been provided. 3DBarolo will search and fit the"
                              " largest source in the datacube. Do you wish to continue?");
    qWarning() << Q_FUNC_INFO << ": " << message;
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(NULL, tr("3DBarolo"), message);
    if (reply != QMessageBox::Yes)
      {
      d->parametersNode->SetStatus(0);
      return;
      }
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
    outputVolume->Update3DDisplayThresholdAttributes();
    outputVolume->UpdateRangeAttributes();
    outputVolume->SetAttribute("SlicerAstro.DATAMODEL", "MODEL");

    residualVolume->Update3DDisplayThresholdAttributes();
    residualVolume->UpdateRangeAttributes();
    outputVolume->SetAttribute("SlicerAstro.DATAMODEL", "DATA");

    if (!d->internalTableNode || !d->internalTableNode->GetTable())
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : "
                    "tables not found!";
      d->TableView->resizeColumnsToContents();
      d->parametersNode->SetStatus(0);
      return;
      }

    d->parametersNode->GetParamsTableNode()->GetTable()->DeepCopy(d->internalTableNode->GetTable());

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
