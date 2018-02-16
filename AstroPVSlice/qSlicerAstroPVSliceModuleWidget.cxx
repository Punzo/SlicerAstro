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
#include <QStringList>

// VTK includes
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkGeneralTransform.h>
#include <vtkNew.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>
#include <vtksys/SystemTools.hxx>

// SlicerQt includes
#include <qMRMLThreeDWidget.h>
#include <qMRMLThreeDView.h>
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>

// AstroPVSlice includes
#include "qSlicerAstroPVSliceModuleWidget.h"
#include "ui_qSlicerAstroPVSliceModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroPVSliceLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAnnotationRulerNode.h>
#include <vtkMRMLAnnotationPointDisplayNode.h>
#include <vtkMRMLAnnotationLineDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLAstroPVSliceParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLDoubleArrayNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#include <sys/time.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroPVSlice
class qSlicerAstroPVSliceModuleWidgetPrivate: public Ui_qSlicerAstroPVSliceModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroPVSliceModuleWidget);
protected:
  qSlicerAstroPVSliceModuleWidget* const q_ptr;
public:

  qSlicerAstroPVSliceModuleWidgetPrivate(qSlicerAstroPVSliceModuleWidget& object);
  ~qSlicerAstroPVSliceModuleWidgetPrivate();

  void init();
  void cleanPointers();

  vtkSlicerAstroPVSliceLogic* logic() const;
  vtkSmartPointer<vtkMRMLAstroPVSliceParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
};

//-----------------------------------------------------------------------------
// qSlicerAstroPVSliceModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroPVSliceModuleWidgetPrivate::qSlicerAstroPVSliceModuleWidgetPrivate(qSlicerAstroPVSliceModuleWidget& object)
  : q_ptr(&object)
{
  this->parametersNode = 0;
  this->selectionNode = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroPVSliceModuleWidgetPrivate::~qSlicerAstroPVSliceModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroPVSliceModuleWidget);

  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidgetPrivate::init : "
                   "could not find qSlicerApplication!";
    return;
    }

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroPVSliceParametersNode(vtkMRMLNode*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->MomentMapNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onMomentMapChanged(vtkMRMLNode*)));

  QObject::connect(this->RulerNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onRulerChanged(vtkMRMLNode*)));

  QObject::connect(this->RotateSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRotateRulerChanged(double)));

  QObject::connect(this->ShiftXSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onShiftXRulerChanged(double)));

  QObject::connect(this->ShiftYSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onShiftYRulerChanged(double)));

  QObject::connect(this->ParallelPushButton, SIGNAL(clicked()),
                   q, SLOT(on3DViewParallel()));

  QObject::connect(this->PerpendicularPushButton, SIGNAL(clicked()),
                   q, SLOT(on3DViewPerpendicular()));

  QObject::connect(this->CenterRightAscensionDoubleSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRulerCenterRightAscensionChanged(double)));

  QObject::connect(this->CenterDeclinationDoubleSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRulerCenterDeclinationChanged(double)));

}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroPVSliceModuleWidget);

  if (!q->mrmlScene())
    {
    return;
    }

  if (this->parametersNode)
    {
    q->mrmlScene()->RemoveNode(this->parametersNode);
    }
  this->parametersNode = 0;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroPVSliceLogic* qSlicerAstroPVSliceModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroPVSliceModuleWidget);
  return vtkSlicerAstroPVSliceLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroPVSliceModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroPVSliceModuleWidget::qSlicerAstroPVSliceModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroPVSliceModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroPVSliceModuleWidget::~qSlicerAstroPVSliceModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::enter()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  this->Superclass::enter();

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::enter : "
                   "appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::enter : "
                   "selectionNode not found!";
    return;
    }

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->onMRMLSelectionNodeModified(d->selectionNode);

  this->initializeMomentMapNode();
  this->initializeRulerNode(false, false);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }
  vtkMRMLAnnotationRulerNode *RulerNode = vtkMRMLAnnotationRulerNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(d->parametersNode->GetRulerNodeID()));
  if (RulerNode)
    {
    vtkMRMLAnnotationPointDisplayNode* PointDisplayNode =
      RulerNode->GetAnnotationPointDisplayNode();
    if (!PointDisplayNode)
      {
      RulerNode->CreateAnnotationPointDisplayNode();
      }
    PointDisplayNode->SetVisibility(1);

    vtkMRMLAnnotationLineDisplayNode* LineDisplayNode =
      RulerNode->GetAnnotationLineDisplayNode();
    if (!LineDisplayNode)
      {
      RulerNode->CreateAnnotationLineDisplayNode();
      }
    LineDisplayNode->SetVisibility(1);
    }

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (yellowSliceNode)
    {
    yellowSliceNode->SetSliceVisible(1);
    yellowSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeThin);
    }

  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (greenSliceNode)
    {
    greenSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeThin);
    }
}

