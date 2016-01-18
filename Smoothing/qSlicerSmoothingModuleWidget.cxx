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

  progressBar->hide();
  progressBar->setMinimum(0);
  progressBar->setMaximum(100);
  CancelButton->hide();
  KLabel->hide();
  KSpinBox->hide();
  TimeStepLabel->hide();
  TimeStepSpinBox->hide();
  vtkCamera* camera = VTKRenderView->activeCamera();
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

  switch (d->parametersNode->GetFilter())
    {
    case 0:
      {
      d->KLabel->hide();
      d->KSpinBox->hide();
      d->TimeStepLabel->hide();
      d->TimeStepSpinBox->hide();
      d->AccuracyLabel->show();
      d->AccuracySpinBox->show();
      d->SigmaYLabel->show();
      d->DoubleSpinBoxY->show();
      d->SigmaZLabel->show();
      d->DoubleSpinBoxZ->show();
      d->SigmaXLabel->setText("SigmaX:");
      d->SigmaYLabel->setText("SigmaY:");
      d->SigmaZLabel->setText("SigmaZ:");
      d->DoubleSpinBoxX->setMaximum(10);
      d->DoubleSpinBoxX->setSingleStep(0.5);
      d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
      d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
      d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
      if (d->DoubleSpinBoxX->decimals() != 2)
        {
        d->DoubleSpinBoxX->setDecimals(2);
        }
      d->AccuracyLabel->setText("Kernel Accuracy:");
      d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());
      d->AccuracySpinBox->setMaximum(5);

      if (d->parametersNode->GetParameterX() == d->parametersNode->GetParameterY() &&
         d->parametersNode->GetParameterY() == d->parametersNode->GetParameterZ())
        {
        d->VTKRenderView->hide();
        d->RxLabel->hide();
        d->RxSpinBox->hide();
        d->RyLabel->hide();
        d->RySpinBox->hide();
        d->RzLabel->hide();
        d->RzSpinBox->hide();
        }
      else
        {
        d->VTKRenderView->show();
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

        d->VTKRenderView->show();
        vtkRenderer* renderer = d->VTKRenderView->renderer();

        vtkActorCollection* col = renderer->GetActors();
        col->InitTraversal();
        for (int i = 0; i < col->GetNumberOfItems(); i++)
          {
          renderer->RemoveActor(col->GetNextActor());
          }

        //Definition and transformation of the vtkEllipsoid
        double RadiusX = d->parametersNode->GetParameterX();
        if(RadiusX < 0.01)
          {
          RadiusX = 0.01;
          }
        double RadiusY = d->parametersNode->GetParameterY();
        if(RadiusY < 0.01)
          {
          RadiusY = 0.01;
          }
        double RadiusZ = d->parametersNode->GetParameterZ();
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
        }

      break;
      }
    case 1:
      {
      d->VTKRenderView->hide();
      d->RxLabel->hide();
      d->RxSpinBox->hide();
      d->RyLabel->hide();
      d->RySpinBox->hide();
      d->RzLabel->hide();
      d->RzSpinBox->hide();
      d->KLabel->show();
      d->KSpinBox->show();
      d->SigmaYLabel->show();
      d->DoubleSpinBoxY->show();
      d->SigmaZLabel->show();
      d->DoubleSpinBoxZ->show();
      d->AccuracyLabel->show();
      d->AccuracySpinBox->show();

      d->KSpinBox->setValue(d->parametersNode->GetK());
      d->TimeStepLabel->show();
      d->TimeStepSpinBox->show();
      d->TimeStepSpinBox->setValue(d->parametersNode->GetTimeStep());
      d->SigmaXLabel->setText("Horizontal Conduntance:");
      d->SigmaYLabel->setText("Vertical Conduntance:");
      d->SigmaZLabel->setText("Depth Conduntance:");
      d->DoubleSpinBoxX->setMaximum(10);
      d->DoubleSpinBoxX->setSingleStep(0.5);
      d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
      d->DoubleSpinBoxY->setValue(d->parametersNode->GetParameterY());
      d->DoubleSpinBoxZ->setValue(d->parametersNode->GetParameterZ());
      if (d->DoubleSpinBoxX->decimals() != 2)
        {
        d->DoubleSpinBoxX->setDecimals(2);
        }
      d->AccuracyLabel->setText("Iterations:");
      d->AccuracySpinBox->setMaximum(30);
      d->AccuracySpinBox->setValue(d->parametersNode->GetAccuracy());
      break;
      }
    case 2:
      {
      d->SigmaYLabel->hide();
      d->DoubleSpinBoxY->hide();
      d->SigmaZLabel->hide();
      d->DoubleSpinBoxZ->hide();
      d->AccuracySpinBox->hide();
      d->AccuracyLabel->hide();
      d->VTKRenderView->hide();
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
      d->SigmaXLabel->setText("Wavelet level:");
      d->DoubleSpinBoxX->setSingleStep(1);
      d->DoubleSpinBoxX->setValue(d->parametersNode->GetParameterX());
      d->DoubleSpinBoxX->setMaximum(5);
      if (d->DoubleSpinBoxX->decimals() != 0)
        {
        d->DoubleSpinBoxX->setDecimals(0);
        }
      break;
      }
    }

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

  int wasModifying;
  wasModifying = d->parametersNode->StartModify();

  if (index == 0)
    {
    d->parametersNode->SetAccuracy(3);
    d->parametersNode->SetParameterX(1.5);
    d->parametersNode->SetParameterY(1.5);
    d->parametersNode->SetParameterZ(1.5);
    d->parametersNode->SetGaussianKernels();
    }

  if (index == 1)
    {
    d->parametersNode->SetAccuracy(10);
    d->parametersNode->SetParameterX(4);
    d->parametersNode->SetParameterY(4);
    d->parametersNode->SetParameterZ(4);
    }

  if (index == 2)
    {
    d->parametersNode->SetParameterX(3);
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

  d->parametersNode->SetRx(value);
  d->parametersNode->SetGaussianKernels();
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onRyChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetRy(value);
  d->parametersNode->SetGaussianKernels();
}

