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
#include <vtkFloatArray.h>
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

// AstroProfiles includes
#include "qSlicerAstroProfilesModuleWidget.h"
#include "ui_qSlicerAstroProfilesModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroProfilesLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>

// qMRML includes
#include <qMRMLSegmentsTableView.h>
#include <qMRMLSliceWidget.h>
#include <qMRMLTableView.h>

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroProfilesParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLDoubleArrayNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLPlotDataNode.h>
#include <vtkMRMLPlotViewNode.h>
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
/// \ingroup Slicer_QtModules_AstroProfiles
class qSlicerAstroProfilesModuleWidgetPrivate: public Ui_qSlicerAstroProfilesModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroProfilesModuleWidget);
protected:
  qSlicerAstroProfilesModuleWidget* const q_ptr;
public:

  qSlicerAstroProfilesModuleWidgetPrivate(qSlicerAstroProfilesModuleWidget& object);
  ~qSlicerAstroProfilesModuleWidgetPrivate();

  void init();
  void cleanPointers();

  vtkSlicerAstroProfilesLogic* logic() const;
  vtkSmartPointer<vtkMRMLAstroProfilesParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeProfile;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;
  vtkSmartPointer<vtkMRMLUnitNode> unitNodeIntensity;
  vtkSmartPointer<vtkMRMLUnitNode> unitNodeVelocity;
};

//-----------------------------------------------------------------------------
// qSlicerAstroProfilesModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModuleWidgetPrivate::qSlicerAstroProfilesModuleWidgetPrivate(qSlicerAstroProfilesModuleWidget& object)
  : q_ptr(&object)
{
  this->parametersNode = 0;
  this->selectionNode = 0;
  this->plotChartNodeProfile = 0;
  this->segmentEditorNode = 0;
  this->unitNodeIntensity = 0;
  this->unitNodeVelocity = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModuleWidgetPrivate::~qSlicerAstroProfilesModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroProfilesModuleWidget);

  this->setupUi(q);

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidgetPrivate::init(): could not find qSlicerApplication!";
    return;
    }

  QObject::connect(this->ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroProfilesParametersNode(vtkMRMLNode*)));

  QObject::connect(this->InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->ProfileVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onProfileVolumeChanged(vtkMRMLNode*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SegmentsTableView, SLOT(setMRMLScene(vtkMRMLScene*)));

  this->SegmentsTableView->setSelectionMode(QAbstractItemView::SingleSelection);

  QObject::connect(this->MaskCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onMaskActiveToggled(bool)));

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
void qSlicerAstroProfilesModuleWidgetPrivate::cleanPointers()
{
  Q_Q(const qSlicerAstroProfilesModuleWidget);

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

  if (this->plotChartNodeProfile)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeProfile);
    }
  this->plotChartNodeProfile = 0;
}

//-----------------------------------------------------------------------------
vtkSlicerAstroProfilesLogic* qSlicerAstroProfilesModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroProfilesModuleWidget);
  return vtkSlicerAstroProfilesLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroProfilesModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModuleWidget::qSlicerAstroProfilesModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroProfilesModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroProfilesModuleWidget::~qSlicerAstroProfilesModuleWidget()
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
void qSlicerAstroProfilesModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!scene)
    {
    return;
    }

  this->Superclass::setMRMLScene(scene);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::setMRMLScene : selectionNode not found!";
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
  this->onInputVolumeChanged(scene->GetNodeByID(d->selectionNode->GetActiveVolumeID()));
  this->onMRMLSelectionNodeReferenceAdded(d->selectionNode);
  this->onMRMLAstroProfilesParametersNodeModified();

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

void qSlicerAstroProfilesModuleWidget::initializeNodes(bool forceNew  /*= false*/)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  this->initializeParameterNode(forceNew);

  this->initializeSegmentations(forceNew);

  this->initializePlotNodes(forceNew);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onEndCloseEvent()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onMRMLSceneEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes(true);
  this->onMRMLAstroProfilesParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onEndImportEvent()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onMRMLSceneEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->initializeNodes();
  this->onMRMLAstroProfilesParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::initializeParameterNode(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!this->mrmlScene() || !d->selectionNode)
    {
    return;
    }

  vtkMRMLAstroProfilesParametersNode *astroParametersNode = NULL;
  unsigned int numNodes = this->mrmlScene()->GetNumberOfNodesByClass("vtkMRMLAstroProfilesParametersNode");
  if(numNodes > 0 && !forceNew)
    {
    astroParametersNode = vtkMRMLAstroProfilesParametersNode::SafeDownCast
      (this->mrmlScene()->GetNthNodeByClass(numNodes - 1, "vtkMRMLAstroProfilesParametersNode"));
    }
  else
    {
    vtkSmartPointer<vtkMRMLNode> parametersNode;
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLAstroProfilesParametersNode");
    parametersNode.TakeReference(foo);
    this->mrmlScene()->AddNode(parametersNode);

    astroParametersNode = vtkMRMLAstroProfilesParametersNode::SafeDownCast(parametersNode);
    int wasModifying = astroParametersNode->StartModify();
    astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
    astroParametersNode->SetMaskActive(false);
    astroParametersNode->EndModify(wasModifying);
    }

  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

