// Qt includes
#include <QDebug>
#include <QMessageBox>

// CTK includes
#include <ctkFlowLayout.h>

// VTK includes
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkRenderer.h>
#include <vtkParametricEllipsoid.h>
#include <vtkParametricFunctionSource.h>
#include <vtkMatrixToLinearTransform.h>
#include <vtkCamera.h>
#include <vtkProperty.h>
#include <vtkMatrix4x4.h>
#include <vtkImageData.h>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>

// Smoothing includes
#include "qSlicerSmoothingModuleWidget.h"
#include "ui_qSlicerSmoothingModuleWidget.h"

// Smoothing Logic includes
#include <vtkSlicerSmoothingLogic.h>

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>

// qMRML includes
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>
#include <qSlicerApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include "qSlicerVolumeRenderingModuleWidget.h"

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLSmoothingParametersNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>
#include <vtkMRMLCameraNode.h>

#define SigmatoFWHM 2.3548200450309493

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
  vtkSmartPointer<vtkParametricEllipsoid> parametricVTKEllipsoid;
  vtkSmartPointer<vtkParametricFunctionSource> parametricFunctionSource;
  vtkSmartPointer<vtkMatrix4x4> transformationMatrix;
  vtkSmartPointer<vtkMatrixToLinearTransform> matrixToLinearTransform;
  vtkSmartPointer<vtkPolyDataMapper> mapper;
  vtkSmartPointer<vtkActor> actor;
  double DegToRad;

};

