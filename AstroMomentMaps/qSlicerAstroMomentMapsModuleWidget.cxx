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

// CTK includes
#include <ctkFlowLayout.h>
#include <ctkDoubleSpinBox.h>
#include <ctkDoubleRangeSlider.h>

// VTK includes
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkNew.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtksys/SystemTools.hxx>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>

// AstroMomentMaps includes
#include "qSlicerAstroMomentMapsModuleWidget.h"
#include "ui_qSlicerAstroMomentMapsModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroMomentMapsLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>

// qMRML includes
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSliceWidget.h>
#include <qMRMLTableView.h>

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroMomentMapsParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeStorageNode.h>
#include <vtkMRMLDoubleArrayNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#include <sys/time.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroMomentMaps
class qSlicerAstroMomentMapsModuleWidgetPrivate: public Ui_qSlicerAstroMomentMapsModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroMomentMapsModuleWidget);
protected:
  qSlicerAstroMomentMapsModuleWidget* const q_ptr;
public:

  qSlicerAstroMomentMapsModuleWidgetPrivate(qSlicerAstroMomentMapsModuleWidget& object);
  ~qSlicerAstroMomentMapsModuleWidgetPrivate();

  void init();
  void cleanPointers();

  vtkSlicerAstroMomentMapsLogic* logic() const;
  vtkSmartPointer<vtkMRMLAstroMomentMapsParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
  vtkSmartPointer<vtkMRMLUnitNode> unitNodeIntensity;
  vtkSmartPointer<vtkMRMLUnitNode> unitNodeVelocity;
};

//-----------------------------------------------------------------------------
// qSlicerAstroMomentMapsModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModuleWidgetPrivate::qSlicerAstroMomentMapsModuleWidgetPrivate(qSlicerAstroMomentMapsModuleWidget& object)
  : q_ptr(&object)
{
  this->parametersNode = 0;
  this->selectionNode = 0;
  this->segmentEditorNode = 0;
  this->unitNodeIntensity = 0;
  this->unitNodeVelocity = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModuleWidgetPrivate::~qSlicerAstroMomentMapsModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroMomentMapsModuleWidget);

  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidgetPrivate::init(): could not find qSlicerApplication!";
    return;
    }

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroMomentMapsParametersNode(vtkMRMLNode*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->ZeroMomentVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onZeroMomentVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->FirstMomentVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onFirstMomentVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->SecondMomentVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onSecondMomentVolumeChanged(vtkMRMLNode*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SegmentsTableView, SLOT(setMRMLScene(vtkMRMLScene*)));

  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);

  QObject::connect(this->MaskCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onMaskActiveToggled(bool)));

  QObject::connect(this->ZeroMomentRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onGenerateZeroToggled(bool)));

  QObject::connect(this->FirstMomentRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onGenerateFirstToggled(bool)));

  QObject::connect(this->SecondMomentRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onGenerateSecondToggled(bool)));

  QObject::connect(this->ThresholdRangeWidget, SIGNAL(valuesChanged(double,double)),
                   q, SLOT(onThresholdRangeChanged(double, double)));

  QObject::connect(this->VelocityRangeWidget, SIGNAL(valuesChanged(double,double)),
                   q, SLOT(onVelocityRangeChanged(double, double)));

  QObject::connect(this->ApplyButton, SIGNAL(clicked()),
                   q, SLOT(onCalculate()));

  QObject::connect(this->CancelButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));

  this->InputSegmentCollapsibleButton->setCollapsed(false);

  this->progressBar->hide();
  this->progressBar->setMinimum(0);
  this->progressBar->setMaximum(100);
  this->CancelButton->hide();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroMomentMapsModuleWidget);

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
}

//-----------------------------------------------------------------------------
vtkSlicerAstroMomentMapsLogic* qSlicerAstroMomentMapsModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroMomentMapsModuleWidget);
  return vtkSlicerAstroMomentMapsLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroMomentMapsModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModuleWidget::qSlicerAstroMomentMapsModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroMomentMapsModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroMomentMapsModuleWidget::~qSlicerAstroMomentMapsModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::enter()
{
  this->Superclass::enter();
}

