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
#include <QLineEdit>
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
#include <vtkTable.h>
#include <vtksys/SystemTools.hxx>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>

// AstroMasking includes
#include "qSlicerAstroMaskingModuleWidget.h"
#include "ui_qSlicerAstroMaskingModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroMaskingLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>

// qMRML includes
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSliceWidget.h>
#include <qMRMLTableView.h>

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroMaskingParametersNode.h>
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
#include <vtkMRMLTableNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#include <sys/time.h>

//-----------------------------------------------------------------------------
/// \ingroup SlicerAstro_QtModules_AstroMasking
class qSlicerAstroMaskingModuleWidgetPrivate: public Ui_qSlicerAstroMaskingModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroMaskingModuleWidget);
protected:
  qSlicerAstroMaskingModuleWidget* const q_ptr;
public:

  qSlicerAstroMaskingModuleWidgetPrivate(qSlicerAstroMaskingModuleWidget& object);
  ~qSlicerAstroMaskingModuleWidgetPrivate();

  /// Initialization function of the widgets
  void init();

  /// clean pointers before importing a scene
  void cleanPointers();

  /// return the logic
  vtkSlicerAstroMaskingLogic* logic() const;

  qSlicerAstroVolumeModuleWidget* astroVolumeWidget;
  vtkSmartPointer<vtkMRMLAstroMaskingParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLAnnotationROINode> InputROINode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;

  bool FitROI = true;
};

//-----------------------------------------------------------------------------
// qSlicerAstroMaskingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModuleWidgetPrivate::qSlicerAstroMaskingModuleWidgetPrivate(qSlicerAstroMaskingModuleWidget& object)
  : q_ptr(&object)
{
  this->astroVolumeWidget = 0;
  this->parametersNode = 0;
  this->InputROINode = 0;
  this->selectionNode = 0;
  this->segmentEditorNode = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModuleWidgetPrivate::~qSlicerAstroMaskingModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroMaskingModuleWidget);

  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidgetPrivate::init(): could not find qSlicerApplication!";
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

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroMaskingParametersNode(vtkMRMLNode*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->OutputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->ROINodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputROIChanged(vtkMRMLNode*)));

  QObject::connect(this->ROIModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(this->SegmentationModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SegmentsTableView, SLOT(setMRMLScene(vtkMRMLScene*)));

  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  this->SegmentsTableView->hide();

  QObject::connect(this->OperationComboBox, SIGNAL(currentIndexChanged(QString)),
                   q, SLOT(onOperationChanged(QString)));

  QObject::connect(this->InsidePushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onInsideBlankRegionChanged()));

  QObject::connect(this->OutsidePushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onOutsideBlankRegionChanged()));

  QObject::connect(this->BlankValueLineEdit, SIGNAL(editingFinished()),
                   q, SLOT(onBlankValueChanged()));

  QObject::connect(this->ApplyButton, SIGNAL(clicked()),
                   q, SLOT(onApply()));

  QObject::connect(this->CancelButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));

  this->InputCollapsibleButton->setCollapsed(false);

  this->progressBar->hide();
  this->progressBar->setMinimum(0);
  this->progressBar->setMaximum(100);
  this->CancelButton->hide();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroMaskingModuleWidget);

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

  if (this->InputROINode)
    {
    q->mrmlScene()->RemoveNode(this->InputROINode);
    }
  this->InputROINode = 0;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroMaskingLogic* qSlicerAstroMaskingModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroMaskingModuleWidget);
  return vtkSlicerAstroMaskingLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroMaskingModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModuleWidget::qSlicerAstroMaskingModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroMaskingModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroMaskingModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroMaskingModuleWidget::~qSlicerAstroMaskingModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::enter()
{
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
          (vtkMRMLLayoutNode::SlicerLayoutDual3DView);

  this->onMRMLAstroMaskingParametersNodeModified();
}

//----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::exit()
{
  this->Superclass::exit();

  this->onROIVisibilityChanged(false);
}

namespace
{
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
void qSlicerAstroMaskingModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::setMRMLScene : selectionNode not found!";
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

  d->InputCollapsibleButton->setCollapsed(false);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  this->initializeParameterNode(forceNew);

  this->initializeSegmentations(forceNew);