//-----------------------------------------------------------------------------
// qSlicerSmoothingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerSmoothingModuleWidgetPrivate::qSlicerSmoothingModuleWidgetPrivate(qSlicerSmoothingModuleWidget& object)
  : q_ptr(&object)
{
  this->parametersNode = 0;
  this->astroVolumeWidget = 0;
  this->parametricVTKEllipsoid = vtkSmartPointer<vtkParametricEllipsoid>::New();
  this->parametricFunctionSource = vtkSmartPointer<vtkParametricFunctionSource>::New();
  this->transformationMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  this->transformationMatrix->Identity();
  this->matrixToLinearTransform = vtkSmartPointer<vtkMatrixToLinearTransform>::New();
  this->mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->actor = vtkSmartPointer<vtkActor>::New();
  this->DegToRad = atan(1.) / 45.;
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

  QObject::connect(AccuracySpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onAccuracyChanged(double)));

  QObject::connect(KSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onKChanged(double)));

  QObject::connect(TimeStepSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onTimeStepChanged(double)));

  QObject::connect(RxSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRxChanged(double)));

  QObject::connect(RySpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRyChanged(double)));

  QObject::connect(RzSpinBox, SIGNAL(valueChanged(double)),
                   q, SLOT(onRzChanged(double)));

  QObject::connect(ApplyButton, SIGNAL(clicked()),
                   q, SLOT(onApply()));

  QObject::connect(CancelButton, SIGNAL(clicked()),
                   q, SLOT(onComputationCancelled()));

  QObject::connect(HardwareComboBox, SIGNAL(currentIndexChanged(int)),
                   q, SLOT(onHardwareChanged(int)));

  QObject::connect(LinkCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onLinkChanged(bool)));


  progressBar->hide();
  progressBar->setMinimum(0);
  progressBar->setMaximum(100);
  CancelButton->hide();
  KLabel->hide();
  KSpinBox->hide();
  TimeStepLabel->hide();
  TimeStepSpinBox->hide();
  vtkCamera* camera = GaussianKernelView->activeCamera();
  double eyePosition[3];
  eyePosition[0] = 0.;
  eyePosition[1] = 0.;
  eyePosition[2] = 30;
  camera->SetPosition(eyePosition);

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
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}

} // end namespace

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

  if (d->parametersNode)
    {
    if (mrmlNode)
      {
      d->parametersNode->SetOutputVolumeNodeID(mrmlNode->GetID());
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
  vtkMRMLAstroVolumeNode *inputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(inputVolumeNodeID));
  if (inputVolumeNode)
    {
    d->InputVolumeNodeSelector->setCurrentNode(inputVolumeNode);
    }

  char *outputVolumeNodeID = d->parametersNode->GetOutputVolumeNodeID();
  vtkMRMLAstroVolumeNode *outputVolumeNode = vtkMRMLAstroVolumeNode::SafeDownCast
      (this->mrmlScene()->GetNodeByID(outputVolumeNodeID));
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
  d->HardwareComboBox->setCurrentIndex(d->parametersNode->GetHardware());

  int status = d->parametersNode->GetStatus();

  if(status == 0)
    {
    switch (d->parametersNode->GetFilter())
      {
      case 0:
        {
        d->AccuracyLabel->hide();
        d->AccuracySpinBox->hide();
        d->AccuracyValueLabel->hide();
        d->HardwareLabel->show();
        d->HardwareComboBox->show();
        d->KLabel->hide();
        d->KSpinBox->hide();
        d->TimeStepLabel->hide();
        d->TimeStepSpinBox->hide();
        d->GaussianKernelView->hide();
        d->RxLabel->hide();
        d->RxSpinBox->hide();
        d->RyLabel->hide();
        d->RySpinBox->hide();
        d->RzLabel->hide();
        d->RzSpinBox->hide();
        d->LinkCheckBox->show();
        d->LinkLabel->show();
        d->LinkCheckBox->setToolTip("Click to link / unlink the parameters N<sub>X</sub>"
                                    ", N<sub>Y</sub> and N<sub>Z</sub>");
        d->LinkCheckBox->setChecked(d->parametersNode->GetLink());
        d->CDELT1Label->show();
        d->CDELT1LabelValue->show();
        d->CDELT2Label->show();
        d->CDELT2LabelValue->show();
        d->CDELT3Label->show();
        d->CDELT3LabelValue->show();
        d->SigmaYLabel->show();
        d->DoubleSpinBoxY->show();
        d->SigmaZLabel->show();
        d->DoubleSpinBoxZ->show();
        double cdelt1 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT1"));
        d->CDELT1LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueX(cdelt1));
        double cdelt2 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT2"));
        d->CDELT2LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueY(cdelt2));
        double cdelt3 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT3"));
        d->CDELT3LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueZ(cdelt3));
        d->SigmaXLabel->setText("N<sub>X</sub>:");
        d->SigmaYLabel->setText("N<sub>Y</sub>:");
        d->SigmaZLabel->setText("N<sub>Z</sub>:");
        d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
        d->DoubleSpinBoxX->setToolTip("Number of pixel of the Box kernel in the X direction");
        d->DoubleSpinBoxX->setSingleStep(2);
        d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
        d->DoubleSpinBoxY->setToolTip("Number of pixel of the Box kernel in the Y direction");
        d->DoubleSpinBoxY->setSingleStep(2);
        d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
        d->DoubleSpinBoxZ->setToolTip("Number of pixel of the Box kernel in the Z direction");
        d->DoubleSpinBoxZ->setSingleStep(2);
        break;
        }
      case 1:
        {
        d->AccuracyLabel->show();
        d->AccuracySpinBox->show();
        d->AccuracyValueLabel->show();
        d->HardwareLabel->show();
        d->HardwareComboBox->show();
        d->KLabel->hide();
        d->KSpinBox->hide();
        d->TimeStepLabel->hide();
        d->TimeStepSpinBox->hide();
        d->LinkCheckBox->show();
        d->LinkLabel->show();
        d->LinkCheckBox->setToolTip("Click to link / unlink the parameters"
                                    " FWHM<sub>X</sub>, FWHM<sub>Y</sub> and FWHM<sub>Z</sub>");
        d->LinkCheckBox->setChecked(d->parametersNode->GetLink());
        d->CDELT1Label->show();
        d->CDELT1LabelValue->show();
        d->CDELT2Label->show();
        d->CDELT2LabelValue->show();
        d->CDELT3Label->show();
        d->CDELT3LabelValue->show();
        d->SigmaYLabel->show();
        d->DoubleSpinBoxY->show();
        d->SigmaZLabel->show();
        d->DoubleSpinBoxZ->show();
        double cdelt1 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT1"));
        d->CDELT1LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueX(cdelt1));
        double cdelt2 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT2"));
        d->CDELT2LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueY(cdelt2));
        double cdelt3 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT3"));
        d->CDELT3LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueZ(cdelt3));
        d->SigmaXLabel->setText("FWHM<sub>X</sub>:");
        d->SigmaYLabel->setText("FWHM<sub>Y</sub>:");
        d->SigmaZLabel->setText("FWHM<sub>Z</sub>:");
        d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
        d->DoubleSpinBoxX->setToolTip("Full width at half maximum in pixel in the X direction");
        d->DoubleSpinBoxX->setSingleStep(1);
        d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
        d->DoubleSpinBoxY->setToolTip("Full width at half maximum in pixel in the Y direction");
        d->DoubleSpinBoxY->setSingleStep(1);
        d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
        d->DoubleSpinBoxZ->setToolTip("Full width at half maximum in pixel in the Z direction");
        d->DoubleSpinBoxZ->setSingleStep(1);
        d->AccuracyLabel->setText("Kernel Accuracy:");
        d->AccuracySpinBox->setSingleStep(1);
        d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());
        d->AccuracySpinBox->setMaximum(5);
        d->AccuracySpinBox->setToolTip("Set the accuracy of the Gaussian Kernel in sigma units");
        switch (d->parametersNode->GetAccuracy())
          {
          case 1:
            {
            d->AccuracyValueLabel->setText("       68.27%");
            break;
            }
          case 2:
            {
            d->AccuracyValueLabel->setText("       95.45%");
            break;
            }
          case 3:
            {
            d->AccuracyValueLabel->setText("       99.73%");
            break;
            }
          case 4:
            {
            d->AccuracyValueLabel->setText("      99.994%");
            break;
            }
          case 5:
            {
            d->AccuracyValueLabel->setText("     99.99994%");
            break;
            }
          }


        if (fabs(d->parametersNode->GetParameterX() - d->parametersNode->GetParameterY()) < 0.001 &&
           fabs(d->parametersNode->GetParameterY() - d->parametersNode->GetParameterZ()) < 0.001)
          {
          d->GaussianKernelView->hide();
          d->RxLabel->hide();
          d->RxSpinBox->hide();
          d->RyLabel->hide();
          d->RySpinBox->hide();
          d->RzLabel->hide();
          d->RzSpinBox->hide();
          }
        else
          {
          d->GaussianKernelView->show();
          d->RxLabel->show();
          d->RxSpinBox->show();
          d->RyLabel->show();
          d->RySpinBox->show();
          d->RzLabel->show();
          d->RzSpinBox->show();

          double Rx = d->parametersNode->GetRx();
          double Ry = d->parametersNode->GetRy();
          double Rz = d->parametersNode->GetRz();

          d->RxSpinBox->setValue(Rx);
          d->RySpinBox->setValue(Ry);
          d->RzSpinBox->setValue(Rz);

          d->GaussianKernelView->show();
          vtkRenderer* renderer = d->GaussianKernelView->renderer();

          vtkActorCollection* col = renderer->GetActors();
          col->InitTraversal();
          for (int i = 0; i < col->GetNumberOfItems(); i++)
            {
            renderer->RemoveActor(col->GetNextActor());
            }

          //Definition and transformation of the vtkEllipsoid
          double RadiusX = d->parametersNode->GetParameterX() / SigmatoFWHM;
          if(RadiusX < 0.01)
            {
            RadiusX = 0.01;
            }
          double RadiusY = d->parametersNode->GetParameterY() / SigmatoFWHM;
          if(RadiusY < 0.01)
            {
            RadiusY = 0.01;
            }
          double RadiusZ = d->parametersNode->GetParameterZ() / SigmatoFWHM;
          if(RadiusZ < 0.01)
            {
            RadiusZ = 0.01;
            }
          d->parametricVTKEllipsoid->SetXRadius(RadiusX);
          d->parametricVTKEllipsoid->SetYRadius(RadiusY);
          d->parametricVTKEllipsoid->SetZRadius(RadiusZ);
          d->parametricFunctionSource->SetParametricFunction(d->parametricVTKEllipsoid);
          d->parametricFunctionSource->Update();

          //Configuration of the rotation

          Rx *= -1;
          Ry *= -1;
          Rz *= -1;

          Rx *= d->DegToRad;
          Ry *= d->DegToRad;
          Rz *= d->DegToRad;
          d->transformationMatrix->Identity();
          double cx = cos(Rx);
          double sx = sin(Rx);
          double cy = cos(Ry);
          double sy = sin(Ry);
          double cz = cos(Rz);
          double sz = sin(Rz);
          d->transformationMatrix->SetElement(0, 0, cy * cz);
          d->transformationMatrix->SetElement(0, 1, -cy * sz);
          d->transformationMatrix->SetElement(0, 2, sy);
          d->transformationMatrix->SetElement(1, 0, cz * sx * sy + cx * sz);
          d->transformationMatrix->SetElement(1, 1, cx * cz - sx * sy * sz);
          d->transformationMatrix->SetElement(1, 2, -cy * sx);
          d->transformationMatrix->SetElement(2, 0, -cx * cz * sy + sx * sz);
          d->transformationMatrix->SetElement(2, 1, cz * sx + cx * sy * sz);
          d->transformationMatrix->SetElement(2, 2, cx * cy);

          d->matrixToLinearTransform->SetInput(d->transformationMatrix);
          d->matrixToLinearTransform->Update();

          d->mapper->SetInputConnection(d->parametricFunctionSource->GetOutputPort());
          d->actor->SetMapper(d->mapper);
          d->actor->GetProperty()->SetColor(0.0, 1.0, 1.0);
          d->actor->SetUserTransform(d->matrixToLinearTransform);

          renderer->AddActor(d->actor);
          renderer->SetBackground(0., 0., 0.);

          vtkCamera* camera = d->GaussianKernelView->activeCamera();

          vtkCollection *coll = this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode");
          vtkMRMLCameraNode *cameraNodeOne =
            vtkMRMLCameraNode::SafeDownCast(coll->GetItemAsObject(0));
          if (cameraNodeOne)
            {
            double Origin[3];
            cameraNodeOne->GetPosition(Origin);
            Origin[0] /= 15.;
            Origin[1] /= 15.;
            Origin[2] /= 15.;
            //Ry(90)
            double temp = Origin[1];
            Origin[1] = -Origin[2];
            Origin[2] = temp;
            //Rz(-180)
            Origin[0] *= -1;
            Origin[1] *= -1;
            camera->SetPosition(Origin);
            cameraNodeOne->GetFocalPoint(Origin);
            camera->SetFocalPoint(Origin);
            cameraNodeOne->GetViewUp(Origin);
            //Ry(90)
            temp = Origin[1];
            Origin[1] = -Origin[2];
            Origin[2] = temp;
            //Rz(-180)
            Origin[0] *= -1;
            Origin[1] *= -1;
            camera->SetViewUp(Origin);
            }
          coll->RemoveAllItems();
          coll->Delete();
          }
        break;
        }
      case 2:
        {
        d->AccuracyLabel->show();
        d->AccuracySpinBox->show();
        d->AccuracyValueLabel->hide();
        d->HardwareLabel->show();
        d->HardwareComboBox->show();
        d->GaussianKernelView->hide();
        d->RxLabel->hide();
        d->RxSpinBox->hide();
        d->RyLabel->hide();
        d->RySpinBox->hide();
        d->RzLabel->hide();
        d->RzSpinBox->hide();
        d->CDELT1Label->hide();
        d->CDELT1LabelValue->hide();
        d->CDELT2Label->hide();
        d->CDELT2LabelValue->hide();
        d->CDELT3Label->hide();
        d->CDELT3LabelValue->hide();

        d->LinkCheckBox->show();
        d->LinkLabel->show();
        d->LinkCheckBox->setToolTip("Click to link / unlink the conductivity parameters");
        d->LinkCheckBox->setChecked(d->parametersNode->GetLink());
        d->KLabel->show();
        d->KSpinBox->show();
        d->SigmaYLabel->show();
        d->DoubleSpinBoxY->show();
        d->SigmaZLabel->show();
        d->DoubleSpinBoxZ->show();

        d->KSpinBox->setValue(d->parametersNode->GetK());
        d->TimeStepLabel->show();
        d->TimeStepSpinBox->show();
        d->SigmaXLabel->setText("Horizontal Conduntance:");
        d->SigmaYLabel->setText("Vertical Conduntance:");
        d->SigmaZLabel->setText("Depth Conduntance:");
        d->DoubleSpinBoxX->setToolTip("");
        d->DoubleSpinBoxY->setToolTip("");
        d->DoubleSpinBoxZ->setToolTip("");
        d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
        d->DoubleSpinBoxX->setSingleStep(1);
        d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
        d->DoubleSpinBoxY->setSingleStep(1);
        d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
        d->DoubleSpinBoxZ->setSingleStep(1);
        d->AccuracyLabel->setText("Iterations:");
        d->AccuracySpinBox->setMaximum(30);
        d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());
        d->AccuracySpinBox->setToolTip("");
        if (d->parametersNode->GetHardware())
          {
          d->AccuracySpinBox->setSingleStep(2);
          d->TimeStepSpinBox->setMaximum(0.625);
          d->TimeStepSpinBox->setValue(d->parametersNode->GetTimeStep());
          }
        else
          {
          d->AccuracySpinBox->setSingleStep(1);
          d->TimeStepSpinBox->setValue(d->parametersNode->GetTimeStep());
          d->TimeStepSpinBox->setMaximum(0.0625);
          }
        break;
        }
      case 3:
        {
        d->AccuracyLabel->show();
        d->AccuracySpinBox->show();
        d->AccuracyValueLabel->hide();
        d->HardwareLabel->hide();
        d->HardwareComboBox->hide();
        d->SigmaYLabel->hide();
        d->DoubleSpinBoxY->hide();
        d->SigmaZLabel->hide();
        d->DoubleSpinBoxZ->hide();
        d->LinkCheckBox->hide();
        d->LinkLabel->hide();
        d->CDELT1Label->hide();
        d->CDELT1LabelValue->hide();
        d->CDELT2Label->hide();
        d->CDELT2LabelValue->hide();
        d->CDELT3Label->hide();
        d->CDELT3LabelValue->hide();
        d->GaussianKernelView->hide();
        d->RxLabel->hide();
        d->RxSpinBox->hide();
        d->RyLabel->hide();
        d->RySpinBox->hide();
        d->RzLabel->hide();
        d->RzSpinBox->hide();
        d->KLabel->hide();
        d->KSpinBox->hide();
        d->TimeStepLabel->hide();
        d->TimeStepSpinBox->hide();
        d->SigmaXLabel->setText("Threshold level:");
        d->DoubleSpinBoxX->setToolTip("");
        d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
        d->DoubleSpinBoxX->setSingleStep(1);
        d->AccuracyLabel->setText("Wavelet level:");
        d->AccuracySpinBox->setSingleStep(1);
        d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());
        d->AccuracySpinBox->setMaximum(5);
        d->AccuracySpinBox->setToolTip("");
        break;
        }
      case 4:
        {
        d->AccuracyLabel->show();
        d->AccuracySpinBox->show();
        d->AccuracyValueLabel->hide();
        d->HardwareLabel->hide();
        d->HardwareComboBox->hide();
        d->SigmaYLabel->hide();
        d->DoubleSpinBoxY->hide();
        d->SigmaZLabel->hide();
        d->DoubleSpinBoxZ->hide();
        d->LinkCheckBox->hide();
        d->LinkLabel->hide();
        d->CDELT1Label->hide();
        d->CDELT1LabelValue->hide();
        d->CDELT2Label->hide();
        d->CDELT2LabelValue->hide();
        d->CDELT3Label->hide();
        d->CDELT3LabelValue->hide();
        d->GaussianKernelView->hide();
        d->RxLabel->hide();
        d->RxSpinBox->hide();
        d->RyLabel->hide();
        d->RySpinBox->hide();
        d->RzLabel->hide();
        d->RzSpinBox->hide();
        d->KLabel->hide();
        d->KSpinBox->hide();
        d->TimeStepLabel->hide();
        d->TimeStepSpinBox->hide();
        d->SigmaXLabel->setText("Threshold level:");
        d->DoubleSpinBoxX->setToolTip("");
        d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
        d->DoubleSpinBoxX->setSingleStep(1);
        d->AccuracyLabel->setText("Wavelet level:");
        d->AccuracySpinBox->setSingleStep(1);
        d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());
        d->AccuracySpinBox->setMaximum(5);
        d->AccuracySpinBox->setToolTip("");
        break;
        }
      }
    }

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

  int wasModifying = d->parametersNode->StartModify();

  if (d->ManualModeRadioButton->isChecked())
    {
    d->parametersNode->SetMode("Manual");
    }
  if (d->AutomaticModeRadioButton->isChecked())
    {
    d->parametersNode->SetMode("Automatic");
    d->parametersNode->SetHardware(0);
    d->parametersNode->SetFilter(2);
    d->parametersNode->SetAccuracy(20);
    d->parametersNode->SetTimeStep(0.0325);
    d->parametersNode->SetK(1.5);
    d->parametersNode->SetParameterX(5);
    d->parametersNode->SetParameterY(5);
    d->parametersNode->SetParameterZ(5);
    }

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onCurrentFilterChanged(int index)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();

  if (index == 0)
    {
    d->parametersNode->SetParameterX(4);
    d->parametersNode->SetParameterY(4);
    d->parametersNode->SetParameterZ(4);
    d->parametersNode->SetKernelLengthX(4);
    d->parametersNode->SetKernelLengthY(4);
    d->parametersNode->SetKernelLengthZ(4);
    }

  if (index == 1)
    {
    d->parametersNode->SetAccuracy(3);
    d->parametersNode->SetParameterX(3);
    d->parametersNode->SetParameterY(3);
    d->parametersNode->SetParameterZ(3);
    d->parametersNode->SetRx(0);
    d->parametersNode->SetRy(0);
    d->parametersNode->SetRz(0);
    d->parametersNode->SetGaussianKernels();
    }

  if (index == 2)
    {
    if (d->parametersNode->GetHardware())
      {
      d->parametersNode->SetAccuracy(19);
      d->parametersNode->SetTimeStep(0.325);
      }
    else
      {
      d->parametersNode->SetTimeStep(0.0325);
      d->parametersNode->SetAccuracy(20);
      }
    d->parametersNode->SetK(1.5);
    d->parametersNode->SetParameterX(5);
    d->parametersNode->SetParameterY(5);
    d->parametersNode->SetParameterZ(5);
    }

  if (index == 3)
    {
    d->parametersNode->SetHardware(0);
    d->parametersNode->SetAccuracy(2);
    d->parametersNode->SetParameterX(1.5);
    }

  if (index == 4)
    {
    d->parametersNode->SetHardware(0);
    d->parametersNode->SetAccuracy(2);
    d->parametersNode->SetParameterX(4);
    }

  d->parametersNode->SetFilter(index);

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onKChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetK(value);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onTimeStepChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetTimeStep(value);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onRxChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRx(value);
  d->parametersNode->SetGaussianKernels();
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onRyChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRy(value);
  d->parametersNode->SetGaussianKernels();
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onRzChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRz(value);
  d->parametersNode->SetGaussianKernels();
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onParameterXChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetParameterX(value);
  if (d->parametersNode->GetLink())
    {
    d->parametersNode->SetParameterY(value);
    d->parametersNode->SetParameterZ(value);
    }
  if (d->parametersNode->GetFilter() == 0)
    {
    d->parametersNode->SetKernelLengthX(value);
    }
  d->parametersNode->SetGaussianKernels();
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onParameterYChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetParameterY(value);
  if (d->parametersNode->GetLink())
    {
    d->parametersNode->SetParameterX(value);
    d->parametersNode->SetParameterZ(value);
    }
  if (d->parametersNode->GetFilter() == 0)
    {
    d->parametersNode->SetKernelLengthY(value);
    }
  d->parametersNode->SetGaussianKernels();
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onParameterZChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetParameterZ(value);
  if (d->parametersNode->GetLink())
    {
    d->parametersNode->SetParameterX(value);
    d->parametersNode->SetParameterY(value);
    }
  if (d->parametersNode->GetFilter() == 0)
    {
    d->parametersNode->SetKernelLengthZ(value);
    }
  d->parametersNode->SetGaussianKernels();
  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onAccuracyChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetAccuracy(value);
  d->parametersNode->SetGaussianKernels();
  d->parametersNode->EndModify(wasModifying);
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

  vtkMRMLScene *scene = this->mrmlScene();

  d->astroVolumeWidget->stopRockView();

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));

  int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  // Check Input volume
  if (n != 3)
    {
    qCritical() << "filtering techniques are available only" <<
                  "for datacube with dimensionality 3 (NAXIS = 3).";
    return;
    }

  inputVolume->SetDisplayVisibility(0);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_Filtered_" <<
    d->parametersNode->GetMode() << "_";

  switch (d->parametersNode->GetFilter())
    {
    case 0:
      {
      outSS<<"Box";
      break;
      }
    case 1:
      {
      outSS<<"Gaussian";
      break;
      }
    case 2:
      {
      outSS<<"Gradient";
      break;
      }
    case 3:
      {
      outSS<<"Haar_Wavelet_Thresholding";
      break;
      }
    case 4:
      {
      outSS<<"Gall_Wavelet_Thresholding";
      break;
      }
    }

  int serial = d->parametersNode->GetOutputSerial();
  outSS<<"_"<< IntToString(serial);
  serial++;
  d->parametersNode->SetOutputSerial(serial);

  // check Output volume
  if (!strcmp(inputVolume->GetID(), outputVolume->GetID()) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) !=
      StringToInt(outputVolume->GetAttribute("SlicerAstro.NAXIS1"))) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) !=
      StringToInt(outputVolume->GetAttribute("SlicerAstro.NAXIS2"))) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3")) !=
      StringToInt(outputVolume->GetAttribute("SlicerAstro.NAXIS3"))))
    {

    vtkSlicerSmoothingLogic* logic =
      vtkSlicerSmoothingLogic::SafeDownCast(this->logic());
   outputVolume = logic->GetAstroVolumeLogic()->
      CloneVolume(scene, inputVolume, outSS.str().c_str());

    outputVolume->SetName(outSS.str().c_str());
    d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());

    int ndnodes = outputVolume->GetNumberOfDisplayNodes();
    for (int i=0; i<ndnodes; i++)
      {
      vtkMRMLVolumeRenderingDisplayNode *dnode =
        vtkMRMLVolumeRenderingDisplayNode::SafeDownCast(
          outputVolume->GetNthDisplayNode(i));
      if (dnode)
        {
        outputVolume->RemoveNthDisplayNodeID(i);
        }
      } 
    }
  else
    {
    outputVolume->SetName(outSS.str().c_str());
    d->parametersNode->SetOutputVolumeNodeID(outputVolume->GetID());
    }

  d->transformationMatrix->Identity();
  inputVolume->GetRASToIJKMatrix(d->transformationMatrix);
  outputVolume->SetRASToIJKMatrix(d->transformationMatrix);
  outputVolume->SetAndObserveTransformNodeID(inputVolume->GetTransformNodeID());
  d->transformationMatrix->Identity();

  if (logic->Apply(d->parametersNode))
    {
    vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
    vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();

    selectionNode->SetReferenceActiveVolumeID(d->parametersNode->GetOutputVolumeNodeID());
    selectionNode->SetReferenceSecondaryVolumeID(inputVolume->GetID());

    appLogic->PropagateVolumeSelection();
    //when SliceRT will release the segmentation
    //let's see to make overlayed contours also the 2-D.
    //The editor now can do only a threshold of one volume.
    //In pricinple we can also create an editor effect specific for this.

    d->astroVolumeWidget->setComparative3DViews
        (inputVolume->GetID(), d->parametersNode->GetOutputVolumeNodeID());
    }
  else
    {
    scene->RemoveNode(outputVolume);
    inputVolume->SetDisplayVisibility(1);
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
void qSlicerSmoothingModuleWidget::onHardwareChanged(int index)
{
 Q_D(qSlicerSmoothingModuleWidget);

 int wasModifying = d->parametersNode->StartModify();

 d->parametersNode->SetHardware(index);
 int filter = d->parametersNode->GetFilter();

 if (filter == 2)
   {
   if (index)
     {
     d->parametersNode->SetTimeStep(0.325);
     d->parametersNode->SetAccuracy(19);
     }
   else
     {
     d->parametersNode->SetTimeStep(0.0325);
     d->parametersNode->SetAccuracy(20);
     }
   }
 d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onLinkChanged(bool index)
{
 Q_D(qSlicerSmoothingModuleWidget);
 d->parametersNode->SetLink(index);
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

