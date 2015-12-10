// Qt includes
#include <QDebug>
#include <QMessageBox>

// CTK includes
#include <ctkFlowLayout.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>

// Smoothing includes
#include "qSlicerSmoothingModuleWidget.h"
#include "ui_qSlicerSmoothingModuleWidget.h"

// Smoothing Logic includes
#include <vtkSlicerSmoothingLogic.h>

// Astro Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// qMRML includes
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>
#include <qSlicerApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerAstroVolumeModuleWidget.h>

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

#include <vtkNew.h>
#include <vtkMatrix4x4.h>

// MRML includes
#include <vtkMRMLSmoothingParametersNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLSelectionNode.h>


//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Smoothing
class qSlicerSmoothingModuleWidgetPrivate: public Ui_qSlicerSmoothingModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerSmoothingModuleWidget);
protected:
  qSlicerSmoothingModuleWidget* const q_ptr;
public:

  qSlicerSmoothingModuleWidgetPrivate(qSlicerSmoothingModuleWidget& object);
  ~qSlicerSmoothingModuleWidgetPrivate();
  void init();

  vtkSlicerSmoothingLogic* logic() const;
  qSlicerAstroVolumeModuleWidget* astroVolumeWidget;
  vtkMRMLSmoothingParametersNode* parametersNode;

};

//-----------------------------------------------------------------------------
// qSlicerSmoothingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerSmoothingModuleWidgetPrivate::qSlicerSmoothingModuleWidgetPrivate(qSlicerSmoothingModuleWidget& object)
  : q_ptr(&object)
{
  this->parametersNode = 0;
  this->astroVolumeWidget = 0;
}

//-----------------------------------------------------------------------------
qSlicerSmoothingModuleWidgetPrivate::~qSlicerSmoothingModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidgetPrivate::init()
{
  Q_Q(qSlicerSmoothingModuleWidget);
  this->setupUi(q);
  qSlicerApplication* app = qSlicerApplication::application();
      qSlicerAbstractCoreModule* astroVolume = app->moduleManager()->module("AstroVolume");
  if (astroVolume)
    {
    this->astroVolumeWidget = dynamic_cast<qSlicerAstroVolumeModuleWidget*>
      (astroVolume->widgetRepresentation());
    }

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   ParametersNodeComboBox, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   InputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   OutputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLSmoothingParametersNode(vtkMRMLNode*)));

  QObject::connect(InputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onInputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(OutputVolumeNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(onOutputVolumeChanged(vtkMRMLNode*)));

  QObject::connect(ManualModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(AutomaticModeRadioButton, SIGNAL(toggled(bool)),
                   q, SLOT(onModeChanged()));

  QObject::connect(FilterComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onCurrentFilterChanged(int)));

  QObject::connect(DoubleSpinBoxX, SIGNAL(valueChanged(double)),
                   q, SLOT(onParameterXChanged(double)));

  QObject::connect(DoubleSpinBoxY, SIGNAL(valueChanged(double)),
                   q, SLOT(onParameterYChanged(double)));

  QObject::connect(DoubleSpinBoxZ, SIGNAL(valueChanged(double)),
                   q, SLOT(onParameterZChanged(double)));

  QObject::connect(AccuracySpinBox, SIGNAL(valueChanged(int)),
                   q, SLOT(onAccuracyChanged(int)));

  QObject::connect(ApplyButton, SIGNAL(clicked()),
                   q, SLOT(onApply()));

  QObject::connect(CancelButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));

  progressBar->hide();
  progressBar->setMinimum(0);
  progressBar->setMaximum(100);
  CancelButton->hide();
}