//----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::exit()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  this->Superclass::exit();

  this->qvtkDisconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }
  vtkMRMLAnnotationRulerNode *RulerNode = vtkMRMLAnnotationRulerNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(d->parametersNode->GetRulerNodeID()));
  if (RulerNode)
    {
    vtkMRMLAnnotationPointDisplayNode* PointDisplayNode =
      RulerNode->GetAnnotationPointDisplayNode();
    if (!PointDisplayNode)
      {
      RulerNode->CreateAnnotationPointDisplayNode();
      }
    PointDisplayNode->SetVisibility(0);

    vtkMRMLAnnotationLineDisplayNode* LineDisplayNode =
      RulerNode->GetAnnotationLineDisplayNode();
    if (!LineDisplayNode)
      {
      RulerNode->CreateAnnotationLineDisplayNode();
      }
    LineDisplayNode->SetVisibility(0);
    }

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (yellowSliceNode)
    {
    yellowSliceNode->SetSliceVisible(0);
    yellowSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeNone);
    }

  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (greenSliceNode)
    {
    greenSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeNone);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::setMRMLScene : selectionNode not found!";
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

  this->onMRMLSelectionNodeModified(d->selectionNode);
  this->onInputVolumeChanged(scene->GetNodeByID(d->selectionNode->GetActiveVolumeID()));
  this->onMRMLAstroPVSliceParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  this->initializeParameterNode(forceNew);

  this->initializeMomentMapNode(forceNew);

  this->initializeRulerNode(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::onEndCloseEvent : "
                   "appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes(true);
  this->onMRMLAstroPVSliceParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::onEndImportEvent : "
                   "appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroPVSliceParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroPVSliceParametersNode *astroParametersNode = NULL;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroPVSliceParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroPVSliceParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroPVSliceParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroPVSliceParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroPVSliceParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//--------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::initializeMomentMapNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    return;
    }

  vtkMRMLAstroVolumeNode *MomentMapNode = NULL;

  vtkSmartPointer<vtkCollection> AstroVolumeNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLAstroVolumeNode"));

  for (int AstroVolumeIndex = 0; AstroVolumeIndex < AstroVolumeNodes->GetNumberOfItems(); AstroVolumeIndex++)
    {
    vtkMRMLAstroVolumeNode* astroVolume = vtkMRMLAstroVolumeNode::SafeDownCast
      (AstroVolumeNodes->GetItemAsObject(AstroVolumeIndex));
    if (!astroVolume)
      {
      continue;
      }
    std::string name = astroVolume->GetName();
    if (name.find("PVSliceMomentMap") != std::string::npos &&
        name.find(inputVolume->GetName()) != std::string::npos && !forceNew)
      {
      MomentMapNode = astroVolume;
      break;
      }
    }

  vtkSlicerAstroPVSliceLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::initializeMomentMapNode : "
                   "logic not found!";
    return;
    }

  if (!MomentMapNode)
    {
    logic->Calculate0thMomentMap(d->parametersNode);
    }
  else
    {
    d->parametersNode->SetMomentMapNodeID(MomentMapNode->GetID());
    }

  logic->SetMomentMapOnRedWidget(d->parametersNode);
}

