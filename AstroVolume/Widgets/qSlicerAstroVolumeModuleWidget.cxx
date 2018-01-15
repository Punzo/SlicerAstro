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
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkImageThreshold.h>
#include <vtkLookupTable.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTransform.h>

// qMRMLWidgets include
#include <qMRMLAstroVolumeInfoWidget.h>
#include <qMRMLThreeDViewControllerWidget.h>
#include <qMRMLThreeDWidget.h>

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
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLLayoutNode.h>
#include <vtkMRMLProceduralColorNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentEditorNode.h>
#include <vtkMRMLSliceNode.h>
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

  qSlicerVolumeRenderingModuleWidget* volumeRenderingWidget;
  qMRMLAstroVolumeInfoWidget *MRMLAstroVolumeInfoWidget;
  vtkSlicerSegmentationsModuleLogic* segmentationsLogic;
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
  this->selectionNode = 0;
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
short StringToShort(const char* str)
{
  return StringToNumber<short>(str);
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

  this->SynchronizeScalarDisplayNodeButton->setEnabled(false);

  QObject::connect(this->VisibilityCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onVisibilityChanged(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this->AstroVolumeDisplayWidget, SLOT(setMRMLVolumeNode(vtkMRMLNode*)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->AstroVolumeDisplayWidget, SLOT(setEnabled(bool)));

  QObject::connect(this->volumeRenderingWidget, SIGNAL(currentVolumeRenderingDisplayNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setDisplayConnection(vtkMRMLNode*)));

  QObject::connect(this->volumeRenderingWidget, SIGNAL(currentVolumeNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLVolumeNode(vtkMRMLNode*)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->VisibilityCheckBox, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->SynchronizeScalarDisplayNodeButton, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->RenderingMethodComboBox, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->QualityControlComboBox, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->PresetOffsetSlider, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->PresetStretchSlider, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->LockPushButton, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->PresetsNodeComboBox, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->activateLabel, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->synchLabel, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->PresetsLabel, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->shiftLabel, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->StretchLabel, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->CropLabel, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->RenderingMethodLabel, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->qualityLabel, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->OpticalVelocityButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->RadioVelocityButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->VelocityLabel, SLOT(setEnabled(bool)));

  QObject::connect(this->OpticalVelocityButton, SIGNAL(clicked()),
                   q, SLOT(setOpticalVelocity()));

  QObject::connect(this->RadioVelocityButton, SIGNAL(clicked()),
                   q, SLOT(setRadioVelocity()));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->DegreeUnitButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->SexagesimalUnitButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->RightAscensionUnitLabel, SLOT(setEnabled(bool)));

  QObject::connect(this->DegreeUnitButton, SIGNAL(clicked()),
                   q, SLOT(setRADegreeUnit()));

  QObject::connect(this->SexagesimalUnitButton, SIGNAL(clicked()),
                   q, SLOT(setRASexagesimalUnit()));

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

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->DisplayThresholdLabel, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->DisplayThresholdDoubleSpinBox, SLOT(setEnabled(bool)));

  QObject::connect(this->DisplayThresholdDoubleSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onDisplayThresholdValueChanged(double)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->CalculateRMSPushButton, SLOT(setEnabled(bool)));

  QObject::connect(this->CalculateRMSPushButton, SIGNAL(clicked()),
                   q, SLOT(onCalculateRMS()));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->ROICropCheckBox, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->ROICropDisplayCheckBox, SLOT(setEnabled(bool)));

  QObject::connect(q, SIGNAL(astroVolumeNodeChanged(bool)),
                   this->ROIFitPushButton, SLOT(setEnabled(bool)));

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

  qSlicerAbstractCoreModule* segmentations= app->moduleManager()->module("Segmentations");
  if (segmentations)
    {
    this->segmentationsLogic = vtkSlicerSegmentationsModuleLogic::SafeDownCast(segmentations->logic());
    }

  QObject::connect(q, SIGNAL(astroLabelMapVolumeNodeChanged(bool)),
                   this->pushButtonCovertLabelMapToSegmentation, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->pushButtonConvertSegmentationToLabelMap, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->CreateSurfaceButton, SLOT(setEnabled(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->pushButton_EditSelected, SLOT(setEnabled(bool)));

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

  this->initializeSegmentations(scene);

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
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::initializeSegmentations(vtkMRMLScene *scene)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!scene)
    {
    return;
    }

  std::string segmentEditorSingletonTag = "SegmentEditor";
  vtkMRMLSegmentEditorNode *segmentEditorNodeSingleton = vtkMRMLSegmentEditorNode::SafeDownCast(
    scene->GetSingletonNode(segmentEditorSingletonTag.c_str(), "vtkMRMLSegmentEditorNode"));

  if (!segmentEditorNodeSingleton)
    {
    d->segmentEditorNode = vtkSmartPointer<vtkMRMLSegmentEditorNode>::New();
    d->segmentEditorNode->SetSingletonTag(segmentEditorSingletonTag.c_str());
    d->segmentEditorNode = vtkMRMLSegmentEditorNode::SafeDownCast(
    scene->AddNode(d->segmentEditorNode));
    }
  else
    {
    d->segmentEditorNode = segmentEditorNodeSingleton;
    }

  this->qvtkReconnect(d->segmentEditorNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onSegmentEditorNodeModified(vtkObject*)));

  this->onSegmentEditorNodeModified(d->segmentEditorNode);

  if (!d->segmentEditorNode->GetSegmentationNode())
    {
    vtkSmartPointer<vtkMRMLNode> segmentationNode;
    vtkMRMLNode *foo = scene->CreateNodeByClass("vtkMRMLSegmentationNode");
    segmentationNode.TakeReference(foo);
    scene->AddNode(segmentationNode);
    d->segmentEditorNode->SetAndObserveSegmentationNode
      (vtkMRMLSegmentationNode::SafeDownCast(segmentationNode));
  }
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::initializeColorNodes()
{
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
      tempColorTableNode->SetAttribute("SlicerAstro.AddFunctions", "on");
      tempColorTableNode->SetAttribute("SlicerAstro.Reverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Inverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Log", "off");
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "GenericColors") ||
        !strcmp(tempColorTableNode->GetName(), "MediumChartColors"))
      {
      continue;
      }

    this->mrmlScene()->RemoveNode(tempColorTableNode);
    }

  // Readd Astro 2D color functions
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

  vtkNew<vtkMRMLColorTableNode> VelocityFieldColorTableNode;
  VelocityFieldColorTableNode->SetType(vtkMRMLColorTableNode::User);
  VelocityFieldColorTableNode->SetName("Velocity Field");
  VelocityFieldColorTableNode->SetDescription("A scale from blue to red.");
  VelocityFieldColorTableNode->SetNumberOfColors(256);

  // Red component
  RedLookupTable->SetNumberOfTableValues(128);
  RedLookupTable->SetTableRange(0, 128);
  RedLookupTable->SetHueRange(0, 0);
  RedLookupTable->SetSaturationRange(1,1);
  RedLookupTable->SetValueRange(0.,1);
  RedLookupTable->SetRampToLinear();
  RedLookupTable->ForceBuild();

  // Green component
  GreenLookupTable->SetNumberOfTableValues(128);
  GreenLookupTable->SetTableRange(0, 128);
  GreenLookupTable->SetHueRange(0.333, 0.333);
  GreenLookupTable->SetSaturationRange(1,1);
  GreenLookupTable->SetValueRange(0.,1);
  GreenLookupTable->SetRampToLinear();
  GreenLookupTable->ForceBuild();

  // Blue component
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

  // Add Generic Colors
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
    if (!strcmp(tempColorTableNode->GetName(), "Heat") ||
        !strcmp(tempColorTableNode->GetName(), "Ronekers") ||
        !strcmp(tempColorTableNode->GetName(), "Velocity Field"))
      {
      tempColorTableNode->SetAttribute("SlicerAstro.AddFunctions", "on");
      tempColorTableNode->SetAttribute("SlicerAstro.Reverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Inverse", "off");
      tempColorTableNode->SetAttribute("SlicerAstro.Log", "off");
      continue;
      }
    if (!strcmp(tempColorTableNode->GetName(), "GenericColors") ||
        !strcmp(tempColorTableNode->GetName(), "MediumChartColors"))
      {
      vtkNew<vtkMRMLColorTableNode> tempTableNode;
      tempTableNode->Copy(tempColorTableNode);
      this->mrmlScene()->RemoveNode(tempColorTableNode);
      tempTableNode->SetAttribute("SlicerAstro.AddFunctions", "off");
      this->mrmlScene()->AddNode(tempTableNode.GetPointer());
      continue;
      }
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
    this->mrmlScene()->RemoveNode(tempProceduralColorTableNode);
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
    emit astroLabelMapVolumeNodeChanged(false);
    emit astroVolumeNodeChanged(false);
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

  bool renderingActive = false;

  if (astroVolumeNode)
    {
    int n = StringToInt(astroVolumeNode->GetAttribute("SlicerAstro.NAXIS"));
    // Check Input volume dimensionality
    if (n == 3)
      {
      renderingActive = true;
      }
    emit astroLabelMapVolumeNodeChanged(false);
    emit astroVolumeNodeChanged(true);

    std::string type = astroVolumeNode->GetAttribute("SlicerAstro.DATAMODEL");
    size_t found = type.find("MOMENTMAP");
    if (!(found != std::string::npos))
      {
      d->selectionNode->SetReferenceActiveVolumeID(astroVolumeNode->GetID());
      d->selectionNode->SetActiveVolumeID(astroVolumeNode->GetID());
      appLogic->PropagateBackgroundVolumeSelection(1);
      }
    astroVolumeNode->Modified();
    }
  else if (labelMapVolumeNode)
    {
    renderingActive = false;
    emit astroLabelMapVolumeNodeChanged(true);
    emit astroVolumeNodeChanged(false);
    d->selectionNode->SetReferenceActiveLabelVolumeID(labelMapVolumeNode->GetID());
    d->selectionNode->SetActiveLabelVolumeID(labelMapVolumeNode->GetID());
    appLogic->PropagateLabelVolumeSelection(1);
    labelMapVolumeNode->Modified();
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

  if (!d->RenderingFrame ||
      !d->ActiveVolumeNodeSelector ||
      !d->volumeRenderingWidget ||
      !d->PresetsNodeComboBox)
    {
    return;
    }

  if (!renderingActive && !astroVolumeNode)
    {
    d->RenderingFrame->setEnabled(false);
    d->RenderingFrame->setCollapsed(true);
    d->VisibilityCheckBox->setChecked(false);
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
    d->PresetsNodeComboBox->setCurrentNode(inputVolume->GetPresetNode());
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
    emit segmentEditorNodeChanged(true);
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

  // Add a segment of 4 voxels to ensure the segmentation Bounds are the same of the LabelMap
  // (however, it will be present a segement more which it is not ideal)
  int* dims = labelMapNode->GetImageData()->GetDimensions();
  const int numElements = dims[0] * dims[1] * dims[2];
  short* voxelPtr = static_cast<short*>(labelMapNode->GetImageData()->GetScalarPointer());
  short val = StringToShort(labelMapNode->GetAttribute("SlicerAstro.DATAMAX")) + 1;

  *(voxelPtr) = val;
  *(voxelPtr + numElements - 1) = val;

  labelMapNode->UpdateRangeAttributes();

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

  *(voxelPtr) = 0;
  *(voxelPtr + numElements - 1) = 0;

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
    emit segmentEditorNodeChanged(true);
    this->qvtkReconnect(d->segmentEditorNode, vtkCommand::ModifiedEvent,
                        this, SLOT(onSegmentEditorNodeModified(vtkObject*)));
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

  vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode> labelMapNode =
    vtkSmartPointer<vtkMRMLAstroLabelMapVolumeNode>::New();

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
      qCritical() <<"qSlicerAstroVolumeModuleWidget::convertFirstSegmentToLabelMap :"
                    " astroVolumelogic not found!";
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

  int Extents[6] = { 0, 0, 0, 0, 0, 0 };
  labelMapNode->GetImageData()->GetExtent(Extents);

  bool exportSuccess = vtkSlicerSegmentationsModuleLogic::ExportSegmentsToLabelmapNode(currentSegmentationNode, segmentIDs, labelMapNode);

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

  labelMapNode->GetAstroLabelMapVolumeDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeFileGenericColors.txt");

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
                                                           bool generateMasks)
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

  vtkMRMLAstroVolumeNode *volumeOne = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeOneID));
  vtkMRMLAstroVolumeNode *volumeTwo = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(volumeNodeTwoID));

  if(!volumeOne || !volumeTwo)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setComparative3DViews : volumes not valid.";
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

  d->selectionNode->SetReferenceActiveVolumeID(volumeOne->GetID());

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

  if(!d->PresetsNodeComboBox)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView error :"
                   " PresetsNodeComboBox not found!"<<endl;
    return;
    }

  d->selectionNode->SetReferenceActiveVolumeID(volumeTwo->GetID());

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

  if(!d->PresetsNodeComboBox)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView error :"
                   " PresetsNodeComboBox not found!"<<endl;
    return;
    }

  d->PresetsNodeComboBox->setCurrentNodeIndex(-1);
  d->PresetsNodeComboBox->setCurrentNodeIndex(0);
  this->applyPreset(d->PresetsNodeComboBox->currentNode());

  d->selectionNode->SetReferenceActiveVolumeID(volumeOne->GetID());
  d->selectionNode->SetReferenceSecondaryVolumeID(volumeTwo->GetID());

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

  appLogic->PropagateVolumeSelection();

  if (!generateMasks)
    {
    return;
    }

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
    SegmentationDisplayNode->SetAllSegmentsVisibility(false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentOneID, false);
    SegmentationDisplayNode->SetSegmentVisibility2DFill(SegmentTwoID, false);
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

  d->selectionNode->SetReferenceActiveVolumeID(volumeTwo->GetID());

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

  d->selectionNode->SetReferenceActiveVolumeID(volumeThree->GetID());

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

  d->selectionNode->SetReferenceSecondaryVolumeID(volumeTwo->GetID());
  d->selectionNode->SetReferenceActiveVolumeID(volumeOne->GetID());

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
  if(!d->PresetsNodeComboBox)
    {
    qCritical() << "qSlicerAstroVolumeModuleWidget::setQuantitative3DView error :"
                   " PresetsNodeComboBox not found!"<<endl;
    return;
    }

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

  astroVolumeLogic->CalculateRMSinROI(roiNode, d->astroVolumeNode);
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
  this->initializeSegmentations(this->mrmlScene());
  this->initializeColorNodes();
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

  this->initializeSegmentations(this->mrmlScene());
  this->initializeColorNodes();
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::onMRMLSceneStartImportEvent()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if (!this->mrmlScene())
    {
    return;
    }

  if (d->segmentEditorNode)
    {
    if (d->segmentEditorNode->GetSegmentationNode())
      {
      this->mrmlScene()->RemoveNode(d->segmentEditorNode->GetSegmentationNode());
      d->segmentEditorNode->SetAndObserveSegmentationNode(NULL);
      }
    }
  if (d->SegmentsTableView_2)
    {
    if (d->SegmentsTableView_2->segmentationNode())
      {
      this->mrmlScene()->RemoveNode(d->SegmentsTableView_2->segmentationNode());
      d->SegmentsTableView_2->setSegmentationNode(NULL);
      }
    }
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
    d->DisplayThresholdDoubleSpinBox->setValue(DisplayThreshold);
    }
  else
    {
    int status = d->DisplayThresholdDoubleSpinBox->blockSignals(true);
    d->DisplayThresholdDoubleSpinBox->setValue(DisplayThreshold);
    d->DisplayThresholdDoubleSpinBox->blockSignals(status);
    }
  double max = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMAX"));
  d->DisplayThresholdDoubleSpinBox->setMaximum(max);
  double min = StringToDouble(d->astroVolumeNode->GetAttribute("SlicerAstro.DATAMIN"));
  if (min < 0.)
    {
    min = 0.;
    }
  d->DisplayThresholdDoubleSpinBox->setMinimum(min);
  d->DisplayThresholdDoubleSpinBox->setSingleStep((max - min) / 1000.);
  QString DisplayThresholdUnit = "  ";
  DisplayThresholdUnit += d->astroVolumeNode->GetAttribute("SlicerAstro.BUNIT");
  d->DisplayThresholdDoubleSpinBox->setSuffix(DisplayThresholdUnit);

  this->resetOffset(d->astroVolumeNode);
  this->resetStretch(d->astroVolumeNode);

  if (forcePreset)
    {
    this->updatePresets(d->astroVolumeNode);
    d->PresetsNodeComboBox->setCurrentNodeIndex(d->astroVolumeNode->GetPresetIndex());
    this->applyPreset(d->astroVolumeNode->GetPresetNode());
    }
  else if (d->astroVolumeNode->GetVolumePropertyNode())
    {
    this->applyPreset(d->astroVolumeNode->GetVolumePropertyNode());
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
      d->PresetsNodeComboBox->setCurrentNodeIndex(0);
      forcePreset = true;
      }
    this->qvtkReconnect(d->astroVolumeNode, vtkMRMLAstroVolumeNode::DisplayThresholdModifiedEvent,
                        this, SLOT(onMRMLVolumeNodeDisplayThresholdModified()));
    this->onMRMLVolumeNodeDisplayThresholdModified(forcePreset);
    d->VisibilityCheckBox->setChecked(true);
    }
  this->qvtkReconnect(d->astroVolumeNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLVolumeNodeModified()));
  this->onMRMLVolumeNodeModified();

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

  d->astroLabelVolumeNode =
    vtkMRMLAstroLabelMapVolumeNode::SafeDownCast(volumeNode);
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
