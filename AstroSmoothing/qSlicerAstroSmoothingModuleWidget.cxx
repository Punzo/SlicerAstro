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
#include <QDebug>
#include <QMessageBox>
#include <QTimer>

// CTK includes
#include <ctkFlowLayout.h>

// VTK includes
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCamera.h>
#include <vtkImageCast.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkMatrixToLinearTransform.h>
#include <vtkNew.h>
#include <vtkParametricEllipsoid.h>
#include <vtkParametricFunctionSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtksys/SystemTools.hxx>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>

// AstroSmoothing includes
#include "qSlicerAstroSmoothingModuleWidget.h"
#include "ui_qSlicerAstroSmoothingModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroSmoothingLogic.h>

// qMRML includes
#include <qMRMLSegmentsTableView.h>
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLAstroSmoothingParametersNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#define SigmatoFWHM 2.3548200450309493

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroSmoothing
class qSlicerAstroSmoothingModuleWidgetPrivate: public Ui_qSlicerAstroSmoothingModuleWidget
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
  qSlicerAstroVolumeModuleWidget* astroVolumeWidget;
  vtkSmartPointer<vtkMRMLAstroSmoothingParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
  vtkSmartPointer<vtkMRMLCameraNode> cameraNodeOne;
  vtkSmartPointer<vtkParametricEllipsoid> parametricVTKEllipsoid;
  vtkSmartPointer<vtkParametricFunctionSource> parametricFunctionSource;
  vtkSmartPointer<vtkMatrix4x4> transformationMatrix;
  vtkSmartPointer<vtkMatrixToLinearTransform> matrixToLinearTransform;
  vtkSmartPointer<vtkPolyDataMapper> mapper;
  vtkSmartPointer<vtkActor> actor;
  double DegToRad;

};

//-----------------------------------------------------------------------------
// qSlicerAstroSmoothingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModuleWidgetPrivate::qSlicerAstroSmoothingModuleWidgetPrivate(qSlicerAstroSmoothingModuleWidget& object)
  : q_ptr(&object)
{
  this->astroVolumeWidget = nullptr;
  this->parametersNode = nullptr;
  this->selectionNode = nullptr;
  this->cameraNodeOne = nullptr;
  this->segmentEditorNode = nullptr;
  this->parametricVTKEllipsoid = vtkSmartPointer<vtkParametricEllipsoid>::New();
  this->parametricFunctionSource = vtkSmartPointer<vtkParametricFunctionSource>::New();
  this->transformationMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  this->transformationMatrix->Identity();
  this->matrixToLinearTransform = vtkSmartPointer<vtkMatrixToLinearTransform>::New();
  this->mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->actor = vtkSmartPointer<vtkActor>::New();
  this->DegToRad = atan(1.) / 45.;
}

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModuleWidgetPrivate::~qSlicerAstroSmoothingModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroSmoothingModuleWidget);
  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();
  if(!app)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidgetPrivate::init : "
                   "could not find qSlicerApplication!";
    return;
    }

  qSlicerAbstractCoreModule* astroVolume = app->moduleManager()->module("AstroVolume");
  if (!astroVolume)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidgetPrivate::init : "
                   "could not find AstroVolume module.";
    return;
    }

  this->astroVolumeWidget = dynamic_cast<qSlicerAstroVolumeModuleWidget*>
    (astroVolume->widgetRepresentation());

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->ParametersNodeComboBox, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->InputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->OutputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->FilterCollapsibleButton, SLOT(setEnabled(bool)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->NodesCollapsibleButton, SLOT(setEnabled(bool)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->AutoRunCheckBox, SLOT(setEnabled(bool)));

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroSmoothingParametersNode(vtkMRMLNode*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->OutputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->ManualModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(this->AutomaticModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(this->MasksGenerateModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onMasksCommandChanged()));

  QObject::connect(this->MasksSkipModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onMasksCommandChanged()));

  QObject::connect(this->FilterComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onCurrentFilterChanged(int)));

  QObject::connect(this->DoubleSpinBoxX, SIGNAL(valueChanged(double)),
                   q, SLOT(onParameterXChanged(double)));

  QObject::connect(this->DoubleSpinBoxY, SIGNAL(valueChanged(double)),
                   q, SLOT(onParameterYChanged(double)));

  QObject::connect(this->DoubleSpinBoxZ, SIGNAL(valueChanged(double)),
                   q, SLOT(onParameterZChanged(double)));

  QObject::connect(this->AccuracySpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onAccuracyChanged(double)));

  QObject::connect(this->KSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onKChanged(double)));

  QObject::connect(this->TimeStepSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onTimeStepChanged(double)));

  QObject::connect(this->RxSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRxChanged(double)));

  QObject::connect(this->RySpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRyChanged(double)));

  QObject::connect(this->RzSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRzChanged(double)));

  QObject::connect(this->ApplyButton, SIGNAL(clicked()),
                   q, SLOT(onApply()));

  QObject::connect(this->CancelButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));

  QObject::connect(this->HardwareComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onHardwareChanged(int)));

  QObject::connect(this->LinkCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onLinkChanged(bool)));

  QObject::connect(this->AutoRunCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onAutoRunChanged(bool)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SegmentsTableView, SLOT(setMRMLScene(vtkMRMLScene*)));


  this->progressBar->hide();
  this->progressBar->setMinimum(0);
  this->progressBar->setMaximum(100);
  this->CancelButton->hide();
  this->KLabel->hide();
  this->KSpinBox->hide();
  this->TimeStepLabel->hide();
  this->TimeStepSpinBox->hide();
  this->OldBeamInfoLabel->hide();
  this->OldBeamInfoLineEdit->hide();
  this->NewBeamInfoLabel->hide();
  this->NewBeamInfoLineEdit->hide();
  this->GaussianKernelView->setOrientationWidgetVisible(true);
  vtkCamera* camera = this->GaussianKernelView->activeCamera();
  double eyePosition[3];
  eyePosition[0] = 0.;
  eyePosition[1] = 0.;
  eyePosition[2] = 30;
  camera->SetPosition(eyePosition);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroSmoothingModuleWidget);

  if (!q->mrmlScene())
    {
    return;
    }

  if (this->segmentEditorNode)
    {
    if (this->segmentEditorNode->GetSegmentationNode())
      {
      q->mrmlScene()->RemoveNode(this->segmentEditorNode->GetSegmentationNode());
      this->segmentEditorNode->SetAndObserveSegmentationNode(nullptr);
      }
    q->mrmlScene()->RemoveNode(this->segmentEditorNode);
    }
  if (this->SegmentsTableView)
    {
    if (this->SegmentsTableView->segmentationNode())
      {
      q->mrmlScene()->RemoveNode(this->SegmentsTableView->segmentationNode());
      this->SegmentsTableView->setSegmentationNode(nullptr);
      }
    }

  if (this->parametersNode)
    {
    q->mrmlScene()->RemoveNode(this->parametersNode);
    }
  this->parametersNode = 0;

  if (this->cameraNodeOne)
    {
    q->mrmlScene()->RemoveNode(this->cameraNodeOne);
    }
  this->cameraNodeOne = 0;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic* qSlicerAstroSmoothingModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroSmoothingModuleWidget);
  return vtkSlicerAstroSmoothingLogic::SafeDownCast(q->logic());
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
  this->Superclass::enter();

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager() || !app->layoutManager()->layoutLogic()
     || !app->layoutManager()->layoutLogic()->GetLayoutNode())
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::enter : "
                   "qSlicerApplication not found.";
    return;
    }

  app->layoutManager()->layoutLogic()->GetLayoutNode()->SetViewArrangement
          (vtkMRMLLayoutNode::SlicerLayoutDual3DView);
}

