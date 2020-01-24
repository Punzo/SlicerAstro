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
#include <qSlicerMarkupsPlaceWidget.h>
#include <qSlicerUtils.h>

// AstroPVDiagram includes
#include "qSlicerAstroPVDiagramModuleWidget.h"
#include "ui_qSlicerAstroPVDiagramModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroPVDiagramLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLAstroPVDiagramParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLDoubleArrayNode.h>
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLMarkupsDisplayNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLModelDisplayNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#include <sys/time.h>

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

} // end namespace

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroPVDiagram
class qSlicerAstroPVDiagramModuleWidgetPrivate: public Ui_qSlicerAstroPVDiagramModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroPVDiagramModuleWidget);
protected:
  qSlicerAstroPVDiagramModuleWidget* const q_ptr;
public:

  qSlicerAstroPVDiagramModuleWidgetPrivate(qSlicerAstroPVDiagramModuleWidget& object);
  ~qSlicerAstroPVDiagramModuleWidgetPrivate();

  void init();
  void cleanPointers();

  vtkSlicerAstroPVDiagramLogic* logic() const;
  vtkSmartPointer<vtkMRMLAstroPVDiagramParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  bool reportDimensionalityError;
};

//-----------------------------------------------------------------------------
// qSlicerAstroPVDiagramModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroPVDiagramModuleWidgetPrivate::qSlicerAstroPVDiagramModuleWidgetPrivate(qSlicerAstroPVDiagramModuleWidget& object)
  : q_ptr(&object)
{
  this->parametersNode = nullptr;
  this->selectionNode = nullptr;
  this->reportDimensionalityError = false;
}