//-----------------------------------------------------------------------------
void qSlicerSmoothingModuleWidget::onRzChanged(double value)
{
  Q_D(qSlicerSmoothingModuleWidget);
  if (!d->parametersNode)
    {
    return;
    }

  d->parametersNode->SetRz(value);
  d->parametersNode->SetGaussianKernels();
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
void qSlicerSmoothingModuleWidget::onAccuracyChanged(double value)
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

  vtkMRMLScene *scene = this->mrmlScene();

  d->astroVolumeWidget->stopRockView();

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetInputVolumeNodeID()));

  inputVolume->SetDisplayVisibility(0);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast(scene->
      GetNodeByID(d->parametersNode->GetOutputVolumeNodeID()));

  std::ostringstream outSS;
  outSS << inputVolume->GetName() << "_Filtered_" <<
    d->parametersNode->GetMode() << "_";

  //in case of Automatic here the filter and parameters has to be setted
  //for the moment Automatic mode is not implemented
  switch (d->parametersNode->GetFilter())
    {
    case 0:
      {
      outSS<<"Gaussian";
      break;
      }
    case 1:
      {
      outSS<<"Gradient";
      break;
      }
    case 2:
      {
      outSS<<"Wavelet Lifting";
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

  if (logic->Apply(d->parametersNode))
    {
    vtkSlicerApplicationLogic *appLogic = this->module()->appLogic();
    vtkMRMLSelectionNode *selectionNode = appLogic->GetSelectionNode();

    //when Autoran will be implemented here should be checked how to improve perfomance.
    //P.S.: before autoran it will be needed to implement the filters on GPU (OpneGL).
    //Autoran implementation: will be a checkable box and if is true
    //onMRMLSmoothingParametersNodeModified will call Apply.
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

