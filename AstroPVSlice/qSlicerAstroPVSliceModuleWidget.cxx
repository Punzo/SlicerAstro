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
#include <vtkAlgorithmOutput.h>
#include <vtkCollection.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkInteractorObserver.h>
#include <vtkGeneralTransform.h>
#include <vtkNew.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkWeakPointer.h>
#include <vtksys/SystemTools.hxx>

// SlicerQt includes
#include <qMRMLSliceWidget.h>
#include <qMRMLSliceView.h>
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
#include <vtkMRMLViewNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// Qt includes
#include <QPointer>

#include <sys/time.h>

//---------------------------------------------------------------------------
class vtkPVSliceEventCallbackCommand : public vtkCallbackCommand
{
public:
  static vtkPVSliceEventCallbackCommand *New()
    {
    return new vtkPVSliceEventCallbackCommand;
    }
  /// PVSlice widget observing the event
  QPointer<qSlicerAstroPVSliceModuleWidget> PVSliceWidget;
  /// Slice widget or 3D widget
  QPointer<qMRMLWidget> ViewWidget;
};

//-----------------------------------------------------------------------------
struct PVSliceEventObservation
{
  vtkSmartPointer<vtkPVSliceEventCallbackCommand> CallbackCommand;
  vtkWeakPointer<vtkObject> ObservedObject;
  QVector<int> ObservationTags;
};

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

} // end namespace

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

  /// Structure containing necessary objects for each slice and 3D view handling interactions
  QVector<PVSliceEventObservation> EventObservations;

  /// Indicates if views and layouts are observed
  /// (essentially, the widget is active).
  bool ViewsObserved;

  /// Indicates if the the center has to be updated on mouse move event.
  bool CenterInteractionActive;

  vtkSlicerAstroPVSliceLogic* logic() const;
  vtkSmartPointer<vtkMRMLAstroPVSliceParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  bool reportDimensionalityError;
};

//-----------------------------------------------------------------------------
// qSlicerAstroPVSliceModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroPVSliceModuleWidgetPrivate::qSlicerAstroPVSliceModuleWidgetPrivate(qSlicerAstroPVSliceModuleWidget& object)
  : q_ptr(&object)
{
  this->parametersNode = 0;
  this->selectionNode = 0;
  this->ViewsObserved = false;
  this->CenterInteractionActive = false;
  this->reportDimensionalityError = false;
}

//-----------------------------------------------------------------------------
qSlicerAstroPVSliceModuleWidgetPrivate::~qSlicerAstroPVSliceModuleWidgetPrivate()
{
  Q_Q(qSlicerAstroPVSliceModuleWidget);
  q->removeViewObservations();
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

  QObject::connect(this->CenterRightAscensionIJKSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRulerCenterRightAscensionIJKChanged(double)));

  QObject::connect(this->CenterDeclinationIJKSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRulerCenterDeclinationIJKChanged(double)));

  QObject::connect(this->CenterRightAscensionWCSSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRulerCenterRightAscensionWCSChanged(double)));

  QObject::connect(this->CenterDeclinationWCSSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRulerCenterDeclinationWCSChanged(double)));

  QObject::connect(this->SetRulerCenterPushButton, SIGNAL(clicked()),
                   q, SLOT(onSetRulerCenterClicked()));

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

  this->setupViewObservations();

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
      PointDisplayNode = RulerNode->GetAnnotationPointDisplayNode();
      }
    PointDisplayNode->SetVisibility(1);

    vtkMRMLAnnotationLineDisplayNode* LineDisplayNode =
      RulerNode->GetAnnotationLineDisplayNode();
    if (!LineDisplayNode)
      {
      RulerNode->CreateAnnotationLineDisplayNode();
      LineDisplayNode = RulerNode->GetAnnotationLineDisplayNode();
      }
    LineDisplayNode->SetVisibility(1);
    }

  vtkMRMLSliceNode *redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (redSliceNode)
    {
    redSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeThin);
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

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager() || !app->layoutManager()->layoutLogic()
     || !app->layoutManager()->layoutLogic()->GetLayoutNode())
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::enter : "
                   "qSlicerApplication not found.";
    return;
    }

  int viewArra = app->layoutManager()->layoutLogic()->GetLayoutNode()->GetViewArrangement();
  if (viewArra != vtkMRMLLayoutNode::SlicerLayoutFourUpView)
    {
    app->layoutManager()->layoutLogic()->GetLayoutNode()->
      SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutFourUpView);
    }
}