//-----------------------------------------------------------------------------
qSlicerAstroPVDiagramModuleWidgetPrivate::~qSlicerAstroPVDiagramModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroPVDiagramModuleWidget);
  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();
  if(!app)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidgetPrivate::init : "
                   "could not find qSlicerApplication!";
    return;
    }

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroPVDiagramParametersNode(vtkMRMLNode*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->MomentMapNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onMomentMapChanged(vtkMRMLNode*)));

  QObject::connect(this->OutputNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->SourcePointsNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onFiducialsMarkupsChanged(vtkMRMLNode*)));

  QObject::connect(this->CurveModelNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onModelChanged(vtkMRMLNode*)));

  QObject::connect(this->InterpolationSplineRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onInterpolationSpline(bool)));

  QObject::connect(this->InterpolationNoneRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onInterpolationNone(bool)));

  QObject::connect(this->AutoUpdateCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onAutoUpdateToggled(bool)));

  QObject::connect(this->GeneratePVPushButton, SIGNAL(clicked()),
                   q, SLOT(generatePVDiagram()));

  ctkColorPickerButton* ColorButton =
    this->PointsMarkupsPlaceWidget->findChild<ctkColorPickerButton*>
      (QString("ColorButton"));
  if (ColorButton)
    {
    QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(ColorButton->sizePolicy().hasHeightForWidth());
    ColorButton->setSizePolicy(sizePolicy);
    ColorButton->setMinimumSize(QSize(0, 30));
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroPVDiagramModuleWidget);

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
vtkSlicerAstroPVDiagramLogic* qSlicerAstroPVDiagramModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroPVDiagramModuleWidget);
  return vtkSlicerAstroPVDiagramLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroPVDiagramModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroPVDiagramModuleWidget::qSlicerAstroPVDiagramModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroPVDiagramModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroPVDiagramModuleWidget::~qSlicerAstroPVDiagramModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::enter()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  this->Superclass::enter();

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager() || !app->layoutManager()->layoutLogic()
     || !app->layoutManager()->layoutLogic()->GetLayoutNode())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "qSlicerApplication not found.";
    return;
    }

  app->layoutManager()->layoutLogic()->GetLayoutNode()->SetViewArrangement
          (vtkMRMLLayoutNode::SlicerLayoutFourUpView);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::enter : "
                   "appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::enter : "
                   "selectionNode not found!";
    return;
    }

  vtkMRMLInteractionNode *interactionNode = appLogic->GetInteractionNode();
  if (!interactionNode)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::enter : "
                   "interactionNode not found.";
    return;
    }

  d->selectionNode->SetReferenceActivePlaceNodeClassName("vtkMRMLMarkupsFiducialNode");
  interactionNode->SetCurrentInteractionMode(vtkMRMLInteractionNode::Place);
  interactionNode->SetPlaceModePersistence(1);

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->onMRMLSelectionNodeModified(d->selectionNode);

  this->initializeNodes();

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLMarkupsFiducialNode *fiducialsMarkupsNode = vtkMRMLMarkupsFiducialNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(d->parametersNode->GetFiducialsMarkupsID()));
  if (fiducialsMarkupsNode)
    {
    vtkMRMLMarkupsDisplayNode* PointsDisplayNode =
      fiducialsMarkupsNode->GetMarkupsDisplayNode();
    if (!PointsDisplayNode)
      {
      fiducialsMarkupsNode->CreateDefaultDisplayNodes();
      PointsDisplayNode = fiducialsMarkupsNode->GetMarkupsDisplayNode();
      }
    PointsDisplayNode->SetVisibility(1);
    }

  vtkMRMLModelNode *ModelNode = vtkMRMLModelNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(d->parametersNode->GetModelID()));
  if (ModelNode)
    {
    vtkMRMLModelDisplayNode* LineModelDisplayNode =
      ModelNode->GetModelDisplayNode();
    if (!LineModelDisplayNode)
      {
      ModelNode->CreateDefaultDisplayNodes();
      LineModelDisplayNode = ModelNode->GetModelDisplayNode();
      }
    LineModelDisplayNode->SetVisibility(1);
    }

  vtkMRMLSliceCompositeNode *redSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
  if (redSliceComposite)
    {
    redSliceComposite->SetLabelVolumeID("");
    redSliceComposite->SetForegroundVolumeID("");
    redSliceComposite->SetForegroundOpacity(0.);
    redSliceComposite->SetBackgroundVolumeID(d->parametersNode->GetMomentMapNodeID());
    }

  vtkMRMLSliceNode *redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (redSliceNode)
    {
    redSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeThin);
    }

  vtkMRMLSliceCompositeNode *yellowSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
  if (yellowSliceComposite)
    {
    yellowSliceComposite->SetLabelVolumeID("");
    yellowSliceComposite->SetForegroundVolumeID("");
    yellowSliceComposite->SetForegroundOpacity(0.);
    yellowSliceComposite->SetBackgroundVolumeID(d->parametersNode->GetOutputVolumeNodeID());
    }

  vtkMRMLSliceCompositeNode *greenSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
    this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
  if (greenSliceComposite)
    {
    greenSliceComposite->SetLabelVolumeID("");
    greenSliceComposite->SetForegroundVolumeID("");
    greenSliceComposite->SetForegroundOpacity(0.);
    greenSliceComposite->SetBackgroundVolumeID("");
    }

  vtkMRMLAstroVolumeNode *PVDiagramVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));
 if (!PVDiagramVolume)
   {
   return;
   }

  const int *dims = PVDiagramVolume->GetImageData()->GetDimensions();

  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::enter :"
                  " yellowSliceNode not found.";
    return;
    }

  yellowSliceNode->SetOrientation("PVDiagram");
  yellowSliceNode->SetSliceOffset(0.);

  vtkMRMLSliceLogic* yellowSliceLogic = appLogic->GetSliceLogic(yellowSliceNode);
  if (yellowSliceLogic)
    {
    int *dims = yellowSliceNode->GetDimensions();
    if (dims)
      {
      yellowSliceLogic->FitSliceToAll(dims[0], dims[1]);
      }
    yellowSliceLogic->SnapSliceOffsetToIJK();
    }

  double FieldOfView[3];
  yellowSliceNode->GetFieldOfView(FieldOfView);
  yellowSliceNode->SetFieldOfView(dims[0], dims[1], FieldOfView[2]);
}

