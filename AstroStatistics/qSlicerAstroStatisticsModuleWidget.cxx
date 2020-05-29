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

// AstroStatistics includes
#include "qSlicerAstroStatisticsModuleWidget.h"
#include "ui_qSlicerAstroStatisticsModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroStatisticsLogic.h>
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
#include <vtkMRMLAstroStatisticsParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
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
/// \ingroup SlicerAstro_QtModules_AstroStatistics
class qSlicerAstroStatisticsModuleWidgetPrivate: public Ui_qSlicerAstroStatisticsModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroStatisticsModuleWidget);
protected:
  qSlicerAstroStatisticsModuleWidget* const q_ptr;
public:

  qSlicerAstroStatisticsModuleWidgetPrivate(qSlicerAstroStatisticsModuleWidget& object);
  ~qSlicerAstroStatisticsModuleWidgetPrivate();

  void init();
  void cleanPointers();

  vtkSlicerAstroStatisticsLogic* logic() const;
  vtkSmartPointer<vtkMRMLAstroStatisticsParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLAnnotationROINode> InputROINode;
  vtkSmartPointer<vtkMRMLTableNode> astroTableNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
  QAction *CopyAction;
  QAction *PasteAction;
  QAction *PlotAction;
};

//-----------------------------------------------------------------------------
// qSlicerAstroStatisticsModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroStatisticsModuleWidgetPrivate::qSlicerAstroStatisticsModuleWidgetPrivate(qSlicerAstroStatisticsModuleWidget& object)
  : q_ptr(&object)
{
  this->parametersNode = nullptr;
  this->InputROINode = nullptr;
  this->astroTableNode = nullptr;
  this->selectionNode = nullptr;
  this->segmentEditorNode = nullptr;
  this->CopyAction = nullptr;
  this->PasteAction = nullptr;
  this->PlotAction = nullptr;
}

//-----------------------------------------------------------------------------
qSlicerAstroStatisticsModuleWidgetPrivate::~qSlicerAstroStatisticsModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroStatisticsModuleWidget);

  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroStatisticsModuleWidgetPrivate::init(): could not find qSlicerApplication!";
    return;
    }

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroStatisticsParametersNode(vtkMRMLNode*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->ROINodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputROIChanged(vtkMRMLNode*)));

  QObject::connect(this->TableNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onTableNodeChanged(vtkMRMLNode*)));

  QObject::connect(this->ROIModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(this->SegmentationModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SegmentsTableView, SLOT(setMRMLScene(vtkMRMLScene*)));

  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
  this->SegmentsTableView->hide();

  QObject::connect(this->NpixelsCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onNpixelsToggled(bool)));

  QObject::connect(this->MinCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onMinToggled(bool)));

  QObject::connect(this->MaxCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onMaxToggled(bool)));

  QObject::connect(this->MeanCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onMeanToggled(bool)));

  QObject::connect(this->StdCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onStdToggled(bool)));

  QObject::connect(this->SumCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onSumToggled(bool)));

  QObject::connect(this->TotalFluxCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onTotalFluxToggled(bool)));

  QObject::connect(this->MedianCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onMedianToggled(bool)));

  QObject::connect(this->ApplyButton, SIGNAL(clicked()),
                   q, SLOT(onCalculate()));

  QObject::connect(this->CancelButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));

  this->InputCollapsibleButton->setCollapsed(false);

  this->progressBar->hide();
  this->progressBar->setMinimum(0);
  this->progressBar->setMaximum(100);
  this->CancelButton->hide();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroStatisticsModuleWidget);

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

  if (this->InputROINode)
    {
    q->mrmlScene()->RemoveNode(this->InputROINode);
    }
  this->InputROINode = 0;

  if (this->astroTableNode)
    {
    q->mrmlScene()->RemoveNode(this->astroTableNode);
    }
  this->astroTableNode = 0;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroStatisticsLogic* qSlicerAstroStatisticsModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroStatisticsModuleWidget);
  return vtkSlicerAstroStatisticsLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroStatisticsModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroStatisticsModuleWidget::qSlicerAstroStatisticsModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroStatisticsModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroStatisticsModuleWidget::~qSlicerAstroStatisticsModuleWidget()
{
}

//----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::enter()
{
  this->Superclass::enter();

  this->onMRMLAstroStatisticsParametersNodeModified();
}