//----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::exit()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  this->Superclass::exit();

  this->removeViewObservations();

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
      PointDisplayNode = RulerNode->GetAnnotationPointDisplayNode();
      }
    PointDisplayNode->SetVisibility(0);

    vtkMRMLAnnotationLineDisplayNode* LineDisplayNode =
      RulerNode->GetAnnotationLineDisplayNode();
    if (!LineDisplayNode)
      {
      RulerNode->CreateAnnotationLineDisplayNode();
      LineDisplayNode = RulerNode->GetAnnotationLineDisplayNode();
      }
    LineDisplayNode->SetVisibility(0);
    }

  vtkMRMLSliceNode *redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (redSliceNode)
    {
    redSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeNone);
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

  // Make connections that depend on the Slicer application
  QObject::connect(qSlicerApplication::application()->layoutManager(), SIGNAL(layoutChanged(int)),
                   this, SLOT(onLayoutChanged(int)) );

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
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
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

  std::string type = inputVolume->GetAttribute("SlicerAstro.DATAMODEL");
  if (type.find("DATA") == std::string::npos &&
      type.find("MODEL") == std::string::npos)
    {
    return;
    }

  int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  if (n != 3)
    {
    if (d->reportDimensionalityError)
      {
      QString message = QString("It is possible to create PVSlice only"
                                " for datacube with dimensionality 3 (NAXIS = 3).");
      qCritical() << Q_FUNC_INFO << ": " << message;
      QMessageBox::warning(NULL, tr("Failed to create PVSlice"), message);
      d->reportDimensionalityError = false;
      }
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

  this->qvtkReconnect(d->parametersNode, AstroPVSliceParaNode,
                      vtkMRMLAstroPVSliceParametersNode::RulerCenterModifiedEvent,
                      this, SLOT(onMRMLAstroPVSliceCenterModified()));

  d->parametersNode = AstroPVSliceParaNode;

  this->onMRMLAstroPVSliceParametersNodeModified();
  this->onMRMLAstroPVSliceCenterModified();
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
    d->reportDimensionalityError = true;
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
void qSlicerAstroPVSliceModuleWidget::onLayoutChanged(int layoutIndex)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  Q_UNUSED(layoutIndex);

  if (d->ViewsObserved)
    {
    // Refresh view observations with the new layout
    this->setupViewObservations();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onMRMLAstroPVSliceCenterModified()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkSlicerAstroPVSliceLogic *logic = d->logic();
  if (logic)
    {
    logic->UpdateRulerFromCenter(d->parametersNode);
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
  d->MomentMapNodeSelector->setCurrentNode(MomentMapNode);

  char *RulerNodeID = d->parametersNode->GetRulerNodeID();
  vtkMRMLAnnotationRulerNode *RulerNode = vtkMRMLAnnotationRulerNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(RulerNodeID));
  d->RulerNodeComboBox->setCurrentNode(RulerNode);

  d->RotateSpinBox->setValue(d->parametersNode->GetRulerAngle());
  d->ShiftXSpinBox->setValue(d->parametersNode->GetRulerShiftX());
  d->ShiftYSpinBox->setValue(d->parametersNode->GetRulerShiftY());

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

  if (!d->parametersNode || !this->mrmlScene())
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

  int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  // Check Input volume
  if (n != 3)
    {
    if (d->reportDimensionalityError)
      {
      QString message = QString("PVSlice is available only"
                                " for datacube with dimensionality 3 (NAXIS = 3).");
      qCritical() << Q_FUNC_INFO << ": " << message;
      QMessageBox::warning(NULL, tr("Failed to calculate PVSlice"), message);
      }
    d->reportDimensionalityError = false;
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

  vtkMRMLAstroVolumeNode *PVMomentMap = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMomentMapNodeID()));
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

  double RulerCenter[3];
  for (int ii = 0; ii < 3; ii++)
    {
    RulerCenter[ii] = (position1[ii] + position2[ii]) * 0.5;
    }

  vtkNew<vtkGeneralTransform> RAStoIJKTransform;
  RAStoIJKTransform->Identity();
  RAStoIJKTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
  PVMomentMap->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
  RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());

  RAStoIJKTransform->TransformPoint(RulerCenter,RulerCenter);
  int IJKRulerCenter[2];
  IJKRulerCenter[0] = RulerCenter[0];
  IJKRulerCenter[1] = RulerCenter[1];
  d->parametersNode->SetRulerCenter(IJKRulerCenter);

  d->CenterRightAscensionIJKSpinBox->blockSignals(true);
  d->CenterDeclinationIJKSpinBox->blockSignals(true);
  d->CenterRightAscensionWCSSpinBox->blockSignals(true);
  d->CenterDeclinationWCSSpinBox->blockSignals(true);

  d->CenterRightAscensionIJKSpinBox->setValue(IJKRulerCenter[0]);
  d->CenterDeclinationIJKSpinBox->setValue(IJKRulerCenter[1]);

  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = NULL;
  if (inputVolumeNode)
    {
    astroDisplay = inputVolumeNode->GetAstroVolumeDisplayNode();
    }
  if (inputVolumeNode && astroDisplay)
    {
    double WCSCoordinates[3], ijk[3];
    const int *dims = inputVolumeNode->GetImageData()->GetDimensions();
    ijk[0] = IJKRulerCenter[0];
    ijk[1] = IJKRulerCenter[1];
    ijk[2] = dims[2];
    astroDisplay->GetReferenceSpace(ijk, WCSCoordinates);

    d->CenterRightAscensionWCSSpinBox->setValue(WCSCoordinates[0]);
    d->CenterDeclinationWCSSpinBox->setValue(WCSCoordinates[1]);
    }
  else
    {
    d->CenterRightAscensionWCSSpinBox->setValue(0.);
    d->CenterDeclinationWCSSpinBox->setValue(0.);
    }

  d->CenterRightAscensionIJKSpinBox->blockSignals(false);
  d->CenterDeclinationIJKSpinBox->blockSignals(false);
  d->CenterRightAscensionWCSSpinBox->blockSignals(false);
  d->CenterDeclinationWCSSpinBox->blockSignals(false);

  d->CenterRightAscensionIJKSpinBox->setEnabled(true);
  d->CenterDeclinationIJKSpinBox->setEnabled(true);
  d->CenterRightAscensionWCSSpinBox->setEnabled(true);
  d->CenterDeclinationWCSSpinBox->setEnabled(true);

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
void qSlicerAstroPVSliceModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  d->cleanPointers();
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onMomentMapChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetMomentMapNodeID(mrmlNode->GetID());

    vtkSlicerAstroPVSliceLogic *logic = d->logic();
    if (logic)
      {
      logic->SetMomentMapOnRedWidget(d->parametersNode);
      this->on3DViewParallel();
      }
    }
  else
    {
    d->parametersNode->SetMomentMapNodeID(NULL);
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
void qSlicerAstroPVSliceModuleWidget::onRulerCenterRightAscensionIJKChanged(double value)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetRulerCenterRightAscension(value);

  d->CenterRightAscensionWCSSpinBox->setEnabled(false);
  d->CenterDeclinationWCSSpinBox->setEnabled(false);
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onRulerCenterDeclinationIJKChanged(double value)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetRulerCenterDeclination(value);

  d->CenterRightAscensionWCSSpinBox->setEnabled(false);
  d->CenterDeclinationWCSSpinBox->setEnabled(false);
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onRulerCenterRightAscensionWCSChanged(double value)
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

  d->CenterRightAscensionIJKSpinBox->setEnabled(false);
  d->CenterDeclinationIJKSpinBox->setEnabled(false);
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onRulerCenterDeclinationWCSChanged(double value)
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

  d->parametersNode->SetRulerCenterDeclination(ijk[1]);

  d->CenterRightAscensionIJKSpinBox->setEnabled(false);
  d->CenterDeclinationIJKSpinBox->setEnabled(false);
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onRulerChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLAnnotationRulerNode* RulerNode = vtkMRMLAnnotationRulerNode::SafeDownCast(mrmlNode);
    if (RulerNode)
    {
    d->parametersNode->SetRulerNodeID(RulerNode->GetID());

    this->qvtkReconnect(RulerNode, vtkCommand::ModifiedEvent,
                        this, SLOT(onMRMLPVSliceRulerNodeModified()));

    this->onMRMLPVSliceRulerNodeModified();
    }
  else
    {
    d->parametersNode->SetRulerNodeID(NULL);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::onSetRulerCenterClicked()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->CenterRightAscensionIJKSpinBox->setEnabled(true);
  d->CenterDeclinationIJKSpinBox->setEnabled(true);
  d->CenterRightAscensionWCSSpinBox->setEnabled(true);
  d->CenterDeclinationWCSSpinBox->setEnabled(true);
  d->parametersNode->InvokeCustomModifiedEvent(vtkMRMLAstroPVSliceParametersNode::RulerCenterModifiedEvent);
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

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::setupViewObservations()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  // Make sure previous observations are cleared before setting up the new ones
  this->removeViewObservations();

  // Set up interactor observations
  qSlicerLayoutManager* layoutManager = qSlicerApplication::application()->layoutManager();
  if (!layoutManager)
    {
    // application is closing
    return;
    }

  // Slice views
  foreach (QString sliceViewName, layoutManager->sliceViewNames())
    {
    // Create command for slice view
    qMRMLSliceWidget* sliceWidget = layoutManager->sliceWidget(sliceViewName);
    qMRMLSliceView* sliceView = sliceWidget->sliceView();
    vtkNew<vtkPVSliceEventCallbackCommand> interactionCallbackCommand;
    interactionCallbackCommand->PVSliceWidget = this;
    interactionCallbackCommand->ViewWidget = sliceWidget;
    interactionCallbackCommand->SetClientData( reinterpret_cast<void*>(interactionCallbackCommand.GetPointer()) );
    interactionCallbackCommand->SetCallback( qSlicerAstroPVSliceModuleWidget::processEvents );

    // Connect interactor events
    vtkRenderWindowInteractor* interactor = sliceView->interactorStyle()->GetInteractor();
    PVSliceEventObservation interactorObservation;
    interactorObservation.CallbackCommand = interactionCallbackCommand.GetPointer();
    interactorObservation.ObservedObject = interactor;
    interactorObservation.ObservationTags << interactor->AddObserver(vtkCommand::KeyPressEvent, interactorObservation.CallbackCommand, 1.0);
    interactorObservation.ObservationTags << interactor->AddObserver(vtkCommand::MouseMoveEvent, interactorObservation.CallbackCommand, 1.0);
    d->EventObservations << interactorObservation;
    }

  d->ViewsObserved = true;
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::removeViewObservations()
{
  Q_D(qSlicerAstroPVSliceModuleWidget);
  foreach (PVSliceEventObservation eventObservation, d->EventObservations)
    {
    if (eventObservation.ObservedObject)
      {
      foreach (int observationTag, eventObservation.ObservationTags)
        {
        eventObservation.ObservedObject->RemoveObserver(observationTag);
        }
      }
    }
  d->EventObservations.clear();
  d->ViewsObserved = false;
}

//---------------------------------------------------------------------------
bool qSlicerAstroPVSliceModuleWidget::processInteractionEvents(
  vtkRenderWindowInteractor *callerInteractor,
  unsigned long eid,
  qMRMLWidget *viewWidget)
{
  Q_D(qSlicerAstroPVSliceModuleWidget);

  if (!d->parametersNode)
    {
    return false;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    return false;
    }

  // This effect only supports interactions in the 2D slice view (Red) currently
  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  qSlicerApplication* app = qSlicerApplication::application();
  if (app && app->layoutManager())
    {
    qMRMLSliceWidget *RedSliceWidget = app->layoutManager()->sliceWidget("Red");
    if (sliceWidget != RedSliceWidget)
      {
      return false;
      }
    }

  int eventPosition[2] = { 0, 0 };
  callerInteractor->GetEventPosition(eventPosition);

  if (eid == vtkCommand::KeyPressEvent)
    {
    const char* key = callerInteractor->GetKeySym();

    if (strcmp(key, "c"))
      {
      return false;
      }

    if (d->CenterInteractionActive)
      {
      d->CenterInteractionActive = false;
      }
    else
      {
      d->CenterInteractionActive = true;
      }
    }
  else if (eid == vtkCommand::MouseMoveEvent)
    {
    if (!d->CenterInteractionActive)
      {
      return false;
      }

    double CenterPositionRAS[3] = {0.}, CenterPositionIJK[3] = {0.};
    if (sliceWidget)
      {
      double eventPositionXY[4] = {
        static_cast<double>(eventPosition[0]),
        static_cast<double>(eventPosition[1]),
        0.0,
        1.0};
      double CenterPosition[4] = {0.};
      sliceWidget->sliceLogic()->GetSliceNode()->GetXYToRAS()->MultiplyPoint(eventPositionXY, CenterPosition);
      for (int ii = 0; ii < 3; ii++)
        {
        CenterPositionRAS[ii] = CenterPosition[ii];
        }
      }

    vtkNew<vtkGeneralTransform> RAStoIJKTransform;
    RAStoIJKTransform->Identity();
    RAStoIJKTransform->PostMultiply();
    vtkNew<vtkMatrix4x4> RAStoIJKMatrix;
    inputVolume->GetRASToIJKMatrix(RAStoIJKMatrix.GetPointer());
    RAStoIJKTransform->Concatenate(RAStoIJKMatrix.GetPointer());
    RAStoIJKTransform->TransformPoint(CenterPositionRAS,CenterPositionIJK);

    d->parametersNode->SetRulerCenterRightAscension(CenterPositionIJK[0]);
    d->parametersNode->SetRulerCenterDeclination(CenterPositionIJK[1]);
    d->parametersNode->InvokeCustomModifiedEvent(vtkMRMLAstroPVSliceParametersNode::RulerCenterModifiedEvent);
    }
  return true;
}

//---------------------------------------------------------------------------
vtkRenderWindow *qSlicerAstroPVSliceModuleWidget::renderWindow(qMRMLWidget *viewWidget)
{
  if (!viewWidget)
    {
    return NULL;
    }

  qMRMLSliceWidget* sliceWidget = qobject_cast<qMRMLSliceWidget*>(viewWidget);
  qMRMLThreeDWidget* threeDWidget = qobject_cast<qMRMLThreeDWidget*>(viewWidget);
  if (sliceWidget)
    {
    if (!sliceWidget->sliceView())
      {
      // probably the application is closing
      return NULL;
      }
    return sliceWidget->sliceView()->renderWindow();
    }
  else if (threeDWidget)
    {
    if (!threeDWidget->threeDView())
      {
      // probably the application is closing
      return NULL;
      }
      return threeDWidget->threeDView()->renderWindow();
    }

  qCritical() << Q_FUNC_INFO << ": Unsupported view widget type!";
  return NULL;
}

//---------------------------------------------------------------------------
vtkRenderer *qSlicerAstroPVSliceModuleWidget::renderer(qMRMLWidget *viewWidget)
{
  vtkRenderWindow* renderWindow = qSlicerAstroPVSliceModuleWidget::renderWindow(viewWidget);
  if (!renderWindow)
    {
    return NULL;
    }

  return vtkRenderer::SafeDownCast(renderWindow->GetRenderers()->GetItemAsObject(0));
}

//---------------------------------------------------------------------------
void qSlicerAstroPVSliceModuleWidget::processEvents(vtkObject *caller, unsigned long eid,
                                                    void *clientData, void* vtkNotUsed(callData))
{
  // Get and parse client data
  vtkPVSliceEventCallbackCommand* callbackCommand = reinterpret_cast<vtkPVSliceEventCallbackCommand*>(clientData);
  qSlicerAstroPVSliceModuleWidget* self = callbackCommand->PVSliceWidget.data();
  qMRMLWidget* viewWidget = callbackCommand->ViewWidget.data();
  if (!self || !viewWidget)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid event data";
    return;
    }
  // Do nothing if scene is closing
  if (!self->mrmlScene() || self->mrmlScene()->IsClosing())
    {
    return;
    }

  // Call processing function of active effect. Handle both interactor and view node events
  vtkRenderWindowInteractor* callerInteractor = vtkRenderWindowInteractor::SafeDownCast(caller);
  if (callerInteractor)
    {
    bool abortEvent = self->processInteractionEvents(callerInteractor, eid, viewWidget);
    if (abortEvent)
      {
      /// Set the AbortFlag on the vtkCommand associated with the event.
      /// It causes other observers of the interactor not to receive the events.
      callbackCommand->SetAbortFlag(1);
      }
    }
  else
    {
    qCritical() << Q_FUNC_INFO << ": Unsupported caller object";
    }
}