//----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::exit()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  this->Superclass::exit();

  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::exit : "
                   "selectionNode not found!";
    return;
    }

  this->qvtkDisconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::exit : "
                   "appLogic not found!";
    return;
    }

  vtkMRMLInteractionNode *interactionNode = appLogic->GetInteractionNode();
  if (!interactionNode)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::exit : "
                   "interactionNode not found.";
    return;
    }

  d->selectionNode->SetReferenceActivePlaceNodeClassName("vtkMRMLMarkupsFiducialNode");
  interactionNode->SetCurrentInteractionMode(vtkMRMLInteractionNode::ViewTransform);
  interactionNode->SetPlaceModePersistence(0);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLSliceNode *redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (redSliceNode)
    {
    redSliceNode->SetRulerType(vtkMRMLSliceNode::RulerTypeNone);
    }

  vtkMRMLMarkupsFiducialNode *fiducialsMarkupsNode = vtkMRMLMarkupsFiducialNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(d->parametersNode->GetFiducialsMarkupsID()));
  if (fiducialsMarkupsNode)
    {
    vtkMRMLMarkupsDisplayNode* PointsDisplayNode =
      fiducialsMarkupsNode->GetMarkupsDisplayNode();
    if (!PointsDisplayNode)
      {
      fiducialsMarkupsNode->CreateDefaultDisplayNodes();
      PointsDisplayNode = fiducialsMarkupsNode->GetMarkupsDisplayNode();
      }
    PointsDisplayNode->SetVisibility(0);
    }

  vtkMRMLModelNode *ModelNode = vtkMRMLModelNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(d->parametersNode->GetModelID()));
  if (ModelNode)
    {
    vtkMRMLModelDisplayNode* LineModelDisplayNode =
      ModelNode->GetModelDisplayNode();
    if (!LineModelDisplayNode)
      {
      ModelNode->CreateDefaultDisplayNodes();
      LineModelDisplayNode = ModelNode->GetModelDisplayNode();
      }
    LineModelDisplayNode->SetVisibility(0);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::setMRMLScene : "
                   "appLogic not found.";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::setMRMLScene : "
                   "selectionNode not found.";
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
void qSlicerAstroPVDiagramModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  this->initializeParameterNode(forceNew);

  this->initializeMomentMapNode(forceNew);

  this->initializeFiducialsMarkupsNode(forceNew);

  this->initializeLineModelNode(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::onEndCloseEvent : "
                   "appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
    return;
    }

  vtkMRMLInteractionNode *interactionNode = appLogic->GetInteractionNode();
  if (!interactionNode)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::enter : "
                   "interactionNode not found.";
    return;
    }

  d->selectionNode->SetReferenceActivePlaceNodeClassName("vtkMRMLMarkupsFiducialNode");
  interactionNode->SetPlaceModePersistence(1);

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->onMRMLSelectionNodeModified(d->selectionNode);

  this->initializeNodes(true);
  this->onMRMLAstroPVDiagramParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::onEndImportEvent : "
                   "appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroPVDiagramParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onFiducialsMarkupsChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLMarkupsFiducialNode* SourcePointsNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(mrmlNode);

  if (SourcePointsNode)
    {
    d->parametersNode->SetFiducialsMarkupsID(SourcePointsNode->GetID());

    this->qvtkReconnect(SourcePointsNode, vtkMRMLMarkupsNode::PointModifiedEvent,
                        this, SLOT(onMRMLSourcePointsNodeModified()));

    this->qvtkReconnect(SourcePointsNode, vtkMRMLMarkupsNode::PointRemovedEvent,
                        this, SLOT(onMRMLSourcePointsNodeModified()));

    this->qvtkReconnect(SourcePointsNode, vtkMRMLMarkupsNode::PointAddedEvent,
                        this, SLOT(onMRMLSourcePointsNodeMarkupAdded()));

    this->onMRMLSourcePointsNodeModified();
    this->onMRMLSourcePointsNodeMarkupAdded();
    }
  else
    {
    d->parametersNode->SetFiducialsMarkupsID(nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroPVDiagramParametersNode *astroParametersNode = nullptr;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroPVDiagramParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroPVDiagramParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroPVDiagramParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroPVDiagramParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroPVDiagramParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//--------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::initializeMomentMapNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

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

  // Check Input volume
  int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  if (n != 3)
    {
    if (d->reportDimensionalityError)
      {
      QString message = QString("It is possible to create PVDiagram only"
                                " for datacubes with dimensionality 3 (NAXIS = 3).");
      qCritical() << Q_FUNC_INFO << ": " << message;
      QMessageBox::warning(nullptr, tr("Failed to create the PVDiagram"), message);
      d->reportDimensionalityError = false;
      }
    return;
    }

  vtkMRMLAstroVolumeNode *MomentMapNode = nullptr;

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
    if (name.find("PVDiagramMomentMap") != std::string::npos &&
        name.find(inputVolume->GetName()) != std::string::npos)
      {
      if (forceNew)
        {
        this->mrmlScene()->RemoveNode(astroVolume);
        }
      else
        {
        MomentMapNode = astroVolume;
        break;
        }
      }
    }

  vtkSlicerAstroPVDiagramLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::initializeMomentMapNode : "
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

//--------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::initializeFiducialsMarkupsNode(bool forceNew)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkSmartPointer<vtkMRMLMarkupsFiducialNode> SourcePointNode = nullptr;

  vtkSmartPointer<vtkCollection> FiducialsMarkupsNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLMarkupsFiducialNode"));

  for (int FiducialsMarkupsIndex = 0; FiducialsMarkupsIndex < FiducialsMarkupsNodes->GetNumberOfItems(); FiducialsMarkupsIndex++)
    {
    vtkMRMLMarkupsFiducialNode* FiducialsMarkupsNode = vtkMRMLMarkupsFiducialNode::SafeDownCast
      (FiducialsMarkupsNodes->GetItemAsObject(FiducialsMarkupsIndex));
    if (!FiducialsMarkupsNode)
      {
      continue;
      }
    std::string name = FiducialsMarkupsNode->GetName();
    if (name.find("PVDiagramSourcePoints") != std::string::npos)
      {
      if (forceNew)
        {
        this->mrmlScene()->RemoveNode(FiducialsMarkupsNode);
        }
      else
        {
        SourcePointNode = FiducialsMarkupsNode;
        break;
        }
      }
    }

  if (!SourcePointNode)
    {
    SourcePointNode.TakeReference(vtkMRMLMarkupsFiducialNode::SafeDownCast
      (this->mrmlScene()->CreateNodeByClass("vtkMRMLMarkupsFiducialNode")));
    SourcePointNode->SetName("PVDiagramSourcePoints");
    this->mrmlScene()->AddNode(SourcePointNode);
    }

  SourcePointNode->RemoveAllMarkups();

  vtkMRMLMarkupsDisplayNode* SourcePointDisplayNode = SourcePointNode->GetMarkupsDisplayNode();
  if (!SourcePointDisplayNode)
    {
    SourcePointNode->CreateDefaultDisplayNodes();
    SourcePointDisplayNode = SourcePointNode->GetMarkupsDisplayNode();
    }
  SourcePointDisplayNode->SetGlyphType(vtkMRMLMarkupsDisplayNode::Sphere3D);
  SourcePointDisplayNode->SetColor(0., 0.66, 1.);
  SourcePointDisplayNode->SetSelectedColor(0., 0.66, 1.);

  d->parametersNode->SetFiducialsMarkupsID(SourcePointNode->GetID());
}