void qSlicerAstroPVSliceModuleWidget::initializeRulerNode(bool forceNew /*= false*/,
                                                          bool InitRulerPositions /*= true*/)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkSlicerAstroPVSliceLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::initializeRulerNode : "
                   "logic not found!";
    return;
    }

  vtkMRMLAnnotationRulerNode *RulerNode = NULL;

  vtkSmartPointer<vtkCollection> RulerNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLAnnotationRulerNode"));

  for (int RulerIndex = 0; RulerIndex < RulerNodes->GetNumberOfItems(); RulerIndex++)
    {
    vtkMRMLAnnotationRulerNode* tempRuler = vtkMRMLAnnotationRulerNode::SafeDownCast
      (RulerNodes->GetItemAsObject(RulerIndex));
    if (!tempRuler)
      {
      continue;
      }
    std::string name = tempRuler->GetName();
    if (name.find("PVSliceRuler") != std::string::npos && !forceNew)
      {
      RulerNode = tempRuler;
      break;
      }
    }

  if (!RulerNode)
    {
    logic->CreateAndSetRuler(d->parametersNode);
    }
  else
    {
    d->parametersNode->SetRulerNodeID(RulerNode->GetID());
    if (InitRulerPositions)
      {
      logic->InitializeRuler(d->parametersNode);
      }
    }

  if (this->isEntered())
    {
    logic->InitializePV(d->parametersNode);
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    return;
    }

  int dims[3];
  inputVolume->GetImageData()->GetDimensions(dims);
  d->ShiftXSpinBox->setMinimum(-dims[0]);
  d->ShiftXSpinBox->setMaximum(dims[0]);
  d->ShiftYSpinBox->setMinimum(-dims[1]);
  d->ShiftYSpinBox->setMaximum(dims[1]);
}

//--------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::on3DViewParallel()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "inputVolume not found!";
    return;
    }

  inputVolume->SetDisplayVisibility(1);

  // Set the camera position
  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));
  if (cameraNodes->GetNumberOfItems() < 1)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "cameraNode not found!";
    return;
    }

  vtkMRMLCameraNode *cameraNode =
    vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(0));
  if (!cameraNode)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "cameraNode not found!";
    return;
    }

  int* dims = inputVolume->GetImageData()->GetDimensions();
  // In RAS the z axes is on the second index
  double Origin[3] = {0.};
  Origin[1] = dims[2] * 2 + sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
  cameraNode->SetPosition(Origin);
  double ViewUp[3] = {0.};
  ViewUp[2] = 1.;
  cameraNode->SetViewUp(ViewUp);
  double FocalPoint[3] = {0.};
  cameraNode->SetFocalPoint(FocalPoint);

  // Reset the 3D rendering boundaries
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager())
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "qSlicerApplication not found.";
    return;
    }

  qMRMLThreeDWidget* ThreeDWidget = app->layoutManager()->threeDWidget(0);
  if(!ThreeDWidget)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "ThreeDWidget not found.";
    return;
    }

  qMRMLThreeDView* ThreeDView = ThreeDWidget->threeDView();
  if(!ThreeDView || !ThreeDView->renderer())
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "ThreeDView not found.";
    return;
    }

  ThreeDView->renderer()->ResetCameraClippingRange();
  ThreeDView->renderer()->Render();
}

//--------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::on3DViewPerpendicular()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "inputVolume not found!";
    return;
    }

  inputVolume->SetDisplayVisibility(1);

  // Set the camera position
  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));
  if (cameraNodes->GetNumberOfItems() < 1)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "cameraNode not found!";
    return;
    }

  vtkMRMLCameraNode *cameraNode =
    vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(0));
  if (!cameraNode)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "cameraNode not found!";
    return;
    }

  vtkMRMLAnnotationRulerNode *RulerNode = vtkMRMLAnnotationRulerNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(d->parametersNode->GetRulerNodeID()));
  if(!RulerNode)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "RulerNode not found!";
    return;
    }

  double position1[3] = {0};
  RulerNode->GetPosition1(position1);
  double position2[3] = {0};
  RulerNode->GetPosition2(position2);
  double MiddlePoint[3] = {0};
  for (int ii = 0; ii < 3; ii++)
    {
    MiddlePoint[ii] = (position1[ii] + position2[ii]) * 0.5;
    }

  // In RAS the z axes is on the second index (Ruler and Camera are in RAS)
  double distX = (position2[0] - position1[0]);
  double distY = (position2[2] - position1[2]);
  double angle = -atan(distY / distX);
  if ((position1[0] - position2[0]) < 0.)
    {
    angle += PI;
    }
  double rulerLength = sqrt((distX * distX) + (distY * distY));
  int* dims = inputVolume->GetImageData()->GetDimensions();
  double shift = sqrt((rulerLength * rulerLength) + (dims[2] * dims[2])) * 1.5;
  double Origin[3] = {0.};
  Origin[0] = MiddlePoint[0] + shift * sin(angle);
  Origin[2] = MiddlePoint[2] + shift * cos(angle);
  cameraNode->SetPosition(Origin);
  double ViewUp[3] = {0.};
  ViewUp[1] = 1.;
  cameraNode->SetViewUp(ViewUp);
  double FocalPoint[3] = {0.};
  FocalPoint[0] = MiddlePoint[0];
  FocalPoint[2] = MiddlePoint[2];
  cameraNode->SetFocalPoint(FocalPoint);


  // Reset the 3D rendering boundaries
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager())
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "qSlicerApplication not found.";
    return;
    }

  qMRMLThreeDWidget* ThreeDWidget = app->layoutManager()->threeDWidget(0);
  if(!ThreeDWidget)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "ThreeDWidget not found.";
    return;
    }

  qMRMLThreeDView* ThreeDView = ThreeDWidget->threeDView();
  if(!ThreeDView || !ThreeDView->renderer())
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::on3DViewParallel : "
                   "ThreeDView not found.";
    return;
    }

  ThreeDView->renderer()->ResetCameraClippingRange();
  ThreeDView->renderer()->Render();
}