//----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::exit()
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
void qSlicerAstroStatisticsModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroStatisticsModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroStatisticsModuleWidget::setMRMLScene : selectionNode not found!";
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
void qSlicerAstroStatisticsModuleWidget::initializeNodes(bool forceNew /*= false*/)
{
  this->initializeParameterNode(forceNew);

  this->initializeSegmentations(forceNew);

  this->initializeROINode(forceNew);

  this->initializeTableNode(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroStatisticsModuleWidget::onEndCloseEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroStatisticsModuleWidget::onEndCloseEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes(true);
  this->onMRMLAstroStatisticsParametersNodeModified();

  if (!this->isEntered())
    {
    this->onROIVisibilityChanged(false);
    }
  d->OutputCollapsibleButton->setCollapsed(true);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroStatisticsModuleWidget::onEndImportEvent :"
                   " appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroStatisticsModuleWidget::onEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroStatisticsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onInputROIChanged(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  vtkMRMLAnnotationROINode* roiNode = vtkMRMLAnnotationROINode::SafeDownCast(node);
  if (d->InputROINode == roiNode)
    {
    return;
    }

  d->InputROINode = roiNode;
  d->parametersNode->SetROINode(roiNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroStatisticsParametersNode *astroParametersNode = nullptr;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroStatisticsParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroStatisticsParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroStatisticsParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroStatisticsParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroStatisticsParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::initializeROINode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkSmartPointer<vtkMRMLNode> roiNode = nullptr;

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
        std::size_t found = ROIName.find("StatisticsROI");
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
  std::string ROINodeName = this->mrmlScene()->GenerateUniqueName("StatisticsROI");
  roiNode->SetName(ROINodeName.c_str());
  this->mrmlScene()->AddNode(roiNode);

  d->InputROINode = vtkMRMLAnnotationROINode::SafeDownCast(roiNode);
  d->parametersNode->SetROINode(d->InputROINode);
  int wasModifyingROI = d->InputROINode->StartModify();
  this->onROIFit();
  d->InputROINode->EndModify(wasModifyingROI);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::initializeTableNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!this->mrmlScene() || !d->parametersNode)
    {
    return;
    }

  vtkSmartPointer<vtkMRMLNode> tableNode = nullptr;

  if (!forceNew)
    {
    if (d->parametersNode->GetTableNode())
      {
      tableNode = d->parametersNode->GetTableNode();
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
        std::size_t found = TableName.find("StatisticsTable");
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
      d->parametersNode->SetTableNode(d->astroTableNode);
      return;
      }
    }

  vtkMRMLNode * foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLTableNode");
  tableNode.TakeReference(foo);
  std::string paramsTableNodeName = this->mrmlScene()->GenerateUniqueName("StatisticsTable");
  tableNode->SetName(paramsTableNodeName.c_str());
  this->mrmlScene()->AddNode(tableNode);

  d->astroTableNode = vtkMRMLTableNode::SafeDownCast(tableNode);
  int wasModifying = d->astroTableNode->StartModify();
  d->astroTableNode->RemoveAllColumns();
  d->astroTableNode->SetUseColumnNameAsColumnHeader(true);

  vtkStringArray* Selection = vtkStringArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Selection)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Selection Column.";
    return;
    }
  Selection->SetName("Selection");
  d->astroTableNode->SetColumnUnitLabel("Selection", "");
  d->astroTableNode->SetColumnLongName("Selection", "Selection");

  d->astroTableNode->SetDefaultColumnType("int");
  vtkIntArray* Npixels = vtkIntArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Npixels)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Npixels Column.";
    return;
    }
  Npixels->SetName("Npixels");
  d->astroTableNode->SetColumnUnitLabel("Npixels", "#");
  d->astroTableNode->SetColumnLongName("Npixels", "Number of pixels");

  d->astroTableNode->SetDefaultColumnType("double");
  vtkDoubleArray* Min = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Min)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Min Column.";
    return;
    }
  Min->SetName("Min");
  d->astroTableNode->SetColumnUnitLabel("Min", "Jy/beam");
  d->astroTableNode->SetColumnLongName("Min", "Minimum");

  vtkDoubleArray* Max = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Max)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Max Column.";
    return;
    }
  Max->SetName("Max");
  d->astroTableNode->SetColumnUnitLabel("Max", "Jy/beam");
  d->astroTableNode->SetColumnLongName("Max", "Maximum");

  vtkDoubleArray* Mean = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Mean)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Mean Column.";
    return;
    }
  Mean->SetName("Mean");
  d->astroTableNode->SetColumnUnitLabel("Mean", "Jy/beam");
  d->astroTableNode->SetColumnLongName("Mean", "Mean");

  vtkDoubleArray* Std = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Std)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Std Column.";
    return;
    }
  Std->SetName("Std");
  d->astroTableNode->SetColumnUnitLabel("Std", "Jy/beam");
  d->astroTableNode->SetColumnLongName("Std", "Standard deviation");

  vtkDoubleArray* Median = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Median)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Median Column.";
    return;
    }
  Median->SetName("Median");
  d->astroTableNode->SetColumnUnitLabel("Median", "Jy/beam");
  d->astroTableNode->SetColumnLongName("Median", "Median");

  vtkDoubleArray* Sum = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!Sum)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the Sum Column.";
    return;
    }
  Sum->SetName("Sum");
  d->astroTableNode->SetColumnUnitLabel("Sum", "Jy/beam");
  d->astroTableNode->SetColumnLongName("Sum", "Sum");

  vtkDoubleArray* TotalFlux = vtkDoubleArray::SafeDownCast(d->astroTableNode->AddColumn());
  if (!TotalFlux)
    {
    qCritical() <<"qSlicerAstroModelingModuleWidget::initializeTableNode : "
                  "Unable to find the TotalFlux Column.";
    return;
    }
  TotalFlux->SetName("TotalFlux");
  d->astroTableNode->SetColumnUnitLabel("TotalFlux", "Jy");
  d->astroTableNode->SetColumnLongName("TotalFlux", "Total Flux");

  d->astroTableNode->EndModify(wasModifying);

  d->parametersNode->SetTableNode(d->astroTableNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

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
bool qSlicerAstroStatisticsModuleWidget::convertSelectedSegmentToLabelMap()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

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
    QMessageBox::warning(nullptr, tr("Failed to select a segment"), message);
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
    QMessageBox::warning(nullptr, tr("Failed to select a segment"), message);
    return false;
    }

  segmentIDs.clear();
  segmentIDs.push_back(selectedSegmentIDs[0].toStdString());

  vtkMRMLAstroVolumeNode* activeVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(
     d->InputVolumeNodeSelector->currentNode());

  if (activeVolumeNode)
    {
    vtkSlicerAstroStatisticsLogic* astroStatisticslogic =
      vtkSlicerAstroStatisticsLogic::SafeDownCast(this->logic());
    if (!astroStatisticslogic)
      {
      qCritical() << Q_FUNC_INFO << ": astroStatisticslogic not found!";
      return false;
      }
    vtkSlicerAstroVolumeLogic* astroVolumelogic =
      vtkSlicerAstroVolumeLogic::SafeDownCast(astroStatisticslogic->GetAstroVolumeLogic());
    if (!astroVolumelogic)
      {
      qCritical() << Q_FUNC_INFO << ": vtkSlicerAstroVolumeLogic not found!";
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
    QMessageBox::warning(nullptr, tr("Failed to export segment"), message);
    this->mrmlScene()->RemoveNode(labelMapNode);
    return false;
    }

  labelMapNode->UpdateRangeAttributes();

  d->parametersNode->SetMaskVolumeNodeID(labelMapNode->GetID());

  return true;
}