//--------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::initializeLineModelNode(bool forceNew)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkSmartPointer<vtkMRMLModelNode> LineModelNode = nullptr;

  vtkSmartPointer<vtkCollection> ModelNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLModelNode"));

  for (int ModelNodesIndex = 0; ModelNodesIndex < ModelNodes->GetNumberOfItems(); ModelNodesIndex++)
    {
    vtkMRMLModelNode* ModelNode = vtkMRMLModelNode::SafeDownCast
      (ModelNodes->GetItemAsObject(ModelNodesIndex));
    if (!ModelNode)
      {
      continue;
      }
    std::string name = ModelNode->GetName();
    if (name.find("PVDiagramLineSelection") != std::string::npos)
      {
      if (forceNew)
        {
        this->mrmlScene()->RemoveNode(ModelNode);
        }
      else
        {
        LineModelNode = ModelNode;
        break;
        }
      }
    }

  if (!LineModelNode)
    {
    LineModelNode.TakeReference(vtkMRMLModelNode::SafeDownCast
      (this->mrmlScene()->CreateNodeByClass("vtkMRMLModelNode")));
    LineModelNode->SetName("PVDiagramLineSelection");
    this->mrmlScene()->AddNode(LineModelNode);
    }

  vtkMRMLModelDisplayNode* LineModelDisplayNode = LineModelNode->GetModelDisplayNode();
  if (!LineModelDisplayNode)
    {
    LineModelNode->CreateDefaultDisplayNodes();
    LineModelDisplayNode = LineModelNode->GetModelDisplayNode();
    }
  LineModelDisplayNode->SetColor(0., 0.5, 1.);
  LineModelDisplayNode->SetVisibility2D(true);
  LineModelDisplayNode->SetSliceDisplayModeToProjection();
  LineModelDisplayNode->RemoveAllViewNodeIDs();
  LineModelDisplayNode->AddViewNodeID("vtkMRMLSliceNodeRed");
  LineModelDisplayNode->AddViewNodeID("vtkMRMLViewNode1");

  d->parametersNode->SetModelID(LineModelNode->GetID());
}