void qSlicerAstroProfilesModuleWidget::initializePlotNodes(bool forceNew  /*= false*/)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  // Check (and create) PlotChart node
  if (!d->plotChartNodeProfile || forceNew)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeProfileCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "ProfileChart"));

    if (plotChartNodeProfileCol->GetNumberOfItems() == 0 || forceNew)
      {
      d->plotChartNodeProfile.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeProfile->SetName("ProfileChart");
      d->plotChartNodeProfile->SetAttribute("TitleName", "Profile");
      d->plotChartNodeProfile->SetAttribute("XAxisLabelName", "Velocity (km/s)");
      d->plotChartNodeProfile->SetAttribute("YAxisLabelName", "Total flux per channel (Jy)");
      d->plotChartNodeProfile->SetAttribute("Type", "Line");
      d->plotChartNodeProfile->SetAttribute("ClickAndDragAlongX", "off");
      d->plotChartNodeProfile->SetAttribute("ClickAndDragAlongY", "off");
      this->mrmlScene()->AddNode(d->plotChartNodeProfile);
      }
    else
      {
      d->plotChartNodeProfile = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeProfileCol->GetItemAsObject(plotChartNodeProfileCol->GetNumberOfItems() - 1));
      }
    }

  // Select NULL Chart
  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID(NULL);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  std::string segmentEditorSingletonTag = "SegmentEditor";
  vtkMRMLSegmentEditorNode *segmentEditorNodeSingleton = vtkMRMLSegmentEditorNode::SafeDownCast(
    this->mrmlScene()->GetSingletonNode(segmentEditorSingletonTag.c_str(), "vtkMRMLSegmentEditorNode"));

  if (!segmentEditorNodeSingleton )
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
bool qSlicerAstroProfilesModuleWidget::convertSelectedSegmentToLabelMap()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
    QString message = QString("No segments selected! Please provide a mask or unCheck the input"
                              " mask option.");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    return false;
    }

  // Export selected segments into a multi-label labelmap volume
  std::vector<std::string> segmentIDs;
  currentSegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIDs);

  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> labelMapNode;

  QStringList selectedSegmentIDs = d->SegmentsTableView->selectedSegmentIDs();

  if (selectedSegmentIDs.size() < 1)
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              "Be sure that segment to export has been selected in the table view (left click). \n\n").
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
    vtkSlicerAstroProfilesLogic* astroProfileslogic =
      vtkSlicerAstroProfilesLogic::SafeDownCast(this->logic());
    if (!astroProfileslogic)
      {
      qCritical() << Q_FUNC_INFO << ": astroProfileslogic not found!";
      return false;
      }
    vtkSlicerAstroVolumeLogic* astroVolumelogic =
      vtkSlicerAstroVolumeLogic::SafeDownCast(astroProfileslogic->GetAstroVolumeLogic());
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

  int Extents[6] = { 0, 0, 0, 0, 0, 0 };
  labelMapNode->GetImageData()->GetExtent(Extents);

  if (!vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode(currentSegmentationNode, segmentIDs, labelMapNode))
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              "Be sure that segment to export has been selected in the table view (left click). \n\n").
                              arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    this->mrmlScene()->RemoveNode(labelMapNode);
    return false;
    }

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