//-----------------------------------------------------------------------------
vtkSlicerSmoothingLogic* qSlicerSmoothingModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerSmoothingModuleWidget);
  return vtkSlicerSmoothingLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerSmoothingModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerSmoothingModuleWidget::qSlicerSmoothingModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerSmoothingModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerSmoothingModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerSmoothingModuleWidget::~qSlicerSmoothingModuleWidget()
{
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerSmoothingModuleWidget);

  this->Superclass::setMRMLScene(scene);
  if (scene == NULL)
    {
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
  if (this->mrmlScene())
    {
    vtkCollection *col = this->mrmlScene()->GetNodesByClass("vtkMRMLSelectionNode");
    unsigned int numNodes = col->GetNumberOfItems();
    for (unsigned int n = 0; n < numNodes; n++)
      {
      vtkMRMLSelectionNode *selectionNodeIter =
          vtkMRMLSelectionNode::SafeDownCast(col->GetItemAsObject(n));
      if (selectionNodeIter)
        {
        // is this the display node?
        if (selectionNode->GetID() && selectionNodeIter->GetID() &&
            strcmp(selectionNode->GetID(), selectionNodeIter->GetID()) == 0)
          {
          // don't disconnect
          // qDebug() << "\tskipping disconnecting " << node->GetID();
          continue;
          }
        this->qvtkDisconnect(selectionNodeIter, vtkCommand::ModifiedEvent,
                           this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));
        }
      }
    col->RemoveAllItems();
    col->Delete();

    this->qvtkConnect(selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));

    }

  this->initializeParameterNode(scene);

  this->onMRMLSmoothingParametersNodeModified();

  this->onOutputVolumeChanged(scene->GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  // observe close event so can re-add a parameters node if necessary
  qvtkReconnect(this->mrmlScene(), vtkMRMLScene::EndCloseEvent,
                this, SLOT(onEndCloseEvent()));

  if (!(this->mrmlScene()->GetNodeByID(selectionNode->GetActiveVolumeID())))
    {
    d->OutputVolumeNodeSelector->setEnabled(false);
    d->ParametersNodeComboBox->setEnabled(false);
    }
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onEndCloseEvent()
{
  this->initializeParameterNode(this->mrmlScene());
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::initializeParameterNode(vtkMRMLScene* scene)
{
  Q_D(qSlicerSmoothingModuleWidget);

  if (!scene)
    {
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
  vtkMRMLSmoothingParametersNode *parametersNode = NULL;
  unsigned int numNodes = scene->
      GetNumberOfNodesByClass("vtkMRMLSmoothingParametersNode");
  if(numNodes > 0)
    {
    parametersNode = vtkMRMLSmoothingParametersNode::
        SafeDownCast(scene->GetNthNodeByClass(0, "vtkMRMLSmoothingParametersNode"));
    parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
    parametersNode->SetOutputVolumeNodeID(selectionNode->GetActiveVolumeID());
    d->ParametersNodeComboBox->setCurrentNode(parametersNode);
    }
  else
    {
    parametersNode = vtkMRMLSmoothingParametersNode::New();
    parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
    parametersNode->SetOutputVolumeNodeID(selectionNode->GetActiveVolumeID());
    scene->AddNode(parametersNode);
    d->ParametersNodeComboBox->setCurrentNode(parametersNode);
    parametersNode->Delete();
    }
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerSmoothingModuleWidget);

  if (!sender)
    {
    return;
    }

  vtkMRMLSelectionNode *selectionNode =
      vtkMRMLSelectionNode::SafeDownCast(sender);

  if (!d->parametersNode)
    {
    return;
    }

  unsigned int numNodes = this->mrmlScene()->
      GetNumberOfNodesByClass("vtkMRMLSmoothingParametersNode");
  if(numNodes == 0)
    {
    vtkMRMLSmoothingParametersNode *parametersNode =
        vtkMRMLSmoothingParametersNode::New();
    this->mrmlScene()->AddNode(parametersNode);
    d->parametersNode = parametersNode;
    parametersNode->Delete();
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
  d->parametersNode->SetOutputVolumeNodeID(selectionNode->GetActiveVolumeID());
  d->parametersNode->EndModify(wasModifying);
}


// --------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::setMRMLSmoothingParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerSmoothingModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLSmoothingParametersNode* smoothingParaNode =
      vtkMRMLSmoothingParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, smoothingParaNode, vtkCommand::ModifiedEvent,
                this, SLOT(onMRMLSmoothingParametersNodeModified()));

  d->parametersNode = smoothingParaNode;

  this->onMRMLSmoothingParametersNodeModified();
  this->setEnabled(smoothingParaNode != 0);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerSmoothingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (mrmlNode)
    {

    d->parametersNode->SetInputVolumeNodeID(mrmlNode->GetID());
    if (this->mrmlScene() &&
        !this->mrmlScene()->IsClosing() &&
        !this->mrmlScene()->IsBatchProcessing())
      {
      // set it to be active in the slice windows
      vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
      vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
      selectionNode->SetReferenceActiveVolumeID(d->parametersNode->GetInputVolumeNodeID());
      appLogic->PropagateVolumeSelection();
      }
    }
  else
    {
    d->parametersNode->SetInputVolumeNodeID(NULL);
    }
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerSmoothingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  if (d->parametersNode)
    {
    if (mrmlNode)
      {
      vtkMRMLScene *scene = this->mrmlScene();

      std::ostringstream outSS;

      vtkMRMLAstroVolumeNode *inputVolume =
        vtkMRMLAstroVolumeNode::SafeDownCast(scene->
          GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));

      vtkMRMLAstroVolumeNode *outputVolume =
        vtkMRMLAstroVolumeNode::SafeDownCast(mrmlNode);

      outSS << inputVolume->GetName() << "-Smoothed";

      // check Output volume
      if (!strcmp(inputVolume->GetID(), outputVolume->GetID()) ||
         (inputVolume->GetAttribute("SlicerAstro.NAXIS1") !=
          outputVolume->GetAttribute("SlicerAstro.NAXIS1")) ||
         (inputVolume->GetAttribute("SlicerAstro.NAXIS2") !=
          outputVolume->GetAttribute("SlicerAstro.NAXIS2")) ||
         (inputVolume->GetAttribute("SlicerAstro.NAXIS3") !=
          outputVolume->GetAttribute("SlicerAstro.NAXIS3")))
        {

        vtkCollection *col = scene->GetNodesByClass("vtkMRMLAstroVolumeNode");
        unsigned int numNodes = col->GetNumberOfItems();
        for (unsigned int n = 0; n < numNodes; n++)
          {
          vtkMRMLAstroVolumeNode *astroVolumeNodeIter =
            vtkMRMLAstroVolumeNode::SafeDownCast(col->GetItemAsObject(n));
          if (astroVolumeNodeIter)
            {
            if (!strcmp(astroVolumeNodeIter->GetName(), outSS.str().c_str()))
              {
              scene->RemoveNode(astroVolumeNodeIter);
              astroVolumeNodeIter->Delete();
              }
            }
          }

        vtkSlicerSmoothingLogic* logic =
          vtkSlicerSmoothingLogic::SafeDownCast(this->logic());
        outputVolume = logic->GetAstroVolumeLogic()->
          CloneVolume(scene, inputVolume, outSS.str().c_str());
        }

      outputVolume->SetName(outSS.str().c_str());

      d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());
      }
    else
      {
      d->parametersNode->SetOutputVolumeNodeID(NULL);
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onMRMLSmoothingParametersNodeModified()
{
  Q_D(qSlicerSmoothingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  char *inputVolumeNodeID = d->parametersNode->GetInputVolumeNodeID();
  vtkMRMLNode *inputVolumeNode = this->mrmlScene()->GetNodeByID(inputVolumeNodeID);
  if (inputVolumeNode)
    {
    d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);
    }

  char *outputVolumeNodeID = d->parametersNode->GetOutputVolumeNodeID();
  vtkMRMLNode *outputVolumeNode = this->mrmlScene()->GetNodeByID(outputVolumeNodeID);
  if (outputVolumeNode)
    {
    d->OutputVolumeNodeSelector->setCurrentNode(outputVolumeNode);
    }

  if (!(strcmp(d->parametersNode->GetMode(), "Automatic")))
    {
    d->AutomaticModeRadioButton->setChecked(true);
    }
  else
    {
    d->ManualModeRadioButton->setChecked(true);
    }

  d->FilterComboBox->setCurrentIndex(d->parametersNode->GetFilter());
  d->SigmaXLabel->show();
  d->SigmaYLabel->show();
  d->SigmaZLabel->show();
  d->DoubleSpinBoxX->show();
  d->DoubleSpinBoxY->show();
  d->DoubleSpinBoxZ->show();
  switch (d->parametersNode->GetFilter())
    {
    case 0:
      {    
      d->SigmaXLabel->setText("SigmaX:");
      d->SigmaYLabel->setText("SigmaY:");
      d->SigmaZLabel->setText("SigmaZ:");
      d->AccuracyLabel->setText("Kernel Accuracy:");
      d->AccuracySpinBox->setMinimum(0);
      d->AccuracySpinBox->setMaximum(5);
      break;
      }
    case 1:
      { 
      d->SigmaXLabel->setText("Cx:");
      d->SigmaYLabel->setText("Cy:");
      d->SigmaZLabel->setText("Cz:");
      d->AccuracySpinBox->setMinimum(0);
      d->AccuracySpinBox->setMaximum(30);
      d->AccuracyLabel->setText("Iterations:");
      break;
      }
    case 2:
      {
      d->SigmaXLabel->hide();
      d->SigmaYLabel->hide();
      d->SigmaZLabel->hide();
      d->DoubleSpinBoxX->hide();
      d->DoubleSpinBoxY->hide();
      d->DoubleSpinBoxZ->hide();
      d->AccuracySpinBox->setMinimum(0);
      d->AccuracySpinBox->setMaximum(3);
      d->AccuracyLabel->setText("Wavelet level:");
      break;
      }
    }
  d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
  d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
  d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
  d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());

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
void qSlicerSmoothingModuleWidget::onModeChanged()
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  if (d->ManualModeRadioButton->isChecked())
    {
    d->parametersNode->SetMode("Manual");
    }
  if (d->AutomaticModeRadioButton->isChecked())
    {
    d->parametersNode->SetMode("Automatic");
    }
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onCurrentFilterChanged(int index)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetFilter(index);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onParameterXChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetParameterX(value);
  d->parametersNode->SetGaussianKernels();
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onParameterYChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetParameterY(value);
  d->parametersNode->SetGaussianKernels();
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onParameterZChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetParameterZ(value);
  d->parametersNode->SetGaussianKernels();
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onAccuracyChanged(int value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetAccuracy(value);
  d->parametersNode->SetGaussianKernels();
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onApply()
{
  Q_D(const qSlicerSmoothingModuleWidget);
  vtkSlicerSmoothingLogic *logic = d->logic();

  if (!d->parametersNode)
    {
    return;
    }

  if (logic->Apply(d->parametersNode))
    {
    vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
    vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();
    selectionNode->SetReferenceActiveVolumeID(d->parametersNode->GetOutputVolumeNodeID());
    appLogic->PropagateVolumeSelection();

    vtkMRMLVolumeNode *outputVolume =
      vtkMRMLVolumeNode::SafeDownCast(appLogic->GetMRMLScene()
        ->GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));
    d->astroVolumeWidget->setMRMLVolumeNode(outputVolume);
    //this once https://github.com/jcfr/Slicer/commit/57854080cde1bb8fb82118dd0fe7366518186f71
    //is approved it can be substitude with:
    //d->astroVolumeWidget->volumeRenderingDisplay()->SetVisibility(true)
    d->astroVolumeWidget->onVisibilityChanged(true);
    }
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onComputationFinished()
{
  Q_D(qSlicerSmoothingModuleWidget);

  d->CancelButton->hide();
  d->progressBar->hide();
  d->ApplyButton->show();
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerSmoothingModuleWidget);

  d->parametersNode->SetStatus(-1);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  d->progressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onComputationStarted()
{
  Q_D(qSlicerSmoothingModuleWidget);
  d->ApplyButton->hide();
  d->progressBar->show();
  d->CancelButton->show();
}

//---------------------------------------------------------------------------
vtkMRMLSmoothingParametersNode* qSlicerSmoothingModuleWidget::
mrmlSmoothingParametersNode()const
{
  Q_D(const qSlicerSmoothingModuleWidget);
  return d->parametersNode;
}