  this->initializeROINode(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::onEndCloseEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes(true);
  this->onMRMLAstroMaskingParametersNodeModified();

  if (!this->isEntered())
    {
    this->onROIVisibilityChanged(false);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::onEndImportEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroMaskingParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onInputROIChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLAnnotationROINode* roiNode = vtkMRMLAnnotationROINode::SafeDownCast(mrmlNode);
  if (d->InputROINode == roiNode)
    {
    return;
    }

  d->InputROINode = roiNode;
  d->parametersNode->SetROINode(roiNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroMaskingParametersNode *astroParametersNode = NULL;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroMaskingParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroMaskingParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroMaskingParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroMaskingParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroMaskingParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::initializeROINode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkSmartPointer<vtkMRMLNode> roiNode = NULL;

  if (!forceNew)
    {
    if (d->parametersNode->GetROINode())
      {
      roiNode = d->parametersNode->GetROINode();
      }
    else
      {
      vtkSmartPointer<vtkCollection> roiNodes = vtkSmartPointer<vtkCollection>::Take
          (this->mrmlScene()->GetNodesByClass("vtkMRMLAnnotationROINode"));

      for (int ii = roiNodes->GetNumberOfItems() - 1; ii >= 0 ; ii--)
        {
        vtkMRMLNode* tempROINode = vtkMRMLNode::SafeDownCast(roiNodes->GetItemAsObject(ii));
        if (!tempROINode)
          {
          continue;
          }
        std::string ROIName = tempROINode->GetName();
        std::size_t found = ROIName.find("MaskingROI");
        if (found != std::string::npos)
          {
          roiNode = tempROINode;
          break;
          }
        }
      }

    if (roiNode)
      {
      d->InputROINode = vtkMRMLAnnotationROINode::SafeDownCast(roiNode);
      d->parametersNode->SetROINode(d->InputROINode);
      return;
      }
    }

  vtkMRMLNode * foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAnnotationROINode");
  roiNode.TakeReference(foo);
  std::string ROINodeName = this->mrmlScene()->GenerateUniqueName("MaskingROI");
  roiNode->SetName(ROINodeName.c_str());
  this->mrmlScene()->AddNode(roiNode);

  d->InputROINode = vtkMRMLAnnotationROINode::SafeDownCast(roiNode);
  d->parametersNode->SetROINode(d->InputROINode);
  int wasModifyingROI = d->InputROINode->StartModify();
  this->onROIFit();
  d->InputROINode->EndModify(wasModifyingROI);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

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
vtkSegment* qSlicerAstroMaskingModuleWidget::convertSelectedSegmentToLabelMap()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!this->mrmlScene())
    {
    return NULL;
    }

  if (!d->segmentEditorNode)
    {
    qCritical() << Q_FUNC_INFO << ": segmentEditorNode not found.";
    return NULL;
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode || !currentSegmentationNode->GetSegmentation())
    {
    QString message = QString("No segmentation available!");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to select a segment"), message);
    return NULL;
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
    return NULL;
    }

  segmentIDs.clear();
  segmentIDs.push_back(selectedSegmentIDs[0].toStdString());

  vtkMRMLAstroVolumeNode* activeVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(
     d->InputVolumeNodeSelector->currentNode());

  if (activeVolumeNode)
    {
    vtkSlicerAstroMaskingLogic* astroMaskinglogic =
      vtkSlicerAstroMaskingLogic::SafeDownCast(this->logic());
    if (!astroMaskinglogic)
      {
      qCritical() << Q_FUNC_INFO << ": astroMaskinglogic not found!";
      return NULL;
      }
    vtkSlicerAstroVolumeLogic* astroVolumelogic =
      vtkSlicerAstroVolumeLogic::SafeDownCast(astroMaskinglogic->GetAstroVolumeLogic());
    if (!astroVolumelogic)
      {
      qCritical() << Q_FUNC_INFO << ": vtkSlicerAstroVolumeLogic not found!";
      return NULL;
      }
    std::string name(activeVolumeNode->GetName());
    name += "Copy_mask" + IntToString(d->parametersNode->GetOutputSerial());
    labelMapNode = astroVolumelogic->CreateAndAddLabelVolume(this->mrmlScene(), activeVolumeNode, name.c_str());
    }
  else
    {
    qCritical() << Q_FUNC_INFO << ": converting current segmentation Node into labelMap Node (Mask),"
                                  " but the labelMap Node is invalid!";
    return NULL;
    }

  if (!vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode(currentSegmentationNode, segmentIDs, labelMapNode, activeVolumeNode))
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              "Be sure that segment to export has been selected in the table view (left click). \n\n").
                              arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    this->mrmlScene()->RemoveNode(labelMapNode);
    return NULL;
    }

  labelMapNode->UpdateRangeAttributes();

  d->parametersNode->SetMaskVolumeNodeID(labelMapNode->GetID());

  // this is necessary to calculate the bondaries later on.
  return currentSegmentationNode->GetSegmentation()->GetSegment(selectedSegmentIDs[0].toStdString());
}

