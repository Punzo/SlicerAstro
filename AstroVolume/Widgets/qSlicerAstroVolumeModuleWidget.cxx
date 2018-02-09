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
#include <QtDebug>
#include <QMessageBox>
#include <QSettings>
#include <QString>
#include <QStringList>

// CTK includes
#include <ctkUtils.h>
#include <ctkVTKVolumePropertyWidget.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkImageHistogram.h>
#include <vtkImageReslice.h>
#include <vtkImageThreshold.h>
#include <vtkLookupTable.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPlotPoints.h>
#include <vtkPointData.h>
#include <vtkRenderer.h>
#include <vtkTable.h>
#include <vtkTransform.h>
#include <vtkVariant.h>

// qMRMLWidgets include
#include <qMRMLAstroVolumeInfoWidget.h>
#include <qMRMLPlotView.h>
#include <qMRMLPlotWidget.h>
#include <qMRMLThreeDViewControllerWidget.h>
#include <qMRMLThreeDWidget.h>
#include <qMRMLThreeDView.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerPresetComboBox.h>
#include <qSlicerPresetComboBox_p.h>
#include <qSlicerUtils.h>
#include <qSlicerVolumeRenderingModuleWidget.h>
#include <ui_qSlicerAstroVolumeModuleWidget.h>

// MRML includes
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLAstroLabelMapVolumeNode.h>
#include <vtkMRMLAstroLabelMapVolumeDisplayNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLColorTableStorageNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLPlotSeriesNode.h>
#include <vtkMRMLPlotChartNode.h>
#include <vtkMRMLProceduralColorNode.h>
#include <vtkMRMLProceduralColorStorageNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLTableNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLVolumePropertyNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

// logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerSegmentationsModuleLogic.h>
#include <vtkSlicerVolumeRenderingLogic.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroVolume
class qSlicerAstroVolumeModuleWidgetPrivate
  : public Ui_qSlicerAstroVolumeModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroVolumeModuleWidget);
protected:
  qSlicerAstroVolumeModuleWidget* const q_ptr;

public:
  qSlicerAstroVolumeModuleWidgetPrivate(qSlicerAstroVolumeModuleWidget& object);
  virtual ~qSlicerAstroVolumeModuleWidgetPrivate();

  virtual void setupUi(qSlicerAstroVolumeModuleWidget*);
  void cleanPointers();

  qSlicerVolumeRenderingModuleWidget* volumeRenderingWidget;
  qMRMLAstroVolumeInfoWidget *MRMLAstroVolumeInfoWidget;
  vtkSlicerSegmentationsModuleLogic* segmentationsLogic;
  vtkSmartPointer<vtkMRMLPlotChartNode> plotChartNodeHistogram;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeMinLine;
  vtkSmartPointer<vtkMRMLTableNode> TableMinNode;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeMaxLine;
  vtkSmartPointer<vtkMRMLTableNode> TableMaxNode;
  vtkSmartPointer<vtkMRMLPlotSeriesNode> PlotSeriesNodeThresholdLine;
  vtkSmartPointer<vtkMRMLTableNode> TableThresholdNode;
  vtkSmartPointer<vtkMRMLSegmentEditorNode> segmentEditorNode;  
  vtkSmartPointer<vtkMRMLAstroVolumeNode> astroVolumeNode;
  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> astroLabelVolumeNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;

  double stretchOldValue;
  double offsetOldValue;
  bool Lock;
};

