// Qt includes
#include <QDebug>
#include <QMessageBox>
#include <QTimer>

// CTK includes
#include <ctkFlowLayout.h>

// VTK includes
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkMatrixToLinearTransform.h>
#include <vtkNew.h>
#include <vtkParametricEllipsoid.h>
#include <vtkParametricFunctionSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtksys/SystemTools.hxx>

// SlicerQt includes
#include <qSlicerAbstractCoreModule.h>

// AstroSmoothing includes
#include "qSlicerAstroSmoothingModuleWidget.h"
#include "ui_qSlicerAstroSmoothingModuleWidget.h"

// Logic includes
#include <vtkSlicerAstroVolumeLogic.h>
#include <vtkSlicerAstroSmoothingLogic.h>

// qMRML includes
#include <qSlicerAbstractCoreModule.h>
#include <qSlicerApplication.h>
#include <qSlicerAstroVolumeModuleWidget.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerUtils.h>
#include "qSlicerVolumeRenderingModuleWidget.h"

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// MRML includes
#include <vtkMRMLAstroSmoothingParametersNode.h>
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLCameraNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLVolumeRenderingDisplayNode.h>

#define SigmatoFWHM 2.3548200450309493

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_AstroSmoothing
class qSlicerAstroSmoothingModuleWidgetPrivate: public Ui_qSlicerAstroSmoothingModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerAstroSmoothingModuleWidget);
protected:
  qSlicerAstroSmoothingModuleWidget* const q_ptr;
public:

  qSlicerAstroSmoothingModuleWidgetPrivate(qSlicerAstroSmoothingModuleWidget& object);
  ~qSlicerAstroSmoothingModuleWidgetPrivate();
  void init();

  vtkSlicerAstroSmoothingLogic* logic() const;
  qSlicerAstroVolumeModuleWidget* astroVolumeWidget;
  vtkSmartPointer<vtkMRMLAstroSmoothingParametersNode> parametersNode;
  vtkSmartPointer<vtkMRMLSelectionNode> selectionNode;
  vtkSmartPointer<vtkParametricEllipsoid> parametricVTKEllipsoid;
  vtkSmartPointer<vtkParametricFunctionSource> parametricFunctionSource;
  vtkSmartPointer<vtkMatrix4x4> transformationMatrix;
  vtkSmartPointer<vtkMatrixToLinearTransform> matrixToLinearTransform;
  vtkSmartPointer<vtkPolyDataMapper> mapper;
  vtkSmartPointer<vtkActor> actor;
  double DegToRad;

};

//-----------------------------------------------------------------------------
// qSlicerAstroSmoothingModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModuleWidgetPrivate::qSlicerAstroSmoothingModuleWidgetPrivate(qSlicerAstroSmoothingModuleWidget& object)
  : q_ptr(&object)
{
  this->astroVolumeWidget = 0;
  this->parametersNode = 0;
  this->selectionNode = 0;
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
qSlicerAstroSmoothingModuleWidgetPrivate::~qSlicerAstroSmoothingModuleWidgetPrivate()
{
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidgetPrivate::init()
{
  Q_Q(qSlicerAstroSmoothingModuleWidget);
  this->setupUi(q);
  qSlicerApplication* app = qSlicerApplication::application();
      qSlicerAbstractCoreModule* astroVolume = app->moduleManager()->module("AstroVolume");
  if (!astroVolume)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidgetPrivate::init(): could not find AstroVolume module.";
    return;
    }

  this->astroVolumeWidget = dynamic_cast<qSlicerAstroVolumeModuleWidget*>
    (astroVolume->widgetRepresentation());

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   ParametersNodeComboBox, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   InputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)),
                   OutputVolumeNodeSelector, SLOT(setMRMLScene(vtkMRMLScene*)));

  QObject::connect(ParametersNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
                   q, SLOT(setMRMLAstroSmoothingParametersNode(vtkMRMLNode*)));

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

  QObject::connect(AutoRunCheckBox, SIGNAL(toggled(bool)),
                   q, SLOT(onAutoRunChanged(bool)));


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
vtkSlicerAstroSmoothingLogic* qSlicerAstroSmoothingModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerAstroSmoothingModuleWidget);
  return vtkSlicerAstroSmoothingLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
// qSlicerAstroSmoothingModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModuleWidget::qSlicerAstroSmoothingModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerAstroSmoothingModuleWidgetPrivate(*this) )
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerAstroSmoothingModuleWidget::~qSlicerAstroSmoothingModuleWidget()
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
void qSlicerAstroSmoothingModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  this->Superclass::setMRMLScene(scene);
  if (scene == NULL)
    {
    return;
    }

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  if (!appLogic)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::setMRMLScene : appLogic not found.";
    return;
    }
  d->selectionNode = appLogic->GetSelectionNode();
  if (!d->selectionNode)
    {
    qCritical() << "qSlicerAstroSmoothingModuleWidget::setMRMLScene : selectionNode not found.";
    return;
    }

  this->qvtkReconnect(d->selectionNode, vtkCommand::ModifiedEvent,
                      this, SLOT(onMRMLSelectionNodeModified(vtkObject*)));

  this->initializeParameterNode(scene);
  this->onMRMLAstroSmoothingParametersNodeModified();
  this->onOutputVolumeChanged(scene->GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  // observe close event so can re-add a parameters node if necessary
  qvtkReconnect(this->mrmlScene(), vtkMRMLScene::EndCloseEvent,
                this, SLOT(onEndCloseEvent()));

  if (!(this->mrmlScene()->GetNodeByID(d->selectionNode->GetActiveVolumeID())))
    {
    d->OutputVolumeNodeSelector->setEnabled(false);
    d->ParametersNodeComboBox->setEnabled(false);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onEndCloseEvent()
{
  this->initializeParameterNode(this->mrmlScene());
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::initializeParameterNode(vtkMRMLScene* scene)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!scene)
    {
    return;
    }

  vtkSmartPointer<vtkMRMLNode> parametersNode;
  unsigned int numNodes = scene->
      GetNumberOfNodesByClass("vtkMRMLAstroSmoothingParametersNode");
  if(numNodes > 0)
    {
    parametersNode = scene->GetNthNodeByClass(0, "vtkMRMLAstroSmoothingParametersNode");
    }
  else
    {
    vtkMRMLNode * foo = scene->CreateNodeByClass("vtkMRMLAstroSmoothingParametersNode");
    parametersNode.TakeReference(foo);
    scene->AddNode(parametersNode);
    }
  vtkMRMLAstroSmoothingParametersNode *astroParametersNode =
    vtkMRMLAstroSmoothingParametersNode::SafeDownCast(parametersNode);
  astroParametersNode->SetInputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
  astroParametersNode->SetOutputVolumeNodeID(d->selectionNode->GetActiveVolumeID());
  d->ParametersNodeComboBox->setCurrentNode(astroParametersNode);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onMRMLSelectionNodeModified(vtkObject* sender)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

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
      GetNumberOfNodesByClass("vtkMRMLAstroSmoothingParametersNode");
  if(numNodes == 0)
    {
    this->initializeParameterNode(selectionNode->GetScene());
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetInputVolumeNodeID(selectionNode->GetActiveVolumeID());
  d->parametersNode->SetOutputVolumeNodeID(selectionNode->GetActiveVolumeID());
  d->parametersNode->EndModify(wasModifying);
}


// --------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::setMRMLAstroSmoothingParametersNode(vtkMRMLNode* mrmlNode)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!mrmlNode)
    {
    return;
    }

  vtkMRMLAstroSmoothingParametersNode* AstroSmoothingParaNode =
      vtkMRMLAstroSmoothingParametersNode::SafeDownCast(mrmlNode);

  this->qvtkReconnect(d->parametersNode, AstroSmoothingParaNode, vtkCommand::ModifiedEvent,
                this, SLOT(onMRMLAstroSmoothingParametersNodeModified()));

  d->parametersNode = AstroSmoothingParaNode;

  this->onMRMLAstroSmoothingParametersNodeModified();
  this->setEnabled(AstroSmoothingParaNode != 0);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onInputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

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
void qSlicerAstroSmoothingModuleWidget::onOutputVolumeChanged(vtkMRMLNode *mrmlNode)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

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
void qSlicerAstroSmoothingModuleWidget::onMRMLAstroSmoothingParametersNodeModified()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);

  if (!d->parametersNode)
    {
    return;
    }

  int status = d->parametersNode->GetStatus();

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

  d->AutoRunCheckBox->setChecked(d->parametersNode->GetAutoRun());
  d->LinkCheckBox->setChecked(d->parametersNode->GetLink());

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
        d->LinkCheckBox->setToolTip("Click to link / unlink the parameters N<sub>X</sub>"
                                    ", N<sub>Y</sub> and N<sub>Z</sub>");
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
                                     ->GetDisplayStringFromValueX(cdelt1).c_str());
        double cdelt2 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT2"));
        d->CDELT2LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueY(cdelt2).c_str());
        double cdelt3 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT3"));
        d->CDELT3LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueZ(cdelt3).c_str());
        d->SigmaXLabel->setText("N<sub>X</sub>:");
        d->SigmaYLabel->setText("N<sub>Y</sub>:");
        d->SigmaZLabel->setText("N<sub>Z</sub>:");
        d->DoubleSpinBoxX->setSingleStep(2);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxX->blockSignals(true);
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          d->DoubleSpinBoxX->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          }
        d->DoubleSpinBoxX->setToolTip("Number of pixel of the Box kernel in the X direction");
        d->DoubleSpinBoxY->setSingleStep(2);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxY->blockSignals(true);
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          d->DoubleSpinBoxY->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          }
        d->DoubleSpinBoxY->setToolTip("Number of pixel of the Box kernel in the Y direction");
        d->DoubleSpinBoxZ->setSingleStep(2);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxZ->blockSignals(true);
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          d->DoubleSpinBoxZ->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          }
        d->DoubleSpinBoxZ->setToolTip("Number of pixel of the Box kernel in the Z direction");
        d->DoubleSpinBoxX->setMaximum(10);
        d->DoubleSpinBoxY->setMaximum(10);
        d->DoubleSpinBoxZ->setMaximum(10);
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
        d->LinkCheckBox->setToolTip("Click to link / unlink the parameters"
                                    " FWHM<sub>X</sub>, FWHM<sub>Y</sub> and FWHM<sub>Z</sub>");
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
                                     ->GetDisplayStringFromValueX(cdelt1).c_str());
        double cdelt2 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT2"));
        d->CDELT2LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueY(cdelt2).c_str());
        double cdelt3 = StringToDouble(inputVolumeNode->GetAttribute("SlicerAstro.CDELT3"));
        d->CDELT3LabelValue->setText(inputVolumeNode->GetAstroVolumeDisplayNode()
                                     ->GetDisplayStringFromValueZ(cdelt3).c_str());
        d->SigmaXLabel->setText("FWHM<sub>X</sub>:");
        d->SigmaYLabel->setText("FWHM<sub>Y</sub>:");
        d->SigmaZLabel->setText("FWHM<sub>Z</sub>:");
        d->DoubleSpinBoxX->setSingleStep(1);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxX->blockSignals(true);
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          d->DoubleSpinBoxX->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          }
        d->DoubleSpinBoxX->setToolTip("Full width at half maximum in pixel in the X direction");
        d->DoubleSpinBoxY->setSingleStep(1);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxY->blockSignals(true);
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          d->DoubleSpinBoxY->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          }
        d->DoubleSpinBoxY->setToolTip("Full width at half maximum in pixel in the Y direction");
        d->DoubleSpinBoxZ->setSingleStep(1);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxZ->blockSignals(true);
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          d->DoubleSpinBoxZ->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          }
        d->DoubleSpinBoxZ->setToolTip("Full width at half maximum in pixel in the Z direction");
        d->DoubleSpinBoxX->setMaximum(10);
        d->DoubleSpinBoxY->setMaximum(10);
        d->DoubleSpinBoxZ->setMaximum(10);
        QString theta = QChar(0x98, 0x03);
        d->RxLabel->setText(theta + "<sub>X</sub>:");
        d->RyLabel->setText(theta + "<sub>Y</sub>:");
        d->RzLabel->setText(theta + "<sub>Z</sub>:");
        d->RxSpinBox->setToolTip("Rotation Euler angle (in degree) with respect to the x axis");
        d->RySpinBox->setToolTip("Rotation Euler angle (in degree) with respect to the y axis");
        d->RzSpinBox->setToolTip("Rotation Euler angle (in degree) with respect to the z axis");
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
          RadiusX *= d->parametersNode->GetAccuracy();
          double RadiusY = d->parametersNode->GetParameterY() / SigmatoFWHM;
          if(RadiusY < 0.01)
            {
            RadiusY = 0.01;
            }
          RadiusY *= d->parametersNode->GetAccuracy();
          double RadiusZ = d->parametersNode->GetParameterZ() / SigmatoFWHM;
          if(RadiusZ < 0.01)
            {
            RadiusZ = 0.01;
            }
          RadiusZ *= d->parametersNode->GetAccuracy();
          d->parametricVTKEllipsoid->SetXRadius(RadiusX);
          d->parametricVTKEllipsoid->SetYRadius(RadiusY);
          d->parametricVTKEllipsoid->SetZRadius(RadiusZ);
          d->parametricFunctionSource->SetParametricFunction(d->parametricVTKEllipsoid);
          d->parametricFunctionSource->Update();

          //Configuration of the rotation

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
          d->transformationMatrix->SetElement(1, 0, -cy * sz);
          d->transformationMatrix->SetElement(2, 0, sy);
          d->transformationMatrix->SetElement(0, 1, cz * sx * sy + cx * sz);
          d->transformationMatrix->SetElement(1, 1, cx * cz - sx * sy * sz);
          d->transformationMatrix->SetElement(2, 1, -cy * sx);
          d->transformationMatrix->SetElement(0, 2, -cx * cz * sy + sx * sz);
          d->transformationMatrix->SetElement(1, 2, cz * sx + cx * sy * sz);
          d->transformationMatrix->SetElement(2, 2, cx * cy);

          d->matrixToLinearTransform->SetInput(d->transformationMatrix);
          d->matrixToLinearTransform->Update();

          d->mapper->SetInputConnection(d->parametricFunctionSource->GetOutputPort());
          d->actor->SetMapper(d->mapper);
          d->actor->GetProperty()->SetColor(0.0, 1.0, 1.0);
          d->actor->SetUserTransform(d->matrixToLinearTransform);
          d->actor->PickableOff();
          d->actor->DragableOff();

          renderer->AddActor(d->actor);
          renderer->SetBackground(0., 0., 0.);

          vtkCamera* camera = d->GaussianKernelView->activeCamera();

          vtkCollection *coll = this->mrmlScene()->GetNodesByClass("vtkMRMLCameraNode");
          vtkMRMLCameraNode *cameraNodeOne =
            vtkMRMLCameraNode::SafeDownCast(coll->GetItemAsObject(0));
          if (cameraNodeOne)
            {
            double VectorThree[3];
            double temp, viewFactor, scaleFactor;
            cameraNodeOne->GetPosition(VectorThree);
            //Ry(90)
            temp = VectorThree[1];
            VectorThree[1] = -VectorThree[2];
            VectorThree[2] = temp;
            //Rz(-180)
            VectorThree[0] *= -1.;
            VectorThree[1] *= -1.;
            viewFactor = 200.;
            scaleFactor = 4.;
            while(fabs(VectorThree[0]) > viewFactor ||
                  fabs(VectorThree[1]) > viewFactor ||
                  fabs(VectorThree[2]) > viewFactor)
              {
              VectorThree[0] /= scaleFactor;
              VectorThree[1] /= scaleFactor;
              VectorThree[2] /= scaleFactor;
              }
            camera->SetPosition(VectorThree);
            cameraNodeOne->GetFocalPoint(VectorThree);
            //Ry(90)
            temp = VectorThree[1];
            VectorThree[1] = -VectorThree[2];
            VectorThree[2] = temp;
            //Rz(-180)
            VectorThree[0] *= -1;
            VectorThree[1] *= -1;
            viewFactor = 15.;
            scaleFactor = 2.;
            while(fabs(VectorThree[0]) > viewFactor ||
                  fabs(VectorThree[1]) > viewFactor ||
                  fabs(VectorThree[2]) > viewFactor)
              {
              VectorThree[0] /= scaleFactor;
              VectorThree[1] /= scaleFactor;
              VectorThree[2] /= scaleFactor;
              }
            camera->SetFocalPoint(VectorThree);
            cameraNodeOne->GetViewUp(VectorThree);
            //Ry(90)
            temp = VectorThree[1];
            VectorThree[1] = -VectorThree[2];
            VectorThree[2] = temp;
            //Rz(-180)
            VectorThree[0] *= -1;
            VectorThree[1] *= -1;
            camera->SetViewUp(VectorThree);
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
        d->LinkCheckBox->setToolTip("Click to link / unlink the conductivity parameters");
        d->KLabel->show();
        d->KSpinBox->show();
        d->SigmaYLabel->show();
        d->DoubleSpinBoxY->show();
        d->SigmaZLabel->show();
        d->DoubleSpinBoxZ->show();

        d->KSpinBox->setValue(d->parametersNode->GetK());
        d->TimeStepLabel->show();
        d->TimeStepSpinBox->show();
        d->SigmaXLabel->setText("Horizontal Conductance:");
        d->SigmaYLabel->setText("Vertical Conductance:");
        d->SigmaZLabel->setText("Depth Conductance:");
        d->DoubleSpinBoxX->setToolTip("");
        d->DoubleSpinBoxY->setToolTip("");
        d->DoubleSpinBoxZ->setToolTip("");
        d->DoubleSpinBoxX->setSingleStep(1);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxX->blockSignals(true);
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          d->DoubleSpinBoxX->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
          }
        d->DoubleSpinBoxY->setSingleStep(1);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxY->blockSignals(true);
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          d->DoubleSpinBoxY->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
          }
        d->DoubleSpinBoxZ->setSingleStep(1);
        if(d->parametersNode->GetLink())
          {
          d->DoubleSpinBoxZ->blockSignals(true);
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          d->DoubleSpinBoxZ->blockSignals(false);
          }
        else
          {
          d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
          }
        d->DoubleSpinBoxX->setMaximum(10);
        d->DoubleSpinBoxY->setMaximum(10);
        d->DoubleSpinBoxZ->setMaximum(10);
        d->AccuracyLabel->setText("Iterations:");
        d->AccuracySpinBox->setMaximum(30);
        d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());
        d->AccuracySpinBox->setToolTip("");
        if (d->parametersNode->GetHardware())
          {
          d->AccuracySpinBox->setSingleStep(2);
          }
        else
          {
          d->AccuracySpinBox->setSingleStep(1);
          }
        d->TimeStepSpinBox->setValue(d->parametersNode->GetTimeStep());
        d->TimeStepSpinBox->setSingleStep(0.003);
        d->TimeStepSpinBox->setMaximum(0.0625);
        break;
        }
      }
    d->parametersNode->SetGaussianKernels();
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
void qSlicerAstroSmoothingModuleWidget::onModeChanged()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
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
void qSlicerAstroSmoothingModuleWidget::onCurrentFilterChanged(int index)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();

  d->parametersNode->SetAutoRun(false);

  if (index == 0)
    {
    d->parametersNode->SetParameterX(5);
    d->parametersNode->SetParameterY(5);
    d->parametersNode->SetParameterZ(5);
    d->parametersNode->SetKernelLengthX(5);
    d->parametersNode->SetKernelLengthY(5);
    d->parametersNode->SetKernelLengthZ(5);
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
    }

  if (index == 2)
    {
    if (d->parametersNode->GetHardware())
      {
      d->parametersNode->SetAccuracy(19);
      }
    else
      {
      d->parametersNode->SetAccuracy(20);
      }
    d->parametersNode->SetTimeStep(0.0325);
    d->parametersNode->SetK(1.5);
    d->parametersNode->SetParameterX(5);
    d->parametersNode->SetParameterY(5);
    d->parametersNode->SetParameterZ(5);
    }

  d->parametersNode->SetFilter(index);

  d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onKChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetK(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onTimeStepChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetTimeStep(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onRxChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRx(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onRyChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRy(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onRzChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }
  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetRz(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onParameterXChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
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
    if (d->parametersNode->GetFilter() == 0)
      {
      d->parametersNode->SetKernelLengthY(value);
      d->parametersNode->SetKernelLengthZ(value);
      }
    }
  if (d->parametersNode->GetFilter() == 0)
    {
    d->parametersNode->SetKernelLengthX(value);
    }
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onParameterYChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
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
    if (d->parametersNode->GetFilter() == 0)
      {
      d->parametersNode->SetKernelLengthX(value);
      d->parametersNode->SetKernelLengthZ(value);
      }
    }
  if (d->parametersNode->GetFilter() == 0)
    {
    d->parametersNode->SetKernelLengthY(value);
    } 
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onParameterZChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
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
    if (d->parametersNode->GetFilter() == 0)
      {
      d->parametersNode->SetKernelLengthX(value);
      d->parametersNode->SetKernelLengthY(value);
      }
    }
  if (d->parametersNode->GetFilter() == 0)
    {
    d->parametersNode->SetKernelLengthZ(value);
    }
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }

}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onAccuracyChanged(double value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  int wasModifying = d->parametersNode->StartModify();
  d->parametersNode->SetAccuracy(value);
  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() > 1)
    {
    d->parametersNode->SetStatus(-1);
    }
  d->parametersNode->EndModify(wasModifying);

  if (d->parametersNode->GetAutoRun() && d->parametersNode->GetStatus() == 0)
    {
    this->onApply();
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onApply()
{
  Q_D(const qSlicerAstroSmoothingModuleWidget);
  vtkSlicerAstroSmoothingLogic *logic = d->logic();

  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetStatus(1);

  vtkMRMLScene *scene = this->mrmlScene();

  d->astroVolumeWidget->stopRockView();

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));

  int n = StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS"));
  // Check Input volume
  if (n != 3)
    {
    qWarning() <<"filtering techniques are available only"<<
             "for datacube with dimensionality 3 (NAXIS = 3).";
    return;
    }

  inputVolume->SetDisplayVisibility(0);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_Filtered_";

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
    }

  int serial = d->parametersNode->GetOutputSerial();
  outSS<<"_"<< IntToString(serial);
  serial++;
  d->parametersNode->SetOutputSerial(serial);

  vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
  vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();

  vtkMRMLAstroVolumeNode *secondaryVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(selectionNode->GetSecondaryVolumeID()));

  if (secondaryVolume && d->parametersNode->GetAutoRun())
    {
    scene->RemoveNode(secondaryVolume);
    }

  // check Output volume
  if (!strcmp(inputVolume->GetID(), outputVolume->GetID()) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS1")) !=
      StringToInt(outputVolume->GetAttribute("SlicerAstro.NAXIS1"))) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS2")) !=
      StringToInt(outputVolume->GetAttribute("SlicerAstro.NAXIS2"))) ||
     (StringToInt(inputVolume->GetAttribute("SlicerAstro.NAXIS3")) !=
      StringToInt(outputVolume->GetAttribute("SlicerAstro.NAXIS3"))))
    {

    vtkSlicerAstroSmoothingLogic* logic =
      vtkSlicerAstroSmoothingLogic::SafeDownCast(this->logic());
   outputVolume = vtkMRMLAstroVolumeNode::SafeDownCast
       (logic->GetAstroVolumeLogic()->CloneVolume(scene, inputVolume, outSS.str().c_str()));

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

  if (logic->Apply(d->parametersNode, d->GaussianKernelView->renderWindow()))
    {
    selectionNode->SetReferenceActiveVolumeID(outputVolume->GetID());
    // this should be not needed. However, without it seems that
    // the connection with the Rendering Display is broken.

    d->astroVolumeWidget->setComparative3DViews
        (inputVolume->GetID(), outputVolume->GetID());

    selectionNode->SetReferenceActiveVolumeID(inputVolume->GetID());
    selectionNode->SetReferenceSecondaryVolumeID(outputVolume->GetID());
    appLogic->PropagateVolumeSelection();
    }
  else
    {
    scene->RemoveNode(outputVolume);
    inputVolume->SetDisplayVisibility(1);
    }
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onComputationFinished()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->CancelButton->hide();
  d->progressBar->hide();
  d->ApplyButton->show();
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onComputationCancelled()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->parametersNode->SetStatus(-1);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::updateProgress(int value)
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->progressBar->setValue(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onHardwareChanged(int index)
{
 Q_D(qSlicerAstroSmoothingModuleWidget);

 int wasModifying = d->parametersNode->StartModify();

 d->parametersNode->SetHardware(index);
 int filter = d->parametersNode->GetFilter();

 if (filter == 2)
   {
   if (index)
     {
     d->parametersNode->SetAccuracy(19);
     }
   else
     {
     d->parametersNode->SetAccuracy(20);
     }
   }
 d->parametersNode->EndModify(wasModifying);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onLinkChanged(bool value)
{
 Q_D(qSlicerAstroSmoothingModuleWidget);
 d->parametersNode->SetLink(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onAutoRunChanged(bool value)
{
 Q_D(qSlicerAstroSmoothingModuleWidget);
 d->parametersNode->SetAutoRun(value);
}

//-----------------------------------------------------------------------------
void qSlicerAstroSmoothingModuleWidget::onComputationStarted()
{
  Q_D(qSlicerAstroSmoothingModuleWidget);
  d->ApplyButton->hide();
  d->progressBar->show();
  d->CancelButton->show();
}

//---------------------------------------------------------------------------
vtkMRMLAstroSmoothingParametersNode* qSlicerAstroSmoothingModuleWidget::
mrmlAstroSmoothingParametersNode()const
{
  Q_D(const qSlicerAstroSmoothingModuleWidget);
  return d->parametersNode;
}

