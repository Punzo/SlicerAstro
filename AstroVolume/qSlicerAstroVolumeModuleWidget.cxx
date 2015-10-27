// Qt includes
#include <QSettings>
#include <QtDebug>

// CTK includes
#include <ctkUtils.h>

// qMRMLWidgets include
#include "qMRMLAstroVolumeInfoWidget.h"

// SlicerQt includes
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>
#include <qSlicerModuleManager.h>
#include <qSlicerApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerAbstractCoreModule.h>
#include "qSlicerAstroVolumeModuleWidget.h"
#include "ui_qSlicerAstroVolumeModuleWidget.h"
#include "qSlicerPresetComboBox.h"
#include "qSlicerPresetComboBox_p.h"
#include "qSlicerVolumeRenderingModuleWidget.h"

// MRML includes
#include "vtkMRMLAnnotationROINode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLViewNode.h"
#include "vtkMRMLVolumeRenderingDisplayNode.h"

// logic includes
#include "vtkSlicerVolumeRenderingLogic.h"
#include "vtkSlicerAstroVolumeLogic.h"

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
};

//-----------------------------------------------------------------------------
// qSlicerVolumeRenderingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidgetPrivate::qSlicerAstroVolumeModuleWidgetPrivate(
  qSlicerAstroVolumeModuleWidget& object)
  : q_ptr(&object)
{
  this->volumeRenderingWidget = 0;
}

//-----------------------------------------------------------------------------
qSlicerAstroVolumeModuleWidgetPrivate::~qSlicerAstroVolumeModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidgetPrivate::setupUi(qSlicerAstroVolumeModuleWidget* q)
{
  this->Ui_qSlicerAstroVolumeModuleWidget::setupUi(q);

  this->MRMLAstroVolumeInfoWidget = new qMRMLAstroVolumeInfoWidget(InfoCollapsibleButton);
  this->MRMLAstroVolumeInfoWidget->setObjectName(QString::fromUtf8("MRMLAstroVolumeInfoWidget"));

  this->verticalLayout->addWidget(MRMLAstroVolumeInfoWidget);

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this->MRMLAstroVolumeInfoWidget, SLOT(setVolumeNode(vtkMRMLNode*)));

  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerAbstractCoreModule* volumeRendering = app->moduleManager()->module("VolumeRendering");
  if (volumeRendering)
    {
     this->volumeRenderingWidget = dynamic_cast<qSlicerVolumeRenderingModuleWidget*>
         (volumeRendering->widgetRepresentation());
    }

  this->SynchronizeScalarDisplayNodeButton->setEnabled(false);

  QObject::connect(this->VisibilityCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onVisibilityChanged(bool)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this->AstroVolumeDisplayWidget, SLOT(setMRMLVolumeNode(vtkMRMLNode*)));
  QObject::connect(this->volumeRenderingWidget, SIGNAL(newCurrentMRMLVolumeNode(vtkMRMLNode*)),
                   this->ActiveVolumeNodeSelector, SLOT(setCurrentNode(vtkMRMLNode*)));
  QObject::connect(this->volumeRenderingWidget, SIGNAL(newCurrentDisplayNode(vtkMRMLNode*)),
                   q, SLOT(setDisplayConnection(vtkMRMLNode*)));

  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->VisibilityCheckBox, SLOT(setEnabled(bool)));
  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   this->volumeRenderingWidget, SLOT(setMRMLVolumeNode(vtkMRMLNode*)));
  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->RenderingMethodComboBox, SLOT(setEnabled(bool)));
  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->QualityControlComboBox, SLOT(setEnabled(bool)));
  QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                   this->PresetOffsetSlider, SLOT(setEnabled(bool)));

 // Techniques
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

 QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                  q, SLOT(resetOffset(vtkMRMLNode*)));

 QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                  q, SLOT(SetPresets(vtkMRMLNode*)));

 QObject::connect(this->PresetsNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                  this->volumeRenderingWidget, SLOT(applyPreset(vtkMRMLNode*)));

 QObject::connect(this->PresetOffsetSlider, SIGNAL(valueChanged(double)),
                  this->volumeRenderingWidget, SLOT(offsetPreset(double)));
 QObject::connect(this->PresetOffsetSlider, SIGNAL(sliderPressed()),
                  this->volumeRenderingWidget, SLOT(startInteraction()));
 QObject::connect(this->PresetOffsetSlider, SIGNAL(valueChanged(double)),
                  this->volumeRenderingWidget, SLOT(interaction()));
 QObject::connect(this->PresetOffsetSlider, SIGNAL(sliderReleased()),
                  this->volumeRenderingWidget, SLOT(endInteraction()));

 QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                  this->ROICropCheckBox, SLOT(setEnabled(bool)));
 QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                  this->ROICropDisplayCheckBox, SLOT(setEnabled(bool)));
 QObject::connect(this->ActiveVolumeNodeSelector, SIGNAL(currentNodeChanged(bool)),
                  this->ROIFitPushButton, SLOT(setEnabled(bool)));

 QObject::connect(this->ROICropDisplayCheckBox, SIGNAL(toggled(bool)),
                  q, SLOT(setDisplayROIEnabled(bool)));

 QObject::connect(this->ROICropDisplayCheckBox, SIGNAL(toggled(bool)),
                  q, SLOT(onROICropDisplayCheckBoxToggled(bool)));

 // Rendering
 QObject::connect(this->ROICropCheckBox, SIGNAL(toggled(bool)),
                  q, SLOT(onCropToggled(bool)));
 QObject::connect(this->ROIFitPushButton, SIGNAL(clicked()),
                  this->volumeRenderingWidget, SLOT(fitROIToVolume()));

 QObject::connect(this->SynchronizeScalarDisplayNodeButton, SIGNAL(clicked()),
                  this->volumeRenderingWidget, SLOT(synchronizeScalarDisplayNode()));

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
}