//--------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::generatePVDiagram()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroPVDiagramModuleWidget::generatePVDiagram"
                  " : inputVolume not found!";
    return;
    }

  // Check Input volume
  int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  if (n != 3)
    {
    if (d->reportDimensionalityError)
      {
      QString message = QString("It is possible to create PVDiagram only"
                                " for datacubes with dimensionality 3 (NAXIS = 3).");
      qCritical() << Q_FUNC_INFO << ": " << message;
      QMessageBox::warning(nullptr, tr("Failed to create the PVDiagram"), message);
      d->reportDimensionalityError = false;
      }
    return;
    }

  vtkMRMLAstroVolumeNode *PVDiagramVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  if (!PVDiagramVolume)
    {
    std::ostringstream outSS;
    outSS << inputVolume->GetName() << "_PVDiagram";

    // create Astro Volume for the moment map
    PVDiagramVolume = vtkMRMLAstroVolumeNode::SafeDownCast
       (d->logic()->GetAstroVolumeLogic()->CloneVolumeWithoutImageData(this->mrmlScene(), inputVolume, outSS.str().c_str()));
    }

  // modify fits attributes
  PVDiagramVolume->SetAttribute("SlicerAstro.DATAMODEL", "PVDIAGRAM");
  PVDiagramVolume->SetAttribute("SlicerAstro.NAXIS", "2");
  PVDiagramVolume->GetAstroVolumeDisplayNode()->SetAttribute("SlicerAstro.NAXIS", "2");
  PVDiagramVolume->GetAstroVolumeDisplayNode()->CopyWCS(inputVolume->GetAstroVolumeDisplayNode());

  PVDiagramVolume->SetAttribute("SlicerAstro.BUNIT", inputVolume->GetAttribute("SlicerAstro.BUNIT"));
  PVDiagramVolume->SetAttribute("SlicerAstro.BTYPE", inputVolume->GetAttribute("SlicerAstro.BTYPE"));

  PVDiagramVolume->SetAttribute("SlicerAstro.NAXIS1", "1");
  PVDiagramVolume->SetAttribute("SlicerAstro.CROTA1", "0");
  PVDiagramVolume->SetAttribute("SlicerAstro.CRPIX1", "0");
  PVDiagramVolume->SetAttribute("SlicerAstro.CRVAL1", "0");
  PVDiagramVolume->SetAttribute("SlicerAstro.CTYPE1", "UNDEFINED");

  PVDiagramVolume->SetAttribute("SlicerAstro.NAXIS2", inputVolume->GetAttribute("SlicerAstro.NAXIS3"));
  PVDiagramVolume->SetAttribute("SlicerAstro.CDELT2", inputVolume->GetAttribute("SlicerAstro.CDELT3"));
  PVDiagramVolume->SetAttribute("SlicerAstro.CROTA2", inputVolume->GetAttribute("SlicerAstro.CROTA3"));
  PVDiagramVolume->SetAttribute("SlicerAstro.CRPIX2", inputVolume->GetAttribute("SlicerAstro.CRPIX3"));
  PVDiagramVolume->SetAttribute("SlicerAstro.CRVAL2", inputVolume->GetAttribute("SlicerAstro.CRVAL3"));
  PVDiagramVolume->SetAttribute("SlicerAstro.CTYPE2", inputVolume->GetAttribute("SlicerAstro.CTYPE3"));
  PVDiagramVolume->SetAttribute("SlicerAstro.CUNIT2", inputVolume->GetAttribute("SlicerAstro.CUNIT3"));
  PVDiagramVolume->SetAttribute("SlicerAstro.DTYPE2", inputVolume->GetAttribute("SlicerAstro.DTYPE3"));
  PVDiagramVolume->SetAttribute("SlicerAstro.DRVAL2", inputVolume->GetAttribute("SlicerAstro.DRVAL3"));
  PVDiagramVolume->SetAttribute("SlicerAstro.DUNIT2", inputVolume->GetAttribute("SlicerAstro.DUNIT3"));

  PVDiagramVolume->RemoveAttribute("SlicerAstro.NAXIS3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.CDELT3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.CROTA3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.CRPIX3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.CRVAL3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.CTYPE3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.CUNIT3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.DTYPE3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.DRVAL3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.DUNIT3");

  PVDiagramVolume->RemoveAttribute("SlicerAstro.PC1_3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.PC2_3");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.PC3_1");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.PC3_2");
  PVDiagramVolume->RemoveAttribute("SlicerAstro.PC3_3");

  // copy 2D image into the Astro Volume object
  int N2 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3"));
  vtkNew<vtkImageData> imageDataTemp;
  imageDataTemp->SetDimensions(1, N2, 1);
  imageDataTemp->SetSpacing(1.,1.,1.);
  imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

  PVDiagramVolume->SetAndObserveImageData(imageDataTemp.GetPointer());

  // Set Origin
  double Origin[3];
  inputVolume->GetOrigin(Origin);
  Origin[1] = 0.;
  PVDiagramVolume->SetOrigin(Origin);

  // Remove old rendering Display
  int ndnodes = PVDiagramVolume->GetNumberOfDisplayNodes();
  for (int ii = 0; ii < ndnodes; ii++)
    {
    vtkMRMLVolumeRenderingDisplayNode *dnode =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
        PVDiagramVolume->GetNthDisplayNode(ii));
    if (dnode)
      {
      PVDiagramVolume->RemoveNthDisplayNodeID(ii);
      }
    }

  // Set no WCS
  PVDiagramVolume->GetAstroVolumeDisplayNode()->SetSpace("IJK");

  // Update parameter Node
  d->parametersNode->SetOutputVolumeNodeID(PVDiagramVolume->GetID());

  vtkMRMLNode* node = nullptr;
  PVDiagramVolume->SetPresetNode(node);

  vtkSlicerAstroPVDiagramLogic *logic = d->logic();
  if (logic)
    {
    logic->GenerateAndSetPVDiagram(d->parametersNode);
    }

  if (!d->parametersNode->GetAutoUpdate())
    {
    vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
    if (!appLogic)
      {
      qCritical() << "qSlicerAstroPVDiagramModuleWidget::generatePVDiagram : "
                     "appLogic not found!";
      return;
      }

    vtkMRMLInteractionNode *interactionNode = appLogic->GetInteractionNode();
    if (!interactionNode)
      {
      qCritical() << "qSlicerAstroPVDiagramModuleWidget::generatePVDiagram : "
                     "interactionNode not found.";
      return;
      }

    interactionNode->SetCurrentInteractionMode(vtkMRMLInteractionNode::ViewTransform);
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onAutoUpdateToggled(bool toggled)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetAutoUpdate(toggled);
}

