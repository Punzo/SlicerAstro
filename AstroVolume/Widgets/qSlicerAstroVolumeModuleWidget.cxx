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
#include <QDoubleSpinBox>
#include <QString>
#include <QStringList>

// CTK includes
#include <ctkUtils.h>
#include <ctkDoubleRangeSlider.h>
#include <ctkRangeWidget.h>
#include <ctkVTKScalarsToColorsWidget.h>
#include <ctkVTKVolumePropertyWidget.h>

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkImageThreshold.h>
#include <vtkIntArray.h>
#include <vtkLookupTable.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPlotPoints.h>
#include <vtkPointData.h>
#include <vtkRenderer.h>
#include <vtkTable.h>
#include <vtkTransform.h>
#include <vtkVariant.h>
#include <vtkVolumeProperty.h>

// qMRMLWidgets include
#include <qMRMLAnnotationROIWidget.h>
#include <qMRMLAstroVolumeInfoWidget.h>
#include <qMRMLPlotView.h>
#include <qMRMLPlotWidget.h>
#include <qMRMLVolumePropertyNodeWidget.h>
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
#include <qSlicerVolumeRenderingPresetComboBox.h>
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
#include <vtkMRMLPlotViewNode.h>
#include <vtkMRMLProceduralColorNode.h>
#include <vtkMRMLProceduralColorStorageNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceLogic.h>
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
  vtkSlicerVolumeRenderingLogic* volumeRenderingLogic;
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
  double OpacityScaling;
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
  this->OpacityScaling = 1.;
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
  QObject::connect(this->VisibilityCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onVisibilityChanged(bool)));

  this->volumeRenderingLogic = vtkSlicerVolumeRenderingLogic::SafeDownCast(volumeRendering->logic());
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
                   q, SLOT(onCurrentRenderingMethodChanged(int)));

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

  QObject::connect(this->PresetStretchSlider, SIGNAL(valueChanged(double)),
                   q, SLOT(spreadPreset(double)));

  QObject::connect(this->LockPushButton, SIGNAL(toggled(bool)),
                   q, SLOT(onLockToggled(bool)));

  QObject::connect(this->ROICropDisplayCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(setDisplayROIEnabled(bool)));

  QObject::connect(this->ROICropDisplayCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onROICropDisplayCheckBoxToggled(bool)));

  QObject::connect(this->ROICropCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onCropToggled(bool)));

  QObject::connect(this->ROIFitPushButton, SIGNAL(clicked()),
                   q, SLOT(fitROIToVolume()));

  QObject::connect(this->SynchronizeScalarDisplayNodeButton, SIGNAL(clicked()),
                   q, SLOT(synchronizeScalarDisplayNode()));

  QObject::connect(this->SynchronizeScalarDisplayNodeButton, SIGNAL(clicked()),
                   q, SLOT(clearPresets()));

  QObject::connect(this->OpacitySliderWidget, SIGNAL(valueChanged(double)),
                   q, SLOT(onOpacityValueChanged(double)));

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
                   this->SegmentsTableView, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(this->pushButtonCovertLabelMapToSegmentation, SIGNAL(clicked()),
                   q, SLOT(onPushButtonCovertLabelMapToSegmentationClicked()));

  QObject::connect(this->pushButtonConvertSegmentationToLabelMap, SIGNAL(clicked()),
                   q, SLOT(onPushButtonConvertSegmentationToLabelMapClicked()));

  QObject::connect(this->pushButton_EditSelected, SIGNAL(clicked()),
                   q, SLOT(onEditSelectedSegment()));

  this->SegmentsTableView->setSelectionMode(QAbstractItemView::MultiSelection);
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

  if (this->SegmentsTableView)
    {
    if (this->SegmentsTableView->segmentationNode())
      {
      q->mrmlScene()->RemoveNode(this->SegmentsTableView->segmentationNode());
      this->SegmentsTableView->setSegmentationNode(NULL);
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

  if (!scene)
    {
    return;
    }

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

  this->initializeSegmentations();
  this->initializePlotNodes();
  this->initializeColorNodes();

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceAddedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceAdded(vtkObject*)));
  this->qvtkReconnect(d->selectionNode, vtkMRMLNode::ReferenceRemovedEvent,
                      this, SLOT(onMRMLSelectionNodeReferenceRemoved(vtkObject*)));

  this->onMRMLSelectionNodeModified(d->selectionNode);
  this->onMRMLSelectionNodeReferenceAdded(d->selectionNode);

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
    vtkMRMLSegmentationNode* sceneSegmentationNode =
      vtkMRMLSegmentationNode::SafeDownCast(segmentationNode);
    sceneSegmentationNode->CreateDefaultDisplayNodes();
    d->segmentEditorNode->SetAndObserveSegmentationNode(sceneSegmentationNode);
    }

  d->segmentEditorNode->GetSegmentationNode()->CreateDefaultDisplayNodes();
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
  colorNodes = vtkSmartPointer<vtkCollection>::Take(
      this->mrmlScene()->GetNodesByClass("vtkMRMLColorTableNode"));
  for (int ii = 0; ii < colorNodes->GetNumberOfItems(); ii++)
    {
    vtkMRMLColorTableNode* tempColorTableNode = vtkMRMLColorTableNode::SafeDownCast
            (colorNodes->GetItemAsObject(ii));
    if (!tempColorTableNode)
      {
      continue;
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
  colorNodes = vtkSmartPointer<vtkCollection>::Take(
      this->mrmlScene()->GetNodesByClass("vtkMRMLColorTableNode"));
  for (int ii = 0; ii < colorNodes->GetNumberOfItems(); ii++)
    {
    vtkMRMLColorTableNode* tempColorTableNode = vtkMRMLColorTableNode::SafeDownCast
            (colorNodes->GetItemAsObject(ii));
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
      }
    }

  // Ensure Lookup Table is updated
  colorNodes = vtkSmartPointer<vtkCollection>::Take(
      this->mrmlScene()->GetNodesByClass("vtkMRMLColorTableNode"));
  for (int ii = 0; ii < colorNodes->GetNumberOfItems(); ii++)
    {
    vtkMRMLColorTableNode* tempColorTableNode = vtkMRMLColorTableNode::SafeDownCast
            (colorNodes->GetItemAsObject(ii));
    if (!tempColorTableNode)
      {
      continue;
      }
    tempColorTableNode->GetLookupTable()->Modified();
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
      d->plotChartNodeHistogram->SetXAxisTitle("Intensity");
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

  this->qvtkReconnect(d->plotChartNodeHistogram, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLPlotChartNodeHistogramModified()));
  this->onMRMLPlotChartNodeHistogramModified();

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
      double color[4] = {0.926, 0.173, 0.2, 1.};
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
      yAxis->SetName("DisplayThreshold");

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
    d->RenderingFrame->setEnabled(false);
    d->RenderingFrame->setCollapsed(true);
    d->pushButtonCovertLabelMapToSegmentation->setEnabled(true);
    d->HistoCollapsibleButton->setEnabled(false);
    d->HistoCollapsibleButton->setCollapsed(true);
    }

  vtkMRMLAstroLabelMapVolumeNode* labelMapVolumeNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node);
  vtkMRMLAstroVolumeNode* astroVolumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast(node);

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
      d->VelocityLabel->setEnabled(true);
      d->OpticalVelocityButton->setEnabled(true);
      d->RadioVelocityButton->setEnabled(true);
      d->RenderingFrame->setEnabled(true);
      d->RenderingFrame->setCollapsed(false);
      }
    else
      {
      d->VelocityLabel->setEnabled(false);
      d->OpticalVelocityButton->setEnabled(false);
      d->RadioVelocityButton->setEnabled(false);
      d->RenderingFrame->setEnabled(false);
      d->RenderingFrame->setCollapsed(true);
      }

    d->pushButtonCovertLabelMapToSegmentation->setEnabled(false);
    d->HistoCollapsibleButton->setEnabled(true);

    astroVolumeNode->Modified();
    d->selectionNode->SetReferenceActiveVolumeID(astroVolumeNode->GetID());
    d->selectionNode->SetActiveVolumeID(astroVolumeNode->GetID());
    }
  else if (labelMapVolumeNode)
    {
    d->RenderingFrame->setEnabled(false);
    d->RenderingFrame->setCollapsed(true);
    d->pushButtonCovertLabelMapToSegmentation->setEnabled(true);
    d->HistoCollapsibleButton->setEnabled(false);
    d->HistoCollapsibleButton->setCollapsed(true);
    labelMapVolumeNode->Modified();
    d->selectionNode->SetReferenceActiveLabelVolumeID(labelMapVolumeNode->GetID());
    d->selectionNode->SetActiveLabelVolumeID(labelMapVolumeNode->GetID());
    }
  else
    {
    d->selectionNode->SetReferenceActiveVolumeID(NULL);
    d->selectionNode->SetActiveVolumeID(NULL);
    d->selectionNode->SetReferenceActiveLabelVolumeID(NULL);
    d->selectionNode->SetActiveLabelVolumeID(NULL);
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
void qSlicerAstroVolumeModuleWidget::resetOpacities()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  bool wasBlocking = d->OpacitySliderWidget->blockSignals(true);
  d->OpacitySliderWidget->setValue(1.);
  d->OpacitySliderWidget->blockSignals(wasBlocking);
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
void qSlicerAstroVolumeModuleWidget::updateWidgetsFromIntensityNode()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
    this->mrmlScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (!selectionNode)
    {
    return;
    }

  vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode("intensity");
  if (!unitNode)
    {
    return;
    }

  if(!d->volumeRenderingWidget)
    {
    return;
    }

  qMRMLVolumePropertyNodeWidget* volumePropertyNodeWidget =
    d->volumeRenderingWidget->findChild<qMRMLVolumePropertyNodeWidget*>
      (QString("VolumePropertyNodeWidget"));
  if (!volumePropertyNodeWidget)
    {
    return;
    }

  ctkVTKVolumePropertyWidget* volumePropertyWidget =
      volumePropertyNodeWidget->findChild<ctkVTKVolumePropertyWidget*>
      (QString("VolumePropertyWidget"));
  if (!volumePropertyWidget)
    {
    return;
    }

  ctkVTKScalarsToColorsWidget* opacityWidget =
      volumePropertyWidget->findChild<ctkVTKScalarsToColorsWidget*>
      (QString("ScalarOpacityWidget"));
  if (!opacityWidget)
    {
    return;
    }

  QDoubleSpinBox* XSpinBox = opacityWidget->findChild<QDoubleSpinBox*>
      (QString("XSpinBox"));
  if (!opacityWidget)
    {
    return;
    }

  XSpinBox->setDecimals(unitNode->GetPrecision());

  ctkDoubleRangeSlider* XRangeSlider = opacityWidget->findChild<ctkDoubleRangeSlider*>
      (QString("XRangeSlider"));
  if (!XRangeSlider)
    {
    return;
    }

  XRangeSlider->setRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());
  XRangeSlider->setSingleStep((unitNode->GetMaximumValue() - unitNode->GetMinimumValue()) / 100.);

  opacityWidget->setXRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());

  ctkVTKScalarsToColorsWidget* colorWidget =
      volumePropertyWidget->findChild<ctkVTKScalarsToColorsWidget*>
      (QString("ScalarColorWidget"));
  if (!colorWidget)
    {
    return;
    }

  QDoubleSpinBox* XSpinBox1 = colorWidget->findChild<QDoubleSpinBox*>
      (QString("XSpinBox"));
  if (!colorWidget)
    {
    return;
    }

  XSpinBox1->setDecimals(unitNode->GetPrecision());

  ctkDoubleRangeSlider* XRangeSlider1 = colorWidget->findChild<ctkDoubleRangeSlider*>
      (QString("XRangeSlider"));
  if (!XRangeSlider1)
    {
    return;
    }

  XRangeSlider1->setRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());
  XRangeSlider1->setSingleStep((unitNode->GetMaximumValue() - unitNode->GetMinimumValue()) / 100.);

  colorWidget->setXRange(unitNode->GetMinimumValue(), unitNode->GetMaximumValue());
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
void qSlicerAstroVolumeModuleWidget::synchronizeScalarDisplayNode()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!d->astroVolumeNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::synchronizeScalarDisplayNode : "
                   "inputVolume not found!";
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
        d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode();
  if (!displayNode || !d->volumeRenderingLogic)
    {
    return;
    }

  d->volumeRenderingLogic->CopyDisplayToVolumeRenderingDisplayNode(displayNode);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::spreadPreset(double stretchValue)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->volumeRenderingWidget)
    {
    return;
    }

  qMRMLVolumePropertyNodeWidget* volumePropertyNodeWidget =
    d->volumeRenderingWidget->findChild<qMRMLVolumePropertyNodeWidget*>
      (QString("VolumePropertyNodeWidget"));
  if (!volumePropertyNodeWidget)
    {
    return;
    }

  ctkVTKVolumePropertyWidget* volumePropertyWidget =
     volumePropertyNodeWidget->findChild<ctkVTKVolumePropertyWidget*>
       (QString("VolumePropertyWidget"));
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
    return;
    }

  vtkMRMLSegmentationNode* segmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!segmentationNode || !segmentationNode->GetSegmentation())
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationNode";
    return;
    }
  vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(
    segmentationNode->GetDisplayNode());
  if (!displayNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentationDisplayNode";
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

  if (!this->mrmlScene())
    {
    return;
    }

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
      return;
      }
    }

  labelMapNode->UpdateRangeAttributes();

  currentSegmentationNode->CreateDefaultDisplayNodes();

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

  if (!this->mrmlScene())
    {
    return;
    }

  if (!d->segmentEditorNode)
    {
    qCritical() << Q_FUNC_INFO << ": segmentEditorNode not found.";
    return;
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode || !currentSegmentationNode->GetSegmentation())
    {
    QString message = QString("No segmentation available!");
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    return;
    }

  // Get or create LabelMapVolumeNode (Mask)
  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> labelMapNode;

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
    return;
    }

  // Export selected segments into labelmap volume
  std::vector<std::string> segmentIDs;
  currentSegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIDs);

  if (segmentIDs.size() < 1)
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              "Be sure that segments are available in the table view. \n\n").
                                arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    return;
    }

  QStringList selectedSegmentIDs = d->SegmentsTableView->selectedSegmentIDs();

  if (selectedSegmentIDs.size() < 1)
    {
    QString message = QString("No segment selected from the segmentation node! "
                              "All the segments will be exported in the mask, do you want to procede?  \n\n"
                              "If you do not desire to export all of them, "
                              "please provide one or more segments by selecting them (Shift + leftClick).");
    qWarning() << Q_FUNC_INFO << ": " << message;
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(NULL, tr("Failed to select a segment"), message);
    if (reply != QMessageBox::Yes)
      {
      return;
      }
    }

  if (selectedSegmentIDs.size() > 0)
    {
    segmentIDs.clear();
    for (int segmentIndex = 0; segmentIndex < selectedSegmentIDs.size(); segmentIndex++)
      {
      segmentIDs.push_back(selectedSegmentIDs[segmentIndex].toStdString());
      }
    }

  if (vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode(currentSegmentationNode, segmentIDs, labelMapNode, activeVolumeNode))
    {
    // update labelMapVolume and remove segments from current segmentation if export was successful
    labelMapNode->UpdateRangeAttributes();
    for (unsigned int ii = 0; ii < segmentIDs.size(); ii++)
      {
      currentSegmentationNode->GetSegmentation()->RemoveSegment(segmentIDs[ii]);
      }
    }
  else
    {
    QString message = QString("Failed to export segments from segmentation %1 to representation node %2!\n\n"
                              "Be sure that segment to export has been selected in the table view (left click). \n\n").
                              arg(currentSegmentationNode->GetName()).arg(labelMapNode->GetName());
    qCritical() << Q_FUNC_INFO << ": " << message;
    QMessageBox::warning(NULL, tr("Failed to export segment"), message);
    this->mrmlScene()->RemoveNode(labelMapNode);
    return;
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onDisplayThresholdValueChanged(double DisplayThreshold)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode)
    {
    return;
    }

  d->astroVolumeNode->SetDisplayThreshold(DisplayThreshold);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onVisibilityChanged(bool visibility)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!d->astroVolumeNode || !d->astroVolumeNode->GetImageData())
    {
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
        d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode();
  if (!displayNode)
    {
    return;
    }

  displayNode->SetVisibility(visibility);

  if (!this->mrmlScene() || !visibility)
    {
    return;
    }

  // Set the camera position
  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));

  int numCameraNodes = cameraNodes->GetNumberOfItems();
  if (numCameraNodes < 1)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onVisibilityChanged : "
                   "cameraNode not found!";
    return;
    }

  for (int cameraIndex = 0; cameraIndex < numCameraNodes; cameraIndex++)
    {
    vtkMRMLCameraNode *cameraNode =
      vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(cameraIndex));
    if (!cameraNode)
      {
      continue;
      }
    double Origin[3];
    d->astroVolumeNode->GetOrigin(Origin);
    int* dims = d->astroVolumeNode->GetImageData()->GetDimensions();
    // In RAS the z axes is on the second index
    Origin[0] = 0.;
    Origin[1] = dims[2] * 2 + sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
    Origin[2] = 0.;
    cameraNode->SetPosition(Origin);
    double ViewUp[3];
    ViewUp[0] = 0.;
    ViewUp[1] = 0.;
    ViewUp[2] = 1.;
    cameraNode->SetViewUp(ViewUp);
    double FocalPoint[3];
    FocalPoint[0] = 0.;
    FocalPoint[1] = 0.;
    FocalPoint[2] = 0.;
    cameraNode->SetFocalPoint(FocalPoint);
    }

  // Reset the 3D rendering boundaries
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app || !app->layoutManager())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onVisibilityChanged : "
                   "qSlicerApplication not found.";
    return;
    }

  for (int ii = 0; ii < app->layoutManager()->threeDViewCount(); ii++)
    {   
    qMRMLThreeDWidget* ThreeDWidget = app->layoutManager()->threeDWidget(ii);
    if(!ThreeDWidget)
      {
      qCritical() << "qSlicerAstroVolumeModuleWidget::onVisibilityChanged : "
                     "ThreeDWidget not found.";
      return;
      }

    qMRMLThreeDView* ThreeDView = ThreeDWidget->threeDView();
    if(!ThreeDView || !ThreeDView->renderer())
      {
      qCritical() << "qSlicerAstroVolumeModuleWidget::onVisibilityChanged : "
                     "ThreeDView not found.";
      return;
      }

    ThreeDView->renderer()->ResetCameraClippingRange();
    ThreeDView->renderer()->Render();

    qMRMLThreeDViewControllerWidget* ThreeDController = ThreeDWidget->threeDController();
    if(!ThreeDView || !ThreeDView->renderer())
      {
      qCritical() << "qSlicerAstroVolumeModuleWidget::onVisibilityChanged : "
                     "ThreeDController not found.";
      return;
      }
    ThreeDController->resetFocalPoint();
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

  if(!app || !app->layoutManager() || !app->layoutManager()->layoutLogic()
     || !app->layoutManager()->layoutLogic()->GetLayoutNode())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "qSlicerApplication not found.";
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

  bool autoPropagate = d->AutoPropagateCheckBox->isChecked();
  d->AutoPropagateCheckBox->setChecked(false);

  vtkMRMLAstroVolumeNode *volumeOne = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeOneID));
  vtkMRMLAstroVolumeNode *volumeTwo = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeTwoID));

  if(!volumeOne || !volumeTwo ||
     !volumeOne->GetImageData() || !volumeTwo->GetImageData())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews :"
                   " volumes not valid.";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setComparative3DViews : appLogic not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  if (!d->selectionNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setComparative3DViews : selectionNode not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  vtkSmartPointer<vtkCollection> col = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLViewNode"));

  d->selectionNode->SetActiveLabelVolumeID("");
  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());

  unsigned int numViewNodes = col->GetNumberOfItems();
  int renderingQuality = 0;

  vtkMRMLVolumeRenderingDisplayNode *volumeOneRenderingDisplay =
    volumeOne->GetAstroVolumeRenderingDisplayNode();
  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->RemoveAllViewNodeIDs();

    for (unsigned int viewIndex = 0; viewIndex < numViewNodes; viewIndex++)
      {
      vtkMRMLViewNode *viewNodeIter =
          vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(viewIndex));
      if (!viewNodeIter)
        {
        continue;
        }

      volumeOneRenderingDisplay->AddViewNodeID(viewNodeIter->GetID());
      break;
      }

    renderingQuality = volumeOneRenderingDisplay->GetPerformanceControl();
    }

  this->updatePresets(volumeOne);

  d->selectionNode->SetActiveVolumeID(volumeTwo->GetID());

  vtkMRMLVolumeRenderingDisplayNode *volumeTwoRenderingDisplay =
    volumeTwo->GetAstroVolumeRenderingDisplayNode();
  if (volumeTwoRenderingDisplay)
    {
    volumeTwoRenderingDisplay->RemoveAllViewNodeIDs();

    bool second = false;
    for (unsigned int viewIndex = 0; viewIndex < numViewNodes; viewIndex++)
      {
      vtkMRMLViewNode *viewNodeIter =
          vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(viewIndex));
      if (!viewNodeIter)
        {
        continue;
        }

      if(second)
        {
        volumeTwoRenderingDisplay->AddViewNodeID(viewNodeIter->GetID());
        break;
        }
      second = true;
      }

    volumeTwoRenderingDisplay->SetVisibility(true);
    volumeTwoRenderingDisplay->SetPerformanceControl(renderingQuality);
    }

  this->updatePresets(volumeTwo);

  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());

  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(true);
    }

  if (overlay2D)
    {
    d->selectionNode->SetSecondaryVolumeID(volumeTwo->GetID());
    }
  else
    {
    d->selectionNode->SetSecondaryVolumeID("");
    }

  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));
  int numCameraNodes = cameraNodes->GetNumberOfItems();

  vtkMRMLViewNode *View1 = vtkMRMLViewNode::SafeDownCast
    (mrmlScene()->GetNodeByID("vtkMRMLViewNode1"));
  if (View1)
    {
    for (int cameraIndex = 0; cameraIndex < numCameraNodes; cameraIndex++)
      {
      vtkMRMLCameraNode *cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(cameraIndex));
      if (!cameraNode)
        {
        continue;
        }

      if (cameraNode->GetActiveTag() &&
          !strcmp(cameraNode->GetActiveTag(), View1->GetID()))
        {
        int* dims = volumeOne->GetImageData()->GetDimensions();
        // In RAS the z axes is on the second index
        double Origin[3] = {0.};
        Origin[1] = dims[2] * 2 + sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
        cameraNode->SetPosition(Origin);
        double ViewUp[3] = {0.};
        ViewUp[2] = 1.;
        cameraNode->SetViewUp(ViewUp);
        double FocalPoint[3] = {0.};
        cameraNode->SetFocalPoint(FocalPoint);
        break;
        }
      }
    }

  vtkMRMLViewNode *View2 = vtkMRMLViewNode::SafeDownCast
    (mrmlScene()->GetNodeByID("vtkMRMLViewNode2"));
  if (View2)
    {
    for (int cameraIndex = 0; cameraIndex < numCameraNodes; cameraIndex++)
      {
      vtkMRMLCameraNode *cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(cameraIndex));
      if (!cameraNode)
        {
        continue;
        }

      if (cameraNode->GetActiveTag() &&
          !strcmp(cameraNode->GetActiveTag(), View2->GetID()))
        {
        int* dims = volumeTwo->GetImageData()->GetDimensions();
        // In RAS the z axes is on the second index
        double Origin[3] = {0.};
        Origin[1] = dims[2] * 2 + sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
        cameraNode->SetPosition(Origin);
        double ViewUp[3] = {0.};
        ViewUp[2] = 1.;
        cameraNode->SetViewUp(ViewUp);
        double FocalPoint[3] = {0.};
        cameraNode->SetFocalPoint(FocalPoint);
        break;
        }
      }
    }

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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  qMRMLThreeDView* ThreeDView1 = ThreeDWidget1->threeDView();
  if(!ThreeDView1 || !ThreeDView1->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDView1 not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  ThreeDView1->renderer()->ResetCameraClippingRange();
  ThreeDView1->renderer()->Render();

  qMRMLThreeDWidget* ThreeDWidget2 = app->layoutManager()->threeDWidget(1);
  if(!ThreeDWidget2)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDWidget2 not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  qMRMLThreeDView* ThreeDView2 = ThreeDWidget2->threeDView();
  if(!ThreeDView2 || !ThreeDView2->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                   "ThreeDView2 not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  ThreeDView2->renderer()->ResetCameraClippingRange();
  ThreeDView2->renderer()->Render();

  if (!generateMasks)
    {
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  // Create Segmentations
  if (!d->segmentEditorNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setComparative3DViews : "
                  "segmentEditorNode not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews() :"
                   " segmentation not found.";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
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

  // Create empty segment in current segmentation
  this->mrmlScene()->SaveStateForUndo();

  std::string SegmentOneID = volumeOne->GetName();
  SegmentOneID += "_3RMS";
  vtkSegment *SegmentOne = currentSegmentationNode->GetSegmentation()->GetSegment(SegmentOneID);
  if(!SegmentOne)
    {
    double color[3] = {0.5, 0.68, 0.5};
    SegmentOneID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentOneID, SegmentOneID, color);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeOne->GetImageData());
    double min, max;
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold")) * 3.;
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
    double color[3] = {1., 0.9, 0.13};
    SegmentTwoID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentTwoID, SegmentTwoID, color);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeTwo->GetImageData());
    double min, max;
    min = StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DisplayThreshold")) * 3.;
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
  d->AutoPropagateCheckBox->setChecked(autoPropagate);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setThreeComparativeView(const char *volumeNodeOneID,
                                                             const char *volumeNodeTwoID,
                                                             const char *volumeNodeThreeID)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

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

  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews :"
                   " scene not found.";
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews :"
                   " appLogic not found!";
    return;
    }

  bool autoPropagate = d->AutoPropagateCheckBox->isChecked();
  d->AutoPropagateCheckBox->setChecked(false);

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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeOneRenderingDisplay =
    volumeOne->GetAstroVolumeRenderingDisplayNode();
  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(false);
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeTwoRenderingDisplay =
    volumeTwo->GetAstroVolumeRenderingDisplayNode();
  if (volumeTwoRenderingDisplay)
    {
    volumeTwoRenderingDisplay->SetVisibility(false);
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeThreeRenderingDisplay =
    volumeThree->GetAstroVolumeRenderingDisplayNode();
  if (volumeThreeRenderingDisplay)
    {
    volumeThreeRenderingDisplay->SetVisibility(false);
    }

  volumeTwo->SetDisplayVisibility(0);
  volumeThree->SetDisplayVisibility(0);
  volumeOne->SetDisplayVisibility(0);

  if (!d->selectionNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : selectionNode not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  // Update 3D Renderings
  d->selectionNode->SetActiveLabelVolumeID("");
  d->selectionNode->SetActiveVolumeID(volumeTwo->GetID());
  this->updatePresets(volumeTwo);

  d->selectionNode->SetActiveVolumeID(volumeThree->GetID());
  this->updatePresets(volumeThree);

  d->selectionNode->SetSecondaryVolumeID(volumeTwo->GetID());
  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());
  this->updatePresets(volumeOne);

  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));
  int numCameraNodes = cameraNodes->GetNumberOfItems();

  vtkMRMLViewNode *View1 = vtkMRMLViewNode::SafeDownCast
    (mrmlScene()->GetNodeByID("vtkMRMLViewNode1"));
  if (View1)
    {
    for (int cameraIndex = 0; cameraIndex < numCameraNodes; cameraIndex++)
      {
      vtkMRMLCameraNode *cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(cameraIndex));
      if (!cameraNode)
        {
        continue;
        }

      if (cameraNode->GetActiveTag() &&
          !strcmp(cameraNode->GetActiveTag(), View1->GetID()))
        {
        int* dims = volumeOne->GetImageData()->GetDimensions();
        // In RAS the z axes is on the second index
        double Origin[3] = {0.};
        Origin[1] = dims[2] * 2 + sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
        cameraNode->SetPosition(Origin);
        double ViewUp[3] = {0.};
        ViewUp[2] = 1.;
        cameraNode->SetViewUp(ViewUp);
        double FocalPoint[3] = {0.};
        cameraNode->SetFocalPoint(FocalPoint);
        break;
        }
      }
    }

  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(true);
    }

  qMRMLThreeDWidget* ThreeDWidget = app->layoutManager()->threeDWidget(0);
  if(!ThreeDWidget)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDWidget not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  qMRMLThreeDView* ThreeDView = ThreeDWidget->threeDView();
  if(!ThreeDView || !ThreeDView->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDView not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  ThreeDView->renderer()->ResetCameraClippingRange();
  ThreeDView->renderer()->Render();

  vtkMRMLSliceCompositeNode* redSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
  if (redSliceComposite)
    {
    redSliceComposite->SetForegroundVolumeID("");
    redSliceComposite->SetForegroundOpacity(0.);
    redSliceComposite->SetBackgroundVolumeID(volumeNodeOneID);
    redSliceComposite->SetLabelVolumeID("");
    }

  vtkMRMLSliceNode* redSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
  if (redSliceNode)
    {
    // setting to the XZ orientation is needed in order to force the refresh
    redSliceNode->SetOrientation("XZ");
    redSliceNode->SetOrientation("XY");
    redSliceNode->SetSliceOffset(0.);
    redSliceNode->SetRulerType(redSliceNode->RulerTypeThin);
    }

  vtkMRMLSliceLogic* redSliceLogic = appLogic->GetSliceLogic(redSliceNode);
  if (redSliceLogic)
    {
    int *dimsSliceRed = redSliceNode->GetDimensions();
    redSliceLogic->FitSliceToAll(dimsSliceRed[0], dimsSliceRed[1]);
    redSliceLogic->SnapSliceOffsetToIJK();
    }

  vtkMRMLSliceCompositeNode* yellowSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
  if (yellowSliceComposite)
    {
    yellowSliceComposite->SetForegroundVolumeID("");
    yellowSliceComposite->SetForegroundOpacity(0.);
    yellowSliceComposite->SetBackgroundVolumeID(volumeNodeTwoID);
    yellowSliceComposite->SetLabelVolumeID("");
    }

  vtkMRMLSliceNode* yellowSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
  if (yellowSliceNode)
    {
    // setting to the XZ orientation is needed in order to force the refresh
    yellowSliceNode->SetOrientation("XZ");
    yellowSliceNode->SetOrientation("XY");
    yellowSliceNode->SetSliceOffset(0.);
    yellowSliceNode->SetRulerType(yellowSliceNode->RulerTypeThin);
    }

  vtkMRMLSliceLogic* yellowSliceLogic = appLogic->GetSliceLogic(yellowSliceNode);
  if (yellowSliceLogic)
    {
    int *dimsSliceYellow = yellowSliceNode->GetDimensions();
    yellowSliceLogic->FitSliceToAll(dimsSliceYellow[0], dimsSliceYellow[1]);
    yellowSliceLogic->SnapSliceOffsetToIJK();
    }

  vtkMRMLSliceCompositeNode* greenSliceComposite = vtkMRMLSliceCompositeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
  if (greenSliceComposite)
    {
    greenSliceComposite->SetForegroundVolumeID("");
    greenSliceComposite->SetForegroundOpacity(0.);
    greenSliceComposite->SetBackgroundVolumeID(volumeNodeThreeID);
    greenSliceComposite->SetLabelVolumeID("");
    }

  vtkMRMLSliceNode* greenSliceNode = vtkMRMLSliceNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));
  if (greenSliceNode)
    {
    // setting to the XZ orientation is needed in order to force the refresh
    greenSliceNode->SetOrientation("XZ");
    greenSliceNode->SetOrientation("XY");
    greenSliceNode->SetSliceOffset(0.);
    greenSliceNode->SetRulerType(greenSliceNode->RulerTypeThin);
    }

  vtkMRMLSliceLogic* greenSliceLogic = appLogic->GetSliceLogic(greenSliceNode);
  if (greenSliceLogic)
    {
    int *dimsSliceGreen = greenSliceNode->GetDimensions();
    greenSliceLogic->FitSliceToAll(dimsSliceGreen[0], dimsSliceGreen[1]);
    greenSliceLogic->SnapSliceOffsetToIJK();
    }

  d->AutoPropagateCheckBox->setChecked(autoPropagate);
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

  bool autoPropagate = d->AutoPropagateCheckBox->isChecked();
  d->AutoPropagateCheckBox->setChecked(false);

  // Set a Plot Layout
  vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(
    this->mrmlScene()->GetFirstNodeByClass("vtkMRMLLayoutNode"));
  if (!layoutNode)
    {
    qWarning() << Q_FUNC_INFO << ": Unable to get layout node!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeOneRenderingDisplay =
    volumeOne->GetAstroVolumeRenderingDisplayNode();
  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(false);
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeTwoRenderingDisplay =
    volumeTwo->GetAstroVolumeRenderingDisplayNode();
  if (volumeTwoRenderingDisplay)
    {
    volumeTwoRenderingDisplay->SetVisibility(false);
    }

  vtkMRMLVolumeRenderingDisplayNode *volumeThreeRenderingDisplay =
    volumeThree->GetAstroVolumeRenderingDisplayNode();
  if (volumeThreeRenderingDisplay)
    {
    volumeThreeRenderingDisplay->SetVisibility(false);
    }

  volumeTwo->SetDisplayVisibility(0);
  volumeThree->SetDisplayVisibility(0);
  volumeOne->SetDisplayVisibility(0);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : appLogic not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  if (!d->selectionNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : selectionNode not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  // Update 3D Renderings
  d->selectionNode->SetActiveLabelVolumeID("");
  d->selectionNode->SetActiveVolumeID(volumeTwo->GetID());
  this->updatePresets(volumeTwo);

  d->selectionNode->SetActiveVolumeID(volumeThree->GetID());
  this->updatePresets(volumeThree);

  d->selectionNode->SetSecondaryVolumeID(volumeTwo->GetID());
  d->selectionNode->SetActiveVolumeID(volumeOne->GetID());
  this->updatePresets(volumeOne);

  vtkSmartPointer<vtkCollection> cameraNodes = vtkSmartPointer<vtkCollection>::Take
      (this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode"));
  int numCameraNodes = cameraNodes->GetNumberOfItems();

  vtkMRMLViewNode *View1 = vtkMRMLViewNode::SafeDownCast
    (mrmlScene()->GetNodeByID("vtkMRMLViewNode1"));
  if (View1)
    {
    for (int cameraIndex = 0; cameraIndex < numCameraNodes; cameraIndex++)
      {
      vtkMRMLCameraNode *cameraNode =
        vtkMRMLCameraNode::SafeDownCast(cameraNodes->GetItemAsObject(cameraIndex));
      if (!cameraNode)
        {
        continue;
        }

      if (cameraNode->GetActiveTag() &&
          !strcmp(cameraNode->GetActiveTag(), View1->GetID()))
        {
        int* dims = volumeOne->GetImageData()->GetDimensions();
        // In RAS the z axes is on the second index
        double Origin[3] = {0.};
        Origin[1] = dims[2] * 2 + sqrt(dims[0] * dims[0] + dims[1] * dims[1]);
        cameraNode->SetPosition(Origin);
        double ViewUp[3] = {0.};
        ViewUp[2] = 1.;
        cameraNode->SetViewUp(ViewUp);
        double FocalPoint[3] = {0.};
        cameraNode->SetFocalPoint(FocalPoint);
        break;
        }
      }
    }

  if (volumeOneRenderingDisplay)
    {
    volumeOneRenderingDisplay->SetVisibility(true);
    }

  // reset the 3D rendering boundaries
  qSlicerApplication* app = qSlicerApplication::application();

  if(!app)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "qSlicerApplication not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  qMRMLThreeDWidget* ThreeDWidget = app->layoutManager()->threeDWidget(0);
  if(!ThreeDWidget)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDWidget not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  qMRMLThreeDView* ThreeDView = ThreeDWidget->threeDView();
  if(!ThreeDView || !ThreeDView->renderer())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                   "ThreeDView not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  ThreeDView->renderer()->ResetCameraClippingRange();
  ThreeDView->renderer()->Render();

  // Create Segmentations
  if (!d->segmentEditorNode)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::setQuantitative3DView : "
                  "segmentEditorNode not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode->GetSegmentation())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView() :"
                   " segmentation not found.";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
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
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold")) * ContourLevel;
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
    double color[3] = {1., 0.9, 0.13};
    SegmentTwoID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentTwoID, SegmentTwoID, color);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeTwo->GetImageData());
    double min, max;
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold")) * ContourLevel;
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
    double color[3] = {0.73, 0.06, 0.59};
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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
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

  d->AutoPropagateCheckBox->setChecked(autoPropagate);
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

  bool autoPropagate = d->AutoPropagateCheckBox->isChecked();
  d->AutoPropagateCheckBox->setChecked(false);

  // Set a Plot Layout
  vtkMRMLLayoutNode* layoutNode = vtkMRMLLayoutNode::SafeDownCast(
    this->mrmlScene()->GetFirstNodeByClass("vtkMRMLLayoutNode"));
  if (!layoutNode)
    {
    qWarning() << Q_FUNC_INFO << ": Unable to get layout node!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  double rms = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold"));
  if (d->PresetOffsetSlider)
    {
    d->PresetOffsetSlider->setValue((rms * ContourLevel) - (rms * 3.));
    }

  if (!d->segmentEditorNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView() : segmentEditorNode not valid.";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  vtkMRMLSegmentationNode* currentSegmentationNode = d->segmentEditorNode->GetSegmentationNode();
  if (!currentSegmentationNode || !currentSegmentationNode->GetSegmentation())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView() : segmentationNode not valid.";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
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
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold")) * ContourLevel;
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
    double color[3] = {1., 0.9, 0.13};
    SegmentTwoID = currentSegmentationNode->GetSegmentation()->AddEmptySegment(SegmentTwoID, SegmentTwoID, color);

    vtkNew<vtkImageThreshold> imageThreshold;
    imageThreshold->SetInputData(volumeTwo->GetImageData());
    double min, max;
    min = StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold")) * ContourLevel;
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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
    return;
    }

  if (!yellowSliceNode->GetSliceToRAS())
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::updateQuantitative3DView : "
                  "SliceToRAS matrix not found!";
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
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
    d->AutoPropagateCheckBox->setChecked(autoPropagate);
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

  d->AutoPropagateCheckBox->setChecked(autoPropagate);
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

  qSlicerVolumeRenderingPresetComboBox *PresetComboBox =
    d->volumeRenderingWidget->findChild<qSlicerVolumeRenderingPresetComboBox*>
      (QString("PresetComboBox"));
  if (!PresetComboBox)
    {
    return;
    }

  PresetComboBox->applyPreset(volumePropertyNode);

  qMRMLVolumePropertyNodeWidget* volumePropertyNodeWidget =
    d->volumeRenderingWidget->findChild<qMRMLVolumePropertyNodeWidget*>
      (QString("VolumePropertyNodeWidget"));
  if (!volumePropertyNodeWidget)
    {
    return;
    }

  ctkVTKVolumePropertyWidget* volumePropertyWidget =
     volumePropertyNodeWidget->findChild<ctkVTKVolumePropertyWidget*>
       (QString("VolumePropertyWidget"));
  if (!volumePropertyWidget)
    {
    return;
    }

  vtkVolumeProperty* volumeProperty = volumePropertyWidget->volumeProperty();
  if (!volumeProperty)
    {
    return;
    }

  vtkPiecewiseFunction* opacities = volumeProperty->GetScalarOpacity();
  if (!opacities)
    {
    return;
    }

  for (int ii = 0; ii < opacities->GetSize() - 1; ii++)
    {
    double value[4];
    opacities->GetNodeValue(ii, value);
    value[1] *= d->OpacityScaling;
    if (value[1] > 1.)
      {
      value[1] = 1.;
      }
    if (value[1] < 0.01)
      {
      value[1] = 0.;
      }

    opacities->SetNodeValue(ii, value);
    }

  volumeProperty->Modified();

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
void qSlicerAstroVolumeModuleWidget::fitROIToVolume()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!d->astroVolumeNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::fitROIToVolume : "
                   "inputVolume not found!";
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
        d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode();
  if (!displayNode || !d->volumeRenderingLogic)
    {
    return;
    }

  // Fit ROI to volume
  d->volumeRenderingLogic->FitROIToVolume(displayNode);

  if (!d->volumeRenderingWidget)
    {
    return;
    }

  qMRMLAnnotationROIWidget* ROIWidget =
    d->volumeRenderingWidget->findChild<qMRMLAnnotationROIWidget*>
      (QString("ROIWidget"));
  if (!ROIWidget)
    {
    return;
    }

  if ( ROIWidget->mrmlROINode() != d->volumeRenderingWidget->mrmlROINode()
       || ROIWidget->mrmlROINode() != displayNode->GetROINode())
    {
    qCritical() << Q_FUNC_INFO << ": ROI node mismatch";
    return;
    }

  if (!ROIWidget->mrmlROINode())
    {
    return;
    }

  double xyz[3] = {0.0};
  double rxyz[3] = {0.0};

  ROIWidget->mrmlROINode()->GetXYZ(xyz);
  ROIWidget->mrmlROINode()->GetRadiusXYZ(rxyz);

  double bounds[6] = {0.0};
  for (int ii= 0; ii < 3; ++ii)
    {
    bounds[ii]   = xyz[ii] - rxyz[ii];
    bounds[3+ii] = xyz[ii] + rxyz[ii];
    }

  ROIWidget->setExtent(bounds[0], bounds[3],
                       bounds[1], bounds[4],
                       bounds[2], bounds[5]);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::offsetPreset(double offsetValue)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->volumeRenderingWidget)
    {
    return;
    }

  qMRMLVolumePropertyNodeWidget* volumePropertyNodeWidget =
    d->volumeRenderingWidget->findChild<qMRMLVolumePropertyNodeWidget*>
      (QString("VolumePropertyNodeWidget"));
  if (!volumePropertyNodeWidget)
    {
    return;
    }

  ctkVTKVolumePropertyWidget* volumePropertyWidget =
     volumePropertyNodeWidget->findChild<ctkVTKVolumePropertyWidget*>
       (QString("VolumePropertyWidget"));
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

  astroVolumeLogic->CalculateDisplayThresholdInROI(roiNode, d->astroVolumeNode);
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
  double DisplayThreshold = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DisplayThreshold"));
  double nBins = d->BinSliderWidget->value();
  double binFlux = (DATAMAX - DATAMIN) / (nBins - 1);

  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());

  if (!astroVolumeLogic)
    {
    return;
    }

  vtkNew<vtkIntArray> histoArray;
  astroVolumeLogic->CalculateHistogram(d->astroVolumeNode, histoArray, binFlux, nBins);

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
  tableNode->SetColumnUnitLabel("Intensity", d->astroVolumeNode->GetAttribute("SlicerAstro.BUNIT"));
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

  table->SetNumberOfRows(nBins);
  if (!histoArray)
    {
    qCritical() <<"qSlicerAstroVolumeModuleWidget::onCreateHistogram : "
                  "Unable to find the Histogram Array.";
    return;
    }

  double histoMaxValue = 0;
  for (int ii = 0; ii < nBins; ii++)
     {
     table->SetValue(ii, 0, DATAMIN + ii * binFlux);
     double histoValue = 0;
     if (histoArray->GetValue(ii) >= 1)
       {
       histoValue = log10(histoArray->GetValue(ii));
       }

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
    double color[4] = {0.086, 0.365, 0.655, 1.};
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
    std::string xunit = "Intensity (";
    xunit += d->astroVolumeNode->GetAttribute("SlicerAstro.BUNIT");
    xunit += ")";
    d->plotChartNodeHistogram->SetXAxisTitle(xunit.c_str());
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

  vtkMRMLPlotViewNode* plotViewNode = plotView->mrmlPlotViewNode();
  if (plotViewNode)
    {
    plotViewNode->SetInteractionMode(vtkMRMLPlotViewNode::InteractionModeMovePoints);
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
void qSlicerAstroVolumeModuleWidget::onMRMLPlotChartNodeHistogramModified()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->plotChartNodeHistogram)
    {
    return;
    }

  vtkMRMLPlotSeriesNode* plotSeriesNode = d->plotChartNodeHistogram->GetPlotSeriesNode();
  bool HistoActive = false;
  if (plotSeriesNode)
    {
    HistoActive = true;
    }

  d->HistoPushButtonPreset1->setEnabled(HistoActive);
  d->HistoPushButtonPreset2->setEnabled(HistoActive);
  d->HistoPushButtonPreset3->setEnabled(HistoActive);
  d->HistoPushButtonPreset4->setEnabled(HistoActive);
  d->HistoPushButtonPreset5->setEnabled(HistoActive);
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

  d->VisibilityCheckBox->setChecked(displayNode->GetVisibility());

  d->ROICropCheckBox->setChecked(displayNode->GetCroppingEnabled());

  d->QualityControlComboBox->setCurrentIndex(displayNode->GetPerformanceControl());

  QString currentVolumeMapper = QString(displayNode->GetClassName());
  d->RenderingMethodComboBox->setCurrentIndex(d->RenderingMethodComboBox->findData(currentVolumeMapper));
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onOpacityValueChanged(double Opacity)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  d->OpacityScaling = Opacity;
  this->applyPreset(d->astroVolumeNode->GetPresetNode());
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

  if(!d->astroVolumeNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onCurrentQualityControlChanged : "
                   "inputVolume not found!";
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
        d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode();
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

  if(!d->astroVolumeNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setDisplayROIEnabled : "
                   "inputVolume not found!";
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
        d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode();
  if (!displayNode)
    {
    return;
    }

  vtkMRMLAnnotationROINode* ROINode = displayNode->GetROINode();
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

  if(!d->astroVolumeNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onCropToggled : "
                   "inputVolume not found!";
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
        d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode();
  if (!displayNode)
    {
    return;
    }

  displayNode->SetCroppingEnabled(crop);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onCurrentRenderingMethodChanged(int index)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!d->astroVolumeNode)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onCurrentRenderingMethodChanged : "
                   "inputVolume not found!";
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
        d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode();
  if (!displayNode || !d->volumeRenderingLogic)
    {
    return;
    }

  QString renderingClassName = d->RenderingMethodComboBox->itemData(index).toString();
  // Display node is already the right type, don't change anything
  if ( !displayNode || renderingClassName.isEmpty()
    || renderingClassName == displayNode->GetClassName())
    {
    return;
    }

  d->volumeRenderingLogic->ChangeVolumeRenderingMethod(renderingClassName.toLatin1());
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
void qSlicerAstroVolumeModuleWidget::setDisplayConnection()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode)
    {
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode *displayNode =
    vtkMRMLVolumeRenderingDisplayNode::SafeDownCast
      (d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode());
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
    if (!displayNodeIter)
      {
      continue;
      }
    // is this the display node?
    if (displayNode->GetID() && displayNodeIter->GetID() && strcmp(displayNode->GetID(), displayNodeIter->GetID()) == 0)
      {
      continue;
      }

    this->qvtkDisconnect(displayNodeIter, vtkCommand::ModifiedEvent,
                         this, SLOT(onMRMLVolumeRenderingDisplayNodeModified(vtkObject*)));
    this->qvtkDisconnect(displayNodeIter->GetROINode(), vtkMRMLDisplayableNode::DisplayModifiedEvent,
                         this, SLOT(onMRMLDisplayROINodeModified(vtkObject*)));

    if (d->AutoPropagateCheckBox->isChecked())
      {
      displayNodeIter->SetVisibility(false);
      }
    }
  col->RemoveAllItems();
  col->Delete();

  this->qvtkConnect(displayNode, vtkCommand::ModifiedEvent,
                    this, SLOT(onMRMLVolumeRenderingDisplayNodeModified(vtkObject*)));
  this->qvtkConnect(displayNode->GetROINode(), vtkMRMLDisplayableNode::DisplayModifiedEvent,
                    this, SLOT(onMRMLDisplayROINodeModified(vtkObject*)));

  displayNode->SetExpectedFPS(100);
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

  vtkMRMLAstroVolumeNode *activeVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(activeVolumeNodeID));
  vtkMRMLAstroLabelMapVolumeNode *activeLabelMapVolumeNode = vtkMRMLAstroLabelMapVolumeNode::SafeDownCast
    (this->mrmlScene()->GetNodeByID(activeLabelMapVolumeNodeID));

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
  else
    {
    this->setMRMLVolumeNode(activeVolumeNode);
    }

  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());

  if (!astroVolumeLogic)
    {
    return;
    }

  astroVolumeLogic->updateIntensityUnitsNode(activeVolumeNode);
  this->updateWidgetsFromIntensityNode();
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

  double DisplayThreshold = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DisplayThreshold"));

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

  d->astroVolumeNode->SetDisplayThreshold(DisplayThreshold);
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

  std::string MIN = d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMIN");
  std::string MAX = d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMAX");

  double DataMin = StringToDouble(MIN.c_str());
  double DataMax = StringToDouble(MAX.c_str());
  std::string DataMinString = DoubleToString(DataMin);
  DataMinString += "  ";
  std::string DataMaxString = DoubleToString(DataMax);
  DataMaxString += "  ";
  if (strcmp(d->astroVolumeNode->GetAttribute("SlicerAstro.BUNIT"), "UNDEFINED"))
    {
    DataMinString += d->astroVolumeNode->GetAttribute("SlicerAstro.BUNIT");
    DataMaxString += d->astroVolumeNode->GetAttribute("SlicerAstro.BUNIT");
    }
  d->DataMinDisplay->setText(DataMinString.c_str());
  d->DataMaxDisplay->setText(DataMaxString.c_str());
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

  double DisplayThreshold = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DisplayThreshold"));
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
  this->resetOpacities();

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
  vtkMRMLAstroVolumeNode* astroVolumeNode =
    vtkMRMLAstroVolumeNode::SafeDownCast(node);

  vtkMRMLAstroLabelMapVolumeNode* labelMapVolumeNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(node);

  if (astroVolumeNode)
    {
    this->setMRMLVolumeNode(astroVolumeNode);
    }
  else if (labelMapVolumeNode)
    {
    this->setMRMLVolumeNode(labelMapVolumeNode);
    }
  else
    {
    this->setMRMLVolumeNode(astroVolumeNode);
    }
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setMRMLVolumeNode(vtkMRMLAstroVolumeNode* volumeNode)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    return;
    }

  if (d->AutoPropagateCheckBox->isChecked())
    {
    appLogic->PropagateBackgroundVolumeSelection(1);
    }

  if (!volumeNode)
    {
    this->onVisibilityChanged(false);
    d->ActiveVolumeNodeSelector->setCurrentNode(volumeNode);
    d->volumeRenderingWidget->setMRMLVolumeNode(volumeNode);
    d->astroVolumeNode = NULL;
    return;
    }

  if (d->astroVolumeNode == volumeNode)
    {
    return;
    } 

  if (!d->astroVolumeNode)
    {
    this->initializeColorNodes();
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
    this->qvtkReconnect(d->astroVolumeNode, vtkMRMLAstroVolumeNode::ReferenceAddedEvent,
                        this, SLOT(setDisplayConnection()));
    this->setDisplayConnection();

    if (d->AutoPropagateCheckBox->isChecked())
      {
      this->onVisibilityChanged(true);
      }
    }
  this->qvtkReconnect(d->astroVolumeNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLVolumeNodeModified()));
  this->onMRMLVolumeNodeModified();
  this->qvtkReconnect(d->astroVolumeNode->GetAstroVolumeDisplayNode(), vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLVolumeDisplayNodeModified()));
  this->onMRMLVolumeDisplayNodeModified();

  d->ActiveVolumeNodeSelector->setCurrentNodeID(volumeNode->GetID());

  d->DisplayCollapsibleButton->setCollapsed(false);
  d->CollapsibleButton_CopyMoveSegment->setCollapsed(true);

  this->setEnabled(volumeNode != 0);
}

//--------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setMRMLVolumeNode(vtkMRMLAstroLabelMapVolumeNode* volumeNode)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    return;
    }

  if (d->AutoPropagateCheckBox->isChecked())
    {
    appLogic->PropagateLabelVolumeSelection(1);
    }

  if (!volumeNode)
    {
    d->ActiveVolumeNodeSelector->setCurrentNode(volumeNode);
    d->astroLabelVolumeNode = NULL;
    return;
    }

  if (d->astroLabelVolumeNode == volumeNode)
    {
    return;
    }

  d->astroLabelVolumeNode = vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(volumeNode);
  this->qvtkReconnect(d->astroLabelVolumeNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLLabelVolumeNodeModified()));
  this->onMRMLLabelVolumeNodeModified();

  d->ActiveVolumeNodeSelector->setCurrentNodeID(volumeNode->GetID());

  d->DisplayCollapsibleButton->setCollapsed(true);
  d->CollapsibleButton_CopyMoveSegment->setCollapsed(false);

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