//----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::exit()
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

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

//----------------------------------------------------------------------------
template <typename T> bool isNaN(T value)
{
  return value != value;
}

//----------------------------------------------------------------------------
bool DoubleIsNaN(double Value)
{
  return isNaN<double>(Value);
}

} // end namespace

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
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  this->initializeParameterNode(forceNew);

  this->initializeSegmentations(forceNew);

  this->initializeCameras();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::onEndCloseEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
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
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::onEndImportEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroModelingModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroSmoothingParametersNodeModified();
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
    vtkSmartPointer<vtkMRMLNode> segmentationNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLSegmentationNode");
    segmentationNode.TakeReference(foo);
    this->mrmlScene()->AddNode(segmentationNode);
    d->segmentEditorNode->SetAndObserveSegmentationNode
      (vtkMRMLSegmentationNode::SafeDownCast(segmentationNode));
    }

  d->segmentEditorNode->GetSegmentationNode()->CreateDefaultDisplayNodes();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

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
}

//--------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

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
void qSlicerAstroSmoothingModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

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
void qSlicerAstroSmoothingModuleWidget::setMRMLAstroSmoothingParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroSmoothingParametersNode* AstroSmoothingParaNode =
      vtkMRMLAstroSmoothingParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroSmoothingParaNode, vtkCommand::ModifiedEvent,
                this, SLOT(onMRMLAstroSmoothingParametersNodeModified()));

  d->parametersNode = AstroSmoothingParaNode;

  this->onMRMLAstroSmoothingParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::initializeCameras()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
    (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));
  for(int ii = 0; ii < cameraNodes->GetNumberOfItems(); ii++)
    {
    vtkMRMLCameraNode *cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(ii));
    if (!cameraNode || cameraNode == d->cameraNodeOne)
      {
      continue;
      }
    d->cameraNodeOne = cameraNode;
    this->qvtkReconnect(d->cameraNodeOne, vtkCommand::ModifiedEvent,
                        this, SLOT(onMRMLCameraNodeModified()));
    this->onMRMLCameraNodeModified();
    break;
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

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
    this->qvtkConnect(mrmlNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onInputVolumeModified()));

    this->onInputVolumeModified();
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(nullptr);
    d->selectionNode->SetActiveVolumeID(nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onInputVolumeModified()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode || !this->mrmlScene() ||
     this->mrmlScene()->IsClosing() || this->mrmlScene()->IsBatchProcessing() ||
     this->mrmlScene()->IsImporting())
    {
    return;
    }

  vtkMRMLAstroVolumeNode* astroMrmlNode = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if (!astroMrmlNode)
    {
    return;
    }

  if (!strcmp(astroMrmlNode->GetAttribute("SlicerAstro.BMAJ"), "UNDEFINED") ||
      !strcmp(astroMrmlNode->GetAttribute("SlicerAstro.BMIN"), "UNDEFINED") ||
      !strcmp(astroMrmlNode->GetAttribute("SlicerAstro.BPA"), "UNDEFINED"))
    {
    d->OldBeamInfoLineEdit->setText("UNDEFINED");
    d->NewBeamInfoLineEdit->setText("UNDEFINED");
    }

  const double degtorad = atan(1.) / 45.;
  const double radtodeg = 45. / atan(1.);
  double degFactor = 1.;
  if (!strcmp(astroMrmlNode->GetAttribute("SlicerAstro.CUNIT1"), "DEGREE") ||
      !strcmp(astroMrmlNode->GetAttribute("SlicerAstro.CUNIT1"), "degree") ||
      !strcmp(astroMrmlNode->GetAttribute("SlicerAstro.CUNIT1"), "DEG") ||
      !strcmp(astroMrmlNode->GetAttribute("SlicerAstro.CUNIT1"), "deg"))
    {
    degFactor = 3600.;
    }

  double OldBeamMaj = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.BMAJ")) * degFactor;
  double OldBeamMin = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.BMIN")) * degFactor;
  double OldBeamPa = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.BPA"));

  double Kernel2DMaj, Kernel2DMin, Kernel2DPA;
  Kernel2DMaj = d->parametersNode->GetParameterX() * degFactor *
                StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.CDELT1"));
  double CDELT2 = 0.;
  if (StringToInt(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS")) > 1)
    {
    CDELT2 = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.CDELT2"));
    }
  Kernel2DMin = d->parametersNode->GetParameterY() * degFactor * CDELT2;
  Kernel2DPA = d->parametersNode->GetRz();

  double a2, b2, a0, b0, th2, th0;

  if (OldBeamMaj > Kernel2DMaj)
    {
    a2  = OldBeamMaj * 0.5;
    b2  = OldBeamMin * 0.5;
    a0  = Kernel2DMaj;
    b0  = Kernel2DMin;
    th2 = OldBeamPa * degtorad;
    th0 = Kernel2DPA * degtorad;
    }
  else
    {
    a2  = Kernel2DMaj;
    b2  = Kernel2DMin;
    a0  = OldBeamMaj * 0.5;
    b0  = OldBeamMin * 0.5;
    th2 = Kernel2DPA * degtorad;
    th0 = OldBeamPa * degtorad;
    }
  double D0  = a0 * a0 - b0 * b0;
  double D2  = a2 * a2 - b2 * b2;
  double D1  = sqrt(D0 * D0 + D2 * D2 - 2 * D0 * D2 * cos(2 * (th0 - th2)));

  double a1, b1, th1;

  double arg = 0.5 * (a0 * a0 + b0 * b0 - a2 * a2 - b2 * b2 + D1);
  a1 = sqrt(arg);

  arg = 0.5 * (a0 * a0 + b0 * b0 - a2 * a2 - b2 * b2 - D1);
  b1 = sqrt(arg);

  double nom   = D0 * sin(2 * th0) - D2 * sin(2 * th2);
  double denom = D0 * cos(2 * th0) - D2 * cos(2 * th2);
  if (denom == 0 && nom == 0)
    {
    th1 = 0.;
    }
  else
    {
    double twoth1 = atan2(nom, denom);
    th1 = twoth1 * 0.5;
    }

  double NewBeamMaj = 2 * a1;
  double NewBeamMin = 2 * b1;
  double NewBeamPa = th1 * radtodeg;

  std::string OldBeamString;
  OldBeamString = "BMAJ: " + DoubleToString(OldBeamMaj) + "\x22" +
                  "; BMIN: " + DoubleToString(OldBeamMin) + "\x22" +
                  "; BPA: " + DoubleToString(OldBeamPa) + "\u00B0 ";
  d->OldBeamInfoLineEdit->setText(OldBeamString.c_str());
  std::string NewBeamString;
  if (DoubleIsNaN(NewBeamMaj) || DoubleIsNaN(NewBeamMin))
    {
    NewBeamString = "BMAJ: UNDEFINED; BMIN: UNDEFINED; BPA: UNDEFINED";
    }
  else
    {
    NewBeamString = "BMAJ: " + DoubleToString(NewBeamMaj) + "\x22" +
                    "; BMIN: " + DoubleToString(NewBeamMin) + "\x22" +
                    "; BPA: " + DoubleToString(NewBeamPa) + "\u00B0 ";
    }
  d->NewBeamInfoLineEdit->setText(NewBeamString.c_str());
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

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
void qSlicerAstroSmoothingModuleWidget::onMRMLAstroSmoothingParametersNodeModified()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int status = d->parametersNode->GetStatus();

  const char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  const char *outputVolumeNodeID = d->parametersNode->GetOutputVolumeNodeID();
  vtkMRMLAstroVolumeNode *outputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(outputVolumeNodeID));
  d->OutputVolumeNodeSelector->setCurrentNode(outputVolumeNode);

  if (!(strcmp(d->parametersNode->GetMode(), "Automatic")))
    {
    d->AutomaticModeRadioButton->setChecked(true);
    d->FilterLabel->setEnabled(false);
    d->FilterComboBox->setEnabled(false);
    d->HardwareLabel->setEnabled(false);
    d->HardwareComboBox->setEnabled(false);
    d->LinkLabel->setEnabled(false);
    d->LinkCheckBox->setEnabled(false);
    d->SigmaXLabel->setEnabled(false);
    d->DoubleSpinBoxX->setEnabled(false);
    d->SigmaYLabel->setEnabled(false);
    d->DoubleSpinBoxY->setEnabled(false);
    d->SigmaZLabel->setEnabled(false);
    d->DoubleSpinBoxZ->setEnabled(false);
    d->KLabel->setEnabled(false);
    d->KSpinBox->setEnabled(false);
    d->TimeStepLabel->setEnabled(false);
    d->TimeStepSpinBox->setEnabled(false);
    d->AccuracyLabel->setEnabled(false);
    d->AccuracySpinBox->setEnabled(false);
    }
  else
    {
    d->ManualModeRadioButton->setChecked(true);
    d->FilterLabel->setEnabled(true);
    d->FilterComboBox->setEnabled(true);
    d->HardwareLabel->setEnabled(true);
    d->HardwareComboBox->setEnabled(true);
    d->LinkLabel->setEnabled(true);
    d->LinkCheckBox->setEnabled(true);
    d->SigmaXLabel->setEnabled(true);
    d->DoubleSpinBoxX->setEnabled(true);
    d->SigmaYLabel->setEnabled(true);
    d->DoubleSpinBoxY->setEnabled(true);
    d->SigmaZLabel->setEnabled(true);
    d->DoubleSpinBoxZ->setEnabled(true);
    d->KLabel->setEnabled(true);
    d->KSpinBox->setEnabled(true);
    d->TimeStepLabel->setEnabled(true);
    d->TimeStepSpinBox->setEnabled(true);
    d->AccuracyLabel->setEnabled(true);
    d->AccuracySpinBox->setEnabled(true);
    }

  if (!(strcmp(d->parametersNode->GetMasksCommand(), "Generate")))
    {
    d->MasksGenerateModeRadioButton->setChecked(true);
    }
  else
    {
    d->MasksSkipModeRadioButton->setChecked(true);
    }

  d->FilterComboBox->setCurrentIndex(d->parametersNode->GetFilter());
  d->HardwareComboBox->setCurrentIndex(d->parametersNode->GetHardware());

  d->AutoRunCheckBox->setChecked(d->parametersNode->GetAutoRun());
  d->LinkCheckBox->setChecked(d->parametersNode->GetLink());

  if(status == 0)
    {  
    switch (d->parametersNode->GetFilter())
      {
      case 0:
        {
        d->OldBeamInfoLabel->hide();
        d->OldBeamInfoLineEdit->hide();
        d->NewBeamInfoLabel->hide();
        d->NewBeamInfoLineEdit->hide();
        d->AccuracyLabel->hide();
        d->AccuracySpinBox->hide();
        d->AccuracyValueLabel->hide();
        d->HardwareLabel->show();
        d->HardwareComboBox->show();
        d->KLabel->hide();
        d->KSpinBox->hide();
        d->TimeStepLabel->hide();
        d->TimeStepSpinBox->hide();
        d->GaussianKernelView->hide();
        d->RxLabel->hide();
        d->RxSpinBox->hide();
        d->RyLabel->hide();
        d->RySpinBox->hide();
        d->RzLabel->hide();
        d->RzSpinBox->hide();
        d->LinkCheckBox->setToolTip("Click to link/unlink the parameters N<sub>X</sub>"
                                    ", N<sub>Y</sub> and N<sub>Z</sub>");
        d->CDELT1Label->show();
        d->CDELT1LabelValue->show();
        d->CDELT2Label->show();
        d->CDELT2LabelValue->show();
        d->CDELT3Label->show();
        d->CDELT3LabelValue->show();
        d->SigmaYLabel->show();
        d->DoubleSpinBoxY->show();
        d->SigmaZLabel->show();
        d->DoubleSpinBoxZ->show();
        if (inputVolumeNode)
          {
          if (StringToInt(inputVolumeNode->GetAttribute("SlicerAstro.NAXIS")) == 3)
            {
            double outputValues[3] = {0.}, oldOutputValues[3] = {0.};
            double cdelt1 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT1"));
            d->CDELT1LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
              ->GetDisplayStringFromValueX(cdelt1, oldOutputValues, outputValues, 3).c_str());
            double cdelt2 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT2"));
            d->CDELT2LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
              ->GetDisplayStringFromValueY(cdelt2, oldOutputValues, outputValues, 3).c_str());
            double cdelt3 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT3"));
            d->CDELT3LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
              ->GetDisplayStringFromValueZ(cdelt3, oldOutputValues, outputValues, 3).c_str());
            }
          }
        d->SigmaXLabel->setText("N<sub>X</sub>:");
        d->SigmaYLabel->setText("N<sub>Y</sub>:");
        d->SigmaZLabel->setText("N<sub>Z</sub>:");
        d->DoubleSpinBoxX->setSingleStep(2);
        d->DoubleSpinBoxX->setPageStep(4);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxX->blockSignals(true);
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          d->DoubleSpinBoxX->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          }
        d->DoubleSpinBoxX->setToolTip("Number of pixel of the Box kernel in the X direction.");
        d->DoubleSpinBoxY->setSingleStep(2);
        d->DoubleSpinBoxY->setPageStep(4);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxY->blockSignals(true);
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          d->DoubleSpinBoxY->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          }
        d->DoubleSpinBoxY->setToolTip("Number of pixel of the Box kernel in the Y direction.");
        d->DoubleSpinBoxZ->setSingleStep(2);
        d->DoubleSpinBoxZ->setPageStep(4);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxZ->blockSignals(true);
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          d->DoubleSpinBoxZ->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          }
        d->DoubleSpinBoxZ->setToolTip("Number of pixel of the Box kernel in the Z direction.");
        d->DoubleSpinBoxX->setMinimum(1);
        d->DoubleSpinBoxY->setMinimum(1);
        d->DoubleSpinBoxZ->setMinimum(1);
        d->DoubleSpinBoxX->setMaximum(11);
        d->DoubleSpinBoxY->setMaximum(11);
        d->DoubleSpinBoxZ->setMaximum(11);
        break;
        }
      case 1:
        {
        d->OldBeamInfoLabel->show();
        d->OldBeamInfoLineEdit->show();
        d->NewBeamInfoLabel->show();
        d->NewBeamInfoLineEdit->show();
        d->AccuracyLabel->show();
        d->AccuracySpinBox->show();
        d->AccuracyValueLabel->show();
        d->HardwareLabel->show();
        d->HardwareComboBox->show();
        d->KLabel->hide();
        d->KSpinBox->hide();
        d->TimeStepLabel->hide();
        d->TimeStepSpinBox->hide();
        d->LinkCheckBox->setToolTip("Click to link/unlink the parameters"
                                    " FWHM<sub>X</sub>, FWHM<sub>Y</sub> and FWHM<sub>Z</sub>.");
        d->CDELT1Label->show();
        d->CDELT1LabelValue->show();
        d->CDELT2Label->show();
        d->CDELT2LabelValue->show();
        d->CDELT3Label->show();
        d->CDELT3LabelValue->show();
        d->SigmaYLabel->show();
        d->DoubleSpinBoxY->show();
        d->SigmaZLabel->show();
        d->DoubleSpinBoxZ->show();
        if (inputVolumeNode)
          {
          if (StringToInt(inputVolumeNode->GetAttribute("SlicerAstro.NAXIS")) == 3)
            {
            double outputValues[3] = {0.}, oldOutputValues[3] = {0.};
            double cdelt1 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT1"));
            d->CDELT1LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
              ->GetDisplayStringFromValueY(cdelt1, oldOutputValues, outputValues, 3).c_str());
            double cdelt2 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT2"));
            d->CDELT2LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
              ->GetDisplayStringFromValueY(cdelt2, oldOutputValues, outputValues, 3).c_str());
            double cdelt3 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT3"));
            d->CDELT3LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
              ->GetDisplayStringFromValueZ(cdelt3, oldOutputValues, outputValues, 3).c_str());
            }
          }
        d->SigmaXLabel->setText("FWHM<sub>X</sub>:");
        d->SigmaYLabel->setText("FWHM<sub>Y</sub>:");
        d->SigmaZLabel->setText("FWHM<sub>Z</sub>:");
        d->DoubleSpinBoxX->setSingleStep(0.01);
        d->DoubleSpinBoxX->setPageStep(2);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxX->blockSignals(true);
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          d->DoubleSpinBoxX->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          }
        d->DoubleSpinBoxX->setToolTip("Full width at half maximum in pixel in the X direction.");
        d->DoubleSpinBoxY->setSingleStep(0.01);
        d->DoubleSpinBoxY->setPageStep(2);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxY->blockSignals(true);
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          d->DoubleSpinBoxY->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          }
        d->DoubleSpinBoxY->setToolTip("Full width at half maximum in pixel in the Y direction.");
        d->DoubleSpinBoxZ->setSingleStep(0.01);
        d->DoubleSpinBoxZ->setPageStep(2);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxZ->blockSignals(true);
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          d->DoubleSpinBoxZ->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          }
        d->DoubleSpinBoxZ->setToolTip("Full width at half maximum in pixel in the Z direction.");
        d->DoubleSpinBoxX->setMinimum(1);
        d->DoubleSpinBoxY->setMinimum(1);
        d->DoubleSpinBoxZ->setMinimum(1);
        d->DoubleSpinBoxX->setMaximum(11);
        d->DoubleSpinBoxY->setMaximum(11);
        d->DoubleSpinBoxZ->setMaximum(11);
        QString theta = QChar(0x98, 0x03);
        d->RxLabel->setText(theta + "<sub>X</sub>:");
        d->RyLabel->setText(theta + "<sub>Y</sub>:");
        d->RzLabel->setText(theta + "<sub>Z</sub>:");
        d->RxSpinBox->setToolTip("Rotation Euler angle (in degree) with respect to the X axes.");
        d->RySpinBox->setToolTip("Rotation Euler angle (in degree) with respect to the Y axes.");
        d->RzSpinBox->setToolTip("Rotation Euler angle (in degree) with respect to the Z axes.");
        d->AccuracyLabel->setText("Kernel Accuracy:");
        d->AccuracySpinBox->setSingleStep(1);
        d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());
        d->AccuracySpinBox->setMaximum(5);
        d->AccuracySpinBox->setToolTip("Set the accuracy of the Gaussian Kernel in sigma units.");
        switch (d->parametersNode->GetAccuracy())
          {
          case 1:
            {
            d->AccuracyValueLabel->setText("       68.27%");
            break;
            }
          case 2:
            {
            d->AccuracyValueLabel->setText("       95.45%");
            break;
            }
          case 3:
            {
            d->AccuracyValueLabel->setText("       99.73%");
            break;
            }
          case 4:
            {
            d->AccuracyValueLabel->setText("      99.994%");
            break;
            }
          case 5:
            {
            d->AccuracyValueLabel->setText("     99.99994%");
            break;
            }
          }

        if (fabs(d->parametersNode->GetParameterX() - d->parametersNode->GetParameterY()) < 0.001 &&
           fabs(d->parametersNode->GetParameterY() - d->parametersNode->GetParameterZ()) < 0.001)
          {
          d->GaussianKernelView->hide();
          d->RxLabel->hide();
          d->RxSpinBox->hide();
          d->RyLabel->hide();
          d->RySpinBox->hide();
          d->RzLabel->hide();
          d->RzSpinBox->hide();
          }
        else
          {
          d->GaussianKernelView->show();
          d->RxLabel->show();
          d->RxSpinBox->show();
          d->RyLabel->show();
          d->RySpinBox->show();
          d->RzLabel->show();
          d->RzSpinBox->show();

          double Rx = d->parametersNode->GetRx();
          double Ry = d->parametersNode->GetRy();
          double Rz = d->parametersNode->GetRz();

          d->RxSpinBox->setValue(Rx);
          d->RySpinBox->setValue(Ry);
          d->RzSpinBox->setValue(Rz);

          d->GaussianKernelView->show();
          vtkRenderer* renderer = d->GaussianKernelView->renderer();

          vtkActorCollection* col = renderer->GetActors();
          col->InitTraversal();
          for (int i = 0; i < col->GetNumberOfItems(); i++)
            {
            renderer->RemoveActor(col->GetNextActor());
            }

          //Definition and transformation of the vtkEllipsoid
          double RadiusX = d->parametersNode->GetParameterX() / SigmatoFWHM;
          if(RadiusX < 0.01)
            {
            RadiusX = 0.01;
            }
          RadiusX *= d->parametersNode->GetAccuracy();
          double RadiusY = d->parametersNode->GetParameterY() / SigmatoFWHM;
          if(RadiusY < 0.01)
            {
            RadiusY = 0.01;
            }
          RadiusY *= d->parametersNode->GetAccuracy();
          double RadiusZ = d->parametersNode->GetParameterZ() / SigmatoFWHM;
          if(RadiusZ < 0.01)
            {
            RadiusZ = 0.01;
            }
          RadiusZ *= d->parametersNode->GetAccuracy();
          d->parametricVTKEllipsoid->SetXRadius(RadiusX);
          d->parametricVTKEllipsoid->SetYRadius(RadiusY);
          d->parametricVTKEllipsoid->SetZRadius(RadiusZ);
          d->parametricFunctionSource->SetParametricFunction(d->parametricVTKEllipsoid);
          d->parametricFunctionSource->Update();

          //Configuration of the rotation
          Rx *= d->DegToRad;
          Ry *= d->DegToRad;
          Rz *= d->DegToRad;
          d->transformationMatrix->Identity();
          double cx = cos(Rx);
          double sx = sin(Rx);
          double cy = cos(Ry);
          double sy = sin(Ry);
          double cz = cos(Rz);
          double sz = sin(Rz);
          d->transformationMatrix->SetElement(0, 0, cy * cz);
          d->transformationMatrix->SetElement(1, 0, -cy * sz);
          d->transformationMatrix->SetElement(2, 0, sy);
          d->transformationMatrix->SetElement(0, 1, cz * sx * sy + cx * sz);
          d->transformationMatrix->SetElement(1, 1, cx * cz - sx * sy * sz);
          d->transformationMatrix->SetElement(2, 1, -cy * sx);
          d->transformationMatrix->SetElement(0, 2, -cx * cz * sy + sx * sz);
          d->transformationMatrix->SetElement(1, 2, cz * sx + cx * sy * sz);
          d->transformationMatrix->SetElement(2, 2, cx * cy);

          d->matrixToLinearTransform->SetInput(d->transformationMatrix);
          d->matrixToLinearTransform->Update();

          d->mapper->SetInputConnection(d->parametricFunctionSource->GetOutputPort());
          d->actor->SetMapper(d->mapper);
          d->actor->GetProperty()->SetColor(0.0, 1.0, 1.0);
          d->actor->SetUserTransform(d->matrixToLinearTransform);
          d->actor->PickableOff();
          d->actor->DragableOff();

          renderer->AddActor(d->actor);
          renderer->SetBackground(0., 0., 0.);

          this->onMRMLCameraNodeModified();
          d->GaussianKernelView->forceRender();
          }

        // Beam info
        this->onInputVolumeModified();
        break;
        }
      case 2:
        {
        d->OldBeamInfoLabel->hide();
        d->OldBeamInfoLineEdit->hide();
        d->NewBeamInfoLabel->hide();
        d->NewBeamInfoLineEdit->hide();
        d->AccuracyLabel->show();
        d->AccuracySpinBox->show();
        d->AccuracyValueLabel->hide();
        d->HardwareLabel->show();
        d->HardwareComboBox->show();
        d->GaussianKernelView->hide();
        d->RxLabel->hide();
        d->RxSpinBox->hide();
        d->RyLabel->hide();
        d->RySpinBox->hide();
        d->RzLabel->hide();
        d->RzSpinBox->hide();
        d->CDELT1Label->hide();
        d->CDELT1LabelValue->hide();
        d->CDELT2Label->hide();
        d->CDELT2LabelValue->hide();
        d->CDELT3Label->hide();
        d->CDELT3LabelValue->hide();
        d->LinkCheckBox->setToolTip("Click to link/unlink the conductivity parameters.");
        d->KLabel->show();
        d->KSpinBox->show();
        d->SigmaYLabel->show();
        d->DoubleSpinBoxY->show();
        d->SigmaZLabel->show();
        d->DoubleSpinBoxZ->show();

        d->KSpinBox->setValue(d->parametersNode->GetK());
        d->TimeStepLabel->show();
        d->TimeStepSpinBox->show();
        d->SigmaXLabel->setText("Horizontal Conductance:");
        d->SigmaYLabel->setText("Vertical Conductance:");
        d->SigmaZLabel->setText("Depth Conductance:");
        d->DoubleSpinBoxX->setToolTip("");
        d->DoubleSpinBoxY->setToolTip("");
        d->DoubleSpinBoxZ->setToolTip("");
        d->DoubleSpinBoxX->setSingleStep(0.1);
        d->DoubleSpinBoxX->setPageStep(1);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxX->blockSignals(true);
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          d->DoubleSpinBoxX->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          }
        d->DoubleSpinBoxY->setSingleStep(0.1);
        d->DoubleSpinBoxY->setPageStep(1);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxY->blockSignals(true);
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          d->DoubleSpinBoxY->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          }
        d->DoubleSpinBoxZ->setSingleStep(0.1);
        d->DoubleSpinBoxZ->setPageStep(1);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxZ->blockSignals(true);
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          d->DoubleSpinBoxZ->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          }
        d->DoubleSpinBoxX->setMinimum(0);
        d->DoubleSpinBoxY->setMinimum(0);
        d->DoubleSpinBoxZ->setMinimum(0);
        d->DoubleSpinBoxX->setMaximum(10);
        d->DoubleSpinBoxY->setMaximum(10);
        d->DoubleSpinBoxZ->setMaximum(10);
        d->AccuracyLabel->setText("Iterations:");
        d->AccuracySpinBox->setMaximum(30);
        d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());
        d->AccuracySpinBox->setToolTip("");
        if (d->parametersNode->GetHardware())
          {
          d->AccuracySpinBox->setSingleStep(2);
          }
        else
          {
          d->AccuracySpinBox->setSingleStep(1);
          }
        d->TimeStepSpinBox->setValue(d->parametersNode->GetTimeStep());
        d->TimeStepSpinBox->setSingleStep(0.003);
        d->TimeStepSpinBox->setMaximum(0.0625);
        break;
        }
      }
    d->parametersNode->SetGaussianKernels();
    }

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
void qSlicerAstroSmoothingModuleWidget::onMRMLCameraNodeModified()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->GaussianKernelView || !d->cameraNodeOne)
    {
    return;
    }

  vtkCamera* camera = d->GaussianKernelView->activeCamera();
  if (!camera)
    {
    return;
    }
  double VectorThree[3];
  double temp, viewFactor, scaleFactor;
  d->cameraNodeOne->GetPosition(VectorThree);
  //Ry(90)
  temp = VectorThree[1];
  VectorThree[1] = -VectorThree[2];
  VectorThree[2] = temp;
  //Rz(-180)
  VectorThree[0] *= -1.;
  VectorThree[1] *= -1.;
  viewFactor = 200.;
  scaleFactor = 4.;
  while(fabs(VectorThree[0]) > viewFactor ||
        fabs(VectorThree[1]) > viewFactor ||
        fabs(VectorThree[2]) > viewFactor)
    {
    VectorThree[0] /= scaleFactor;
    VectorThree[1] /= scaleFactor;
    VectorThree[2] /= scaleFactor;
    }
  camera->SetPosition(VectorThree);
  d->cameraNodeOne->GetFocalPoint(VectorThree);
  //Ry(90)
  temp = VectorThree[1];
  VectorThree[1] = -VectorThree[2];
  VectorThree[2] = temp;
  //Rz(-180)
  VectorThree[0] *= -1;
  VectorThree[1] *= -1;
  viewFactor = 15.;
  scaleFactor = 2.;
  while(fabs(VectorThree[0]) > viewFactor ||
        fabs(VectorThree[1]) > viewFactor ||
        fabs(VectorThree[2]) > viewFactor)
    {
    VectorThree[0] /= scaleFactor;
    VectorThree[1] /= scaleFactor;
    VectorThree[2] /= scaleFactor;
    }
  camera->SetFocalPoint(VectorThree);
  d->cameraNodeOne->GetViewUp(VectorThree);
  //Ry(90)
  temp = VectorThree[1];
  VectorThree[1] = -VectorThree[2];
  VectorThree[2] = temp;
  //Rz(-180)
  VectorThree[0] *= -1;
  VectorThree[1] *= -1;
  camera->SetViewUp(VectorThree);
  d->GaussianKernelView->forceRender();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onModeChanged()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
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
    d->parametersNode->SetHardware(0);
    d->parametersNode->SetFilter(2);
    d->parametersNode->SetAccuracy(20);
    d->parametersNode->SetTimeStep(0.0325);
    d->parametersNode->SetK(1.5);
    d->parametersNode->SetParameterX(5);
    d->parametersNode->SetParameterY(5);
    d->parametersNode->SetParameterZ(5);
    }

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onMasksCommandChanged()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();

  if (d->MasksGenerateModeRadioButton->isChecked())
    {
    d->parametersNode->SetMasksCommand("Generate");
    }
  if (d->MasksSkipModeRadioButton->isChecked())
    {
    d->parametersNode->SetMasksCommand("Skip");
    }

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onCurrentFilterChanged(int index)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();

  d->parametersNode->SetAutoRun(false);

  if (index == 0)
    {
    d->parametersNode->SetKernelLengthX(5);
    d->parametersNode->SetKernelLengthY(5);
    d->parametersNode->SetKernelLengthZ(5);
    }

  if (index == 1)
    {
    d->parametersNode->SetAccuracy(3);
    d->parametersNode->SetRx(0.);
    d->parametersNode->SetRy(0.);
    d->parametersNode->SetRz(0.);
    }

  if (index == 2)
    {
    if (d->parametersNode->GetHardware())
      {
      d->parametersNode->SetAccuracy(19);
      }
    else
      {
      d->parametersNode->SetAccuracy(20);
      }
    d->parametersNode->SetTimeStep(0.0325);
    d->parametersNode->SetK(1.5);
    }

  d->parametersNode->SetParameterX(5);
  d->parametersNode->SetParameterY(5);
  d->parametersNode->SetParameterZ(5);

  d->parametersNode->SetFilter(index);

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onKChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetK(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
  }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

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
  if (segmentationNode != segmentationNodeTable)
    {
    d->SegmentsTableView->setSegmentationNode(segmentationNode);
  }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onTimeStepChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetTimeStep(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onRxChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRx(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onRyChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRy(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onRzChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRz(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onParameterXChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetParameterX(value);
  if (d->parametersNode->GetLink())
    {
    d->parametersNode->SetParameterY(value);
    d->parametersNode->SetParameterZ(value);
    if (d->parametersNode->GetFilter() == 0)
      {
      d->parametersNode->SetKernelLengthY(value);
      d->parametersNode->SetKernelLengthZ(value);
      }
    }
  if (d->parametersNode->GetFilter() == 0)
    {
    d->parametersNode->SetKernelLengthX(value);
    }
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onParameterYChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetParameterY(value);
  if (d->parametersNode->GetLink())
    {
    d->parametersNode->SetParameterX(value);
    d->parametersNode->SetParameterZ(value);
    if (d->parametersNode->GetFilter() == 0)
      {
      d->parametersNode->SetKernelLengthX(value);
      d->parametersNode->SetKernelLengthZ(value);
      }
    }
  if (d->parametersNode->GetFilter() == 0)
    {
    d->parametersNode->SetKernelLengthY(value);
    } 
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onParameterZChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetParameterZ(value);
  if (d->parametersNode->GetLink())
    {
    d->parametersNode->SetParameterX(value);
    d->parametersNode->SetParameterY(value);
    if (d->parametersNode->GetFilter() == 0)
      {
      d->parametersNode->SetKernelLengthX(value);
      d->parametersNode->SetKernelLengthY(value);
      }
    }
  if (d->parametersNode->GetFilter() == 0)
    {
    d->parametersNode->SetKernelLengthZ(value);
    }
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }

}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onAccuracyChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetAccuracy(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onApply()
{
  Q_D(const qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::onApply() : "
                   "parametersNode not found!";
    return;
    }

  vtkSlicerAstroSmoothingLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroSmoothingModuleWidget::onApply() : astroSmoothingLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->parametersNode->SetStatus(1);

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroSmoothingModuleWidget::onApply"
                  " : scene not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroSmoothingModuleWidget::onApply"
                  " : inputVolume not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  // Check Input volume
  if (n != 3)
    {
    QString message = QString("Filtering is available only"
                              " for datacube with dimensionality 3 (NAXIS = 3).");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(nullptr, tr("Failed to run the filter"), message);
    d->parametersNode->SetStatus(0);
    return;
    }

  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_Filtered_";

  switch (d->parametersNode->GetFilter())
    {
    case 0:
      {
      outSS<<"Box";
      break;
      }
    case 1:
      {
      outSS<<"Gaussian";
      break;
      }
    case 2:
      {
      outSS<<"Gradient";
      break;
      }
    }

  int serial = d->parametersNode->GetOutputSerial();
  outSS<<"_"<< IntToString(serial);
  serial++;
  d->parametersNode->SetOutputSerial(serial);

  // Create output volume
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
    else if (name.find("_Filtered_") != std::string::npos)
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
      scene->RemoveNode(outputVolume);
      }
    }

  // Create Astro Volume for the output volume
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

  // Necessary to guarantee that the renderWindow is initialized
  d->GaussianKernelView->show();
  d->GaussianKernelView->hide();

  // Run calculation
  if (logic->Apply(d->parametersNode, d->GaussianKernelView->renderWindow()))
    {
    if (!strcmp(d->parametersNode->GetMasksCommand(), "Generate"))
      {
      d->astroVolumeWidget->setComparative3DViews
          (inputVolume->GetID(), outputVolume->GetID(), true);
      d->OutputSegmentCollapsibleButton->setCollapsed(false);
      }
    else
      {
      d->astroVolumeWidget->setComparative3DViews
          (inputVolume->GetID(), outputVolume->GetID(), false);
      }
    }
  else
    {
    scene->RemoveNode(outputVolume);
    inputVolume->SetDisplayVisibility(1);
    }

  d->parametersNode->SetStatus(0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->CancelButton->hide();
  d->progressBar->hide();
  d->ApplyButton->show();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->parametersNode->SetStatus(-1);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->progressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onHardwareChanged(int index)
{
 Q_D(qSlicerAstroSmoothingModuleWidget);

 int wasModifying = d->parametersNode->StartModify();

 d->parametersNode->SetHardware(index);
 int filter = d->parametersNode->GetFilter();

 if (filter == 2)
   {
   if (index)
     {
     d->parametersNode->SetAccuracy(19);
     }
   else
     {
     d->parametersNode->SetAccuracy(20);
     }
   }
 d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onLinkChanged(bool value)
{
 Q_D(qSlicerAstroSmoothingModuleWidget);
 d->parametersNode->SetLink(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onAutoRunChanged(bool value)
{
 Q_D(qSlicerAstroSmoothingModuleWidget);
 d->parametersNode->SetAutoRun(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->ApplyButton->hide();
  d->progressBar->show();
  d->CancelButton->show();
}

//---------------------------------------------------------------------------
vtkMRMLAstroSmoothingParametersNode* qSlicerAstroSmoothingModuleWidget::
mrmlAstroSmoothingParametersNode()const
{
  Q_D(const qSlicerAstroSmoothingModuleWidget);
  return d->parametersNode;
}