//-----------------------------------------------------------------------------
// qSlicerVolumeRenderingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidgetPrivate::qSlicerAstroVolumeModuleWidgetPrivate(
  qSlicerAstroVolumeModuleWidget& object)
  : q_ptr(&object)
{
  this->MRMLAstroVolumeInfoWidget = 0;
  this->segmentationsLogic = 0;
  this->volumeRenderingWidget = 0;
  this->plotChartNodeHistogram = 0;
  this->astroVolumeNode = 0;
  this->astroLabelVolumeNode = 0;
  this->selectionNode = 0;
  this->segmentEditorNode = 0;
  this->PlotSeriesNodeMinLine = 0;
  this->TableMinNode = 0;
  this->PlotSeriesNodeMaxLine = 0;
  this->TableMaxNode = 0;
  this->PlotSeriesNodeThresholdLine = 0;
  this->TableThresholdNode = 0;
  this->stretchOldValue = 0.;
  this->offsetOldValue = 0.;
  this->Lock = false;
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidgetPrivate::~qSlicerAstroVolumeModuleWidgetPrivate()
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
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

//----------------------------------------------------------------------------
template <typename T> bool isNaN(T Value)
{
  return Value != Value;
}

//----------------------------------------------------------------------------
bool DoubleIsNaN(double Value)
{
  return isNaN<double>(Value);
}

//----------------------------------------------------------------------------
bool DoubleIsInf(double Value)
{
  return !DoubleIsNaN(Value) && DoubleIsNaN(Value - Value);
}

} // end namespace

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidgetPrivate::setupUi(qSlicerAstroVolumeModuleWidget* q)
{
  this->Ui_qSlicerAstroVolumeModuleWidget::setupUi(q); 

  this->MRMLAstroVolumeInfoWidget = new qMRMLAstroVolumeInfoWidget(InfoCollapsibleButton);
  this->MRMLAstroVolumeInfoWidget->setObjectName(QLatin1String("MRMLAstroVolumeInfoWidget"));

  this->verticalLayout->addWidget(MRMLAstroVolumeInfoWidget);

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this->MRMLAstroVolumeInfoWidget, SLOT(setVolumeNode(vtkMRMLNode*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->MRMLAstroVolumeInfoWidget, SLOT(setMRMLScene(vtkMRMLScene*)));

  qSlicerApplication* app = qSlicerApplication::application();

  if (!app)
    {
    qCritical() << Q_FUNC_INFO << ": qSlicerApplication not found, SlicerAstro could not initialize AstroVolume GUI!";
    return;
    }

  qSlicerAbstractCoreModule* volumeRendering = app->moduleManager()->module("VolumeRendering");

  if (!volumeRendering)
    {
    qCritical() << Q_FUNC_INFO << ": volumeRenderingModule not found, SlicerAstro could not initialize AstroVolume GUI!";
    return;
    }

  this->volumeRenderingWidget = dynamic_cast<qSlicerVolumeRenderingModuleWidget*>
    (volumeRendering->widgetRepresentation());

  if (!this->volumeRenderingWidget)
    {
    qCritical() << Q_FUNC_INFO << ": volumeRenderingModuleWidget not found, SlicerAstro could not initialize AstroVolume GUI!";
    return;
    }

  //Enable widgets
  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->InfoCollapsibleButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->WCSCollapsibleButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->HistoCollapsibleButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->DisplayCollapsibleButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->CollapsibleButton_CopyMoveSegment, SLOT(setEnabled(bool)));

  // WCS widget connections
  QObject::connect(this->OpticalVelocityButton, SIGNAL(clicked()),
                   q, SLOT(setOpticalVelocity()));

  QObject::connect(this->RadioVelocityButton, SIGNAL(clicked()),
                   q, SLOT(setRadioVelocity()));

  QObject::connect(this->DegreeUnitButton, SIGNAL(clicked()),
                   q, SLOT(setRADegreeUnit()));

  QObject::connect(this->SexagesimalUnitButton, SIGNAL(clicked()),
                   q, SLOT(setRASexagesimalUnit()));

  // Histogram widget connections
  QObject::connect(this->HistoPushButtonPreset1, SIGNAL(clicked()),
                   q, SLOT(onHistoClippingChanged1()));

  QObject::connect(this->HistoPushButtonPreset2, SIGNAL(clicked()),
                   q, SLOT(onHistoClippingChanged2()));

  QObject::connect(this->HistoPushButtonPreset3, SIGNAL(clicked()),
                   q, SLOT(onHistoClippingChanged3()));

  QObject::connect(this->HistoPushButtonPreset4, SIGNAL(clicked()),
                   q, SLOT(onHistoClippingChanged4()));

  QObject::connect(this->HistoPushButtonPreset5, SIGNAL(clicked()),
                   q, SLOT(onHistoClippingChanged5()));

  QObject::connect(this->CreateHistoPushButton, SIGNAL(clicked()),
                   q, SLOT(onCreateHistogram()));

  // 2D Display widget connections
  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this->AstroVolumeDisplayWidget, SLOT(setMRMLVolumeNode(vtkMRMLNode*)));

  // 3D Display widget connections
  QObject::connect(q, SIGNAL(deactivate3DLabelMapVolumeNode(bool)),
                   q, SLOT(onDeactivate3DLabelMapVolumeNode(bool)));

  QObject::connect(q, SIGNAL(activate3DAstroVolumeNode(bool)),
                   q, SLOT(onActivate3DAstroVolumeNode(bool)));

  QObject::connect(this->VisibilityCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onVisibilityChanged(bool)));

  QObject::connect(this->volumeRenderingWidget, SIGNAL(currentVolumeRenderingDisplayNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setDisplayConnection(vtkMRMLNode*)));

  QObject::connect(this->volumeRenderingWidget, SIGNAL(currentVolumeNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLVolumeNode(vtkMRMLNode*)));

  vtkSlicerVolumeRenderingLogic* volumeRenderingLogic =
    vtkSlicerVolumeRenderingLogic::SafeDownCast(volumeRendering->logic());
  std::map<std::string, std::string> methods =
    volumeRenderingLogic->GetRenderingMethods();
  std::map<std::string, std::string>::const_iterator it;
  for (it = methods.begin(); it != methods.end(); ++it)
    {
    this->RenderingMethodComboBox->addItem(
      QString::fromStdString(it->first), QString::fromStdString(it->second));
    }

  const char* defaultRenderingMethod = volumeRenderingLogic->GetDefaultRenderingMethod();
  if (defaultRenderingMethod == 0)
    {
    defaultRenderingMethod = "vtkMRMLCPURayCastVolumeRenderingDisplayNode";
    }
  int defaultRenderingMethodIndex = this->RenderingMethodComboBox->findData(
    QString(defaultRenderingMethod));
  this->RenderingMethodComboBox->setCurrentIndex(defaultRenderingMethodIndex);

  QObject::connect(this->RenderingMethodComboBox, SIGNAL(currentIndexChanged(int)),
                   this->volumeRenderingWidget, SLOT(onCurrentRenderingMethodChanged(int)));

  QObject::connect(this->QualityControlComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onCurrentQualityControlChanged(int)));

  QObject::connect(this->PresetsNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(resetOffset(vtkMRMLNode*)));

  QObject::connect(this->PresetsNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(resetStretch(vtkMRMLNode*)));

  QObject::connect(this->PresetsNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onPresetsNodeChanged(vtkMRMLNode*)));

  QObject::connect(this->PresetsNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(applyPreset(vtkMRMLNode*)));

  QObject::connect(this->PresetOffsetSlider, SIGNAL(valueChanged(double)),
                   q, SLOT(offsetPreset(double)));

  QObject::connect(this->PresetOffsetSlider, SIGNAL(valueChanged(double)),
                   this->volumeRenderingWidget, SLOT(interaction()));

  QObject::connect(this->PresetStretchSlider, SIGNAL(valueChanged(double)),
                   q, SLOT(spreadPreset(double)));

  QObject::connect(this->PresetStretchSlider, SIGNAL(valueChanged(double)),
                   this->volumeRenderingWidget, SLOT(interaction()));

  QObject::connect(this->LockPushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onLockToggled(bool)));

  QObject::connect(this->ROICropDisplayCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(setDisplayROIEnabled(bool)));

  QObject::connect(this->ROICropDisplayCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onROICropDisplayCheckBoxToggled(bool)));

  QObject::connect(this->ROICropCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onCropToggled(bool)));

  QObject::connect(this->ROIFitPushButton, SIGNAL(clicked()),
                   this->volumeRenderingWidget, SLOT(fitROIToVolume()));

  QObject::connect(this->SynchronizeScalarDisplayNodeButton, SIGNAL(clicked()),
                   this->volumeRenderingWidget, SLOT(synchronizeScalarDisplayNode()));

  QObject::connect(this->SynchronizeScalarDisplayNodeButton, SIGNAL(clicked()),
                   q, SLOT(clearPresets()));

  QObject::connect(this->DisplayThresholdSliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onDisplayThresholdValueChanged(double)));

  QObject::connect(this->CalculateRMSPushButton, SIGNAL(clicked()),
                   q, SLOT(onCalculateRMS()));

  // Segmentations widget connections
  qSlicerAbstractCoreModule* segmentations= app->moduleManager()->module("Segmentations");
  if (segmentations)
    {
    this->segmentationsLogic = vtkSlicerSegmentationsModuleLogic::SafeDownCast(segmentations->logic());
    }

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SegmentsTableView_2, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(this->pushButtonCovertLabelMapToSegmentation, SIGNAL(clicked()),
                   q, SLOT(onPushButtonCovertLabelMapToSegmentationClicked()));

  QObject::connect(this->pushButtonConvertSegmentationToLabelMap, SIGNAL(clicked()),
                   q, SLOT(onPushButtonConvertSegmentationToLabelMapClicked()));

  QObject::connect(this->CreateSurfaceButton, SIGNAL(toggled(bool)),
                   q, SLOT(onCreateSurfaceButtonToggled(bool)));

  QObject::connect(this->pushButton_EditSelected, SIGNAL(clicked()),
                   q, SLOT(onEditSelectedSegment()));

  this->SegmentsTableView_2->setSelectionMode(QAbstractItemView::SingleSelection);
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidgetPrivate::cleanPointers()
{
  Q_Q(qSlicerAstroVolumeModuleWidget);

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

  this->segmentEditorNode = 0;

  if (this->SegmentsTableView_2)
    {
    if (this->SegmentsTableView_2->segmentationNode())
      {
      q->mrmlScene()->RemoveNode(this->SegmentsTableView_2->segmentationNode());
      this->SegmentsTableView_2->setSegmentationNode(NULL);
      }
    }

  if (this->plotChartNodeHistogram)
    {
    q->mrmlScene()->RemoveNode(this->plotChartNodeHistogram);
    }
  this->plotChartNodeHistogram = 0;

  if (this->PlotSeriesNodeMinLine)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeMinLine);
    }
  this->PlotSeriesNodeMinLine = 0;

  if (this->TableMinNode)
    {
    q->mrmlScene()->RemoveNode(this->TableMinNode);
    }
  this->TableMinNode = 0;

  if (this->PlotSeriesNodeMaxLine)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeMaxLine);
    }
  this->PlotSeriesNodeMaxLine = 0;

  if (this->TableMaxNode)
    {
    q->mrmlScene()->RemoveNode(this->TableMaxNode);
    }
  this->TableMaxNode = 0;

  if (this->PlotSeriesNodeThresholdLine)
    {
    q->mrmlScene()->RemoveNode(this->PlotSeriesNodeThresholdLine);
    }
  this->PlotSeriesNodeThresholdLine = 0;

  if (this->TableThresholdNode)
    {
    q->mrmlScene()->RemoveNode(this->TableThresholdNode);
    }
  this->TableThresholdNode = 0;

  if (this->astroVolumeNode)
    {
    q->mrmlScene()->RemoveNode(this->astroVolumeNode);
    }
  this->astroVolumeNode = 0;

  if (this->astroLabelVolumeNode)
    {
    q->mrmlScene()->RemoveNode(this->astroLabelVolumeNode);
    }
  this->astroLabelVolumeNode = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidget::qSlicerAstroVolumeModuleWidget(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerAstroVolumeModuleWidgetPrivate(*this))
{
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidget::~qSlicerAstroVolumeModuleWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  this->Superclass::setMRMLScene(scene);

  this->qvtkReconnect(scene, vtkMRMLScene::StartImportEvent,
                      this, SLOT(onMRMLSceneStartImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndImportEvent,
                      this, SLOT(onMRMLSceneEndImportEvent()));
  this->qvtkReconnect(scene, vtkMRMLScene::EndCloseEvent,
                      this, SLOT(onMRMLSceneEndCloseEvent()));

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setMRMLScene : selectionNode not found!";
    return;
    }

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceAddedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceAdded(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceRemovedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceRemoved(vtkObject*)));
  this->onMRMLSelectionNodeModified(d->selectionNode);
  this->onInputVolumeChanged(scene->GetNodeByID(d->selectionNode->GetActiveVolumeID()));
  this->onMRMLSelectionNodeReferenceAdded(d->selectionNode);

  this->initializeSegmentations();
  this->initializePlotNodes();
  this->initializeColorNodes();

  if(!d->PresetsNodeComboBox)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setMRMLScene error :"
                   " PresetsNodeComboBox not found!"<<endl;
    return;
    }

  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  if (!astroVolumeLogic)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setMRMLScene error :"
                     " astroVolumeLogic not found!"<<endl;
    return;
    }
  vtkMRMLScene *presetsScene = astroVolumeLogic->GetPresetsScene();
  d->PresetsNodeComboBox->setMRMLScene(presetsScene);
  d->PresetsNodeComboBox->setCurrentNodeIndex(-1);
  this->onMRMLVolumeNodeDisplayThresholdModified(true);
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::initializeSegmentations(bool forceNew /*= false*/)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

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
void qSlicerAstroVolumeModuleWidget::initializeColorNodes()
{
  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::initializeColorNodes error :"
                     " scene not found!"<<endl;
    return;
    }

  // Add Astro 2D color functions
  vtkSmartPointer<vtkCollection> colorNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClassByName("vtkMRMLColorTableNode","Heat"));
  if (colorNodes->GetNumberOfItems() == 0)
    {
    vtkNew<vtkMRMLColorTableNode> HeatColorTableNode;
    HeatColorTableNode->SetType(vtkMRMLColorTableNode::User);
    HeatColorTableNode->SetName("Heat");
    HeatColorTableNode->SetDescription("A scale from red to yellow.");
    HeatColorTableNode->SetNumberOfColors(256);

    // Red component
    vtkNew<vtkLookupTable> RedLookupTable;
    RedLookupTable->SetNumberOfTableValues(85);
    RedLookupTable->SetTableRange(0, 85);
    RedLookupTable->SetHueRange(0, 0);
    RedLookupTable->SetSaturationRange(1,1);
    RedLookupTable->SetValueRange(0.,1);
    RedLookupTable->SetRampToLinear();
    RedLookupTable->ForceBuild();

    // Green component
    vtkNew<vtkLookupTable> GreenLookupTable;
    GreenLookupTable->SetNumberOfTableValues(256);
    GreenLookupTable->SetTableRange(0, 256);
    GreenLookupTable->SetHueRange(0.333, 0.333);
    GreenLookupTable->SetSaturationRange(1,1);
    GreenLookupTable->SetValueRange(0.,1);
    GreenLookupTable->SetRampToLinear();
    GreenLookupTable->ForceBuild();

    // Blue component
    vtkNew<vtkLookupTable> BlueLookupTable;
    BlueLookupTable->SetNumberOfTableValues(85);
    BlueLookupTable->SetTableRange(0, 85);
    BlueLookupTable->SetHueRange(0.667, 0.667);
    BlueLookupTable->SetSaturationRange(1,1);
    BlueLookupTable->SetValueRange(0,1);
    BlueLookupTable->SetRampToLinear();
    BlueLookupTable->ForceBuild();

    for (int ii = 0; ii < 85; ii++)
      {
      double RGBRed[3], RGBGreen[3], RGBA[4];
      RedLookupTable->GetTableValue(ii, RGBRed);
      GreenLookupTable->GetTableValue(ii, RGBGreen);

      RGBA[0] = RGBRed[0];
      RGBA[1] = RGBGreen[1];
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      HeatColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 85; ii < 171; ii++)
      {
      double RGBGreen[3], RGBA[4];
      GreenLookupTable->GetTableValue(ii, RGBGreen);

      RGBA[0] = 1.;
      RGBA[1] = RGBGreen[1];
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      HeatColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 171; ii < 256; ii++)
      {
      double RGBGreen[3], RGBBlue[3], RGBA[4];
      GreenLookupTable->GetTableValue(ii, RGBGreen);
      BlueLookupTable->GetTableValue(ii - 171, RGBBlue);

      RGBA[0] = 1.;
      RGBA[1] = RGBGreen[1];
      RGBA[2] = RGBBlue[2];
      RGBA[3] = 1.;
      HeatColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }

    HeatColorTableNode->SetNamesFromColors();
    this->mrmlScene()->AddNode(HeatColorTableNode.GetPointer());
    }

  colorNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClassByName("vtkMRMLColorTableNode","Ronekers"));
  if (colorNodes->GetNumberOfItems() == 0)
    {
    vtkNew<vtkMRMLColorTableNode> RonekersColorTableNode;
    RonekersColorTableNode->SetType(vtkMRMLColorTableNode::User);
    RonekersColorTableNode->SetName("Ronekers");
    RonekersColorTableNode->SetDescription("Discrete rainbow color function. Very useful to visualize for Astro HI datasets.");
    RonekersColorTableNode->SetNumberOfColors(256);

    for (int ii = 0; ii < 28; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.199;
      RGBA[1] = 0.199;
      RGBA[2] = 0.199;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 28; ii < 53; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.473;
      RGBA[1] = 0.;
      RGBA[2] = 0.606;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 53; ii < 78; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.;
      RGBA[1] = 0.;
      RGBA[2] = 0.781;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 78; ii < 103; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.371;
      RGBA[1] = 0.652;
      RGBA[2] = 0.922;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 103; ii < 128; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.;
      RGBA[1] = 0.566;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 128; ii < 153; ii++)
      {
      double RGBA[4];

      RGBA[0] = 0.;
      RGBA[1] = 0.961;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 153; ii < 178; ii++)
      {
      double RGBA[4];

      RGBA[0] = 1.;
      RGBA[1] = 1.;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 178; ii < 203; ii++)
      {
      double RGBA[4];

      RGBA[0] = 1.;
      RGBA[1] = 0.691;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 203; ii < 228; ii++)
      {
      double RGBA[4];

      RGBA[0] = 1.;
      RGBA[1] = 0.;
      RGBA[2] = 0.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 228; ii < 256; ii++)
      {
      double RGBA[4];

      RGBA[0] = 1.;
      RGBA[1] = 1.;
      RGBA[2] = 1.;
      RGBA[3] = 1.;
      RonekersColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }

    RonekersColorTableNode->SetNamesFromColors();
    this->mrmlScene()->AddNode(RonekersColorTableNode.GetPointer());
    }

  colorNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClassByName("vtkMRMLColorTableNode","VelocityField"));
  if (colorNodes->GetNumberOfItems() == 0)
    {
    vtkNew<vtkMRMLColorTableNode> VelocityFieldColorTableNode;
    VelocityFieldColorTableNode->SetType(vtkMRMLColorTableNode::User);
    VelocityFieldColorTableNode->SetName("VelocityField");
    VelocityFieldColorTableNode->SetDescription("A scale from blue to red.");
    VelocityFieldColorTableNode->SetNumberOfColors(256);

    // Red component
    vtkNew<vtkLookupTable> RedLookupTable;
    RedLookupTable->SetNumberOfTableValues(128);
    RedLookupTable->SetTableRange(0, 128);
    RedLookupTable->SetHueRange(0, 0);
    RedLookupTable->SetSaturationRange(1,1);
    RedLookupTable->SetValueRange(0.,1);
    RedLookupTable->SetRampToLinear();
    RedLookupTable->ForceBuild();

    // Green component
    vtkNew<vtkLookupTable> GreenLookupTable;
    GreenLookupTable->SetNumberOfTableValues(128);
    GreenLookupTable->SetTableRange(0, 128);
    GreenLookupTable->SetHueRange(0.333, 0.333);
    GreenLookupTable->SetSaturationRange(1,1);
    GreenLookupTable->SetValueRange(0.,1);
    GreenLookupTable->SetRampToLinear();
    GreenLookupTable->ForceBuild();

    // Blue component
    vtkNew<vtkLookupTable> BlueLookupTable;
    BlueLookupTable->SetNumberOfTableValues(128);
    BlueLookupTable->SetTableRange(0, 128);
    BlueLookupTable->SetHueRange(0.667, 0.667);
    BlueLookupTable->SetSaturationRange(1,1);
    BlueLookupTable->SetValueRange(0,1);
    BlueLookupTable->SetRampToLinear();
    BlueLookupTable->ForceBuild();

    VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(0, 0, 0, 0);
    for (int ii = 1; ii < 128; ii++)
      {
      double RGBBlue[3], RGBGreen[3], RGBA[4];
      int blueIndex = 128 - ii;
      BlueLookupTable->GetTableValue(blueIndex, RGBBlue);
      GreenLookupTable->GetTableValue(ii, RGBGreen);

      RGBA[0] = 0.;
      RGBA[1] = RGBGreen[1];
      RGBA[2] = RGBBlue[2];
      RGBA[3] = 1.;

      VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    for (int ii = 128; ii < 255; ii++)
      {
      double RGBGreen[3], RGBRed[3], RGBA[4];
      int redIndex = ii - 128;
      int greeIndex = 128 - (redIndex);
      GreenLookupTable->GetTableValue(greeIndex, RGBGreen);
      RedLookupTable->GetTableValue(redIndex, RGBRed);

      RGBA[0] = RGBRed[0];
      RGBA[1] = RGBGreen[1];
      RGBA[2] = 0.;
      RGBA[3] = 1.;

      VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(ii, RGBA);
      }
    VelocityFieldColorTableNode->GetLookupTable()->SetTableValue(255, 0, 0, 0);
    VelocityFieldColorTableNode->SetNamesFromColors();
    this->mrmlScene()->AddNode(VelocityFieldColorTableNode.GetPointer());
    }

  // Remove unwanted 2D color functions
  vtkSmartPointer<vtkCollection> ColorTableNodeCol =
    vtkSmartPointer<vtkCollection>::Take(
      this->mrmlScene()->GetNodesByClass("vtkMRMLColorTableNode"));
  for (int ii = 0; ii < ColorTableNodeCol->GetNumberOfItems(); ii++)
    {
    vtkMRMLColorTableNode* tempColorTableNode = vtkMRMLColorTableNode::SafeDownCast
            (ColorTableNodeCol->GetItemAsObject(ii));
    if (!tempColorTableNode)
      {
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "Grey"))
      {
      tempColorTableNode->SetTypeToBlue();
      tempColorTableNode->SetTypeToGrey();
      }
    if (!strcmp(tempColorTableNode->GetName(), "Grey") ||
        !strcmp(tempColorTableNode->GetName(), "Heat") ||
        !strcmp(tempColorTableNode->GetName(), "Ronekers") ||
        !strcmp(tempColorTableNode->GetName(), "VelocityField"))
      {
      tempColorTableNode->SetAttribute("SlicerAstro.AddFunctions", "on");
      tempColorTableNode->SetAttribute("SlicerAstro.Reverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Inverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Log", "off");
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "GenericColors") ||
        !strcmp(tempColorTableNode->GetName(), "DarkBrightChartColors") ||
        !strcmp(tempColorTableNode->GetName(), "Random"))
      {
      continue;
      }

    vtkMRMLColorTableStorageNode* tempColorTableStorageNode =
      vtkMRMLColorTableStorageNode::SafeDownCast
        (tempColorTableNode->GetStorageNode());
    this->mrmlScene()->RemoveNode(tempColorTableStorageNode);
    this->mrmlScene()->RemoveNode(tempColorTableNode);
    }

  vtkSmartPointer<vtkCollection> ProceduralColorTableNodeCol =
    vtkSmartPointer<vtkCollection>::Take(
      this->mrmlScene()->GetNodesByClass("vtkMRMLProceduralColorNode"));
  for (int ii = 0; ii < ProceduralColorTableNodeCol->GetNumberOfItems(); ii++)
    {
    vtkMRMLProceduralColorNode* tempProceduralColorTableNode = vtkMRMLProceduralColorNode::SafeDownCast
            (ProceduralColorTableNodeCol->GetItemAsObject(ii));
    if (!tempProceduralColorTableNode)
      {
      continue;
      }

    vtkMRMLProceduralColorStorageNode* tempProceduralColorTableStorageNode =
      vtkMRMLProceduralColorStorageNode::SafeDownCast
        (tempProceduralColorTableNode->GetStorageNode());
    this->mrmlScene()->RemoveNode(tempProceduralColorTableStorageNode);
    this->mrmlScene()->RemoveNode(tempProceduralColorTableNode);
    }

  this->mrmlScene()->RemoveUnusedNodeReferences();

  colorNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClassByName("vtkMRMLColorTableNode","Rainbow"));
  if (colorNodes->GetNumberOfItems() == 0)
    {
    // Add Rainbow
    vtkNew<vtkMRMLColorTableNode> RainbowTableNode;
    RainbowTableNode->SetName("Rainbow");
    RainbowTableNode->SetType(vtkMRMLColorTableNode::User);
    RainbowTableNode->SetDescription("Goes from red to purple, passing through the colors of the rainbow in between.");
    RainbowTableNode->SetNumberOfColors(256);
    RainbowTableNode->GetLookupTable()->SetNumberOfTableValues(256);
    RainbowTableNode->GetLookupTable()->SetHueRange(0., 0.8);
    RainbowTableNode->GetLookupTable()->SetSaturationRange(1,1);
    RainbowTableNode->GetLookupTable()->SetValueRange(1,1);
    RainbowTableNode->GetLookupTable()->SetRampToLinear();
    RainbowTableNode->GetLookupTable()->ForceBuild();
    RainbowTableNode->SetColor(0, 0, 0, 0);
    RainbowTableNode->SetColor(255, 0, 0, 0);
    RainbowTableNode->SetNamesFromColors();
    RainbowTableNode->SetAttribute("SlicerAstro.AddFunctions", "on");
    RainbowTableNode->SetAttribute("SlicerAstro.Reverse", "off");
    RainbowTableNode->SetAttribute("SlicerAstro.Inverse", "off");
    RainbowTableNode->SetAttribute("SlicerAstro.Log", "off");
    this->mrmlScene()->AddNode(RainbowTableNode.GetPointer());
    }

  // ReAdd Generic, Random and DarkBrightChart Colors
  ColorTableNodeCol = vtkSmartPointer<vtkCollection>::Take(
      this->mrmlScene()->GetNodesByClass("vtkMRMLColorTableNode"));
  for (int ii = 0; ii < ColorTableNodeCol->GetNumberOfItems(); ii++)
    {
    vtkMRMLColorTableNode* tempColorTableNode = vtkMRMLColorTableNode::SafeDownCast
            (ColorTableNodeCol->GetItemAsObject(ii));
    if (!tempColorTableNode)
      {
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "GenericColors") ||
        !strcmp(tempColorTableNode->GetName(), "DarkBrightChartColors") ||
        !strcmp(tempColorTableNode->GetName(), "Random"))
      {
      vtkNew<vtkMRMLColorTableNode> tempTableNode;
      tempTableNode->Copy(tempColorTableNode);
      this->mrmlScene()->RemoveNode(tempColorTableNode);
      tempTableNode->SetAttribute("SlicerAstro.AddFunctions", "off");
      if (!strcmp(tempColorTableNode->GetName(), "Random"))
        {
        tempTableNode->GetLookupTable()->SetTableValue(0, 0.047, 0.063, 0.635);
        tempTableNode->SetNamesFromColors();
        }
      this->mrmlScene()->AddNode(tempTableNode.GetPointer());
      continue;
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::initializePlotNodes(bool forceNew  /*= false*/)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  // Check (and create) PlotChart node
  if (!d->plotChartNodeHistogram || forceNew)
    {
    vtkSmartPointer<vtkCollection> plotChartNodeHistogramCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotChartNode", "HistogramChart"));

    if (plotChartNodeHistogramCol->GetNumberOfItems() == 0)
      {
      d->plotChartNodeHistogram.TakeReference(vtkMRMLPlotChartNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotChartNode")));
      d->plotChartNodeHistogram->SetName("HistogramChart");
      d->plotChartNodeHistogram->SetTitle("Histogram");
      d->plotChartNodeHistogram->SetXAxisTitle("Intensity (Jy/beam)");
      d->plotChartNodeHistogram->SetYAxisTitle("Log10(#)");
      d->plotChartNodeHistogram->SetEnablePointMoveAlongX(true);
      d->plotChartNodeHistogram->SetEnablePointMoveAlongY(false);
      this->mrmlScene()->AddNode(d->plotChartNodeHistogram);
      }
    else
      {
      d->plotChartNodeHistogram = vtkMRMLPlotChartNode::SafeDownCast
        (plotChartNodeHistogramCol->GetItemAsObject(0));
      }
    }

  // Select NULL Chart
  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID(NULL);
    }

  // Check (and create) TableMinNode
  if (!d->TableMinNode || forceNew)
    {
    vtkSmartPointer<vtkCollection> TableMinNodeCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLTableNode", "MinTable"));

    if (TableMinNodeCol->GetNumberOfItems() == 0)
      {
      d->TableMinNode.TakeReference(vtkMRMLTableNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLTableNode")));
      d->TableMinNode->SetName("MinTable");
      d->TableMinNode->RemoveAllColumns();
      d->TableMinNode->SetUseColumnNameAsColumnHeader(true);
      d->TableMinNode->SetDefaultColumnType("double");

      vtkDoubleArray* xAxis = vtkDoubleArray::SafeDownCast(d->TableMinNode->AddColumn());
      if (!xAxis)
        {
        qCritical() <<"qSlicerAstroVolumeModuleWidget::initializePlotNodes : "
                      "Unable to find the xAxis Column.";
        return;
        }
      xAxis->SetName("xAxis");

      vtkDoubleArray* yAxis = vtkDoubleArray::SafeDownCast(d->TableMinNode->AddColumn());
      if (!yAxis)
        {
        qCritical() <<"qSlicerAstroVolumeModuleWidget::initializePlotNodes : "
                      "Unable to find the yAxis Column.";
        return;
        }
      yAxis->SetName("Min");

      d->TableMinNode->GetTable()->SetNumberOfRows(3);
      for (int ii = 0; ii < 3; ii++)
        {
        d->TableMinNode->GetTable()->SetValue(ii, 0, 0.);
        d->TableMinNode->GetTable()->SetValue(ii, 1, 0.);
        }
      this->mrmlScene()->AddNode(d->TableMinNode);
      }
    else
      {
      d->TableMinNode = vtkMRMLTableNode::SafeDownCast
        (TableMinNodeCol->GetItemAsObject(0));
      }
    }

  this->qvtkReconnect(d->TableMinNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLTableMinNodeModified()));

  // Check (and create) PlotSeriesNodeMinLine
  if (!d->PlotSeriesNodeMinLine || forceNew)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeMinLineCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "MinLine"));

    if (PlotSeriesNodeMinLineCol->GetNumberOfItems() == 0)
      {
      d->PlotSeriesNodeMinLine.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      d->PlotSeriesNodeMinLine->SetName("MinLine");
      d->PlotSeriesNodeMinLine->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeMinLine->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeMinLine->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeMinLine->SetMarkerSize(11);
      d->PlotSeriesNodeMinLine->SetLineWidth(4);
      double color[4] = {0.071, 0.545, 0.290, 1.};
      d->PlotSeriesNodeMinLine->SetColor(color);
      d->PlotSeriesNodeMinLine->SetAndObserveTableNodeID(d->TableMinNode->GetID());
      d->PlotSeriesNodeMinLine->SetXColumnName(d->TableMinNode->GetColumnName(0));
      d->PlotSeriesNodeMinLine->SetYColumnName(d->TableMinNode->GetColumnName(1));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeMinLine);
      }
    else
      {
      d->PlotSeriesNodeMinLine = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeMinLineCol->GetItemAsObject(0));
      }
    }

  // Check (and create) TableMaxNode
  if (!d->TableMaxNode || forceNew)
    {
    vtkSmartPointer<vtkCollection> TableMaxNodeCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLTableNode", "MaxTable"));

    if (TableMaxNodeCol->GetNumberOfItems() == 0)
      {
      d->TableMaxNode.TakeReference(vtkMRMLTableNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLTableNode")));
      d->TableMaxNode->SetName("MaxTable");
      d->TableMaxNode->RemoveAllColumns();
      d->TableMaxNode->SetUseColumnNameAsColumnHeader(true);
      d->TableMaxNode->SetDefaultColumnType("double");

      vtkDoubleArray* xAxis = vtkDoubleArray::SafeDownCast(d->TableMaxNode->AddColumn());
      if (!xAxis)
        {
        qCritical() <<"qSlicerAstroVolumeModuleWidget::initializePlotNodes : "
                      "Unable to find the xAxis Column.";
        return;
        }
      xAxis->SetName("xAxis");

      vtkDoubleArray* yAxis = vtkDoubleArray::SafeDownCast(d->TableMaxNode->AddColumn());
      if (!yAxis)
        {
        qCritical() <<"qSlicerAstroVolumeModuleWidget::initializePlotNodes : "
                      "Unable to find the yAxis Column.";
        return;
        }
      yAxis->SetName("Max");

      d->TableMaxNode->GetTable()->SetNumberOfRows(3);
      for (int ii = 0; ii < 3; ii++)
        {
        d->TableMaxNode->GetTable()->SetValue(ii, 0, 0.);
        d->TableMaxNode->GetTable()->SetValue(ii, 1, 0.);
        }
      this->mrmlScene()->AddNode(d->TableMaxNode);
      }
    else
      {
      d->TableMaxNode = vtkMRMLTableNode::SafeDownCast
        (TableMaxNodeCol->GetItemAsObject(0));
      }
    }

  this->qvtkReconnect(d->TableMaxNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLTableMaxNodeModified()));

  // Check (and create) PlotSeriesNodeMaxLine
  if (!d->PlotSeriesNodeMaxLine || forceNew)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeMaxLineCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "MaxLine"));

    if (PlotSeriesNodeMaxLineCol->GetNumberOfItems() == 0)
      {
      d->PlotSeriesNodeMaxLine.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      d->PlotSeriesNodeMaxLine->SetName("MaxLine");
      d->PlotSeriesNodeMaxLine->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeMaxLine->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeMaxLine->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeMaxLine->SetMarkerSize(11);
      d->PlotSeriesNodeMaxLine->SetLineWidth(4);
      double color[4] = {0.086, 0.365, 0.655, 1.};
      d->PlotSeriesNodeMaxLine->SetColor(color);
      d->PlotSeriesNodeMaxLine->SetAndObserveTableNodeID(d->TableMaxNode->GetID());
      d->PlotSeriesNodeMaxLine->SetXColumnName(d->TableMaxNode->GetColumnName(0));
      d->PlotSeriesNodeMaxLine->SetYColumnName(d->TableMaxNode->GetColumnName(1));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeMaxLine);
      }
    else
      {
      d->PlotSeriesNodeMaxLine = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeMaxLineCol->GetItemAsObject(0));
      }
    }

  // Check (and create) TableThresholdNode
  if (!d->TableThresholdNode || forceNew)
    {
    vtkSmartPointer<vtkCollection> TableThresholdNodeCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLTableNode", "ThresholdTable"));

    if (TableThresholdNodeCol->GetNumberOfItems() == 0)
      {
      d->TableThresholdNode.TakeReference(vtkMRMLTableNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLTableNode")));
      d->TableThresholdNode->SetName("ThresholdTable");
      d->TableThresholdNode->RemoveAllColumns();
      d->TableThresholdNode->SetUseColumnNameAsColumnHeader(true);
      d->TableThresholdNode->SetDefaultColumnType("double");

      vtkDoubleArray* xAxis = vtkDoubleArray::SafeDownCast(d->TableThresholdNode->AddColumn());
      if (!xAxis)
        {
        qCritical() <<"qSlicerAstroVolumeModuleWidget::initializePlotNodes : "
                      "Unable to find the xAxis Column.";
        return;
        }
      xAxis->SetName("xAxis");

      vtkDoubleArray* yAxis = vtkDoubleArray::SafeDownCast(d->TableThresholdNode->AddColumn());
      if (!yAxis)
        {
        qCritical() <<"qSlicerAstroVolumeModuleWidget::initializePlotNodes : "
                      "Unable to find the yAxis Column.";
        return;
        }
      yAxis->SetName("3DDisplayThreshold");

      d->TableThresholdNode->GetTable()->SetNumberOfRows(3);
      for (int ii = 0; ii < 3; ii++)
        {
        d->TableThresholdNode->GetTable()->SetValue(ii, 0, 0.);
        d->TableThresholdNode->GetTable()->SetValue(ii, 1, 0.);
        }
      this->mrmlScene()->AddNode(d->TableThresholdNode);
      }
    else
      {
      d->TableThresholdNode = vtkMRMLTableNode::SafeDownCast
        (TableThresholdNodeCol->GetItemAsObject(0));
      }
    }

  this->qvtkReconnect(d->TableThresholdNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLTableThresholdNodeModified()));

  // Check (and create) PlotSeriesNodeThresholdLine
  if (!d->PlotSeriesNodeThresholdLine || forceNew)
    {
    vtkSmartPointer<vtkCollection> PlotSeriesNodeThresholdLineCol =
      vtkSmartPointer<vtkCollection>::Take(
        this->mrmlScene()->GetNodesByClassByName("vtkMRMLPlotSeriesNode", "ThresholdLine"));

    if (PlotSeriesNodeThresholdLineCol->GetNumberOfItems() == 0)
      {
      d->PlotSeriesNodeThresholdLine.TakeReference(vtkMRMLPlotSeriesNode::SafeDownCast
        (this->mrmlScene()->CreateNodeByClass("vtkMRMLPlotSeriesNode")));
      d->PlotSeriesNodeThresholdLine->SetName("ThresholdLine");
      d->PlotSeriesNodeThresholdLine->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatter);
      d->PlotSeriesNodeThresholdLine->SetMarkerStyle(vtkMRMLPlotSeriesNode::MarkerStyleCircle);
      d->PlotSeriesNodeThresholdLine->SetLineStyle(vtkMRMLPlotSeriesNode::LineStyleSolid);
      d->PlotSeriesNodeThresholdLine->SetMarkerSize(11);
      d->PlotSeriesNodeThresholdLine->SetLineWidth(4);
      double color[4] = {0.949, 0.481, 0.176, 1.};
      d->PlotSeriesNodeThresholdLine->SetColor(color);
      d->PlotSeriesNodeThresholdLine->SetAndObserveTableNodeID(d->TableThresholdNode->GetID());
      d->PlotSeriesNodeThresholdLine->SetXColumnName(d->TableThresholdNode->GetColumnName(0));
      d->PlotSeriesNodeThresholdLine->SetYColumnName(d->TableThresholdNode->GetColumnName(1));
      this->mrmlScene()->AddNode(d->PlotSeriesNodeThresholdLine);
      }
    else
      {
      d->PlotSeriesNodeThresholdLine = vtkMRMLPlotSeriesNode::SafeDownCast
        (PlotSeriesNodeThresholdLineCol->GetItemAsObject(0));
      }
    }
}