//----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::exit()
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
void qSlicerAstroMomentMapsModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::setMRMLScene : selectionNode not found!";
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

  d->InputSegmentCollapsibleButton->setCollapsed(false);

  d->unitNodeIntensity = d->selectionNode->GetUnitNode("intensity");
  this->qvtkReconnect(d->unitNodeIntensity, vtkCommand::ModifiedEvent,
                      this, SLOT(onUnitNodeIntensityChanged(vtkObject*)));
  this->onUnitNodeIntensityChanged(d->unitNodeIntensity);

  d->unitNodeVelocity = d->selectionNode->GetUnitNode("velocity");
  this->qvtkReconnect(d->unitNodeVelocity, vtkCommand::ModifiedEvent,
                      this, SLOT(onUnitNodeVelocityChanged(vtkObject*)));
  this->onUnitNodeVelocityChanged(d->unitNodeVelocity);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::initializeNodes(bool forceNew)
{
  this->initializeParameterNode(forceNew);

  this->initializeSegmentations(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onEndCloseEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes(true);
  this->onMRMLAstroMomentMapsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onEndImportEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroMomentMapsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroMomentMapsParametersNode *astroParametersNode = NULL;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroMomentMapsParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroMomentMapsParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroMomentMapsParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroMomentMapsParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroMomentMapsParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->SetMaskActive(false);
    astroParametersNode->SetGenerateZero(true);
    astroParametersNode->SetGenerateFirst(true);
    astroParametersNode->SetGenerateSecond(true);
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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
bool qSlicerAstroMomentMapsModuleWidget::convertSelectedSegmentToLabelMap()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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
    QString message = QString("No segmentation available!");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to select a segment"), message);
    return false;
    }

  // Export selected segments into a multi-label labelmap volume
  std::vector<std::string> segmentIDs;
  currentSegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIDs);

  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> labelMapNode;

  QStringList selectedSegmentIDs = d->SegmentsTableView->selectedSegmentIDs();

  if (selectedSegmentIDs.size() < 1)
    {
    QString message = QString("No segment selected from the segmentation node! Please provide a segment.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to select a segment"), message);
    return false;
    }

  segmentIDs.clear();
  segmentIDs.push_back(selectedSegmentIDs[0].toStdString());

  vtkMRMLAstroVolumeNode* activeVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(
     d->InputVolumeNodeSelector->currentNode());

  if (activeVolumeNode)
    {
    vtkSlicerAstroMomentMapsLogic* astroMomentMapslogic =
      vtkSlicerAstroMomentMapsLogic::SafeDownCast(this->logic());
    if (!astroMomentMapslogic)
      {
      qCritical()  << Q_FUNC_INFO << ": astroMomentMapslogic not found!";
      return false;
      }
    vtkSlicerAstroVolumeLogic* astroVolumelogic =
      vtkSlicerAstroVolumeLogic::SafeDownCast(astroMomentMapslogic->GetAstroVolumeLogic());
    if (!astroVolumelogic)
      {
      qCritical()  << Q_FUNC_INFO << ": vtkSlicerAstroVolumeLogic not found!";
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

  if (!vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode(currentSegmentationNode, segmentIDs, labelMapNode, activeVolumeNode))
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              "Be sure that segment to export has been selected in the table view (left click). \n\n").
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

//--------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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
void qSlicerAstroMomentMapsModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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
void qSlicerAstroMomentMapsModuleWidget::onSecondMomentVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetSecondMomentVolumeNodeID(mrmlNode->GetID());
    }
  else
    {
    d->parametersNode->SetSecondMomentVolumeNodeID(NULL);
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::setMRMLAstroMomentMapsParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroMomentMapsParametersNode* AstroMomentMapsParaNode =
      vtkMRMLAstroMomentMapsParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroMomentMapsParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroMomentMapsParametersNodeModified()));

  d->parametersNode = AstroMomentMapsParaNode;

  this->onMRMLAstroMomentMapsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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

    this->qvtkConnect(mrmlNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onInputVolumeModified()));

    this->onInputVolumeModified();
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(NULL);
    d->selectionNode->SetActiveVolumeID(NULL);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onInputVolumeModified()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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
  vtkMRMLAstroVolumeDisplayNode * astroMrmlDisplayNode = astroMrmlNode->GetAstroVolumeDisplayNode();
  if (!astroMrmlDisplayNode)
    {
    return;
    }

  double ijk[3], worldOne[3], worldTwo[3];
  ijk[0] = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS1")) * 0.5;
  ijk[1] = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS2")) * 0.5;
  ijk[2] = 0.;
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldOne);
  struct wcsprm* WCS = astroMrmlDisplayNode->GetWCSStruct();
  if (!WCS)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onInputVolumeModified :"
                   " WCS not found!";
    return;
    }
  if(!strcmp(WCS->cunit[2], "m/s"))
    {
    worldOne[2] /= 1000.;
    }
  ijk[2] = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.NAXIS3"));
  if (ijk[2] < 2)
    {
    ijk[2] += 1;
    }
  astroMrmlDisplayNode->GetReferenceSpace(ijk, worldTwo);
  if(!strcmp(WCS->cunit[2], "m/s"))
    {
    worldTwo[2] /= 1000.;
    }

  double Vmin, Vmax;

  if (worldOne[2] < worldTwo[2])
    {
    Vmin = worldOne[2];
    Vmax = worldTwo[2];
    }
  else
    {
    Vmin = worldTwo[2];
    Vmax = worldOne[2];
    }

  d->VelocityRangeWidget->reset();
  int wasBlocked = d->VelocityRangeWidget->blockSignals(true);
  d->VelocityRangeWidget->setRange(Vmin, Vmax);
  d->VelocityRangeWidget->setSingleStep((Vmax - Vmin) / 200.);
  d->VelocityRangeWidget->blockSignals(wasBlocked);

  double min = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.DATAMIN"));
  double max = StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.DATAMAX"));

  d->ThresholdRangeWidget->reset();
  wasBlocked = d->ThresholdRangeWidget->blockSignals(true);
  d->ThresholdRangeWidget->setRange(min, max);
  d->ThresholdRangeWidget->setSingleStep((max - min) / 200.);
  d->ThresholdRangeWidget->blockSignals(wasBlocked);

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetVelocityMin(Vmin);
  d->parametersNode->SetVelocityMax(Vmax);

  d->parametersNode->SetIntensityMin(StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.DATAMIN")));
  d->parametersNode->SetIntensityMax(StringToDouble(astroMrmlNode->GetAttribute("SlicerAstro.DATAMAX")));

  d->parametersNode->EndModify(wasModifying);

  d->VelocityUnitLabel->setText("km/s");
  d->ThresholdUnitLabel->setText(astroMrmlNode->GetAttribute("SlicerAstro.BUNIT"));
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onFirstMomentVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetFirstMomentVolumeNodeID(mrmlNode->GetID());
    }
  else
    {
    d->parametersNode->SetFirstMomentVolumeNodeID(NULL);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onGenerateFirstToggled(bool generate)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetGenerateFirst(generate);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onGenerateSecondToggled(bool generate)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetGenerateSecond(generate);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onGenerateZeroToggled(bool generate)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetGenerateZero(generate);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onMaskActiveToggled(bool active)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetMaskActive(active);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onMRMLAstroMomentMapsParametersNodeModified()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  char *zeroMomentVolumeNodeID = d->parametersNode->GetZeroMomentVolumeNodeID();
  vtkMRMLAstroVolumeNode *zeroMomentVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(zeroMomentVolumeNodeID));
  d->ZeroMomentVolumeNodeSelector->setCurrentNode(zeroMomentVolumeNode);

  char *firstMomentVolumeNodeID = d->parametersNode->GetFirstMomentVolumeNodeID();
  vtkMRMLAstroVolumeNode *firstMomentVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(firstMomentVolumeNodeID));
  d->FirstMomentVolumeNodeSelector->setCurrentNode(firstMomentVolumeNode);

  char *secondMomentVolumeNodeID = d->parametersNode->GetSecondMomentVolumeNodeID();
  vtkMRMLAstroVolumeNode *secondMomentVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(secondMomentVolumeNodeID));
  d->SecondMomentVolumeNodeSelector->setCurrentNode(secondMomentVolumeNode);

  d->MaskCheckBox->setChecked(d->parametersNode->GetMaskActive());
  d->SegmentsTableView->setEnabled(d->parametersNode->GetMaskActive());
  if (d->parametersNode->GetMaskActive())
    {  
    d->VelocityRangeLabel->hide();
    d->VelocityRangeWidget->hide();
    d->VelocityUnitLabel->hide();
    d->ThresholdRangeLabel->hide();
    d->ThresholdRangeWidget->hide();
    d->ThresholdUnitLabel->hide();
    }
  else
    {
    d->VelocityRangeLabel->show();
    d->VelocityRangeWidget->show();
    d->VelocityUnitLabel->show();
    d->ThresholdRangeLabel->show();
    d->ThresholdRangeWidget->show();
    d->ThresholdUnitLabel->show();
    }
  d->ZeroMomentRadioButton->setChecked(d->parametersNode->GetGenerateZero());
  d->FirstMomentRadioButton->setChecked(d->parametersNode->GetGenerateFirst());
  d->SecondMomentRadioButton->setChecked(d->parametersNode->GetGenerateSecond());

  bool wasBlocked = d->VelocityRangeWidget->blockSignals(true);
  d->VelocityRangeWidget->setMinimumValue(d->parametersNode->GetVelocityMin());
  d->VelocityRangeWidget->setMaximumValue(d->parametersNode->GetVelocityMax());
  d->VelocityRangeWidget->blockSignals(wasBlocked);

  wasBlocked = d->ThresholdRangeWidget->blockSignals(true);
  d->ThresholdRangeWidget->setMinimumValue(d->parametersNode->GetIntensityMin());
  d->ThresholdRangeWidget->setMaximumValue(d->parametersNode->GetIntensityMax());
  d->ThresholdRangeWidget->blockSignals(wasBlocked);

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
void qSlicerAstroMomentMapsModuleWidget::onMRMLSelectionNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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
void qSlicerAstroMomentMapsModuleWidget::onCalculate()
{
  Q_D(const qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onCalculate :"
                   " parametersNode not found!";
    return;
    }

  vtkSlicerAstroMomentMapsLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroMomentMapsModuleWidget::onCalculate :"
                  " vtkSlicerAstroMomentMapsLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->parametersNode->SetStatus(1);

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroMomentMapsModuleWidget::onCalculate :"
                  " scene not found!";
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroMomentMapsModuleWidget::onCalculate :"
                  " inputVolume not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  // Check Input volume
  int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  if (n != 3)
    {
    QString message = QString("It is possible to create Moment Maps only"
                              " for datacubes with dimensionality 3 (NAXIS = 3).");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to create the Moment Maps"), message);
    d->parametersNode->SetStatus(0);
    return;
    }

  if (d->parametersNode->GetMaskActive())
    {
    if (!this->convertSelectedSegmentToLabelMap())
      {
      qCritical() <<"qSlicerAstroMomentMapsModuleWidget::onCalculate : "
                    "convertSelectedSegmentToLabelMap failed!";
      d->parametersNode->SetStatus(0);
      return;
      }
    }

  int serial = d->parametersNode->GetOutputSerial();

  // Create 0thMomentMapVolume
  vtkMRMLAstroVolumeNode *ZeroMomentVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetZeroMomentVolumeNodeID()));

  if (d->parametersNode->GetGenerateZero()  ||
      d->parametersNode->GetGenerateFirst() ||
      d->parametersNode->GetGenerateSecond())
    {
    std::ostringstream outSS;
    outSS << inputVolume->GetName() << "_mom0th";
    outSS <<"_"<< IntToString(serial);

    if (ZeroMomentVolume)
      {
      std::string name;
      name = ZeroMomentVolume->GetName();
      if (!name.compare(inputVolume->GetName()))
        {
        ZeroMomentVolume = NULL;
        }
      else if (name.find("_mom0th_") != std::string::npos)
        {
        vtkMRMLAstroVolumeStorageNode* astroStorage =
          vtkMRMLAstroVolumeStorageNode::SafeDownCast(ZeroMomentVolume->GetStorageNode());
        scene->RemoveNode(astroStorage);
        scene->RemoveNode(ZeroMomentVolume->GetAstroVolumeDisplayNode());

        vtkMRMLVolumeRenderingDisplayNode *volumeRenderingDisplay =
          vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(ZeroMomentVolume->GetDisplayNode());
        if (volumeRenderingDisplay)
          {
          scene->RemoveNode(volumeRenderingDisplay->GetROINode());
          scene->RemoveNode(volumeRenderingDisplay);
          }
        scene->RemoveNode(ZeroMomentVolume);
        }
      }

    // Get dimensions
    int N1 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1"));
    int N2 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2"));

    // Create an empty 2D image
    vtkNew<vtkImageData> imageDataTemp;
    imageDataTemp->SetDimensions(N1, N2, 1);
    imageDataTemp->SetSpacing(1.,1.,1.);
    imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

    // create Astro Volume for the moment map
    ZeroMomentVolume = vtkMRMLAstroVolumeNode::SafeDownCast
       (logic->GetAstroVolumeLogic()->CloneVolumeWithoutImageData(scene, inputVolume, outSS.str().c_str()));

    // modify fits attributes
    ZeroMomentVolume->SetAttribute("SlicerAstro.NAXIS", "2");
    ZeroMomentVolume->GetAstroVolumeDisplayNode()->SetAttribute("SlicerAstro.NAXIS", "2");
    ZeroMomentVolume->GetAstroVolumeDisplayNode()->CopyWCS(inputVolume->GetAstroVolumeDisplayNode());
    std::string Bunit = ZeroMomentVolume->GetAttribute("SlicerAstro.BUNIT");
    Bunit += " km/s";
    ZeroMomentVolume->SetAttribute("SlicerAstro.BUNIT", Bunit.c_str());
    std::string Btype = "";
    Btype = inputVolume->GetAstroVolumeDisplayNode()->AddVelocityInfoToDisplayStringZ(Btype);
    ZeroMomentVolume->SetAttribute("SlicerAstro.BTYPE", Btype.c_str());
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.NAXIS3");
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.CDELT3");
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.CROTA3");
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.CRPIX3");
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.CRVAL3");
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.CTYPE3");
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.CUNIT3");
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.DTYPE3");
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.DRVAL3");
    ZeroMomentVolume->RemoveAttribute("SlicerAstro.DUNIT3");

    // copy 2D image into the Astro Volume object
    ZeroMomentVolume->SetAndObserveImageData(imageDataTemp.GetPointer());

    ZeroMomentVolume->SetName(outSS.str().c_str());
    d->parametersNode->SetZeroMomentVolumeNodeID(ZeroMomentVolume->GetID());

    // Remove old rendering Display
    int ndnodes = ZeroMomentVolume->GetNumberOfDisplayNodes();
    for (int ii = 0; ii < ndnodes; ii++)
      {
      vtkMRMLVolumeRenderingDisplayNode *dnode =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
          ZeroMomentVolume->GetNthDisplayNode(ii));
      if (dnode)
        {
        ZeroMomentVolume->RemoveNthDisplayNodeID(ii);
        }
      }

    ZeroMomentVolume->SetAttribute("SlicerAstro.DATAMODEL", "ZEROMOMENTMAP");
    }

  // Create 1stMomentMapVolume
  vtkMRMLAstroVolumeNode *FirstMomentVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetFirstMomentVolumeNodeID()));

  if (d->parametersNode->GetGenerateFirst() ||
      d->parametersNode->GetGenerateSecond())
    {
    std::ostringstream outSS;
    outSS << inputVolume->GetName() << "_mom1st";
    outSS <<"_"<< IntToString(serial);

    if (FirstMomentVolume)
      {
      std::string name;
      name = FirstMomentVolume->GetName();
      if (!name.compare(inputVolume->GetName()))
        {
        FirstMomentVolume = NULL;
        }
      else if (name.find("_mom1st_") != std::string::npos)
        {
        vtkMRMLAstroVolumeStorageNode* astroStorage =
          vtkMRMLAstroVolumeStorageNode::SafeDownCast(FirstMomentVolume->GetStorageNode());
        scene->RemoveNode(astroStorage);
        scene->RemoveNode(FirstMomentVolume->GetAstroVolumeDisplayNode());

        vtkMRMLVolumeRenderingDisplayNode *volumeRenderingDisplay =
          vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(FirstMomentVolume->GetDisplayNode());
        if (volumeRenderingDisplay)
          {
          scene->RemoveNode(volumeRenderingDisplay->GetROINode());
          scene->RemoveNode(volumeRenderingDisplay);
          }
        scene->RemoveNode(FirstMomentVolume);
        }
      }

    // Get dimensions
    int N1 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1"));
    int N2 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2"));

    // Create an empty 2D image
    vtkNew<vtkImageData> imageDataTemp;
    imageDataTemp->SetDimensions(N1, N2, 1);
    imageDataTemp->SetSpacing(1.,1.,1.);
    imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

    // create Astro Volume for the moment map
    FirstMomentVolume = vtkMRMLAstroVolumeNode::SafeDownCast
       (logic->GetAstroVolumeLogic()->CloneVolumeWithoutImageData(scene, inputVolume, outSS.str().c_str()));

    // modify fits attributes
    FirstMomentVolume->SetAttribute("SlicerAstro.NAXIS", "2");
    FirstMomentVolume->GetAstroVolumeDisplayNode()->SetAttribute("SlicerAstro.NAXIS", "2");
    FirstMomentVolume->GetAstroVolumeDisplayNode()->CopyWCS(inputVolume->GetAstroVolumeDisplayNode());
    FirstMomentVolume->SetAttribute("SlicerAstro.BUNIT", "km/s");
    std::string Btype = "";
    Btype = inputVolume->GetAstroVolumeDisplayNode()->AddVelocityInfoToDisplayStringZ(Btype);
    FirstMomentVolume->SetAttribute("SlicerAstro.BTYPE", Btype.c_str());
    FirstMomentVolume->RemoveAttribute("SlicerAstro.NAXIS3");
    FirstMomentVolume->RemoveAttribute("SlicerAstro.CDELT3");
    FirstMomentVolume->RemoveAttribute("SlicerAstro.CROTA3");
    FirstMomentVolume->RemoveAttribute("SlicerAstro.CRPIX3");
    FirstMomentVolume->RemoveAttribute("SlicerAstro.CRVAL3");
    FirstMomentVolume->RemoveAttribute("SlicerAstro.CTYPE3");
    FirstMomentVolume->RemoveAttribute("SlicerAstro.CUNIT3");
    FirstMomentVolume->RemoveAttribute("SlicerAstro.DTYPE3");
    FirstMomentVolume->RemoveAttribute("SlicerAstro.DRVAL3");
    FirstMomentVolume->RemoveAttribute("SlicerAstro.DUNIT3");

    // copy 2D image into the Astro Volume object
    FirstMomentVolume->SetAndObserveImageData(imageDataTemp.GetPointer());

    // change colorMap of the 2D image
    vtkMRMLAstroVolumeDisplayNode* displayNode = FirstMomentVolume->GetAstroVolumeDisplayNode();
    vtkMRMLColorTableNode* velocityFieldColorTableNode = vtkMRMLColorTableNode::SafeDownCast
      (scene->GetFirstNodeByName("VelocityField"));
    if (!velocityFieldColorTableNode)
      {
      qCritical() <<"qSlicerAstroMomentMapsModuleWidget::onCalculate : "
                    "velocityFieldColorTableNode not found!";
      d->parametersNode->SetStatus(0);
      return;
      }
    displayNode->SetAndObserveColorNodeID(velocityFieldColorTableNode->GetID());

    FirstMomentVolume->SetName(outSS.str().c_str());
    d->parametersNode->SetFirstMomentVolumeNodeID(FirstMomentVolume->GetID());

    // Remove old rendering Display
    int ndnodes = FirstMomentVolume->GetNumberOfDisplayNodes();
    for (int ii = 0; ii < ndnodes; ii++)
      {
      vtkMRMLVolumeRenderingDisplayNode *dnode =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
          FirstMomentVolume->GetNthDisplayNode(ii));
      if (dnode)
        {
        FirstMomentVolume->RemoveNthDisplayNodeID(ii);
        }
      }

    FirstMomentVolume->SetAttribute("SlicerAstro.DATAMODEL", "FIRSTMOMENTMAP");
    }

  // Create 2ndMomentMapVolume
  vtkMRMLAstroVolumeNode *SecondMomentVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetSecondMomentVolumeNodeID()));

  if (d->parametersNode->GetGenerateSecond())
    {
    std::ostringstream outSS;
    outSS << inputVolume->GetName() << "_mom2nd";
    outSS <<"_"<< IntToString(serial);

    if (SecondMomentVolume)
      {
      std::string name;
      name = SecondMomentVolume->GetName();
      if (!name.compare(inputVolume->GetName()))
        {
        SecondMomentVolume = NULL;
        }
      else if (name.find("_mom2nd_") != std::string::npos)
        {
        vtkMRMLAstroVolumeStorageNode* astroStorage =
          vtkMRMLAstroVolumeStorageNode::SafeDownCast(SecondMomentVolume->GetStorageNode());
        scene->RemoveNode(astroStorage);
        scene->RemoveNode(SecondMomentVolume->GetAstroVolumeDisplayNode());

        vtkMRMLVolumeRenderingDisplayNode *volumeRenderingDisplay =
          vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(SecondMomentVolume->GetDisplayNode());
        if (volumeRenderingDisplay)
          {
          scene->RemoveNode(volumeRenderingDisplay->GetROINode());
          scene->RemoveNode(volumeRenderingDisplay);
          }
        scene->RemoveNode(SecondMomentVolume);
        }
      }

    // Get dimensions
    int N1 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1"));
    int N2 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2"));

    // Create an empty 2D image
    vtkNew<vtkImageData> imageDataTemp;
    imageDataTemp->SetDimensions(N1, N2, 1);
    imageDataTemp->SetSpacing(1.,1.,1.);
    imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

    // create Astro Volume for the moment map
    SecondMomentVolume = vtkMRMLAstroVolumeNode::SafeDownCast
       (logic->GetAstroVolumeLogic()->CloneVolumeWithoutImageData(scene, inputVolume, outSS.str().c_str()));

    // modify fits attributes
    SecondMomentVolume->SetAttribute("SlicerAstro.NAXIS", "2");
    SecondMomentVolume->GetAstroVolumeDisplayNode()->SetAttribute("SlicerAstro.NAXIS", "2");
    SecondMomentVolume->GetAstroVolumeDisplayNode()->CopyWCS(inputVolume->GetAstroVolumeDisplayNode());
    SecondMomentVolume->SetAttribute("SlicerAstro.BUNIT", "km/s");
    std::string Btype = "";
    Btype = inputVolume->GetAstroVolumeDisplayNode()->AddVelocityInfoToDisplayStringZ(Btype);
    SecondMomentVolume->SetAttribute("SlicerAstro.BTYPE", Btype.c_str());
    SecondMomentVolume->RemoveAttribute("SlicerAstro.NAXIS3");
    SecondMomentVolume->RemoveAttribute("SlicerAstro.CDELT3");
    SecondMomentVolume->RemoveAttribute("SlicerAstro.CROTA3");
    SecondMomentVolume->RemoveAttribute("SlicerAstro.CRPIX3");
    SecondMomentVolume->RemoveAttribute("SlicerAstro.CRVAL3");
    SecondMomentVolume->RemoveAttribute("SlicerAstro.CTYPE3");
    SecondMomentVolume->RemoveAttribute("SlicerAstro.CUNIT3");
    SecondMomentVolume->RemoveAttribute("SlicerAstro.DTYPE3");
    SecondMomentVolume->RemoveAttribute("SlicerAstro.DRVAL3");
    SecondMomentVolume->RemoveAttribute("SlicerAstro.DUNIT3");

    // copy 2D image into the Astro Volume object
    SecondMomentVolume->SetAndObserveImageData(imageDataTemp.GetPointer());

    // change colorMap of the 2D image
    vtkMRMLAstroVolumeDisplayNode* displayNode = SecondMomentVolume->GetAstroVolumeDisplayNode();
    vtkMRMLColorTableNode* RainbowColorTableNode = vtkMRMLColorTableNode::SafeDownCast
      (scene->GetFirstNodeByName("Rainbow"));
    if (!RainbowColorTableNode)
      {
      qCritical() <<"qSlicerAstroMomentMapsModuleWidget::onCalculate : "
                    "RainbowColorTableNode not found!";
      d->parametersNode->SetStatus(0);
      return;
      }
    displayNode->SetAndObserveColorNodeID(RainbowColorTableNode->GetID());

    SecondMomentVolume->SetName(outSS.str().c_str());
    d->parametersNode->SetSecondMomentVolumeNodeID(SecondMomentVolume->GetID());

    // Remove old rendering Display
    int ndnodes = SecondMomentVolume->GetNumberOfDisplayNodes();
    for (int ii = 0; ii < ndnodes; ii++)
      {
      vtkMRMLVolumeRenderingDisplayNode *dnode =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
          SecondMomentVolume->GetNthDisplayNode(ii));
      if (dnode)
        {
        SecondMomentVolume->RemoveNthDisplayNodeID(ii);
        }
      }

    SecondMomentVolume->SetAttribute("SlicerAstro.DATAMODEL", "SECONDMOMENTMAP");
    }

  serial++;
  d->parametersNode->SetOutputSerial(serial);

  if (!logic->CalculateMomentMaps(d->parametersNode))
    {
    qCritical() <<"qSlicerAstroMomentMapsModuleWidget::onCalculate : "
                  "CalculateMomentMaps error!";
    if (d->parametersNode->GetGenerateSecond())
      {
      scene->RemoveNode(ZeroMomentVolume);
      scene->RemoveNode(FirstMomentVolume);
      scene->RemoveNode(SecondMomentVolume);
      }
    else if (d->parametersNode->GetGenerateFirst())
      {
      scene->RemoveNode(ZeroMomentVolume);
      scene->RemoveNode(FirstMomentVolume);
      }
    else if(d->parametersNode->GetGenerateZero())
      {
      scene->RemoveNode(ZeroMomentVolume);
      }
    d->parametersNode->SetStatus(0);

    if (d->parametersNode->GetMaskActive())
      {
      vtkMRMLAstroLabelMapVolumeNode *maskVolume =
        vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
          (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMaskVolumeNodeID()));
      if(maskVolume)
        {
        scene->RemoveNode(maskVolume);
        }
      }

    return;
    }

  if (d->parametersNode->GetMaskActive())
    {
    vtkMRMLAstroLabelMapVolumeNode *maskVolume =
      vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMaskVolumeNodeID()));
    if(maskVolume)
      {
      scene->RemoveNode(maskVolume);
      }
    }

  if (!d->parametersNode->GetGenerateZero())
    {
    scene->RemoveNode(ZeroMomentVolume);
    }

  if (!d->parametersNode->GetGenerateFirst())
    {
    scene->RemoveNode(FirstMomentVolume);
    }

  if (!d->parametersNode->GetGenerateSecond())
    {
    scene->RemoveNode(SecondMomentVolume);
    }

  // Setting the Layout for the Output
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onCalculate : qSlicerApplication not found!";
    return;
    }

  qSlicerLayoutManager* layoutManager = app->layoutManager();

  if(!app)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onCalculate : layoutManager not found!";
    return;
    }

  layoutManager->layoutLogic()->GetLayoutNode()->SetViewArrangement(2);

  vtkSlicerApplicationLogic *appLogic = app->applicationLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMomentMapsModuleWidget::onCalculate : appLogic not found!";
    return;
    }

  if (d->parametersNode->GetGenerateZero())
    {
    vtkMRMLSliceCompositeNode *redSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
    redSliceComposite->SetLabelVolumeID("");
    redSliceComposite->SetForegroundVolumeID("");
    redSliceComposite->SetForegroundOpacity(0.);
    redSliceComposite->SetBackgroundVolumeID(ZeroMomentVolume->GetID());

    vtkMRMLSliceNode *redSlice = vtkMRMLSliceNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
    // setting to the XZ orientation is needed in order to force the refresh
    redSlice->SetOrientation("XZ");
    redSlice->SetOrientation("XY");
    redSlice->SetSliceOffset(0.);

    vtkMRMLSliceLogic* redSliceLogic = appLogic->GetSliceLogic(redSlice);
    if (redSliceLogic)
      {
      int *dims = redSlice->GetDimensions();
      if (dims)
        {
        redSliceLogic->FitSliceToAll(dims[0], dims[1]);
        }
      redSliceLogic->SnapSliceOffsetToIJK();
      }
    }

  if (d->parametersNode->GetGenerateFirst())
    {
    vtkMRMLSliceCompositeNode *yellowSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
    yellowSliceComposite->SetLabelVolumeID("");
    yellowSliceComposite->SetForegroundVolumeID("");
    yellowSliceComposite->SetForegroundOpacity(0.);
    yellowSliceComposite->SetBackgroundVolumeID(FirstMomentVolume->GetID());

    vtkMRMLSliceNode *yellowSlice = vtkMRMLSliceNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
    yellowSlice->SetOrientation("XZ");
    yellowSlice->SetOrientation("XY");
    yellowSlice->SetSliceOffset(0.);

    vtkMRMLSliceLogic* yellowSliceLogic = appLogic->GetSliceLogic(yellowSlice);
    if (yellowSliceLogic)
      {
      int *dims = yellowSlice->GetDimensions();
      if (dims)
        {
        yellowSliceLogic->FitSliceToAll(dims[0], dims[1]);
        }
      yellowSliceLogic->SnapSliceOffsetToIJK();
      }
    }

  if (d->parametersNode->GetGenerateSecond())
    {
    vtkMRMLSliceCompositeNode *greenSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
    greenSliceComposite->SetLabelVolumeID("");
    greenSliceComposite->SetForegroundVolumeID("");
    greenSliceComposite->SetForegroundOpacity(0.);
    greenSliceComposite->SetBackgroundVolumeID(SecondMomentVolume->GetID());

    vtkMRMLSliceNode *greenSlice = vtkMRMLSliceNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
    greenSlice->SetOrientation("XZ");
    greenSlice->SetOrientation("XY");
    greenSlice->SetSliceOffset(0.);

    vtkMRMLSliceLogic* greenSliceLogic = appLogic->GetSliceLogic(greenSlice);
    if (greenSliceLogic)
      {
      int *dims = greenSlice->GetDimensions();
      if (dims)
        {
        greenSliceLogic->FitSliceToAll(dims[0], dims[1]);
        }
      greenSliceLogic->SnapSliceOffsetToIJK();
      }
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode)
    {
    vtkSmartPointer<vtkMRMLSegmentationNode> newSegmentationNode =
      vtkSmartPointer<vtkMRMLSegmentationNode>::New();
    this->mrmlScene()->AddNode(newSegmentationNode);
    d->segmentEditorNode->SetAndObserveSegmentationNode(newSegmentationNode);
    currentSegmentationNode = newSegmentationNode;
    }

  if (!currentSegmentationNode->GetDisplayNode())
    {
    currentSegmentationNode->CreateDefaultDisplayNodes();
    }

  for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
    SegmentationDisplayNode->SetAllSegmentsVisibility(false);
    }

  d->parametersNode->SetStatus(0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->CancelButton->hide();
  d->progressBar->hide();
  d->ApplyButton->show();
}