//--------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::setup()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

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
  d->AstroStatisticsCopyButton->setDefaultAction(d->CopyAction);
  this->connect(d->CopyAction, SIGNAL(triggered()), d->TableView, SLOT(copySelection()));
  d->AstroStatisticsPasteButton->setDefaultAction(d->PasteAction);
  this->connect(d->PasteAction, SIGNAL(triggered()), d->TableView, SLOT(pasteSelection()));
  d->AstroStatisticsPlotButton->setDefaultAction(d->PlotAction);
  this->connect(d->PlotAction, SIGNAL(triggered()), d->TableView, SLOT(plotSelection()));

  // Table View resize options
  d->TableView->resizeColumnsToContents();
}

//--------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

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
void qSlicerAstroStatisticsModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

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
void qSlicerAstroStatisticsModuleWidget::onNpixelsToggled(bool toggled)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetNpixels(toggled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onStdToggled(bool toggled)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetStd(toggled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onROIFit()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  d->logic()->SnapROIToVoxelGrid(d->parametersNode);
  d->logic()->FitROIToInputVolume(d->parametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onROIVisibilityChanged(bool visible)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
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
void qSlicerAstroStatisticsModuleWidget::setMRMLAstroStatisticsParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroStatisticsParametersNode* AstroStatisticsParaNode =
      vtkMRMLAstroStatisticsParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroStatisticsParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroStatisticsParametersNodeModified()));

  d->parametersNode = AstroStatisticsParaNode;

  this->onMRMLAstroStatisticsParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

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
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(nullptr);
    d->selectionNode->SetActiveVolumeID(nullptr);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onMaxToggled(bool toggled)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetMax(toggled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onMeanToggled(bool toggled)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetMean(toggled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onMedianToggled(bool toggled)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetMedian(toggled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onMinToggled(bool toggled)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetMin(toggled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onModeChanged()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
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
void qSlicerAstroStatisticsModuleWidget::onMRMLAstroStatisticsParametersNodeModified()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  const char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);

  d->ROINodeComboBox->setCurrentNode(d->parametersNode->GetROINode());
  d->TableNodeComboBox->setCurrentNode(d->parametersNode->GetTableNode());

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

  d->TableView->setMRMLTableNode(d->parametersNode->GetTableNode());

  d->MaxCheckBox->setChecked(d->parametersNode->GetMax());
  d->MeanCheckBox->setChecked(d->parametersNode->GetMean());
  d->MedianCheckBox->setChecked(d->parametersNode->GetMedian());
  d->MinCheckBox->setChecked(d->parametersNode->GetMin());
  d->NpixelsCheckBox->setChecked(d->parametersNode->GetNpixels());
  d->StdCheckBox->setChecked(d->parametersNode->GetStd());
  d->SumCheckBox->setChecked(d->parametersNode->GetSum());
  d->TotalFluxCheckBox->setChecked(d->parametersNode->GetTotalFlux());

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
void qSlicerAstroStatisticsModuleWidget::onMRMLSelectionNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

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

  this->onROIFit();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onCalculate()
{
  Q_D(const qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroStatisticsModuleWidget::onCalculate : "
                   "parametersNode not found!";
    return;
    }

  vtkSlicerAstroStatisticsLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroStatisticsModuleWidget::onCalculate : "
                  "vtkSlicerAstroStatisticsLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroStatisticsModuleWidget::onCalculate : "
                   "scene not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->parametersNode->SetStatus(1);

  if (!(strcmp(d->parametersNode->GetMode(), "Segmentation")))
    {
    if (!this->convertSelectedSegmentToLabelMap())
      {
      qCritical() <<"qSlicerAstroStatisticsModuleWidget::onCalculate : "
                    "convertSelectedSegmentToLabelMap failed!";
      d->parametersNode->SetStatus(0);
      return;
      }
    }

  // Run computation
  if (!logic->CalculateStatistics(d->parametersNode))
    {
    qCritical() <<"qSlicerAstroStatisticsModuleWidget::onCalculate : "
                  "CalculateStatistics error!";
    }

  if (!(strcmp(d->parametersNode->GetMode(), "Segmentation")))
    {
    vtkMRMLAstroLabelMapVolumeNode *maskVolume =
      vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(d->parametersNode->GetMaskVolumeNodeID()));
    if(maskVolume)
      {
      this->mrmlScene()->RemoveNode(maskVolume);
      }
    }

  d->parametersNode->SetStatus(0);
  d->OutputCollapsibleButton->setCollapsed(false);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  d->CancelButton->hide();
  d->progressBar->hide();
  d->ApplyButton->show();
}

//---------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

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
void qSlicerAstroStatisticsModuleWidget::onSumToggled(bool toggled)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetSum(toggled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onTableNodeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

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
  d->parametersNode->SetTableNode(tableNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onTotalFluxToggled(bool toggled)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetTotalFlux(toggled);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  d->parametersNode->SetStatus(-1);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  d->progressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroStatisticsModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroStatisticsModuleWidget);
  d->ApplyButton->hide();
  d->progressBar->show();
  d->CancelButton->show();
}

//---------------------------------------------------------------------------
vtkMRMLAstroStatisticsParametersNode* qSlicerAstroStatisticsModuleWidget::
mrmlAstroStatisticsParametersNode()const
{
  Q_D(const qSlicerAstroStatisticsModuleWidget);
  return d->parametersNode;
}

