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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

// Qt includes
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
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCamera.h>
#include <vtkCollection.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>
#include <vtkMatrixToLinearTransform.h>
#include <vtkNew.h>
#include <vtkParametricEllipsoid.h>
#include <vtkParametricFunctionSource.h>
#include <vtkPointData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtksys/SystemTools.hxx>
#include <vtkTable.h>

// SlicerQt includes
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
#include <qMRMLSegmentsTableView.h>
#include <qMRMLTableView.h>

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
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentEditorNode.h>
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
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
  qMRMLTableView *MRMLTableView;
  qSlicerAstroModelingModuleWorker *worker;
  QPushButton *CalculatePushButton;
  QThread *thread;
};

//-----------------------------------------------------------------------------
// qSlicerAstroModelingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidgetPrivate::qSlicerAstroModelingModuleWidgetPrivate(qSlicerAstroModelingModuleWidget& object)
  : q_ptr(&object)
{
  this->astroVolumeWidget = 0;
  this->MRMLTableView = 0;
  this->CalculatePushButton = 0;
  this->worker = 0;
  this->thread = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroModelingModuleWidgetPrivate::~qSlicerAstroModelingModuleWidgetPrivate()
{
  if (this->astroVolumeWidget)
    {
    delete this->astroVolumeWidget;
    }

  if (this->MRMLTableView)
    {
    delete this->MRMLTableView;
    }

  if (this->CalculatePushButton)
    {
    delete this->CalculatePushButton;
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

  QObject::connect(InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(OutputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));

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

  QObject::connect(DISPRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onRotationVelocityFitChanged(bool)));

  QObject::connect(VROTRadioButton, SIGNAL(toggled(bool)),
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

  QObject::connect(VisualizePushButton, SIGNAL(clicked()),
                   q, SLOT(onVisualize()));

  QObject::connect(ApplyButton, SIGNAL(clicked()),
                   q, SLOT(onApply()));

  QObject::connect(CancelButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));


  MRMLTableView = new qMRMLTableView(OutputCollapsibleButton);
  MRMLTableView->setObjectName(QString::fromUtf8("MRMLTableView"));
  QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
  sizePolicy1.setHeightForWidth(MRMLTableView->sizePolicy().hasHeightForWidth());
  MRMLTableView->setSizePolicy(sizePolicy1);
  MRMLTableView->setMinimumSize(QSize(0, 0));
  MRMLTableView->setEditTriggers(QAbstractItemView::AnyKeyPressed|
                                 QAbstractItemView::DoubleClicked|
                                 QAbstractItemView::EditKeyPressed|
                                 QAbstractItemView::SelectedClicked);
  MRMLTableView->setAlternatingRowColors(true);
  MRMLTableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerItem);
  MRMLTableView->setCornerButtonEnabled(false);
  MRMLTableView->setEnabled(false);

  gridLayout_3->addWidget(MRMLTableView, 2, 0, 1, 1);

  CalculatePushButton = new QPushButton(OutputCollapsibleButton);
  CalculatePushButton->setObjectName(QString::fromUtf8("CalculatePushButton"));
  CalculatePushButton->setMinimumSize(QSize(0, 35));
  CalculatePushButton->setEnabled(false);
  CalculatePushButton->setText("Calculate and Visualize Model");
  CalculatePushButton->setToolTip("Click to recalculate and visualize the model using"
                                  " the parameters in Tabular. The model and the data"
                                  " will be visualized at the chosen Contour Level. ");

  gridLayout_3->addWidget(CalculatePushButton, 3, 0, 1, 1);

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   MRMLTableView, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(CalculatePushButton, SIGNAL(clicked()),
                   q, SLOT(onCalculateAndVisualize()));

  InputSegmentCollapsibleButton->setCollapsed(false);
  FittingParametersCollapsibleButton->setCollapsed(false);
  OutputCollapsibleButton->setCollapsed(true);

  progressBar->hide();
  progressBar->setMinimum(0);
  progressBar->setMaximum(100);
  CancelButton->hide();

  this->thread = new QThread();
  this->worker = new qSlicerAstroModelingModuleWorker();

  this->worker->moveToThread(thread);

  this->worker->SetAstroModelingLogic(this->logic());
  this->worker->SetAstroModelingParametersNode(this->parametersNode);

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

  d->parametersNode->SetMaskActive(true);

  d->InputSegmentCollapsibleButton->setCollapsed(false);
  d->FittingParametersCollapsibleButton->setCollapsed(false);
  d->OutputCollapsibleButton->setCollapsed(true);
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onEndCloseEvent()
{
  this->initializeParameterNode(this->mrmlScene());
  this->onMRMLAstroModelingParametersNodeModified();
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

  if (!scene)
    {
    return;
    }

  vtkSmartPointer<vtkMRMLNode> parametersNode;
  unsigned int numNodes = scene->
      GetNumberOfNodesByClass("vtkMRMLAstroModelingParametersNode");
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
  astroParametersNode->SetMaskActive(true);
  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
bool qSlicerAstroModelingModuleWidget::convertFirstSegmentToLabelMap()
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
    QString message = QString("No segmentation selected! Please provide a mask or untoggle the input"
                              " mask option to perform automatic masking with Bbarolo.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
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
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              " be sure that segment to export have been selected in the table view (left click).! \n\n "
                              " Otherwise, most probably the segment cannot be converted into representation"
                              " corresponding to the selected representation node.").
                                arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
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
      qCritical() <<"qSlicerAstroModelingModuleWidget::convertFirstSegmentToLabelMap :"
                    " astroModelinglogic not found!";
      return false;
      }
    vtkSlicerAstroVolumeLogic* astroVolumelogic =
      vtkSlicerAstroVolumeLogic::SafeDownCast(astroModelinglogic->GetAstroVolumeLogic());
    if (!astroVolumelogic)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::convertFirstSegmentToLabelMap :"
                    " vtkSlicerAstroVolumeLogic not found!";
      return false;
      }
    std::string name(activeVolumeNode->GetName());
    name += "Copy_mask";
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

  if (!vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode(currentSegmentationNode, segmentIDs, labelMapNode))
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\nMost probably"
                              " the segment cannot be converted into representation corresponding to the selected representation node \n or"
                              " be sure that segment to export are present in the table view.").
                              arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    this->mrmlScene()->RemoveNode(labelMapNode);
    return false;
    }

  labelMapNode->GetAstroLabelMapVolumeDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeFileGenericColors.txt");

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

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onCalculateAndVisualize()
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!d->parametersNode)
    {
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

  if (!d->logic()->UpdateModelFromTable(d->parametersNode))
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onCalculateAndVisualize :"
                  " UpdateModel error!";
    return;
    }

  d->astroVolumeWidget->updateQuantitative3DView
        (activeVolumeNodeID,
         secondaryVolumeNodeID,
         d->parametersNode->GetContourLevel(),
         true);
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

  this->qvtkReconnect(d->parametersNode, AstroModelingParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroModelingParametersNodeModified()));

  d->parametersNode = AstroModelingParaNode;

  this->onMRMLAstroModelingParametersNodeModified();

  if (!d->MRMLTableView)
    {
    return;
    }

  d->MRMLTableView->setMRMLTableNode(d->parametersNode->GetParamsTableNode());
  d->MRMLTableView->resizeColumnsToContents();
  d->MRMLTableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);

  this->qvtkReconnect(d->parametersNode->GetParamsTableNode(), vtkCommand::ModifiedEvent,
                   this, SLOT(onParamsTableNodeModified(vtkObject*)));

  this->setEnabled(AstroModelingParaNode != 0);
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
void qSlicerAstroModelingModuleWidget::onParamsTableNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroModelingModuleWidget);

  if (!sender)
    {
    return;
    }

  vtkMRMLTableNode *paramsTableNode =
      vtkMRMLTableNode::SafeDownCast(sender);

  if (!d->parametersNode || !paramsTableNode || !this->mrmlScene())
    {
    return;
    }

  vtkCollection* colCharts = d->parametersNode->GetChartNodes();
  if (!colCharts)
    {
    qCritical()<<"qSlicerAstroModelingModuleWidget::"
                   "onParamsTableNodeModified : colCharts invalid!"<<endl;
    return;
    }

  vtkCollection* colArray = d->parametersNode->GetArrayNodes();
  if (!colArray)
    {
    qCritical()<<"qSlicerAstroModelingModuleWidget::"
                   "onParamsTableNodeModified : colArray invalid!"<<endl;
    return;
    }

  vtkSmartPointer<vtkCollection> colChartViews = vtkSmartPointer<vtkCollection>::Take
    (this->mrmlScene()->GetNodesByClass("vtkMRMLChartViewNode"));
  if (!colChartViews)
    {
    qCritical()<<"qSlicerAstroModelingModuleWidget::"
                   "onParamsTableNodeModified : colChartViews invalid!"<<endl;
    return;
    }

  colChartViews->InitTraversal();
  vtkMRMLChartViewNode* chartViewNode = vtkMRMLChartViewNode::SafeDownCast
          (colChartViews->GetNextItemAsObject());
  if (!chartViewNode)
    {
    qCritical()<<"qSlicerAstroModelingModuleWidget::"
                 "onParamsTableNodeModified : chartNodeView invalid!"<<endl;
    return;
    }

  std::string chartVRotID;
  bool chartIDOn = false;

  for(int jj = 0; jj < colCharts->GetNumberOfItems(); jj++)
    {
    vtkMRMLChartNode* chartNode =
      vtkMRMLChartNode::SafeDownCast(colCharts->GetItemAsObject(jj));
    if (!chartNode)
      {
      continue;
      }

    vtkMRMLDoubleArrayNode* arrayNode = NULL;
    for(int kk = 0; kk < colArray->GetNumberOfItems(); kk++)
      {
      vtkMRMLDoubleArrayNode* iterArrayNode =
        vtkMRMLDoubleArrayNode::SafeDownCast(colArray->GetItemAsObject(kk));
      if (!iterArrayNode)
        {
        continue;
        }

      std::string arrayName(iterArrayNode->GetName());
      std::string temp = chartNode->GetName();
      temp.erase(temp.begin(), temp.begin()+5);
      size_t found = arrayName.find(temp);

      if (found != std::string::npos)
        {
        size_t foundFirst = arrayName.find("first");
        if (foundFirst != std::string::npos)
          {
          if (d->parametersNode->GetFirstPlot())
            {
            arrayNode = iterArrayNode;
            break;
            }
          else
            {
            continue;
            }
          }
        else
          {
          if (d->parametersNode->GetFirstPlot())
            {
            continue;
            }
          else
            {
            arrayNode = iterArrayNode;
            break;
            }
          }
        }
      }

    if (!arrayNode)
      {
      qCritical()<<"qSlicerAstroModelingModuleWidget::"
                   "onParamsTableNodeModified : arrayNode invalid!"<<endl;
      }

    vtkDoubleArray* data = arrayNode->GetArray();

    if (!data)
      {
      qCritical()<<"qSlicerAstroModelingModuleWidget::"
                   "onParamsTableNodeModified : array invalid!"<<endl;
      }
    data->SetNumberOfTuples(d->parametersNode->GetNumberOfRings());

    if (!strcmp(chartNode->GetName(), "chartVRot"))
      {
      chartVRotID = chartNode->GetID();
      }

    for (int ii = 0; ii < d->parametersNode->GetNumberOfRings(); ii++)
      {
      data->SetComponent(ii, 0, StringToDouble(paramsTableNode->GetCellText(
                         ii, vtkMRMLAstroModelingParametersNode::ParamsColumnRadii).c_str()));
      data->SetComponent(ii, 2, 0.);

      if (!strcmp(chartNode->GetName(), "chartXPos"))
        {
        data->SetComponent(ii, 1, StringToDouble(paramsTableNode->GetCellText(
                           ii, vtkMRMLAstroModelingParametersNode::ParamsColumnXPos).c_str()));
        }
      else if (!strcmp(chartNode->GetName(), "chartYPos"))
        {
        data->SetComponent(ii, 1, StringToDouble(paramsTableNode->GetCellText(
                           ii, vtkMRMLAstroModelingParametersNode::ParamsColumnYPos).c_str()));
        }
      else if (!strcmp(chartNode->GetName(), "chartVSys"))
        {
        data->SetComponent(ii, 1, StringToDouble(paramsTableNode->GetCellText(
                           ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVSys).c_str()));
        }
      else if (!strcmp(chartNode->GetName(), "chartVRot"))
        {
        data->SetComponent(ii, 1, StringToDouble(paramsTableNode->GetCellText(
                           ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVRot).c_str()));
        }
      else if (!strcmp(chartNode->GetName(), "chartVDisp"))
        {
        data->SetComponent(ii, 1, StringToDouble(paramsTableNode->GetCellText(
                           ii, vtkMRMLAstroModelingParametersNode::ParamsColumnVDisp).c_str()));
        }
      else if (!strcmp(chartNode->GetName(), "chartDens"))
        {
        data->SetComponent(ii, 1, StringToDouble(paramsTableNode->GetCellText(
                           ii, vtkMRMLAstroModelingParametersNode::ParamsColumnDens).c_str()));
        }
      else if (!strcmp(chartNode->GetName(), "chartZ0"))
        {
        data->SetComponent(ii, 1, StringToDouble(paramsTableNode->GetCellText(
                           ii, vtkMRMLAstroModelingParametersNode::ParamsColumnZ0).c_str()));
        }
      else if (!strcmp(chartNode->GetName(), "chartInc"))
        {
        data->SetComponent(ii, 1, StringToDouble(paramsTableNode->GetCellText(
                           ii, vtkMRMLAstroModelingParametersNode::ParamsColumnInc).c_str()));
        }
      else if (!strcmp(chartNode->GetName(), "chartPhi"))
        {
        data->SetComponent(ii, 1, StringToDouble(paramsTableNode->GetCellText(
                           ii, vtkMRMLAstroModelingParametersNode::ParamsColumnPhi).c_str()));
        }
      else
        {
        qCritical()<<"qSlicerAstroModelingModuleWidget::"
                     "onParamsTableNodeModified : chartNode invalid!"<<endl;
        return;
        }
      }

    chartNode->AddArray(chartNode->GetName(), arrayNode->GetID());
    chartNode->Modified();

    if (chartViewNode->GetChartNodeID() != NULL &&
        !strcmp(chartViewNode->GetChartNodeID(), chartNode->GetID()))
      {
      chartIDOn = true;
      }

    }

  if (!chartIDOn)
    {
    chartViewNode->SetChartNodeID(chartVRotID.c_str());
    }

  chartViewNode->Modified();

  d->parametersNode->SetFirstPlot(false);
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

  d->MRMLTableView->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourSliderWidget->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourLabel->setEnabled(d->parametersNode->GetFitSuccess());
  d->ContourLabelUnit->setEnabled(d->parametersNode->GetFitSuccess());
  d->VisualizePushButton->setEnabled(d->parametersNode->GetFitSuccess());
  d->CalculatePushButton->setEnabled(d->parametersNode->GetFitSuccess());

  // Set params table to table view
  if (d->MRMLTableView->mrmlTableNode() != d->parametersNode->GetParamsTableNode())
    {
    d->MRMLTableView->setMRMLTableNode( d->parametersNode->GetParamsTableNode());
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
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onApply()
{
  Q_D(const qSlicerAstroModelingModuleWidget);

  vtkSlicerAstroModelingLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : astroModelingLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onApply() : parametersNode not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->parametersNode->SetStatus(1);
  d->parametersNode->SetFitSuccess(false);
  d->parametersNode->SetFirstPlot(true);

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : scene not found!";
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : inputVolume not found!";
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
    QMessageBox::warning(NULL, tr("Failed to run Bbarolo"), message);
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
    QMessageBox::warning(NULL, tr("Failed to run Bbarolo"), message);
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
  serial++;
  d->parametersNode->SetOutputSerial(serial);

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

  // Check if there are segment and feed the mask to Bbarolo
  if (d->parametersNode->GetMaskActive())
    {
    if (!this->convertFirstSegmentToLabelMap())
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onApply() : convertFirstSegmentToLabelMap failed!";
      d->parametersNode->SetStatus(0);
      return;
      }
    }

  d->worker->SetAstroModelingParametersNode(d->parametersNode);
  d->worker->SetAstroModelingLogic(logic);
  d->worker->requestWork();
}