//--------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::setMRMLAstroPVSliceParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroPVSliceParametersNode* AstroPVSliceParaNode =
      vtkMRMLAstroPVSliceParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroPVSliceParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroPVSliceParametersNodeModified()));

  d->parametersNode = AstroPVSliceParaNode;

  this->onMRMLAstroPVSliceParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onInputVolumeChanged(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode || !d->selectionNode)
    {
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::onInputVolumeChanged : "
                   "appLogic not found!";
    return;
    }

  if (mrmlNode)
    {
    d->selectionNode->SetReferenceActiveVolumeID(mrmlNode->GetID());
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    this->initializeMomentMapNode();
    this->initializeRulerNode();
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(NULL);
    d->selectionNode->SetActiveVolumeID(NULL);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onMRMLAstroPVSliceParametersNodeModified()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  char *MomentMapNodeID = d->parametersNode->GetMomentMapNodeID();
  vtkMRMLAstroVolumeNode *MomentMapNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(MomentMapNodeID));
  if (MomentMapNode)
    {
    d->MomentMapNodeSelector->setCurrentNode(MomentMapNode);
    }

  char *RulerNodeID = d->parametersNode->GetRulerNodeID();
  vtkMRMLAnnotationRulerNode *RulerNode = vtkMRMLAnnotationRulerNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(RulerNodeID));
  if (RulerNode)
    {
    d->RulerNodeComboBox->setCurrentNode(RulerNode);
    }

  d->RotateSpinBox->setValue(d->parametersNode->GetRulerAngle());
  d->ShiftXSpinBox->setValue(d->parametersNode->GetRulerShiftX());
  d->ShiftYSpinBox->setValue(d->parametersNode->GetRulerShiftY());

  /*double IJKRulerCenter[2];
  d->parametersNode->GetRulerCenter(IJKRulerCenter);

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = inputVolumeNode->GetAstroVolumeDisplayNode();
  if (inputVolumeNode && astroDisplay)
    {
    double WCSCoordinates[3], ijk[3];
    const int *dims = inputVolumeNode->GetImageData()->GetDimensions();
    ijk[0] = IJKRulerCenter[0];
    ijk[1] = IJKRulerCenter[1];
    ijk[2] = dims[2];
    astroDisplay->GetReferenceSpace(ijk, WCSCoordinates);

    d->CenterRightAscensionDoubleSpinBox->setValue(WCSCoordinates[0]);
    d->CenterDeclinationDoubleSpinBox->setValue(WCSCoordinates[1]);
    }
  else
    {
    d->CenterRightAscensionDoubleSpinBox->setValue(0.);
    d->CenterDeclinationDoubleSpinBox->setValue(0.);
    }*/

  vtkSlicerAstroPVSliceLogic *logic = d->logic();
  if (logic)
    {
    logic->UpdateRuler(d->parametersNode);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onMRMLPVSliceRulerNodeModified()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLAnnotationRulerNode *RulerNode = vtkMRMLAnnotationRulerNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(d->parametersNode->GetRulerNodeID()));

  if (!RulerNode)
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::onMRMLPVSliceRulerNodeModified : "
                   "RulerNode not found!";
    return;
    }

  vtkMRMLAstroVolumeNode *PVMomentMap =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetMomentMapNodeID()));
  if(!PVMomentMap || !PVMomentMap->GetImageData())
    {
    qCritical() << "qSlicerAstroPVSliceModuleWidget::onMRMLPVSliceRulerNodeModified : "
                   "PVMomentMap not found!";
    return;
    }

  RulerNode->DisableModifiedEventOn();
  double* RASOrigin = PVMomentMap->GetOrigin();
  // RAS Origin z is the second axes
  double position1[3] = {0};
  RulerNode->GetPosition1(position1);
  if (fabs(position1[1] - RASOrigin[1]) > 0.01)
    {
    position1[1] = RASOrigin[1];
    }
  RulerNode->SetPosition1(position1);

  double position2[3] = {0};
  RulerNode->GetPosition2(position2);
  if (fabs(position2[1] - RASOrigin[1]) > 0.01)
    {
    position2[1] = RASOrigin[1];
    }
  RulerNode->SetPosition2(position2);
  RulerNode->DisableModifiedEventOff();

  vtkSlicerAstroPVSliceLogic *logic = d->logic();
  if (logic)
    {
    logic->UpdatePV(d->parametersNode);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onMRMLSelectionNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

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

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  d->cleanPointers();
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onMomentMapChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode || !mrmlNode)
    {
    return;
    }

  d->parametersNode->SetMomentMapNodeID(mrmlNode->GetID());

  vtkSlicerAstroPVSliceLogic *logic = d->logic();
  if (logic)
    {
    logic->SetMomentMapOnRedWidget(d->parametersNode);
    this->on3DViewParallel();
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onRotateRulerChanged(double theta)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRulerOldAngle(d->parametersNode->GetRulerAngle());
  d->parametersNode->SetRulerAngle(theta);
  d->parametersNode->EndModify(wasModifying);
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onRulerCenterRightAscensionChanged(double value)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = inputVolume->GetAstroVolumeDisplayNode();
  if (!astroDisplay)
    {
    return;
    }

  if (strcmp(astroDisplay->GetSpace(), "WCS"))
    {
    return;
    }

  double WCSCoordinates[3], ijk[3];
  const int *dims = inputVolume->GetImageData()->GetDimensions();
  ijk[0] = dims[0] * 0.5;
  ijk[1] = dims[1] * 0.5;
  ijk[2] = dims[2];

  astroDisplay->GetReferenceSpace(ijk, WCSCoordinates);

  WCSCoordinates[0] = value;

  astroDisplay->GetIJKSpace(WCSCoordinates, ijk);

  d->parametersNode->SetRulerCenterRightAscension(ijk[0]);
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onRulerCenterDeclinationChanged(double value)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = inputVolume->GetAstroVolumeDisplayNode();
  if (!astroDisplay)
    {
    return;
    }

  if (strcmp(astroDisplay->GetSpace(), "WCS"))
    {
    return;
    }

  double WCSCoordinates[3], ijk[3];
  const int *dims = inputVolume->GetImageData()->GetDimensions();
  ijk[0] = dims[0] * 0.5;
  ijk[1] = dims[1] * 0.5;
  ijk[2] = dims[2];

  astroDisplay->GetReferenceSpace(ijk, WCSCoordinates);

  WCSCoordinates[1] = value;

  astroDisplay->GetIJKSpace(WCSCoordinates, ijk);

  d->parametersNode->SetRulerCenterRightAscension(ijk[1]);
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onRulerChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  vtkMRMLAnnotationRulerNode* RulerNode = vtkMRMLAnnotationRulerNode::SafeDownCast(mrmlNode);
  if (!RulerNode || !d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetRulerNodeID(RulerNode->GetID());

  this->qvtkReconnect(RulerNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLPVSliceRulerNodeModified()));

  this->onMRMLPVSliceRulerNodeModified();
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onShiftXRulerChanged(double shiftX)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRulerOldShiftX(d->parametersNode->GetRulerShiftX());
  d->parametersNode->SetRulerShiftX(shiftX);
  d->parametersNode->EndModify(wasModifying);
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onShiftYRulerChanged(double shiftY)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRulerOldShiftY(d->parametersNode->GetRulerShiftY());
  d->parametersNode->SetRulerShiftY(shiftY);
  d->parametersNode->EndModify(wasModifying);
}

//---------------------------------------------------------------------------
vtkMRMLAstroPVSliceParametersNode* qSlicerAstroPVSliceModuleWidget::
mrmlAstroPVSliceParametersNode()const
{
  Q_D(const qSlicerAstroPVSliceModuleWidget);
  return d->parametersNode;
}