//-----------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::setup()
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  d->setupUi(this);

  this->qvtkDisconnectAll();

  if(d->volumeRenderingWidget)
    {
    vtkMRMLVolumeRenderingDisplayNode* displayNode = vtkMRMLVolumeRenderingDisplayNode::
       SafeDownCast(d->volumeRenderingWidget->mrmlDisplayNode());

    if(displayNode)
      {
      this->qvtkConnect(displayNode, vtkCommand::ModifiedEvent,
                  this, SLOT(onMRMLVolumeRenderingDisplayNodeModified(vtkObject*)));
      }
    }
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::resetOffset(vtkMRMLNode* node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  double width = 0.;
  if (node && (node->IsA("vtkMRMLAstroVolumeNode") ||
               node->IsA("vtkMRMLAstroLabelMapVolumeNode")))
    {
    width = StringToDouble(node->GetAttribute("SlicerAstro.DATAMAX")) -
        StringToDouble(node->GetAttribute("SlicerAstro.DATAMIN"));
    }

  d->PresetOffsetSlider->setValue(0.);

  bool wasBlocking = d->PresetOffsetSlider->blockSignals(true);
  d->PresetOffsetSlider->setSingleStep(
    width ? ctk::closestPowerOfTen(width) / 100. : 0.1);
  d->PresetOffsetSlider->setPageStep(d->PresetOffsetSlider->singleStep());
  d->PresetOffsetSlider->setRange(-width, width);
  d->PresetOffsetSlider->blockSignals(wasBlocking);
}

//---------------------------------------------------------------------------
void qSlicerAstroVolumeModuleWidget::SetPresets(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!node)
    {
    return;
    }

  vtkSlicerAstroVolumeLogic* astroVolumeLogic =
    vtkSlicerAstroVolumeLogic::SafeDownCast(this->logic());
  vtkMRMLScene *presetsScene = astroVolumeLogic->GetPresetsScene();

  if(astroVolumeLogic->synchronizePresetsToVolumeNode(node))
    {
    qWarning() << "error encountered in adjusting the Color Function for the 3-D display.";
    }

  d->PresetsNodeComboBox->setMRMLScene(presetsScene);
  d->PresetsNodeComboBox->setCurrentNode(0);
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
void qSlicerAstroVolumeModuleWidget::
  onMRMLVolumeRenderingDisplayNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

  if(!sender)
    {
    return;
    }

  vtkMRMLVolumeRenderingDisplayNode* displayNode =
      vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(sender);
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
void qSlicerAstroVolumeModuleWidget::setDisplayConnection(vtkMRMLNode *node)
{
  Q_D(qSlicerAstroVolumeModuleWidget);

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
    if (displayNodeIter && displayNode)
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