//--------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::setMRMLAstroPVDiagramParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroPVDiagramParametersNode* AstroPVDiagramParaNode =
      vtkMRMLAstroPVDiagramParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroPVDiagramParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroPVDiagramParametersNodeModified()));

  this->qvtkReconnect(d->parametersNode, AstroPVDiagramParaNode,
                      vtkMRMLAstroPVDiagramParametersNode::InterpolationModifiedEvent,
                      this, SLOT(onMRMLAstroPVDiagramInterpolationModeModified()));

  d->parametersNode = AstroPVDiagramParaNode;

  this->onMRMLAstroPVDiagramParametersNodeModified();
  this->onMRMLAstroPVDiagramInterpolationModeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onInputVolumeChanged(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode || !d->selectionNode)
    {
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroPVDiagramModuleWidget::onInputVolumeChanged : "
                   "appLogic not found!";
    return;
    }

  if (mrmlNode)
    {
    d->selectionNode->SetReferenceActiveVolumeID(mrmlNode->GetID());
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    d->reportDimensionalityError = true;
    this->initializeMomentMapNode();
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(nullptr);
    d->selectionNode->SetActiveVolumeID(nullptr);
  }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onInterpolationNone(bool toggled)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (!toggled)
    {
    return;
    }

  d->parametersNode->SetInterpolation(false);

}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onInterpolationSpline(bool toggled)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (!toggled)
    {
    return;
    }

  d->parametersNode->SetInterpolation(true);
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onModelChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetModelID(mrmlNode->GetID());
    }
  else
    {
    d->parametersNode->SetModelID(nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onMRMLAstroPVDiagramParametersNodeModified()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  char *momentMapNodeID = d->parametersNode->GetMomentMapNodeID();
  vtkMRMLAstroVolumeNode *momentMapNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(momentMapNodeID));
  d->MomentMapNodeSelector->setCurrentNode(momentMapNode);

  char *outputVolumeNodeID = d->parametersNode->GetOutputVolumeNodeID();
  vtkMRMLAstroVolumeNode *outputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(outputVolumeNodeID));
  d->OutputNodeSelector->setCurrentNode(outputVolumeNode);

  char *fiducialsMarkupsNodeID = d->parametersNode->GetFiducialsMarkupsID();
  vtkMRMLMarkupsFiducialNode *fiducialsMarkupsNode = vtkMRMLMarkupsFiducialNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(fiducialsMarkupsNodeID));
  d->SourcePointsNodeComboBox->setCurrentNode(fiducialsMarkupsNode);
  d->PointsMarkupsPlaceWidget->setCurrentNode(fiducialsMarkupsNode);

  char *modelNodeID = d->parametersNode->GetModelID();
  vtkMRMLModelNode *modelNode = vtkMRMLModelNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(modelNodeID));
  d->CurveModelNodeComboBox->setCurrentNode(modelNode);

  d->AutoUpdateCheckBox->setChecked(d->parametersNode->GetAutoUpdate());
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onMRMLSelectionNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

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

  this->initializeMomentMapNode(true);
  this->initializeFiducialsMarkupsNode(false);

  // Update Line Selection
  vtkSlicerAstroPVDiagramLogic *logic = d->logic();
  if (logic)
    {
    logic->UpdateSliceSelection(d->parametersNode);
    }
  this->initializeLineModelNode(false);
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onMRMLSourcePointsNodeMarkupAdded()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLMarkupsFiducialNode *fiducialsMarkupsNode = vtkMRMLMarkupsFiducialNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(d->parametersNode->GetFiducialsMarkupsID()));
  if (!fiducialsMarkupsNode)
    {
    return;
    }

  int FiducialIndex = fiducialsMarkupsNode->GetNumberOfFiducials() - 1;
  if (FiducialIndex < 0)
    {
    return;
    }

  fiducialsMarkupsNode->SetNthFiducialLabel(FiducialIndex, IntToString(FiducialIndex + 1));
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onMRMLSourcePointsNodeModified()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLMarkupsFiducialNode *fiducialsMarkupsNode =
    vtkMRMLMarkupsFiducialNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(d->parametersNode->GetFiducialsMarkupsID()));
  if (!fiducialsMarkupsNode)
    {
    return;
    }

  vtkMRMLAstroVolumeNode *PVMomentMap =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMomentMapNodeID()));
  if(!PVMomentMap || !PVMomentMap->GetImageData())
    {
    return;
    }

  int nOfControlPoints = fiducialsMarkupsNode->GetNumberOfFiducials();
  if (nOfControlPoints < 1)
    {
    return;
    }

  // Ensure the fiducials are restrained on the Moment Map
  fiducialsMarkupsNode->DisableModifiedEventOn();
  double* RASOrigin = PVMomentMap->GetOrigin();
  int dimsIJK[3];
  double dimsIJKDouble[3], dimsRAS[3];
  PVMomentMap->GetImageData()->GetDimensions(dimsIJK);
  dimsIJKDouble[0] = dimsIJK[0];
  dimsIJKDouble[1] = dimsIJK[1];
  dimsIJKDouble[2] = dimsIJK[2];
  vtkNew<vtkGeneralTransform> IJKtoRASTransform;
  IJKtoRASTransform->Identity();
  IJKtoRASTransform->PostMultiply();
  vtkNew<vtkMatrix4x4> IJKtoRASMatrix;
  PVMomentMap->GetRASToIJKMatrix(IJKtoRASMatrix.GetPointer());
  IJKtoRASMatrix->Invert();
  IJKtoRASTransform->Concatenate(IJKtoRASMatrix.GetPointer());
  IJKtoRASTransform->TransformPoint(dimsIJKDouble, dimsRAS);

  dimsRAS[0] = fabs(dimsRAS[0]) - 2;
  dimsRAS[2] = fabs(dimsRAS[2]) - 2;

  double position[3] = {0};
  for (int fiducialIndex = 0; fiducialIndex < nOfControlPoints; fiducialIndex++)
    {
    fiducialsMarkupsNode->GetNthFiducialPosition(fiducialIndex, position);
    // RAS Origin Z is the second axes
    if (fabs(position[1] - RASOrigin[1]) > 0.01)
      {
      position[1] = RASOrigin[1];
      }
    // X
    if (position[0] < -dimsRAS[0])
      {
      position[0] = -dimsRAS[0];
      }
    if (position[0] > dimsRAS[0])
      {
      position[0] = dimsRAS[0];
      }
    // Y
    if (position[2] < -dimsRAS[2])
      {
      position[2] = -dimsRAS[2];
      }
    if (position[2] > dimsRAS[2])
      {
      position[2] = dimsRAS[2];
      }
    fiducialsMarkupsNode->SetNthFiducialPositionFromArray(fiducialIndex, position);
    }
  fiducialsMarkupsNode->DisableModifiedEventOff();

  // Update Line Selection
  vtkSlicerAstroPVDiagramLogic *logic = d->logic();
  if (logic)
    {
    logic->UpdateSliceSelection(d->parametersNode);
    }

  if (d->parametersNode->GetAutoUpdate())
    {
    this->generatePVDiagram();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  d->cleanPointers();
}

