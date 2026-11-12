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
#include <QApplication>
#include <QEvent>
#include <QThread>
#include <QProgressDialog>
#include <QEventLoop>
#include <QPointer>
#include <QAbstractItemView>
#include <QtDebug>
#include <QMessageBox>
#include <QMainWindow>
#include <QStatusBar>
#include <QDir>
#include <QFileDialog>
#include <QTimer>

// MRMLWidgets includes
#include <qMRMLSortFilterProxyModel.h>
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
#include <vtkDataArray.h>
#include <vtkPointData.h>
#include <vtkSegmentation.h>

// Segmentations includes
#include <qMRMLSegmentEditorWidget.h>
#include <vtkWeakPointer.h>
#include <vtkRenderer.h>
#include <vtkTable.h>
#include <vtkTransform.h>
#include <vtkVariant.h>
#include <vtkVolumeProperty.h>

// qMRMLWidgets include
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
#include <qSlicerAbstractModule.h>
#include <qSlicerAbstractModuleRepresentation.h>
#include <qSlicerModuleManager.h>
#include <qSlicerPresetComboBox.h>
#include <qSlicerPresetComboBox_p.h>
#include <qSlicerUtils.h>
#include <qSlicerAbstractModuleWidget.h>
#include <qSlicerVolumeRenderingPresetComboBox.h>
#include <ui_qSlicerAstroVolumeModuleWidget.h>

// MRML includes
#include <vtkMRMLMarkupsROINode.h>
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLStorageNode.h>
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
/// \ingroup SlicerAstro_QtModules_AstroVolume
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

  QPointer<qSlicerAbstractModuleWidget> volumeRenderingWidget;
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
  /// Mask of the Masks section (observed to enable Save as FITS mask)
  vtkWeakPointer<vtkMRMLSegmentationNode> MaskNode;
  QTimer* SaveFITSMaskButtonTimer{ nullptr };
  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> astroLabelVolumeNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;

  bool Lock;
};