//--------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onBlankValueChanged()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetBlankValue(d->BlankValueLineEdit->text().toStdString().c_str());
}

//--------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

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
void qSlicerAstroMaskingModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

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
void qSlicerAstroMaskingModuleWidget::onOperationChanged(QString Operation)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetOperation(Operation.toStdString().c_str());
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

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
    d->parametersNode->SetOutputVolumeNodeID(NULL);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onInsideBlankRegionChanged()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetBlankRegion("Inside");
  d->parametersNode->Modified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onOutsideBlankRegionChanged()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetBlankRegion("Outside");
  d->parametersNode->Modified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onROIFit()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  d->logic()->SnapROIToVoxelGrid(d->parametersNode);
  d->logic()->FitROIToInputVolume(d->parametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onROIVisibilityChanged(bool visible)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLAnnotationROINode *ROINode = d->parametersNode->GetROINode();
  if(!ROINode)
    {
    return;
    }

  ROINode->SetDisplayVisibility(visible);
}

//--------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::setMRMLAstroMaskingParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroMaskingParametersNode* AstroMaskingParaNode =
      vtkMRMLAstroMaskingParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroMaskingParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroMaskingParametersNodeModified()));

  d->parametersNode = AstroMaskingParaNode;

  this->onMRMLAstroMaskingParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode || !d->selectionNode)
    {
    return;
    }

  if (mrmlNode)
    {
    d->selectionNode->SetReferenceActiveVolumeID(mrmlNode->GetID());
    d->selectionNode->SetActiveVolumeID(mrmlNode->GetID());
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(NULL);
    d->selectionNode->SetActiveVolumeID(NULL);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onModeChanged()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();

  if (d->ROIModeRadioButton->isChecked())
    {
    d->parametersNode->SetMode("ROI");
    }
  if (d->SegmentationModeRadioButton->isChecked())
    {
    d->parametersNode->SetMode("Segmentation");
    }

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onMRMLAstroMaskingParametersNodeModified()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  char *outputVolumeNodeID = d->parametersNode->GetOutputVolumeNodeID();
  vtkMRMLAstroVolumeNode *outputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(outputVolumeNodeID));
  d->OutputVolumeNodeSelector->setCurrentNode(outputVolumeNode);

  d->ROINodeComboBox->setCurrentNode(d->parametersNode->GetROINode());

  if (!(strcmp(d->parametersNode->GetMode(), "ROI")))
    {  
    d->ROIModeRadioButton->setChecked(true);
    d->SegmentsTableView->hide();
    if (this->isEntered())
      {
      this->onROIVisibilityChanged(true);
      }
    else
      {
      this->onROIVisibilityChanged(false);
      }

    if (d->segmentEditorNode)
      {
      vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
      if (currentSegmentationNode && this->isEntered())
        {
        for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
          {
          vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
            vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
          if (!SegmentationDisplayNode)
            {
            continue;
            }
          SegmentationDisplayNode->SetAllSegmentsVisibility(false);
          }
        }
      }
    }
  else if (!(strcmp(d->parametersNode->GetMode(), "Segmentation")))
    {
    d->SegmentationModeRadioButton->setChecked(true);
    d->SegmentsTableView->show();
    this->onROIVisibilityChanged(false);

    if (d->segmentEditorNode)
      {
      vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
      if (currentSegmentationNode && this->isEntered())
        {
        for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
          {
          vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
            vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
          if (!SegmentationDisplayNode)
            {
            continue;
            }
          SegmentationDisplayNode->SetAllSegmentsVisibility(true);
          }
        }
      }
    }

  if (!(strcmp(d->parametersNode->GetOperation(), "Blank")))
    {
    d->OperationComboBox->setCurrentIndex(0);
    d->InsidePushButton->show();
    d->OutsidePushButton->show();
    d->BlankValueLabel->show();
    d->BlankValueLineEdit->show();
    }
  else if (!(strcmp(d->parametersNode->GetOperation(), "Crop")))
    {
    d->OperationComboBox->setCurrentIndex(1);
    d->InsidePushButton->hide();
    d->OutsidePushButton->hide();
    d->BlankValueLabel->hide();
    d->BlankValueLineEdit->hide();
    }

  bool InsideState = d->InsidePushButton->blockSignals(true);
  bool OutsideState = d->OutsidePushButton->blockSignals(true);
  if (!(strcmp(d->parametersNode->GetBlankRegion(), "Inside")))
    {
    d->InsidePushButton->setChecked(true);
    d->OutsidePushButton->setChecked(false);
    }
  else if (!(strcmp(d->parametersNode->GetBlankRegion(), "Outside")))
    {
    d->InsidePushButton->setChecked(false);
    d->OutsidePushButton->setChecked(true);
    }
  d->OutsidePushButton->blockSignals(OutsideState);
  d->InsidePushButton->blockSignals(InsideState);

  d->BlankValueLineEdit->setText(d->parametersNode->GetBlankValue());

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
void qSlicerAstroMaskingModuleWidget::onMRMLSelectionNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

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

  if (d->FitROI)
    {
    this->onROIFit();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onApply()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::on Apply : "
                   "parametersNode not found!";
    return;
    }

  vtkSlicerAstroMaskingLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroMaskingModuleWidget::on Apply : "
                  "vtkSlicerAstroMaskingLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->segmentEditorNode)
    {
    qCritical() << "qSlicerAstroMaskingModuleWidget::on Apply : "
                   "d->segmentEditorNode not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->FitROI = false;
  d->parametersNode->SetStatus(1);

  vtkSegment *segment = NULL;
  if (!(strcmp(d->parametersNode->GetMode(), "Segmentation")))
    {
    segment = this->convertSelectedSegmentToLabelMap();
    if (!segment)
      {
      qCritical() <<"qSlicerAstroMaskingModuleWidget::on Apply : "
                    "convertSelectedSegmentToLabelMap failed!";
      d->parametersNode->SetStatus(0);
      return;
      }
    }

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroMaskingModuleWidget::onApply"
                  " : scene not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume || !inputVolume->GetImageData())
    {
    qCritical() <<"qSlicerAstroMaskingModuleWidget::onApply"
                  " : inputVolume not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  // Create output volume node
  std::ostringstream outSS;
  outSS << inputVolume->GetName();

  if (!(strcmp(d->parametersNode->GetOperation(), "Blank")))
    {
    outSS<<"_Blank";
    }
  else if (!(strcmp(d->parametersNode->GetOperation(), "Crop")))
    {
    outSS<<"_Crop";
    }

  int serial = d->parametersNode->GetOutputSerial();
  outSS<<"_"<< IntToString(serial);
  serial++;
  d->parametersNode->SetOutputSerial(serial);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  if (outputVolume)
    {
    std::string name;
    name = outputVolume->GetName();
    if (!name.compare(inputVolume->GetName()))
      {
      outputVolume = NULL;
      }
    else if (name.find("_Blank_") != std::string::npos ||
             name.find("_Crop_") != std::string::npos )
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

  // Create Astro Volume for the output
  outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
    (logic->GetAstroVolumeLogic()->CloneVolume(scene, inputVolume, outSS.str().c_str()));

  d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());

  vtkMRMLNode* node = NULL;
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

  // Run computation
  if (logic->ApplyMask(d->parametersNode, d->segmentEditorNode->GetSegmentationNode(), segment))
    {
    if (!(strcmp(d->parametersNode->GetOperation(), "Blank")))
      {
      d->astroVolumeWidget->setComparative3DViews
          (inputVolume->GetID(), outputVolume->GetID(), false, true);
      }
    else if (!(strcmp(d->parametersNode->GetOperation(), "Crop")))
      {
      d->astroVolumeWidget->setComparative3DViews
          (inputVolume->GetID(), outputVolume->GetID(), false, false);
      }
    }
  else
    {
    qCritical() <<"qSlicerAstroMaskingModuleWidget::onApply : "
                  "ApplyMask error!";
    scene->RemoveNode(outputVolume);
    }

  d->parametersNode->SetStatus(0);
  d->FitROI = true;

  if (!(strcmp(d->parametersNode->GetMode(), "Segmentation")))
    {
    vtkMRMLAstroLabelMapVolumeNode *maskVolume =
      vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMaskVolumeNodeID()));
    if(maskVolume)
      {
      scene->RemoveNode(maskVolume);
      }
    if (d->segmentEditorNode)
      {
      vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
      if (currentSegmentationNode && this->isEntered())
        {
        for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
          {
          vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
            vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
          if (!SegmentationDisplayNode)
            {
            continue;
            }
          SegmentationDisplayNode->SetAllSegmentsVisibility(false);
          }
        }
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  d->CancelButton->hide();
  d->progressBar->hide();
  d->ApplyButton->show();
}

//---------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

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
void qSlicerAstroMaskingModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  d->parametersNode->SetStatus(-1);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  d->progressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroMaskingModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroMaskingModuleWidget);

  d->ApplyButton->hide();
  d->progressBar->show();
  d->CancelButton->show();
}

//---------------------------------------------------------------------------
vtkMRMLAstroMaskingParametersNode* qSlicerAstroMaskingModuleWidget::
mrmlAstroMaskingParametersNode()const
{
  Q_D(const qSlicerAstroMaskingModuleWidget);

  return d->parametersNode;
}