//--------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::setMRMLAstroProfilesParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroProfilesParametersNode* AstroProfilesParaNode =
      vtkMRMLAstroProfilesParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroProfilesParaNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLAstroProfilesParametersNodeModified()));

  d->parametersNode = AstroProfilesParaNode;

  this->onMRMLAstroProfilesParametersNodeModified();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
  appLogic->PropagateVolumeSelection();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onInputVolumeModified()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
    qCritical() << "qSlicerAstroProfilesModuleWidget::onInputVolumeModified :"
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
void qSlicerAstroProfilesModuleWidget::onMaskActiveToggled(bool active)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }
  d->parametersNode->SetMaskActive(active);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onMRMLAstroProfilesParametersNodeModified()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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

  char *ProfileVolumeNodeID = d->parametersNode->GetProfileVolumeNodeID();
  vtkMRMLAstroVolumeNode *ProfileVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(ProfileVolumeNodeID));
  if (ProfileVolumeNode)
    {
    d->ProfileVolumeNodeSelector->setCurrentNode(ProfileVolumeNode);
    }

  d->MaskCheckBox->setChecked(d->parametersNode->GetMaskActive());
  d->SegmentsTableView->setEnabled(d->parametersNode->GetMaskActive());
  if (d->parametersNode->GetMaskActive())
    {
    d->ParametersCollapsibleButton->setChecked(false);
    d->ParametersCollapsibleButton->setEnabled(false);
    }
  else
    {
    d->ParametersCollapsibleButton->setChecked(true);
    d->ParametersCollapsibleButton->setEnabled(true);
    }

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
void qSlicerAstroProfilesModuleWidget::onMRMLSelectionNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onCalculate()
{
  Q_D(const qSlicerAstroProfilesModuleWidget);

  vtkSlicerAstroProfilesLogic *logic = d->logic();
  if (!logic)
    {
    qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate"
                  " : vtkSlicerAstroProfilesLogic not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  if (!d->parametersNode)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onCalculate"
                   " : parametersNode not found!";
    d->parametersNode->SetStatus(0);
    return;
    }

  d->parametersNode->SetStatus(1);

  vtkMRMLScene *scene = this->mrmlScene();
  if(!scene)
    {
    qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate"
                  " : scene not found!";
    }

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
  if(!inputVolume)
    {
    qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate"
                  " : inputVolume not found!";
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
    QMessageBox::warning(NULL, tr("Failed to create the Profile"), message);
    d->parametersNode->SetStatus(0);
    return;
    }

  if (d->parametersNode->GetMaskActive())
    {
    if (!this->convertSelectedSegmentToLabelMap())
      {
      qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate : "
                    "convertSelectedSegmentToLabelMap failed!";
      d->parametersNode->SetStatus(0);
      return;
      }
    }

  int serial = d->parametersNode->GetOutputSerial();

  // Create 0thMomentMapVolume
  vtkMRMLAstroVolumeNode *ProfileVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetProfileVolumeNodeID()));

  if (!ProfileVolume)
    {
    ProfileVolume = vtkMRMLAstroVolumeNode::SafeDownCast(scene->
               GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));
    }

  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_profile";
  outSS <<"_"<< IntToString(serial);

  // Check Profile volume
  if (!strcmp(inputVolume->GetID(), ProfileVolume->GetID()) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3")) !=
      StringToInt(ProfileVolume->GetAttribute("SlicerAstro.NAXIS1"))))
    {

    // Get dimensions
    int N1 = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3"));

    // Create an empty 2D image
    vtkNew<vtkImageData> imageDataTemp;
    imageDataTemp->SetDimensions(N1, 1, 1);
    imageDataTemp->SetSpacing(1.,1.,1.);
    imageDataTemp->AllocateScalars(inputVolume->GetImageData()->GetScalarType(), 1);

    // create Astro Volume for the moment map
    ProfileVolume = vtkMRMLAstroVolumeNode::SafeDownCast
       (logic->GetAstroVolumeLogic()->CloneVolume(scene, inputVolume, outSS.str().c_str()));

    // modify fits attributes
    ProfileVolume->SetAttribute("SlicerAstro.NAXIS", "1");
    std::string Bunit = ProfileVolume->GetAttribute("SlicerAstro.BUNIT");
    Bunit += " km/s";
    ProfileVolume->SetAttribute("SlicerAstro.BUNIT", Bunit.c_str());
    std::string Btype = "";
    Btype = inputVolume->GetAstroVolumeDisplayNode()->AddVelocityInfoToDisplayStringZ(Btype);
    ProfileVolume->SetAttribute("SlicerAstro.BTYPE", Btype.c_str());
    ProfileVolume->SetAttribute("SlicerAstro.NAXIS1", inputVolume->GetAttribute("SlicerAstro.NAXIS3"));
    ProfileVolume->SetAttribute("SlicerAstro.CROTA1", inputVolume->GetAttribute("SlicerAstro.CROTA3"));
    ProfileVolume->SetAttribute("SlicerAstro.CRPIX1", inputVolume->GetAttribute("SlicerAstro.CRPIX3"));
    ProfileVolume->SetAttribute("SlicerAstro.CRVAL1", inputVolume->GetAttribute("SlicerAstro.CRVAL3"));
    ProfileVolume->SetAttribute("SlicerAstro.CTYPE1", inputVolume->GetAttribute("SlicerAstro.CTYPE3"));
    ProfileVolume->SetAttribute("SlicerAstro.CUNIT1", inputVolume->GetAttribute("SlicerAstro.CUNIT3"));
    ProfileVolume->SetAttribute("SlicerAstro.DTYPE1", inputVolume->GetAttribute("SlicerAstro.DTYPE3"));
    ProfileVolume->SetAttribute("SlicerAstro.DRVAL1", inputVolume->GetAttribute("SlicerAstro.DRVAL3"));
    ProfileVolume->SetAttribute("SlicerAstro.DUNIT1", inputVolume->GetAttribute("SlicerAstro.DUNIT3"));

    ProfileVolume->RemoveAttribute("SlicerAstro.NAXIS2");
    ProfileVolume->RemoveAttribute("SlicerAstro.CROTA2");
    ProfileVolume->RemoveAttribute("SlicerAstro.CRPIX2");
    ProfileVolume->RemoveAttribute("SlicerAstro.CRVAL2");
    ProfileVolume->RemoveAttribute("SlicerAstro.CTYPE2");
    ProfileVolume->RemoveAttribute("SlicerAstro.CUNIT2");
    ProfileVolume->RemoveAttribute("SlicerAstro.DTYPE2");
    ProfileVolume->RemoveAttribute("SlicerAstro.DRVAL2");
    ProfileVolume->RemoveAttribute("SlicerAstro.DUNIT2");
    ProfileVolume->RemoveAttribute("SlicerAstro.NAXIS3");
    ProfileVolume->RemoveAttribute("SlicerAstro.CROTA3");
    ProfileVolume->RemoveAttribute("SlicerAstro.CRPIX3");
    ProfileVolume->RemoveAttribute("SlicerAstro.CRVAL3");
    ProfileVolume->RemoveAttribute("SlicerAstro.CTYPE3");
    ProfileVolume->RemoveAttribute("SlicerAstro.CUNIT3");
    ProfileVolume->RemoveAttribute("SlicerAstro.DTYPE3");
    ProfileVolume->RemoveAttribute("SlicerAstro.DRVAL3");
    ProfileVolume->RemoveAttribute("SlicerAstro.DUNIT3");

    // copy 1D image into the Astro Volume object
    ProfileVolume->SetAndObserveImageData(imageDataTemp.GetPointer());

    double Origin[3];
    inputVolume->GetOrigin(Origin);
    Origin[1] = 0.;
    ProfileVolume->SetOrigin(Origin);

    ProfileVolume->SetName(outSS.str().c_str());
    d->parametersNode->SetProfileVolumeNodeID(ProfileVolume->GetID());

    // Remove old rendering Display
    int ndnodes = ProfileVolume->GetNumberOfDisplayNodes();
    for (int ii = 0; ii < ndnodes; ii++)
      {
      vtkMRMLVolumeRenderingDisplayNode *dnode =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
          ProfileVolume->GetNthDisplayNode(ii));
      if (dnode)
        {
        ProfileVolume->RemoveNthDisplayNodeID(ii);
        }
      }
    }
  else
    {
    ProfileVolume->SetName(outSS.str().c_str());
    d->parametersNode->SetProfileVolumeNodeID(ProfileVolume->GetID());
    }

  ProfileVolume->SetAttribute("SlicerAstro.DATAMODEL", "PROFILE");

  if (!logic->CalculateProfile(d->parametersNode))
    {
    qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate : "
                  "CalculateProfiles error!";

    scene->RemoveNode(ProfileVolume);
    d->parametersNode->SetStatus(0);
    d->parametersNode->SetProfileVolumeNodeID("");
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

  d->parametersNode->SetProfileVolumeNodeID("");

  // Setting the Layout for the Output
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onCalculate :"
                   " qSlicerApplication not found!";
    return;
    }

  qSlicerLayoutManager* layoutManager = app->layoutManager();

  if(!app)
    {
    qCritical() << "qSlicerAstroProfilesModuleWidget::onCalculate :"
                   " layoutManager not found!";
    return;
    }

  // Plot the profile
  layoutManager->layoutLogic()->GetLayoutNode()->SetViewArrangement(vtkMRMLLayoutNode::SlicerLayoutConventionalPlotView);

  vtkNew<vtkTable> table;
  vtkNew<vtkMRMLTableNode> tableNode;

  int wasModifying = tableNode->StartModify();

  std::string name = inputVolume->GetName();
  name += "_Profile_" + IntToString(serial);

  std::string nameTable = name;
  nameTable += "Table";
  tableNode->SetName(nameTable.c_str());
  tableNode->SetAndObserveTable(table.GetPointer());
  tableNode->RemoveAllColumns();
  tableNode->SetUseColumnNameAsColumnHeader(true);
  tableNode->SetDefaultColumnType("double");

  vtkDoubleArray* Velocity = vtkDoubleArray::SafeDownCast(tableNode->AddColumn());
  if (!Velocity)
    {
    qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate : "
                  "Unable to find the Velocity Column.";
    return;
    }
  Velocity->SetName("Velocity");
  tableNode->SetColumnUnitLabel("Velocity", "km/s");
  tableNode->SetColumnLongName("Velocity", "Velocity axes");

  vtkDoubleArray* Intensity = vtkDoubleArray::SafeDownCast(tableNode->AddColumn());
  if (!Intensity)
    {
    qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate : "
                  "Unable to find the Intensity Column.";
    return;
    }
  Intensity->SetName(name.c_str());
  tableNode->SetColumnUnitLabel(name.c_str(), "Jy");
  tableNode->SetColumnLongName(name.c_str(), "Intensity axes");

  table->SetNumberOfRows(StringToInt(ProfileVolume->GetAttribute("SlicerAstro.NAXIS1")));

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = inputVolume->GetAstroVolumeDisplayNode();
  if (!astroDisplay)
    {
    qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate :"
                  " astroDisplay not found!";
    return;
    }
  double ijk[3], world[3], VelFactor;
  struct wcsprm* WCS = astroDisplay->GetWCSStruct();
  if (!WCS)
    {
    qCritical() <<"qSlicerAstroProfilesModuleWidget::onCalculate :"
                  " WCS not found!";
    return;
    }
  if (!strcmp(WCS->cunit[2], "m/s"))
    {
    VelFactor = 0.001;
    }
  ijk[0] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) * 0.5;
  ijk[1] = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) * 0.5;

  for (int ii = 0; ii < StringToInt(ProfileVolume->GetAttribute("SlicerAstro.NAXIS1")); ii++)
     {
     ijk[2] = ii;
     astroDisplay->GetReferenceSpace(ijk, world);
     table->SetValue(ii, 0, world[2] * VelFactor);
     table->SetValue(ii, 1, ProfileVolume->GetImageData()->GetScalarComponentAsDouble(ii,0,0,0));
     }

  tableNode->EndModify(wasModifying);

  scene->AddNode(tableNode.GetPointer());
  if (d->selectionNode)
    {
    d->selectionNode->SetActiveTableID(tableNode->GetID());
    }

  vtkNew<vtkMRMLPlotDataNode> plotDataNode;
  plotDataNode->SetName(name.c_str());
  plotDataNode->SetAndObserveTableNodeID(tableNode->GetID());
  plotDataNode->SetXColumnName(tableNode->GetColumnName(0));
  plotDataNode->SetYColumnName(tableNode->GetColumnName(tableNode->GetNumberOfColumns() - 1));
  scene->AddNode(plotDataNode.GetPointer());

  if (d->plotChartNodeProfile)
    {
    d->plotChartNodeProfile->AddAndObservePlotDataNodeID(plotDataNode->GetID());
    }

  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID(d->plotChartNodeProfile->GetID());
    d->selectionNode->SetActiveTableID(tableNode->GetID());
    }

  serial++;
  d->parametersNode->SetOutputSerial(serial);
  d->parametersNode->SetStatus(0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->CancelButton->hide();
  d->progressBar->hide();
  d->ApplyButton->show();
}

//---------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onStartImportEvent()
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  d->cleanPointers();
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onThresholdRangeChanged(double min, double max)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onUnitNodeIntensityChanged(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onUnitNodeVelocityChanged(vtkObject *sender)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onVelocityRangeChanged(double min, double max)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

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
void qSlicerAstroProfilesModuleWidget::onProfileVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroProfilesModuleWidget);

  if (!d->parametersNode || !mrmlNode)
    {
    return;
    }

  d->parametersNode->SetProfileVolumeNodeID(mrmlNode->GetID());
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->parametersNode->SetStatus(-1);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->progressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroProfilesModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroProfilesModuleWidget);
  d->ApplyButton->hide();
  d->progressBar->show();
  d->CancelButton->show();
}

//---------------------------------------------------------------------------
vtkMRMLAstroProfilesParametersNode* qSlicerAstroProfilesModuleWidget::
mrmlAstroProfilesParametersNode()const
{
  Q_D(const qSlicerAstroProfilesModuleWidget);
  return d->parametersNode;
}