//---------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onMomentMapChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetMomentMapNodeID(mrmlNode->GetID());

    vtkSlicerAstroPVDiagramLogic *logic = d->logic();
    if (logic)
      {
      logic->SetMomentMapOnRedWidget(d->parametersNode);
      }
    }
  else
    {
    d->parametersNode->SetMomentMapNodeID(nullptr);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onMRMLAstroPVDiagramInterpolationModeModified()
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

  if (!d->parametersNode || !this->mrmlScene())
    {
    return;
    }

  if (d->parametersNode->GetInterpolation())
    {
    d->InterpolationSplineRadioButton->setChecked(true);
    d->InterpolationNoneRadioButton->setChecked(false);
    }
  else
    {
    d->InterpolationSplineRadioButton->setChecked(false);
    d->InterpolationNoneRadioButton->setChecked(true);
    }

  this->onMRMLSourcePointsNodeModified();
}

//---------------------------------------------------------------------------
void qSlicerAstroPVDiagramModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroPVDiagramModuleWidget);

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

//---------------------------------------------------------------------------
vtkMRMLAstroPVDiagramParametersNode* qSlicerAstroPVDiagramModuleWidget::
mrmlAstroPVDiagramParametersNode()const
{
  Q_D(const qSlicerAstroPVDiagramModuleWidget);
  return d->parametersNode;
}