//-----------------------------------------------------------------------------
// qSlicerVolumeRenderingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidgetPrivate::qSlicerAstroVolumeModuleWidgetPrivate(
  qSlicerAstroVolumeModuleWidget& object)
  : q_ptr(&object)
{
  this->MRMLAstroVolumeInfoWidget = nullptr;
  this->segmentationsLogic = nullptr;
  this->volumeRenderingWidget = nullptr;
  this->plotChartNodeHistogram = nullptr;
  this->astroVolumeNode = nullptr;
  this->astroLabelVolumeNode = nullptr;
  this->selectionNode = nullptr;
  this->segmentEditorNode = nullptr;
  this->PlotSeriesNodeMinLine = nullptr;
  this->TableMinNode = nullptr;
  this->PlotSeriesNodeMaxLine = nullptr;
  this->TableMaxNode = nullptr;
  this->PlotSeriesNodeThresholdLine = nullptr;
  this->TableThresholdNode = nullptr;
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

  // qSlicerVolumeRenderingModuleWidget is internal to the VolumeRendering
  // module: access it through the generic module widget interface.
  this->volumeRenderingWidget = dynamic_cast<qSlicerAbstractModuleWidget*>
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

  QObject::connect(this->QualityControlComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onCurrentQualityControlChanged(int)));

  QObject::connect(this->PresetsNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(resetAndApplyPreset(vtkMRMLNode*)));

  QObject::connect(this->PresetsNodeComboBox, SIGNAL(nodeActivated(vtkMRMLNode*)),
                   q, SLOT(resetAndApplyPreset(vtkMRMLNode*)));

  QObject::connect(this->PresetOffsetSlider, SIGNAL(valueChanged(double)),
                   q, SLOT(onOffsetValueChanged(double)));

  QObject::connect(this->PresetStretchSlider, SIGNAL(valueChanged(double)),
                   q, SLOT(onStretchValueChanged(double)));

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

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   this->SegmentationNodeComboBox, SLOT(setMRMLScene(vtkMRMLScene*)));
  QObject::connect(this->SegmentationNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onSegmentationNodeChanged(vtkMRMLNode*)));
  // only masks (binary labelmaps) in the selector: not the contours
  QObject::connect(this->SegmentationNodeComboBox, SIGNAL(nodeAdded(vtkMRMLNode*)),
                   q, SLOT(updateMaskSelector()));
  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   q, SLOT(updateMaskSelector()));
  QObject::connect(this->SaveFITSMaskPushButton, SIGNAL(clicked()),
                   q, SLOT(onSaveFITSMask()));
  // enabled for masks with voxels (checked after the edits, not at each stroke)
  this->SaveFITSMaskButtonTimer = new QTimer(q);
  this->SaveFITSMaskButtonTimer->setSingleShot(true);
  this->SaveFITSMaskButtonTimer->setInterval(200);
  QObject::connect(this->SaveFITSMaskButtonTimer, SIGNAL(timeout()),
                   q, SLOT(updateSaveFITSMaskButton()));

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

  // The segmentation (e.g. a FITS mask of the user) and the Segment Editor
  // node stay in the scene: only the references of the widget are released.
  this->segmentEditorNode = nullptr;

  if (this->SegmentsTableView)
    {
    this->SegmentsTableView->setSegmentationNode(nullptr);
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
  this->contoursSegNodeSmoothing = nullptr;
  this->contoursSegNodeModeling = nullptr;
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidget::~qSlicerAstroVolumeModuleWidget()
{
  if (this->contoursSegNodeSmoothing)
    {
    this->mrmlScene()->RemoveNode(this->contoursSegNodeSmoothing);
    this->contoursSegNodeSmoothing->Delete();
    }
  this->contoursSegNodeSmoothing = nullptr;

  if (this->contoursSegNodeModeling)
    {
    this->mrmlScene()->RemoveNode(this->contoursSegNodeModeling);
    this->contoursSegNodeModeling->Delete();
    }
  this->contoursSegNodeModeling = nullptr;
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
                   " PresetsNodeComboBox not found!";
    return;
    }

  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  if (!astroVolumeLogic)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setMRMLScene error :"
                   " astroVolumeLogic not found!";
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
    // the mask of the active cube (no segmentation without data)
    vtkSlicerAstroVolumeLogic* astroVolumeLogic = vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
    vtkMRMLSelectionNode* selectionNode = (this->module() && this->module()->appLogic()) ?
      this->module()->appLogic()->GetSelectionNode() : nullptr;
    vtkMRMLAstroVolumeNode* activeVolume = (selectionNode && selectionNode->GetActiveVolumeID()) ?
      vtkMRMLAstroVolumeNode::SafeDownCast(this->mrmlScene()->GetNodeByID(selectionNode->GetActiveVolumeID())) : nullptr;
    vtkMRMLSegmentationNode* sceneSegmentationNode = (astroVolumeLogic && activeVolume) ?
      astroVolumeLogic->GetMaskSegmentationForVolume(activeVolume, true) : nullptr;
    if (sceneSegmentationNode)
      {
      d->segmentEditorNode->SetAndObserveSegmentationNode(sceneSegmentationNode);
      }
    }

  if (d->segmentEditorNode->GetSegmentationNode())
    {
    d->segmentEditorNode->GetSegmentationNode()->CreateDefaultDisplayNodes();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::initializeColorNodes()
{
  if (!this->mrmlScene())
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::initializeColorNodes error :"
                     " scene not found!";
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
    this->mrmlScene()->AddNode(RainbowTableNode.GetPointer());
    }
  else
    {
    vtkMRMLColorTableNode* RainbowTableNode = vtkMRMLColorTableNode::SafeDownCast(colorNodes->GetItemAsObject(0));
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
        !strcmp(tempColorTableNode->GetName(), "VelocityField") ||
        !strcmp(tempColorTableNode->GetName(), "Rainbow"))
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

  // GenericColors, DarkBrightChartColors and Random (color tables of Slicer,
  // e.g. for segments and labels) are kept unmodified: copies of them were
  // saved with the scene and could not be read again (Random)

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

  // Select nullptr Chart
  if (d->selectionNode)
    {
    d->selectionNode->SetActivePlotChartID(nullptr);
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
      d->TableMinNode->SetUseColumnTitleAsColumnHeader(true);
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
      d->TableMaxNode->SetUseColumnTitleAsColumnHeader(true);
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
      d->TableThresholdNode->SetUseColumnTitleAsColumnHeader(true);
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
  std::string newSourceRepresentation(representation.toLatin1().constData());

  // Set master representation to the added one if segmentation is empty or master representation is undefined
  if (segmentation->GetNumberOfSegments() == 0)
    {
    segmentation->SetSourceRepresentationName(newSourceRepresentation);
    return true;
    }

  // No action is necessary if segmentation is non-empty and the master representation matches the contained one in segment
  if (segmentation->GetSourceRepresentationName() == newSourceRepresentation)
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
    .arg(segmentationNode->GetName()).arg(newSourceRepresentation.c_str())
    .arg(segmentation->GetSourceRepresentationName().c_str());
  QMessageBox::StandardButton answer =
    QMessageBox::question(nullptr, tr("Master representation is needed to be changed to add segment"), message,
    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (answer == QMessageBox::No)
    {
    return false;
    }

  // Make sure the new master representation exists before setting it
  if (!segmentation->CreateRepresentation(newSourceRepresentation.c_str()))
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
    segmentation->SetSourceRepresentationName(newSourceRepresentation.c_str());
    return false;
    }

  // Set master representation to the added one if user agreed
  segmentation->SetSourceRepresentationName(newSourceRepresentation.c_str());
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

    d->HistoCollapsibleButton->setEnabled(true);

    astroVolumeNode->Modified();
    d->selectionNode->SetActiveVolumeID(astroVolumeNode->GetID());
    d->selectionNode->SetActiveVolumeID(astroVolumeNode->GetID());
    }
  else if (labelMapVolumeNode)
    {
    d->RenderingFrame->setEnabled(false);
    d->RenderingFrame->setCollapsed(true);
    d->HistoCollapsibleButton->setEnabled(false);
    d->HistoCollapsibleButton->setCollapsed(true);
    labelMapVolumeNode->Modified();
    d->selectionNode->SetActiveLabelVolumeID(labelMapVolumeNode->GetID());
    d->selectionNode->SetActiveLabelVolumeID(labelMapVolumeNode->GetID());
    }
  else
    {
    d->selectionNode->SetActiveVolumeID(nullptr);
    d->selectionNode->SetActiveVolumeID(nullptr);
    d->selectionNode->SetActiveLabelVolumeID(nullptr);
    d->selectionNode->SetActiveLabelVolumeID(nullptr);
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

  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (!volume)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* volumeDisplay =
    volume->GetAstroVolumeDisplayNode();
  if (!volumeDisplay)
    {
    return;
    }

  int wasModifying = volumeDisplay->StartModify();
  volumeDisplay->SetPresetOffset(0.);
  volumeDisplay->SetOldPresetOffset(0.);
  volumeDisplay->EndModify(wasModifying);

  double width = StringToDouble(volume->GetAttribute("SlicerAstro.DATAMAX")) -
                 StringToDouble(volume->GetAttribute("SlicerAstro.DATAMIN"));
  bool wasBlocking = d->PresetOffsetSlider->blockSignals(true);
  d->PresetOffsetSlider->setSingleStep(
    width ? ctk::closestPowerOfTen(width) / 100. : 0.1);
  d->PresetOffsetSlider->setPageStep(d->PresetOffsetSlider->singleStep());
  d->PresetOffsetSlider->setRange(-width, width);
  d->PresetOffsetSlider->blockSignals(wasBlocking);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::resetOffsetStep(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (!volume)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* volumeDisplay =
    volume->GetAstroVolumeDisplayNode();
  if (!volumeDisplay)
    {
    return;
    }

  double width = StringToDouble(volume->GetAttribute("SlicerAstro.DATAMAX")) -
                 StringToDouble(volume->GetAttribute("SlicerAstro.DATAMIN"));
  d->PresetOffsetSlider->setSingleStep(
    width ? ctk::closestPowerOfTen(width) / 100. : 0.1);
  d->PresetOffsetSlider->setPageStep(d->PresetOffsetSlider->singleStep());
  d->PresetOffsetSlider->setRange(-width, width);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::resetOpacityShift(vtkMRMLNode *node)
{
  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (!volume)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* volumeDisplay =
    volume->GetAstroVolumeDisplayNode();
  if (!volumeDisplay)
    {
    return;
    }

  int wasModifying = volumeDisplay->StartModify();
  volumeDisplay->SetOpacityShift(0.);
  volumeDisplay->SetOldOpacityShift(0.);
  volumeDisplay->EndModify(wasModifying);
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

  if (inputVolume->GetPresetNode() != nullptr)
    {
    d->PresetsNodeComboBox->setCurrentNodeIndex(inputVolume->GetPresetIndex());
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
void qSlicerAstroVolumeModuleWidget::ensureVolumeRenderingTransferFunctionGaps()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode || !d->astroVolumeNode->GetImageData())
    {
    return;
    }
  vtkMRMLVolumeRenderingDisplayNode* displayNode =
    d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode();
  if (!displayNode || !displayNode->GetVolumePropertyNode())
    {
    return;
    }
  // The default transfer functions created by the Volume Rendering module
  // are rendered until the astronomical presets are applied: their points
  // must not be too close for the lookup table textures (see
  // vtkSlicerAstroVolumeLogic::EnsureMinimumTransferFunctionGap).
  double range[2] = { 0., 0. };
  d->astroVolumeNode->GetImageData()->GetPointData()->GetScalars()->GetFiniteRange(range);
  vtkSlicerAstroVolumeLogic::EnsureMinimumTransferFunctionGap(
    displayNode->GetVolumePropertyNode()->GetVolumeProperty(), range);
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
void qSlicerAstroVolumeModuleWidget::onStretchValueChanged(double stretchValue)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* volumeDisplay =
    d->astroVolumeNode->GetAstroVolumeDisplayNode();
  if (!volumeDisplay)
    {
    return;
    }

  int wasModifying = volumeDisplay->StartModify();
  volumeDisplay->SetOldPresetStretch(volumeDisplay->GetPresetStretch());
  volumeDisplay->SetPresetStretch(stretchValue);
  if (d->Lock)
    {
    volumeDisplay->SetOldPresetOffset(volumeDisplay->GetPresetOffset());
    volumeDisplay->SetPresetOffset(stretchValue);
    }
  volumeDisplay->EndModify(wasModifying);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::clearPresets()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  d->PresetsNodeComboBox->setCurrentNodeIndex(-1);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onCreateSurfaceButtonToggled(vtkMRMLSegmentationNode* segmentationNode)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->segmentEditorNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment editor parameter set node";
    return;
    }

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

  // Contours: closed surfaces in 2D, binary labelmaps in 3D (not a mask)
  vtkSlicerAstroVolumeLogic::SetContoursDisplay(segmentationNode);

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

  // The Masks section follows the Segment Editor for masks only (the table
  // shows the mask of the selector: onSegmentationNodeChanged)
  if (d->SegmentationNodeComboBox->currentNode() != segmentationNode &&
      !vtkSlicerAstroVolumeLogic::IsContoursSegmentation(segmentationNode))
    {
    d->SegmentationNodeComboBox->setCurrentNode(segmentationNode);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::resetStretch(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (!volume)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* volumeDisplay =
    volume->GetAstroVolumeDisplayNode();
  if (!volumeDisplay)
    {
    return;
    }

  int wasModifying = volumeDisplay->StartModify();
  volumeDisplay->SetPresetStretch(0.);
  volumeDisplay->SetOldPresetStretch(0.);
  volumeDisplay->EndModify(wasModifying);

  double width = StringToDouble(volume->GetAttribute("SlicerAstro.DATAMAX")) -
                 StringToDouble(volume->GetAttribute("SlicerAstro.DATAMIN"));
  bool wasBlocking = d->PresetStretchSlider->blockSignals(true);
  d->PresetStretchSlider->setSingleStep(
    width ? ctk::closestPowerOfTen(width) / 100. : 0.1);
  d->PresetStretchSlider->setPageStep(d->PresetStretchSlider->singleStep());
  d->PresetStretchSlider->setRange(-width, width);
  d->PresetStretchSlider->blockSignals(wasBlocking);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::resetStretchStep(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLAstroVolumeNode* volume = vtkMRMLAstroVolumeNode::SafeDownCast(node);
  if (!volume)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* volumeDisplay =
    volume->GetAstroVolumeDisplayNode();
  if (!volumeDisplay)
    {
    return;
    }

  double width = StringToDouble(volume->GetAttribute("SlicerAstro.DATAMAX")) -
                 StringToDouble(volume->GetAttribute("SlicerAstro.DATAMIN"));
  d->PresetStretchSlider->setSingleStep(
    width ? ctk::closestPowerOfTen(width) / 100. : 0.1);
  d->PresetStretchSlider->setPageStep(d->PresetStretchSlider->singleStep());
  d->PresetStretchSlider->setRange(-width, width);
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
    QMessageBox::warning(nullptr, tr("Failed to import from labelmap volume"), message);
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
      QMessageBox::warning(nullptr, tr("Failed to import from labelmap volume"), message);
      return;
      }
    }

  labelMapNode->UpdateRangeAttributes();

  currentSegmentationNode->CreateDefaultDisplayNodes();

  // a mask: binary labelmap displayed in 2D and 3D
  vtkSlicerAstroVolumeLogic::SetBinaryLabelmapDisplay(currentSegmentationNode);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    return;
    }

  if (!d->selectionNode)
    {
    return;
    }

  d->selectionNode->SetActiveLabelVolumeID("");
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
    QMessageBox::warning(nullptr, tr("Failed to export segment"), message);
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
    QMessageBox::warning(nullptr, tr("Failed to export segment"), message);
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
    reply = QMessageBox::question(nullptr, tr("Failed to select a segment"), message);
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
    QMessageBox::warning(nullptr, tr("Failed to export segment"), message);
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

  if (fabs(d->astroVolumeNode->GetDisplayThreshold() - DisplayThreshold) > 1.E-9)
    {
    d->astroVolumeNode->SetDisplayThreshold(DisplayThreshold);
    }
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
    // looking along the velocity axis (A in RAS), north up, centered on the cube
    const double directionOfProjection[3] = {0., -1., 0.};
    const double viewUp[3] = {0., 0., 1.};
    vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(cameraNode, d->astroVolumeNode,
                                                    directionOfProjection, viewUp);
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
      continue;
      }

    qMRMLThreeDView* ThreeDView = ThreeDWidget->threeDView();
    if(!ThreeDView || !ThreeDView->renderer())
      {
      qCritical() << "qSlicerAstroVolumeModuleWidget::onVisibilityChanged : "
                     "ThreeDView not found.";
      continue;
      }

    ThreeDView->renderer()->ResetCameraClippingRange();
    //ThreeDView->renderer()->Render();

    qMRMLThreeDViewControllerWidget* ThreeDController = ThreeDWidget->threeDController();
    if(!ThreeDController)
      {
      qCritical() << "qSlicerAstroVolumeModuleWidget::onVisibilityChanged : "
                     "ThreeDController not found.";
      continue;
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

      if (cameraNode->GetLayoutName() && View1->GetLayoutName() &&
          !strcmp(cameraNode->GetLayoutName(), View1->GetLayoutName()))
        {
        // looking along the velocity axis (A in RAS), north up, centered on the cube
        const double directionOfProjection[3] = {0., -1., 0.};
        const double viewUp[3] = {0., 0., 1.};
        vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(cameraNode, volumeOne,
                                                        directionOfProjection, viewUp);
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

      if (cameraNode->GetLayoutName() && View2->GetLayoutName() &&
          !strcmp(cameraNode->GetLayoutName(), View2->GetLayoutName()))
        {
        // looking along the velocity axis (A in RAS), north up, centered on the cube
        const double directionOfProjection[3] = {0., -1., 0.};
        const double viewUp[3] = {0., 0., 1.};
        vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(cameraNode, volumeTwo,
                                                        directionOfProjection, viewUp);
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

  // Smoothing makes a mask only (no contours): the contours segmentation of
  // the Smoothing module of previous runs or scenes is removed
  vtkSmartPointer<vtkCollection> segNodes = vtkSmartPointer<vtkCollection>::Take
    (this->mrmlScene()->GetNodesByClassByName("vtkMRMLSegmentationNode", "AstroSmoothingContoursSeg"));
  for (int ii = 0; ii < segNodes->GetNumberOfItems(); ii++)
    {
    this->mrmlScene()->RemoveNode(vtkMRMLNode::SafeDownCast(segNodes->GetItemAsObject(ii)));
    }
  this->contoursSegNodeSmoothing = nullptr;

  // Mask of the smoothed data at 3 RMS: a mask (binary labelmap in 2D and 3D,
  // FITS mask) on the grid of the input data, e.g. for the other modules
  vtkSmartPointer<vtkMRMLSegmentationNode> maskNode = vtkSmartPointer<vtkMRMLSegmentationNode>::Take(
    vtkMRMLSegmentationNode::SafeDownCast(this->mrmlScene()->CreateNodeByClass("vtkMRMLSegmentationNode")));
  maskNode->SetName(this->mrmlScene()->GetUniqueNameByString((std::string(volumeTwo->GetName()) + "_SEG").c_str()));
  this->mrmlScene()->AddNode(maskNode);
  maskNode->CreateDefaultDisplayNodes();
  vtkSlicerAstroVolumeLogic::SetMaskReferenceVolume(maskNode, volumeOne);

  const std::string SegmentTwoName = std::string(volumeTwo->GetName()) + "_3RMS";
  double maskColor[3] = {1., 0.9, 0.13};
  const std::string SegmentTwoID = maskNode->GetSegmentation()->AddEmptySegment(SegmentTwoName, SegmentTwoName, maskColor);
    {
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
    modifierLabelmap->ShallowCopy(imageThreshold->GetOutput());
    vtkNew<vtkMatrix4x4> IJKToRASMatrix;
    volumeTwo->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
    modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());
    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
        modifierLabelmap.GetPointer(), maskNode, SegmentTwoID, vtkSlicerSegmentationsModuleLogic::MODE_REPLACE))
      {
      qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
      }
    }
  vtkSlicerAstroVolumeLogic::SetBinaryLabelmapDisplay(maskNode);
  vtkMRMLSegmentationDisplayNode* maskDisplayNode =
    vtkMRMLSegmentationDisplayNode::SafeDownCast(maskNode->GetDisplayNode());
  if (maskDisplayNode)
    {
    // shown with the input data (first 3D view, semi-transparent) and in the
    // slice views; not with the smoothed data (second 3D view)
    maskDisplayNode->SetSegmentOpacity3D(SegmentTwoID, 0.3);
    maskDisplayNode->RemoveAllViewNodeIDs();
    vtkMRMLViewNode* inputDataView = col->GetNumberOfItems() > 0 ?
      vtkMRMLViewNode::SafeDownCast(col->GetItemAsObject(0)) : nullptr;
    if (inputDataView)
      {
      maskDisplayNode->AddViewNodeID(inputDataView->GetID());
      }
    std::vector<vtkMRMLNode*> sliceNodes;
    this->mrmlScene()->GetNodesByClass("vtkMRMLSliceNode", sliceNodes);
    for (vtkMRMLNode* sliceNode : sliceNodes)
      {
      maskDisplayNode->AddViewNodeID(sliceNode->GetID());
      }
    }
  // the mask is the segmentation of the Segment Editor (and of the mask
  // selectors of the modules)
  d->segmentEditorNode->SetAndObserveSegmentationNode(maskNode);

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

      if (cameraNode->GetLayoutName() && View1->GetLayoutName() &&
          !strcmp(cameraNode->GetLayoutName(), View1->GetLayoutName()))
        {
        // looking along the velocity axis (A in RAS), north up, centered on the cube
        const double directionOfProjection[3] = {0., -1., 0.};
        const double viewUp[3] = {0., 0., 1.};
        vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(cameraNode, volumeOne,
                                                        directionOfProjection, viewUp);
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
bool qSlicerAstroVolumeModuleWidget::askUser(QWidget* parent, const QString& title, const QString& question)
{
  qInfo().noquote() << title << ":" << question;
  qSlicerApplication* application = qSlicerApplication::application();
  if (!application || application->testAttribute(qSlicerCoreApplication::AA_EnableTesting))
    {
    return true;
    }
  return QMessageBox::question(parent, title, question, QMessageBox::Yes | QMessageBox::No,
                               QMessageBox::Yes) == QMessageBox::Yes;
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::informUser(QWidget* parent, QMessageBox::Icon icon,
                                                const QString& title, const QString& message)
{
  if (icon == QMessageBox::Critical)
    {
    qCritical().noquote() << title << ":" << message;
    }
  else if (icon == QMessageBox::Warning)
    {
    qWarning().noquote() << title << ":" << message;
    }
  else
    {
    qInfo().noquote() << title << ":" << message;
    }
  qSlicerApplication* application = qSlicerApplication::application();
  if (!application || application->testAttribute(qSlicerCoreApplication::AA_EnableTesting))
    {
    return;
    }
  QMessageBox messageBox(icon, title, message, QMessageBox::Ok, parent);
  messageBox.exec();
}

//---------------------------------------------------------------------------
namespace
{
// While a background computation runs: user input (mouse, keyboard) goes only
// to the progress dialog (no other operation can start, e.g. before the
// dialog is shown)
class BackgroundInputBlocker : public QObject
{
public:
  explicit BackgroundInputBlocker(QWidget* allowed)
    : Allowed(allowed)
    {
    }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override
    {
    switch (event->type())
      {
      case QEvent::MouseButtonPress:
      case QEvent::MouseButtonRelease:
      case QEvent::MouseButtonDblClick:
      case QEvent::KeyPress:
      case QEvent::KeyRelease:
      case QEvent::Wheel:
      case QEvent::Shortcut:
      case QEvent::ShortcutOverride:
      case QEvent::ContextMenu:
        {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        return !(widget && this->Allowed && (widget == this->Allowed || this->Allowed->isAncestorOf(widget)));
        }
      default:
        return false;
      }
    }

private:
  QWidget* Allowed;
};
} // end of anonymous namespace

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::runInBackground(QWidget* parent, const QString& label,
                                                     const std::function<void()>& work,
                                                     const std::function<int()>& progress,
                                                     const std::function<void()>& cancel)
{
  QThread* thread = QThread::create(work);
  QProgressDialog dialog(label, tr("Cancel"), 0, 100, parent);
  dialog.setWindowTitle(tr("SlicerAstro"));
  dialog.setWindowModality(Qt::WindowModal);
  dialog.setMinimumDuration(500);
  dialog.setAutoClose(false);
  dialog.setAutoReset(false);
  dialog.setValue(0);
  QObject::connect(&dialog, &QProgressDialog::canceled, [&dialog, cancel]()
    {
    dialog.setLabelText(tr("Cancelling..."));
    cancel();
    });
  QEventLoop loop;
  QTimer timer;
  timer.setInterval(100);
  // the end of the work is polled (no lost signal before the loop starts)
  QObject::connect(&timer, &QTimer::timeout, [&dialog, &loop, thread, progress]()
    {
    const int value = progress();
    if (value >= 0 && value <= 100 && !dialog.wasCanceled())
      {
      dialog.setValue(value);
      }
    if (thread->isFinished())
      {
      loop.quit();
      }
    });
  BackgroundInputBlocker inputBlocker(&dialog);
  qApp->installEventFilter(&inputBlocker);
  thread->start();
  timer.start();
  loop.exec();
  qApp->removeEventFilter(&inputBlocker);
  timer.stop();
  thread->wait();
  delete thread;
  // closing a QProgressDialog emits canceled(): not a cancel request
  QObject::disconnect(&dialog, nullptr, nullptr, nullptr);
  dialog.close();
}

//---------------------------------------------------------------------------
const char* qSlicerAstroVolumeModuleWidget::modelingDataContoursSegmentID()
{
  return "ModelingDataContours";
}

//---------------------------------------------------------------------------
const char* qSlicerAstroVolumeModuleWidget::modelingModelContoursSegmentID()
{
  return "ModelingModelContours";
}

//---------------------------------------------------------------------------
const char* qSlicerAstroVolumeModuleWidget::modelingModelMaskSegmentID()
{
  return "ModelingModelMask";
}

//---------------------------------------------------------------------------
bool qSlicerAstroVolumeModuleWidget::updateModelingContourSegment(const char* segmentID,
                                                                  const std::string& name,
                                                                  const double color[3],
                                                                  vtkMRMLAstroVolumeNode* volume,
                                                                  double lower, double upper)
{
  if (!this->contoursSegNodeModeling || !this->contoursSegNodeModeling->GetSegmentation() ||
      !volume || !volume->GetImageData())
    {
    return false;
    }
  // one segment per kind of contours, recomputed in place (the labelmap of a
  // segment has the size of the cube)
  vtkSegmentation* segmentation = this->contoursSegNodeModeling->GetSegmentation();
  vtkSegment* segment = segmentation->GetSegment(segmentID);
  if (!segment)
    {
    double segmentColor[3] = {color[0], color[1], color[2]};
    segmentation->AddEmptySegment(segmentID, name, segmentColor);
    }
  else
    {
    segment->SetName(name.c_str());
    }

  vtkNew<vtkImageThreshold> imageThreshold;
  imageThreshold->SetInputData(volume->GetImageData());
  imageThreshold->ThresholdBetween(lower, upper);
  imageThreshold->SetInValue(1);
  imageThreshold->SetOutValue(0);
  imageThreshold->SetOutputScalarType(VTK_SHORT);
  imageThreshold->Update();
  vtkNew<vtkOrientedImageData> modifierLabelmap;
  modifierLabelmap->ShallowCopy(imageThreshold->GetOutput());
  vtkNew<vtkMatrix4x4> IJKToRASMatrix;
  volume->GetIJKToRASMatrix(IJKToRASMatrix.GetPointer());
  modifierLabelmap->SetGeometryFromImageToWorldMatrix(IJKToRASMatrix.GetPointer());

  vtkSlicerAstroVolumeLogic::PrepareContoursUpdate(this->contoursSegNodeModeling);
  if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
      modifierLabelmap.GetPointer(), this->contoursSegNodeModeling, segmentID,
      vtkSlicerSegmentationsModuleLogic::MODE_REPLACE))
    {
    qCritical() << Q_FUNC_INFO << ": failed to update the segment" << segmentID;
    return false;
    }
  return true;
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::removeObsoleteModelingContourSegments()
{
  if (!this->contoursSegNodeModeling || !this->contoursSegNodeModeling->GetSegmentation())
    {
    return;
    }
  // e.g. one segment per contour level or per model (scenes of SlicerAstro 1)
  vtkSegmentation* segmentation = this->contoursSegNodeModeling->GetSegmentation();
  std::vector<std::string> segmentIDs;
  segmentation->GetSegmentIDs(segmentIDs);
  for (const std::string& segmentID : segmentIDs)
    {
    if (segmentID != modelingDataContoursSegmentID() &&
        segmentID != modelingModelContoursSegmentID() &&
        segmentID != modelingModelMaskSegmentID())
      {
      segmentation->RemoveSegment(segmentID);
      }
    }
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

      if (cameraNode->GetLayoutName() && View1->GetLayoutName() &&
          !strcmp(cameraNode->GetLayoutName(), View1->GetLayoutName()))
        {
        // looking along the velocity axis (A in RAS), north up, centered on the cube
        const double directionOfProjection[3] = {0., -1., 0.};
        const double viewUp[3] = {0., 0., 1.};
        vtkSlicerAstroVolumeLogic::CenterCameraOnVolume(cameraNode, volumeOne,
                                                        directionOfProjection, viewUp);
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

  vtkSmartPointer<vtkCollection> segNodes = vtkSmartPointer<vtkCollection>::Take
    (this->mrmlScene()->GetNodesByClassByName("vtkMRMLSegmentationNode", "AstroModelingContoursSeg"));
  this->contoursSegNodeModeling = vtkMRMLSegmentationNode::SafeDownCast(segNodes->GetItemAsObject(0));
  if (!this->contoursSegNodeModeling)
    {
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLSegmentationNode");
    this->contoursSegNodeModeling = vtkMRMLSegmentationNode::SafeDownCast(foo);
    this->contoursSegNodeModeling->SetName("AstroModelingContoursSeg");
    this->contoursSegNodeModeling->SetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName(), "true");
    this->mrmlScene()->AddNode(this->contoursSegNodeModeling);
    }

  // the contours are not edited: the Segment Editor keeps its segmentation (mask)

  if (!this->contoursSegNodeModeling->GetDisplayNode())
    {
    this->contoursSegNodeModeling->CreateDefaultDisplayNodes();
    }

  // Create empty segment in current segmentation

  const double contourThreshold =
    StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold")) * ContourLevel;
  const std::string levelSuffix = "_" + DoubleToString(ContourLevel) + "RMS";
  const double dataColor[3] = {0.5, 0.68, 0.5};
  const double modelColor[3] = {1., 0.9, 0.13};
  const double maskColor[3] = {0.73, 0.06, 0.59};
  const std::string SegmentOneID = modelingDataContoursSegmentID();
  const std::string SegmentTwoID = modelingModelContoursSegmentID();
  const std::string SegmentThreeID = modelingModelMaskSegmentID();
  this->removeObsoleteModelingContourSegments();
  this->updateModelingContourSegment(SegmentOneID.c_str(), std::string(volumeOne->GetName()) + levelSuffix,
    dataColor, volumeOne, contourThreshold,
    StringToDouble(volumeOne->GetAttribute("SlicerAstro.DATAMAX")));
  this->updateModelingContourSegment(SegmentTwoID.c_str(), std::string(volumeTwo->GetName()) + levelSuffix,
    modelColor, volumeTwo, contourThreshold,
    StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX")));
  // region of the model (independent of the contour level)
  this->updateModelingContourSegment(SegmentThreeID.c_str(), std::string(volumeTwo->GetName()) + "_asMask",
    maskColor, volumeTwo, 1.E-6, StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX")));

  if (this->contoursSegNodeModeling->GetNumberOfDisplayNodes() < 1)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView error :"
                   " SegmentationNode has no displayNodes!";
    }

  for (int ii = 0; ii < this->contoursSegNodeModeling->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(this->contoursSegNodeModeling->GetNthDisplayNode(ii));
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

  // data and model contours of the Modeling module: closed surfaces in 2D
  // (contour lines), binary labelmaps in 3D
  vtkSlicerAstroVolumeLogic::SetContoursDisplay(this->contoursSegNodeModeling);

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

  vtkSmartPointer<vtkCollection> segNodes = vtkSmartPointer<vtkCollection>::Take
    (this->mrmlScene()->GetNodesByClassByName("vtkMRMLSegmentationNode", "AstroModelingContoursSeg"));
  this->contoursSegNodeModeling = vtkMRMLSegmentationNode::SafeDownCast(segNodes->GetItemAsObject(0));
  if (!this->contoursSegNodeModeling)
    {
    vtkMRMLNode *foo = this->mrmlScene()->CreateNodeByClass("vtkMRMLSegmentationNode");
    this->contoursSegNodeModeling = vtkMRMLSegmentationNode::SafeDownCast(foo);
    this->contoursSegNodeModeling->SetName("AstroModelingContoursSeg");
    this->contoursSegNodeModeling->SetAttribute(vtkSlicerAstroVolumeLogic::GetContoursAttributeName(), "true");
    this->mrmlScene()->AddNode(this->contoursSegNodeModeling);
    }

  // the contours are not edited: the Segment Editor keeps its segmentation (mask)

  if (!this->contoursSegNodeModeling->GetDisplayNode())
    {
    this->contoursSegNodeModeling->CreateDefaultDisplayNodes();
    }

  // Create empty segment in current segmentation

  const double contourThreshold =
    StringToDouble(volumeOne->GetAttribute("SlicerAstro.DisplayThreshold")) * ContourLevel;
  const std::string levelSuffix = "_" + DoubleToString(ContourLevel) + "RMS";
  const double dataColor[3] = {0.5, 0.68, 0.5};
  const double modelColor[3] = {1., 0.9, 0.13};
  const double maskColor[3] = {0.73, 0.06, 0.59};
  const std::string SegmentOneID = modelingDataContoursSegmentID();
  const std::string SegmentTwoID = modelingModelContoursSegmentID();
  const std::string SegmentThreeID = modelingModelMaskSegmentID();
  this->removeObsoleteModelingContourSegments();
  this->updateModelingContourSegment(SegmentOneID.c_str(), std::string(volumeOne->GetName()) + levelSuffix,
    dataColor, volumeOne, contourThreshold,
    StringToDouble(volumeOne->GetAttribute("SlicerAstro.DATAMAX")));
  this->updateModelingContourSegment(SegmentTwoID.c_str(), std::string(volumeTwo->GetName()) + levelSuffix,
    modelColor, volumeTwo, contourThreshold,
    StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX")));
  // region of the model: recomputed when the model changes (independent of
  // the contour level)
  if (overrideSegments || !this->contoursSegNodeModeling->GetSegmentation()->GetSegment(SegmentThreeID))
    {
    this->updateModelingContourSegment(SegmentThreeID.c_str(), std::string(volumeTwo->GetName()) + "_asMask",
      maskColor, volumeTwo, 1.E-6, StringToDouble(volumeTwo->GetAttribute("SlicerAstro.DATAMAX")));
    }

  if (this->contoursSegNodeModeling->GetNumberOfDisplayNodes() < 1)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::updateQuantitative3DView error :"
                   " SegmentationNode has no displayNodes!";
    }

  for (int ii = 0; ii < this->contoursSegNodeModeling->GetNumberOfDisplayNodes(); ii++)
    {
    vtkMRMLSegmentationDisplayNode *SegmentationDisplayNode =
      vtkMRMLSegmentationDisplayNode::SafeDownCast(this->contoursSegNodeModeling->GetNthDisplayNode(ii));
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

  // data and model contours of the Modeling module: closed surfaces in 2D
  // (contour lines), binary labelmaps in 3D
  vtkSlicerAstroVolumeLogic::SetContoursDisplay(this->contoursSegNodeModeling);

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

  if (!d->astroVolumeNode || !d->astroVolumeNode->GetAstroVolumeDisplayNode())
    {
    return;
    }

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

  vtkMRMLVolumeRenderingDisplayNode* volumeRenderingDisplayNode = this->volumeRenderingDisplay();
  vtkMRMLVolumePropertyNode* newVolumePropertyNode =
    volumeRenderingDisplayNode ? volumeRenderingDisplayNode->GetVolumePropertyNode() : nullptr;
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
void qSlicerAstroVolumeModuleWidget::resetAndApplyPreset(vtkMRMLNode *volumePropertyNode)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode)
    {
    return;
    }

  d->astroVolumeNode->SetPresetNode(volumePropertyNode);

  this->resetOffset(d->astroVolumeNode);
  this->resetStretch(d->astroVolumeNode);
  this->resetOpacityShift(d->astroVolumeNode);

  this->applyPreset(volumePropertyNode);
}

//---------------------------------------------------------------------------
vtkMRMLMarkupsROINode* qSlicerAstroVolumeModuleWidget::volumeRenderingROINode(bool create)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode || !d->volumeRenderingLogic)
    {
    return nullptr;
    }
  vtkMRMLVolumeRenderingDisplayNode* displayNode =
    d->astroVolumeNode->GetAstroVolumeRenderingDisplayNode();
  if (!displayNode)
    {
    return nullptr;
    }
  vtkMRMLMarkupsROINode* roiNode = displayNode->GetMarkupsROINode();
  if (!roiNode && create)
    {
    // The Volume Rendering module creates the cropping ROI only on request:
    // create it, fitted to the volume.
    roiNode = vtkMRMLMarkupsROINode::SafeDownCast(d->volumeRenderingLogic->CreateROINode(displayNode));
    if (!roiNode)
      {
      qCritical() << Q_FUNC_INFO << ": the ROI of the volume rendering could not be created.";
      return nullptr;
      }
    this->qvtkConnect(roiNode, vtkMRMLDisplayableNode::DisplayModifiedEvent,
                      this, SLOT(onMRMLDisplayROINodeModified(vtkObject*)));
    }
  return roiNode;
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

  // Fit ROI to volume (a new ROI is already fitted)
  if (displayNode->GetMarkupsROINode() || this->volumeRenderingROINode(true))
    {
    d->volumeRenderingLogic->FitROIToVolume(displayNode);
    }

  if (!d->volumeRenderingWidget)
    {
    return;
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onOffsetValueChanged(double offsetValue)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* volumeDisplay =
    d->astroVolumeNode->GetAstroVolumeDisplayNode();
  if (!volumeDisplay)
    {
    return;
    }

  int wasModifying = volumeDisplay->StartModify();
  volumeDisplay->SetOldPresetOffset(volumeDisplay->GetPresetOffset());
  volumeDisplay->SetPresetOffset(offsetValue);
  if (d->Lock)
    {
    volumeDisplay->SetOldPresetStretch(volumeDisplay->GetPresetStretch());
    volumeDisplay->SetPresetStretch(offsetValue);
    }
  volumeDisplay->EndModify(wasModifying);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onCalculateRMS()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode || !d->volumeRenderingWidget)
    {
    return;
    }

  // Use the cropping ROI of the volume rendering display node: if there is
  // none yet, it is created (fitted to the volume) and shown, so that the
  // user can see and adjust the region used for the noise.
  vtkMRMLMarkupsROINode *roiNode = this->volumeRenderingROINode(false);
  if (!roiNode)
    {
    roiNode = this->volumeRenderingROINode(true);
    if (!roiNode)
      {
      return;
      }
    d->ROICropDisplayCheckBox->setChecked(true);
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
  tableNode->SetUseColumnTitleAsColumnHeader(true);
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
  tableNode->SetColumnTitle("Intensity", "Intensity axes");

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
  tableNode->SetColumnTitle(name.c_str(), "Counts");

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
  vtkMRMLPlotSeriesNode *PlotSeriesNode = nullptr;
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

  vtkMRMLMarkupsROINode* ROINode =
      vtkMRMLMarkupsROINode::SafeDownCast(sender);
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

  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  if (!astroVolumeLogic)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::onMRMLSceneEndCloseEvent error :"
                   " astroVolumeLogic not found!";
    return;
    }
  vtkMRMLScene *presetsScene = astroVolumeLogic->GetPresetsScene();
  d->PresetsNodeComboBox->setMRMLScene(presetsScene);
  d->PresetsNodeComboBox->setCurrentNodeIndex(-1);
  this->onMRMLVolumeNodeDisplayThresholdModified(true);
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
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLViewNodeModified(vtkObject *sender)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!sender)
      {
      return;
      }

  vtkMRMLViewNode* viewNode =
      vtkMRMLViewNode::SafeDownCast(sender);

  if (!viewNode)
    {
    return;
    }

  d->QualityControlComboBox->setCurrentIndex(viewNode ? viewNode->GetVolumeRenderingQuality() : -1);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onOpacityValueChanged(double Opacity)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!d->astroVolumeNode)
    {
    return;
    }

  vtkMRMLAstroVolumeDisplayNode* volumeDisplay =
    d->astroVolumeNode->GetAstroVolumeDisplayNode();
  if (!volumeDisplay)
    {
    return;
    }

  int wasModifying = volumeDisplay->StartModify();
  volumeDisplay->SetOldOpacityShift(volumeDisplay->GetOpacityShift());
  volumeDisplay->SetOpacityShift(Opacity);
  volumeDisplay->EndModify(wasModifying);
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

  vtkTable *histoTable = nullptr;
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

  std::vector<vtkMRMLNode*> viewNodes;
  displayNode->GetScene()->GetNodesByClass("vtkMRMLViewNode", viewNodes);
  for (std::vector<vtkMRMLNode*>::iterator it=viewNodes.begin(); it!=viewNodes.end(); ++it)
    {
    vtkMRMLViewNode* viewNode = vtkMRMLViewNode::SafeDownCast(*it);
    if (displayNode->IsDisplayableInView(viewNode->GetID()))
      {
      viewNode->SetVolumeRenderingQuality(index);
      }
    }

  displayNode->Modified();
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

  vtkMRMLMarkupsROINode* ROINode = this->volumeRenderingROINode(visibility);
  if (!ROINode)
    {
    return;
    }

  int n = ROINode->GetNumberOfDisplayNodes();
  std::vector<int> wasModifying(n);
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

  if (crop)
    {
    this->volumeRenderingROINode(true);
    }
  displayNode->SetCroppingEnabled(crop);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onSegmentationNodeChanged(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
  d->SegmentsTableView->setSegmentationNode(segmentationNode);

  // Save as FITS mask: only masks with voxels
  for (unsigned long event : { vtkSegmentation::SegmentAdded, vtkSegmentation::SegmentRemoved,
                               vtkSegmentation::SegmentModified, vtkSegmentation::RepresentationModified,
                               vtkSegmentation::SourceRepresentationModified })
    {
    this->qvtkReconnect(d->MaskNode, segmentationNode, event, d->SaveFITSMaskButtonTimer, SLOT(start()));
    }
  d->MaskNode = segmentationNode;

  // A new mask (e.g. "Create new Segmentation" in the selector) gets the
  // active cube as reference geometry: it is a mask of that cube
  if (segmentationNode && d->selectionNode && this->mrmlScene())
    {
    vtkMRMLAstroVolumeNode* activeVolume = vtkMRMLAstroVolumeNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID(d->selectionNode->GetActiveVolumeID()));
    vtkSlicerAstroVolumeLogic::SetMaskReferenceVolume(segmentationNode, activeVolume);
    // the mask chosen for the cube becomes its mask (e.g. a SoFiA mask)
    if (activeVolume && vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode) &&
        segmentationNode->GetNodeReference(vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str()) == activeVolume)
      {
      activeVolume->SetNodeReferenceID(vtkSlicerAstroVolumeLogic::GetMaskReferenceRole(), segmentationNode->GetID());
      }
    }
  this->updateSaveFITSMaskButton();
  if (segmentationNode && d->segmentEditorNode &&
      d->segmentEditorNode->GetSegmentationNode() != segmentationNode)
    {
    d->segmentEditorNode->SetAndObserveSegmentationNode(segmentationNode);
    }
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  if (segmentationNode && astroVolumeLogic)
    {
    astroVolumeLogic->EnsureFITSMaskStorageNode(segmentationNode);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::updateMaskSelector()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  QStringList hiddenNodeIDs;
  vtkMRMLScene* scene = this->mrmlScene();
  if (scene)
    {
    std::vector<vtkMRMLNode*> nodes;
    scene->GetNodesByClass("vtkMRMLSegmentationNode", nodes);
    for (vtkMRMLNode* node : nodes)
      {
      vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);
      if (!segmentationNode || !segmentationNode->GetID() || !segmentationNode->GetSegmentation())
        {
        continue;
        }
      if (!vtkSlicerAstroVolumeLogic::IsMaskSegmentation(segmentationNode))
        {
        hiddenNodeIDs << QString::fromUtf8(segmentationNode->GetID());
        }
      }
    }
  d->SegmentationNodeComboBox->sortFilterProxyModel()->setHiddenNodeIDs(hiddenNodeIDs);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::updateSaveFITSMaskButton()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLSegmentationNode* segmentationNode =
    vtkMRMLSegmentationNode::SafeDownCast(d->SegmentationNodeComboBox->currentNode());
  const bool hasVoxels = vtkSlicerAstroVolumeLogic::HasNonEmptySegments(segmentationNode);
  d->SaveFITSMaskPushButton->setEnabled(hasVoxels);
  d->SaveFITSMaskPushButton->setToolTip(hasVoxels ?
    tr("Save the segmentation as a FITS mask (one label value per segment, with the FITS header "
       "of the mask or of the reference data cube).") :
    tr("The mask has no segments or its segments are empty: nothing to save."));
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onSaveFITSMask()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  vtkMRMLScene* scene = this->mrmlScene();
  vtkMRMLSegmentationNode* segmentationNode =
    vtkMRMLSegmentationNode::SafeDownCast(d->SegmentationNodeComboBox->currentNode());
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  if (!scene || !segmentationNode || !astroVolumeLogic)
    {
    return;
    }
  const QString title = tr("Save as FITS mask");

  // The FITS header and the geometry of the mask: the ones of the mask read
  // from file, or of the astronomical volume used as reference geometry
  if (!vtkSlicerAstroVolumeLogic::IsAstroSegmentation(segmentationNode))
    {
    if (!d->astroVolumeNode)
      {
      QMessageBox::warning(this, title,
        tr("The segmentation %1 has no FITS header: select an astronomical data cube in the module "
           "(or as source volume in the Segment Editor) before saving it as a FITS mask.")
          .arg(QString::fromUtf8(segmentationNode->GetName())));
      return;
      }
    if (QMessageBox::question(this, title,
          tr("The segmentation %1 has no FITS header. Use the header and the geometry of %2?")
            .arg(QString::fromUtf8(segmentationNode->GetName()))
            .arg(QString::fromUtf8(d->astroVolumeNode->GetName()))) != QMessageBox::Yes)
      {
      return;
      }
    segmentationNode->SetReferenceImageGeometryParameterFromVolumeNode(d->astroVolumeNode);
    }

  // FITS mask storage (replaces e.g. a .seg.nrrd storage node)
  vtkMRMLStorageNode* storageNode = segmentationNode->GetStorageNode();
  if (storageNode && !storageNode->IsA("vtkMRMLAstroSegmentationStorageNode"))
    {
    segmentationNode->SetAndObserveStorageNodeID(nullptr);
    scene->RemoveNode(storageNode);
    }
  astroVolumeLogic->EnsureFITSMaskStorageNode(segmentationNode);
  storageNode = segmentationNode->GetStorageNode();
  if (!storageNode)
    {
    QMessageBox::critical(this, title, tr("The FITS mask storage could not be created."));
    return;
    }

  QString defaultFileName = storageNode->GetFileName() ? QString::fromUtf8(storageNode->GetFileName()) : QString();
  if (defaultFileName.isEmpty())
    {
    const QString directory = scene->GetRootDirectory() ? QString::fromUtf8(scene->GetRootDirectory()) : QDir::homePath();
    defaultFileName = QDir(directory).filePath(QString::fromUtf8(segmentationNode->GetName()) + ".fits");
    }
  QString fileName = QFileDialog::getSaveFileName(this, title, defaultFileName, tr("FITS mask (*.fits)"));
  if (fileName.isEmpty())
    {
    return;
    }
  if (!fileName.endsWith(".fits", Qt::CaseInsensitive) && !fileName.endsWith(".fits.gz", Qt::CaseInsensitive))
    {
    fileName += ".fits";
    }

  storageNode->SetFileName(fileName.toUtf8().constData());
  storageNode->GetUserMessages()->ClearMessages();
  if (!storageNode->WriteData(segmentationNode))
    {
    QMessageBox::critical(this, title,
      tr("The segmentation could not be saved as FITS mask:\n%1")
        .arg(QString::fromStdString(storageNode->GetUserMessages()->GetAllMessagesAsString())));
    return;
    }
  qSlicerApplication* app = qSlicerApplication::application();
  if (app && app->mainWindow() && app->mainWindow()->statusBar())
    {
    app->mainWindow()->statusBar()->showMessage(tr("Saved %1").arg(fileName), 5000);
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onEditSelectedSegment()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

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

  // Edit the selected mask, with the active cube as source volume
  vtkMRMLSegmentationNode* segmentationNode =
    vtkMRMLSegmentationNode::SafeDownCast(d->SegmentationNodeComboBox->currentNode());
  vtkMRMLScalarVolumeNode* sourceVolumeNode = nullptr;
  if (d->selectionNode && this->mrmlScene() && d->selectionNode->GetActiveVolumeID())
    {
    sourceVolumeNode = vtkMRMLScalarVolumeNode::SafeDownCast(
      this->mrmlScene()->GetNodeByID(d->selectionNode->GetActiveVolumeID()));
    }
  qSlicerAbstractModule* segmentEditorModule = qobject_cast<qSlicerAbstractModule*>(module);
  QWidget* moduleWidget = segmentEditorModule ?
    dynamic_cast<QWidget*>(segmentEditorModule->widgetRepresentation()) : nullptr;
  qMRMLSegmentEditorWidget* editorWidget = moduleWidget ?
    moduleWidget->findChild<qMRMLSegmentEditorWidget*>() : nullptr;
  if (editorWidget)
    {
    if (segmentationNode)
      {
      editorWidget->setSegmentationNode(segmentationNode);
      }
    if (sourceVolumeNode)
      {
      editorWidget->setSourceVolumeNode(sourceVolumeNode);
      }
    }
  else if (d->segmentEditorNode)
    {
    if (segmentationNode)
      {
      d->segmentEditorNode->SetAndObserveSegmentationNode(segmentationNode);
      }
    if (sourceVolumeNode)
      {
      d->segmentEditorNode->SetAndObserveSourceVolumeNode(sourceVolumeNode);
      }
    }
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

  vtkTable *histoTable = nullptr;
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

  vtkMRMLViewNode* firstViewNode = nullptr;
  if (displayNode && displayNode->GetScene())
    {
    firstViewNode = displayNode->GetFirstViewNode();
    }

  this->qvtkConnect(firstViewNode, vtkCommand::ModifiedEvent,
                    this, SLOT(onMRMLViewNodeModified(vtkObject*)));

  this->onMRMLVolumeRenderingDisplayNodeModified(displayNode);
  this->onMRMLDisplayROINodeModified(displayNode->GetROINode());
  this->onMRMLViewNodeModified(firstViewNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  vtkMRMLSelectionNode *selectionNode =
      vtkMRMLSelectionNode::SafeDownCast(sender);

  if (!selectionNode || !this->mrmlScene())
    {
    return;
    }
  const char *activeVolumeNodeID = selectionNode->GetActiveVolumeID();
  const char *activeLabelMapVolumeNodeID = selectionNode->GetActiveLabelVolumeID();

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

  vtkMRMLAstroVolumeDisplayNode* volumeDisplay =
    d->astroVolumeNode->GetAstroVolumeDisplayNode();
  if (!volumeDisplay)
    {
    return;
    }

  int n = StringToInt(d->astroVolumeNode->GetAttribute("SlicerAstro.NAXIS"));
  if (n == 3)
    {
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

    volumePropertyWidget->moveAllPoints(volumeDisplay->GetPresetOffset() -
                                        volumeDisplay->GetOldPresetOffset(), 0., true);
    d->PresetOffsetSlider->setValue(volumeDisplay->GetPresetOffset());

    volumeDisplay->DisableModifiedEventOn();
    volumeDisplay->SetOldPresetOffset(volumeDisplay->GetPresetOffset());
    volumeDisplay->DisableModifiedEventOff();

    volumePropertyWidget->spreadAllPoints(volumeDisplay->GetPresetStretch() -
                                          volumeDisplay->GetOldPresetStretch(), true);
    d->PresetStretchSlider->setValue(volumeDisplay->GetPresetStretch());

    volumeDisplay->DisableModifiedEventOn();
    volumeDisplay->SetOldPresetStretch(volumeDisplay->GetPresetStretch());
    volumeDisplay->DisableModifiedEventOff();

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

    double OpacityShift = volumeDisplay->GetOpacityShift() -
                          volumeDisplay->GetOldOpacityShift();
    for (int ii = 0; ii < opacities->GetSize(); ii++)
      {
      double value[4];
      opacities->GetNodeValue(ii, value);
      if (fabs(value[1]) < 1.E-4)
        {
        continue;
        }
      value[1] += OpacityShift;
      if (value[1] > 1.)
        {
        value[1] = 1.;
        }
      if (fabs(value[1]) < 1.E-4)
        {
        value[1] = 1.E-3;
        }

      opacities->SetNodeValue(ii, value);
      }

    volumeProperty->Modified();

    volumeDisplay->DisableModifiedEventOn();
    volumeDisplay->SetOldOpacityShift(volumeDisplay->GetOpacityShift());
    volumeDisplay->DisableModifiedEventOff();
    }

  d->OpacitySliderWidget->setValue(volumeDisplay->GetOpacityShift());

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

  if (!d->astroVolumeNode || !this->mrmlScene())
    {
    return;
    }

  double DisplayThreshold = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DisplayThreshold"));

  int status = 0;
  if (forcePreset)
    {
    status = d->DisplayThresholdSliderWidget->blockSignals(false);
    d->DisplayThresholdSliderWidget->setValue(DisplayThreshold);
    }
  else
    {
    status = d->DisplayThresholdSliderWidget->blockSignals(true);
    d->DisplayThresholdSliderWidget->setValue(DisplayThreshold);
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
  d->DisplayThresholdSliderWidget->blockSignals(status);

  if (forcePreset)
    {
    this->resetOffset(d->astroVolumeNode);
    this->resetStretch(d->astroVolumeNode);
    this->resetOpacityShift(d->astroVolumeNode);
    }
  else if (d->astroVolumeNode->GetVolumePropertyNode())
    {
    this->resetOffsetStep(d->astroVolumeNode);
    this->resetStretchStep(d->astroVolumeNode);
    }

  this->updatePresets(d->astroVolumeNode);
  this->applyPreset(d->astroVolumeNode->GetPresetNode());

  if (d->TableThresholdNode && forcePreset)
    {
    d->TableThresholdNode->DisableModifiedEventOn();
    d->TableThresholdNode->GetTable()->SetValue(0, 0, DisplayThreshold);
    d->TableThresholdNode->GetTable()->SetValue(1, 0, DisplayThreshold);
    d->TableThresholdNode->GetTable()->SetValue(2, 0, DisplayThreshold);
    d->TableThresholdNode->GetTable()->Modified();
    if (d->plotChartNodeHistogram)
      {
      d->plotChartNodeHistogram->Modified();
      }
    d->TableThresholdNode->DisableModifiedEventOff();
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
    QMetaObject::invokeMethod(d->volumeRenderingWidget, "setMRMLVolumeNode", Q_ARG(vtkMRMLNode*, volumeNode));
    d->astroVolumeNode = nullptr;
    return;
    }

  if (d->astroVolumeNode == volumeNode)
    {
    return;
    }

  // the mask of this cube (created when it was loaded) in the Masks section;
  // otherwise the mask of the Masks section (e.g. created when the module
  // was opened, before the data was loaded) becomes a mask of this cube
  vtkSlicerAstroVolumeLogic* astroVolumeLogic = vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  vtkMRMLSegmentationNode* volumeMask = astroVolumeLogic ?
    astroVolumeLogic->GetMaskSegmentationForVolume(volumeNode, false) : nullptr;
  if (volumeMask)
    {
    if (d->SegmentationNodeComboBox->currentNode() != volumeMask)
      {
      d->SegmentationNodeComboBox->setCurrentNode(volumeMask);
      }
    }
  else
    {
    vtkSlicerAstroVolumeLogic::SetMaskReferenceVolume(
      vtkMRMLSegmentationNode::SafeDownCast(d->SegmentationNodeComboBox->currentNode()), volumeNode);
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
      QMetaObject::invokeMethod(d->volumeRenderingWidget, "setMRMLVolumeNode", Q_ARG(vtkMRMLNode*, d->astroVolumeNode));
      this->ensureVolumeRenderingTransferFunctionGaps();
      }

    d->Lock = false;
    d->LockPushButton->setChecked(false);
    d->LockPushButton->setIcon(QIcon(":Icons/astrounlock.png"));

    bool forcePreset = false;
    if (d->astroVolumeNode->GetVolumePropertyNode() == nullptr)
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
    this->qvtkReconnect(d->astroVolumeNode, vtkMRMLAstroVolumeNode::ReferenceRemovedEvent,
                        this, SLOT(setDisplayConnection()));
    this->qvtkReconnect(d->astroVolumeNode, vtkMRMLAstroVolumeNode::ReferenceModifiedEvent,
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

  d->ActiveVolumeNodeSelector->setCurrentNodeID(d->astroVolumeNode->GetID());

  d->DisplayCollapsibleButton->setCollapsed(false);
  d->CollapsibleButton_CopyMoveSegment->setCollapsed(true);

  this->setEnabled(d->astroVolumeNode != 0);
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
    d->astroLabelVolumeNode = nullptr;
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
qSlicerAbstractModuleWidget* qSlicerAstroVolumeModuleWidget::volumeRenderingWidget() const
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
  if (!d->volumeRenderingLogic || !d->astroVolumeNode)
    {
    return nullptr;
    }
  return d->volumeRenderingLogic->GetFirstVolumeRenderingDisplayNode(d->astroVolumeNode);
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