//-----------------------------------------------------------------------------
void qSlicerAstroModelingModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroModelingModuleWidget);
  d->CancelButton->hide();
  d->progressBar->hide();
  d->ApplyButton->show();
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

  if (!d->parametersNode)
    {
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

  d->astroVolumeWidget->updateQuantitative3DView
        (activeVolumeNodeID,
         secondaryVolumeNodeID,
         d->parametersNode->GetContourLevel());
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

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : appLogic not found!";
    return;
    }

  vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
  if (!selectionNode)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : selectionNode not found!";
    return;
    }

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : scene not found!";
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : inputVolume not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));
  if(!outputVolume)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : outputVolume not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (d->parametersNode->GetFitSuccess())
    {
    outputVolume->UpdateNoiseAttributes();
    outputVolume->UpdateRangeAttributes();
    outputVolume->SetAttribute("SlicerAstro.DATATYPE", "MODEL");

    vtkSlicerAstroModelingLogic *logic = d->logic();
    if (!logic)
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : astroModelingLogic not found!";
      return;
      }

    if (!logic->UpdateTable(d->parametersNode))
      {
      qCritical() <<"qSlicerAstroModelingModuleWidget::onWorkFinished : UpdateTable error!";
      return;
      }

    selectionNode->SetReferenceActiveVolumeID(outputVolume->GetID());
    // this should be not needed. However, without it seems that
    // the connection with the Rendering Display is broken.

    d->astroVolumeWidget->setQuantitative3DView
        (inputVolume->GetID(), outputVolume->GetID(), d->parametersNode->GetContourLevel());

    selectionNode->SetReferenceActiveVolumeID(inputVolume->GetID());
    selectionNode->SetReferenceSecondaryVolumeID(outputVolume->GetID());
    appLogic->PropagateVolumeSelection();
    d->InputSegmentCollapsibleButton->setCollapsed(true);
    d->FittingParametersCollapsibleButton->setCollapsed(true);
    d->OutputCollapsibleButton->setCollapsed(false);

    }
   else
    {
    scene->RemoveNode(outputVolume);

    inputVolume->SetDisplayVisibility(1);

    if (!d->parametersNode->GetMaskActive())
      {
      return;
      }

    vtkMRMLAstroLabelMapVolumeNode *maskVolume =
      vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMaskVolumeNodeID()));

    if(!maskVolume)
      {
      return;
      }

    scene->RemoveNode(maskVolume);
    }
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
  d->ApplyButton->hide();
  d->progressBar->show();
  d->CancelButton->show();
}

//---------------------------------------------------------------------------
vtkMRMLAstroModelingParametersNode* qSlicerAstroModelingModuleWidget::
mrmlAstroModelingParametersNode()const
{
  Q_D(const qSlicerAstroModelingModuleWidget);
  return d->parametersNode;
}