//---------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

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

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onThresholdRangeChanged(double min, double max)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetIntensityMin(min);
  d->parametersNode->SetIntensityMax(max);
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onUnitNodeIntensityChanged(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!sender)
    {
    return;
    }

  vtkMRMLUnitNode* unitNodeIntensity = vtkMRMLUnitNode::SafeDownCast(sender);
  d->ThresholdRangeWidget->setDecimals(unitNodeIntensity->GetPrecision());
  d->ThresholdRangeWidget->minimumSpinBox()->setDecimals(unitNodeIntensity->GetPrecision());
  d->ThresholdRangeWidget->maximumSpinBox()->setDecimals(unitNodeIntensity->GetPrecision());
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onUnitNodeVelocityChanged(vtkObject *sender)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!sender)
    {
    return;
    }

  vtkMRMLUnitNode* unitNodeVelocity = vtkMRMLUnitNode::SafeDownCast(sender);
  d->VelocityRangeWidget->setDecimals(unitNodeVelocity->GetPrecision());
  d->VelocityRangeWidget->minimumSpinBox()->setDecimals(unitNodeVelocity->GetPrecision());
  d->VelocityRangeWidget->maximumSpinBox()->setDecimals(unitNodeVelocity->GetPrecision());
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onVelocityRangeChanged(double min, double max)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetVelocityMin(min);
  d->parametersNode->SetVelocityMax(max);
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onZeroMomentVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->parametersNode->SetZeroMomentVolumeNodeID(mrmlNode->GetID());
    }
  else
    {
    d->parametersNode->SetZeroMomentVolumeNodeID(NULL);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->parametersNode->SetStatus(-1);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->progressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMomentMapsModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroMomentMapsModuleWidget);
  d->ApplyButton->hide();
  d->progressBar->show();
  d->CancelButton->show();
}

//---------------------------------------------------------------------------
vtkMRMLAstroMomentMapsParametersNode* qSlicerAstroMomentMapsModuleWidget::
mrmlAstroMomentMapsParametersNode()const
{
  Q_D(const qSlicerAstroMomentMapsModuleWidget);
  return d->parametersNode;
}