//-----------------------------------------------------------------------------
bool qSlicerAstroVolumeModuleWidget::updateMasterRepresentationInSegmentation(vtkSegmentation *segmentation, QString representation)
{
  if (!segmentation || representation.isEmpty())
    {
    return false;
    }
  std::string newMasterRepresentation(representation.toLatin1().constData());

  // Set master representation to the added one if segmentation is empty or master representation is undefined
  if (segmentation->GetNumberOfSegments() == 0)
    {
    segmentation->SetMasterRepresentationName(newMasterRepresentation);
    return true;
    }

  // No action is necessary if segmentation is non-empty and the master representation matches the contained one in segment
  if (segmentation->GetMasterRepresentationName() == newMasterRepresentation)
    {
    return true;
    }

  // Get segmentation node for segmentation
  vtkMRMLScene* scene = this->mrmlScene();
  if (!scene)
    {
    return false;
    }
  vtkMRMLSegmentationNode* segmentationNode = vtkSlicerSegmentationsModuleLogic::GetSegmentationNodeForSegmentation(scene, segmentation);
  if (!segmentationNode)
    {
    return false;
    }

  // Ask the user if master was different but not empty
  QString message = QString("Segment is to be added in segmentation '%1' that contains a representation (%2) different than the master representation in the segmentation (%3). "
    "The master representation need to be changed so that the segment can be added. This might result in unwanted data loss.\n\n"
    "Do you wish to change the master representation to %2?")
    .arg(segmentationNode->GetName()).arg(newMasterRepresentation.c_str())
    .arg(segmentation->GetMasterRepresentationName().c_str());
  QMessageBox::StandardButton answer =
    QMessageBox::question(NULL, tr("Master representation is needed to be changed to add segment"), message,
    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (answer == QMessageBox::No)
    {
    return false;
    }

  // Make sure the new master representation exists before setting it
  if (!segmentation->CreateRepresentation(newMasterRepresentation.c_str()))
    {
    std::vector<std::string> containedRepresentationNamesInSegmentation;
    segmentation->GetContainedRepresentationNames(containedRepresentationNamesInSegmentation);
    if (containedRepresentationNamesInSegmentation.empty())
      {
      qCritical() << Q_FUNC_INFO << ": Master representation cannot be created in segmentation as it does not contain any representations!";
      return false;
      }

    std::string firstContainedRepresentation = (*containedRepresentationNamesInSegmentation.begin());
    qCritical() << Q_FUNC_INFO << ": Master representation cannot be created in segmentation! Setting master to the first representation found: " << firstContainedRepresentation.c_str();
    segmentation->SetMasterRepresentationName(newMasterRepresentation.c_str());
    return false;
    }

  // Set master representation to the added one if user agreed
  segmentation->SetMasterRepresentationName(newMasterRepresentation.c_str());
  return true;
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setup()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  d->setupUi(this);  
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onInputVolumeChanged(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!node)
    {
    emit activate3DAstroVolumeNode(false);
    }

  vtkMRMLAstroLabelMapVolumeNode* labelMapVolumeNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node);
  vtkMRMLAstroVolumeNode* astroVolumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast(node);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    return;
    }

  if (!d->selectionNode)
    {
    return;
    }

  if (astroVolumeNode)
    {
    int n = StringToInt(astroVolumeNode->GetAttribute("SlicerAstro.NAXIS"));
    // Check Input volume dimensionality
    if (n == 3)
      {
      emit activate3DAstroVolumeNode(true);
      }
    else
      {
      emit activate3DAstroVolumeNode(false);
      }

    astroVolumeNode->Modified();
    std::string type = astroVolumeNode->GetAttribute("SlicerAstro.DATAMODEL");
    size_t found = type.find("MOMENTMAP");
    if (found == std::string::npos)
      {
      d->selectionNode->SetReferenceActiveVolumeID(astroVolumeNode->GetID());
      d->selectionNode->SetActiveVolumeID(astroVolumeNode->GetID());
      appLogic->PropagateBackgroundVolumeSelection(1);
      }
    }
  else if (labelMapVolumeNode)
    {
    emit deactivate3DLabelMapVolumeNode(true);
    labelMapVolumeNode->Modified();
    d->selectionNode->SetReferenceActiveLabelVolumeID(labelMapVolumeNode->GetID());
    d->selectionNode->SetActiveLabelVolumeID(labelMapVolumeNode->GetID());
    appLogic->PropagateLabelVolumeSelection(1);
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(NULL);
    d->selectionNode->SetActiveVolumeID(NULL);
    appLogic->PropagateBackgroundVolumeSelection(1);
    d->selectionNode->SetReferenceActiveLabelVolumeID(NULL);
    d->selectionNode->SetActiveLabelVolumeID(NULL);
    appLogic->PropagateLabelVolumeSelection(1);
    }

  if (d->plotChartNodeHistogram)
    {
    d->plotChartNodeHistogram->RemoveAllPlotSeriesNodeIDs();
    }

  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID("");
    d->selectionNode->SetActiveTableID("");
    }

  if (!this->mrmlScene())
    {
    return;
    }

  vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode"));
  for(int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
    {
    vtkMRMLSliceNode* sliceNode =
        vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i));
    if (sliceNode)
      {
      sliceNode->Modified();
      }
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onLockToggled(bool toggled)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  d->Lock = toggled;

  if (!d->LockPushButton)
    {
    return;
    }

  if (d->Lock)
    {
    d->LockPushButton->setIcon(QIcon(":Icons/astrolock.png"));
    }
  else
    {
    d->LockPushButton->setIcon(QIcon(":Icons/astrounlock.png"));
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::resetOffset(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!node)
    {
    return;
    }

  d->offsetOldValue = 0.;
  d->PresetOffsetSlider->setValue(0.);

  if (!node->IsA("vtkMRMLAstroVolumeNode") &&
      !node->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    return;
    }

  double width = StringToDouble(node->GetAttribute("SlicerAstro.DATAMAX")) -
          StringToDouble(node->GetAttribute("SlicerAstro.DATAMIN"));
  bool wasBlocking = d->PresetOffsetSlider->blockSignals(true);
  d->PresetOffsetSlider->setSingleStep(
    width ? ctk::closestPowerOfTen(width) / 100. : 0.1);
  d->PresetOffsetSlider->setPageStep(d->PresetOffsetSlider->singleStep());
  d->PresetOffsetSlider->setRange(-width, width);
  d->PresetOffsetSlider->blockSignals(wasBlocking);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::updatePresets(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLAstroVolumeNode* inputVolume = vtkMRMLAstroVolumeNode::SafeDownCast(node);

  if(!inputVolume)
    {
    return;
    }

  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());

  if(!astroVolumeLogic->synchronizePresetsToVolumeNode(node))
    {
    qWarning() << "qSlicerAstroVolumeModuleWidget::updatePresets error : synchronizePresetsToVolumeNode failed;"
                  " encountered in adjusting the Color Function for the 3D display.";
    }

  if (inputVolume->GetPresetNode() != NULL)
    {
    d->PresetsNodeComboBox->setCurrentNodeIndex(d->astroVolumeNode->GetPresetIndex());
    }
  else
    {
    d->PresetsNodeComboBox->setCurrentNodeIndex(-1);
    d->PresetsNodeComboBox->setCurrentNodeIndex(0);
    }
  d->PresetsNodeComboBox->update();
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setRadioVelocity()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->ActiveVolumeNodeSelector || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLNode *node = d->ActiveVolumeNodeSelector->currentNode();
  vtkMRMLAstroVolumeNode *volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  vtkMRMLAstroLabelMapVolumeNode *volumeLabelNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node);
  if (!volumeNode && !volumeLabelNode)
    {
    return;
    }

  bool updateSlice = false;
  if (volumeNode)
    {
    vtkMRMLAstroVolumeDisplayNode* volumeDisplayNode =
      volumeNode->GetAstroVolumeDisplayNode();
    if (!volumeDisplayNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::setRadioVelocity : "
                    "volumeDisplayNode not found.";
      return;
      }
    if (volumeDisplayNode->SetRadioVelocityDefinition())
      {
      volumeNode->Modified();
      updateSlice = true;
      }
    }
  else if (volumeLabelNode)
    {
    vtkMRMLAstroLabelMapVolumeDisplayNode* volumeLabelDisplayNode =
      volumeLabelNode->GetAstroLabelMapVolumeDisplayNode();
    if (!volumeLabelDisplayNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::setRadioVelocity : "
                    "volumeLabelDisplayNode not found.";
      return;
      }
    if (volumeLabelDisplayNode->SetRadioVelocityDefinition())
      {
      volumeLabelNode->Modified();
      updateSlice = true;
      }
    }

  if (updateSlice)
    {
    vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
        (this->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode"));
    for(int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
      {
      vtkMRMLSliceNode* sliceNode =
          vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i));
      if (sliceNode)
        {
        sliceNode->Modified();
        }
      }
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::spreadPreset(double stretchValue)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->volumeRenderingWidget)
    {
    return;
    }

  ctkVTKVolumePropertyWidget* volumePropertyWidget =
     d->volumeRenderingWidget->findChild<ctkVTKVolumePropertyWidget*>
       (QString("VolumeProperty"));

  if (!volumePropertyWidget)
    {
    return;
    }

  volumePropertyWidget->spreadAllPoints(stretchValue - d->stretchOldValue, true);
  d->stretchOldValue = stretchValue;

  if (d->Lock)
    {
    d->PresetOffsetSlider->setValue(stretchValue);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::clearPresets()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  d->PresetsNodeComboBox->setCurrentNodeIndex(-1);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onCreateSurfaceButtonToggled(bool toggle)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->segmentEditorNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment editor parameter set node";
    d->CreateSurfaceButton->blockSignals(true);
    d->CreateSurfaceButton->setChecked(false);
    d->CreateSurfaceButton->blockSignals(false);
    return;
    }

  vtkMRMLSegmentationNode* segmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!segmentationNode || !segmentationNode->GetSegmentation())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationNode";
    d->CreateSurfaceButton->blockSignals(true);
    d->CreateSurfaceButton->setChecked(false);
    d->CreateSurfaceButton->blockSignals(false);
    return;
    }
  vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(
    segmentationNode->GetDisplayNode());
  if (!displayNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationDisplayNode";
    d->CreateSurfaceButton->blockSignals(true);
    d->CreateSurfaceButton->setChecked(false);
    d->CreateSurfaceButton->blockSignals(false);
    return;
    }

  // If just have been checked, then create closed surface representation and show it
  if (toggle)
    {
    // Make sure closed surface representation exists
    if (segmentationNode->GetSegmentation()->CreateRepresentation(
      vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName() ))
      {
      // Set closed surface as displayed poly data representation
      displayNode->SetPreferredDisplayRepresentationName3D(
        vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName() );
      // But keep binary labelmap for 2D
      displayNode->SetPreferredDisplayRepresentationName2D(
        vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName() );
      }
    }
  // If unchecked, then remove representation (but only if it's not the master representation)
  else if (segmentationNode->GetSegmentation()->GetMasterRepresentationName() !=
           vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName())
    {
    segmentationNode->GetSegmentation()->RemoveRepresentation(
      vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onROICropDisplayCheckBoxToggled(bool toggle)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (toggle)
    {
    d->ROICropCheckBox->setChecked(true);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onSegmentEditorNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

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
    d->SegmentsTableView_2->segmentationNode());
  if (!segmentationNodeTable)
    {
    d->SegmentsTableView_2->setSegmentationNode(segmentationNode);
    return;
    }

  if (segmentationNode != segmentationNodeTable)
    {
    d->SegmentsTableView_2->setSegmentationNode(segmentationNode);
  }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::resetStretch(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!node)
    {
    return;
    }

  d->stretchOldValue = 0.;
  d->PresetStretchSlider->setValue(0.);

  if (!node->IsA("vtkMRMLAstroVolumeNode") &&
      !node->IsA("vtkMRMLAstroLabelMapVolumeNode"))
    {
    return;
    }

  double width = StringToDouble(node->GetAttribute("SlicerAstro.DATAMAX")) -
                 StringToDouble(node->GetAttribute("SlicerAstro.DATAMIN"));
  bool wasBlocking = d->PresetStretchSlider->blockSignals(true);
  d->PresetStretchSlider->setSingleStep(
    width ? ctk::closestPowerOfTen(width) / 100. : 0.1);
  d->PresetStretchSlider->setPageStep(d->PresetOffsetSlider->singleStep());
  d->PresetStretchSlider->setRange(-width, width);
  d->PresetStretchSlider->blockSignals(wasBlocking);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onPushButtonCovertLabelMapToSegmentationClicked()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->segmentEditorNode)
    {
    qCritical() << Q_FUNC_INFO << ": segmentEditorNode not found.";
    return;
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

  vtkMRMLAstroLabelMapVolumeNode* labelMapNode = vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(
    d->ActiveVolumeNodeSelector->currentNode());

  if (!labelMapNode)
    {
    QString message = QString("the current volume is not a labelmap volume (Mask)!");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to import from labelmap volume"), message);
    d->pushButtonCovertLabelMapToSegmentation->blockSignals(true);
    d->pushButtonCovertLabelMapToSegmentation->setChecked(false);
    d->pushButtonCovertLabelMapToSegmentation->blockSignals(false);
    return;
    }

  labelMapNode->UpdateRangeAttributes();

  currentSegmentationNode->SetReferenceImageGeometryParameterFromVolumeNode(labelMapNode);

  if (this->updateMasterRepresentationInSegmentation(currentSegmentationNode->GetSegmentation(), vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName()))
    {
    if (!vtkSlicerSegmentationsModuleLogic::ImportLabelmapToSegmentationNode(labelMapNode, currentSegmentationNode))
      {
      QString message = QString("Failed to copy labels from labelmap volume node %1!").arg(labelMapNode->GetName());
      qCritical() << Q_FUNC_INFO << ": " << message;
      QMessageBox::warning(NULL, tr("Failed to import from labelmap volume"), message);
      d->pushButtonCovertLabelMapToSegmentation->blockSignals(true);
      d->pushButtonCovertLabelMapToSegmentation->setChecked(false);
      d->pushButtonCovertLabelMapToSegmentation->blockSignals(false);
      return;
      }
    }

  labelMapNode->UpdateRangeAttributes();

  currentSegmentationNode->CreateDefaultDisplayNodes();

  d->pushButtonCovertLabelMapToSegmentation->blockSignals(true);
  d->pushButtonCovertLabelMapToSegmentation->setChecked(false);
  d->pushButtonCovertLabelMapToSegmentation->blockSignals(false);

  this->onCreateSurfaceButtonToggled(true);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    return;
    }

  if (!d->selectionNode)
    {
    return;
    }

  d->selectionNode->SetReferenceActiveLabelVolumeID("");
  appLogic->PropagateLabelVolumeSelection(1);

  if (!d->ActiveVolumeNodeSelector || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLAstroVolumeNode* activeNode = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(d->selectionNode->GetActiveVolumeID()));

  d->ActiveVolumeNodeSelector->setCurrentNode(activeNode);

  if (!activeNode)
    {
    qSlicerApplication* app = qSlicerApplication::application();
    for (int i = 0; i < app->layoutManager()->threeDViewCount(); i++)
      {
      app->layoutManager()->threeDWidget(i)->threeDController()->resetFocalPoint();
      }
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onPushButtonConvertSegmentationToLabelMapClicked()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->segmentEditorNode)
    {
    qCritical() << Q_FUNC_INFO << ": segmentEditorNode not found.";
    return;
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode)
    {
    QString message = QString("No segmentation selected!");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(true);
    d->pushButtonConvertSegmentationToLabelMap->setChecked(false);
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(false);
    return;
    }

  // Export selected segments into a multi-label labelmap volume
  std::vector<std::string> segmentIDs;
  currentSegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIDs);

  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> labelMapNode;

  if (segmentIDs.size() < 1)
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              "Be sure that segment to export has been selected in the table view (left click). \n\n").
                                arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(true);
    d->pushButtonConvertSegmentationToLabelMap->setChecked(false);
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(false);
    return;
    }

  int maskID = 0;
  std::string maskIDName;
  for (unsigned int ii = 0; ii < segmentIDs.size(); ii++)
    {
    vtkSegment* segment = currentSegmentationNode->GetSegmentation()->GetSegment(segmentIDs[ii]);
    int segmentMaskID = StringToInt(segment->GetName());
    if (segmentMaskID > maskID)
      {
      maskID = segmentMaskID;
      maskIDName = segmentIDs[ii];
      }
    }

  currentSegmentationNode->GetSegmentation()->RemoveSegment(maskIDName);
  currentSegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIDs);

  if (segmentIDs.size() < 1)
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              "Be sure that segment to export has been selected in the table view (left click). \n\n").
                               arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(true);
    d->pushButtonConvertSegmentationToLabelMap->setChecked(false);
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(false);
    return;
    }

  vtkMRMLAstroLabelMapVolumeNode* activelabelMapNode=
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(d->ActiveVolumeNodeSelector->currentNode());

  vtkMRMLAstroVolumeNode* activeVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(
    d->ActiveVolumeNodeSelector->currentNode());

  if(activelabelMapNode)
    {
    labelMapNode = vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode>::New();
    labelMapNode->Copy(activelabelMapNode);
    std::string name(activelabelMapNode->GetName());
    std::string str1("Copy_mask");
    std::string str2("_mask");
    std::size_t found = name.find(str1);
    if (found != std::string::npos)
      { 
      name = name.substr(0, found);
      }
    found = name.find(str2);
    if (found != std::string::npos)
      {
      name = name.substr(0, found);
      }
    name += "Copy_mask";
    std::string uname = this->mrmlScene()->GetUniqueNameByString(name.c_str());
    labelMapNode->SetName(uname.c_str());
    this->mrmlScene()->AddNode(labelMapNode);
    }
  else if (activeVolumeNode)
    {
    vtkSlicerAstroVolumeLogic* logic = vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
    if (!logic)
      {
      qCritical() << Q_FUNC_INFO <<" : astroVolumelogic not found!";
      return;
      }
    std::string name(activeVolumeNode->GetName());
    name += "Copy_mask";
    labelMapNode = logic->CreateAndAddLabelVolume(this->mrmlScene(), activeVolumeNode, name.c_str());
    }
  else
    {
    qCritical() << Q_FUNC_INFO << ": converting current segmentation Node into labelMap Node (Mask),"
                                  " but the labelMap Node is invalid!";
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(true);
    d->pushButtonConvertSegmentationToLabelMap->setChecked(false);
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(false);
    return;
    }

  bool exportSuccess = vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode(currentSegmentationNode, segmentIDs, labelMapNode, activeVolumeNode);

  if (!exportSuccess)
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              "Be sure that segment to export has been selected in the table view (left click). \n\n").
                              arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    this->mrmlScene()->RemoveNode(labelMapNode);
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(true);
    d->pushButtonConvertSegmentationToLabelMap->setChecked(false);
    d->pushButtonConvertSegmentationToLabelMap->blockSignals(false);
    return;
    }

  labelMapNode->UpdateRangeAttributes();

  d->pushButtonConvertSegmentationToLabelMap->blockSignals(true);
  d->pushButtonConvertSegmentationToLabelMap->setChecked(false);
  d->pushButtonConvertSegmentationToLabelMap->blockSignals(false);

  // Remove segments from current segmentation if export was successful
  if (exportSuccess)
    {
    for (unsigned int ii = 0; ii < segmentIDs.size(); ii++)
      {
      currentSegmentationNode->GetSegmentation()->RemoveSegment(segmentIDs[ii]);
      }
    }

  d->CreateSurfaceButton->blockSignals(true);
  d->CreateSurfaceButton->setChecked(false);
  d->CreateSurfaceButton->blockSignals(false);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onDisplayThresholdValueChanged(double DisplayThreshold)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode)
    {
    return;
    }

  d->astroVolumeNode->Set3DDisplayThreshold(DisplayThreshold);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onVisibilityChanged(bool visibility)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
        d->volumeRenderingWidget->mrmlDisplayNode();

  if (!displayNode)
    {
    return;
    }

  displayNode->SetVisibility(visibility);

  qSlicerApplication* app = qSlicerApplication::application();
  for (int i = 0; i < app->layoutManager()->threeDViewCount(); i++)
    {
    app->layoutManager()->threeDWidget(i)->threeDController()->resetFocalPoint();
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setComparative3DViews(const char* volumeNodeOneID,
                                                           const char* volumeNodeTwoID,
                                                           bool generateMasks /* = false */,
                                                           bool overlay2D /* = true*/)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "qSlicerApplication not found!";
    return;
    }

  app->layoutManager()->layoutLogic()->GetLayoutNode()->SetViewArrangement
          (vtkMRMLLayoutNode::SlicerLayoutDual3DView);

  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews :"
                   " scene not found.";
    return;
    }

  vtkMRMLAstroVolumeNode *volumeOne = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeOneID));
  vtkMRMLAstroVolumeNode *volumeTwo = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeTwoID));

  if(!volumeOne || !volumeTwo)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews :"
                   " volumes not valid.";
    return;
    }

  volumeTwo->SetDisplayVisibility(0);
  volumeOne->SetDisplayVisibility(0);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setComparative3DViews : appLogic not found!";
    return;
    }

  if (!d->selectionNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setComparative3DViews : selectionNode not found!";
    return;
    }

  vtkSmartPointer<vtkCollection> col = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLViewNode"));

  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());

  unsigned int numViewNodes = col->GetNumberOfItems();
  int n = volumeOne->GetNumberOfDisplayNodes();
  int renderingQuality = 0;
  for (int i = 0; i < n; i++)
    {
    vtkMRMLVolumeRenderingDisplayNode *volumeOneRenderingDisplay =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(volumeOne->GetNthDisplayNodeID(i)));
    if (volumeOneRenderingDisplay)
      {
      volumeOneRenderingDisplay->RemoveAllViewNodeIDs();

      for (unsigned int n = 0; n < numViewNodes; n++)
        {
        vtkMRMLViewNode *viewNodeIter =
            vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(n));
        if (viewNodeIter)
          {
          volumeOneRenderingDisplay->AddViewNodeID(viewNodeIter->GetID());
          renderingQuality = volumeOneRenderingDisplay->GetPerformanceControl();
          break;
          }
        }
      }
    }

  this->updatePresets(volumeOne);

  d->selectionNode->SetActiveVolumeID(volumeTwo->GetID());

  n = volumeTwo->GetNumberOfDisplayNodes();
  for (int i = 0; i < n; i++)
    {
    vtkMRMLVolumeRenderingDisplayNode *volumeTwoRenderingDisplay =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(volumeTwo->GetNthDisplayNodeID(i)));
    if (volumeTwoRenderingDisplay)
      {
      volumeTwoRenderingDisplay->RemoveAllViewNodeIDs();

      bool second = false;
      for (unsigned int n = 0; n < numViewNodes; n++)
        {
        vtkMRMLViewNode *viewNodeIter =
            vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(n));
        if (viewNodeIter)
          {
          if(second)
            {
            volumeTwoRenderingDisplay->AddViewNodeID(viewNodeIter->GetID());
            volumeTwoRenderingDisplay->SetPerformanceControl(renderingQuality);
            break;
            }
          second = true;
          }
        }
      }
    }
  this->updatePresets(volumeTwo);

  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());
  if (overlay2D)
    {
    d->selectionNode->SetSecondaryVolumeID(volumeTwo->GetID());
    }
  else
    {
    d->selectionNode->SetSecondaryVolumeID("");
    }

  vtkSmartPointer<vtkCollection> col1 = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));

  unsigned int numCameraNodes = col1->GetNumberOfItems();
  if (numCameraNodes < 2)
    {
    return;
    }
  vtkMRMLCameraNode *cameraNodeOne =
    vtkMRMLCameraNode::SafeDownCast(col1->GetItemAsObject(0));
  if (cameraNodeOne)
    {
    double Origin[3];
    volumeOne->GetOrigin(Origin);
    int* dims = volumeOne->GetImageData()->GetDimensions();
    Origin[0] = 0.;
    Origin[1] = dims[2] * 2 + sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
    Origin[2] = 0.;
    cameraNodeOne->SetPosition(Origin);
    Origin[0] = 0.;
    Origin[1] = 0.;
    Origin[2] = 1.;
    cameraNodeOne->SetViewUp(Origin);
    Origin[0] = 0.;
    Origin[1] = 0.;
    Origin[2] = 0.;
    cameraNodeOne->SetFocalPoint(Origin);
    }
  vtkMRMLCameraNode *cameraNodeTwo =
    vtkMRMLCameraNode::SafeDownCast(col1->GetItemAsObject(1));
  if (cameraNodeTwo)
    {
    double Origin[3];
    volumeTwo->GetOrigin(Origin);
    int* dims = volumeTwo->GetImageData()->GetDimensions();
    Origin[0] = 0.;
    Origin[1] = dims[2] * 2 + sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
    Origin[2] = 0.;
    cameraNodeTwo->SetPosition(Origin);
    Origin[0] = 0.;
    Origin[1] = 0.;
    Origin[2] = 1.;
    cameraNodeTwo->SetViewUp(Origin);
    Origin[0] = 0.;
    Origin[1] = 0.;
    Origin[2] = 0.;
    cameraNodeTwo->SetFocalPoint(Origin);
    }

  volumeTwo->SetDisplayVisibility(1);
  volumeOne->SetDisplayVisibility(1);

  if (overlay2D)
    {
    vtkMRMLSliceCompositeNode *yellowSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
    yellowSliceComposite->SetForegroundOpacity(1.);

    vtkMRMLSliceNode *yellowSlice = vtkMRMLSliceNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
    // setting to the XZ orientation is needed in order to force the refresh
    yellowSlice->SetOrientation("XZ");
    yellowSlice->SetOrientation("XY");
    yellowSlice->SetSliceOffset(0.);
    }

  appLogic->PropagateVolumeSelection();

  // reset the 3D rendering boundaries
  qMRMLThreeDWidget* ThreeDWidget1 = app->layoutManager()->threeDWidget(0);
  if(!ThreeDWidget1)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDWidget1 not found!";
    return;
    }

  qMRMLThreeDView* ThreeDView1 = ThreeDWidget1->threeDView();
  if(!ThreeDView1 || !ThreeDView1->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDView1 not found!";
    return;
    }

  ThreeDView1->renderer()->ResetCameraClippingRange();
  ThreeDView1->renderer()->Render();

  qMRMLThreeDWidget* ThreeDWidget2 = app->layoutManager()->threeDWidget(1);
  if(!ThreeDWidget2)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDWidget2 not found!";
    return;
    }

  qMRMLThreeDView* ThreeDView2 = ThreeDWidget2->threeDView();
  if(!ThreeDView2 || !ThreeDView2->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDView2 not found!";
    return;
    }

  ThreeDView2->renderer()->ResetCameraClippingRange();
  ThreeDView2->renderer()->Render();

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
    if (!SegmentationDisplayNode)
      {
      continue;
      }
    SegmentationDisplayNode->SetAllSegmentsVisibility(false);
    }

  if (!generateMasks)
    {
    return;
    }

  // Create empty segment in current segmentation
  this->mrmlScene()->SaveStateForUndo();

  std::string SegmentOneID = volumeOne->GetName();
  SegmentOneID += "_3RMS";
  vtkSegment *SegmentOne = currentSegmentationNode->GetSegmentation()->GetSegment(SegmentOneID);
  if(!SegmentOne)
    {
    SegmentOneID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentOneID, SegmentOneID);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeOne->GetImageData());
    double min, max;
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.3DDisplayThreshold")) * 3.;
    max = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DATAMAX"));
    imageThreshold->ThresholdBetween(min, max);
    imageThreshold->SetInValue(1);
    imageThreshold->SetOutValue(0);
    imageThreshold->SetOutputScalarType(VTK_SHORT);
    imageThreshold->Update();
    vtkNew<vtkOrientedImageData> modifierLabelmap;
    modifierLabelmap->DeepCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    volumeOne->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
        modifierLabelmap.GetPointer(), currentSegmentationNode, SegmentOneID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MAX))
      {
      qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
      }
    }

  std::string SegmentTwoID = volumeTwo->GetName();
  SegmentTwoID += "_3RMS";
  vtkSegment *SegmentTwo = currentSegmentationNode->GetSegmentation()->GetSegment(SegmentTwoID);
  if(!SegmentTwo)
    {
    SegmentTwoID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentTwoID, SegmentTwoID);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeTwo->GetImageData());
    double min, max;
    min = StringToDouble(volumeTwo->GetAttribute("SlicerAstro.3DDisplayThreshold")) * 3.;
    max = StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX"));
    imageThreshold->ThresholdBetween(min, max);
    imageThreshold->SetInValue(1);
    imageThreshold->SetOutValue(0);
    imageThreshold->SetOutputScalarType(VTK_SHORT);
    imageThreshold->Update();
    vtkNew<vtkOrientedImageData> modifierLabelmap;
    modifierLabelmap->DeepCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    volumeTwo->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
        modifierLabelmap.GetPointer(), currentSegmentationNode, SegmentTwoID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MAX))
      {
      qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
      }
    }

  for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
    if (!SegmentationDisplayNode)
      {
      continue;
      }
    SegmentationDisplayNode->SetSegmentVisibility(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentTwoID, false);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentOpacity3D(SegmentTwoID, 0.8);
    }

  this->onCreateSurfaceButtonToggled(true);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setQuantitative3DView(const char *volumeNodeOneID,
                                                           const char *volumeNodeTwoID,
                                                           const char *volumeNodeThreeID,
                                                           double ContourLevel,
                                                           double PVPhiMajor,
                                                           double PVPhiMinor,
                                                           double RAS[3])
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView :"
                   " scene not found.";
    return;
    }

  // Set a Plot Layout
  vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(
    this->mrmlScene()->GetFirstNodeByClass("vtkMRMLLayoutNode"));
  if (!layoutNode)
    {
    qWarning() << Q_FUNC_INFO << ": Unable to get layout node!";
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

  vtkMRMLAstroVolumeNode *volumeOne = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeOneID));
  vtkMRMLAstroVolumeNode *volumeTwo = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeTwoID));
  vtkMRMLAstroVolumeNode *volumeThree = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeThreeID));

  if(!volumeOne || !volumeTwo || !volumeThree ||
     !volumeOne->GetImageData() || !volumeTwo->GetImageData() ||
     !volumeThree->GetImageData())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : volumes not valid.";
    return;
    }

  volumeTwo->SetDisplayVisibility(0);
  volumeThree->SetDisplayVisibility(0);
  volumeOne->SetDisplayVisibility(0);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : appLogic not found!";
    return;
    }

  if (!d->selectionNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : selectionNode not found!";
    return;
    }

  // Update 3D Renderings
  vtkSmartPointer<vtkCollection> col = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLViewNode"));

  unsigned int numViewNodes = col->GetNumberOfItems();

  d->selectionNode->SetActiveVolumeID(volumeTwo->GetID());

  int n = volumeTwo->GetNumberOfDisplayNodes();
  for (int i = 0; i < n; i++)
    {
    vtkMRMLVolumeRenderingDisplayNode *volumeTwoRenderingDisplay =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(volumeTwo->GetNthDisplayNodeID(i)));
    if (volumeTwoRenderingDisplay)
      {
      volumeTwoRenderingDisplay->RemoveAllViewNodeIDs();

      for (unsigned int n = 0; n < numViewNodes; n++)
        {
        vtkMRMLViewNode *viewNodeIter =
            vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(n));
        if (viewNodeIter)
          {
          volumeTwoRenderingDisplay->AddViewNodeID(viewNodeIter->GetID());
          break;
          }
        }
      }
    }
  this->updatePresets(volumeTwo);

  d->selectionNode->SetActiveVolumeID(volumeThree->GetID());

  n = volumeThree->GetNumberOfDisplayNodes();
  for (int i = 0; i < n; i++)
    {
    vtkMRMLVolumeRenderingDisplayNode *volumeThreeRenderingDisplay =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(volumeThree->GetNthDisplayNodeID(i)));
    if (volumeThreeRenderingDisplay)
      {
      volumeThreeRenderingDisplay->RemoveAllViewNodeIDs();

      for (unsigned int n = 0; n < numViewNodes; n++)
        {
        vtkMRMLViewNode *viewNodeIter =
            vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(n));
        if (viewNodeIter)
          {
          volumeThreeRenderingDisplay->AddViewNodeID(viewNodeIter->GetID());
          break;
          }
        }
      }
    }
  this->updatePresets(volumeThree);

  d->selectionNode->SetSecondaryVolumeID(volumeTwo->GetID());
  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());

  n = volumeOne->GetNumberOfDisplayNodes();
  for (int i = 0; i < n; i++)
    {
    vtkMRMLVolumeRenderingDisplayNode *volumeOneRenderingDisplay =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast
        (this->mrmlScene()->GetNodeByID(volumeOne->GetNthDisplayNodeID(i)));
    if (volumeOneRenderingDisplay)
      {
      volumeOneRenderingDisplay->RemoveAllViewNodeIDs();

      for (unsigned int n = 0; n < numViewNodes; n++)
        {
        vtkMRMLViewNode *viewNodeIter =
            vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(n));
        if (viewNodeIter)
          {
          volumeOneRenderingDisplay->AddViewNodeID(viewNodeIter->GetID());
          break;
          }
        }
      }
    }

  this->updatePresets(volumeOne);

  vtkSmartPointer<vtkCollection> col1 = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));

  unsigned int numCameraNodes = col1->GetNumberOfItems();
  if (numCameraNodes < 1)
    {
    return;
    }
  vtkMRMLCameraNode *cameraNode =
    vtkMRMLCameraNode::SafeDownCast(col1->GetItemAsObject(0));
  if (cameraNode)
    {
    double Origin[3];
    volumeOne->GetOrigin(Origin);
    int* dims = volumeOne->GetImageData()->GetDimensions();
    Origin[0] = 0.;
    Origin[1] = dims[2] * 2 + sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
    Origin[2] = 0.;
    cameraNode->SetPosition(Origin);
    Origin[0] = 0.;
    Origin[1] = 0.;
    Origin[2] = 1.;
    cameraNode->SetViewUp(Origin);
    Origin[0] = 0.;
    Origin[1] = 0.;
    Origin[2] = 0.;
    cameraNode->SetFocalPoint(Origin);
    }

  volumeTwo->SetDisplayVisibility(1);
  volumeThree->SetDisplayVisibility(1);
  volumeOne->SetDisplayVisibility(1);

  // reset the 3D rendering boundaries
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "qSlicerApplication not found!";
    return;
    }

  qMRMLThreeDWidget* ThreeDWidget = app->layoutManager()->threeDWidget(0);
  if(!ThreeDWidget)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDWidget not found!";
    return;
    }

  qMRMLThreeDView* ThreeDView = ThreeDWidget->threeDView();
  if(!ThreeDView || !ThreeDView->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDView not found!";
    return;
    }

  ThreeDView->renderer()->ResetCameraClippingRange();
  ThreeDView->renderer()->Render();

  // Create Segmentations
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

  if (!currentSegmentationNode->GetSegmentation())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView() : segmentation not found.";
    return;
    }

  // Create empty segment in current segmentation
  this->mrmlScene()->SaveStateForUndo();

  std::string SegmentOneID = volumeOne->GetName();
  SegmentOneID += "_" + DoubleToString(ContourLevel) + "RMS";
  vtkSegment *SegmentOne = currentSegmentationNode->GetSegmentation()->GetSegment(SegmentOneID);
  if(!SegmentOne)
    {
    double color[3] = {0.5, 0.68, 0.5};
    SegmentOneID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentOneID, SegmentOneID, color);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeOne->GetImageData());
    double min, max;
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.3DDisplayThreshold")) * ContourLevel;
    max = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DATAMAX"));
    imageThreshold->ThresholdBetween(min, max);
    imageThreshold->SetInValue(1);
    imageThreshold->SetOutValue(0);
    imageThreshold->SetOutputScalarType(VTK_SHORT);
    imageThreshold->Update();
    vtkNew<vtkOrientedImageData> modifierLabelmap;
    modifierLabelmap->DeepCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    volumeOne->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
        modifierLabelmap.GetPointer(), currentSegmentationNode, SegmentOneID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MAX))
      {
      qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
      }
    }

  std::string SegmentTwoID = volumeTwo->GetName();
  SegmentTwoID += "_" + DoubleToString(ContourLevel) + "RMS";
  vtkSegment *SegmentTwo = currentSegmentationNode->GetSegmentation()->GetSegment(SegmentTwoID);
  if(!SegmentTwo)
    {
    double color[3] = {0.17, 0.40, 0.57};
    SegmentTwoID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentTwoID, SegmentTwoID, color);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeTwo->GetImageData());
    double min, max;
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.3DDisplayThreshold")) * ContourLevel;
    max = StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX"));
    imageThreshold->ThresholdBetween(min, max);
    imageThreshold->SetInValue(1);
    imageThreshold->SetOutValue(0);
    imageThreshold->SetOutputScalarType(VTK_SHORT);
    imageThreshold->Update();
    vtkNew<vtkOrientedImageData> modifierLabelmap;
    modifierLabelmap->DeepCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    volumeTwo->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
        modifierLabelmap.GetPointer(), currentSegmentationNode, SegmentTwoID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MAX))
      {
      qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
      }
    }

  std::string SegmentThreeID = volumeTwo->GetName();
  SegmentThreeID += "_asMask";
  vtkSegment *SegmentThree = currentSegmentationNode->GetSegmentation()->GetSegment(SegmentThreeID);
  if(!SegmentThree)
    {
    double color[3] = {1., 0.9, 0.13};
    SegmentThreeID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentThreeID, SegmentThreeID, color);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeTwo->GetImageData());
    double max;
    max = StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX"));
    imageThreshold->ThresholdBetween(1.E-6 , max);
    imageThreshold->SetInValue(1);
    imageThreshold->SetOutValue(0);
    imageThreshold->SetOutputScalarType(VTK_SHORT);
    imageThreshold->Update();
    vtkNew<vtkOrientedImageData> modifierLabelmap;
    modifierLabelmap->DeepCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    volumeTwo->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
        modifierLabelmap.GetPointer(), currentSegmentationNode, SegmentThreeID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MAX))
      {
      qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
      }
    }

  if (currentSegmentationNode->GetNumberOfDisplayNodes() < 1)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView error :"
                   " SegmentationNode has no displayNodes!"<<endl;
    }

  for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
    if (!SegmentationDisplayNode)
      {
      continue;
      }
    SegmentationDisplayNode->SetAllSegmentsVisibility(false);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentThreeID, true);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentThreeID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentThreeID,false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentTwoID, false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentThreeID, false);
    }

  this->onCreateSurfaceButtonToggled(true);

  // Create PV major
  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "yellowSliceNode not found!";
    return;
    }

  yellowSliceNode->SetOrientation("XZ");
  vtkSmartPointer<vtkMatrix3x3> PVMajorMatrix = vtkSmartPointer<vtkMatrix3x3>::New();
  vtkNew<vtkTransform> transformMajor;
  transformMajor->SetMatrix(yellowSliceNode->GetSliceToRAS());
  transformMajor->RotateY(PVPhiMajor);

  PVMajorMatrix->SetElement(0, 0, transformMajor->GetMatrix()->GetElement(0,0));
  PVMajorMatrix->SetElement(0, 1, transformMajor->GetMatrix()->GetElement(0,1));
  PVMajorMatrix->SetElement(0, 2, transformMajor->GetMatrix()->GetElement(0,2));
  PVMajorMatrix->SetElement(1, 0, transformMajor->GetMatrix()->GetElement(1,0));
  PVMajorMatrix->SetElement(1, 1, transformMajor->GetMatrix()->GetElement(1,1));
  PVMajorMatrix->SetElement(1, 2, transformMajor->GetMatrix()->GetElement(1,2));
  PVMajorMatrix->SetElement(2, 0, transformMajor->GetMatrix()->GetElement(2,0));
  PVMajorMatrix->SetElement(2, 1, transformMajor->GetMatrix()->GetElement(2,1));
  PVMajorMatrix->SetElement(2, 2, transformMajor->GetMatrix()->GetElement(2,2));

  if (yellowSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }
  yellowSliceNode->SetOrientation("PVMajor");

  // Translate to X and Y center
  yellowSliceNode->GetSliceToRAS()->SetElement(0, 3, RAS[0]);
  yellowSliceNode->GetSliceToRAS()->SetElement(1, 3, RAS[1]);
  yellowSliceNode->GetSliceToRAS()->SetElement(2, 3, RAS[2]);
  yellowSliceNode->UpdateMatrices();

  // Create PV minor
  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "greenSliceNode not found!";
    return;
    }

  greenSliceNode->SetOrientation("XZ");
  vtkSmartPointer<vtkMatrix3x3> PVMinorMatrix = vtkSmartPointer<vtkMatrix3x3>::New();
  vtkNew<vtkTransform> transformMinor;
  transformMinor->SetMatrix(greenSliceNode->GetSliceToRAS());
  transformMinor->RotateY(PVPhiMinor);
  PVMinorMatrix->SetElement(0, 0, transformMinor->GetMatrix()->GetElement(0,0));
  PVMinorMatrix->SetElement(0, 1, transformMinor->GetMatrix()->GetElement(0,1));
  PVMinorMatrix->SetElement(0, 2, transformMinor->GetMatrix()->GetElement(0,2));
  PVMinorMatrix->SetElement(1, 0, transformMinor->GetMatrix()->GetElement(1,0));
  PVMinorMatrix->SetElement(1, 1, transformMinor->GetMatrix()->GetElement(1,1));
  PVMinorMatrix->SetElement(1, 2, transformMinor->GetMatrix()->GetElement(1,2));
  PVMinorMatrix->SetElement(2, 0, transformMinor->GetMatrix()->GetElement(2,0));
  PVMinorMatrix->SetElement(2, 1, transformMinor->GetMatrix()->GetElement(2,1));
  PVMinorMatrix->SetElement(2, 2, transformMinor->GetMatrix()->GetElement(2,2));

  if (greenSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    greenSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    greenSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
  greenSliceNode->SetOrientation("PVMinor");
  // Translate to X and Y center
  greenSliceNode->GetSliceToRAS()->SetElement(0, 3, RAS[0]);
  greenSliceNode->GetSliceToRAS()->SetElement(1, 3, RAS[1]);
  greenSliceNode->GetSliceToRAS()->SetElement(2, 3, RAS[2]);
  greenSliceNode->UpdateMatrices();

  // Add Presents PV to Red, Yellow and Green slices
  vtkMRMLSliceNode *redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (!redSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "redSliceNode not found!";
    return;
    }

  if (redSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    redSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    redSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }

  if (redSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    redSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    redSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
  redSliceNode->Modified();

  if (greenSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    greenSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    greenSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }

  if (yellowSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::updateQuantitative3DView(const char *volumeNodeOneID,
                                                              const char *volumeNodeTwoID,
                                                              double ContourLevel,
                                                              double PVPhiMajor,
                                                              double PVPhiMinor,
                                                              double yellowRAS[3],
                                                              double greenRAS[3],
                                                              bool overrideSegments/*=false*/)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView :"
                   " scene not found.";
    return;
    }

  // Set a Plot Layout
  vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(
    this->mrmlScene()->GetFirstNodeByClass("vtkMRMLLayoutNode"));
  if (!layoutNode)
    {
    qWarning() << Q_FUNC_INFO << ": Unable to get layout node!";
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

  vtkMRMLAstroVolumeNode *volumeOne = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeOneID));
  vtkMRMLAstroVolumeNode *volumeTwo = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeTwoID));

  if(!volumeOne || !volumeTwo || !volumeOne->GetImageData() || !volumeTwo->GetImageData())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView() : volumes not valid.";
    return;
    }

  double rms = StringToDouble(volumeOne->GetAttribute("SlicerAstro.3DDisplayThreshold"));
  if (d->PresetOffsetSlider)
    {
    d->PresetOffsetSlider->setValue((rms * ContourLevel) - (rms * 3.));
    }

  if (!d->segmentEditorNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView() : segmentEditorNode not valid.";
    return;
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode || !currentSegmentationNode->GetSegmentation())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView() : segmentationNode not valid.";
    return;
    }

  if (!currentSegmentationNode->GetDisplayNode())
    {
    currentSegmentationNode->CreateDefaultDisplayNodes();
    }

  // Create empty segment in current segmentation
  this->mrmlScene()->SaveStateForUndo();

  if (overrideSegments)
    {
    std::vector<std::string> segmentIds;
    currentSegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIds);

    for (unsigned int ii = 0; ii < segmentIds.size(); ii++)
      {
      size_t foundVolumeOne = segmentIds[ii].find(volumeOne->GetName());
      size_t foundVolumeTwo = segmentIds[ii].find(volumeTwo->GetName());

      if (foundVolumeOne != std::string::npos || foundVolumeTwo != std::string::npos)
        {
        currentSegmentationNode->GetSegmentation()->GlobalWarningDisplayOff();
        currentSegmentationNode->GetSegmentation()->RemoveSegment(segmentIds[ii]);
        currentSegmentationNode->GetSegmentation()->GlobalWarningDisplayOn();
        }
      }
    }

  std::string SegmentOneID = volumeOne->GetName();
  SegmentOneID += "_" + DoubleToString(ContourLevel) + "RMS";
  vtkSegment *SegmentOne = currentSegmentationNode->GetSegmentation()->GetSegment(SegmentOneID);
  if (!SegmentOne)
    {
    double color[3] = {0.5, 0.68, 0.5};
    SegmentOneID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentOneID, SegmentOneID, color);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeOne->GetImageData());
    double min, max;
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.3DDisplayThreshold")) * ContourLevel;
    max = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DATAMAX"));
    imageThreshold->ThresholdBetween(min, max);
    imageThreshold->SetInValue(1);
    imageThreshold->SetOutValue(0);
    imageThreshold->SetOutputScalarType(VTK_SHORT);
    imageThreshold->Update();
    vtkNew<vtkOrientedImageData> modifierLabelmap;
    modifierLabelmap->DeepCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    volumeOne->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
        modifierLabelmap.GetPointer(), currentSegmentationNode, SegmentOneID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MAX))
       {
       qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
       }
    }

  std::string SegmentTwoID = volumeTwo->GetName();
  SegmentTwoID += "_" + DoubleToString(ContourLevel) + "RMS";
  vtkSegment *SegmentTwo = currentSegmentationNode->GetSegmentation()->GetSegment(SegmentTwoID);
  if(!SegmentTwo)
    {
    double color[3] = {0.17, 0.40, 0.57};
    SegmentTwoID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentTwoID, SegmentTwoID, color);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeTwo->GetImageData());
    double min, max;
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.3DDisplayThreshold")) * ContourLevel;
    max = StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX"));
    imageThreshold->ThresholdBetween(min, max);
    imageThreshold->SetInValue(1);
    imageThreshold->SetOutValue(0);
    imageThreshold->SetOutputScalarType(VTK_SHORT);
    imageThreshold->Update();
    vtkNew<vtkOrientedImageData> modifierLabelmap;
    modifierLabelmap->DeepCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    volumeTwo->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
        modifierLabelmap.GetPointer(), currentSegmentationNode, SegmentTwoID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MAX))
      {
      qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
      }
    }

  if (currentSegmentationNode->GetNumberOfDisplayNodes() < 1)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView error :"
                   " SegmentationNode has no displayNodes!"<<endl;
    }

  for (int ii = 0; ii < currentSegmentationNode->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(currentSegmentationNode->GetNthDisplayNode(ii));
    if (!SegmentationDisplayNode)
      {
      continue;
      }
    SegmentationDisplayNode->SetAllSegmentsVisibility(false);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility3D(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentOneID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DOutline(SegmentTwoID, true);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentTwoID, false);
    }

  this->onCreateSurfaceButtonToggled(true);

  // Create PV major
  vtkMRMLSliceNode *yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (!yellowSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::updateQuantitative3DView : "
                  "yellowSliceNode not found!";
    return;
    }

  if (!yellowSliceNode->GetSliceToRAS())
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::updateQuantitative3DView : "
                  "SliceToRAS matrix not found!";
    return;
    }

  yellowSliceNode->SetOrientation("XZ");
  vtkSmartPointer<vtkMatrix3x3> PVMajorMatrix = vtkSmartPointer<vtkMatrix3x3>::New();
  vtkNew<vtkTransform> transformMajor;
  transformMajor->SetMatrix(yellowSliceNode->GetSliceToRAS());
  transformMajor->RotateY(PVPhiMajor);
  for (int ii = 0; ii < 3; ii++)
    {
    for (int jj = 0; jj < 3; jj++)
      {
      PVMajorMatrix->SetElement(ii,jj, transformMajor->GetMatrix()->GetElement(ii,jj));
      }
    }

  if (yellowSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }
  yellowSliceNode->SetOrientation("PVMajor");
  // Translate to X and Y center
  yellowSliceNode->GetSliceToRAS()->SetElement(0, 3, yellowRAS[0]);
  yellowSliceNode->GetSliceToRAS()->SetElement(1, 3, yellowRAS[1]);
  yellowSliceNode->GetSliceToRAS()->SetElement(2, 3, yellowRAS[2]);
  yellowSliceNode->UpdateMatrices();

  // Create PV minor
  vtkMRMLSliceNode *greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (!greenSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::updateQuantitative3DView : "
                  "greenSliceNode not found!";
    return;
    }

  greenSliceNode->SetOrientation("XZ");
  vtkSmartPointer<vtkMatrix3x3> PVMinorMatrix = vtkSmartPointer<vtkMatrix3x3>::New();
  vtkNew<vtkTransform> transformMinor;
  transformMinor->SetMatrix(greenSliceNode->GetSliceToRAS());
  transformMinor->RotateY(PVPhiMinor);
  for (int ii = 0; ii < 3; ii++)
    {
    for (int jj = 0; jj < 3; jj++)
      {
      PVMinorMatrix->SetElement(ii,jj, transformMinor->GetMatrix()->GetElement(ii,jj));
      }
    }

  if (greenSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    greenSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    greenSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
  greenSliceNode->SetOrientation("PVMinor");
  // Translate to X and Y center
  greenSliceNode->GetSliceToRAS()->SetElement(0, 3, greenRAS[0]);
  greenSliceNode->GetSliceToRAS()->SetElement(1, 3, greenRAS[1]);
  greenSliceNode->GetSliceToRAS()->SetElement(2, 3, greenRAS[2]);
  greenSliceNode->UpdateMatrices();

  // Add Presents PV to Red, Yellow and Green slices
  vtkMRMLSliceNode *redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (!redSliceNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "redSliceNode not found!";
    return;
    }

  if (redSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    redSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    redSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }

  if (redSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    redSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    redSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
  redSliceNode->Modified();

  if (greenSliceNode->HasSliceOrientationPreset("PVMajor"))
    {
    greenSliceNode->GetSliceOrientationPreset("PVMajor")->DeepCopy(PVMajorMatrix);
    }
  else
    {
    greenSliceNode->AddSliceOrientationPreset("PVMajor", PVMajorMatrix);
    }

  if (yellowSliceNode->HasSliceOrientationPreset("PVMinor"))
    {
    yellowSliceNode->GetSliceOrientationPreset("PVMinor")->DeepCopy(PVMinorMatrix);
    }
  else
    {
    yellowSliceNode->AddSliceOrientationPreset("PVMinor", PVMinorMatrix);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::stopRockView()
{
  qSlicerApplication* app = qSlicerApplication::application();
  for (int i = 0; i < app->layoutManager()->threeDViewCount(); i++)
    {
    app->layoutManager()->threeDWidget(i)->threeDController()->rockView(false);
  }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::applyPreset(vtkMRMLNode *volumePropertyNode)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->volumeRenderingWidget || !volumePropertyNode)
    {
    return;
    }

  d->volumeRenderingWidget->applyPreset(volumePropertyNode);

  vtkMRMLVolumePropertyNode* newVolumePropertyNode =
    d->volumeRenderingWidget->mrmlVolumePropertyNode();
  if (!newVolumePropertyNode || !d->astroVolumeNode)
    {
    return;
    }
  std::string newVolumePropertyNodeName = newVolumePropertyNode->GetName();   
  std::size_t found = newVolumePropertyNodeName.find(d->astroVolumeNode->GetName());
  if (found == std::string::npos)
    {
    newVolumePropertyNodeName += d->astroVolumeNode->GetName();
    }
  newVolumePropertyNode->SetName(newVolumePropertyNodeName.c_str());

  d->astroVolumeNode->SetVolumePropertyNode(newVolumePropertyNode);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::offsetPreset(double offsetValue)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->volumeRenderingWidget)
    {
    return;
    }

  ctkVTKVolumePropertyWidget* volumePropertyWidget =
     d->volumeRenderingWidget->findChild<ctkVTKVolumePropertyWidget*>
       (QString("VolumeProperty"));

  if (!volumePropertyWidget)
    {
    return;
    }

  volumePropertyWidget->moveAllPoints(offsetValue - d->offsetOldValue, 0., true);
  d->offsetOldValue = offsetValue;

  if (d->Lock)
    {
    d->PresetStretchSlider->setValue(offsetValue);
  }
}

void qSlicerAstroVolumeModuleWidget::onActivate3DAstroVolumeNode(bool activate)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->RenderingFrame)
    {
    return;
    }

  if (activate)
    {
    d->RenderingFrame->setEnabled(true);
    d->RenderingFrame->setCollapsed(false);
    }
  else
    {
    d->RenderingFrame->setEnabled(false);
    d->RenderingFrame->setCollapsed(true);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onCalculateRMS()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode || !d->volumeRenderingWidget)
    {
    return;
    }

  vtkMRMLAnnotationROINode *roiNode = d->volumeRenderingWidget->mrmlROINode();
  if (!roiNode)
    {
    return;
    }

  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  if (!astroVolumeLogic)
    {
    return;
    }

  astroVolumeLogic->Calculate3DDisplayThresholdInROI(roiNode, d->astroVolumeNode);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onCreateHistogram()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLScene *scene = this->mrmlScene();

  if(!d->astroVolumeNode || !d->astroVolumeNode->GetImageData()
     || !scene)
    {
    return;
    }

  double DATAMAX = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMAX"));
  double DATAMIN = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMIN"));
  double DisplayThreshold = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.3DDisplayThreshold"));
  double nBins = d->BinSliderWidget->value();
  double binFlux = (DATAMAX - DATAMIN) / (nBins - 1);

  vtkNew<vtkImageHistogram> histogram;
  histogram->SetAutomaticBinning(false);
  histogram->SetHistogramImageScale(vtkImageHistogram::Log);
  histogram->SetInputData(d->astroVolumeNode->GetImageData());
  histogram->SetBinSpacing(binFlux);
  histogram->SetBinOrigin(DATAMIN);
  histogram->SetNumberOfBins(nBins);
  histogram->Update();

  vtkNew<vtkTable> table;
  vtkNew<vtkMRMLTableNode> tableNode;

  int wasModifying = tableNode->StartModify();
  tableNode->SetAndObserveTable(table.GetPointer());
  tableNode->RemoveAllColumns();
  tableNode->SetUseColumnNameAsColumnHeader(true);
  tableNode->SetDefaultColumnType("double");

  vtkDoubleArray* Intensity = vtkDoubleArray::SafeDownCast(tableNode->AddColumn());
  if (!Intensity)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onCreateHistogram : "
                  "Unable to find the Intensity Column.";
    return;
    }
  Intensity->SetName("Intensity");
  tableNode->SetColumnUnitLabel("Intensity", "Jy/beam");
  tableNode->SetColumnLongName("Intensity", "Intensity axes");

  std::string name = d->astroVolumeNode->GetName();
  name += "_Histogram";
  vtkDoubleArray* Counts = vtkDoubleArray::SafeDownCast(tableNode->AddColumn());
  if (!Counts)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onCreateHistogram : "
                  "Unable to find the Counts Column.";
    return;
    }
  Counts->SetName(name.c_str());
  tableNode->SetColumnUnitLabel(name.c_str(), "Log10(#)");
  tableNode->SetColumnLongName(name.c_str(), "Counts");

  table->SetNumberOfRows(histogram->GetNumberOfBins());
  vtkIdTypeArray* histoArray = histogram->GetHistogram();
  if (!histoArray)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onCreateHistogram : "
                  "Unable to find the Histogram Array.";
    return;
    }

  double histoMaxValue = 0;
  for (int ii = 0; ii < histogram->GetNumberOfBins(); ii++)
     {
     table->SetValue(ii, 0, DATAMIN + ii * binFlux);
     double histoValue = log10(histoArray->GetValue(ii));
     if (DoubleIsInf(histoValue))
       {
       table->SetValue(ii, 1, 0.);
       }
     else
       {
       table->SetValue(ii, 1, histoValue);
       if (histoValue > histoMaxValue)
         {
         histoMaxValue = histoValue;
         }
       }
     }

  tableNode->EndModify(wasModifying);
  scene->AddNode(tableNode.GetPointer());

  if (d->selectionNode)
    {
    d->selectionNode->SetActiveTableID(tableNode->GetID());
    }

  vtkSmartPointer<vtkCollection> PlotSeriesNodes = vtkSmartPointer<vtkCollection>::Take
      (scene->GetNodesByClassByName("vtkMRMLPlotSeriesNode", name.c_str()));
  vtkMRMLPlotSeriesNode *PlotSeriesNode = NULL;
  if (PlotSeriesNodes->GetNumberOfItems() == 0)
    {
    vtkNew<vtkMRMLPlotSeriesNode> newPlotSeriesNode;
    newPlotSeriesNode->SetName(name.c_str());
    double color[4] = {0.926, 0.173, 0.2, 1.};
    newPlotSeriesNode->SetColor(color);
    newPlotSeriesNode->SetPlotType(vtkMRMLPlotSeriesNode::PlotTypeScatterBar);
    scene->AddNode(newPlotSeriesNode.GetPointer());
    PlotSeriesNode = newPlotSeriesNode.GetPointer();
    }
  else
    {
    PlotSeriesNode = vtkMRMLPlotSeriesNode::SafeDownCast
      (PlotSeriesNodes->GetItemAsObject(0));
    }

  PlotSeriesNode->SetAndObserveTableNodeID(tableNode->GetID());
  PlotSeriesNode->SetXColumnName(tableNode->GetColumnName(0));
  PlotSeriesNode->SetYColumnName(tableNode->GetColumnName(1));

  // Connect PlotWidget with AstroVolumeWidget
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onCreateHistogram : "
                   "qSlicerApplication not found!";
    return;
    }

  qSlicerLayoutManager* layoutManager = app->layoutManager();

  if(!app)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onCreateHistogram : "
                   "layoutManager not found!";
    return;
    }

  vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(
    scene->GetFirstNodeByClass("vtkMRMLLayoutNode"));
  if (!layoutNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onCreateHistogram : "
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
    qCritical() << "qSlicerAstroVolumeModuleWidget::onCreateHistogram : "
                   "plotWidget not found!";
    return;
    }

  qMRMLPlotView* plotView = plotWidget->plotView();
  if(!plotWidget)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onCreateHistogram : "
                   "plotView not found!";
    return;
    }

  QObject::connect(plotView, SIGNAL(dataSelected(vtkStringArray*, vtkCollection*)),
                   this, SLOT(onPlotSelectionChanged(vtkStringArray*, vtkCollection*)));

  if (d->TableMinNode && d->TableMaxNode && d->TableThresholdNode)
    {
    d->TableMinNode->GetTable()->SetValue(0, 0, DATAMIN);
    d->TableMinNode->GetTable()->SetValue(0, 1, 0.);
    d->TableMinNode->GetTable()->SetValue(1, 0, DATAMIN);
    d->TableMinNode->GetTable()->SetValue(1, 1, histoMaxValue * 0.5);
    d->TableMinNode->GetTable()->SetValue(2, 0, DATAMIN);
    d->TableMinNode->GetTable()->SetValue(2, 1, histoMaxValue);

    d->TableMaxNode->GetTable()->SetValue(0, 0, DATAMAX);
    d->TableMaxNode->GetTable()->SetValue(0, 1, 0.);
    d->TableMaxNode->GetTable()->SetValue(1, 0, DATAMAX);
    d->TableMaxNode->GetTable()->SetValue(1, 1, histoMaxValue * 0.5);
    d->TableMaxNode->GetTable()->SetValue(2, 0, DATAMAX);
    d->TableMaxNode->GetTable()->SetValue(2, 1, histoMaxValue);

    d->TableThresholdNode->GetTable()->SetValue(0, 0, DisplayThreshold);
    d->TableThresholdNode->GetTable()->SetValue(0, 1, 0.);
    d->TableThresholdNode->GetTable()->SetValue(1, 0, DisplayThreshold);
    d->TableThresholdNode->GetTable()->SetValue(1, 1, histoMaxValue * 0.5);
    d->TableThresholdNode->GetTable()->SetValue(2, 0, DisplayThreshold);
    d->TableThresholdNode->GetTable()->SetValue(2, 1, histoMaxValue);
    }

  if (d->plotChartNodeHistogram)
    {
    d->plotChartNodeHistogram->RemoveAllPlotSeriesNodeIDs();
    d->plotChartNodeHistogram->AddAndObservePlotSeriesNodeID(PlotSeriesNode->GetID());
    }

  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID(d->plotChartNodeHistogram->GetID());
    d->selectionNode->SetActiveTableID(tableNode->GetID());
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onCreateHistogram"
                   " : appLogic not found!";
    return;
    }

  appLogic->PropagatePlotChartSelection();

  if (d->plotChartNodeHistogram)
    {
    d->plotChartNodeHistogram->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeMinLine->GetID());
    d->plotChartNodeHistogram->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeMaxLine->GetID());
    d->plotChartNodeHistogram->AddAndObservePlotSeriesNodeID(d->PlotSeriesNodeThresholdLine->GetID());
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::startRockView()
{
  qSlicerApplication* app = qSlicerApplication::application();
  for (int i = 0; i < app->layoutManager()->threeDViewCount(); i++)
    {
    app->layoutManager()->threeDWidget(i)->threeDController()->rockView(true);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLDisplayROINodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!sender)
    {
    return;
    }

  vtkMRMLAnnotationROINode* ROINode =
      vtkMRMLAnnotationROINode::SafeDownCast(sender);
  if (!ROINode)
    {
    return;
    }

  if(ROINode->GetDisplayVisibility())
    {
    d->ROICropDisplayCheckBox->setChecked(true);
    }
  else
    {
    d->ROICropDisplayCheckBox->setChecked(false);
  }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLLabelVolumeNodeModified()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroLabelVolumeNode)
    {
    return;
    }

  vtkMRMLAstroLabelMapVolumeDisplayNode* astroLabelVolumeDisplayNode =
    d->astroLabelVolumeNode->GetAstroLabelMapVolumeDisplayNode();
  if (!astroLabelVolumeDisplayNode)
    {
    return;
    }

  bool opticalState = d->OpticalVelocityButton->blockSignals(true);
  bool radioState = d->RadioVelocityButton->blockSignals(true);
  d->OpticalVelocityButton->setChecked(false);
  d->RadioVelocityButton->setChecked(false);
  if (!strncmp(astroLabelVolumeDisplayNode->GetVelocityDefinition().c_str(), "VOPT", 4))
    {
    d->OpticalVelocityButton->setChecked(true);
    }
  else if (!strncmp(astroLabelVolumeDisplayNode->GetVelocityDefinition().c_str(), "VRAD", 4))
    {
    d->RadioVelocityButton->setChecked(true);
    }
  d->RadioVelocityButton->blockSignals(radioState);
  d->OpticalVelocityButton->blockSignals(opticalState);

  bool DegreeState = d->DegreeUnitButton->blockSignals(true);
  bool SexagesimalState = d->SexagesimalUnitButton->blockSignals(true);
  d->DegreeUnitButton->setChecked(false);
  d->SexagesimalUnitButton->setChecked(false);
  vtkStringArray* spaceQuantities = astroLabelVolumeDisplayNode->GetSpaceQuantities();
  if (!strcmp(spaceQuantities->GetValue(0).c_str(), "length"))
    {
    d->DegreeUnitButton->setChecked(true);
    }
  else if (!strcmp(spaceQuantities->GetValue(0).c_str(), "time"))
    {
    d->SexagesimalUnitButton->setChecked(true);
    }
  d->DegreeUnitButton->blockSignals(DegreeState);
  d->SexagesimalUnitButton->blockSignals(SexagesimalState);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSceneEndImportEvent()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onMRMLSceneEndImportEvent"
                   " : selectionNode not found!";
    return;
    }

  this->onMRMLSelectionNodeModified(d->selectionNode);
  this->initializeSegmentations();
  this->initializePlotNodes();
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSceneEndCloseEvent()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setMRMLScene : appLogic not found!";
    return;
    }

  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onMRMLSceneEndImportEvent"
                   " : selectionNode not found!";
    return;
  }

  this->initializeSegmentations(true);
  this->initializePlotNodes(true);
  this->initializeColorNodes();
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSceneStartImportEvent()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  d->cleanPointers();
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLVolumeRenderingDisplayNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!sender)
    {
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(sender);

  if (!displayNode)
    {
    return;
    }

  d->VisibilityCheckBox->setChecked(
    displayNode ? displayNode->GetVisibility() : false);

  d->ROICropCheckBox->setChecked(
    displayNode ? displayNode->GetCroppingEnabled() : false);

  d->QualityControlComboBox->setCurrentIndex(
    displayNode ? displayNode->GetPerformanceControl() : -1);

  QSettings settings;
  QString defaultRenderingMethod =
    settings.value("VolumeRendering/RenderingMethod",
                 QString("vtkMRMLCPURayCastVolumeRenderinDisplayNode")).toString();
  QString currentVolumeMapper = displayNode ?
    QString(displayNode->GetClassName()) : defaultRenderingMethod;
  d->RenderingMethodComboBox->setCurrentIndex(
              d->RenderingMethodComboBox->findData(currentVolumeMapper) );
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onPlotSelectionChanged(vtkStringArray *mrmlPlotDataIDs,
                                                            vtkCollection *selectionCol)
{
  Q_D(qSlicerAstroVolumeModuleWidget);
  if (!this->mrmlScene() || !d->astroVolumeNode ||
      !mrmlPlotDataIDs || !selectionCol)
    {
    return;
    }

  vtkTable *histoTable = NULL;
  int minIndex = 0;
  int maxIndex = 0;
  for (int mrmlPlotDataIndex = 0; mrmlPlotDataIndex < mrmlPlotDataIDs->GetNumberOfValues(); mrmlPlotDataIndex++)
    {
    vtkMRMLPlotSeriesNode* PlotSeriesNode = vtkMRMLPlotSeriesNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(mrmlPlotDataIDs->GetValue(mrmlPlotDataIndex)));
    if (!PlotSeriesNode)
      {
      continue;
      }

    std::string name = PlotSeriesNode->GetName();
    if (name.find("_Histogram") != std::string::npos &&
        PlotSeriesNode->GetTableNode())
      {
      histoTable = PlotSeriesNode->GetTableNode()->GetTable();

      vtkIdTypeArray *selectionArray = vtkIdTypeArray::SafeDownCast
        (selectionCol->GetItemAsObject(mrmlPlotDataIndex));
      if (!selectionArray)
        {
        return;
        }

      minIndex = selectionArray->GetValue(0);
      maxIndex = selectionArray->GetValue(selectionArray->GetNumberOfValues() - 1);
      break;
      }
    }

  if (!histoTable)
    {
    return;
    }

  vtkDoubleArray *fluxArray = vtkDoubleArray::SafeDownCast(histoTable->GetColumn(0));
  if (!fluxArray)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onPlotSelectionChanged : "
                  "Unable to find the flux Histogram Array.";
    return;
    }

  d->astroVolumeNode->SetAttribute("SlicerAstro.HistoMinSel",
                                   DoubleToString(fluxArray->GetValue(minIndex)).c_str());
  d->astroVolumeNode->SetAttribute("SlicerAstro.HistoMaxSel",
                                   DoubleToString(fluxArray->GetValue(maxIndex)).c_str());
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onPresetsNodeChanged(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode || !node)
    {
    return;
    }

  d->astroVolumeNode->SetPresetNode(node);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onCurrentQualityControlChanged(int index)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!d->volumeRenderingWidget)
    {
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
      d->volumeRenderingWidget->mrmlDisplayNode();

  if (!displayNode)
    {
    return;
    }

  displayNode->SetPerformanceControl(index);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setDisplayROIEnabled(bool visibility)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!d->volumeRenderingWidget)
    {
    return;
    }

  vtkMRMLAnnotationROINode* ROINode =
      d->volumeRenderingWidget->mrmlDisplayNode()->GetROINode();

  if (!ROINode)
    {
    return;
    }

  int n = ROINode->GetNumberOfDisplayNodes();
  int wasModifying [n];
  for(int i = 0; i < n; i++)
    {
    wasModifying[i] = ROINode->GetNthDisplayNode(i)->StartModify();
    }

 ROINode->SetDisplayVisibility(visibility);

  for(int i = 0; i < n; i++)
    {
    ROINode->GetNthDisplayNode(i)->EndModify(wasModifying[i]);
  }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setOpticalVelocity()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->ActiveVolumeNodeSelector || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLNode *node = d->ActiveVolumeNodeSelector->currentNode();
  vtkMRMLAstroVolumeNode *volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  vtkMRMLAstroLabelMapVolumeNode *volumeLabelNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node);
  if (!volumeNode && !volumeLabelNode)
    {
    return;
    }

  bool updateSlice = false;
  if (volumeNode)
    {
    vtkMRMLAstroVolumeDisplayNode* volumeDisplayNode =
      volumeNode->GetAstroVolumeDisplayNode();
    if (!volumeDisplayNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::setRadioVelocity : "
                    "volumeDisplayNode not found.";
      return;
      }
    if (volumeDisplayNode->SetOpticalVelocityDefinition())
      {
      volumeNode->Modified();
      updateSlice = true;
      }
    }
  else if (volumeLabelNode)
    {
    vtkMRMLAstroLabelMapVolumeDisplayNode* volumeLabelDisplayNode =
      volumeLabelNode->GetAstroLabelMapVolumeDisplayNode();
    if (!volumeLabelDisplayNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::setRadioVelocity : "
                    "volumeLabelDisplayNode not found.";
      return;
      }
    if (volumeLabelDisplayNode->SetOpticalVelocityDefinition())
      {
      volumeLabelNode->Modified();
      updateSlice = true;
      }
    }

  if (updateSlice)
    {
    vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
        (this->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode"));
    for(int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
      {
      vtkMRMLSliceNode* sliceNode =
          vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i));
      if (sliceNode)
        {
        sliceNode->Modified();
        }
      }
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setRADegreeUnit()
{
 Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->ActiveVolumeNodeSelector || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLNode *node = d->ActiveVolumeNodeSelector->currentNode();
  vtkMRMLAstroVolumeNode *volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  vtkMRMLAstroLabelMapVolumeNode *volumeLabelNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node);
  if (!volumeNode && !volumeLabelNode)
    {
    return;
    }

  bool updateSlice = false;
  if (volumeNode)
    {
    vtkMRMLAstroVolumeDisplayNode* volumeDisplayNode =
      volumeNode->GetAstroVolumeDisplayNode();
    if (!volumeDisplayNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::setRADegreeUnit : "
                    "volumeDisplayNode not found.";
      return;
      }
    volumeDisplayNode->SetSpaceQuantity(0, "length");
    volumeNode->Modified();
    updateSlice = true;
    }
  else if (volumeLabelNode)
    {
    vtkMRMLAstroLabelMapVolumeDisplayNode* volumeLabelDisplayNode =
      volumeLabelNode->GetAstroLabelMapVolumeDisplayNode();
    if (!volumeLabelDisplayNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::setRADegreeUnit : "
                    "volumeLabelDisplayNode not found.";
      return;
      }
    volumeLabelDisplayNode->SetSpaceQuantity(0, "length");
    volumeLabelNode->Modified();
    updateSlice = true;
    }

  if (updateSlice)
    {
    vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
        (this->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode"));
    for(int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
      {
      vtkMRMLSliceNode* sliceNode =
          vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i));
      if (sliceNode)
        {
        sliceNode->Modified();
        }
      }
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setRASexagesimalUnit()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->ActiveVolumeNodeSelector || !this->mrmlScene())
    {
    return;
    }

  vtkMRMLNode *node = d->ActiveVolumeNodeSelector->currentNode();
  vtkMRMLAstroVolumeNode *volumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  vtkMRMLAstroLabelMapVolumeNode *volumeLabelNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node);
  if (!volumeNode && !volumeLabelNode)
    {
    return;
    }

  bool updateSlice = false;
  if (volumeNode)
    {
    vtkMRMLAstroVolumeDisplayNode* volumeDisplayNode =
      volumeNode->GetAstroVolumeDisplayNode();
    if (!volumeDisplayNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::setRADegreeUnit : "
                    "volumeDisplayNode not found.";
      return;
      }
    volumeDisplayNode->SetSpaceQuantity(0, "time");
    volumeNode->Modified();
    updateSlice = true;
    }
  else if (volumeLabelNode)
    {
    vtkMRMLAstroLabelMapVolumeDisplayNode* volumeLabelDisplayNode =
      volumeLabelNode->GetAstroLabelMapVolumeDisplayNode();
    if (!volumeLabelDisplayNode)
      {
      qCritical() <<"qSlicerAstroVolumeModuleWidget::setRADegreeUnit : "
                    "volumeLabelDisplayNode not found.";
      return;
      }
    volumeLabelDisplayNode->SetSpaceQuantity(0, "time");
    volumeLabelNode->Modified();
    updateSlice = true;
    }

  if (updateSlice)
    {
    vtkSmartPointer<vtkCollection> sliceNodes = vtkSmartPointer<vtkCollection>::Take
        (this->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode"));
    for(int i = 0; i < sliceNodes->GetNumberOfItems(); i++)
      {
      vtkMRMLSliceNode* sliceNode =
          vtkMRMLSliceNode::SafeDownCast(sliceNodes->GetItemAsObject(i));
      if (sliceNode)
        {
        sliceNode->Modified();
        }
      }
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onCropToggled(bool crop)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!d->volumeRenderingWidget)
    {
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
      d->volumeRenderingWidget->mrmlDisplayNode();

  if (!displayNode)
    {
    return;
    }

  displayNode->SetCroppingEnabled(crop);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onDeactivate3DLabelMapVolumeNode(bool deactivate)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->RenderingFrame)
    {
    return;
    }

  if (deactivate)
    {
    d->RenderingFrame->setEnabled(false);
    d->RenderingFrame->setCollapsed(true);
    }
  else
    {
    d->RenderingFrame->setEnabled(true);
    d->RenderingFrame->setCollapsed(false);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onEditSelectedSegment()
{
  qSlicerModuleManager * moduleManager = qSlicerCoreApplication::application()->moduleManager();
  if (!moduleManager)
    {
    return;
    }
  qSlicerAbstractCoreModule * module = moduleManager->module("SegmentEditor");
  if(!module)
    {
    QMessageBox::warning(
      this, this->tr("Raising %1 Module:").arg("Segment Editor"),
      this->tr("Unfortunately, this requested module is not available in this Slicer session."),
      QMessageBox::Ok);
    return;
    }
  qSlicerLayoutManager * layoutManager = qSlicerApplication::application()->layoutManager();
  if (!layoutManager)
    {
    return;
    }
  layoutManager->setCurrentModule("SegmentEditor");
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onHistoClippingChanged(double percentage)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->plotChartNodeHistogram ||
      !d->TableMaxNode || !d->TableMinNode)
    {
    return;
    }

  vtkTable *histoTable = NULL;
  for (int ii = 0; ii < d->plotChartNodeHistogram->GetNumberOfPlotSeriesNodes(); ii++)
    {
    vtkMRMLPlotSeriesNode *PlotSeriesNode = d->plotChartNodeHistogram->GetNthPlotSeriesNode(ii);
    if (!PlotSeriesNode)
      {
      continue;
      }
    std::string name = PlotSeriesNode->GetName();
    if (name.find("_Histogram") != std::string::npos &&
        PlotSeriesNode->GetTableNode())
      {
      histoTable = PlotSeriesNode->GetTableNode()->GetTable();
      break;
      }
    }

  if (!histoTable)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onHistoClippingChanged : "
                  "Unable to find the Histogram.";
    return;
    }

  vtkDoubleArray *fluxArray = vtkDoubleArray::SafeDownCast(histoTable->GetColumn(0));
  if (!fluxArray)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onHistoClippingChanged : "
                  "Unable to find the flux Histogram Array.";
    return;
    }

  vtkDoubleArray *histoArray = vtkDoubleArray::SafeDownCast(histoTable->GetColumn(1));
  if (!histoArray)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onHistoClippingChanged : "
                  "Unable to find the Histogram Array.";
    return;
    }

  double totContsLeft = 0;
  int iiLeft = 0;
  while (fluxArray->GetValue(iiLeft) < 0.)
    {
    totContsLeft += pow(10,histoArray->GetValue(iiLeft));
    iiLeft++;
    }

  double parContsLeft = 0;
  iiLeft = 0;
  while (parContsLeft / totContsLeft < (1 - percentage))
    {
    parContsLeft += pow(10,histoArray->GetValue(iiLeft));
    iiLeft++;
    }

  double totContsRight = 0;
  int iiRight = histoArray->GetNumberOfValues() - 1;
  while (fluxArray->GetValue(iiRight) > 0.)
    {
    totContsRight += pow(10,histoArray->GetValue(iiRight));
    iiRight--;
    }

  double parContsRight = 0;
  iiRight = histoArray->GetNumberOfValues() - 1;
  while (parContsRight / totContsRight < (1 - percentage))
    {
    parContsRight += pow(10,histoArray->GetValue(iiRight));
    iiRight--;
    }

  double TwoDColorFunctionMax = fluxArray->GetValue(iiRight);
  double TwoDColorFunctionMin = fluxArray->GetValue(iiLeft);

  d->TableMaxNode->GetTable()->SetValue(1, 0, TwoDColorFunctionMax);
  d->TableMaxNode->GetTable()->Modified();

  d->TableMinNode->GetTable()->SetValue(1, 0, TwoDColorFunctionMin);
  d->TableMinNode->GetTable()->Modified();

}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onHistoClippingChanged1()
{
  this->onHistoClippingChanged(0.95);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onHistoClippingChanged2()
{
  this->onHistoClippingChanged(0.98);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onHistoClippingChanged3()
{
  this->onHistoClippingChanged(0.99);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onHistoClippingChanged4()
{
  this->onHistoClippingChanged(0.995);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onHistoClippingChanged5()
{
  this->onHistoClippingChanged(0.999);
}
//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setDisplayConnection(vtkMRMLNode *node)
{
  vtkMRMLVolumeRenderingDisplayNode *displayNode =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(node);
  if(!displayNode)
    {
    return;
    }

  vtkCollection *col = this->mrmlScene()->GetNodesByClass("vtkMRMLVolumeRenderingDisplayNode");
  unsigned int numNodes = col->GetNumberOfItems();
  for (unsigned int n = 0; n < numNodes; n++)
    {
    vtkMRMLVolumeRenderingDisplayNode *displayNodeIter =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(col->GetItemAsObject(n));
    if (displayNodeIter)
      {
      // is this the display node?
      if (displayNode->GetID() && displayNodeIter->GetID() && strcmp(displayNode->GetID(), displayNodeIter->GetID()) == 0)
        {
        // don't disconnect
        // qDebug() << "\tskipping disconnecting " << node->GetID();
        continue;
        }
      this->qvtkDisconnect(displayNodeIter, vtkCommand::ModifiedEvent,
                           this, SLOT(onMRMLVolumeRenderingDisplayNodeModified(vtkObject*)));
      this->qvtkDisconnect(displayNodeIter->GetROINode(), vtkMRMLDisplayableNode::DisplayModifiedEvent,
                           this, SLOT(onMRMLDisplayROINodeModified(vtkObject*)));
      }
    }
  col->RemoveAllItems();
  col->Delete();

  this->qvtkConnect(displayNode, vtkCommand::ModifiedEvent,
                    this, SLOT(onMRMLVolumeRenderingDisplayNodeModified(vtkObject*)));
  this->qvtkConnect(displayNode->GetROINode(), vtkMRMLDisplayableNode::DisplayModifiedEvent,
                    this, SLOT(onMRMLDisplayROINodeModified(vtkObject*)));

  this->onMRMLVolumeRenderingDisplayNodeModified(displayNode);
  this->onMRMLDisplayROINodeModified(displayNode->GetROINode());
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLSelectionNode *selectionNode =
      vtkMRMLSelectionNode::SafeDownCast(sender);

  if (!selectionNode || !this->mrmlScene())
    {
    return;
    }
  char *activeVolumeNodeID = selectionNode->GetActiveVolumeID();
  char *activeLabelMapVolumeNodeID = selectionNode->GetActiveLabelVolumeID();

  vtkMRMLNode *activeVolumeNode = this->mrmlScene()->GetNodeByID(activeVolumeNodeID);
  vtkMRMLNode *activeLabelMapVolumeNode = this->mrmlScene()->GetNodeByID(activeLabelMapVolumeNodeID);

  if(activeVolumeNode && activeLabelMapVolumeNode)
    {
    if(activeVolumeNode->GetMTime() > activeLabelMapVolumeNode->GetMTime())
      {
      this->setMRMLVolumeNode(activeVolumeNode);
      }
    else
      {
      this->setMRMLVolumeNode(activeLabelMapVolumeNode);
      }
    }
  else if (activeVolumeNode)
    {
    this->setMRMLVolumeNode(activeVolumeNode);
    }
  else if (activeLabelMapVolumeNode)
    {
    this->setMRMLVolumeNode(activeLabelMapVolumeNode);
    }

  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());

  if (!astroVolumeLogic)
    {
    return;
    }

  astroVolumeLogic->updateUnitsNodes(activeVolumeNode);
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSelectionNodeReferenceAdded(vtkObject *sender)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

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
    emit segmentEditorNodeChanged(false);
    return;
    }

  d->segmentEditorNode = segmentEditorNode;
  emit segmentEditorNodeChanged(true);
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSelectionNodeReferenceRemoved(vtkObject *sender)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

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
    emit segmentEditorNodeChanged(false);
    return;
    }

  d->segmentEditorNode = segmentEditorNode;
  emit segmentEditorNodeChanged(true);
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLTableMaxNodeModified()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->TableThresholdNode || !d->astroVolumeNode)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = d->astroVolumeNode->GetAstroVolumeDisplayNode();
  if (!astroDisplay)
    {
    return;
    }

  // click and drag can shift only one points
  // here we check which one has been shifted
  // and we shift the other two as well

  double max = astroDisplay->GetWindowLevelMax();
  double min = astroDisplay->GetWindowLevelMin();

  double value1 = d->TableMaxNode->GetTable()->GetValue(0, 0).ToDouble();
  double value2 = d->TableMaxNode->GetTable()->GetValue(1, 0).ToDouble();
  double value3 = d->TableMaxNode->GetTable()->GetValue(2, 0).ToDouble();

  if (fabs(value1 - value2) > 1.E-6 &&
      fabs(value1 - value3) > 1.E-6)
    {
    if ((value1 - min) < 1.E-6)
      {
      value1 = min;
      }
    d->TableMaxNode->GetTable()->SetValue(0, 0, value1);
    d->TableMaxNode->GetTable()->SetValue(1, 0, value1);
    d->TableMaxNode->GetTable()->SetValue(2, 0, value1);
    max = value1;
    }
  else if (fabs(value2 - value1) > 1.E-6 &&
           fabs(value2 - value3) > 1.E-6)
    {
    if ((value2 - min) < 1.E-6)
      {
      value2 = min;
      }
    d->TableMaxNode->GetTable()->SetValue(0, 0, value2);
    d->TableMaxNode->GetTable()->SetValue(1, 0, value2);
    d->TableMaxNode->GetTable()->SetValue(2, 0, value2);
    max = value2;
    }
  else if (fabs(value3 - value1) > 1.E-6 &&
           fabs(value3 - value2) > 1.E-6)
    {
    if ((value3 - min) < 1.E-6)
      {
      value3 = min;
      }
    d->TableMaxNode->GetTable()->SetValue(0, 0, value3);
    d->TableMaxNode->GetTable()->SetValue(1, 0, value3);
    d->TableMaxNode->GetTable()->SetValue(2, 0, value3);
    max = value3;
    }
  else
    {
    return;
    }

  double window = max - min;
  double level = 0.5 * (max + min);

  astroDisplay->SetWindowLevel(window, level);
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLTableMinNodeModified()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->TableThresholdNode || !d->astroVolumeNode)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = d->astroVolumeNode->GetAstroVolumeDisplayNode();
  if (!astroDisplay)
    {
    return;
    }

  // click and drag can shift only one points
  // here we check which one has been shifted
  // and we shift the other two as well

  double min = astroDisplay->GetWindowLevelMin();
  double max = astroDisplay->GetWindowLevelMax();

  double value1 = d->TableMinNode->GetTable()->GetValue(0, 0).ToDouble();
  double value2 = d->TableMinNode->GetTable()->GetValue(1, 0).ToDouble();
  double value3 = d->TableMinNode->GetTable()->GetValue(2, 0).ToDouble();

  if (fabs(value1 - value2) > 1.E-6 &&
      fabs(value1 - value3) > 1.E-6)
    {
    if ((value1 - max) > 1.E-6)
      {
      value1 = max;
      }
    d->TableMinNode->GetTable()->SetValue(0, 0, value1);
    d->TableMinNode->GetTable()->SetValue(1, 0, value1);
    d->TableMinNode->GetTable()->SetValue(2, 0, value1);
    min = value1;
    }
  else if (fabs(value2 - value1) > 1.E-6 &&
           fabs(value2 - value3) > 1.E-6)
    {
    if ((value2 - max) > 1.E-6)
      {
      value2 = max;
      }
    d->TableMinNode->GetTable()->SetValue(0, 0, value2);
    d->TableMinNode->GetTable()->SetValue(1, 0, value2);
    d->TableMinNode->GetTable()->SetValue(2, 0, value2);
    min = value2;
    }
  else if (fabs(value3 - value1) > 1.E-6 &&
           fabs(value3 - value2) > 1.E-6)
    {
    if ((value3 - max) > 1.E-6)
      {
      value3 = max;
      }
    d->TableMinNode->GetTable()->SetValue(0, 0, value3);
    d->TableMinNode->GetTable()->SetValue(1, 0, value3);
    d->TableMinNode->GetTable()->SetValue(2, 0, value3);
    min = value3;
    }
  else
    {
    return;
    }

  double window = max - min;
  double level = 0.5 * (max + min);

  astroDisplay->SetWindowLevel(window, level);
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLTableThresholdNodeModified()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->TableThresholdNode || !d->astroVolumeNode)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = d->astroVolumeNode->GetAstroVolumeDisplayNode();
  if (!astroDisplay)
    {
    return;
    }

  // click and drag can shift only one points
  // here we check which one has been shifted
  // and we shift the other two as well

  double DisplayThreshold = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.3DDisplayThreshold"));

  double value1 = d->TableThresholdNode->GetTable()->GetValue(0, 0).ToDouble();
  double value2 = d->TableThresholdNode->GetTable()->GetValue(1, 0).ToDouble();
  double value3 = d->TableThresholdNode->GetTable()->GetValue(2, 0).ToDouble();

  if (fabs(value1 - value2) > 1.E-6 &&
      fabs(value1 - value3) > 1.E-6)
    {
    if (value1 < 0.)
      {
      value1 = 0.;
      }
    d->TableThresholdNode->GetTable()->SetValue(0, 0, value1);
    d->TableThresholdNode->GetTable()->SetValue(1, 0, value1);
    d->TableThresholdNode->GetTable()->SetValue(2, 0, value1);
    DisplayThreshold = value1;
    }
  else if (fabs(value2 - value1) > 1.E-6 &&
           fabs(value2 - value3) > 1.E-6)
    {
    if (value2 < 0.)
      {
      value2 = 0.;
      }
    d->TableThresholdNode->GetTable()->SetValue(0, 0, value2);
    d->TableThresholdNode->GetTable()->SetValue(1, 0, value2);
    d->TableThresholdNode->GetTable()->SetValue(2, 0, value2);
    DisplayThreshold = value2;
    }
  else if (fabs(value3 - value1) > 1.E-6 &&
           fabs(value3 - value2) > 1.E-6)
    {
    if (value3 < 0.)
      {
      value3 = 0.;
      }
    d->TableThresholdNode->GetTable()->SetValue(0, 0, value3);
    d->TableThresholdNode->GetTable()->SetValue(1, 0, value3);
    d->TableThresholdNode->GetTable()->SetValue(2, 0, value3);
    DisplayThreshold = value3;
    }
  else
    {
    return;
    }

  d->astroVolumeNode->Set3DDisplayThreshold(DisplayThreshold);
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLVolumeNodeModified()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode || !d->volumeRenderingWidget)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroVolumeDisplayNode =
    d->astroVolumeNode->GetAstroVolumeDisplayNode();
  if (!astroVolumeDisplayNode)
    {
    return;
    }

  bool opticalState = d->OpticalVelocityButton->blockSignals(true);
  bool radioState = d->RadioVelocityButton->blockSignals(true);
  d->OpticalVelocityButton->setChecked(false);
  d->RadioVelocityButton->setChecked(false);
  if (!strncmp(astroVolumeDisplayNode->GetVelocityDefinition().c_str(), "VOPT", 4))
    {
    d->OpticalVelocityButton->setChecked(true);
    }
  else if (!strncmp(astroVolumeDisplayNode->GetVelocityDefinition().c_str(), "VRAD", 4))
    {
    d->RadioVelocityButton->setChecked(true);
    }
  d->RadioVelocityButton->blockSignals(radioState);
  d->OpticalVelocityButton->blockSignals(opticalState);

  bool DegreeState = d->DegreeUnitButton->blockSignals(true);
  bool SexagesimalState = d->SexagesimalUnitButton->blockSignals(true);
  d->DegreeUnitButton->setChecked(false);
  d->SexagesimalUnitButton->setChecked(false);
  vtkStringArray* spaceQuantities = astroVolumeDisplayNode->GetSpaceQuantities();
  if (!strcmp(spaceQuantities->GetValue(0).c_str(), "length"))
    {
    d->DegreeUnitButton->setChecked(true);
    }
  else if (!strcmp(spaceQuantities->GetValue(0).c_str(), "time"))
    {
    d->SexagesimalUnitButton->setChecked(true);
    }
  d->DegreeUnitButton->blockSignals(DegreeState);
  d->SexagesimalUnitButton->blockSignals(SexagesimalState);

  vtkMRMLUnitNode* unitNode = NULL;
  if (d->selectionNode)
    {
    unitNode = d->selectionNode->GetUnitNode("intensity");
    }

  std::string MIN = d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMIN");
  std::string MAX = d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMAX");
  if (unitNode)
    {
    double DataMin = StringToDouble(MIN.c_str());
    double DataMax = StringToDouble(MAX.c_str());
    d->DataMinDisplay->setText(unitNode->GetDisplayStringFromValue(DataMin));
    d->DataMaxDisplay->setText(unitNode->GetDisplayStringFromValue(DataMax));
    }
  else
    {
    d->DataMinDisplay->setText(MIN.c_str());
    d->DataMaxDisplay->setText(MAX.c_str());
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLVolumeDisplayNodeModified()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* astroDisplay = d->astroVolumeNode->GetAstroVolumeDisplayNode();
  if (!astroDisplay || !d->TableMaxNode || !d->TableMinNode)
    {
    return;
    }

  double TwoDColorFunctionMax = astroDisplay->GetWindowLevelMax();
  d->TableMaxNode->GetTable()->SetValue(1, 0, TwoDColorFunctionMax);
  d->TableMaxNode->GetTable()->Modified();

  double TwoDColorFunctionMin = astroDisplay->GetWindowLevelMin();
  d->TableMinNode->GetTable()->SetValue(1, 0, TwoDColorFunctionMin);
  d->TableMinNode->GetTable()->Modified();
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLVolumeNodeDisplayThresholdModified(bool forcePreset /*= true*/)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode)
    {
    return;
    }

  double DisplayThreshold = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.3DDisplayThreshold"));
  if (forcePreset)
    {
    d->DisplayThresholdSliderWidget->setValue(DisplayThreshold);
    }
  else
    {
    int status = d->DisplayThresholdSliderWidget->blockSignals(true);
    d->DisplayThresholdSliderWidget->setValue(DisplayThreshold);
    d->DisplayThresholdSliderWidget->blockSignals(status);
    }
  double max = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMAX"));
  d->DisplayThresholdSliderWidget->setMaximum(max);
  double min = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMIN"));
  if (min < 0.)
    {
    min = 0.;
    }
  d->DisplayThresholdSliderWidget->setMinimum(min);
  d->DisplayThresholdSliderWidget->setSingleStep((max - min) / 10000.);
  QString DisplayThresholdUnit = "  ";
  DisplayThresholdUnit += d->astroVolumeNode->GetAttribute("SlicerAstro.BUNIT");
  d->DisplayThresholdSliderWidget->setSuffix(DisplayThresholdUnit);

  this->resetOffset(d->astroVolumeNode);
  this->resetStretch(d->astroVolumeNode);

  if (forcePreset)
    {
    this->updatePresets(d->astroVolumeNode);
    this->applyPreset(d->astroVolumeNode->GetPresetNode());
    }
  else if (d->astroVolumeNode->GetVolumePropertyNode())
    {
    this->applyPreset(d->astroVolumeNode->GetVolumePropertyNode());
    }

  if (d->TableThresholdNode)
    {
    d->TableThresholdNode->GetTable()->SetValue(1, 0, DisplayThreshold);
    d->TableThresholdNode->GetTable()->Modified();
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setMRMLVolumeNode(vtkMRMLNode* node)
{
  if (!node)
    {
    return;
    }

  vtkMRMLAstroLabelMapVolumeNode* labelMapVolumeNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node);
  vtkMRMLAstroVolumeNode* astroVolumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (astroVolumeNode)
    {
    this->setMRMLVolumeNode(astroVolumeNode);
    }
  else if (labelMapVolumeNode)
    {
    this->setMRMLVolumeNode(labelMapVolumeNode);
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setMRMLVolumeNode(vtkMRMLAstroVolumeNode* volumeNode)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!volumeNode)
    {
    return;
    }

  d->astroVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast(volumeNode);

  int n = StringToInt(d->astroVolumeNode->GetAttribute("SlicerAstro.NAXIS"));
  // Check Input volume dimensionality
  if (n == 3)
    {
    d->RenderingFrame->setEnabled(true);
    d->RenderingFrame->setCollapsed(false);
    if (d->volumeRenderingWidget)
      {
      d->volumeRenderingWidget->setMRMLVolumeNode(d->astroVolumeNode);
      }

    d->Lock = false;
    d->LockPushButton->setChecked(false);
    d->LockPushButton->setIcon(QIcon(":Icons/astrounlock.png"));
    d->PresetsNodeComboBox->setCurrentNodeIndex(-1);
    bool forcePreset = false;
    if (d->astroVolumeNode->GetVolumePropertyNode() == NULL)
      {
      d->PresetsNodeComboBox->setCurrentNodeIndex(-1);
      d->PresetsNodeComboBox->setCurrentNodeIndex(0);
      forcePreset = true;
      }
    this->qvtkReconnect(d->astroVolumeNode, vtkMRMLAstroVolumeNode::DisplayThresholdModifiedEvent,
                        this, SLOT(onMRMLVolumeNodeDisplayThresholdModified()));
    this->onMRMLVolumeNodeDisplayThresholdModified(forcePreset);
    this->onVisibilityChanged(true);
    }
  this->qvtkReconnect(d->astroVolumeNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLVolumeNodeModified()));
  this->onMRMLVolumeNodeModified();
  this->qvtkReconnect(d->astroVolumeNode->GetAstroVolumeDisplayNode(), vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLVolumeDisplayNodeModified()));
  this->onMRMLVolumeDisplayNodeModified();

  d->ActiveVolumeNodeSelector->setCurrentNodeID(volumeNode->GetID());

  this->setEnabled(volumeNode != 0);
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode* volumeNode)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!volumeNode || d->astroLabelVolumeNode == volumeNode)
    {
    return;
    }

  d->astroLabelVolumeNode = vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(volumeNode);
  this->qvtkReconnect(d->astroLabelVolumeNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLLabelVolumeNodeModified()));
  this->onMRMLLabelVolumeNodeModified();

  d->ActiveVolumeNodeSelector->setCurrentNodeID(volumeNode->GetID());

  this->setEnabled(volumeNode != 0);
}

//--------------------------------------------------------------------------
qSlicerVolumeRenderingModuleWidget* qSlicerAstroVolumeModuleWidget::volumeRenderingWidget() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);

    return d->volumeRenderingWidget;
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::enter()
{
  this->Superclass::enter();
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::exit()
{
  this->Superclass::exit();
}

//--------------------------------------------------------------------------
vtkMRMLVolumeRenderingDisplayNode* qSlicerAstroVolumeModuleWidget::volumeRenderingDisplay() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->volumeRenderingWidget->mrmlDisplayNode();
}

//--------------------------------------------------------------------------
qMRMLAstroVolumeInfoWidget* qSlicerAstroVolumeModuleWidget::astroVolumeInfoWidget() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->MRMLAstroVolumeInfoWidget;
}

//--------------------------------------------------------------------------
qSlicerAstroVolumeDisplayWidget* qSlicerAstroVolumeModuleWidget::astroVolumeDisplayWidget() const
{
  Q_D(const qSlicerAstroVolumeModuleWidget);
  return d->AstroVolumeDisplayWidget;
}
